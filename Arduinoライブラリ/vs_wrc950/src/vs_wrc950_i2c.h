// Copyright 2026 Vstone Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

/**
 * VS-WRC950上のESP32とSTM32間のi2c通信に関するライブラリ
 */
#ifndef WRC950_I2C_H
#define WRC950_I2C_H

#include <Arduino.h>
#include <Wire.h>
#include "vs_wrc950_memmap.h"

void i2cMasterInit();
void i2cSlaveInit(uint8_t addr);





#endif /* I2C_H */
