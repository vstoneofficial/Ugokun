// Copyright 2026 Vstone Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

/**
 * VS-WRC950用ライブラリの各ファイルをまとめてincludeするためのヘッダ
 * およびユーティリティ的な関数集
 */
#ifndef WRC950_H
#define WRC950_H

#include "vs_wrc950_memmap.h"
#include "vs_wrc950_i2c.h"
#include "vs_wrc950_serial.h"
#include "vs_wrc950_btaudio.h"
#include "vs_wrc950_servo.h"
#include "vs_wrc950_wifi.h"
#include "vs_wrc950_testaudio.h"

//LED制御のための定義
#define LED_PIN            4

enum wrc950StatusCode{
    NO_INPUT     = 0,          //入力無し or 無効な入力
    HTTP_ACCES   = 1,          //1~2はHTTPアクセスによる有効なメモリマップ操作命令
    HTTP_PAD     = 3,          //HTTPコントローラからのボタン入力
    //HTTP_SYSTEM  = WIFI_VIN,   //HTMLコントローラに電圧を表示するためのアクセス =4
    //HTTP_BAD     = BAD_REQUEST, //HTTPアクセスによるbad request =5    
    SERIAL_ACCES = 6,          //有線シリアル経由による有効なメモリマップ操作命令
    BLE_ACCES    = 7,
    BTC_ACCES    = 8,
    ROS_CTRL     = 9,
    ROS_ERR      = 10,
    PAD_INPUT    = 11

};

extern bool isInterrupt;        //タイマー割り込み発砲フラグ

void IRAM_ATTR onTimer();
void setupInterruptTimer();

void ledInit();
void LED(int cmd);
void chkLEDFlag();

void setTimeNow();

uint16_t checkWDT();


#endif /* WRC950 */
