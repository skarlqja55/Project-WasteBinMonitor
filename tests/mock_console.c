#include <assert.h>
#include "main.h"
#define __disable_irq() ((void)0)

#define main firmware_main
#include "../Core/Src/main.c"
#undef main

static uint32_t ticks;
static int matrix[3];
static int alert = -1;
static unsigned beeps;
static uint8_t lcd_row, lcd_col, pending_nibble;
static int have_nibble;
static int four_bit;
static char lcd[2][17];

uint32_t HAL_GetTick(void) { return ticks; }
void HAL_Delay(uint32_t ms) { ticks += ms; }

HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *bus, uint16_t address,
                                        uint8_t *data, uint16_t length, uint32_t timeout)
{
    (void)timeout;
    assert(bus == &hi2c1 && address == 0x4e && length == 1);
    uint8_t value = *data;
    if (!(value & 0x04))
        return HAL_OK;
    uint8_t nibble = value >> 4;
    if (!four_bit) {
        if (!(value & 1) && nibble == 2)
            four_bit = 1;
        return HAL_OK;
    }
    if (!have_nibble) {
        pending_nibble = nibble;
        have_nibble = 1;
        return HAL_OK;
    }
    have_nibble = 0;
    uint8_t byte = (pending_nibble << 4) | nibble;
    if (value & 1) {
        if (lcd_col < 16)
            lcd[lcd_row][lcd_col++] = (char)byte;
    } else if (byte == 1) {
        memset(lcd[0], ' ', 16);
        memset(lcd[1], ' ', 16);
        lcd_row = lcd_col = 0;
    } else if (byte & 0x80) {
        lcd_row = (byte & 0x40) != 0;
        lcd_col = byte & 0x3f;
    }
    return HAL_OK;
}

HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *uart, uint8_t *data, uint16_t len)
{
    assert(uart == &huart1 && data == &rxData && len == 1);
    return HAL_OK;
}

HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *uart, const uint8_t *data,
                                   uint16_t len, uint32_t timeout)
{
    (void)data;
    (void)len;
    (void)timeout;
    assert(uart == &huart2);
    return HAL_OK;
}

void DM_Map_Init(void) {}
void DM_Map_Loop(void) {}
void DM_Map_SetBinPercent(uint8_t bin, int percent) { matrix[bin] = percent; }
void DM_Map_RequestAlert(uint8_t bin, uint32_t duration) { assert(duration == 5000); alert = bin; }
void Buzzer_Init(void) {}
void Buzzer_Loop(void) {}
void Buzzer_ManualBeep(uint32_t duration) { assert(duration == 2000); beeps++; }

static void receive(const char *line)
{
    while (*line) {
        rxData = (uint8_t)*line++;
        HAL_UART_RxCpltCallback(&huart1);
    }
}

int main(void)
{
    huart1.Instance = USART1;
    App_Init();
    assert(strstr(lcd[0], "FOOD WASTE") && strstr(lcd[1], "SYSTEM START"));
    receive("STATUS@80@35@91\r\n");
    App_Tick();
    assert(matrix[0] == 80 && matrix[1] == 35 && matrix[2] == 91);
    assert(strstr(lcd[0], "80") && strstr(lcd[0], "35") && strstr(lcd[1], "91"));

    receive("REQUEST@A\nREQUEST@B\nREQUEST@C\nSTATUS@-1@36@92\n");
    for (int bin = 0; bin < 3; ++bin) {
        App_Tick();
        char expected[] = "REQUEST BIN A";
        expected[12] += bin;
        assert(strstr(lcd[0], expected) && strstr(lcd[1], "COLLECT REQUEST"));
        assert(alert == bin && beeps == (unsigned)(bin + 1));
    }
    App_Tick();
    assert(matrix[0] == -1 && matrix[2] == 92);
    ticks = requestDisplayUntil - 1;
    App_Tick();
    assert(strstr(lcd[0], "REQUEST BIN C"));
    ++ticks;
    App_Tick();
    assert(strstr(lcd[0], "-1%") && strstr(lcd[1], "92"));
    assert(uartDroppedLines == 0);

    receive("REQUEST@A\nREQUEST@A\nREQUEST@A\nREQUEST@A\n"
            "REQUEST@A\nREQUEST@A\nREQUEST@A\nREQUEST@C\n");
    assert(uartDroppedLines == 1);
    for (int count = 0; count < 7; ++count)
        App_Tick();
    assert(alert == 0);
    receive("REQUEST@B\n");
    App_Tick();
    assert(alert == 1);
    puts("PASS: boot LCD, all requests, status, five-second restore, sensor error, queue and recovery");
}
