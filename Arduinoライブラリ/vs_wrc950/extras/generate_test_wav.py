# Copyright 2026 Vstone Co., Ltd.
# SPDX-License-Identifier: Apache-2.0
"""
テスト再生用の音源 test.wav を合成して作るスクリプト

外部の音源を使わず、このスクリプトだけで同じファイルを作り直せる。
Python 3 の標準ライブラリだけで動く（追加インストール不要）。

    python extras/generate_test_wav.py [出力先]

出力先を省略すると examples/ugokun_common/data/test.wav に書き込む。

動作感度（audio_gain）の調整に使う、ダンス系の短いループ（128BPM・8小節＝ちょうど15秒）。
キック・クラップ・ハイハット・ベース・メロディで、拍に合わせた自然な強弱がある。
最初の2小節はクラップとメロディを控えめにし、少しだけ盛り上がりをつけている。
音量レベル（|サンプル|の平均）は、スマホから流す市販の曲と同じくらいになるよう
TARGET_LEVEL に合わせる。テスト再生の初期音量50%では、その半分のレベルで計測される。
"""

import math
import os
import random
import struct
import sys
import wave

SAMPLE_RATE = 22050
FULL_SCALE = 32767
BPM = 128
BARS = 8
BEAT_S = 60.0 / BPM                  # 1拍の長さ[秒]
DURATION_S = BEAT_S * 4 * BARS       # 15.0秒
TARGET_LEVEL = 4400                  # 再生音量100%のときの平均の音量レベル（50%では約2200）

# 音の高さ[Hz]
def note(name):
    names = {"C": -9, "D": -7, "E": -5, "F": -4, "G": -2, "A": 0, "B": 2}
    return 440.0 * 2 ** ((names[name[0]] + 12 * (int(name[1]) - 4)) / 12)

# 2小節ずつのコード進行（Am - F - C - G）。ベースの音と、メロディに使う音
CHORDS = [
    ("A2", ["A4", "C5", "E5", "A5"]),
    ("F2", ["F4", "A4", "C5", "F5"]),
    ("C3", ["E4", "G4", "C5", "E5"]),
    ("G2", ["D4", "G4", "B4", "D5"]),
]

# 1小節分（16分音符×16）のメロディのリズム。数字はコードの何番目の音か、None は休み
LEAD_PATTERN = [0, None, 2, None, 1, None, 2, 3, None, 2, None, 1, 2, None, 0, None]


class Mixer:
    def __init__(self):
        self.total = int(round(DURATION_S * SAMPLE_RATE))
        self.buf = [0.0] * self.total

    def add(self, start_s, samples):
        """start_s の位置から samples を足し込む。最後からはみ出した分は先頭に回す（ループのつなぎ目対策）"""
        start = int(round(start_s * SAMPLE_RATE))
        for i, v in enumerate(samples):
            self.buf[(start + i) % self.total] += v


def kick(amp):
    """ピッチが下がっていくキック（160Hz→50Hz）"""
    n = int(0.25 * SAMPLE_RATE)
    out, phase = [], 0.0
    for i in range(n):
        t = i / SAMPLE_RATE
        freq = 50 + 110 * math.exp(-t * 30)
        phase += 2 * math.pi * freq / SAMPLE_RATE
        out.append(amp * math.exp(-t * 12) * math.sin(phase))
    return out


def noise_hit(rng, amp, decay_s, length_s, brightness):
    """ノイズで作る打楽器。brightness が大きいほど高い音（ハイハット）、小さいほどクラップ寄り"""
    n = int(length_s * SAMPLE_RATE)
    out, prev = [], 0.0
    for i in range(n):
        x = rng.uniform(-1, 1)
        y = brightness * (x - prev) + (1 - brightness) * x   # 簡単なハイパス
        prev = x
        out.append(amp * math.exp(-i / SAMPLE_RATE / decay_s) * y)
    return out


def bass(freq, amp, length_s):
    """のこぎり波に近い、太めのベース"""
    n = int(length_s * SAMPLE_RATE)
    edge = int(0.005 * SAMPLE_RATE)
    out = []
    for i in range(n):
        t = i / SAMPLE_RATE
        env = min(1.0, (i + 1) / edge, (n - i) / edge) * math.exp(-t * 4)
        w = 2 * math.pi * freq * t
        s = sum(math.sin(k * w) / k for k in range(1, 7))
        out.append(amp * env * s)
    return out


def pluck(freq, amp, length_s):
    """はじいたような短いシンセ音"""
    n = int(length_s * SAMPLE_RATE)
    edge = int(0.003 * SAMPLE_RATE)
    out = []
    for i in range(n):
        t = i / SAMPLE_RATE
        env = min(1.0, (i + 1) / edge, (n - i) / edge) * math.exp(-t * 9)
        w = 2 * math.pi * freq * t
        out.append(amp * env * (math.sin(w) + 0.5 * math.sin(2 * w + 0.3) + 0.25 * math.sin(3 * w)))
    return out


def build():
    rng = random.Random(950)   # 毎回同じファイルになるよう乱数の種を固定
    mix = Mixer()
    sixteenth = BEAT_S / 4

    for bar in range(BARS):
        bar_s = bar * 4 * BEAT_S
        intro = bar < 2                      # 最初の2小節は控えめ
        root, tones = CHORDS[(bar // 2) % len(CHORDS)]

        for beat in range(4):
            beat_s = bar_s + beat * BEAT_S
            # キック：4つ打ち
            mix.add(beat_s, kick(1.0))
            # クラップ：2拍目と4拍目
            if beat in (1, 3):
                mix.add(beat_s, noise_hit(rng, 0.35 if intro else 0.6, 0.06, 0.2, 0.3))
            # ハイハット：裏拍は強く、16分の細かい刻みは弱く
            mix.add(beat_s + BEAT_S / 2, noise_hit(rng, 0.3, 0.04, 0.1, 0.9))
            if not intro:
                for k in (1, 3):
                    mix.add(beat_s + k * sixteenth, noise_hit(rng, 0.1, 0.015, 0.05, 0.9))
            # ベース：裏拍に入るハウスらしいベース
            mix.add(beat_s + BEAT_S / 2, bass(note(root), 0.3, BEAT_S / 2 * 0.9))

        # メロディ：16分音符のパターン。8小節目の後半は1オクターブ上げて次のループへつなぐ
        for step, idx in enumerate(LEAD_PATTERN):
            if idx is None:
                continue
            freq = note(tones[idx])
            if bar == BARS - 1 and step >= 8:
                freq *= 2
            amp = 0.12 if intro else 0.22
            if step % 4 == 0:
                amp *= 1.3                   # 拍の頭を少し強く
            mix.add(bar_s + step * sixteenth, pluck(freq, amp, sixteenth * 1.8))

    # 全体の音量レベルを TARGET_LEVEL に合わせる。大きすぎる部分はやわらかく丸めるので、数回かけて合わせ込む
    scale = TARGET_LEVEL / (sum(abs(v) for v in mix.buf) / mix.total)
    for _ in range(4):
        out = [int(round(soft_clip(v * scale / FULL_SCALE) * FULL_SCALE)) for v in mix.buf]
        scale *= TARGET_LEVEL / (sum(abs(v) for v in out) / len(out))
    return out


def soft_clip(x, knee=0.7):
    """knee までは素通しし、それを超える部分は 1.0 を超えないようなめらかに抑える"""
    a = abs(x)
    if a <= knee:
        return x
    return math.copysign(knee + (1 - knee) * math.tanh((a - knee) / (1 - knee)), x)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    default_out = os.path.join(here, "..", "examples", "ugokun_common", "data", "test.wav")
    out = sys.argv[1] if len(sys.argv) > 1 else default_out

    samples = build()
    with wave.open(out, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SAMPLE_RATE)
        w.writeframes(struct.pack("<%dh" % len(samples), *samples))
    print("wrote %s (%d samples, %.1f s)" % (os.path.normpath(out), len(samples), len(samples) / SAMPLE_RATE))


if __name__ == "__main__":
    main()
