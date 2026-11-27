CROSS_COMPILE ?= arm-none-eabi-
CC := $(CROSS_COMPILE)gcc
BUILD := build
HAL := Drivers/STM32F4xx_HAL_Driver
CMSIS := Drivers/CMSIS
CPU := -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
CPPFLAGS := -DUSE_HAL_DRIVER -DSTM32F411xE -ICore/Inc -I$(HAL)/Inc \
            -I$(CMSIS)/Include -I$(CMSIS)/Device/ST/STM32F4xx/Include
CFLAGS := $(CPU) -std=gnu11 -Og -g -ffunction-sections -fdata-sections
LINKER := STM32F411RETX_FLASH.ld
LDFLAGS := $(CPU) -T$(LINKER) --specs=nano.specs --specs=nosys.specs \
           -Wl,--gc-sections,-Map=$(BUILD)/waste-bin.map
SOURCES := $(wildcard Core/Src/*.c) \
           $(wildcard $(HAL)/Src/*.c) Core/Startup/startup_stm32f411retx.s
OBJECTS := $(addprefix $(BUILD)/,$(addsuffix .o,$(SOURCES)))

.PHONY: all clean
all: $(BUILD)/waste-bin.elf

$(BUILD)/waste-bin.elf: $(OBJECTS) $(LINKER)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@
	$(CROSS_COMPILE)size $@

$(BUILD)/%.c.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/%.s.o: %.s
	@mkdir -p $(@D)
	$(CC) $(CPU) -c $< -o $@

-include $(OBJECTS:.o=.d)

clean:
	rm -rf $(BUILD)
