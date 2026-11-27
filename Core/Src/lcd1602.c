/*
 * lcd1602.c
 *
 *  Created on: 2026. 10. 6.
 *      Author: kccistc
 */


#include "lcd1602.h"

#include <stdio.h>
#include <string.h>

/*
 * I2C1은 CubeMX에서 생성된 핸들 사용
 */
extern I2C_HandleTypeDef hi2c1;


/*
 * LCD1602 I2C 주소
 *
 * 대부분 PCF8574 백팩은 0x27
 * HAL에서는 7bit 주소를 왼쪽으로 1bit shift해서 사용
 */
#define LCD_ADDR        (0x27 << 1)

#define LCD_BACKLIGHT   0x08
#define LCD_ENABLE      0x04
#define LCD_RW          0x02
#define LCD_RS          0x01


/* ================================================
 * I2C로 1바이트 전송
 * ================================================ */
static void LCD_I2C_Write(uint8_t data)
{
    HAL_I2C_Master_Transmit(
        &hi2c1,
        LCD_ADDR,
        &data,
        1,
        100
    );
}


/* ================================================
 * Enable Pulse
 * ================================================ */
static void LCD_EnablePulse(uint8_t data)
{
    LCD_I2C_Write(data | LCD_ENABLE);

    HAL_Delay(1);

    LCD_I2C_Write(data & ~LCD_ENABLE);

    HAL_Delay(1);
}


/* ================================================
 * 4bit 전송
 * ================================================ */
static void LCD_Write4Bits(uint8_t data)
{
    LCD_I2C_Write(data | LCD_BACKLIGHT);

    LCD_EnablePulse(data | LCD_BACKLIGHT);
}


/* ================================================
 * Command 전송
 * ================================================ */
static void LCD_SendCommand(uint8_t cmd)
{
    uint8_t high;
    uint8_t low;

    high = cmd & 0xF0;
    low  = (cmd << 4) & 0xF0;

    /* RS = 0 : Command */
    LCD_Write4Bits(high);
    LCD_Write4Bits(low);
}


/* ================================================
 * 문자 하나 전송
 * ================================================ */
static void LCD_SendData(uint8_t data)
{
    uint8_t high;
    uint8_t low;

    high = (data & 0xF0) | LCD_RS;
    low  = ((data << 4) & 0xF0) | LCD_RS;

    /* RS = 1 : Data */
    LCD_Write4Bits(high);
    LCD_Write4Bits(low);
}


/* ================================================
 * LCD 초기화
 * ================================================ */
void LCD_Init(void)
{
    HAL_Delay(50);

    /*
     * HD44780 4bit 초기화 과정
     */
    LCD_Write4Bits(0x30);
    HAL_Delay(5);

    LCD_Write4Bits(0x30);
    HAL_Delay(1);

    LCD_Write4Bits(0x30);
    HAL_Delay(1);

    LCD_Write4Bits(0x20);
    HAL_Delay(1);


    /*
     * 4bit mode
     * 2 line
     * 5x8 font
     */
    LCD_SendCommand(0x28);

    /* 화면 ON, Cursor OFF */
    LCD_SendCommand(0x0C);

    /* 화면 Clear */
    LCD_SendCommand(0x01);
    HAL_Delay(2);

    /* Entry mode */
    LCD_SendCommand(0x06);
}


/* ================================================
 * 화면 지우기
 * ================================================ */
void LCD_Clear(void)
{
    LCD_SendCommand(0x01);

    HAL_Delay(2);
}


/* ================================================
 * Cursor 위치
 *
 * row = 0 : 첫째 줄
 * row = 1 : 둘째 줄
 * ================================================ */
void LCD_SetCursor(uint8_t row, uint8_t col)
{
    uint8_t address;

    if (row == 0)
        address = 0x00 + col;
    else
        address = 0x40 + col;

    LCD_SendCommand(0x80 | address);
}


/* ================================================
 * 문자열 출력
 * ================================================ */
void LCD_Print(char *str)
{
    while (*str)
    {
        LCD_SendData((uint8_t)*str);

        str++;
    }
}


/* ================================================
 * 쓰레기통 상태 LCD 출력
 *
 * 예:
 *
 * A: 80% B: 35%
 * C: 91%
 * ================================================ */
void LCD_UpdateBins(int a, int b, int c)
{
    char line1[17];
    char line2[17];


    /*
     * %-형태 대신 %3d를 써서
     * 5 -> "  5"
     * 50 -> " 50"
     * 100 -> "100"
     *
     * 이전 숫자가 화면에 남는 것도 방지
     */
    snprintf(
        line1,
        sizeof(line1),
        "A:%3d%% B:%3d%%",
        a,
        b
    );

    snprintf(
        line2,
        sizeof(line2),
        "C:%3d%%          ",
        c
    );


    LCD_SetCursor(0, 0);
    LCD_Print(line1);

    LCD_SetCursor(1, 0);
    LCD_Print(line2);
}
