// LovyanGFX + LVGL test (ESP32 + ST7789 240x320)

#include <Arduino.h>
#include "../include/LGFX_Driver.hpp"
#include <esp_heap_caps.h>
#include <SPI.h>
#include <SD.h>
#include <BluetoothA2DPSink.h>
#include <lvgl.h>
#include "CST820.h"

static LGFX tft;
static BluetoothA2DPSink a2dp;

extern "C" uint32_t lvgl_tick_get_cb(void) { return millis(); }

// LVGL draw buffer（行数控えめ）
#ifndef LV_LINES
#define LV_LINES 20
#endif
static lv_color_t lvbuf1[320 * LV_LINES];

static void print_mem(const char* stage) {
    size_t free8   = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t freeDMA = heap_caps_get_free_size(MALLOC_CAP_DMA);
    size_t freePS  = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    Serial.printf("[MEM %s] DRAM free=%u KB, largest=%u KB, DMA free=%u KB, PSRAM free=%u KB\n",
                  stage,
                  (unsigned)(free8 / 1024),
                  (unsigned)(largest / 1024),
                  (unsigned)(freeDMA / 1024),
                  (unsigned)(freePS / 1024));
}

static void lvgl_flush(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    // 無駄のない経路に統一: LVGL側で LV_COLOR_16_SWAP=1 にしておき、ここではスワップせず送る
    tft.pushImage(area->x1, area->y1, w, h, (uint16_t*)&color_p->full, false /*swapBytes*/);
    lv_disp_flush_ready(disp);
}

void setup() {
    Serial.begin(115200);
    delay(100);

    tft.init();
    tft.setRotation(1);              // landscape 320x240（製品の正位を維持）
    tft.setColorDepth(16);
    pinMode(27, OUTPUT);             // BL 強制点灯
    digitalWrite(27, HIGH);
    tft.setBrightness(255);
    print_mem("boot");

    // LVGL 初期化
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
    print_mem("after_lvgl");

    // UI: タイトル + スライダー + ラベル + ボタン
    lv_obj_t* root = lv_scr_act();
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(root, 12, 0);
    lv_obj_set_style_pad_gap(root, 12, 0);
    lv_obj_set_flex_align(root, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* title = lv_label_create(root);
    lv_label_set_text(title, "LVGL + LovyanGFX");

    lv_obj_t* slider = lv_slider_create(root);
    lv_obj_set_width(slider, lv_pct(80));
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, 50, LV_ANIM_OFF);

    lv_obj_t* val = lv_label_create(root);
    lv_label_set_text(val, "Value: 50");
    lv_obj_add_event_cb(slider, [](lv_event_t* e) {
        lv_obj_t* s = lv_event_get_target(e);
        int v = lv_slider_get_value(s);
        lv_obj_t* lbl = (lv_obj_t*)lv_event_get_user_data(e);
        lv_label_set_text_fmt(lbl, "Value: %d", v);
    }, LV_EVENT_VALUE_CHANGED, val);

    lv_obj_t* btn = lv_btn_create(root);
    lv_obj_set_size(btn, 120, 48);
    lv_obj_t* bl = lv_label_create(btn);
    lv_label_set_text(bl, "OFF");
    lv_obj_center(bl);
    lv_obj_add_event_cb(btn, [](lv_event_t* e) {
        lv_obj_t* b = lv_event_get_target(e);
        lv_obj_t* l = lv_obj_get_child(b, 0);
        const char* t = lv_label_get_text(l);
        lv_label_set_text(l, (strcmp(t, "ON") == 0) ? "OFF" : "ON");
    }, LV_EVENT_CLICKED, NULL);

    // --- Touch indev (CST820 I2C) ---
    // CYD: SDA=33, SCL=32, RST=25, INT=21
    static CST820 tp(33, 32, 25, 21, I2C_ADDR_CST820);
    tp.begin();

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = [](lv_indev_drv_t* drv, lv_indev_data_t* data) {
        static CST820* s_tp = nullptr;
        if (!s_tp) s_tp = (CST820*)drv->user_data;

        uint16_t rx, ry; uint8_t g;
        bool pressed = s_tp->getTouch(&rx, &ry, &g);

        // フィルタ切替マクロ（デフォルトOFF）
        #ifndef TOUCH_FILTER_ENABLE
        #define TOUCH_FILTER_ENABLE 0
        #endif

        #if TOUCH_FILTER_ENABLE
            static bool last_pressed = false;
            static uint8_t press_cnt = 0, release_cnt = 0;
            static uint16_t fx = 0, fy = 0; // filtered
            const uint8_t PRESS_STABLE = 2;
            const uint8_t RELEASE_STABLE = 2;
            const uint16_t DEADZONE = 3; // px

            if (!pressed) {
                release_cnt++;
                press_cnt = 0;
                if (last_pressed && release_cnt >= RELEASE_STABLE) last_pressed = false;
                data->state = last_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
                if (last_pressed) { data->point.x = fx; data->point.y = fy; }
                return;
            }

            uint16_t sx = ry;
            uint16_t sy = (uint16_t)(240 - 1) - rx;
            if (sx >= tft.width())  sx = tft.width() - 1;
            if (sy >= tft.height()) sy = tft.height() - 1;

            if (!last_pressed) { fx = sx; fy = sy; }
            else {
                uint16_t dx = (sx > fx) ? (sx - fx) : (fx - sx);
                uint16_t dy = (sy > fy) ? (sy - fy) : (fy - sy);
                if (dx > DEADZONE || dy > DEADZONE) {
                    fx = (uint16_t)((fx * 3 + sx) / 4);
                    fy = (uint16_t)((fy * 3 + sy) / 4);
                }
            }

            press_cnt++;
            release_cnt = 0;
            if (!last_pressed && press_cnt >= PRESS_STABLE) last_pressed = true;

            data->state = last_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
            data->point.x = fx; data->point.y = fy;
        #else
            if (!pressed) { data->state = LV_INDEV_STATE_RELEASED; return; }
            uint16_t sx = ry;
            uint16_t sy = (uint16_t)(240 - 1) - rx;
            if (sx >= tft.width())  sx = tft.width() - 1;
            if (sy >= tft.height()) sy = tft.height() - 1;
            data->state = LV_INDEV_STATE_PRESSED;
            data->point.x = sx; data->point.y = sy;
        #endif
    };
    indev_drv.user_data = &tp;
    lv_indev_drv_register(&indev_drv);

    // --- A2DP sink init (I2S: LRCK=22, BCK=26, DATA=4) ---
    {
        i2s_pin_config_t pin_cfg = {
            .bck_io_num   = 26,
            .ws_io_num    = 22,
            .data_out_num = 4,
            .data_in_num  = I2S_PIN_NO_CHANGE
        };
        a2dp.set_pin_config(pin_cfg);
        a2dp.set_auto_reconnect(true);
        a2dp.set_volume(90); // 0..100
        const char* dev_name = "CYD A2DP Sink";
        a2dp.start(dev_name);
        Serial.printf("[A2DP] ready as '%s'\n", dev_name);
        print_mem("after_bt");
    }

    // --- SD read/write test (VSPI: SCK=18, MISO=19, MOSI=23, CS=5) ---
    SPIClass sdSPI(VSPI);
    sdSPI.begin(18, 19, 23, 5);
    bool sd_ok = SD.begin(5, sdSPI, 10000000);

    // 右下に結果を表示するラベル（既存UIの配置は維持）
    lv_obj_t* sd_lbl = lv_label_create(lv_scr_act());
    lv_obj_align(sd_lbl, LV_ALIGN_BOTTOM_RIGHT, -4, -4);

    if (sd_ok) {
        // ルートを少し列挙
        File rootDir = SD.open("/");
        int count = 0; String names;
        if (rootDir) {
            for (File f = rootDir.openNextFile(); f; f = rootDir.openNextFile()) {
                if (count < 3) {
                    names += f.name(); names += ' ';
                }
                f.close();
                ++count;
            }
            rootDir.close();
        }

        // RWテスト
        const char* testPath = "/lovgfx_sd_test.txt";
        String payload = String("Hello SD @") + String(millis());
        bool wr_ok = false, rd_ok = false; String readBack;

        File wf = SD.open(testPath, FILE_WRITE);
        if (wf) {
            size_t n = wf.print(payload);
            wf.flush(); wf.close();
            wr_ok = (n == payload.length());
        }
        File rf = SD.open(testPath, FILE_READ);
        if (rf) {
            while (rf.available() && readBack.length() < 64) readBack += (char)rf.read();
            rf.close();
            rd_ok = (readBack.length() > 0);
        }

        Serial.printf("[SD] OK files=%d %s | RW=%s/%s '%s'\n",
                      count, names.c_str(), wr_ok?"OK":"NG", rd_ok?"OK":"NG", readBack.c_str());
        lv_label_set_text_fmt(sd_lbl, "SD: OK CS=5 VSPI files=%d %s\nRW: %s/%s %s",
                              count, names.length()? ("[" + names + "]").c_str() : "",
                              wr_ok?"OK":"NG", rd_ok?"OK":"NG", rd_ok? readBack.c_str(): "");
        print_mem("after_sd");
    } else {
        Serial.println("[SD] Not found (VSPI CS=5)");
        lv_label_set_text(sd_lbl, "SD: Not found");
        print_mem("after_sd");
    }

    // simple color bars
    int w = tft.width();
    int h = tft.height();
    int s = w / 4;
    tft.fillRect(0 * s, 0, s, h, TFT_RED);
    tft.fillRect(1 * s, 0, s, h, TFT_GREEN);
    tft.fillRect(2 * s, 0, s, h, TFT_BLUE);
    tft.fillRect(3 * s, 0, s, h, TFT_WHITE);

    delay(500);
    tft.fillScreen(TFT_BLACK);

    // grid
    for (int x = 0; x < w; x += 16) tft.drawFastVLine(x, 0, h, TFT_DARKGREY);
    for (int y = 0; y < h; y += 16) tft.drawFastHLine(0, y, w, TFT_DARKGREY);

    // shapes
    tft.drawRect(5, 5, w - 10, h - 10, TFT_YELLOW);
    tft.fillCircle(w / 2, h / 2, 40, TFT_CYAN);
    tft.drawCircle(w / 2, h / 2, 60, TFT_MAGENTA);
    tft.drawLine(0, 0, w - 1, h - 1, TFT_WHITE);
    tft.drawLine(0, h - 1, w - 1, 0, TFT_WHITE);

    // text
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.print("LovyanGFX test");
}

void loop() {
    lv_timer_handler();
    delay(5);
}
