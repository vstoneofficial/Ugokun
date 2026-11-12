// Copyright 2026 Vstone Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

/**
 * スケッチ名：ugokun_common
 * バージョン：0.0.1
 * 概要：
 * 　うごくんを外部のデバイスから制御する際に汎用的に使用可能なスケッチです。
 *   ROS使用時にROSとの接続がない場合、自動的に停止します。
 * 
 */

#define WRC_ENABLE 1
#define WRC_DISABLE 0


/*******************************************************************
 * WRC_WIRELESS_MODEに使用する規格を定義してください。
 *
 * 〇WiFi使用時
 * WRC_WIRELESS_MODE を USE_WIFI で定義してください。
 * ブラウザから http://<IPアドレス>/ を開くと、動作感度などのパラメータを変更できます。
 * 接続先のアクセスポイントのSSIDとパスワードは「WiFi関連の設定」で設定してください。
 * SSIDが空、または接続できなかった場合は、うごくん自身がアクセスポイントになります
 * (SSIDはBluetoothのデバイス名と同じ、IPアドレスは192.168.4.1)。
 * Bluetooth接続中は設定できません。Bluetoothを切断すると設定できます。
 *
 * 
 * 〇無線通信を使用しないとき
 * WRC_WIRELESS_MODE を WIRELESS_OFF で定義してください。
 */ 
#define WIRELESS_OFF 0
#define USE_WIFI     1
#define WRC_WIRELESS_MODE   USE_WIFI

/*******************************************************************
 * 本製品はROSから制御することが可能です。
 * WiFi接続と有線シリアル接続を選ぶことができます。両方同時には使えません。
 * WRC_ROS_SERIAL_MODEに使用する規格を定義してください。
 * 
 * 〇WiFi接続時
 * WRC_ROS_SERIAL_MODE を MODE_WIFI で定義してください。
 * また、WRC021_WIRELESS_MODE を USE_WIFI で定義してください。
 * 
 * 〇有線シリアル使用時
 * WRC_ROS_SERIAL_MODE を MODE_SERIAL で定義してください。
 * 
 * 〇ROSを使用しないとき
 * WRC_ROS_SERIAL_MODE を MODE_OFF で定義してください。
 */
#define MODE_OFF    0
#define MODE_WIFI   1
#define MODE_SERIAL 2
#define WRC_ROS_SERIAL_MODE MODE_OFF
#define ROS2_SERIAL_BAUDRATE 921600UL

/*******************************************************************
 * ウォッチドッグタイマに関する設定
 * 何らかの要因により外部機器との通信が停止した場合、WDTIMEに設定した時間[ms]後にモータを停止します。
 * 0を設定するとESP32によるタイマのクリアは無効化されますが、
 * 外部からメモリマップのMU16_WDTに値をセットすることでクリアすることができます。
 */
#define WDTIME 500

#define ROBOT_NUM 0
#define STRINGIFY_VALUE(value) #value
#define STRINGIFY(value) STRINGIFY_VALUE(value)

static const char ros2_node_name[] = "ugokun_" STRINGIFY(ROBOT_NUM);
static const char ros2_svctrl_topic[] = "/ugokun" STRINGIFY(ROBOT_NUM) "/svctrl";

#define PWM_MODE  0
#define SERIAL_MODE 1
#define SERVO_CTR_MODE  SERIAL_MODE

#define NON_INTERLOCKING  3
#define INTERLOCKING_YAW 2
#define INTERLOCKING_THROTLE 1
#define INTERLOCKING_BOTH  0
#define AUDIO_INTERLOCKING  INTERLOCKING_BOTH

/*******************************************************************
 * 動作感度(音量に応じたサーボの可動範囲を決める倍率)
 * 音量レベルにこの倍率を掛けてから可動範囲(±30°～±60°)を決めます。
 * 値は倍率の100倍の整数で、100で1倍、250で2.5倍。大きくするほど小さい音量でも大きく動きます。
 * ブラウザの設定画面では倍率(1.00など)で表示します。
 * 起動時にLittleFS上のCONFIG_FILE_PATHの"audio_gain"から読み込みます。
 * ファイルが無い、または値が不正な場合はAUDIO_GAINの値を使用します。
 * メモリマップのMU16_AUD_GAINに書き込むことで、実行中に変更することもできます。
 */
#define AUDIO_GAIN  100
#define CONFIG_FILE_PATH  "/config.json"

#if (WRC_ROS_SERIAL_MODE == MODE_SERIAL)
#define DEBUG_PRINT(msg) do {} while (0)
#define DEBUG_PRINTLN(msg) do {} while (0)
#else
#define DEBUG_PRINT(msg) Serial.print(msg)
#define DEBUG_PRINTLN(msg) Serial.println(msg)
#endif

#include <vs_wrc950_ugokun.h>

/*
 * デバイス名(Bluetoothのデバイス名と、自身がアクセスポイントになる場合のSSID)
 * 起動時にLittleFS上のCONFIG_FILE_PATHの"device_name"から読み込みます。
 * ファイルが無い、または値が不正な場合は "UGOKUN_" + ROBOT_NUM を使用します。
 * ブラウザの設定画面から変更でき、再起動後に反映されます。
 */
static char bluetooth_device_name[DEVICE_NAME_MAX_LEN + 1] = "UGOKUN_" STRINGIFY(ROBOT_NUM);

#include <micro_ros_arduino.h>

#include <stdio.h>
#include <string.h>
#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <rmw_microros/rmw_microros.h>
#include <micro_ros_utilities/type_utilities.h>

#include <std_msgs/msg/int16_multi_array.h>

#include <WiFi.h>
#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

static HardwareSerial* ros2_serial_port = &Serial;
static uint32_t ros2_serial_baudrate = ROS2_SERIAL_BAUDRATE;

extern "C" bool arduino_transport_open(struct uxrCustomTransport * transport)
{
  (void)transport;
  ros2_serial_port->end();
  delay(20);
  ros2_serial_port->begin(ros2_serial_baudrate);
  ros2_serial_port->setTimeout(1);
  delay(20);
  return true;
}

extern "C" bool arduino_transport_close(struct uxrCustomTransport * transport)
{
  (void)transport;
  ros2_serial_port->flush();
  ros2_serial_port->end();
  delay(20);
  return true;
}

extern "C" size_t arduino_transport_write(struct uxrCustomTransport* transport, const uint8_t * buf, size_t len, uint8_t * err)
{
  (void)transport;
  (void)err;
  return ros2_serial_port->write(buf, len);
}

extern "C" size_t arduino_transport_read(struct uxrCustomTransport* transport, uint8_t* buf, size_t len, int timeout, uint8_t* err)
{
  (void)transport;
  (void)err;
  ros2_serial_port->setTimeout(timeout);
  return ros2_serial_port->readBytes((char *)buf, len);
}


/*******************************************
 * プロトタイプ宣言
 */
void LED(int cmd);


/*******************************************
 * WiFi関連の設定
 */
const char* ssid     = "";            //WiFi使用時に接続するアクセスポイントのSSIDを設定してください ※2.4GHzのみ対応
const char* password = "";            //WiFi使用時に接続するSSIDのパスワードを設定してください
const char* ap_password = "ugokunugokun";         //自身がアクセスポイントになる場合のパスワード(8文字以上)。空の場合はパスワード無し

/*******************************************
 * ROS接続設定
 */
char*    ROSserverIP = "192.168.1.1";        //micro-ros Agentを起動したデバイスのIPアドレスを設定してください。
uint16_t serverPort = 8888;       //通信ポートを設定してください。

/*******************************************
 *  ROS用の各種定義
 */
rcl_subscription_t subscriber;

rclc_executor_t executor;
rclc_support_t support;
rcl_allocator_t allocator;
rcl_node_t node;
rcl_timer_t timer;

static const rmw_qos_profile_t sub_last_one_profile = 
{
  RMW_QOS_POLICY_HISTORY_KEEP_LAST,
  1,
  RMW_QOS_POLICY_RELIABILITY_BEST_EFFORT,
  RMW_QOS_POLICY_DURABILITY_VOLATILE,
  RMW_QOS_DEADLINE_DEFAULT,
  RMW_QOS_LIFESPAN_DEFAULT,
  RMW_QOS_POLICY_LIVELINESS_SYSTEM_DEFAULT,
  RMW_QOS_LIVELINESS_LEASE_DURATION_DEFAULT,
  false
};

std_msgs__msg__Int16MultiArray submsg_svctrl;
micro_ros_utilities_memory_conf_t submsg_svctrl_conf = {0};

enum states {
  WAITING_AGENT,
  AGENT_AVAILABLE,
  AGENT_CONNECTED,
  AGENT_DISCONNECTED
} ros2state;


#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){error_loop();}}
#define RCINITCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){DEBUG_PRINT("ROS INIT ERR: "); DEBUG_PRINTLN(#fn); return false;}}
#define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){}}

const int publish_rate = 50;      //publishする周波数[Hz]

bool ros2_support_created = false;
bool ros2_node_created = false;
bool ros2_subscriber_created = false;
bool ros2_timer_created = false;
bool ros2_executor_created = false;
bool ros2_sensor_memory_created = false;

void error_loop(){
  while(1){
    static bool led_brink = true;
    LED(led_brink);
    led_brink = !led_brink;
    DEBUG_PRINTLN("ROS ERR");
    delay(1000);
  }
}

void wait_agent(){
  static int64_t last_time = -1;
  if (last_time == -1) { last_time = uxr_millis();}

  if(uxr_millis() - last_time > 2000){
    ros2state = (RMW_RET_OK == rmw_uros_ping_agent(200, 1)) ? AGENT_AVAILABLE : WAITING_AGENT;
    last_time = uxr_millis();
  }
}

void check_agent(){
  static int64_t last_time = -1;
  if (last_time == -1) { last_time = uxr_millis();}

  if(uxr_millis() - last_time > 2000 && ros2state == AGENT_CONNECTED){
    ros2state = (RMW_RET_OK == rmw_uros_ping_agent(200, 1)) ? AGENT_CONNECTED : AGENT_DISCONNECTED;
    last_time = uxr_millis();

    if(ros2state == AGENT_CONNECTED){
      DEBUG_PRINTLN("ROS CA-> OK");
    }else{
      DEBUG_PRINTLN("ROS CA-> DISCONNECTED");
    }
  }
}

bool ros2_init(){
  allocator = rcl_get_default_allocator();
  DEBUG_PRINTLN("FIN rcl_get_default_allocator");

  //create init_options
  RCINITCHECK(rclc_support_init(&support, 0, NULL, &allocator));
  ros2_support_created = true;
  DEBUG_PRINTLN("FIN rclc_support_init");

  // create node
  RCINITCHECK(rclc_node_init_default(&node, ros2_node_name, "", &support));
  ros2_node_created = true;
  DEBUG_PRINTLN("FIN rclc_node_init_default");

  // create subscriber
  RCINITCHECK(rclc_subscription_init(
    &subscriber,
    &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int16MultiArray),
    ros2_svctrl_topic,
    &sub_last_one_profile));
  ros2_subscriber_created = true;
  DEBUG_PRINTLN("FIN rclc_subscription_init_default");

  // create timer,
  RCINITCHECK(rclc_timer_init_default(
    &timer,
    &support,
    RCL_MS_TO_NS(1000/publish_rate),
    timer_callback));
  ros2_timer_created = true;

  submsg_svctrl_conf.max_ros2_type_sequence_capacity = 8;
  submsg_svctrl_conf.max_basic_type_sequence_capacity = 8;
  if(!micro_ros_utilities_create_message_memory(
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int16MultiArray),
    &submsg_svctrl,
    submsg_svctrl_conf))
  {
    DEBUG_PRINTLN("FIN submsg memory NG");
    return false;
  }
  ros2_sensor_memory_created = true;

  // create executor
  const uint8_t num_handles = 2; // total number of handles = #subscriptions + #timers
  DEBUG_PRINTLN("START rclc_executor_init");
  RCINITCHECK(rclc_executor_init(&executor, &support.context, num_handles, &allocator));
  ros2_executor_created = true;
  DEBUG_PRINTLN("FIN rclc_executor_init");
  RCINITCHECK(rclc_executor_add_subscription(&executor, &subscriber, &submsg_svctrl, &subscription_callback, ON_NEW_DATA));
  RCINITCHECK(rclc_executor_add_timer(&executor, &timer));


  return true;
}

//destroy entities when re-connect
void destroy_entities(){
  if(ros2_support_created){
    rmw_context_t * rmw_context = rcl_context_get_rmw_context(&support.context);
    (void) rmw_uros_set_context_entity_destroy_session_timeout(rmw_context, 0);
  }

  if(ros2_executor_created){
    rclc_executor_fini(&executor);
    ros2_executor_created = false;
  }
  if(ros2_sensor_memory_created){
    micro_ros_utilities_destroy_message_memory(
      ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int16MultiArray),
      &submsg_svctrl,
      submsg_svctrl_conf);
    ros2_sensor_memory_created = false;
    submsg_svctrl_conf.max_string_capacity = 0;
    submsg_svctrl_conf.max_ros2_type_sequence_capacity = 0;
    submsg_svctrl_conf.max_basic_type_sequence_capacity = 0;
    submsg_svctrl_conf.rules = NULL;
    submsg_svctrl_conf.n_rules = 0;
    submsg_svctrl_conf.allocator = NULL;
  }
  if(ros2_timer_created){
    rcl_timer_fini(&timer);
    ros2_timer_created = false;
  }
  if(ros2_subscriber_created){
    rcl_subscription_fini(&subscriber, &node);
    ros2_subscriber_created = false;
  }
  if(ros2_node_created){
    rcl_node_fini(&node);
    ros2_node_created = false;
  }
  if(ros2_support_created){
    rclc_support_fini(&support);
    ros2_support_created = false;
  }

  subscriber = rcl_get_zero_initialized_subscription();
  timer = rcl_get_zero_initialized_timer();
  executor = rclc_executor_get_zero_initialized_executor();
  node = rcl_get_zero_initialized_node();
  memset(&support, 0, sizeof(support));
}

//timer_callback
void timer_callback(rcl_timer_t * timer, int64_t last_call_time){  
  RCLC_UNUSED(last_call_time);
  if (timer != NULL) {
    
  }
}


//servo control message callback
void subscription_callback(const void *msgin) {
  const std_msgs__msg__Int16MultiArray * msg = (const std_msgs__msg__Int16MultiArray *)msgin;

  const int cols = 4;
  int8_t  audioSvE=0; 

  /*
  メッセージ内容
  [サーボ1音源連携イネーブル,サーボ1角度,サーボ1中心角度,サーボ1速度,
  サーボ2音源連携イネーブル,サーボ2角度,サーボ2中心角度,サーボ2速度]
  */

  if (msg->data.size >= 2 * cols){
    audioSvE = 0x03 & (msg->data.data[1 * cols + 0]<<1 | msg->data.data[0 * cols + 0]);
    wrc950.u8Map(MU8_E_SVU, audioSvE);
    if((wrc950.u8Map(MU8_E_SVU) & 0x01) == 1) wrc950.s16Map(MS16_D_SV0, msg->data.data[0 * cols + 1]);
    wrc950.s16Map(MS16_CD_SV0, msg->data.data[0 * cols + 2]);
    wrc950.u16Map(MU16_S_SV0, msg->data.data[0 * cols + 3]);
    if(((wrc950.u8Map(MU8_E_SVU)>>1)&0x01)==1) wrc950.s16Map(MS16_D_SV1, msg->data.data[1 * cols + 1]);
    wrc950.s16Map(MS16_CD_SV1, msg->data.data[1 * cols + 2]);
    wrc950.u16Map(MU16_S_SV1, msg->data.data[1 * cols + 3]);
  }
  
  wrc950.u16Map(MU16_WDT, WDTIME);
}

/*******************************************
 * LittleFS上の設定ファイルを読み込む
 * 読み込めなかった場合はfalseを返す
 */
bool loadConfig(JsonDocument& doc){
  if(!LittleFS.begin(false)){
    DEBUG_PRINTLN("LittleFS mount failed");
    return false;
  }

  File file = LittleFS.open(CONFIG_FILE_PATH, "r");
  if(!file){
    DEBUG_PRINTLN("config file not found");
    LittleFS.end();
    return false;
  }

  DeserializationError error = deserializeJson(doc, file);
  file.close();
  LittleFS.end();
  if(error){
    DEBUG_PRINT("config parse error: ");
    DEBUG_PRINTLN(error.c_str());
    return false;
  }
  return true;
}

/*******************************************
 * 設定ファイルからデバイス名を読み込み、bluetooth_device_nameに設定する
 * 読み込めなかった場合は初期値のまま
 */
void loadDeviceName(){
  JsonDocument doc;
  if(!loadConfig(doc)) return;
  const char* name = doc["device_name"];
  if(name == nullptr) return;
  if(!isValidDeviceName(name)){
    DEBUG_PRINTLN("invalid device_name");
    return;
  }
  strlcpy(bluetooth_device_name, name, sizeof(bluetooth_device_name));
}

/*******************************************
 * LittleFS上の設定ファイルから動作感度(倍率の100倍)を読み込む
 * 読み込めなかった場合はAUDIO_GAINを返す
 */
uint16_t loadAudioGain(){
  JsonDocument doc;
  if(!loadConfig(doc)) return AUDIO_GAIN;

  //0は等倍扱いになるため、1～65535の整数のみ受け付ける
  if(!doc["audio_gain"].is<uint16_t>() || doc["audio_gain"].as<uint16_t>() == 0){
    DEBUG_PRINTLN("invalid audio_gain");
    return AUDIO_GAIN;
  }
  return doc["audio_gain"].as<uint16_t>();
}

/*******************************************
 * セットアップ
 */
void setup()
{
  #if (WRC_ROS_SERIAL_MODE != MODE_SERIAL)
  Serial.begin(921600);
  #endif
  DEBUG_PRINTLN("start_setup");

  //ウォッチドッグタイマの設定
  wdTime = WDTIME;

  //タイマー割り込み設定
  setupInterruptTimer();
  
  //LEDセットアップ
  ledInit();
  LED(1);                         //LEDを点ける

  //バッファのクリア
  clearPersBuf();

  //デバイス名の読み込み。Wi-FiとBluetoothの初期化より前に行う
  loadDeviceName();
  DEBUG_PRINT("device_name: ");
  DEBUG_PRINTLN(bluetooth_device_name);

  //[ROS]接続モード別設定値の設定
  #if (WRC_ROS_SERIAL_MODE == MODE_WIFI)
    // using ROS over Wi-Fi
    DEBUG_PRINTLN("USE ROS2 WIFI");
    set_microros_wifi_transports(ssid, password, ROSserverIP, serverPort);
    ros2state = WAITING_AGENT;
  #elif (WRC_ROS_SERIAL_MODE == MODE_SERIAL)
    // using ROS2 via USB-Serial
    DEBUG_PRINTLN("USE ROS2");
    set_microros_transports();
    delay(500);
    ros2state = WAITING_AGENT;
  #else
    DEBUG_PRINTLN("ROS2 OFF");
  #endif

  //各無線通信機能のセットアップ
  #if (WRC_WIRELESS_MODE == USE_WIFI)
    //ROSをWiFiで使用する場合は、set_microros_wifi_transports()で接続済み
    #if (WRC_ROS_SERIAL_MODE != MODE_WIFI)
    if(wifiInit(ssid, password, bluetooth_device_name, ap_password)){
      DEBUG_PRINT("WiFi connected: ");
      DEBUG_PRINTLN(WiFi.localIP());
    }else{
      DEBUG_PRINT("WiFi AP started: ");
      DEBUG_PRINTLN(WiFi.softAPIP());
    }
    #endif
    serverInit(CONFIG_FILE_PATH);
  #endif
  
  DEBUG_PRINTLN("start_initnode");

  //メモリマップ初期化
  wrc950.initMemmap(18.0); 

  //動作感度初期化
  wrc950.u16Map(MU16_AUD_GAIN,100);

  servoInit(SERVO_CTR_MODE);
  DEBUG_PRINTLN("servo_init");

  device_name = bluetooth_device_name;
  wrc950.u8Map(MU8_E_SVU,AUDIO_INTERLOCKING);
  audioInit();
  wrc950.u32Map(MU32_AUD_MIN,50);
  wrc950.u32Map(MU32_AUD_MAX,2000);
  wrc950.u16Map(MU16_AUD_GAIN,loadAudioGain());
  DEBUG_PRINT("audio_gain: ");
  DEBUG_PRINTLN(wrc950.u16Map(MU16_AUD_GAIN));
  DEBUG_PRINTLN("audio_init");
  DEBUG_PRINT("heap_free: ");
  DEBUG_PRINT(ESP.getFreeHeap());
  DEBUG_PRINT(" heap_max_block: ");
  DEBUG_PRINTLN(ESP.getMaxAllocHeap());
  wrc950.u16Map(MU16_AY_MIN,0);
  wrc950.u16Map(MU16_AY_MAX,90);
  wrc950.u16Map(MU16_AT_MIN,0);
  wrc950.u16Map(MU16_AT_MAX,150);

  delay(10);
  LED(0);  

  wrc950.u8Map(MU8_DPS_EN,0);
  wrc950.u16Map(MU16_DPS_SV0,100);
  wrc950.u16Map(MU16_DPS_SV1,100);

  delay(50);
  LED(1);  

  DEBUG_PRINTLN("start_init");

  LED(0);  

  DEBUG_PRINTLN("readey");
  LED(1);                         //LEDを点ける

}

void loop(){
  uint32_t last_sound_time=wrc950.u32Map(MU32_MOV_T0);
  uint32_t audio_vol=wrc950.u32Map(MU32_AUD_VOL);
  bool sound_active = (millis() - last_sound_time) < ACT_DIFF;
  update_servo(sound_active, audio_vol);
  moveServo();

  #if (WRC_WIRELESS_MODE == USE_WIFI)
    handleWebServer();
  #endif

  #if (WRC_ROS_SERIAL_MODE == MODE_WIFI || WRC_ROS_SERIAL_MODE == MODE_SERIAL)
  //ROS 2接続処理
  switch (ros2state) {
    case WAITING_AGENT:
      wait_agent();
      LED(0);
      //printf("Wait starting your micro-ros Agent\n");
      break;
    case AGENT_AVAILABLE:
      ros2state = (true == ros2_init()) ? AGENT_CONNECTED : WAITING_AGENT;
      if (ros2state == WAITING_AGENT) {
        destroy_entities();
      }
      break;
    case AGENT_CONNECTED:
      if (ros2state == AGENT_CONNECTED) {
        rclc_executor_spin_some(&executor, RCL_MS_TO_NS(1));
      }
      break;
    case AGENT_DISCONNECTED:
      destroy_entities();
      ros2state = WAITING_AGENT;
      break;
    default:
      break;
  }
  #endif

  #if (WRC_ROS_SERIAL_MODE != MODE_SERIAL)
  if(Serial.available()){
    LED(0);                                                                             //LEDを消し、
    if(persMsgViaSerial()){                                                             //シリアル入力を解釈し、有効なアクセスであれば
      wrc950.u16Map(MU16_WDT, WDTIME);                                                  //ウォッチドッグタイマをクリアする。
    }

  }
  #endif

  if(isInterrupt){
    isInterrupt = false;

    //ROS使用時にROSが未接続の場合の処理
    #if (WRC_ROS_SERIAL_MODE == MODE_WIFI || WRC_ROS_SERIAL_MODE == MODE_SERIAL)
    if(ros2state != AGENT_CONNECTED){
      //wrc950.s16Map(MS16_D_SV0, 0);
      //wrc950.s16Map(MS16_D_SV1, 0);
    }
    #endif
  
    if(checkWDT() == 0xffff){
      #if (WRC_ROS_SERIAL_MODE == MODE_WIFI || WRC_ROS_SERIAL_MODE == MODE_SERIAL)
      check_agent();
      #endif
      LED(0);
    }
  }

}
