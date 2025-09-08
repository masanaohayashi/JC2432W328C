#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// JC2432W328 / ESP32-2432S028R (CYD) ST7789 240x320 専用設定
// 配線（TFT/HSPI）: SCLK=14, MOSI=13, MISO=12(未使用), CS=15, DC=2, BL=27

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789  _panel;   // 240x320 ST7789
  lgfx::Bus_SPI       _bus;     // HSPI
  lgfx::Light_PWM     _light;   // Backlight

public:
  LGFX(void) {
    // SPIバス設定
    auto cfg = _bus.config();
    cfg.freq_write  = 80000000;
    cfg.pin_sclk = 14;
    cfg.pin_mosi = 13;
    cfg.pin_miso = 12;
    cfg.pin_dc   = 2;
    _bus.config(cfg);
    _panel.setBus(&_bus);

    // ディスプレイ設定
    auto panel_cfg = _panel.config();
    panel_cfg.pin_cs = 15;
    panel_cfg.pin_rst = -1;
    panel_cfg.offset_x = 0;
    panel_cfg.offset_y = 0;
    panel_cfg.memory_width = 240;
    panel_cfg.memory_height = 320;
    panel_cfg.panel_width = 240;
    panel_cfg.panel_height = 320;
    _panel.config(panel_cfg);

    // バックライト設定
    auto light_cfg = _light.config();
    light_cfg.pin_bl = 27;
    _light.config(light_cfg);
    _panel.setLight(&_light);

    setPanel(&_panel);
  }
};

