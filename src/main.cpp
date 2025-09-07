// ESP32 + ST7789 (JC2432W328)
// Adafruit_GFX/Adafruit_ST7789 + LVGL 連携最小実装

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <lvgl.h>
#include "CST820.h"

#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1   // RST未接続なら -1
#define TFT_BL   27
#define TFT_SCLK 14
#define TFT_MOSI 13
#define TFT_MISO -1

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

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

static bool i2c_probe_addr(uint8_t sda, uint8_t scl, uint8_t addr) {
  Wire.begin(sda, scl);
  Wire.setClock(400000);
  Wire.beginTransmission(addr);
  int err = Wire.endTransmission(true);
  return err == 0;
}

// 自動探索は使わない（固定ピンで初期化）

// LVGL用ダブルバッファ（行数は40程度）
static lv_color_t lv_buf1[320 * 40];
static lv_color_t lv_buf2[320 * 40];

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
  lv_disp_draw_buf_init(&draw_buf, lv_buf1, lv_buf2, hor * 40);

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
  delay(50);

  // SPIピンを指定（ESP32はデフォルトが SCLK=18 MOSI=23 なので必須）
  SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);

  tft.init(240, 320);          // ST7789 240x320（ネイティブ）
  tft.setRotation(1);          // 横向き 320x240
  tft.setSPISpeed(27000000);   // 27MHz（不安定なら 20000000 へ）
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
}

void loop() {
  lv_timer_handler();
  delay(5);
}
