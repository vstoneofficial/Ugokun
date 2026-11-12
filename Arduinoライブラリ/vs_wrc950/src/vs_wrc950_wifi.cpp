// Copyright 2026 Vstone Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

#include "vs_wrc950_wifi.h"
#include "vs_wrc950_memmap.h"
#include "vs_wrc950_servo.h"
#include "vs_wrc950_btaudio.h"
#include "vs_wrc950_testaudio.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <esp_wifi.h>
#include <esp_coexist.h>

WebServer webServer(80);

//自身がアクセスポイントになった場合の設定。Bluetooth接続中の扱いはWIFI_AP_PAUSE_ON_BTで切り替える
static bool isApMode = false;
static bool isApRunning = false;
static String apSsid;
static String apPassword;
#if WIFI_AP_PAUSE_ON_BT
static uint32_t btConnectedTime = 0;  //最後にBluetooth接続中を確認した時刻[ms]
#endif

static const char* configFilePath = "/config.json";

//設定画面のHTML。スケッチのdataフォルダからLittleFSに書き込んでおく
static const char* indexFilePath = "/index.html";

/*
 * ブラウザから読み書きするパラメータの一覧
 * ここに追加すると画面とAPIの両方に反映される
 */
enum webParamType{
  PARAM_U16 = 0,
  PARAM_U32 = 1,
  PARAM_U16_DEG_AS_MM = 2  //メモリマップはスロットル軸の角度[deg]。APIと画面ではラックの移動距離[0.1mm]に換算する
};

struct webParam{
  const char* key;      //JSONのキー
  const char* label;    //画面の表示名
  const char* unit;     //画面の単位表示
  uint8_t addr;         //メモリマップのアドレス
  uint8_t type;         //webParamType
  bool writable;        //ブラウザから書き込めるか
  uint32_t minValue;    //書き込みを受け付ける最小値
  uint32_t maxValue;    //書き込みを受け付ける最大値
  uint16_t scale;       //画面では値をこの数で割って表示する(100なら100→1.00)。APIの値は整数(PARAM_U16_DEG_AS_MM以外はメモリマップと同じ値)
  bool hidden;          //画面に表示しない。APIでは読み書きできる
};

static const webParam webParams[] = {
  {"audio_gain", "動作感度(倍)",               "",    MU16_AUD_GAIN, PARAM_U16, true,  1, 65535, 100, false},
  {"aud_min",    "動作する最低音量レベル",     "",    MU32_AUD_MIN,  PARAM_U32, true,  0, 32768, 1, true},
  {"aud_max",    "可動範囲が最大になる音量レベル", "", MU32_AUD_MAX,  PARAM_U32, true,  1, 32768, 1, true},
  //ヨー軸はサーボの可動範囲(±150deg)、スロットル軸はラックの上下ストローク(35mm)が上限
  {"ay_max",     "ヨー軸 可動範囲の最大",       "deg", MU16_AY_MAX,   PARAM_U16, true,  0, 150, 1, false},
  {"at_max",     "スロットル軸 可動範囲の最大", "mm",  MU16_AT_MAX,   PARAM_U16_DEG_AS_MM, true,  0, 350, 10, false},
  {"aud_vol",    "現在の音量レベル",           "",    MU32_AUD_VOL,  PARAM_U32, false, 0, 0, 1, true},
};
static const int webParamCount = sizeof(webParams) / sizeof(webParams[0]);

//角度[deg]→ラックの移動距離[0.1mm]
static uint32_t degToStroke(uint16_t deg){
  return (uint32_t)(deg * 10.0 / THROTLE_DEG_PER_MM + 0.5);
}

//ラックの移動距離[0.1mm]→角度[deg]
static uint16_t strokeToDeg(uint32_t stroke){
  return (uint16_t)(stroke / 10.0 * THROTLE_DEG_PER_MM + 0.5);
}

static uint32_t readParam(const webParam& p){
  if(p.type == PARAM_U32) return wrc950.u32Map(p.addr);
  if(p.type == PARAM_U16_DEG_AS_MM) return degToStroke(wrc950.u16Map(p.addr));
  return wrc950.u16Map(p.addr);
}

static void writeParam(const webParam& p, uint32_t value){
  if(p.type == PARAM_U32) wrc950.u32Map(p.addr, value);
  else if(p.type == PARAM_U16_DEG_AS_MM) wrc950.u16Map(p.addr, strokeToDeg(value));
  else wrc950.u16Map(p.addr, (uint16_t)value);
}

static void sendJson(int code, JsonDocument& doc){
  String body;
  serializeJson(doc, body);
  webServer.send(code, "application/json", body);
}

static void sendError(int code, const char* message){
  JsonDocument doc;
  doc["error"] = message;
  sendJson(code, doc);
}

static void sendParams(){
  JsonDocument doc;
  JsonArray list = doc["params"].to<JsonArray>();
  for(int i = 0; i < webParamCount; i++){
    JsonObject item = list.add<JsonObject>();
    item["key"] = webParams[i].key;
    item["label"] = webParams[i].label;
    item["unit"] = webParams[i].unit;
    item["value"] = readParam(webParams[i]);
    item["writable"] = webParams[i].writable;
    if(webParams[i].scale > 1) item["scale"] = webParams[i].scale;
    if(webParams[i].hidden) item["hidden"] = true;
    if(webParams[i].writable){
      item["min"] = webParams[i].minValue;
      item["max"] = webParams[i].maxValue;
    }
  }
  sendJson(200, doc);
}

/*
 * LittleFS上の設定画面のHTMLを返す
 * スケッチ側がsetup()でアンマウントするため、リクエストごとにマウントする(マウント済みなら何もしない)
 * テスト音源の再生中はファイルを開いたままにするため、アンマウントはしない
 */
static void handleRoot(){
  if(!LittleFS.begin(false)){
    webServer.send(500, "text/plain; charset=utf-8", "LittleFS mount failed");
    return;
  }
  File file = LittleFS.open(indexFilePath, "r");
  if(!file){
    webServer.send(404, "text/plain; charset=utf-8",
      "index.html not found. Upload the sketch data folder to LittleFS.");
    return;
  }
  webServer.streamFile(file, "text/html; charset=utf-8");
  file.close();
}

static void handleGetParams(){
  sendParams();
}

/*
 * 受け取った値をすべて検証してから書き込む
 * 1つでも不正な値があれば何も書き込まない
 */
static void handlePostParams(){
  JsonDocument doc;
  if(deserializeJson(doc, webServer.arg("plain"))){
    sendError(400, "invalid json");
    return;
  }

  uint32_t newValues[webParamCount];
  bool hasValue[webParamCount];
  for(int i = 0; i < webParamCount; i++){
    newValues[i] = readParam(webParams[i]);
    hasValue[i] = false;
    JsonVariant v = doc[webParams[i].key];
    if(v.isNull() || !webParams[i].writable) continue;
    if(!v.is<uint32_t>() || v.as<uint32_t>() < webParams[i].minValue || v.as<uint32_t>() > webParams[i].maxValue){
      sendError(400, webParams[i].key);
      return;
    }
    newValues[i] = v.as<uint32_t>();
    hasValue[i] = true;
  }

  //最低音量と最大音量が同じだと可動範囲が計算できないため、大小関係を確認する
  uint32_t audMin = 0, audMax = 0;
  for(int i = 0; i < webParamCount; i++){
    if(webParams[i].addr == MU32_AUD_MIN) audMin = newValues[i];
    if(webParams[i].addr == MU32_AUD_MAX) audMax = newValues[i];
  }
  if(audMin >= audMax){
    sendError(400, "aud_max must be greater than aud_min");
    return;
  }

  for(int i = 0; i < webParamCount; i++){
    if(hasValue[i]) writeParam(webParams[i], newValues[i]);
  }
  sendParams();
}

/*
 * 設定ファイルを読み込む。ファイルが無い・読めない場合は空のオブジェクトにする
 * まだファイルシステムが書き込まれていない場合はフォーマットする
 */
static bool readConfig(const char* config_path, JsonDocument& doc){
  if(!LittleFS.begin(true)) return false;
  File file = LittleFS.open(config_path, "r");
  if(file){
    if(deserializeJson(doc, file)) doc.clear();
    file.close();
  }
  if(!doc.is<JsonObject>()) doc.to<JsonObject>();
  return true;
}

//テスト音源の再生中はファイルを開いているため、アンマウントしない
static bool writeConfig(const char* config_path, JsonDocument& doc){
  File file = LittleFS.open(config_path, "w");
  if(!file) return false;
  size_t written = serializeJsonPretty(doc, file);
  file.close();
  return written > 0;
}

/*
 * デバイス名を返す
 * device_name: 現在使っている名前、saved: 設定ファイルに保存されている名前(再起動後に使われる)
 */
static void sendDeviceName(){
  JsonDocument config;
  readConfig(configFilePath, config);
  const char* saved = config["device_name"];
  if(saved == nullptr || !isValidDeviceName(saved)) saved = device_name;

  JsonDocument doc;
  doc["device_name"] = device_name;
  doc["saved"] = saved;
  doc["max_length"] = DEVICE_NAME_MAX_LEN;
  sendJson(200, doc);
}

static void handleGetDeviceName(){
  sendDeviceName();
}

static void handlePostDeviceName(){
  JsonDocument doc;
  if(deserializeJson(doc, webServer.arg("plain"))){
    sendError(400, "invalid json");
    return;
  }
  const char* name = doc["device_name"];
  if(name == nullptr || !isValidDeviceName(name)){
    sendError(400, "device_name must be 1-32 half-width alphanumeric characters or symbols");
    return;
  }
  if(!saveDeviceName(configFilePath, name)){
    sendError(500, "failed to write config file");
    return;
  }
  sendDeviceName();
}

static void handleSave(){
  if(!saveAudioGain(configFilePath)){
    sendError(500, "failed to write config file");
    return;
  }
  JsonDocument doc;
  doc["saved"] = true;
  doc["audio_gain"] = wrc950.u16Map(MU16_AUD_GAIN);
  sendJson(200, doc);
}

static void sendTestAudio(){
  JsonDocument doc;
  doc["playing"] = testAudioIsPlaying();
  doc["volume"] = testAudioGetVolume();
  doc["max_volume"] = TEST_AUDIO_MAX_VOLUME;
  //再生できない場合の原因調査用
  if(testAudioLastError() != nullptr) doc["error"] = testAudioLastError();
  doc["frames"] = testAudioPlayedFrames();
  doc["heap_free"] = ESP.getFreeHeap();
  doc["heap_max_block"] = ESP.getMaxAllocHeap();
  sendJson(200, doc);
}

static void handleGetTestAudio(){
  sendTestAudio();
}

/*
 * テスト音源の再生・停止と再生音量を変更する
 * {"playing": true/false, "volume": 0～TEST_AUDIO_MAX_VOLUME} のどちらか、または両方を受け付ける
 */
static void handlePostTestAudio(){
  JsonDocument doc;
  if(deserializeJson(doc, webServer.arg("plain"))){
    sendError(400, "invalid json");
    return;
  }

  JsonVariant volume = doc["volume"];
  if(!volume.isNull()){
    if(!volume.is<uint8_t>() || volume.as<uint8_t>() > TEST_AUDIO_MAX_VOLUME){
      sendError(400, "volume");
      return;
    }
    testAudioSetVolume(volume.as<uint8_t>());
  }

  JsonVariant playing = doc["playing"];
  if(playing.is<bool>()){
    if(playing.as<bool>()){
      const char* error = testAudioStart();
      if(error != nullptr){
        sendError(409, error);
        return;
      }
    }else{
      testAudioStop();
    }
  }
  sendTestAudio();
}

static void handleNotFound(){
  sendError(404, "not found");
}

/*
 * Bluetooth接続中は設定できないよう、リクエストを拒否してから各ハンドラを呼ぶ
 * WIFI_AP_PAUSE_ON_BTが0で、アクセスポイントが動いたままの場合に使う
 */
static WebServer::THandlerFunction rejectWhileBluetooth(void (*handler)()){
  return [handler](){
    if(isApMode && a2dp_sink.is_connected()){
      webServer.send(503, "text/plain; charset=utf-8",
        "Bluetooth接続中は設定できません。Bluetoothを切断してから開き直してください。");
      return;
    }
    handler();
  };
}

#if WIFI_AP_DEBUG
static void apLog(const char* message, int result = ESP_OK){
  Serial.printf("[AP] %s result=%d heap_free=%u heap_max_block=%u\n",
                message, result, ESP.getFreeHeap(), ESP.getMaxAllocHeap());
}

static void onWiFiEvent(arduino_event_id_t event){
  switch(event){
    case ARDUINO_EVENT_WIFI_AP_START:           apLog("event AP_START"); break;
    case ARDUINO_EVENT_WIFI_AP_STOP:            apLog("event AP_STOP"); break;
    case ARDUINO_EVENT_WIFI_AP_STACONNECTED:    apLog("event STA_CONNECTED"); break;
    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED: apLog("event STA_DISCONNECTED"); break;
    case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:   apLog("event STA_IP_ASSIGNED"); break;
    default: break;
  }
}
#else
static inline void apLog(const char* message, int result = ESP_OK){ (void)message; (void)result; }
#endif

static void startAccessPoint(){
  bool started;
  if(apPassword.length() >= 8){
    started = WiFi.softAP(apSsid.c_str(), apPassword.c_str());
  }else{
    started = WiFi.softAP(apSsid.c_str());
  }
  apLog(apPassword.length() >= 8 ? "softAP (WPA2)" : "softAP (open)", started ? ESP_OK : ESP_FAIL);
  isApRunning = true;
}

#if WIFI_AP_PAUSE_ON_BT
/*
 * アクセスポイントの電波だけを止める・再開する
 * WiFi.softAPdisconnect(true)やWiFi.mode(WIFI_OFF)はWi-Fiのドライバを解放(esp_wifi_deinit)してしまい、
 * 再開時に確保し直すメモリがBluetooth接続後は足りず、接続できない・不安定になる
 * esp_wifi_stop()/esp_wifi_start()はドライバとSSIDなどの設定を残したまま電波だけを止める
 */
static void pauseAccessPoint(){
  esp_err_t result = esp_wifi_stop();
  apLog("pause (esp_wifi_stop)", result);
  if(result == ESP_OK) isApRunning = false;
}

static void resumeAccessPoint(){
  esp_err_t result = esp_wifi_start();
  apLog("resume (esp_wifi_start)", result);
  if(result == ESP_OK) isApRunning = true;
}

/*
 * Bluetooth接続中はアクセスポイントを止めて、ブラウザから設定できないようにする
 * Wi-Fiを止めることで、Bluetoothオーディオが電波を占有できる
 * 切断後すぐに再接続されることがあるため、少し待ってから再開する
 */
static void updateAccessPoint(){
  if(a2dp_sink.is_connected()){
    btConnectedTime = millis();
    if(isApRunning) pauseAccessPoint();
    return;
  }
  if(!isApRunning && millis() - btConnectedTime >= WIFI_AP_RESTART_DELAY_MS){
    resumeAccessPoint();
    //再開できなかった場合は、少し待ってから再試行する
    if(!isApRunning) btConnectedTime = millis();
  }
}
#endif

bool wifiInit(const char* ssid, const char* password, const char* ap_ssid, const char* ap_password){
  if(ssid != nullptr && ssid[0] != '\0'){
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    uint32_t startTime = millis();
    while(WiFi.status() != WL_CONNECTED && millis() - startTime < WIFI_CONNECT_TIMEOUT_MS){
      delay(100);
    }
    if(WiFi.status() == WL_CONNECTED) return true;
    WiFi.disconnect(true);
  }

  //接続先が無い、または接続できなかった場合は自身がアクセスポイントになる
#if WIFI_AP_DEBUG
  WiFi.onEvent(onWiFiEvent);
#endif
  /*
   * WiFi.mode(WIFI_AP)は前回Flashに保存された設定で電波を出してしまい、
   * その後に電波を出したままSSIDやパスワードを書き換えると、パスワード付きの場合に接続できないことがある
   * 設定をFlashに保存しないようにし、電波を止めた状態で設定してから電波を出す
   */
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  apLog("esp_wifi_stop before config", esp_wifi_stop());
  apSsid = ap_ssid;
  apPassword = (ap_password != nullptr) ? ap_password : "";
  startAccessPoint();
  apLog("esp_wifi_start after config", esp_wifi_start());
  isApMode = true;
  return false;
}

void serverInit(const char* config_path){
  configFilePath = config_path;
  webServer.on("/", HTTP_GET, rejectWhileBluetooth(handleRoot));
  webServer.on("/api/params", HTTP_GET, rejectWhileBluetooth(handleGetParams));
  webServer.on("/api/params", HTTP_POST, rejectWhileBluetooth(handlePostParams));
  webServer.on("/api/save", HTTP_POST, rejectWhileBluetooth(handleSave));
  webServer.on("/api/device_name", HTTP_GET, rejectWhileBluetooth(handleGetDeviceName));
  webServer.on("/api/device_name", HTTP_POST, rejectWhileBluetooth(handlePostDeviceName));
  webServer.on("/api/test_audio", HTTP_GET, rejectWhileBluetooth(handleGetTestAudio));
  webServer.on("/api/test_audio", HTTP_POST, rejectWhileBluetooth(handlePostTestAudio));
  webServer.onNotFound(handleNotFound);
  webServer.begin();
}

/*
 * Wi-FiとBluetoothはアンテナを交互に使うため、どちらを優先するかを接続状態で切り替える
 * Bluetooth未接続時もBluetoothは接続の待ち受けを続けており、標準の配分(BALANCE)のままだと
 * パスワード付きアクセスポイントへの接続時の鍵交換が取りこぼされ、「パスワードが違う」と表示されて接続できない
 */
static void updateCoexPreference(){
  static int8_t lastPrefer = -1;
  int8_t prefer = a2dp_sink.is_connected() ? ESP_COEX_PREFER_BT : ESP_COEX_PREFER_WIFI;
  if(prefer == lastPrefer) return;
  esp_err_t result = esp_coex_preference_set((esp_coex_prefer_t)prefer);
  apLog(prefer == ESP_COEX_PREFER_WIFI ? "coex prefer WIFI" : "coex prefer BT", result);
  if(result == ESP_OK) lastPrefer = prefer;
}

void handleWebServer(){
  updateCoexPreference();
#if WIFI_AP_PAUSE_ON_BT
  if(isApMode) updateAccessPoint();
#endif
  webServer.handleClient();
}

//既存の設定ファイルを読み込み、他のキーを残したまま audio_gain だけを書き換える
bool saveAudioGain(const char* config_path){
  JsonDocument doc;
  if(!readConfig(config_path, doc)) return false;
  doc["audio_gain"] = wrc950.u16Map(MU16_AUD_GAIN);
  return writeConfig(config_path, doc);
}

bool isValidDeviceName(const char* name){
  if(name == nullptr) return false;
  size_t len = strlen(name);
  if(len == 0 || len > DEVICE_NAME_MAX_LEN) return false;
  for(size_t i = 0; i < len; i++){
    if(name[i] < 0x20 || name[i] > 0x7e) return false;
  }
  return true;
}

bool saveDeviceName(const char* config_path, const char* name){
  if(!isValidDeviceName(name)) return false;
  JsonDocument doc;
  if(!readConfig(config_path, doc)) return false;
  doc["device_name"] = name;
  return writeConfig(config_path, doc);
}
