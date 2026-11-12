// Copyright 2026 Vstone Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

/**
 * VS-WRC950用ライブラリの各ファイルをまとめてincludeするためのヘッダ
 * およびユーティリティ的な関数集
 */
#ifndef WRC950_SERVO_H
#define WRC950_SERVO_H

#include "vs_wrc950_memmap.h"
#include <ESP32Servo.h>
#include "vs_wrc950_ugokun.h"
#include <Arduino.h>
#include <iostream>
#include <vector>
#include <algorithm>    
#include <iterator>  
#include "driver/gpio.h"
#include "soc/gpio_sig_map.h"
#include "esp32/rom/gpio.h"

#define YAW_SERVO_PIN 14
#define THROTLE_SERVO_PIN 27
#define SERIAL_SERVO_RX_PIN 16
#define SERIAL_SERVO_TX_PIN 17

#define SERVO_MIN 560
#define SERVO_MAX 2480
#define SERVO_FREQ 250

#define ACT_DIFF  200

#define MAX_DPS 193

//スロットル軸(サーボ2)のラック&ピニオン：モジュール2、12歯（ピッチ円直径24mm）、上下35mm
#define THROTLE_PITCH_DIAMETER_MM 24.0
#define THROTLE_DEG_PER_MM (360.0 / (PI * THROTLE_PITCH_DIAMETER_MM))  //1mm移動に必要な回転角度[deg]（約4.77deg）

extern unsigned char TxData[];  
extern unsigned int BaudRate[];  

extern Servo servo_yaw;
extern Servo servo_throtle;

void clearRX();
void initOneWireUART();

void changeCtrMode(int mode);

void update_servo(bool sound_active, uint32_t audio_level);
void servoInit();
void servoInit(int mode);

void moveServo();
int moveServoDps(unsigned char ID, int Angle, double degPerSec, int temp);
int moveServoTime(unsigned char ID, int Angle, int Time, int temp);

//PWM制御
void MovePWM();

//シリアル制御
void servoCtrl();
void allServoOn();
void allServoOff();
void servoEN(unsigned char ID, unsigned char data);
void moveSerial(unsigned char ID, int Angle);
void moveSerial(unsigned char ID, int Angle, int Speed);
void changeSpeed(unsigned char ID, int Speed);
void changeBaud(unsigned char ID, int rateNum);
struct ServoTemperatureStatus {
    bool valid = false;
    bool limitError = false;
    bool limitAlarm = false;
    uint8_t flags = 0;
};

double readAngle(unsigned char ID);
double readAngle(unsigned char ID, ServoTemperatureStatus *temperatureStatus);
// On failure, torque is unchanged and temperatureStatus is invalid.
bool readServoTorque(unsigned char ID, uint8_t &torque, ServoTemperatureStatus *temperatureStatus = nullptr);
int waitReadAngle();
// Status is valid only for a checked response without a packet-processing error.
int waitReadAngle(ServoTemperatureStatus *temperatureStatus);
int readTempLimit(unsigned char ID, ServoTemperatureStatus *temperatureStatus = nullptr);
int readTemp(unsigned char ID, ServoTemperatureStatus *temperatureStatus = nullptr);
void setLimit(unsigned char ID, int uLimit, int lLimit);

#endif /* WRC950 */
