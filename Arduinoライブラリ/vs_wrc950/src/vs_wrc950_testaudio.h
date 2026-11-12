// Copyright 2026 Vstone Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

/**
 * VS-WRC950用 パラメータ調整のためのテスト音源のループ再生
 * LittleFS上のWAVファイルを、Bluetoothで受信した音声と同じ処理(音量計測・フィルタ)を通してI2Sに出力する
 * I2SはBluetoothオーディオ(a2dp_sink)が初期化したものを使うため、audioInit()の後に使用すること
 */
#ifndef WRC950_TESTAUDIO_H
#define WRC950_TESTAUDIO_H

#include <Arduino.h>

#define TEST_AUDIO_PATH            "/test.wav"  //再生するLittleFS上のファイル(16bit PCM、44100/22050/11025Hz)
#define TEST_AUDIO_DEFAULT_VOLUME  50           //再生音量の初期値[%]
#define TEST_AUDIO_MAX_VOLUME      100          //再生音量の最大値[%]

/*
 * テスト音源のループ再生を開始する
 * Bluetooth接続中、またはファイルが無い場合は再生しない
 * 戻り値: 開始できればnullptr、できなければ理由を表す文字列
 */
const char* testAudioStart();

/*
 * テスト音源の再生を停止する
 */
void testAudioStop();

/*
 * 再生中(開始要求済みを含む)であればtrue
 * Bluetoothが接続されると自動的に停止する
 */
bool testAudioIsPlaying();

/*
 * 再生音量[%]を設定する。0～TEST_AUDIO_MAX_VOLUME
 * スマホ側の音量と同じく、音量計測の前に掛けるため、サーボの動きにも反映される
 */
void testAudioSetVolume(uint8_t percent);
uint8_t testAudioGetVolume();

/*
 * 動作確認用の情報
 * testAudioLastError(): 直近の再生で止まった理由。問題が無ければnullptr
 * testAudioPlayedFrames(): 直近の再生でI2Sに出力したフレーム数。再生中に増えていれば音は出力されている
 */
const char* testAudioLastError();
uint32_t testAudioPlayedFrames();

#endif /* WRC950_TESTAUDIO_H */
