#include "bsp_orientation.h"
#include "bsp_lvgl_port.h"
#include "./src/port_bsp/qmi8658.h"

#include <Arduino.h>
#include <math.h>

static qmi8658_dev_t imu = {};
static bool imu_ok = false;
static lv_display_rotation_t stable_rot = LV_DISPLAY_ROTATION_90;
static uint8_t stable_count = 0;
static uint32_t manual_override_until = 0;
static uint32_t last_poll_ms = 0;

void bsp_orientation_init(I2cMasterBus &i2cbus) {
  esp_err_t err = qmi8658_init(&imu, i2cbus.Get_I2cBusHandle(), QMI8658_ADDRESS_HIGH);
  if (err != ESP_OK) {
    err = qmi8658_init(&imu, i2cbus.Get_I2cBusHandle(), QMI8658_ADDRESS_LOW);
  }
  if (err != ESP_OK) {
    return;
  }

  qmi8658_set_accel_odr(&imu, QMI8658_ACCEL_ODR_31_25HZ);
  qmi8658_enable_sensors(&imu, QMI8658_ENABLE_ACCEL);
  qmi8658_set_accel_unit_mg(&imu, true);
  imu_ok = true;
  stable_rot = Lcd_GetRotation();
}

bool bsp_orientation_available() {
  return imu_ok;
}

void bsp_orientation_pause(uint32_t ms) {
  manual_override_until = millis() + ms;
  stable_count = 0;
}

static lv_display_rotation_t rotationFromGravity(float ax, float ay) {
  const float abs_ax = fabsf(ax);
  const float abs_ay = fabsf(ay);

  if (abs_ax < 350.0f && abs_ay < 350.0f) {
    return Lcd_GetRotation();
  }

  if (abs_ax > abs_ay * 1.15f) {
    return (ax > 0.0f) ? LV_DISPLAY_ROTATION_90 : LV_DISPLAY_ROTATION_270;
  }
  if (abs_ay > abs_ax * 1.15f) {
    return (ay > 0.0f) ? LV_DISPLAY_ROTATION_0 : LV_DISPLAY_ROTATION_180;
  }
  return Lcd_GetRotation();
}

bool bsp_orientation_poll() {
  if (!imu_ok) {
    return false;
  }

  const uint32_t now = millis();
  if (now - last_poll_ms < 400) {
    return false;
  }
  last_poll_ms = now;

  if (now < manual_override_until) {
    return false;
  }

  float ax = 0.0f;
  float ay = 0.0f;
  float az = 0.0f;
  if (qmi8658_read_accel(&imu, &ax, &ay, &az) != ESP_OK) {
    return false;
  }

  const lv_display_rotation_t want = rotationFromGravity(ax, ay);
  if (want == stable_rot) {
    if (stable_count < 3) {
      stable_count++;
    }
  } else {
    stable_rot = want;
    stable_count = 0;
    return false;
  }

  if (stable_count < 3 || want == Lcd_GetRotation()) {
    return false;
  }

  if (!bsp_lvgl_lock(20)) {
    return false;
  }
  Lcd_SetRotation(want);
  bsp_lvgl_unlock();
  return true;
}
