// Project-specific TFT_eSPI setup for JC2432W328 (ST7789 240x320)

#ifndef USER_SETUP_LOADED
#define USER_SETUP_LOADED

// Driver selection
// ST7789ドライバ
#define ST7789_DRIVER

// Panel resolution (portrait base)
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// Color order and inversion (per sample configs in repo)
// 色順と反転は実機により差異あり
// landscapeで色ズレ・右側ノイズ対策としてRGB+INVERSION_ONを試す
#define TFT_RGB_ORDER TFT_BGR
#define TFT_INVERSION_OFF

// 一部ST7789はSPI Mode3で安定
#define TFT_SPI_MODE SPI_MODE3

// SPI pins (per JC2432W328 examples)
#define TFT_MISO  -1
#define TFT_MOSI  13
#define TFT_SCLK  14
#define TFT_CS    15
#define TFT_DC     2
#define TFT_RST   -1   // RST wired to EN or 3V3 on board

// Backlight (if available on the board)
#define TFT_BL    27
#define TFT_BACKLIGHT_ON HIGH

// SPI frequencies
#define SPI_FREQUENCY  40000000
#define SPI_READ_FREQUENCY 20000000

// Fonts (enable minimal set)
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF

#endif // USER_SETUP_LOADED
