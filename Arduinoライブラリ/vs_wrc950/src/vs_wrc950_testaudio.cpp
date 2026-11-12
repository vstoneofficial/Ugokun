// Copyright 2026 Vstone Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

#include "vs_wrc950_testaudio.h"
#include "vs_wrc950_btaudio.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <driver/i2s.h>

#define TEST_AUDIO_I2S_PORT     I2S_NUM_0   //a2dp_sinkが使うI2Sのポート
#define TEST_AUDIO_SAMPLE_RATE  44100       //I2Sへの出力周波数。audio_data_callback()のフィルタはこの周波数が前提
#define TEST_AUDIO_FRAMES       256         //1回にI2Sへ書き込むフレーム数(1フレーム=左右2サンプル)
#define TEST_AUDIO_STACK_SIZE   4096        //再生用タスクのスタックサイズ
#define TEST_AUDIO_PRIORITY     2           //loop()より優先して音切れを防ぐ
#define TEST_AUDIO_CORE         1

static volatile bool playRequested = false;
static volatile bool isPlaying = false;
static volatile uint8_t volumePercent = TEST_AUDIO_DEFAULT_VOLUME;
static const char* volatile lastError = nullptr;
static volatile uint32_t playedFrames = 0;
static TaskHandle_t testAudioTaskHandle = nullptr;

static int16_t readBuffer[TEST_AUDIO_FRAMES * 2];
static int16_t outBuffer[TEST_AUDIO_FRAMES * 2];

struct wavInfo{
  uint16_t channels;    //1:モノラル 2:ステレオ
  uint8_t repeat;       //44100Hzにするために同じサンプルを出力する回数
  uint32_t dataOffset;  //PCMデータの先頭位置
  uint32_t dataSize;    //PCMデータのバイト数
};

static uint32_t readU32(File& file){
  uint8_t b[4] = {0};
  file.read(b, 4);
  return b[0] | (b[1] << 8) | (b[2] << 16) | ((uint32_t)b[3] << 24);
}

static uint16_t readU16(File& file){
  uint8_t b[2] = {0};
  file.read(b, 2);
  return b[0] | (b[1] << 8);
}

/*
 * WAVファイルのヘッダを読み、PCMデータの位置と形式を調べる
 * 対応形式: リニアPCM 16bit、モノラル/ステレオ、44100/22050/11025Hz
 * 戻り値: 対応していればnullptr、できなければ理由を表す文字列
 */
static const char* parseWav(File& file, wavInfo& info){
  char id[4];
  if(file.read((uint8_t*)id, 4) != 4 || memcmp(id, "RIFF", 4) != 0) return "not a wav file";
  readU32(file);
  if(file.read((uint8_t*)id, 4) != 4 || memcmp(id, "WAVE", 4) != 0) return "not a wav file";

  bool hasFormat = false;
  while(file.read((uint8_t*)id, 4) == 4){
    uint32_t size = readU32(file);
    uint32_t next = file.position() + size + (size & 1);

    if(memcmp(id, "fmt ", 4) == 0){
      uint16_t format = readU16(file);
      info.channels = readU16(file);
      uint32_t rate = readU32(file);
      readU32(file);  //バイトレート
      readU16(file);  //ブロックサイズ
      uint16_t bits = readU16(file);
      if(format != 1 || bits != 16) return "wav must be 16bit PCM";
      if(info.channels != 1 && info.channels != 2) return "wav must be mono or stereo";
      if(rate == 0 || TEST_AUDIO_SAMPLE_RATE % rate != 0 || TEST_AUDIO_SAMPLE_RATE / rate > 4){
        return "wav sample rate must be 44100/22050/11025";
      }
      info.repeat = TEST_AUDIO_SAMPLE_RATE / rate;
      hasFormat = true;
    }else if(memcmp(id, "data", 4) == 0){
      if(!hasFormat) return "wav fmt chunk not found";
      info.dataOffset = file.position();
      info.dataSize = min(size, (uint32_t)(file.size() - info.dataOffset));
      info.dataSize -= info.dataSize % (info.channels * 2);
      if(info.dataSize == 0) return "wav has no data";
      return nullptr;
    }
    if(!file.seek(next)) break;
  }
  return "wav data chunk not found";
}

/*
 * outBufferをaudio_data_callback()に通してからI2Sに出力する
 * i2s_write()はDMAバッファが空くまで待つため、再生速度に合わせて読み込みが進む
 */
static void writeFrames(uint16_t frames){
  size_t len = frames * 2 * sizeof(int16_t);
  audio_data_callback((const uint8_t*)outBuffer, len);
  size_t written = 0;
  i2s_write(TEST_AUDIO_I2S_PORT, outBuffer, len, &written, portMAX_DELAY);
  playedFrames += frames;
}

/*
 * 停止要求、またはBluetoothが接続されるまでループ再生する
 * Bluetooth接続時や再生できない場合は、停止要求と同じ状態にして終わる
 * MP3と違いデコードが不要なため、追加で必要なメモリはほとんど無い
 */
static void playLoop(){
  playedFrames = 0;
  File file = LittleFS.open(TEST_AUDIO_PATH, "r");
  wavInfo info = {};
  lastError = file ? parseWav(file, info) : "file open failed";
  if(lastError != nullptr){
    if(file) file.close();
    playRequested = false;
    return;
  }

  isPlaying = true;
  //Bluetoothの音声が44100Hz以外だった場合に備えて設定し直す
  i2s_set_clk(TEST_AUDIO_I2S_PORT, TEST_AUDIO_SAMPLE_RATE, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO);
  i2s_start(TEST_AUDIO_I2S_PORT);

  uint16_t bytesPerFrame = info.channels * sizeof(int16_t);
  uint32_t readSize = (TEST_AUDIO_FRAMES / info.repeat) * bytesPerFrame;
  uint32_t remaining = 0;
  while(playRequested){
    if(a2dp_sink.is_connected()){
      playRequested = false;
      break;
    }
    //最後まで再生したら先頭から再生し直す
    if(remaining == 0){
      file.seek(info.dataOffset);
      remaining = info.dataSize;
    }

    int got = file.read((uint8_t*)readBuffer, min(readSize, remaining));
    got -= got % bytesPerFrame;
    if(got <= 0){
      lastError = "wav read failed";
      playRequested = false;
      break;
    }
    remaining -= got;

    //音量を掛け、ステレオ・44100Hzにそろえる
    int32_t volume = volumePercent;
    uint16_t frames = got / bytesPerFrame;
    uint16_t n = 0;
    for(uint16_t i = 0; i < frames; i++){
      int16_t left = readBuffer[i * info.channels] * volume / 100;
      int16_t right = readBuffer[i * info.channels + info.channels - 1] * volume / 100;
      for(uint8_t r = 0; r < info.repeat; r++){
        outBuffer[n * 2] = left;
        outBuffer[n * 2 + 1] = right;
        n++;
      }
    }
    writeFrames(n);
  }

  file.close();
  i2s_zero_dma_buffer(TEST_AUDIO_I2S_PORT);
  //音量レベルは音声データが来たときにしか更新されないため、0に戻さないと最後の音量でサーボが動き続ける
  wrc950.u32Map(MU32_AUD_VOL, 0);
  isPlaying = false;
}

static void testAudioTask(void* arg){
  (void)arg;
  for(;;){
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if(playRequested) playLoop();
  }
}

/*
 * 再生中はファイルを開いたままにするため、LittleFSはアンマウントしないこと
 */
const char* testAudioStart(){
  lastError = nullptr;
  if(a2dp_sink.is_connected()) return "bluetooth connected";
  if(!LittleFS.begin(false) || !LittleFS.exists(TEST_AUDIO_PATH)) return TEST_AUDIO_PATH " not found";
  if(testAudioTaskHandle == nullptr){
    if(xTaskCreatePinnedToCore(testAudioTask, "testAudio", TEST_AUDIO_STACK_SIZE, nullptr,
                               TEST_AUDIO_PRIORITY, &testAudioTaskHandle, TEST_AUDIO_CORE) != pdPASS){
      testAudioTaskHandle = nullptr;
      return "failed to create task";
    }
  }
  playRequested = true;
  xTaskNotifyGive(testAudioTaskHandle);
  return nullptr;
}

void testAudioStop(){
  playRequested = false;
}

bool testAudioIsPlaying(){
  return playRequested || isPlaying;
}

void testAudioSetVolume(uint8_t percent){
  volumePercent = min(percent, (uint8_t)TEST_AUDIO_MAX_VOLUME);
}

uint8_t testAudioGetVolume(){
  return volumePercent;
}

const char* testAudioLastError(){
  return lastError;
}

uint32_t testAudioPlayedFrames(){
  return playedFrames;
}
