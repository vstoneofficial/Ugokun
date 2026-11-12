# vs_wrc950

VS-WRC950（ESP32）を搭載した「うごくん」用の Arduino ライブラリです。
メインのスケッチは [examples/ugokun_common/ugokun_common.ino](examples/ugokun_common/ugokun_common.ino) です。

## 動作環境

| 項目 | 内容 |
| --- | --- |
| Arduino IDE | 1.8.19 / 2.3.10 |
| ボードパッケージ | esp32 by Espressif Systems **2.0.13**（2.0.x 系） |
| ボード | ESP32 Dev Module |
| Flash Size | **8MB (64Mb)** |
| Partition Scheme | **8M with spiffs (3MB APP/1.5MB SPIFFS)**（出荷時と同じ設定） |

### 注意点

- **ESP32 ボードパッケージ 3.x は使用できません。** ライブラリ内で `timerBegin(0, 80, true)` / `timerAlarmWrite()`、`ledcSetup()` / `ledcAttachPin()` など 2.x 系の API を使用しているためです。
- **Flash Size は「8MB」、Partition Scheme は「8M with spiffs (3MB APP/1.5MB SPIFFS)」を選択してください。** 出荷時のうごくんと同じ設定です。既定の「Default 4MB with spiffs (1.2MB APP)」ではプログラムサイズ超過（約 1.40MB / 上限 1.31MB）でビルドに失敗します。
- 「Huge APP (3MB No OTA/1MB SPIFFS)」でもビルドできますが、ファイルシステムの位置と容量（896KB）が変わります。その場合は `data` フォルダも書き込み直してください。
- micro-ROS ライブラリは ESP32 用にビルド済みの `libmicroros.a` をリンクします（`precompiled=true`）。Arduino IDE 1.8.19 / 2.3.10 はこの形式に対応しているため、`platform.txt` の変更は不要です。

## 必要なライブラリ

### 別途インストールが必要なもの

| ライブラリ | 確認したバージョン | 使用箇所 | 入手先 |
| --- | --- | --- | --- |
| ESP32-A2DP | 1.8.8 | [src/vs_wrc950_btaudio.h](src/vs_wrc950_btaudio.h)（`BluetoothA2DPSink.h`：Bluetooth オーディオ受信） | https://github.com/pschatzmann/ESP32-A2DP |
| ESP32Servo | 0.12.0 | [src/vs_wrc950_servo.h](src/vs_wrc950_servo.h)（`ESP32Servo.h`：PWM サーボ制御） | ライブラリマネージャで「ESP32Servo」を検索、または https://github.com/madhephaestus/ESP32Servo |
| ArduinoJson | 7.0.4（**7.x が必要**） | [ugokun_common.ino](examples/ugokun_common/ugokun_common.ino)（設定ファイル `config.json` の読み込み） | ライブラリマネージャで「ArduinoJson」を検索、または https://github.com/bblanchon/ArduinoJson |
| micro_ros_arduino（ROS 2 **Humble**, ヴイストン版） | Humble 用リリース | [ugokun_common.ino](examples/ugokun_common/ugokun_common.ino)（`micro_ros_arduino.h`、`rcl/`、`rclc/`、`std_msgs/msg/int16_multi_array.h` など） | https://github.com/vstoneofficial/micro_ros_arduino/releases（Humble 用 ZIP） |

#### ESP32-A2DP について

本ライブラリは `a2dp_sink.set_pin_config(i2s_pin_config_t)` による旧来の I2S 出力（legacy I2S）を使用しています。

- ESP32 ボードパッケージ 2.x（ESP-IDF 4.x）と組み合わせれば、ESP32-A2DP 1.8.x でも legacy I2S が有効になりビルドできます。
- **`AudioTools`（arduino-audio-tools）を同じ `libraries` フォルダに入れないでください。** ESP32-A2DP は `AudioTools.h` を検出すると AudioTools 経由の出力に切り替わり、`set_pin_config()` が使えなくなります。

#### micro_ros_arduino について

- 実機での動作確認は **ROS 2 Humble** で行っています。Humble 用のリリースを `libraries/micro_ros_arduino_humble` に配置してください。
- スケッチは `#include <micro_ros_arduino.h>` で読み込んでいます。
- 公式版（micro-ROS/micro_ros_arduino）や Jazzy 版など、**別の micro_ros_arduino を同時にインストールしないでください。** 同名ヘッダーが競合します。
- PC 側は ROS 2 Humble と micro-ROS Agent（Humble）を使用します。マイコン側ライブラリ・PC 側 ROS 2・Agent の版はすべて揃えてください。
- 有線シリアル接続時の通信速度は 921600 bps です（`ROS2_SERIAL_BAUDRATE`）。Docker で Agent を起動する例：

  ```bash
  docker run -it --rm --net=host --device=/dev/ttyUSB0 \
    microros/micro-ros-agent:humble serial --dev /dev/ttyUSB0 -b 921600
  ```

### ESP32 ボードパッケージに同梱（追加インストール不要）

| ライブラリ / ヘッダー | 使用箇所 |
| --- | --- |
| Wire | [src/vs_wrc950_i2c.cpp](src/vs_wrc950_i2c.cpp)（I2C マスター / スレーブ） |
| LittleFS | [ugokun_common.ino](examples/ugokun_common/ugokun_common.ino)（設定ファイルの読み込み）、[src/vs_wrc950_wifi.cpp](src/vs_wrc950_wifi.cpp)（設定画面の配信、設定ファイルへの保存） |
| WiFi | [ugokun_common.ino](examples/ugokun_common/ugokun_common.ino)、[src/vs_wrc950_memmap.h](src/vs_wrc950_memmap.h)、[src/vs_wrc950_wifi.cpp](src/vs_wrc950_wifi.cpp) |
| WebServer | [src/vs_wrc950_wifi.cpp](src/vs_wrc950_wifi.cpp)（ブラウザ用の設定画面） |
| ESP-IDF ヘッダー（`driver/uart.h`、`driver/gpio.h`、`soc/gpio_sig_map.h`、`esp32/rom/gpio.h`） | [src/vs_wrc950_onewireuart.cpp](src/vs_wrc950_onewireuart.cpp)（1 線式 UART サーボ通信） |

## インストール手順

1. Arduino IDE の「環境設定」（2.x では「ファイル」→「基本設定」）→「追加のボードマネージャの URL」に次を追加します。
   `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
2. 「ツール」→「ボード」→「ボードマネージャ」で **esp32 2.0.13** をインストールします。3.x は選ばないでください。
3. 上記「別途インストールが必要なもの」のライブラリを、スケッチブックの `libraries` フォルダ（例：`Documents/Arduino/libraries`）に配置します。
4. 本ライブラリ（`vs_wrc950` フォルダ）も同じ `libraries` フォルダに配置します。
5. Arduino IDE を再起動し、`ファイル` → `スケッチ例` → `vs_wrc950` → `ugokun_common` を開きます。
6. 「ツール」で次を設定してビルド・書き込みします。
   - ボード：ESP32 Dev Module
   - Flash Size：8MB (64Mb)
   - Partition Scheme：8M with spiffs (3MB APP/1.5MB SPIFFS)
7. `data` フォルダ（設定ファイル `config.json` と設定画面 `index.html`）を ESP32 のファイルシステムへ書き込みます（次節）。

## 設定ファイル（config.json）

`ugokun_common.ino` は、起動時に ESP32 のファイルシステム（LittleFS）から `/config.json` を読み込みます。
元のファイルは [examples/ugokun_common/data/config.json](examples/ugokun_common/data/config.json) です。

```json
{
  "audio_gain": 100
}
```

| キー | 内容 |
| --- | --- |
| `audio_gain` | 動作感度。音量レベルにこの倍率を掛けてから、サーボの可動範囲を決めます。値は倍率の 100 倍の整数で、100 で 1 倍、250 で 2.5 倍です。1〜65535 の整数を指定します。大きくするほど、小さい音でも大きく動きます。 |
| `device_name` | デバイス名（省略可）。Bluetooth のデバイス名と、うごくん自身がアクセスポイントになる場合の SSID に使います。半角英数字と記号（空白を含む）で 1〜32 文字です。省略した場合は `UGOKUN_` + `ROBOT_NUM`（例：`UGOKUN_3`）になります。 |

- ファイルがない、JSON として読めない、値が範囲外の場合は、スケッチ内の `AUDIO_GAIN`（100）を使います。
- 読み込んだ値はメモリマップの `MU16_AUD_GAIN` に設定されます。起動後にメモリマップへ書き込めば、実行中に変更することもできます（ファイルには保存されません）。
- `device_name` が不正な場合（33 文字以上、全角文字を含むなど）は、`UGOKUN_` + `ROBOT_NUM` を使います。
- `device_name` はブラウザの設定画面から変更できます。保存した名前は再起動後に反映されます。ROS のノード名（`ugokun_3` など）は `ROBOT_NUM` のままで、変わりません。
- 同梱の `config.json` には `device_name` を入れていません。入れると、data フォルダを書き込んだすべての個体が同じ名前になるためです。また、data フォルダを書き込み直すと `config.json` も上書きされ、画面で保存した動作感度とデバイス名は消えます。
- 起動時のシリアル出力（`device_name: …`、`audio_gain: …`）で、実際に使われている値を確認できます（ROS の有線シリアルモード時は出力されません）。

### ファイルシステムへの書き込み方法

スケッチの `data` フォルダの中身を、LittleFS としてフラッシュへ書き込みます。
スケッチの書き込みとは別の操作です。`config.json` を変更したときも、この操作で書き込み直してください。

- **Arduino IDE 2.x**：[arduino-littlefs-upload](https://github.com/earlephilhower/arduino-littlefs-upload) の `.vsix` を `~/.arduinoIDE/plugins/` に置いて IDE を再起動し、`Ctrl + Shift + P` →「Upload LittleFS to Pico/ESP8266/ESP32」を実行します。
- **Arduino IDE 1.8.19**：[arduino-esp32fs-plugin](https://github.com/lorol/arduino-esp32fs-plugin) を `Arduino/tools/` に置いて IDE を再起動し、「ツール」→「ESP32 Sketch Data Upload」で **LittleFS** を選んで実行します。

書き込み時は、シリアルモニタを閉じてください。また、ボード設定の Partition Scheme をスケッチ書き込み時と同じにしてください。
ファイルシステムの書き込みでは、`data` フォルダにある他のファイル（`padAddr.txt` など）も一緒に書き込まれ、既存の内容はすべて置き換わります。
ブラウザの設定画面で保存した値も上書きされるので、必要なら `data/config.json` に同じ値を書いてから書き込んでください。

## ブラウザからのパラメータ設定（Wi-Fi）

`ugokun_common.ino` の `WRC_WIRELESS_MODE` を `USE_WIFI` にすると、ESP32 が Web サーバとして動作します。
ブラウザで `http://<IPアドレス>/` を開くと、パラメータを確認・変更できます。
設定画面は [examples/ugokun_common/data/index.html](examples/ugokun_common/data/index.html) で、LittleFS から配信します。
事前に `data` フォルダをファイルシステムへ書き込んでください（未書き込みの場合は `index.html not found` と表示されます）。
画面を変更したときも、スケッチの書き込みは不要で、ファイルシステムの書き込みだけで反映されます。

### 接続方法

| `ssid` の設定 | 動作 | 開くアドレス |
| --- | --- | --- |
| 設定あり | そのアクセスポイントに接続します（最大 10 秒待ちます） | 起動時にシリアルモニタに表示される IP アドレス |
| 空、または接続失敗 | うごくん自身がアクセスポイントになります。SSID は Bluetooth のデバイス名（例：`UGOKUN_3`）です | `http://192.168.4.1/` |

自身がアクセスポイントになる場合、`ap_password` が空だとパスワードなしで誰でも接続できます。8 文字以上のパスワードを設定することをおすすめします。

アクセスポイントの設定は Flash に保存せず、起動のたびに電波を止めた状態で設定してから電波を出します。電波を出したまま設定を書き換えると、前回の設定が残り、正しいパスワードでも「パスワードが違う」と表示されて接続できないことがあるためです。

自身がアクセスポイントになる場合、設定できるのは Bluetooth が未接続のときだけです。

| Bluetooth | アクセスポイント（`UGOKUN_3` など） |
| --- | --- |
| 未接続 | 動作します。接続して設定画面を開けます |
| 接続中 | Wi-Fi はつながったままですが、設定画面と API は「Bluetooth接続中は設定できません」と返し、設定を受け付けません |
| 切断後 | すぐに設定画面を開けます |

設定を変更するときは、スマホなどの Bluetooth 接続を切ってから、ブラウザでページを開き直してください。

Bluetooth 接続中はアクセスポイント自体を止めたい場合は、[src/vs_wrc950_wifi.h](src/vs_wrc950_wifi.h) の `WIFI_AP_PAUSE_ON_BT` を 1 にしてください。切断後、3 秒（`WIFI_AP_RESTART_DELAY_MS`）たってから再開します。ただし、この設定で再開した後の動作は、メモリ不足の対策をした後には実機で確認していません。

Wi-Fi に接続できない、ページが開かないときは、`WIFI_AP_DEBUG` を 1 にすると、端末の接続・切断などのイベントが空きメモリと一緒にシリアルモニタに出力されます。
`ssid` を設定して既存のアクセスポイントに接続している場合は、Bluetooth の接続状態に関係なく設定画面を開けます。

うごくん自身がアクセスポイントになっている場合は、スマホや PC を `UGOKUN_3` などに接続してから、ブラウザで `http://192.168.4.1/` を開きます。

Android 版 Chrome では、`ERR_TOO_MANY_RETRIES` で開けないことがあります。
`UGOKUN_3` はインターネットにつながらないため、Chrome が Wi-Fi を通信に使わないことが原因です。
Wi-Fi 接続時に「インターネットに接続されていません」と表示されたら「接続を維持」を選ぶか、モバイルデータをオフにするか、Firefox など別のブラウザで開いてください。

### テスト再生しながら調整する

`data` フォルダに `test.wav` を入れてファイルシステムへ書き込んでおくと、設定画面の「テスト再生」で、その音源をうごくんのスピーカーからループ再生できます。
Bluetooth で受信した音声と同じ処理（音量計測・フィルタ）を通すため、サーボも実際の再生時と同じように動きます。
動かしながら動作感度などを変えて「適用」すると、すぐに動きに反映されます。

| 操作 | 内容 |
| --- | --- |
| 再生 / 停止 | `test.wav` をループ再生・停止します |
| 再生音量スライダー | 0〜100%。スマホ側の音量を変えたときと同じく、音量計測の前に掛けます。大きい音・小さい音でどう動くかを試せます |

- 再生できるのは Bluetooth が未接続のときだけです。再生中に Bluetooth がつながると、自動で停止します。
- 再生できるのは **16bit リニア PCM の WAV**（モノラル / ステレオ、44100 / 22050 / 11025Hz）です。22050Hz と 11025Hz は、同じサンプルを繰り返して 44100Hz にして出力します（動作のフィルタが 44.1kHz 前提のため）。
- MP3 は使えません。Bluetooth と Wi-Fi の動作中は空きメモリに余裕がなく、MP3 のデコード（約 30KB）ができないためです。WAV はデコードが不要なので、追加のメモリはほとんど使いません。
- 同梱の `test.wav` は、[extras/generate_test_wav.py](extras/generate_test_wav.py) で合成した 15 秒の音源（22050Hz モノラル、約 650KB）です。キック・クラップ・ハイハット・ベース・メロディによるダンス系のループ（128BPM・8 小節）で、拍に合わせた強弱があります。音量レベルは、スマホから流す市販の曲と同じくらい（再生音量 50% で平均約 2200）に合わせています。`python extras/generate_test_wav.py` を実行すると、同じファイルを作り直せます（Python 3 の標準ライブラリだけで動きます）。
- 別の音源にする場合は、たとえば ffmpeg で次のように変換します。

  ```bash
  ffmpeg -i 元の音源.mp3 -t 15 -ac 1 -ar 22050 -c:a pcm_s16le -map_metadata -1 test.wav
  ```
- 8M with spiffs のファイルシステムは 1.5MB です。`config.json`、`index.html` と合わせて収まるサイズ（目安 1.4MB 以下）にしてください。22050Hz モノラルで約 30 秒、44100Hz ステレオでは約 8 秒です（Huge APP の場合は 896KB で、目安 800KB 以下）。
- 再生できない場合は、再生ボタンの下に停止理由（`wav must be 16bit PCM` など）が表示されます。
- 再生音量は保存されません。再起動すると 50% に戻ります。

### 変更できるパラメータ

| 項目 | メモリマップ | 範囲 |
| --- | --- | --- |
| 動作感度（倍） | `MU16_AUD_GAIN` | 0.01〜655.35 倍（画面では倍率で表示。メモリマップと `config.json` の値は 100 倍した整数） |
| ヨー軸 可動範囲の最大 | `MU16_AY_MAX` | 0〜150 deg（中心から左右それぞれの角度） |
| スロットル軸 可動範囲の最大 | `MU16_AT_MAX` | 0.0〜35.0 mm（上下の移動距離。メモリマップの値は角度[deg]のままで、画面と `/api/params` では移動距離に換算する。API の値は 10 倍した整数） |

次の 3 つは画面には表示していませんが、`/api/params` からは読み書きできます。画面に戻す場合は、[src/vs_wrc950_wifi.cpp](src/vs_wrc950_wifi.cpp) のパラメータ一覧で `hidden` を `false` にしてください。

| 項目 | メモリマップ | 範囲 |
| --- | --- | --- |
| 動作する最低音量レベル | `MU32_AUD_MIN` | 0〜32768 |
| 可動範囲が最大になる音量レベル | `MU32_AUD_MAX` | 1〜32768（最低音量レベルより大きい値） |
| 現在の音量レベル（読み取りのみ） | `MU32_AUD_VOL` | — |

- **適用**：すぐに動作に反映されますが、再起動すると元に戻ります。
- **適用して保存**：適用したうえで、動作感度を `/config.json` に保存します。次回の起動時から、その値が使われます。`config.json` の他のキーは残ります。

項目を増やすときは、[src/vs_wrc950_wifi.cpp](src/vs_wrc950_wifi.cpp) の `webParams` に追加してください。

### 注意点

- Wi-Fi を有効にすると、プログラムが約 320KB 増えます（約 1.48MB → 約 1.81MB）。Partition Scheme は、アプリ領域が 3MB 以上のもの（8M with spiffs、Huge APP など）を選んでください。
- ESP32 は、Bluetooth オーディオと Wi-Fi で同じアンテナを共用します。Wi-Fi の通信中に、音が途切れることがあります。
- Bluetooth と Wi-Fi を同時に使うとメモリがぎりぎりになります。そのため `audioInit()` では、使わない BLE 用のメモリを解放してから、通常の Bluetooth（A2DP）だけで起動しています。これで起動時の空きメモリが約 41KB から約 58KB に増え、Bluetooth を切断した後も Wi-Fi で設定画面を開けるようになりました。BLE は使えません。
- ROS を Wi-Fi で使う場合（`WRC_ROS_SERIAL_MODE` が `MODE_WIFI`）は、micro-ROS が接続した Wi-Fi をそのまま使います。アクセスポイントモードにはなりません。

## ビルド確認結果

以下の構成で `ugokun_common.ino` のコンパイルが通ることを確認しています。
なお、このコンパイル確認は Jazzy 版 micro_ros_arduino で行ったものです。実機での動作確認は Humble 版で行っています。

| 項目 | バージョン |
| --- | --- |
| Arduino IDE（arduino-builder） | 1.8.19 |
| esp32 ボードパッケージ | 2.0.6（ESP-IDF v4.4.3） |
| ESP32-A2DP | 1.8.8 |
| ESP32Servo | 0.12.0 |
| ArduinoJson | 7.0.4 |
| micro_ros_arduino（Jazzy、コンパイル確認のみ） | 2.0.8-jazzy |
| 結果（Huge APP） | 1,396,109 バイト（44%）／ 3,145,728 バイト |

## ライセンス

このライブラリは [Apache License 2.0](LICENSE) で公開しています。

Copyright 2026 Vstone Co., Ltd.

ただし、以下のファイルは他者が作成したもので、それぞれのファイルに記載されたライセンスに従います。

| ファイル | 作成元 | ライセンス |
| --- | --- | --- |
| [examples/ugokun_common/debug.svd](examples/ugokun_common/debug.svd)、[esp32.svd](examples/ugokun_common/esp32.svd) | Espressif Systems | Apache License 2.0 |
| [examples/ugokun_common/debug.cfg](examples/ugokun_common/debug.cfg) | OpenOCD | GPL-2.0-or-later |

依存ライブラリ（ESP32-A2DP、ESP32Servo、ArduinoJson、micro_ros_arduino）は同梱していません。各ライブラリのライセンスに従ってご利用ください。
