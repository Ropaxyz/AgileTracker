#include "bsp_buttons.h"

#include <Arduino.h>
#include <driver/gpio.h>

#include "user_config.h"

static BspButtonHandler keyClick = nullptr;
static BspButtonHandler keyLong = nullptr;
static BspButtonHandler bootClick = nullptr;
static BspButtonHandler bootLong = nullptr;

static bool bootLast = true;
static bool keyLast = true;
static uint32_t bootDownMs = 0;
static uint32_t keyDownMs = 0;
static bool bootLongFired = false;
static bool keyLongFired = false;

static const uint32_t DEBOUNCE_MS = 35;
static const uint32_t LONG_PRESS_MS = 650;

static void setupInput(gpio_num_t pin) {
  gpio_config_t cfg = {};
  cfg.intr_type = GPIO_INTR_DISABLE;
  cfg.mode = GPIO_MODE_INPUT;
  cfg.pin_bit_mask = (1ULL << pin);
  cfg.pull_up_en = GPIO_PULLUP_ENABLE;
  cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
  gpio_config(&cfg);
}

static bool isPressed(gpio_num_t pin) {
  return gpio_get_level(pin) == 0;
}

static void pollButton(gpio_num_t pin, bool *last, uint32_t *downMs, bool *longFired,
                       BspButtonHandler onClick, BspButtonHandler onLong) {
  const bool pressed = isPressed(pin);
  const uint32_t now = millis();

  if (pressed && !*last) {
    *downMs = now;
    *longFired = false;
  }

  if (pressed && !*longFired && onLong != nullptr && (now - *downMs) >= LONG_PRESS_MS) {
    *longFired = true;
    onLong();
  }

  if (!pressed && *last && (now - *downMs) >= DEBOUNCE_MS && !*longFired && onClick != nullptr) {
    onClick();
  }

  *last = pressed;
}

void bsp_buttons_init() {
  setupInput(BSP_BTN_BOOT_GPIO);
  setupInput(BSP_BTN_KEY_GPIO);
  bootLast = !isPressed(BSP_BTN_BOOT_GPIO);
  keyLast = !isPressed(BSP_BTN_KEY_GPIO);
}

void bsp_buttons_poll() {
  pollButton(BSP_BTN_BOOT_GPIO, &bootLast, &bootDownMs, &bootLongFired, bootClick, bootLong);
  pollButton(BSP_BTN_KEY_GPIO, &keyLast, &keyDownMs, &keyLongFired, keyClick, keyLong);
}

void bsp_buttons_on_key_click(BspButtonHandler handler) {
  keyClick = handler;
}

void bsp_buttons_on_key_long(BspButtonHandler handler) {
  keyLong = handler;
}

void bsp_buttons_on_boot_click(BspButtonHandler handler) {
  bootClick = handler;
}

void bsp_buttons_on_boot_long(BspButtonHandler handler) {
  bootLong = handler;
}
