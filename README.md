# Ugokun

ヴイストンの存在強調デバイス「うごくん」のソフトウェアとCADデータです。

うごくんは、Bluetoothスピーカーとして再生した音に合わせて、上部のステージ台が上下・左右に動くデバイスです。ステージ台に載せたキャラクターや物体を、話しているように見せる演出ができます。制御基板 VS-WRC950（ESP32搭載）のプログラムは、Arduino IDEで書き換えられます。

- 製品ページ：[うごくん（ヴイストンロボットショップ）](https://www.vstone.co.jp/robotshop/index.php?main_page=product_info&cPath=846&products_id=5441)

## このリポジトリの内容

| フォルダ | 内容 |
| --- | --- |
| [arduino/vs_wrc950](arduino/vs_wrc950) | うごくん用のArduinoライブラリと、標準スケッチ `ugokun_common` |
| [cad](cad) | アタッチメントと、アタッチメントを取り付ける本体側の部品（ステージ台）のSTEPデータ |

## 使い方

### Bluetoothスピーカーとして使う

プログラムの書き込みは不要です。電源を入れ、スマートフォンやPCのBluetooth設定から「UGOKUN_0」に接続して音を再生してください。詳しくは、製品の取扱説明書を参照してください。

### 動き方を変える・プログラムを書き換える

1. Arduino IDE（2.3.10 または 1.8.19）と、ESP32ボードパッケージ（esp32 by Espressif Systems 2.0.x）をインストールします。3.x は使えません。
2. このリポジトリを「Code」→「Download ZIP」でダウンロードして展開します。
3. `arduino/vs_wrc950` フォルダを、Arduinoの `libraries` フォルダにコピーします。ライブラリがリポジトリの直下にないため、「.ZIP形式のライブラリをインストール」は使えません。
4. 必要な外部ライブラリ（ESP32-A2DP、ESP32Servo、ArduinoJson、micro_ros_arduino）をインストールします。
5. 「ファイル」→「スケッチ例」→「vs_wrc950」→「ugokun_common」を開き、書き込みます。

動作環境、ボード設定、ファイルシステム（`data` フォルダ）の書き込み方は、[arduino/vs_wrc950/README.md](arduino/vs_wrc950/README.md) を参照してください。

### アタッチメントを作る

ステージ台には、はめ込み式のアタッチメントを取り付けられます。[cad](cad) フォルダのSTEPデータをもとに、自由にアタッチメントを作成できます。ファイルの一覧と注意事項は、[cad/README.md](cad/README.md) を参照してください。

## 出荷時の設定

| 項目 | 内容 |
| --- | --- |
| Bluetoothのデバイス名 | UGOKUN_0 |
| Wi-Fi | うごくん自身がアクセスポイントになります（SSID：UGOKUN_0、パスワード：ugokunugokun） |
| 設定画面 | http://192.168.4.1/ （動作感度、可動範囲、デバイス名を変更できます） |

## ライセンス

このリポジトリは、[Apache License 2.0](arduino/vs_wrc950/LICENSE) で公開しています。一部のファイルは他者が作成したもので、それぞれのライセンスに従います。詳しくは [arduino/vs_wrc950/README.md](arduino/vs_wrc950/README.md) のライセンスの項を参照してください。依存ライブラリは同梱していません。各ライブラリのライセンスに従ってご利用ください。

Copyright 2026 Vstone Co., Ltd.

## お問い合わせ

ヴイストン株式会社（[www.vstone.co.jp](https://www.vstone.co.jp)）
E-mail：infodesk@vstone.co.jp
