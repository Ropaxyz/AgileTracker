#ifndef BSP_ORIENTATION_H
#define BSP_ORIENTATION_H

#include "./src/port_bsp/i2c_bsp.h"
#include <lvgl.h>

void bsp_orientation_init(I2cMasterBus &i2cbus);
bool bsp_orientation_available();
void bsp_orientation_pause(uint32_t ms);
bool bsp_orientation_poll();

#endif
