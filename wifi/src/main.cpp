// WiFi SSID選択＆パスワード入力 → 接続

#include <Arduino.h>
#include "../include/LGFX_Driver.hpp"
#include <lvgl.h>
#include <WiFi.h>
#include "CST820.h"

static LGFX tft;

extern "C" uint32_t lvgl_tick_get_cb(void) { return millis(); }

#ifndef LV_LINES
#define LV_LINES 20
#endif
static lv_color_t lvbuf1[320 * LV_LINES];

static void lvgl_flush(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  // color_p は先頭画素へのポインタなので、そのまま16bit配列として渡す
  tft.pushImage(area->x1, area->y1, w, h, reinterpret_cast<const uint16_t*>(color_p), false /*swapBytes*/);
  lv_disp_flush_ready(disp);
}

// UI要素
static lv_obj_t* status_lbl = nullptr;
static lv_obj_t* list_box = nullptr;   // SSIDリスト

// 接続チェック用タイマー
static lv_timer_t* conn_timer = nullptr;
static String pending_ssid;

static void set_status(const char* fmt, ...) {
  if (!status_lbl) return;
  char buf[160];
  va_list ap; va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  lv_label_set_text(status_lbl, buf);
}

static void populate_ssid_list();

// パスワード入力ダイアログ
static void open_password_dialog(const char* ssid) {
  lv_obj_t* modal = lv_obj_create(lv_scr_act());
  lv_obj_set_size(modal, lv_pct(90), lv_pct(80));
  lv_obj_center(modal);
  lv_obj_set_style_pad_all(modal, 10, 0);
  lv_obj_set_flex_flow(modal, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_gap(modal, 8, 0);

  lv_obj_t* title = lv_label_create(modal);
  lv_label_set_text_fmt(title, "Connect to: %s", ssid);

  lv_obj_t* ta = lv_textarea_create(modal);
  lv_textarea_set_password_mode(ta, true);
  lv_textarea_set_one_line(ta, true);
  lv_obj_set_width(ta, lv_pct(100));
  lv_textarea_set_placeholder_text(ta, "Password");

  // キーボード
  lv_obj_t* kb = lv_keyboard_create(modal);
  lv_keyboard_set_textarea(kb, ta);

  // ボタン行
  lv_obj_t* row = lv_obj_create(modal);
  lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_gap(row, 8, 0);

  lv_obj_t* btn_ok = lv_btn_create(row);
  lv_obj_t* lbl_ok = lv_label_create(btn_ok);
  lv_label_set_text(lbl_ok, "Connect");
  lv_obj_center(lbl_ok);

  lv_obj_t* btn_cancel = lv_btn_create(row);
  lv_obj_t* lbl_ca = lv_label_create(btn_cancel);
  lv_label_set_text(lbl_ca, "Cancel");
  lv_obj_center(lbl_ca);

  // キャンセル
  lv_obj_add_event_cb(btn_cancel, [](lv_event_t* e){
    lv_obj_t* modal = lv_obj_get_parent(lv_event_get_target(e));
    modal = lv_obj_get_parent(modal); // row -> modal
    lv_obj_del(modal);
  }, LV_EVENT_CLICKED, nullptr);

  // 接続
  lv_obj_add_event_cb(btn_ok, [](lv_event_t* e){
    lv_obj_t* btn = lv_event_get_target(e);
    lv_obj_t* row = lv_obj_get_parent(btn);
    lv_obj_t* modal = lv_obj_get_parent(row);
    lv_obj_t* kb = lv_obj_get_child(modal, 2); // title=0, ta=1, kb=2, row=3
    lv_obj_t* ta = lv_obj_get_child(modal, 1);
    const char* ssid = lv_label_get_text(lv_obj_get_child(modal, 0)) + strlen("Connect to: ");
    const char* pass = lv_textarea_get_text(ta);

    // UI更新
    lv_obj_add_state(btn, LV_STATE_DISABLED);
    lv_obj_add_state(row, LV_STATE_DISABLED);
    set_status("Connecting to: %s ...", ssid);

    // 実接続
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true, true);
    delay(100);
    WiFi.begin(ssid, pass);

    pending_ssid = ssid;

    if (conn_timer) { lv_timer_del(conn_timer); conn_timer = nullptr; }
    conn_timer = lv_timer_create([](lv_timer_t* t){
      wl_status_t st = WiFi.status();
      if (st == WL_CONNECTED) {
        IPAddress ip = WiFi.localIP();
        set_status("Connected: %s / IP: %d.%d.%d.%d", pending_ssid.c_str(), ip[0], ip[1], ip[2], ip[3]);
        if (conn_timer) { lv_timer_del(conn_timer); conn_timer = nullptr; }
      } else if (st == WL_CONNECT_FAILED) {
        set_status("Failed: %s (auth error)", pending_ssid.c_str());
        if (conn_timer) { lv_timer_del(conn_timer); conn_timer = nullptr; }
      } else {
        static uint16_t cnt = 0;
        if (++cnt > 60) { // 約30秒
          set_status("Timeout: %s", pending_ssid.c_str());
          if (conn_timer) { lv_timer_del(conn_timer); conn_timer = nullptr; }
        }
      }
    }, 500, nullptr);

    // ダイアログを閉じる
    lv_obj_del(modal);
  }, LV_EVENT_CLICKED, nullptr);
}

// SSIDリストを作成
static void populate_ssid_list() {
  if (!list_box) return;
  lv_obj_clean(list_box);

  set_status("Scanning...");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(100);
  int n = WiFi.scanNetworks(/*async=*/false, /*hidden=*/true);
  if (n <= 0) {
    set_status("No networks found");
    return;
  }
  set_status("Found %d network(s)", n);

  // 強度順に既に整列している想定
  const int MAX_ITEMS = 15; // 生成オブジェクト数を抑制してメモリ枯渇を回避
  for (int i = 0; i < n && i < MAX_ITEMS; ++i) {
    String ssid = WiFi.SSID(i);
    int32_t rssi = WiFi.RSSI(i);
    bool enc = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);

    lv_obj_t* btn = lv_btn_create(list_box);
    lv_obj_set_width(btn, lv_pct(100));
    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text_fmt(lbl, "%s  (%ddBm)%s", ssid.c_str(), (int)rssi, enc?" [secured]":"");
    lv_obj_center(lbl);

    // クリックでパスワード入力へ
    lv_obj_add_event_cb(btn, [](lv_event_t* e){
      lv_obj_t* btn = lv_event_get_target(e);
      lv_obj_t* lbl = lv_obj_get_child(btn, 0);
      const char* text = lv_label_get_text(lbl);
      // 表記からSSID部のみを抽出（最後の2スペース前まで）
      String s(text);
      int p = s.indexOf("  (");
      String ssid = (p > 0) ? s.substring(0, p) : s;
      open_password_dialog(ssid.c_str());
    }, LV_EVENT_CLICKED, nullptr);
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);

  tft.init();
  tft.setRotation(1);              // landscape 320x240
  tft.setColorDepth(16);
  pinMode(27, OUTPUT);             // BL
  digitalWrite(27, HIGH);
  tft.setBrightness(255);

  // LVGL初期化
  lv_init();
  static lv_disp_draw_buf_t draw_buf;
  lv_disp_draw_buf_init(&draw_buf, lvbuf1, NULL, 320 * LV_LINES);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = tft.width();
  disp_drv.ver_res = tft.height();
  disp_drv.flush_cb = lvgl_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  // タッチ（CST820）: SDA=33, SCL=32, RST=25, INT=21
  static CST820 tp(33, 32, 25, 21, I2C_ADDR_CST820);
  tp.begin();
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = [](lv_indev_drv_t* drv, lv_indev_data_t* data){
    static CST820* s_tp = nullptr;
    if (!s_tp) s_tp = (CST820*)drv->user_data;
    uint16_t rx, ry; uint8_t g;
    bool pressed = s_tp->getTouch(&rx, &ry, &g);
    if (!pressed) { data->state = LV_INDEV_STATE_RELEASED; return; }
    uint16_t sx = ry;
    uint16_t sy = (uint16_t)(240 - 1) - rx;
    if (sx >= tft.width())  sx = tft.width() - 1;
    if (sy >= tft.height()) sy = tft.height() - 1;
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = sx; data->point.y = sy;
  };
  indev_drv.user_data = &tp;
  lv_indev_drv_register(&indev_drv);

  // ルートUI
  lv_obj_t* root = lv_scr_act();
  lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(root, 10, 0);
  lv_obj_set_style_pad_gap(root, 8, 0);

  lv_obj_t* title = lv_label_create(root);
  lv_label_set_text(title, "WiFi Setup");

  // ステータス
  status_lbl = lv_label_create(root);
  lv_label_set_text(status_lbl, "");

  // ボタン行
  lv_obj_t* row = lv_obj_create(root);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_style_pad_gap(row, 8, 0);

  lv_obj_t* rescan = lv_btn_create(row);
  lv_obj_t* rl = lv_label_create(rescan);
  lv_label_set_text(rl, "Rescan");
  lv_obj_center(rl);
  lv_obj_add_event_cb(rescan, [](lv_event_t* e){ populate_ssid_list(); }, LV_EVENT_CLICKED, nullptr);

  // SSIDリスト
  list_box = lv_obj_create(root);
  lv_obj_set_size(list_box, lv_pct(100), lv_pct(100));
  lv_obj_set_flex_flow(list_box, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_gap(list_box, 6, 0);
  lv_obj_set_scroll_dir(list_box, LV_DIR_VER);

  // 初回スキャン
  populate_ssid_list();
}

void loop() {
  lv_timer_handler();
  delay(5);
}
