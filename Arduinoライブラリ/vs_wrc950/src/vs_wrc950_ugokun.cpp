// Copyright 2026 Vstone Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

#include "vs_wrc950_ugokun.h"
#include "vs_wrc950_memmap.h"
#include "vs_wrc950_serial.h"
#include "vs_wrc950_btaudio.h"
#include "vs_wrc950_servo.h"

//LED制御のための定義
#define LEDC_CHANNEL_0     0
#define LEDC_TIMER_8_BIT   8
#define LEDC_BASE_FREQ     5000


/*******************************************
 * タイマー割り込み設定
 */
hw_timer_t *interruptTimer = NULL;
bool isInterrupt = false;

volatile SemaphoreHandle_t timerSemaphore;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR onTimer(){
  portENTER_CRITICAL_ISR(&timerMux);

  isInterrupt = true;

  portEXIT_CRITICAL_ISR(&timerMux);
  xSemaphoreGiveFromISR(timerSemaphore, NULL);

}

void setupInterruptTimer(){
  //セマフォを設定
  timerSemaphore = xSemaphoreCreateBinary();

  //4つ中1つ目のタイマーを使用する（カウントは0から）
  //dividerを80に設定
  interruptTimer = timerBegin(0, 80, true);

  //割り込みで発砲する関数にonTimer()を設定
  timerAttachInterrupt(interruptTimer, &onTimer, true);

  //割り込み周期を設定(単位はmicroSec)
  //繰り返し割り込みを許可（第３引数）
  timerAlarmWrite(interruptTimer, 20000, true);  //10000

  //割り込み判定開始
  timerAlarmEnable(interruptTimer);

}

/*******************************************
 * LED関連
 */
//LEDのセットアップ
void ledInit(){
  ledcSetup(LEDC_CHANNEL_0, LEDC_BASE_FREQ, LEDC_TIMER_8_BIT);
  ledcAttachPin(LED_PIN, LEDC_CHANNEL_0);

  return;
}

//LED制御関数
void LED(int cmd){
  if(cmd){
    //LEDを点ける
    ledcWrite(LEDC_CHANNEL_0, 255);
  }else{
    //LEDを消す
    ledcWrite(LEDC_CHANNEL_0, 0);
  }

  return;
}

//メモリマップからのLED操作(MU8_CR0 b1 highなら消灯,lowならフリー)
void chkLEDFlag(){

  if(wrc950.memMap[MU8_CR0] & 0x02){
    LED(0);
  }else{
    LED(1);
  }

  return;
}


/*******************************************
 * 時間管理
 */
void setTimeNow(){
  wrc950.u32Map(MU32_TRIPTIME, millis());

  return;
}

/*******************************************
 * WDT
 */
uint16_t checkWDT(){
  static uint32_t p_time = millis();
  uint32_t n_time = millis();

  if(wrc950.u16Map(MU16_WDT) == 0){

  }else if(wrc950.u16Map(MU16_WDT) == 0xffff){
    //WDT発動

  }else{
    //前回から今回の処理までの経過時間
    uint32_t elapsed_time = (n_time - p_time); //[msec]
    if(elapsed_time < 0){
      elapsed_time = (n_time + (UINT32_MAX - p_time)); //[msec]
    }
    
    if(wrc950.u16Map(MU16_WDT) <= elapsed_time ){
      wrc950.u16Map(MU16_WDT, 0xffff);
      
    }else{
      wrc950.u16Map(MU16_WDT, wrc950.u16Map(MU16_WDT)-elapsed_time);
    }
  }

  p_time = n_time;

  //Serial.println(wrc950.u16Map(MU16_WDT), HEX);

  return wrc950.u16Map(MU16_WDT);
}