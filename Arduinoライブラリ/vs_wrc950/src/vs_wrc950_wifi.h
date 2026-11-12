// Copyright 2026 Vstone Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

/**
 * VS-WRC950用 Wi-Fi接続とブラウザからのパラメータ設定用Webサーバ
 */
#ifndef WRC950_WIFI_H
#define WRC950_WIFI_H

#include "vs_wrc950_memmap.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

#define WIFI_CONNECT_TIMEOUT_MS 10000   //アクセスポイントへの接続待ち時間[ms]
#define DEVICE_NAME_MAX_LEN 32          //デバイス名の最大文字数。Wi-FiのSSIDの上限に合わせる
#define WIFI_AP_RESTART_DELAY_MS 3000   //Bluetooth切断後、自身のアクセスポイントを再開するまでの時間[ms]

/*
 * Bluetooth接続中の自身のアクセスポイントの扱い
 * 1: 電波を止める(SSIDも見えなくなる)
 * 0: 電波は止めず、設定画面とAPIへのアクセスを拒否する(503を返す)
 * 0で、Bluetooth切断後に設定画面を開けることを実機で確認済み。1は再開後の動作を確認していない
 */
#define WIFI_AP_PAUSE_ON_BT 0

/*
 * 1にすると、アクセスポイントの停止・再開や端末の接続・切断を空きメモリと一緒にSerialへ出力する
 * Wi-Fiに接続できない場合の調査用。ROSをシリアルで使う場合は通信を妨げるため0にすること
 */
#define WIFI_AP_DEBUG 0

extern WebServer webServer;

/*
 * Wi-Fiに接続する
 * ssidが空、または接続できなかった場合は、ap_ssid/ap_passwordで自身がアクセスポイントになる
 * ap_passwordが空、または8文字未満の場合はパスワード無しのアクセスポイントになる
 * 自身がアクセスポイントになった場合、Bluetooth接続中はアクセスポイントを停止する(handleWebServer()で切り替え)
 * ROSのシリアル通信を妨げないよう、WIFI_AP_DEBUGが0のときはSerialに出力しない
 * 戻り値: アクセスポイントに接続できればtrue、自身がアクセスポイントになった場合はfalse
 */
bool wifiInit(const char* ssid, const char* password, const char* ap_ssid, const char* ap_password);

/*
 * パラメータ設定用のWebサーバを開始する
 * 設定画面はLittleFS上の/index.htmlを返すため、スケッチのdataフォルダを書き込んでおくこと
 * config_path: 保存ボタンで書き換えるLittleFS上の設定ファイルのパス
 */
void serverInit(const char* config_path);

/*
 * Webサーバへのリクエストを処理する。loop()から呼び出す
 * 自身がアクセスポイントの場合は、Bluetoothの接続状態に応じてアクセスポイントを停止・再開する
 */
void handleWebServer();

/*
 * 現在のMU16_AUD_GAINの値をLittleFS上の設定ファイルに保存する
 * 設定ファイル内の他のキーは保持する
 */
bool saveAudioGain(const char* config_path);

/*
 * デバイス名として使えるか調べる
 * Bluetoothのデバイス名と、自身がアクセスポイントになる場合のSSIDに使うため、
 * どちらでも扱える半角英数字と記号(空白を含む)の1～DEVICE_NAME_MAX_LEN文字に限る
 */
bool isValidDeviceName(const char* name);

/*
 * デバイス名をLittleFS上の設定ファイルの"device_name"に保存する
 * 反映されるのは次回の起動時。設定ファイル内の他のキーは保持する
 */
bool saveDeviceName(const char* config_path, const char* name);

#endif /* WRC950_WIFI_H */
