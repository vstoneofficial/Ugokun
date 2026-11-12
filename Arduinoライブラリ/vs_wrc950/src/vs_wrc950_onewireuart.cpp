// Copyright 2026 Vstone Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

#include <Arduino.h>

#include "vs_wrc950_onewireuart.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "soc/gpio_sig_map.h"
#include "esp32/rom/gpio.h"


// =====================================================
// One-Wire UART 初期化
//
// Arduino-ESP32 Core 2.0.6用
// uart_set_pin()を使用せずGPIO Matrixへ直接接続
// =====================================================
void setupOneWireUART()
{
    uart_config_t config = {};

    config.baud_rate = OW_UART_BAUD;
    config.data_bits = UART_DATA_8_BITS;
    config.parity    = UART_PARITY_DISABLE;
    config.stop_bits = UART_STOP_BITS_1;
    config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    config.source_clk = UART_SCLK_APB;


    ESP_ERROR_CHECK(
        uart_driver_install(
            OW_UART,
            256,    // RX buffer
            0,      // TX buffer
            0,
            NULL,
            0
        )
    );


    ESP_ERROR_CHECK(
        uart_param_config(
            OW_UART,
            &config
        )
    );


    gpio_reset_pin((gpio_num_t)OW_UART_PIN);


    // RX + Open Drain TX
    ESP_ERROR_CHECK(
        gpio_set_direction(
            (gpio_num_t)OW_UART_PIN,
            GPIO_MODE_INPUT_OUTPUT_OD
        )
    );


    // 外部2.2kΩプルアップ使用
    ESP_ERROR_CHECK(
        gpio_set_pull_mode(
            (gpio_num_t)OW_UART_PIN,
            GPIO_FLOATING
        )
    );


    // UART2 TX → GPIO16
    gpio_matrix_out(
        OW_UART_PIN,
        U2TXD_OUT_IDX,
        false,
        false
    );


    // UART2 RX ← GPIO16
    gpio_matrix_in(
        OW_UART_PIN,
        U2RXD_IN_IDX,
        false
    );


    uart_flush_input(OW_UART);

    Serial.println("FUTABA One-Wire UART initialized");
}

// =====================================================
// 書き込み送信共通
//
// 書き込み時の自己エコーは不要なので、
// 送信完了後少し待ってRXバッファを捨てる
// =====================================================

void sendWritePacket(
    const uint8_t *packet,
    int length
)
{
    // 古い受信データ削除
    uart_flush_input(OW_UART);


    uart_write_bytes(
        OW_UART,
        (const char *)packet,
        length
    );


    uart_wait_tx_done(
        OW_UART,
        pdMS_TO_TICKS(20)
    );


    // 自己エコーがFIFOへ入るのを少し待つ
    delayMicroseconds(300);


    // 書き込みでは返信不要なのでEchoを削除
    uart_flush_input(OW_UART);
}



// =====================================================
// RAW受信
// =====================================================

int receiveRaw(
    uint8_t *rx,
    int maxLength,
    uint32_t timeoutMs
)
{
    return uart_read_bytes(
        OW_UART,
        rx,
        maxLength,
        pdMS_TO_TICKS(timeoutMs)
    );
}