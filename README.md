# JC2432W328C LCD + LVGL テスト（ESP32/PlatformIO）

このプロジェクトは、JC2432W328C（ESP32 + ST7789 2.8" 240x320 + CST820 静電タッチ）を使って、
Adafruit_GFX（ST7789）と LVGL v8 を組み合わせた最小 UI（スライダー＋ボタン）を表示・操作できることを確認するためのテストです。

## これは何か

- 目的: JC2432W328C のディスプレイ表示とタッチ入力を確認する動作検証用コード。
- 構成: Arduino フレームワーク（PlatformIO）上で Adafruit_ST7789 を描画先にし、LVGL のフラッシュ関数で橋渡し。
- 表示: 画面は横向き（landscape, 320x240）で初期化。LVGLの簡易デモ（スライダーとボタン）を表示。
- タッチ: CST820（I2C）からの座標を取得し、画面向きに合わせて座標補正して LVGL のポインタ入力に接続。

## ハードウェア接続（JC2432W328C 既定）

ディスプレイ（ST7789, SPI）

- MOSI: GPIO 13
- SCLK: GPIO 14
- CS:   GPIO 15
- DC:   GPIO 2
- RST:  基板上でEN/3V3に結線（GPIO未使用のためソフト未接続=-1）
- BL:   GPIO 27（バックライト制御, HIGH=点灯）

タッチパネル（CST820, I2C）

- SDA:  GPIO 33
- SCL:  GPIO 32
- RST:  GPIO 25
- INT:  GPIO 21
- I2C アドレス: 0x15

基板により配線が異なる個体があるため、動作しない場合は上記ピン定義を実機に合わせて `src/main.cpp` 冒頭の `#define TOUCH_*` を修正してください。

## ソフトウェア構成

- フレームワーク: Arduino（PlatformIO）
- ライブラリ:
  - Adafruit GFX Library / Adafruit ST7735 and ST7789 Library / Adafruit BusIO
  - LVGL v8
- 設定ファイル:
  - `platformio.ini`: 依存関係・ポート・ビルドフラグ
  - `include/lv_conf.h`: LVGL 設定（16bit色、LV_TICK_CUSTOM、LV_COLOR_16_SWAP=1 など）
- 橋渡し（表示）:
  - `src/main.cpp` で `lv_disp_drv_t::flush_cb` を実装し、Adafruit_ST7789 の `writePixels()` へ転送
  - `LV_COLOR_16_SWAP=1` に合わせ、`writePixels(..., bigEndian=true)` を使用
- 橋渡し（タッチ）:
  - `src/CST820.{h,cpp}`: シンプルな CST820 I2C ドライバ（0x15）
  - 取得した生座標を画面回転に合わせて変換（横向き/180度補正）して `lv_indev` に供給

## ビルドと書き込み

1. USB で ESP32 を接続
2. シリアルポートを確認: `pio device list`
3. `platformio.ini` の `upload_port`/`monitor_port` を実ポートに合わせる（例: `/dev/cu.usbserial-210`）
4. ビルド＆書き込み: `pio run -t upload`
5. モニタ（任意）: `pio device monitor -b 115200`
6. ビルド&書き込みして即座にモニタする: `pio run -t upload && sleep 1 && pio device monitor -b 115200`

起動後の期待動作

- 画面に「Slider + Button Demo」とスライダー、ボタンが表示されます。
- 画面左下にタッチの配線情報（例: `Touch: SDA=33 SCL=32 addr=0x15`）が表示されます。
- タッチでスライダーを動かすと「Value: xx」が更新され、ボタンは ON/OFF が切り替わります。

## コードのポイント（概要）

- `src/main.cpp`
  - 表示初期化: `tft.init(240, 320)` → `tft.setRotation(1)`（横向き 320x240）
  - LVGL 初期化: 320×40 行のダブルバッファで `lv_disp_drv` を登録
  - フラッシュ関数: `tft.writePixels()` で矩形転送（`LV_COLOR_16_SWAP=1`）
  - UI: スライダーとボタンを Flex レイアウトで縦並びに配置
  - タッチ: CST820 から (x,y) を取得し、横向きかつ 180°の補正をかけて `lv_indev` に渡す
- `src/CST820.cpp`
  - I2C 400kHz、アドレス 0x15 を使用
  - 最小限の読み書きと 0x03 以降の座標バイトから 12bit 座標を生成
- `include/lv_conf.h`
  - `LV_COLOR_DEPTH 16` / `LV_COLOR_16_SWAP 1` / `LV_TICK_CUSTOM 1 (millis)`

## トラブルシュート

- 画面が真っ白
  - SPI ピン（MOSI=13/SCLK=14/CS=15/DC=2/BL=27）が正しいか確認
  - `tft.invertDisplay(true/false)` や `tft.setSPISpeed(20000000)` を試す
- 色がずれる/端にノイズ
  - `LV_COLOR_16_SWAP` と `writePixels(..., bigEndian)` の組み合わせを見直す
  - `tft.setColRowStart(x,y)`（必要時のみ）
- タッチが反応しない
  - `SDA=33/SCL=32/RST=25/INT=21` の配線確認
  - I2C プルアップや配線不良を確認
  - 位置がずれる場合は `src/main.cpp` 冒頭の補正マクロ（`TOUCH_ROTATE_180` など）を調整

## 参考

- JC2432W328 ドキュメントとサンプル: `repo_JC2432W328/`
- LVGL: https://lvgl.io
- Adafruit ST77xx/Adafruit GFX: https://github.com/adafruit/Adafruit-ST7735-Library

JC2432W328
https://github.com/maxpill/JC2432W328

・JC2432W328 「Arduino_GFX_Library」でニュース表示をしてみた
https://gijin77.blog.jp/archives/41390669.html

・JC2432W328で抵抗のカラーコード計算機を作ってみた
https://gijin77.blog.jp/archives/42106816.html?fbclid=IwY2xjawMqQ1VleHRuA2FlbQIxMABicmlkETF0MjhsZkREZVpUWmh5TGFYAR5lH7baFeib1DR1prp01_gbpNxxXXF9Q4fk96wMWo8q_M3bXAw0hISidxATxA_aem_8DWv-j3tDK17Jk4XeptLvw

esp32-2432s028rでLovyanGFX+LVGLを使う
https://qiita.com/sylphy_00/items/77f5b9d5fdba85860d9d

ESP32-2432S028R + EZZ studioで動かしてみた
https://qiita.com/kyo-kobo/items/91f918f32cb5c06d0cd2

[ESP32-2432S024C] uses CST820 instead of CST816 and the INT GPIO is different #51
https://github.com/rzeldent/platformio-espressif32-sunton/issues/51

Working CYD JC2432W328 Display 240x320 2.8" USB-C working with code example.
https://www.reddit.com/r/esp32/comments/1dy5k11/working_cyd_jc2432w328_display_240x320_28_usbc/?show=original

Getting Started with ESP32 Cheap Yellow Display Board – CYD (ESP32-2432S028R)
https://randomnerdtutorials.com/cheap-yellow-display-esp32-2432s028r/

CYD - JC2432W328 - Working Nerdminer V2
https://www.reddit.com/r/esp32/comments/1k5tskc/cyd_jc2432w328_working_nerdminer_v2/

touch not Working CYD JC2432W328 Display 240x320 2.8" USB-C
https://www.reddit.com/r/esp32/comments/1gmpmy4/touch_not_working_cyd_jc2432w328_display_240x320/?show=original

ESP32 2432S028R (CYD)でLVGL - ILI9341 vs ST7789 速度比較
https://embedded-kiddie.github.io/2025/03/09/
