// Copyright 2026 Vstone Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

#include "vs_wrc950_memmap.h"
#include "BluetoothA2DPSink.h"
#include "vs_wrc950_ugokun.h"
#include "vs_wrc950_btaudio.h"

BluetoothA2DPSink a2dp_sink;
char* device_name=DEFAULT_NAME;

static const float FS = 44100.0f;   // A2DPのPCMが44.1k前提
static const float FC = 150.0f;     // カットオフ周波数(Hz) 80〜150で調整推奨
static const float Q  = 0.7071f;    // Butterworth

// biquad係数（Direct Form II Transposed）
static float b0, b1, b2, a1, a2;
static float z1 = 0.0f, z2 = 0.0f;  // フィルタ状態

void hpf_init() {
    float w0 = 2.0f * 3.1415926f * (FC / FS);
    float cosw0 = cosf(w0);
    float sinw0 = sinf(w0);
    float alpha = sinw0 / (2.0f * Q);

    // RBJ cookbook: High-pass
    float _b0 =  (1.0f + cosw0) / 2.0f;
    float _b1 = -(1.0f + cosw0);
    float _b2 =  (1.0f + cosw0) / 2.0f;
    float _a0 =  1.0f + alpha;
    float _a1 = -2.0f * cosw0;
    float _a2 =  1.0f - alpha;

    // 正規化
    b0 = _b0 / _a0;
    b1 = _b1 / _a0;
    b2 = _b2 / _a0;
    a1 = _a1 / _a0;
    a2 = _a2 / _a0;

    z1 = z2 = 0.0f;
}

static inline int16_t hpf_process(int16_t x) {
  // Direct Form II Transposed
  float in = (float)x;
  float out = b0 * in + z1;
  z1 = b1 * in - a1 * out + z2;
  z2 = b2 * in - a2 * out;

  // クリップしてint16に戻す
  if (out > 32767.0f) out = 32767.0f;
  if (out < -32768.0f) out = -32768.0f;
  return (int16_t)out;
}

constexpr int16_t LIMIT_PEAK = 14000;

// 0～1
static float limiterGain = 1.0f;

// デバッグ表示用
uint16_t debugPeak = 0;
float debugLimiterGain = 1.0f;

void audio_data_callback(const uint8_t* data, uint32_t len)
{
    uint32_t audio_level = 0;
    uint32_t last_sound_ms = 0;

    int16_t* samples = (int16_t*)data;
    int count = len / 2;

    /* ---------- ① 音量計測（ステレオのまま） ---------- */
    uint32_t sum = 0;
    for (int i = 0; i < count; i++) {
        sum += abs(samples[i]);
    }
    audio_level = sum / count;
    wrc950.u32Map(MU32_AUD_VOL,audio_level);

    if (audio_level > 10) {
        last_sound_ms = millis();
        wrc950.u32Map(MU32_MOV_T0, last_sound_ms);
    }

    uint16_t peak = 0;

    // ========================================
    // 1. モノラル化 + HPF
    // ========================================

    for (int i = 0; i < count; i += 2) {

        int32_t mono =
            ((int32_t)samples[i] +
             (int32_t)samples[i + 1]) / 2;

        int16_t y =
            hpf_process((int16_t)mono);

        samples[i]     = y;
        samples[i + 1] = y;

        uint16_t absSample;

        if (y == INT16_MIN) {
            absSample = 32768;
        } else {
            absSample = abs(y);
        }

        if (absSample > peak) {
            peak = absSample;
        }
    }


    // ========================================
    // 2. 必要なLimiter Gainを計算
    // ========================================

    float targetGain = 1.0f;

    if (peak > LIMIT_PEAK) {

        targetGain =
            (float)LIMIT_PEAK /
            (float)peak;
    }


    // ========================================
    // 3. Attack / Release
    //
    // 音が大きい時は即座に下げる
    // 音が小さくなった時はゆっくり戻す
    // ========================================

    if (targetGain < limiterGain) {

        // Attack
        // 即座にゲインを下げる
        limiterGain = targetGain;

    } else {

        // Release
        // 徐々に1.0へ戻す
        limiterGain +=
            (targetGain - limiterGain) * 0.02f;
    }


    // ========================================
    // 4. Gain適用
    // ========================================

    if (limiterGain < 0.999f) {

        for (int i = 0; i < count; i++) {

            int32_t v =
                (int32_t)(
                    samples[i] * limiterGain
                );

            if (v > 32767)  v = 32767;
            if (v < -32768) v = -32768;

            samples[i] = (int16_t)v;
        }
    }


    // ========================================
    // Debug
    // ========================================

    debugPeak = peak;
    debugLimiterGain = limiterGain;
}

/*
 * BluetoothをクラシックBT(A2DP)専用で起動する
 * a2dp_sink.start()が使うArduinoのbtStart()はBLEとのデュアルモードで起動し、使わないBLE用のメモリを確保したままになる
 * 先にBLE用のメモリを解放してクラシックBTだけで起動しておくと、その分をWi-Fiなどが使える
 * 起動済みの場合は何もしない
 */
static void btStartClassicOnly(){
    if(esp_bt_controller_get_status() != ESP_BT_CONTROLLER_STATUS_IDLE) return;
    esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
    esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    cfg.mode = ESP_BT_MODE_CLASSIC_BT;
    if(esp_bt_controller_init(&cfg) != ESP_OK) return;
    esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
}

void audioInit(){
    i2s_pin_config_t pin_config = {
                .bck_io_num = BCLK_PIN,   // BCLK
                .ws_io_num  = LRC_PIN,   // LRC
                .data_out_num = AUDIO_DO_PIN,  // DIN
                .data_in_num = I2S_PIN_NO_CHANGE
                };
    
    hpf_init();

    a2dp_sink.set_pin_config(pin_config);
    a2dp_sink.set_volume(DEFAULT_VOLME); 
    a2dp_sink.set_stream_reader(audio_data_callback, true);
    btStartClassicOnly();
    a2dp_sink.start(device_name);
}
