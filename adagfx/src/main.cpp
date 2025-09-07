// ESP32 + ST7789 (JC2432W328)
// Adafruit_GFX/Adafruit_ST7789 + LVGL 連携最小実装

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SD.h>
#include <esp_heap_caps.h>
#include <lvgl.h>
#include "CST820.h"

#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1   // RST未接続なら -1
#define TFT_BL   27
#define TFT_SCLK 14
#define TFT_MOSI 13
#define TFT_MISO 12

// SPIバスを分離する:
//  - TFT: HSPI (SCLK=14, MOSI=13, MISO=12, CS=15)
//  - SD : VSPI (SCLK=18, MOSI=23, MISO=19, CS=5)
static SPIClass hspi(HSPI);
Adafruit_ST7789 tft = Adafruit_ST7789(&hspi, TFT_CS, TFT_DC, TFT_RST);

// JC2432W328 (Capacitive) 既定ピン: SDA=33, SCL=32, RST=25, INT=21
#ifndef TOUCH_SDA
#define TOUCH_SDA 33
#endif
#ifndef TOUCH_SCL
#define TOUCH_SCL 32
#endif
#ifndef TOUCH_RST
#define TOUCH_RST 25
#endif
#ifndef TOUCH_INT
#define TOUCH_INT 21
#endif

// Rotation=1想定の座標補正
#ifndef TOUCH_SWAP_XY
#define TOUCH_SWAP_XY 1
#endif
#ifndef TOUCH_FLIP_X
#define TOUCH_FLIP_X 1
#endif
#ifndef TOUCH_FLIP_Y
#define TOUCH_FLIP_Y 0
#endif

// 必要に応じてタッチ座標を180度回転
#ifndef TOUCH_ROTATE_180
#define TOUCH_ROTATE_180 1
#endif

static CST820* tp = nullptr;

static void print_mem(const char* stage) {
  size_t free8   = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  size_t freeDMA = heap_caps_get_free_size(MALLOC_CAP_DMA);
  size_t freePS  = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  Serial.printf("[MEM %s] DRAM free=%u KB, largest=%u KB, DMA free=%u KB, PSRAM free=%u KB\n",
                stage, (unsigned)(free8/1024), (unsigned)(largest/1024), (unsigned)(freeDMA/1024), (unsigned)(freePS/1024));
}

static bool i2c_probe_addr(uint8_t sda, uint8_t scl, uint8_t addr) {
  Wire.begin(sda, scl);
  Wire.setClock(400000);
  Wire.beginTransmission(addr);
  int err = Wire.endTransmission(true);
  return err == 0;
}

// 自動探索は使わない（固定ピンで初期化）

// LVGL用描画バッファ
// メモリ節約のためシングルバッファに変更（80行: 約50KB）
#ifndef LVGL_BUF_LINES
#define LVGL_BUF_LINES 80
#endif
static lv_color_t lv_buf1[320 * LVGL_BUF_LINES];

extern "C" uint32_t lvgl_tick_get_cb(void) { return millis(); }

static void my_disp_flush(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  // LV_COLOR_16_SWAP の設定に合わせてエンディアンを選択
  tft.writePixels((uint16_t*)&color_p->full, w * h, true /*block*/, LV_COLOR_16_SWAP /*bigEndian*/);
  tft.endWrite();

  lv_disp_flush_ready(disp);
}

static void lvgl_begin(uint16_t hor, uint16_t ver) {
  lv_init();

  static lv_disp_draw_buf_t draw_buf;
  // 2ndバッファをNULLにしてシングルバッファ運用
  lv_disp_draw_buf_init(&draw_buf, lv_buf1, NULL, hor * LVGL_BUF_LINES);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = hor;
  disp_drv.ver_res = ver;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  // =========== UI: Slider + Button デモ ===========
  // スライダーの値表示ラベル
  static lv_obj_t* slider_label;
  static lv_obj_t* slider;
  static lv_obj_t* btn;
  static lv_obj_t* btn_label;

  // スライダーイベント: 値をラベルに反映
  auto slider_event_cb = [](lv_event_t* e) {
    lv_obj_t* s = lv_event_get_target(e);
    int32_t v = lv_slider_get_value(s);
    lv_label_set_text_fmt(slider_label, "Value: %d", (int)v);
  };

  // ボタンイベント: 押したらトグル
  auto btn_event_cb = [](lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
      const char* txt = lv_label_get_text(btn_label);
      lv_label_set_text(btn_label, (strcmp(txt, "ON") == 0) ? "OFF" : "ON");
    }
  };

  // レイアウト: グリッド風に配置
  lv_obj_t* root = lv_scr_act();
  lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(root, 12, 0);
  lv_obj_set_style_pad_gap(root, 12, 0);

  // タイトル
  lv_obj_t* title = lv_label_create(root);
  lv_label_set_text(title, "Slider + Button Demo");
  lv_obj_set_style_text_font(title, LV_FONT_DEFAULT, 0);

  // スライダー
  slider = lv_slider_create(root);
  lv_obj_set_width(slider, hor - 24);
  lv_slider_set_range(slider, 0, 100);
  lv_slider_set_value(slider, 50, LV_ANIM_OFF);
  lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);

  // 値表示ラベル
  slider_label = lv_label_create(root);
  lv_label_set_text(slider_label, "Value: 50");

  // ボタン
  btn = lv_btn_create(root);
  lv_obj_set_size(btn, 120, 50);
  lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, nullptr);
  btn_label = lv_label_create(btn);
  lv_label_set_text(btn_label, "OFF");
  lv_obj_center(btn_label);
  // =========== UI ここまで ===========
}

// SD機能は一旦無効化（タッチ優先のため）

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("Boot: JC2432W328 LVGL demo starting");
  print_mem("boot");

  // TFT用: HSPIにピンを割り当て
  hspi.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);

  tft.init(240, 320);          // ST7789 240x320（ネイティブ）
  tft.setRotation(1);          // 横向き 320x240
  tft.setSPISpeed(80000000);   // 80MHz（不安定なら 60MHz/40MHz に戻す）
  tft.invertDisplay(false);    // 必要に応じて true に
  // tft.setColRowStart(x, y);  // ずれがある場合のみ有効化

  // バックライト
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  // 追加のバックライト制御は不要

  // LVGL開始
  uint16_t W = tft.width();
  uint16_t H = tft.height();
  Serial.printf("Rotation=1 width=%u height=%u\n", W, H);
  lvgl_begin(W, H);
  Serial.printf("LVGL buffer lines=%d, bytes=%u\n", (int)LVGL_BUF_LINES, (unsigned)sizeof(lv_buf1));
  print_mem("after_lvgl");

  // Touch開始（自動探索）
  tp = new CST820(TOUCH_SDA, TOUCH_SCL, TOUCH_RST, TOUCH_INT, I2C_ADDR_CST820);
  tp->begin();
  // 画面にも表示
  lv_obj_t* lbl = lv_label_create(lv_scr_act());
  lv_label_set_text_fmt(lbl, "Touch: SDA=%u SCL=%u addr=0x%02X", (unsigned)TOUCH_SDA, (unsigned)TOUCH_SCL, (unsigned)I2C_ADDR_CST820);
  lv_obj_align(lbl, LV_ALIGN_BOTTOM_LEFT, 4, -4);

  // LVGL input device登録
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = [](lv_indev_drv_t* drv, lv_indev_data_t* data) {
    uint16_t rx, ry; uint8_t g;
    bool pressed = false;
    if (tp) pressed = tp->getTouch(&rx, &ry, &g);
    if (!pressed) {
      data->state = LV_INDEV_STATE_RELEASED;
      return;
    }
    // 座標補正（画面は setRotation(1) 横向き。タッチは縦向き基準）
    // 基本回転: new_x = rawY, new_y = (240-1) - rawX
    uint16_t sx = ry;
    uint16_t sy = (uint16_t)(240 - 1) - rx;
    // 180度ズレている場合の補正
    #if TOUCH_ROTATE_180
      sx = (uint16_t)(tft.width()  - 1) - sx;
      sy = (uint16_t)(tft.height() - 1) - sy;
    #endif
    // 範囲クランプ
    if (sx >= tft.width())  sx = tft.width() - 1;
    if (sy >= tft.height()) sy = tft.height() - 1;
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = sx;
    data->point.y = sy;
  };
  lv_indev_drv_register(&indev_drv);

  // SD (VSPI: SCK=18, MISO=19, MOSI=23, CS=5)
  SPIClass sdSPI(VSPI);
  sdSPI.begin(18, 19, 23, 5);
  bool sd_ok = SD.begin(5, sdSPI, 10000000);
  lv_obj_t* sd_lbl = lv_label_create(lv_scr_act());
  if (sd_ok) {
    // ルートを少し列挙
    File root = SD.open("/");
    int count = 0; String names = "";
    if (root) {
      File f;
      while ((f = root.openNextFile())) {
        if (count < 3) names += String(f.name()) + " ";
        f.close();
        count++;
      }
      root.close();
    }
    Serial.printf("SD OK: files=%d %s\n", count, names.c_str());
    lv_label_set_text_fmt(sd_lbl, "SD: OK CS=5 VSPI, files=%d %s", count, names.length()? ("["+names+"]").c_str():"");

    // 読み書きテスト
    const char* testPath = "/lvgl_sd_test.txt";
    String payload = String("Hello SD @") + String(millis());
    bool wr_ok = false, rd_ok = false; String readBack = "";

    // Write
    File wf = SD.open(testPath, FILE_WRITE);
    if (wf) {
      size_t n = wf.print(payload);
      wf.flush(); wf.close();
      wr_ok = (n == payload.length());
    }

    // Read
    File rf = SD.open(testPath, FILE_READ);
    if (rf) {
      while (rf.available() && readBack.length() < 64) {
        readBack += (char)rf.read();
      }
      rf.close();
      rd_ok = (readBack.length() > 0);
    }
    Serial.printf("SD RW: write=%s read=%s content='%s'\n", wr_ok?"OK":"NG", rd_ok?"OK":"NG", readBack.c_str());

    // 画面に結果ラベル
    lv_obj_t* sd_rw = lv_label_create(lv_scr_act());
    lv_label_set_text_fmt(sd_rw, "RW: %s/%s %s", wr_ok?"OK":"NG", rd_ok?"OK":"NG", rd_ok? readBack.c_str():"");
    lv_obj_align(sd_rw, LV_ALIGN_BOTTOM_RIGHT, -4, -22);
  } else {
    Serial.println("SD NG: Not found (VSPI CS=5)");
    lv_label_set_text(sd_lbl, "SD: Not found");
  }
    lv_obj_align(sd_lbl, LV_ALIGN_BOTTOM_RIGHT, -4, -4);
    print_mem("after_sd");
}

void loop() {
  lv_timer_handler();
  static uint32_t last = 0; uint32_t now = millis();
  if (now - last > 1000) { last = now; Serial.println("HB"); }
  delay(5);
}
