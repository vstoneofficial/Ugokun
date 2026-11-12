// Copyright 2026 Vstone Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

/**
 * ESP32で使用するメモリマップに関する定義
 * 
 */
#ifndef Wrc950_MEMMAP_H
#define Wrc950_MEMMAP_H

#include "vs_wrc950_i2c.h"
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>

//メモリマップ 
#define MAP_SIZE 0x100

//メモリマップ読み書きマクロ
#define R_MU8(x)  (*(uint8_t  *)(&wheel.memMap[x]))
#define R_MU16(x) (*(uint16_t *)(&wheel.memMap[x]))
#define R_MU32(x) (*(uint32_t *)(&wheel.memMap[x]))

#define R_MS8(x)  (*(int8_t   *)(&wheel.memMap[x]))
#define R_MS16(x) (*(int16_t  *)(&wheel.memMap[x]))
#define R_MS32(x) (*(int32_t  *)(&wheel.memMap[x]))




//SerialやBluetooth,Wi-Fiで受信した命令を格納する構造体
struct cmd{
  uint8_t cmdType;   //(読込)r, （書込）w
  uint8_t addr;     //読み書きするメモリマップの先頭アドレス
  uint8_t *value;   //書き込む値
  uint8_t valueCount;      //書き込む値の長さ
  uint8_t readLength;   //読みこみで先頭アドレスから何バイト読み込むか 0x00~0xff
};

enum memmapAddr{
  MU16_SYSNAME  = 0x00,  /* R  システム名     */
  MU16_FIRMREV  = 0x02,  /* R  ファームウェアリビジョン  */
  MU32_TRIPTIME = 0x04,  /* R  経過時間(ms)  */
  MU16_WDT      =	0x08,	 /* R/W	ウォッチドッグタイマー(ms) */
/*                              0で何もしない、0xffffでモータ停止、それ以外でカウントダウン */
	MU8_CR0       =	0x0c,	 /* R	b0: 1でLED消灯 */
  MU16_POWOFF_T = 0x0e,  /* R/W	 msec後に電源が切れる */  
  MU16_SD_VI    = 0x12,  /* R/W  シャットダウン電圧*/

  MU8_SV_TEMPERR1 = 0x20,
  MU8_SV_TEMPERR2 = 0x21,
  MU8_SV_TEMPLIMIT1 = 0x22, /* R  サーボ1温度リミット[degC] */
  MU8_SV_TEMPLIMIT2 = 0x23, /* R  サーボ2温度リミット[degC] */
  MU8_SV_TEMP1 = 0x24,      /* R  サーボ1現在温度[degC] */
  MU8_SV_TEMP2 = 0x25,      /* R  サーボ2現在温度[degC] */

  MU8_DPS_EN    = 0x30,  /* R/W DPS制御イネーブル */

  MU8_E_SVU    = 0x49,  /* R    b0:1でサーボ1外部制御
                                b1:1でサーボ2外部制御
                           */
  MU8_T_SVE    = 0x50,  /* R/W サーボトルクイネーブル 0 or 1 */
  MS16_D_SV0    = 0x51,  /* R/W サーボ1目標角度[deg/10] */
  MS16_D_SV1    = 0x53,  /* R/W サーボ2目標角度[deg/10] */
  MU16_S_SV0    = 0x55,  /* R/W サーボ1遷移時間[ms/10] */
  MU16_S_SV1    = 0x57,  /* R/W サーボ2遷移時間[ms/10] */
  MS16_CD_SV0   = 0x59,  /* R/W サーボ1中心角度[deg/10] */
  MS16_CD_SV1   = 0x5b,  /* R/W サーボ2中心角度[deg/10] */
  MS16_ULD_SV0   = 0x6d,  /* R/W サーボ1上限角度[deg/10] */
  MS16_ULD_SV1   = 0x6f,  /* R/W サーボ2上限角度[deg/10] */
  MS16_LLD_SV0   = 0x71,  /* R/W サーボ1下限角度[deg/10] */
  MS16_LLD_SV1   = 0x73,  /* R/W サーボ2下限角度[deg/10] */
  MU16_DPS_SV0    = 0x75,  /* R/W サーボ1遷移速度[deg/sec] */
  MU16_DPS_SV1    = 0x77,  /* R/W サーボ2遷移速度[deg/sec] */
  MU32_MOV_T0   = 0x79,  /* R/W  動作するかの基準時間(ms)*/
  MU32_AUD_VOL   = 0x7d, /* R  入力されたオーディオの音量レベル*/
  MU32_AUD_MIN   = 0x82, /* R/W  サーボを動作させる最低音量レベル*/
  MU32_AUD_MAX   = 0x86, /* R/W  サーボを動作させる最低音量レベル*/
  MU16_AUD_GAIN  = 0x8a, /* R/W  動作感度。可動範囲を決める際に音量レベルへ掛ける倍率の100倍(100で1倍、250で2.5倍)。0は100として扱う*/
  MU16_AY_MAX   = 0x8c,  /* R/W オーディオ制御時ヨー軸可動範囲の最大[deg] */
  MU16_AT_MAX   = 0x8e,  /* R/W オーディオ制御時スロットル軸可動範囲の最大[deg]（設定画面ではmmに換算して表示） */
  MU16_AY_MIN   = 0x91,  /* R/W オーディオ制御時ヨー軸可動範囲の最小[deg] */
  MU16_AT_MIN   = 0x93,  /* R/W オーディオ制御時スロットル軸可動範囲の最小[deg] */
  
};

//メモリマップ初期値
//0x10から0x8fまでのメモリマップ初期化用
extern const uint8_t initialMemmap[MAP_SIZE];


//Wrc950のメモリマップクラス
class Wrc05x{
  public:
  Wrc05x(uint8_t id);
  uint8_t devId;
  uint8_t memMap[MAP_SIZE];
  uint8_t writeFlag[MAP_SIZE];
  uint8_t memUpCounter;


  virtual void initMemmap(double cutOffLevel);
  void memMapClean();
  virtual int readMemmap(uint8_t addr, uint8_t readLength);
  int readAll();
  virtual int writeMemmap(uint8_t addr, uint8_t data[], uint8_t writeLength);
  uint8_t getAddr();

  void checkMsg(String rcvMsg);
  void checkMsg(String rcvMsg, WiFiClient* client);
  void checkMsg(String rcvMsg, uint8_t viaBle);
  void checkMsg(String rcvMsg, WiFiClient* client, uint8_t viaBlt);
  void sendMap2pc(struct cmd rcvCmd);
  void sendMap2pc(struct cmd rcvCmd, WiFiClient* client);
  void sendMap2pc(struct cmd rcvCmd, WiFiClient* client, uint8_t viaBle);
  void setWriteMapViaMsg(struct cmd rcvCmd);

  int8_t   s8Map(uint8_t addr);
  int8_t   s8Map(uint8_t addr, int8_t data);
  uint8_t  u8Map(uint8_t addr);
  uint8_t  u8Map(uint8_t addr, uint8_t data);
  int16_t  s16Map(uint8_t addr);
  int16_t  s16Map(uint8_t addr, int16_t data);
  uint16_t u16Map(uint8_t addr);
  uint16_t u16Map(uint8_t addr, uint16_t data);
  int32_t  s32Map(uint8_t addr);
  int32_t  s32Map(uint8_t addr, int32_t data);
  uint32_t u32Map(uint8_t addr);
  uint32_t u32Map(uint8_t addr, uint32_t data);

  uint8_t checkWriteFlag(uint8_t addr);

  double getVin();

};

class Wrc950 : public Wrc05x{
  public:
  Wrc950(uint8_t id);
  void updateSysMem();

  void initMemmap(double cutOffLevel);
  int readMemmap(uint8_t addr, uint8_t readLength);
  int writeMemmap(uint8_t addr, uint8_t data[], uint8_t writeLength);

};

extern Wrc950 wrc950;

//extern static char hexToA[16];
uint8_t cToHex(uint8_t c);
int checkI2cAddrOfMsg(String rcvMsg, int rcvMsgCount);
int checkI2cAddrOfMsg(String rcvMsg, int rcvMsgCount, uint8_t viaBle);

//ウォッチドッグタイマ設定値
extern uint16_t wdTime;

#endif /* MEMMAP_H */


