/* Minimal LVGL v8 config for ESP32 + ST7789 (240x320) */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   CORE FEATURES
 *====================*/

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 1
#define LV_COLOR_SCREEN_TRANSP 0

#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (48U * 1024U)

#define LV_DISP_DEF_REFR_PERIOD 30

#define LV_TICK_CUSTOM 1
#if LV_TICK_CUSTOM
  #define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
  extern uint32_t lvgl_tick_get_cb(void);
  #define LV_TICK_CUSTOM_SYS_TIME_EXPR (lvgl_tick_get_cb())
#endif

#define LV_USE_LOG 0

/*====================
   HAL DRIVERS
 *====================*/

#define LV_USE_DRAW_SW 1

/*====================
   EXTRA MODULES
 *====================*/

#define LV_USE_DEMO_WIDGETS 1
#define LV_USE_DEMO_BENCHMARK 0
#define LV_USE_DEMO_MUSIC 0

#endif /* LV_CONF_H */
