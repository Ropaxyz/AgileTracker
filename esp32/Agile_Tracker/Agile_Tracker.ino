#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>

#include "agile_api.h"
#include "agile_config.h"
#include "agile_ui.h"
#include "bsp_buttons.h"
#include "bsp_lvgl_port.h"
#include <lvgl.h>
#include "secrets.h"
#include "./src/port_bsp/axp2101_bsp.h"
#include "./src/port_bsp/i2c_bsp.h"

I2cMasterBus I2cMasterBus_(GPIO_NUM_7, GPIO_NUM_8, I2C_NUM_0);

static TaskHandle_t fetchTaskHandle = nullptr;
static lv_timer_t *uiTickTimer = nullptr;

static void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("Connecting to %s", WIFI_SSID);
  for (int i = 0; i < 60 && WiFi.status() != WL_CONNECTED; ++i) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("WiFi connected: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("WiFi connection failed");
  }
}

static void uiTickTimerCb(lv_timer_t *timer) {
  (void)timer;
  agileUiTick(g_agileStore);
}

static void saveRotation(lv_display_rotation_t rotation) {
  Preferences prefs;
  prefs.begin("agile", false);
  prefs.putUChar("rotation", static_cast<uint8_t>(rotation));
  prefs.end();
}

static void onKeyLong() {
  Lcd_ToggleLandscape();
  agileUiRelayout();
  saveRotation(Lcd_GetRotation());
}

static void fetchTask(void *arg) {
  (void)arg;
  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      if (agileFetchRates(g_agileStore)) {
        agileFetchStandingCharge(g_tariffMeta, g_agileStore);
        agileUiRefresh(g_agileStore);
      } else {
        agileUiRefresh(g_agileStore);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(RATE_POLL_MS));
  }
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("Agile tracker starting");

  Custom_PmicPortInit(&I2cMasterBus_, 0x34);
  bsp_lvgl_init(I2cMasterBus_);

  connectWifi();
  agileInitTime();

  uint8_t brightness = 80;
  uint8_t rotation = LV_DISPLAY_ROTATION_90;
  uint8_t savedRotation = LV_DISPLAY_ROTATION_90;
  Preferences prefs;
  if (prefs.begin("agile", true)) {
    brightness = prefs.getUChar("backlight", 80);
    savedRotation = prefs.getUChar("rotation", LV_DISPLAY_ROTATION_90);
    rotation = savedRotation;
    prefs.end();
  }
  if (brightness < 10) {
    brightness = 10;
  }
  rotation = Lcd_NormalizeLandscapeRotation(static_cast<lv_display_rotation_t>(rotation));
  if (rotation != savedRotation) {
    saveRotation(static_cast<lv_display_rotation_t>(rotation));
  }

  if (bsp_lvgl_lock(0)) {
    Lcd_SetRotation(static_cast<lv_display_rotation_t>(rotation));
    agileUiInit();
    agileUiSetBrightness(brightness);
    uiTickTimer = lv_timer_create(uiTickTimerCb, 1000, nullptr);
    bsp_lvgl_unlock();
  }

  bsp_buttons_init();
  bsp_buttons_on_key_click([]() { agileUiBrightnessStep(10); });
  bsp_buttons_on_boot_click([]() { agileUiBrightnessStep(-10); });
  bsp_buttons_on_key_long(onKeyLong);
  bsp_buttons_on_boot_long(agileUiNextTab);

  if (agileFetchRates(g_agileStore)) {
    agileFetchStandingCharge(g_tariffMeta, g_agileStore);
    agileUiRefresh(g_agileStore);
  }

  // Backlight (incl. auto night-dim) is applied by agileUiSetBrightness above.

  xTaskCreate(fetchTask, "agile_fetch", 12288, nullptr, 1, &fetchTaskHandle);
}

void loop() {
  bsp_buttons_poll();
  if (WiFi.status() != WL_CONNECTED) {
    connectWifi();
  }
  vTaskDelay(pdMS_TO_TICKS(20));
}
