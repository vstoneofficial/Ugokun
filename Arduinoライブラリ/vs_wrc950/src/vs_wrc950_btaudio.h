// Copyright 2026 Vstone Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

/**
 * VS-WRC950用ライブラリの各ファイルをまとめてincludeするためのヘッダ
 * およびユーティリティ的な関数集
 */
#ifndef WRC950_BTAUDIO_H
#define WRC950_BTAUDIO_H

#include "vs_wrc950_memmap.h"
#include "BluetoothA2DPSink.h"
#include "vs_wrc950_ugokun.h"

#define BCLK_PIN    26
#define LRC_PIN     25
#define AUDIO_DO_PIN   5
#define DEFAULT_VOLME   63
#define DEFAULT_NAME    "UGOKUN"

extern BluetoothA2DPSink a2dp_sink;

extern int default_volume;
extern char* device_name;

void hpf_init();
void audio_data_callback(const uint8_t* data, uint32_t len);
void audioInit();

#endif /* WRC950 */
