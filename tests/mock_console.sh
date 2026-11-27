#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build
cc -std=gnu11 -Wno-int-to-pointer-cast -ffunction-sections -fdata-sections \
  -DSTM32F411xE -DUSE_HAL_DRIVER -ICore/Inc \
  -IDrivers/STM32F4xx_HAL_Driver/Inc -IDrivers/CMSIS/Include \
  -IDrivers/CMSIS/Device/ST/STM32F4xx/Include \
  tests/mock_console.c Core/Src/lcd1602.c -Wl,--gc-sections -o build/mock-console
build/mock-console
