// Copyright 2026 Vstone Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

#include <Arduino.h>

#include "driver/uart.h"
#include "driver/gpio.h"
#include "soc/gpio_sig_map.h"
#include "esp32/rom/gpio.h"

#define OW_UART   UART_NUM_2
#define OW_UART_PIN    16
#define OW_UART_BAUD   115200

void setupOneWireUART();