// Copyright 2026 Vstone Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

#include "vs_wrc950_memmap.h"
#include "vs_wrc950_i2c.h"
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>

#define POS_OF_I2CADDR 1

/**
 * メモリマップ初期値(0x10~0x8f) 
 * シャットダウン電圧 11.0V
 */
const uint8_t initialMemmap[MAP_SIZE] = {0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                         0x00, 0x01, 0x00, 0x01, 0x08, 0x00, 0x08, 0x00, 0x40, 0x00, 0x40, 0x00, 0x00, 0x10, 0x00, 0x10,
                                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};


//16進数をアスキー文字に変換するテーブル
//itoa[a] = 'A'
//itoa[3] = '3'
static char hexToA[] = "0123456789ABCDEF";

//アスキー文字を16進数に変換
// '1' => 0x01, 'a' => 0x0a
uint8_t cToHex(uint8_t c){
  
  // '0'～'9'
  if (isDigit(c)) {
    return (c - 0x30);
  }
  // 'A'～'F'
  if (isUpperCase(c)) {
    return (c - 0x37);
  }
  // 'a'～'f'
  if (isLowerCase(c)) {
    return (c - 0x57);
  }
  return 0;
}

Wrc05x::Wrc05x(uint8_t id){
  devId = id;
}

Wrc950::Wrc950(uint8_t id):Wrc05x(id){
}


//0x10-0x1fの値を定期的にアップデートする
uint16_t wdTime = 0;
void Wrc950::updateSysMem(){
  memUpCounter++;
  if(memUpCounter > 0x0d){
    memUpCounter = 0;
  }
  if(wdTime != 0){
      u16Map(MU16_WDT, wdTime);
  }
  return;
}

void Wrc950::initMemmap(double cutOffLevel){
  //電源カットオフ電圧をメモリマップ用数値に変換
  uint16_t cutOffHex = (uint16_t)((cutOffLevel/56.1)*0xfff);

  //シャットダウン電圧の設定
  Wrc950::u16Map(MU16_SD_VI, cutOffHex); 

  //システム名設定
  Wrc950::u8Map(MU16_SYSNAME, 0x30);
  Wrc950::u8Map(MU16_FIRMREV, 0x07);

  delay(20);

  //緑LED点灯
  Wrc950::u8Map(MU8_CR0, Wrc950::u8Map(MU8_CR0) & ~0x02);

  //WDTリセット
  Wrc950::u16Map(MU16_WDT, wdTime);
  
}

//memMapのクリーン
void Wrc05x::memMapClean(){  
  int i = 0;
  for(i = 0; i<MAP_SIZE; i++){
    memMap[i] = 0x00;
    writeFlag[i] = 0x00;
  }

  return;
}

//1-64byteをSTM32のメモリマップから読み込む
int Wrc05x::readMemmap(uint8_t addr, uint8_t readLength){
  return readLength;
}


int Wrc950::readMemmap(uint8_t addr, uint8_t readLength){
  return readLength;
}

//STM32の全てのメモリマップを読み込み。
int Wrc05x::readAll(){

  readMemmap(0x00,64);
  readMemmap(0x40,64);
  readMemmap(0x80,64);
  readMemmap(0xC0,64);

  return 1;
}

int Wrc950::writeMemmap(uint8_t addr, uint8_t data[], uint8_t writeLength){
  return 1;
}

//デバイスアドレスの取得
uint8_t Wrc05x::getAddr(){
  return devId;
}

//シリアルからの接続での呼び出し用
void Wrc05x::checkMsg(String rcvMsg){
  Wrc05x::checkMsg(rcvMsg, NULL, NULL);
}

//WiFiからの接続での呼び出し用
void Wrc05x::checkMsg(String rcvMsg, WiFiClient* client){
  Wrc05x::checkMsg(rcvMsg, client, NULL);
}

//BLEからの接続での呼び出し用
void Wrc05x::checkMsg(String rcvMsg, uint8_t viaBle){
  Wrc05x::checkMsg(rcvMsg, NULL, viaBle);
}

void Wrc05x::checkMsg(String rcvMsg, WiFiClient* client, uint8_t viaBle){
  //改行も含めた全体の文字数が奇数ならフォーマットエラー
  if(rcvMsg.length()%2){
    Serial.println("ERR: The number of characters is odd");
    return;
  }
  
  //コマンドの種類判定
  if(rcvMsg[0] == 'r' || rcvMsg[0] == 'R' || rcvMsg[0] == 'w' || rcvMsg[0] == 'W'){
    struct cmd rcvCmd;
    
    rcvCmd.cmdType = rcvMsg[0];

    //spaceチェック
    if(!isSpace(rcvMsg[1])){
      Serial.println("ERR: Cmd format");
      return;
    }

    //メモリマップのアドレスチェック
    if(isHexadecimalDigit(rcvMsg[2]) && isHexadecimalDigit(rcvMsg[3])){
      rcvCmd.addr = ((cToHex(rcvMsg[2]) << 4) | (cToHex(rcvMsg[3])));
    }

    //spaceチェック
    if(!isSpace(rcvMsg[4])){
      Serial.println("ERR: Cmd format");
      return;
    }

    //読み込みと書込みで分岐
    //読み込み
    if(rcvCmd.cmdType == 'r' || rcvCmd.cmdType == 'R'){
      //コマンド長チェック
      if(rcvMsg.length() == 8){
        //読み込みバイト数チェック
        if(isHexadecimalDigit(rcvMsg[5]) && isHexadecimalDigit(rcvMsg[6])){
          rcvCmd.readLength = ((cToHex(rcvMsg[5]) << 4) | (cToHex(rcvMsg[6])));
          Wrc05x::sendMap2pc(rcvCmd, client, viaBle);
          return;
        }
      }else{
        Serial.println("ERR: Read format");
        return;
      }

    //書込み
    }else{
      
      int i = 5;
      int j = 0;
      uint8_t value[MAP_SIZE];

      //受信した2文字ずつ一バイトの16進数に変換
      //（例） '2','f' => 0x2f
      while(rcvMsg[i] != '\n'){
        if(isHexadecimalDigit(rcvMsg[i]) && isHexadecimalDigit(rcvMsg[i+1])){
          value[j] = ((cToHex(rcvMsg[i]) << 4) | (cToHex(rcvMsg[i+1])));
          i += 2;
          j++;
                    
        }else{
          rcvCmd.valueCount = 0;
          Serial.println("ERR: Write format");
          return;
        }
      }
      rcvCmd.valueCount = j;
      rcvCmd.value = value;

      Wrc05x::setWriteMapViaMsg(rcvCmd);
      return;
    }    
  }

  Serial.println("ERR: format");
  return;
}

void Wrc05x::sendMap2pc(struct cmd rcvCmd){
  Wrc05x::sendMap2pc(rcvCmd, NULL, NULL);
}

void Wrc05x::sendMap2pc(struct cmd rcvCmd, WiFiClient* client){
  Wrc05x::sendMap2pc(rcvCmd, client, NULL);
}

void Wrc05x::sendMap2pc(struct cmd rcvCmd, WiFiClient* client, uint8_t viaBlt){

  //読み込みコマンドか確認
  if(rcvCmd.cmdType == 'r' || rcvCmd.cmdType == 'R'){
    int i = 0;
    int j = 0;
    char msg[MAP_SIZE];

    if(rcvCmd.readLength == 0x00){  //読み込みbyteが0なら、メモリマップ全体を送信
      Wrc05x::readAll();

      msg[2] = '\0';

      Serial.println("     0|  1|  2|  3|  4|  5|  6|  7|  8|  9|  A|  B|  C|  D|  E|  F");

      for(i = 0; i < 16; i++){
        Serial.print(i,HEX);
        Serial.print("0");
        Serial.print(": ");

        for(j = 0; j < 16; j++){
          msg[0]= hexToA[memMap[(i*16)+j] >> 4];
          msg[1]= hexToA[memMap[(i*16)+j] & 0x0f];
          Serial.print(msg);
          Serial.print("  ");
        }
        Serial.println();
      }
      Serial.println();
      
    }else{
      readMemmap(rcvCmd.addr, rcvCmd.readLength);

      for(i = 0; i < rcvCmd.readLength; i++){
          msg[i*2]= hexToA[memMap[rcvCmd.addr + i] >> 4];
          msg[(i*2)+1]= hexToA[memMap[rcvCmd.addr + i] & 0x0f];
      }
      msg[i*2] = '\0';
      Serial.println(msg);
      
      if(client != NULL){
        client->println("HTTP/1.1 200 OK");
        client->println("Content-type:text/html");
        client->println();
        client->print(msg);
        client->println();
      }      
      
    }
  }
}

void Wrc05x::setWriteMapViaMsg(struct cmd rcvCmd){

  //書込みコマンドか確認
  if(rcvCmd.cmdType == 'w' || rcvCmd.cmdType == 'W'){
    //Wrc950::writeMemmap(rcvCmd.addr, rcvCmd.value, rcvCmd.valueCount);
    int i;
    for(i = 0; i < rcvCmd.valueCount; i++){
      Wrc05x::u8Map(rcvCmd.addr +i, *(rcvCmd.value +i));
    }
    
  }
}

//符号付き8bitで読み出し
int8_t Wrc05x::s8Map(uint8_t addr){
  return (*(int8_t *)(&memMap[addr]));
}
//符号付き8bitで書き込み
int8_t Wrc05x::s8Map(uint8_t addr, int8_t data){
  (*(int8_t *)(&memMap[addr])) = data;
  (*(uint8_t *)(&writeFlag[addr])) = 0x01;
  return (*(int8_t *)(&memMap[addr]));
}

//符号なし8bitで読み出し
uint8_t Wrc05x::u8Map(uint8_t addr){
  return (*(uint8_t *)(&memMap[addr]));
}
//符号なし8bitで書き込み
uint8_t Wrc05x::u8Map(uint8_t addr, uint8_t data){
    (*(uint8_t *)(&memMap[addr])) = data;
    (*(uint8_t *)(&writeFlag[addr])) = 0x01;
  return (*(uint8_t *)(&memMap[addr]));
}

//符号付き16bitで読み出し
int16_t Wrc05x::s16Map(uint8_t addr){
  return (*(int16_t *)(&memMap[addr]));
}
//符号付き16bitで書き込み
int16_t Wrc05x::s16Map(uint8_t addr, int16_t data){
    (*(int16_t *)(&memMap[addr])) = data;
    (*(uint16_t *)(&writeFlag[addr])) = 0x0101;
  return (*(int16_t *)(&memMap[addr]));
}

//符号なし16bitで読み出し
uint16_t Wrc05x::u16Map(uint8_t addr){
  return (*(uint16_t *)(&memMap[addr]));
}
//符号なし16bitで書き込み
uint16_t Wrc05x::u16Map(uint8_t addr, uint16_t data){
    (*(uint16_t *)(&memMap[addr])) = data;
    (*(uint16_t *)(&writeFlag[addr])) = 0x0101;
  return (*(uint16_t *)(&memMap[addr]));
}

//符号付き32bitで読み出し
int32_t Wrc05x::s32Map(uint8_t addr){
  return (*(int32_t *)(&memMap[addr]));
}
//符号付き32bitで書き込み
int32_t Wrc05x::s32Map(uint8_t addr, int32_t data){
    (*(int32_t *)(&memMap[addr])) = data;
    (*(uint32_t *)(&writeFlag[addr])) = 0x01010101;
  return (*(int32_t *)(&memMap[addr]));
}

//符号なし32bitで読み出し
uint32_t Wrc05x::u32Map(uint8_t addr){
  return (*(uint32_t *)(&memMap[addr]));
}
//符号なし32bitで書き込み
uint32_t Wrc05x::u32Map(uint8_t addr, uint32_t data){
    (*(uint32_t *)(&memMap[addr])) = data;
    (*(uint32_t *)(&writeFlag[addr])) = 0x01010101;
  return (*(uint32_t *)(&memMap[addr]));
}
  
//writeFlagの状態を確認
uint8_t Wrc05x::checkWriteFlag(uint8_t addr){
  return writeFlag[addr];
}

int checkI2cAddrOfMsg(String rcvMsg, int rcvMsgCount){
  return checkI2cAddrOfMsg(rcvMsg, rcvMsgCount, NULL);
}

int checkI2cAddrOfMsg(String rcvMsg, int rcvMsgCount, uint8_t viaBle){
  //正常なコマンドかどうかを確認
  if(rcvMsg[rcvMsgCount] != '\n'){  //rcvMsgの末尾が改行コードかどうかを確認
    Serial.println("ERR: no LF");
    return 0;
      
  }else if(rcvMsgCount >= 7){       //rcvMsgが改行コードを含めて8文字以上あるかどうか
    //正常なコマンド
    //I2Cアドレスを復号し，文字列から削除
    uint8_t i2cAddr = 0xff;
    if(isHexadecimalDigit(rcvMsg[POS_OF_I2CADDR]) && isHexadecimalDigit(rcvMsg[POS_OF_I2CADDR+1])){
      i2cAddr = ((cToHex(rcvMsg[POS_OF_I2CADDR]) << 4) | (cToHex(rcvMsg[POS_OF_I2CADDR+1])));
      rcvMsg.remove(1, 2);
    }else if(rcvMsg[POS_OF_I2CADDR] == ' '){
      //I2Cアドレスが無いor構文に誤りがある可能性がある場合
      //2文字目が空白であればアドレス指定が無いものとして、i2cアドレスを0x10とする
      i2cAddr = 0x10;        
    }

    //i2cAddrの内容によって呼び出すメモリマップを振り分け
    if(i2cAddr == 0x10){
      wrc950.checkMsg(rcvMsg, viaBle);
    }else{
      Serial.print("ERR: invalid devID addr");
      printf(" 0x%2x \n", i2cAddr);
      return 0;
    }
      
  }else if(rcvMsgCount == 0){       //改行コードのみを受信
    Serial.println('\n');
    return 0;
      
  }else{                            //コマンドが短い
    Serial.println("ERR: Msg length too short");
    return 0;
  }

  return 1;    
  
}


Wrc950 wrc950(0x10);
