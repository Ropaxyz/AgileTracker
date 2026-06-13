#pragma once

#include <stdint.h>

typedef void (*BspButtonHandler)(void);

void bsp_buttons_init();
void bsp_buttons_poll();

void bsp_buttons_on_key_click(BspButtonHandler handler);
void bsp_buttons_on_key_long(BspButtonHandler handler);
void bsp_buttons_on_boot_click(BspButtonHandler handler);
void bsp_buttons_on_boot_long(BspButtonHandler handler);
