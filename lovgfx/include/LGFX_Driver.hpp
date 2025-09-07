#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// JC2432W328 / ESP32-2432S028R (CYD) ST7789 240x320 専用設定
// 配線（TFT/HSPI）: SCLK=14, MOSI=13, MISO(未使用), CS=15, DC=2, BL=27

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

    // パネルの解像度を設定
    // ST7789はコンストラクタで解像度を指定するタイプです
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

/*  
public:
  LGFX() {
    { // SPIバス
      auto cfg = _bus.config();
      cfg.spi_host    = HSPI_HOST;   // HSPI
      cfg.spi_mode    = 3;           // ST7789で安定する個体が多いMode3
      cfg.freq_write  = 10000000;    // まず10MHzで必ず出るか確認
      cfg.freq_read   = 16000000;
      cfg.spi_3wire   = false;
      cfg.use_lock    = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk    = 14;
      cfg.pin_mosi    = 13;
      cfg.pin_miso    = -1;          // 読み出し未使用
      cfg.pin_dc      = 2;           // DC
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }

    { // パネル
      auto cfg = _panel.config();
      cfg.pin_cs            = 15;
      cfg.pin_rst           = -1;
      cfg.pin_busy          = -1;
      cfg.panel_width       = 240;
      cfg.panel_height      = 320;
      cfg.memory_width      = 240;
      cfg.memory_height     = 320;
      cfg.offset_x          = 0;
      cfg.offset_y          = 0;
      cfg.offset_rotation   = 0;
      cfg.dummy_read_pixel  = 8;
      cfg.dummy_read_bits   = 1;
      cfg.readable          = false;
      cfg.invert            = false;   // 必要なら true
      cfg.rgb_order         = false;   // RGB（LVGL側はスワップなしのためRGBに合わせる）
      cfg.dlen_16bit        = false;   // 8bit転送に落として確実化
      cfg.bus_shared        = false;
      _panel.config(cfg);
    }

    { // バックライト（BL=27）
      auto cfg = _light.config();
      cfg.pin_bl      = 27;
      cfg.invert      = false;
      cfg.freq        = 12000;
      cfg.pwm_channel = 1;
      _light.config(cfg);
      _panel.setLight(&_light);
    }

    setPanel(&_panel);
  }
};
*/
