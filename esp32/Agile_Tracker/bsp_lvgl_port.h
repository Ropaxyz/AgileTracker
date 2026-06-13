#ifndef BSP_LVGL_PORT_H
#define BSP_LVGL_PORT_H

#include "./src/port_bsp/i2c_bsp.h"
#include <lvgl.h>

bool bsp_lvgl_lock(int timeout_ms);
void bsp_lvgl_unlock(void);
void bsp_lvgl_init(I2cMasterBus& i2cbus);

void Lcd_SetBacklight(uint8_t brightness);
void Lcd_SetRotation(lv_display_rotation_t rotation);
lv_display_rotation_t Lcd_GetRotation();
lv_display_rotation_t Lcd_NormalizeLandscapeRotation(lv_display_rotation_t rotation);
void Lcd_ToggleLandscape();

#endif // BSP_LVGL_PORT_H