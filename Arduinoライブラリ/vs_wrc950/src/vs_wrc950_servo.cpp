// Copyright 2026 Vstone Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

#include "vs_wrc950_memmap.h"
#include <ESP32Servo.h>
#include "vs_wrc950_ugokun.h"
#include "vs_wrc950_servo.h"

int control_mode=1;

Servo servo_yaw;
Servo servo_throtle;

int16_t servo_param[2][6] = {{1, 0 ,1500, -1500,10,76}, //サーボID、中心角度、上限角度、下限角度、遷移時間,温度リミット
                             {2, 300, 310, -1400, 10,76}};  //サーボ2：ラック可動範囲 0~35mm（モジュール2・12歯、30.0deg=0mm、-137deg=35mm）

int servo_mem[2][4] = {{MS16_CD_SV0, MS16_ULD_SV0 , MS16_LLD_SV0, MU16_S_SV0},
                        {MS16_CD_SV1, MS16_ULD_SV1 , MS16_LLD_SV1, MU16_S_SV1}};

int servo_temp_limit_mem[2] = {MU8_SV_TEMPLIMIT1, MU8_SV_TEMPLIMIT2};
int servo_temp_mem[2] = {MU8_SV_TEMP1, MU8_SV_TEMP2};
int servo_temp_status_mem[2] = {MU8_SV_TEMPERR1, MU8_SV_TEMPERR2};

unsigned char TxData[12] = {0xFA, 0xAF, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
unsigned int BaudRate[10] = {9600, 14400, 19200, 28800, 38400, 57600, 76800, 115200, 153600, 230400};

unsigned int old_t_f=0;
int16_t old_deg[3]={0,0,0};
int16_t old_cd[3]={0,0,0};

float currentDeg[2] = {0,0};
float startDeg[2];
float targetDeg[2];

unsigned long startTime;
int duration;

int stepIndex = 0;

uint32_t last_move_ms = 0;
int z_zero_deg=0;//-35;

const uint16_t FUTABA_PACKET_INTERVAL_US = 2500;
const uint16_t SERIAL_SERVO_CTRL_INTERVAL_MS = 40;
const uint16_t SERIAL_SERVO_POWER_ON_WAIT_MS = 800;
const uint16_t SERIAL_SERVO_INIT_INTERVAL_MS = 120;
const int16_t SERIAL_SERVO_ID2_WAKE_ANGLE = -50;
const uint16_t SERIAL_SERVO_ID2_WAKE_DELAY_MS = 120;
const uint16_t SERIAL_SERVO_READ_TIMEOUT_MS = 50;
const uint8_t SERIAL_SERVO_READ_ATTEMPTS = 2;
uint32_t last_serial_servo_ctrl_ms = 0;
unsigned char last_read_angle_id = 0;

static int waitServoValue(uint8_t address, uint8_t minimumLength, ServoTemperatureStatus *temperatureStatus);

void clearRX()
{
    while (Serial2.available()) {
        Serial2.read();
    }
}

void initOneWireUART(){
    Serial2.begin(
        115200,
        SERIAL_8N1,
        SERIAL_SERVO_RX_PIN,
        SERIAL_SERVO_TX_PIN,
        false,
        20000UL,
        1
    );

    delay(10);


    // -------------------------------------------------
    // GPIO17は使用しない
    // 基板上でBUSにつながっていてもHi-Zにする
    // -------------------------------------------------

    gpio_reset_pin(
        (gpio_num_t)SERIAL_SERVO_TX_PIN
    );

    gpio_set_direction(
        (gpio_num_t)SERIAL_SERVO_TX_PIN,
        GPIO_MODE_INPUT
    );

    gpio_set_pull_mode(
        (gpio_num_t)SERIAL_SERVO_TX_PIN,
        GPIO_FLOATING
    );


    // -------------------------------------------------
    // GPIO16をRX + Open Drain TXにする
    // -------------------------------------------------

    gpio_reset_pin(
        (gpio_num_t)SERIAL_SERVO_RX_PIN
    );

    gpio_set_direction(
        (gpio_num_t)SERIAL_SERVO_RX_PIN,
        GPIO_MODE_INPUT_OUTPUT_OD
    );

    // 外部2.2kΩプルアップを使用
    gpio_set_pull_mode(
        (gpio_num_t)SERIAL_SERVO_RX_PIN,
        GPIO_FLOATING
    );


    // UART2 TX → GPIO16
    gpio_matrix_out(
        SERIAL_SERVO_RX_PIN,
        U2TXD_OUT_IDX,
        false,
        false
    );

    // UART2 RX ← GPIO16
    gpio_matrix_in(
        SERIAL_SERVO_RX_PIN,
        U2RXD_IN_IDX,
        false
    );


    clearRX();
}

int servoIndexFromID(unsigned char ID){
    for(int i=0; i<2; i++){
        if(servo_param[i][0] == ID) return i;
    }
    return -1;
}

int clampServoAngle(unsigned char ID, int Angle){
    int index = servoIndexFromID(ID);
    if(index < 0) return Angle;
    if(Angle > servo_param[index][2]) return servo_param[index][2];
    if(Angle < servo_param[index][3]) return servo_param[index][3];
    return Angle;
}

void sendFutabaPacket(int lastIndex){
    Serial2.write(TxData, lastIndex + 1);
    Serial2.flush();
    delayMicroseconds(FUTABA_PACKET_INTERVAL_US);
}

void changeCtrMode(int mode){
    control_mode=mode;
    servoInit(mode);
}

void movePWM(){
    double yaw_angle_us=0,throtle_angle_us=0;
    yaw_angle_us = 1520- (double(wrc950.s16Map(MS16_CD_SV0))/10*960/144) - (double(wrc950.s16Map(MS16_D_SV0))/10*960/144);
    throtle_angle_us = 1520- (double(wrc950.s16Map(MS16_CD_SV1))/10*960/144) - (double(wrc950.s16Map(MS16_D_SV1))/10*960/144);
    servo_yaw.writeMicroseconds(int(yaw_angle_us));
    servo_throtle.writeMicroseconds(int(throtle_angle_us));
}

void update_servo(bool sound_active, uint32_t audio_level) {
    uint32_t min_audio_level = wrc950.u32Map(MU32_AUD_MIN);
    uint32_t max_audio_level = wrc950.u32Map(MU32_AUD_MAX);

    // 小さい音量でも大きく動かせるよう、音量レベルに係数を掛ける
    uint16_t audio_gain = wrc950.u16Map(MU16_AUD_GAIN);
    if (audio_gain == 0) audio_gain = 100;
    uint32_t scaled_audio_level = audio_level * double(audio_gain) / 100;

    if(scaled_audio_level<min_audio_level){
        if(sound_active){
            // 無音・小音時は中央
            if((wrc950.u8Map(MU8_E_SVU) & 0x01)==0) wrc950.s16Map(MS16_D_SV0,0);
            if(((wrc950.u8Map(MU8_E_SVU)>>1)&0x01)==0) wrc950.s16Map(MS16_D_SV1,0);
        }
        return;
    }

    uint16_t yaw_range[2]={wrc950.u16Map(MU16_AY_MIN),wrc950.u16Map(MU16_AY_MAX)};
    uint16_t throtle_range[2]={wrc950.u16Map(MU16_AT_MIN),wrc950.u16Map(MU16_AT_MAX)};

    // 動かしすぎ防止（200msに1回）
    if (millis() - last_move_ms < ACT_DIFF) return;
    last_move_ms = millis();

    // 音量に応じて可動範囲を変える
    int angle_range = map(scaled_audio_level,
                        min_audio_level, max_audio_level,
                        yaw_range[0], yaw_range[1]);   // 小音量:±30°, 大音量:±90°
    angle_range = constrain(angle_range, yaw_range[0], yaw_range[1]);

    int angle_range_throtle = map(scaled_audio_level,
                        min_audio_level, max_audio_level,
                        throtle_range[0], throtle_range[1]);
    angle_range_throtle = -1*constrain(angle_range_throtle, throtle_range[0], throtle_range[1]); //実機に合わせて符号反転

    int16_t target_throtle = random(angle_range_throtle,0);
    int16_t target_yaw = random(-angle_range, angle_range);

    if((wrc950.u8Map(MU8_E_SVU) & 0x01)==0) wrc950.s16Map(MS16_D_SV0,target_yaw*10);
    if(((wrc950.u8Map(MU8_E_SVU)>>1)&0x01)==0) wrc950.s16Map(MS16_D_SV1,target_throtle*10);
}

void servoInit(){
    servoInit(control_mode);
}

void servoInit(int mode){
    if(mode==0){
        wrc950.s16Map(MS16_CD_SV0,0);
        wrc950.s16Map(MS16_CD_SV1,0);
        servo_yaw.setPeriodHertz(SERVO_FREQ);      // Standard 50hz servo
        servo_yaw.attach(YAW_SERVO_PIN,SERVO_MIN,SERVO_MAX);
        servo_throtle.setPeriodHertz(SERVO_FREQ);      // Standard 50hz servo
        servo_throtle.attach(THROTLE_SERVO_PIN,SERVO_MIN,SERVO_MAX);
    }
    else if(mode==1){
        initOneWireUART();
        delay(SERIAL_SERVO_POWER_ON_WAIT_MS);
        wrc950.s16Map(MS16_D_SV0, 0);
        wrc950.s16Map(MS16_D_SV1, 0);
        //トルクオン、温度リミット取得
        for(int i=0;i<2;i++){
            unsigned char id = servo_param[i][0];
            for(int j=0;j<4;j++) wrc950.s16Map(servo_mem[i][j],servo_param[i][j+1]);
            servoEN(id, 0);
            setLimit(id, servo_param[i][2], servo_param[i][3]);
            changeSpeed(id, servo_param[i][4]);
            int init_target = servo_param[i][1];
            /*if(id == servo_param[1][0] && init_target == 0){
                init_target = SERIAL_SERVO_ID2_WAKE_ANGLE;
            }*/
            moveSerial(id, init_target,300);
            old_deg[i]=clampServoAngle(id, init_target);
            old_cd[i]=servo_param[i][1];
            delay(SERIAL_SERVO_INIT_INTERVAL_MS);
            servo_param[i][5]=readTempLimit(id);
            if(servo_param[i][5] != INT16_MIN){
                wrc950.u8Map(servo_temp_limit_mem[i], (uint8_t)servo_param[i][5]);
            }
        }
        delay(20);
        wrc950.u8Map(MU8_T_SVE,1);
        allServoOn();
        old_t_f=wrc950.u8Map(MU8_T_SVE);
    }
}

int servo_dps_mem[2] = {MU16_DPS_SV0, MU16_DPS_SV1};

/*  Futabaサーボコントローラ
    メモリマップ上の値に変化があったときのみサーボと通信 */
void servoCtrl(){
    uint32_t now = millis();
    if((uint32_t)(now - last_serial_servo_ctrl_ms) < SERIAL_SERVO_CTRL_INTERVAL_MS){
        return;
    }
    last_serial_servo_ctrl_ms = now;

    int n_t_f=wrc950.u8Map(MU8_T_SVE);  //現在のサーボイネーブル
    if(n_t_f!=old_t_f){
        if(n_t_f) {
            allServoOn();
        } else {
            allServoOff();
        }
        old_t_f = n_t_f;
    }
    if(!n_t_f) return;

    int16_t n_ld[2];    //現在の角度限界設定
    int16_t n_s;    //現在の遷移時間
    int16_t n_cd;   //現在の中心角度
    int16_t n_deg;  //現在の目標角度

    int i,j;
    /*サーボを制御*/
    for(i=0; i<2; i++){
        for(j=0; j<2; j++) n_ld[j] = wrc950.s16Map(servo_mem[i][j+1]);
        //メモリマップ上の角度限界設定値に変更があればサーボに書き込み
        bool limit_changed = (n_ld[0]!=servo_param[i][2] || n_ld[1]!=servo_param[i][3]);
        if(limit_changed){
            setLimit(servo_param[i][0],n_ld[0],n_ld[1]);
            for(j=0;j<2;j++) servo_param[i][j+2] = n_ld[j];
        }

        //メモリマップ上の中心角度の取得
        n_cd = wrc950.s16Map(servo_mem[i][0]);

        //メモリマップ上の目標角度、遷移時間の取得
        n_s = wrc950.s16Map(servo_mem[i][3]);
        n_deg = wrc950.s16Map(MS16_D_SV0+2*i);

        int16_t target = clampServoAngle(servo_param[i][0], n_deg+n_cd);
        bool move_changed = (target != old_deg[i]);
        bool dps_mode = (wrc950.u8Map(MU8_DPS_EN)==1);
        //DPSモードでは遷移時間を使わないため、変化を無視する
        bool speed_changed = !dps_mode && (n_s != servo_param[i][4]);
        bool center_changed = (n_cd != servo_param[i][1]);

        ServoTemperatureStatus temp_status;
        int temp=readTemp(servo_param[i][0], &temp_status);
        wrc950.u8Map(servo_temp_status_mem[i], temp_status.flags);
        if(temp != INT16_MIN){
            wrc950.u8Map(servo_temp_mem[i], (uint8_t)temp);
        }

        if(move_changed || speed_changed || center_changed || limit_changed){
            //moveSerial(servo_param[i][0], target, n_s);
            // Keep a failed target pending so the next control cycle retries it.
            if(dps_mode){
                if(moveServoDps(servo_param[i][0], target, wrc950.u16Map(servo_dps_mem[i]), temp)){
                    old_deg[i] = target;
                    servo_param[i][1]=n_cd;
                }
            }
            else if(moveServoTime(servo_param[i][0], target, n_s, temp)){
                old_deg[i] = target;
                servo_param[i][4]=n_s;
                servo_param[i][1]=n_cd;
            }
        }
        delay(1);
    }

}

void allServoOn(){
    for(int i=0; i<2; i++){
        unsigned char id = servo_param[i][0];
        int target = clampServoAngle(id, wrc950.s16Map(MS16_D_SV0+2*i) + wrc950.s16Map(servo_mem[i][0]));
        if(id == servo_param[1][0] && target == 0){
            moveSerial(id, SERIAL_SERVO_ID2_WAKE_ANGLE);
        } else {
            moveSerial(id, target);
        }
        servoEN(id, 1);
        if(id == servo_param[1][0] && target == 0){
            delay(SERIAL_SERVO_ID2_WAKE_DELAY_MS);
        }
        moveSerial(id, target);
        old_deg[i] = target;
        servo_param[i][1] = wrc950.s16Map(servo_mem[i][0]);
        delay(SERIAL_SERVO_INIT_INTERVAL_MS);
    }
}

void allServoOff(){
    for(int i=0; i<2; i++){
        servoEN(servo_param[i][0], 0);
    }
}

/* サーボのトルクのオンオフ　data: 0x00 or 0x01 */
void servoEN(unsigned char ID, unsigned char data){
    unsigned char CheckSum = 0; // チェックサム計算用変数

    // パケットデータ生成
    TxData[2] = ID;             // ID
    TxData[3] = 0x00;           // Flags
    TxData[4] = 0x24;           // Address
    TxData[5] = 0x01;           // Length
    TxData[6] = 0x01;           // Count
    TxData[7] = data;           // Data
    // チェックサム計算
    for(int i=2; i<=7; i++){
        CheckSum = CheckSum ^ TxData[i]; // ID～DATAまでのXOR
    }
    TxData[8] = CheckSum;       // Sum
    
    // パケットデータ送信
    sendFutabaPacket(8);
    clearRX();
} 

/* サーボの駆動　angle: -1500 ~ 1500 (-150.0 ~ 150.0[deg]) */
void moveSerial(unsigned char ID, int Angle){
    unsigned char CheckSum = 0; // チェックサム計算用変数

    // パケットデータ生成
    TxData[2] = ID;             // ID
    TxData[3] = 0x00;           // Flags
    TxData[4] = 0x1E;           // Address
    TxData[5] = 0x02;           // Length
    TxData[6] = 0x01;           // Count
    Angle = clampServoAngle(ID, Angle);
    // Angle
    TxData[7] = (unsigned char)0x00FF & Angle;           // Low byte
    TxData[8] = (unsigned char)0x00FF & (Angle >> 8);    // Hi  byte
    // チェックサム計算
    for(int i=2; i<=8; i++){
        CheckSum = CheckSum ^ TxData[i];  // ID～DATAまでのXOR
    }
    TxData[9] = CheckSum;       // Sum
    
    // パケットデータ送信
    sendFutabaPacket(9);
    clearRX();
}

int moveServoDps(unsigned char ID, int Angle, double degPerSec, int temp){
    if(degPerSec<=0.0) return 0;

    int index = servoIndexFromID(ID);
    if(index < 0) return 0;

    ServoTemperatureStatus status;
    double nowAngle = readAngle(ID, &status);
    double targetAngle = double(Angle)/10;
    double angle_diff=targetAngle-nowAngle;

    if (!status.valid) {
        /*Serial.print("ID:");
        Serial.print(ID);
        Serial.println(" Read failed");*/
        return 0;
    }
    if (status.limitError){
        /*Serial.print("ID:");
        Serial.print(ID);
        Serial.println(" Temperature limit error");*/
        return 0;
    }
    if (status.limitAlarm){
        /*Serial.print("ID:");
        Serial.print(ID);
        Serial.println(" Temperature limit alarm");*/
        return 0;
    }

    delay(1);

    int tempDiff=servo_param[index][5]-temp;
    double tempDiffGain=1.0;
    if(tempDiff<=10) tempDiffGain=0.5;
    else if(tempDiff<20) tempDiffGain=0.8;

    double dpsLimit = MAX_DPS * tempDiffGain;
    if(degPerSec>dpsLimit) degPerSec=dpsLimit;

    double transitionSec = fabs(angle_diff) / degPerSec;
    uint16_t goalTime = (uint16_t)ceil(transitionSec * 100.0);

    moveSerial(ID, Angle, goalTime);

    return 1;
}

int moveServoTime(unsigned char ID, int Angle, int Time, int temp){
    if(Time<=0) return 0;
    ServoTemperatureStatus status;
    double nowAngle = readAngle(ID, &status);
    double targetAngle = double(Angle)/10;
    double angle_diff=targetAngle-nowAngle;

    
    if (!status.valid) {
        /*Serial.print("ID:");
        Serial.print(ID);
        Serial.println(" Read failed");*/
        return 0;
    } else {
        if (status.limitError){
            /*Serial.print("ID:");
            Serial.print(ID);
            Serial.println(" Temperature limit error");*/
            return 0;
        }
        if (status.limitAlarm){
            /*Serial.print("ID:");
            Serial.print(ID);
            Serial.println(" Temperature limit alarm");*/
            return 0;
        }
    }

    delay(1);

    /*Serial.print(ID);
    Serial.print(" ");
    Serial.print(servo_param[ID-1][5]);
    Serial.print(" ");
    Serial.print(temp);
    Serial.print(" ");*/

    int tempDiff=servo_param[ID-1][5]-temp;
    double tempDiffGain=1.0;
    if(tempDiff<=10) tempDiffGain=0.5;
    else if(tempDiff<20) tempDiffGain=0.8;

    double degPerSec=fabs(angle_diff) / (double(Time) * 0.01);
    double dpsLimit = MAX_DPS * tempDiffGain;
    //Serial.println(dpsLimit);

    if(degPerSec>dpsLimit){
        degPerSec=dpsLimit;
        double transitionSec = fabs(angle_diff) / degPerSec;
        Time = (uint16_t)ceil(transitionSec * 100.0);
    }

    moveSerial(ID, Angle, Time);

    return 1;
}

/* サーボの駆動　angle: -1500 ~ 1500 (-150.0 ~ 150.0[deg]) speed : 0~16383 (遷移時間　10[ms]単位) */
void moveSerial(unsigned char ID, int Angle, int Speed){
    unsigned char CheckSum = 0; // チェックサム計算用変数

    // パケットデータ生成
    TxData[2] = ID;             // ID
    TxData[4] = 0x1E;           // Address
    TxData[3] = 0x00;           // Flags
    TxData[5] = 0x04;           // Length
    TxData[6] = 0x01;           // Count
    Angle = clampServoAngle(ID, Angle);
    // Angle
    TxData[7] = (unsigned char)0x00FF & Angle;           // Low byte
    TxData[8] = (unsigned char)0x00FF & (Angle >> 8);    // Hi  byte
    // Speed
    TxData[9] = (unsigned char)0x00FF & Speed;           // Low byte
    TxData[10] = (unsigned char)0x00FF & (Speed >> 8);   // Hi  byte
    // チェックサム計算
    for(int i=2; i<=10; i++){
        CheckSum = CheckSum ^ TxData[i];  // ID～DATAまでのXOR
    }
    TxData[11] = CheckSum;       // Sum
    
    // パケットデータ送信
    sendFutabaPacket(11);
    clearRX();
}

/* サーボの角度遷移時間の更新　speed : 0~16383 (遷移時間　10[ms]単位) */
void changeSpeed(unsigned char ID, int Speed){
    unsigned char CheckSum = 0; // チェックサム計算用変数

    // パケットデータ生成
    TxData[2] = ID;             // ID
    TxData[3] = 0x00;           // Flags
    TxData[4] = 0x20;           // Address
    TxData[5] = 0x02;           // Length
    TxData[6] = 0x01;           // Count
    // Speed
    TxData[7] = (unsigned char)0x00FF & Speed;           // Low byte
    TxData[8] = (unsigned char)0x00FF & (Speed >> 8);   // Hi  byte
    // チェックサム計算
    for(int i=2; i<=8; i++){
        CheckSum = CheckSum ^ TxData[i];  // ID～DATAまでのXOR
    }
    TxData[9] = CheckSum;       // Sum
    
    // パケットデータ送信
    sendFutabaPacket(9);
    clearRX();
}

void changeBaud(unsigned char ID, int rateNum){
    unsigned char CheckSum = 0; // チェックサム計算用変数

    // パケットデータ生成
    TxData[2] = ID;             // ID
    TxData[3] = 0x00;           // Flags
    TxData[4] = 0x06;           // Address
    TxData[5] = 0x01;           // Length
    TxData[6] = 0x01;           // Count
    // Speed
    TxData[7] = rateNum;           // Data
    // チェックサム計算
    for(int i=2; i<=7; i++){
        CheckSum = CheckSum ^ TxData[i];  // ID～DATAまでのXOR
    }
    TxData[8] = CheckSum;       // Sum
    
    // パケットデータ送信
    sendFutabaPacket(8);
    clearRX();
}

double readAngle(unsigned char ID){
    return readAngle(ID, nullptr);
}

double readAngle(unsigned char ID, ServoTemperatureStatus *temperatureStatus){
    ServoTemperatureStatus responseStatus;
    unsigned char CheckSum = 0; // チェックサム計算用変数

    // パケットデータ生成
    while(Serial2.available()){
        Serial2.read();
    }

    TxData[2] = ID;             // ID
    TxData[3] = 0x0F;           // Read the specified address without writing.
    TxData[4] = 0x2A;           // Current position
    TxData[5] = 0x02;           // Two bytes
    TxData[6] = 0x00;           // Read only
    // チェックサム計算
    for(int i=2; i<=6; i++){
        CheckSum = CheckSum ^ TxData[i]; // ID～DATAまでのXOR
    }
    TxData[7] = CheckSum;       // Sum
    
    // パケットデータ送信
    last_read_angle_id = ID;
    for(uint8_t attempt=0; attempt<SERIAL_SERVO_READ_ATTEMPTS; attempt++){
        clearRX();
        sendFutabaPacket(7);
        int angle = waitReadAngle(&responseStatus);
        if(responseStatus.valid){
            if(temperatureStatus) *temperatureStatus = responseStatus;
            return double(angle)/10;
        }
    }
    if(temperatureStatus) *temperatureStatus = responseStatus;
    return double(INT16_MIN)/10;
}

int readTempLimit(unsigned char ID, ServoTemperatureStatus *temperatureStatus){
    ServoTemperatureStatus responseStatus;
    unsigned char CheckSum = 0; // チェックサム計算用変数

    // パケットデータ生成
    while(Serial2.available()){
        Serial2.read();
    }

    TxData[2] = ID;             // ID
    TxData[3] = 0x0F;           // Read the specified address without writing.
    TxData[4] = 0x0E;           // Temperature limit
    TxData[5] = 0x02;           // Two bytes
    TxData[6] = 0x00;           // Read only
    // チェックサム計算
    for(int i=2; i<=6; i++){
        CheckSum = CheckSum ^ TxData[i]; // ID～DATAまでのXOR
    }
    TxData[7] = CheckSum;       // Sum
    
    // パケットデータ送信
    last_read_angle_id = ID;
    for(uint8_t attempt=0; attempt<SERIAL_SERVO_READ_ATTEMPTS; attempt++){
        clearRX();
        sendFutabaPacket(7);
        int temp = waitServoValue(0x0E, 2, &responseStatus);
        if(responseStatus.valid){
            if(temperatureStatus) *temperatureStatus = responseStatus;
            return temp;
        }
    }
    if(temperatureStatus) *temperatureStatus = responseStatus;
    return INT16_MIN;
}

int readTemp(unsigned char ID, ServoTemperatureStatus *temperatureStatus){
    ServoTemperatureStatus responseStatus;
    unsigned char CheckSum = 0; // チェックサム計算用変数

    // パケットデータ生成
    while(Serial2.available()){
        Serial2.read();
    }

    TxData[2] = ID;             // ID
    TxData[3] = 0x0F;           // Read the specified address without writing.
    TxData[4] = 0x32;           // Current temp
    TxData[5] = 0x02;           // Two bytes
    TxData[6] = 0x00;           // Read only
    // チェックサム計算
    for(int i=2; i<=6; i++){
        CheckSum = CheckSum ^ TxData[i]; // ID～DATAまでのXOR
    }
    TxData[7] = CheckSum;       // Sum
    
    // パケットデータ送信
    last_read_angle_id = ID;
    for(uint8_t attempt=0; attempt<SERIAL_SERVO_READ_ATTEMPTS; attempt++){
        clearRX();
        sendFutabaPacket(7);
        int temp = waitServoValue(0x32, 2, &responseStatus);
        if(responseStatus.valid){
            if(temperatureStatus) *temperatureStatus = responseStatus;
            return temp;
        }
    }
    if(temperatureStatus) *temperatureStatus = responseStatus;
    return INT16_MIN;
}

int waitReadAngle(){
    return waitReadAngle(nullptr);
}

int waitReadAngle(ServoTemperatureStatus *temperatureStatus){
    return waitServoValue(0x2A, 2, temperatureStatus);
}

bool readServoTorque(unsigned char ID, uint8_t &torque, ServoTemperatureStatus *temperatureStatus){
    ServoTemperatureStatus responseStatus;
    last_read_angle_id = ID;
    TxData[2] = ID;
    TxData[3] = 0x0F;
    TxData[4] = 0x24;
    TxData[5] = 0x01;
    TxData[6] = 0x00;
    TxData[7] = 0;
    for(int i=2; i<=6; i++) TxData[7] ^= TxData[i];

    for(uint8_t attempt=0; attempt<SERIAL_SERVO_READ_ATTEMPTS; attempt++){
        clearRX();
        sendFutabaPacket(7);
        int value = waitServoValue(0x24, 1, &responseStatus);
        if(responseStatus.valid){
            torque = (uint8_t)value;
            if(temperatureStatus) *temperatureStatus = responseStatus;
            return true;
        }
    }
    if(temperatureStatus) *temperatureStatus = responseStatus;
    return false;
}

static int waitServoValue(uint8_t address, uint8_t minimumLength, ServoTemperatureStatus *temperatureStatus){
    if(temperatureStatus){
        *temperatureStatus = ServoTemperatureStatus{};
    }
    uint8_t buffer[26];
    int index = 0;
    uint32_t start_ms = millis();

    while((uint32_t)(millis() - start_ms) < SERIAL_SERVO_READ_TIMEOUT_MS){
        if(!Serial2.available()){
            delay(1);
            continue;
        }

        uint8_t data = Serial2.read();
        if(index == 0){
            if(data != 0xFD) continue;
            buffer[index++] = data;
            continue;
        }
        if(index == 1){
            if(data == 0xDF){
                buffer[index++] = data;
            }else{
                index = (data == 0xFD) ? 1 : 0;
            }
            continue;
        }

        if(index >= (int)sizeof(buffer)){
            index = 0;
            continue;
        }
        buffer[index++] = data;
        if(index < 7) continue;

        int packet_size = 7 + buffer[5] * buffer[6] + 1;
        if(packet_size > (int)sizeof(buffer)){
            index = 0;
            continue;
        }
        if(index < packet_size) continue;

        unsigned char CheckSum = 0;
        for(int i=2; i<packet_size-1; i++){
            CheckSum = CheckSum ^ buffer[i];
        }

        bool valid_packet = (buffer[packet_size-1] == CheckSum)
                            && (last_read_angle_id == 0 || buffer[2] == last_read_angle_id)
                            && (buffer[4] == address)
                            && (buffer[5] >= minimumLength)
                            && (buffer[6] == 0x01);
        if(valid_packet){
            if(temperatureStatus){
                temperatureStatus->valid = (buffer[3] & 0x02) == 0;
                temperatureStatus->limitError = (buffer[3] & 0x80) != 0;
                temperatureStatus->limitAlarm = (buffer[3] & 0x20) != 0;
                temperatureStatus->flags = buffer[3];
            }
            // A checked response can still report that the request was rejected.
            if(buffer[3] & 0x02) return INT16_MIN;
            if(minimumLength == 1) return buffer[7];
            return (int16_t)(buffer[7] | (buffer[8] << 8));
        }

        index = 0;
    }

    clearRX();

    return INT16_MIN;
}

void setLimit(unsigned char ID, int uLimit, int lLimit){
    unsigned char CheckSum = 0; // チェックサム計算用変数

    // パケットデータ生成
    TxData[2] = ID;             // ID
    TxData[3] = 0x00;           // Flags
    TxData[4] = 0x08;           // Address
    TxData[5] = 0x04;           // Length
    TxData[6] = 0x01;           // Count
    // Angle
    TxData[7] = (unsigned char)0x00FF & uLimit;           // Low byte
    TxData[8] = (unsigned char)0x00FF & (uLimit >> 8);    // Hi  byte
    // Speed
    TxData[9] = (unsigned char)0x00FF & lLimit;           // Low byte
    TxData[10] = (unsigned char)0x00FF & (lLimit >> 8);   // Hi  byte
    // チェックサム計算
    for(int i=2; i<=10; i++){
        CheckSum = CheckSum ^ TxData[i];  // ID～DATAまでのXOR
    }
    TxData[11] = CheckSum;       // Sum
    
    // パケットデータ送信
    sendFutabaPacket(11);
    clearRX();

    int index = servoIndexFromID(ID);
    if(index >= 0){
        servo_param[index][2] = (int16_t)uLimit;
        servo_param[index][3] = (int16_t)lLimit;
    }
}

void moveServo(){
    if(control_mode==0) movePWM();
    else servoCtrl();
}
