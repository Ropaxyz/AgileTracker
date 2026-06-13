#include "agile_ui.h"

#include <stdio.h>
#include <string.h>

#include "agile_api.h"
#include "agile_config.h"
#include "bsp_lvgl_port.h"
#include "user_config.h"
#include "./src/port_bsp/axp2101_bsp.h"
#include <Preferences.h>
#include <lvgl.h>

#define OCT_NAVY_BG      0x0A1520
#define OCT_NAVY_PANEL   0x101E2C
#define OCT_NAVY_CARD    0x142433
#define OCT_NAVY_TAB     0x0E1A28
#define OCT_PURPLE       0x6950FF
#define OCT_CYAN         0x1FC0FF
#define OCT_PINK         0xFF71CF
#define OCT_GREEN        0x00FFA3
#define OCT_RED          0xFF5D5D
#define OCT_AMBER        0xFFB020
#define OCT_TEXT         0xFFFFFF
#define OCT_TEXT_MUTED   0x9BB4C8
#define OCT_TEXT_DIM     0x5A7085

#define HEADER_H         52
#define SCREEN_W         BSP_LCD_H_RES
#define SCREEN_H         BSP_LCD_V_RES
#define HEADER_PAD_H     BSP_ROUND_HEADER_PAD_H
#define TAB_PAD_H        BSP_ROUND_CONTENT_PAD_H
#define TAB_PAD_V        BSP_ROUND_CONTENT_PAD_V

static lv_obj_t *safe_root = nullptr;
static lv_obj_t *tabview = nullptr;
static lv_obj_t *tab_now = nullptr;
static lv_obj_t *header_bar = nullptr;
static lv_obj_t *lbl_brand = nullptr;
static lv_obj_t *lbl_region = nullptr;
static lv_obj_t *lbl_clock = nullptr;
static lv_obj_t *lbl_updated = nullptr;
static lv_obj_t *chip_battery = nullptr;
static lv_obj_t *lbl_battery = nullptr;

static lv_obj_t *price_card = nullptr;
static lv_obj_t *lbl_now_price = nullptr;
static lv_obj_t *lbl_now_unit = nullptr;
static lv_obj_t *price_cluster = nullptr;
static lv_obj_t *lbl_now_alert = nullptr;
static lv_obj_t *panel_run_score = nullptr;
static lv_obj_t *lbl_run_score = nullptr;
static lv_obj_t *stats_card = nullptr;
static lv_obj_t *lbl_now_until = nullptr;
static lv_obj_t *lbl_now_next_price = nullptr;
static lv_obj_t *lbl_now_next_when = nullptr;
static lv_obj_t *lbl_now_range = nullptr;
static lv_obj_t *lbl_now_median = nullptr;
static lv_obj_t *lbl_now_cheap = nullptr;
static lv_obj_t *lbl_now_fixed = nullptr;

static lv_obj_t *lbl_today_title = nullptr;
static lv_obj_t *lbl_today_meta = nullptr;
static lv_obj_t *lbl_chart_now = nullptr;
static lv_obj_t *chart_today = nullptr;
static lv_chart_series_t *series_today = nullptr;
static lv_chart_cursor_t *cursor_today = nullptr;

static lv_obj_t *tab_tomorrow = nullptr;
static lv_obj_t *lbl_tomorrow_title = nullptr;
static lv_obj_t *card_tomorrow_summary = nullptr;
static lv_obj_t *lbl_tomorrow_summary = nullptr;
static lv_obj_t *card_tomorrow_window = nullptr;
static lv_obj_t *lbl_tomorrow_window = nullptr;
static lv_obj_t *card_tomorrow_cheapest = nullptr;
static lv_obj_t *lbl_tomorrow_cheapest = nullptr;
static lv_obj_t *card_tomorrow_peak = nullptr;
static lv_obj_t *lbl_tomorrow_peak = nullptr;
static lv_obj_t *lbl_tomorrow_fixed = nullptr;

static lv_obj_t *lbl_brightness = nullptr;
static lv_obj_t *slider_brightness = nullptr;
static lv_obj_t *switch_night = nullptr;
static lv_obj_t *lbl_night = nullptr;
static lv_obj_t *flash_overlay = nullptr;

// Auto day/night brightness: dim overnight, restore in the morning.
#define NIGHT_START_MIN  (22 * 60)        // 22:00
#define DAY_START_MIN    (6 * 60 + 30)    // 06:30
#define NIGHT_BRIGHTNESS 15               // backlight % during the night window

static uint8_t dayBrightness = 80;        // the normal (daytime) level the user picks
static bool nightModeEnabled = true;      // auto dim on/off
static int nightPhaseApplied = -1;        // -1 unknown, 0 day applied, 1 night applied

static float chartBarRates[48] = {};
static float chartBarMin = 0.0f;
static float chartBarMax = 0.0f;
static size_t chartBarCount = 0;
static int chartCurrentIdx = -1;

static bool negativeFxActive = false;
static bool wasNegative = false;
static int cachedBatteryPct = -1;
static time_t lastBatteryReadUtc = 0;

static void borderPulseAnim(void *var, int32_t v) {
  lv_obj_set_style_border_opa((lv_obj_t *)var, (lv_opa_t)v, 0);
}

static void alertBlinkAnim(void *var, int32_t v) {
  lv_obj_set_style_text_opa((lv_obj_t *)var, (lv_opa_t)v, 0);
}

static void stopNegativeFx() {
  if (!negativeFxActive) {
    return;
  }
  negativeFxActive = false;

  lv_anim_delete(price_card, nullptr);
  lv_anim_delete(lbl_now_alert, nullptr);

  lv_obj_add_flag(lbl_now_alert, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_border_width(price_card, 1, 0);
  lv_obj_set_style_border_color(price_card, lv_color_hex(OCT_PURPLE), 0);
  lv_obj_set_style_border_opa(price_card, LV_OPA_30, 0);
  lv_obj_set_style_bg_color(price_card, lv_color_hex(OCT_NAVY_CARD), 0);
}

static void startNegativeFx() {
  if (negativeFxActive) {
    return;
  }
  negativeFxActive = true;

  lv_label_set_text(lbl_now_alert, "PAID TO USE");
  lv_obj_remove_flag(lbl_now_alert, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_text_color(lbl_now_alert, lv_color_hex(OCT_GREEN), 0);
  lv_obj_set_style_text_opa(lbl_now_alert, LV_OPA_COVER, 0);

  lv_obj_set_style_text_color(lbl_now_price, lv_color_hex(OCT_GREEN), 0);
  lv_obj_set_style_border_color(price_card, lv_color_hex(OCT_GREEN), 0);
  lv_obj_set_style_border_width(price_card, 2, 0);
  lv_obj_set_style_bg_color(price_card, lv_color_hex(0x0A2820), 0);

  lv_anim_t anim;
  lv_anim_init(&anim);
  lv_anim_set_var(&anim, price_card);
  lv_anim_set_exec_cb(&anim, borderPulseAnim);
  lv_anim_set_values(&anim, LV_OPA_50, LV_OPA_COVER);
  lv_anim_set_time(&anim, 700);
  lv_anim_set_playback_time(&anim, 700);
  lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
  lv_anim_start(&anim);

  lv_anim_init(&anim);
  lv_anim_set_var(&anim, lbl_now_alert);
  lv_anim_set_exec_cb(&anim, alertBlinkAnim);
  lv_anim_set_values(&anim, LV_OPA_40, LV_OPA_COVER);
  lv_anim_set_time(&anim, 450);
  lv_anim_set_playback_time(&anim, 450);
  lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
  lv_anim_start(&anim);
}

static void updateNegativeFx(float pence) {
  if (pence < 0.0f) {
    startNegativeFx();
  } else {
    stopNegativeFx();
  }
}

static void formatLocalTime(time_t utc, char *buf, size_t len) {
  struct tm tm = {};
  localtime_r(&utc, &tm);
  strftime(buf, len, "%H:%M", &tm);
}

static void formatPence(float pence, char *buf, size_t len) {
  snprintf(buf, len, "%.1fp", pence);
}

static lv_color_t priceColor(float price, float minVal, float maxVal) {
  if (maxVal <= minVal) {
    return lv_color_hex(OCT_TEXT);
  }
  const float t = (price - minVal) / (maxVal - minVal);
  if (t < 0.33f) {
    return lv_color_hex(OCT_GREEN);
  }
  if (t < 0.66f) {
    return lv_color_hex(OCT_CYAN);
  }
  return lv_color_hex(OCT_PINK);
}

static void flashOpacityAnim(void *var, int32_t v) {
  lv_obj_set_style_bg_opa((lv_obj_t *)var, (lv_opa_t)v, 0);
}

static void flashAnimReady(lv_anim_t *anim) {
  if (flash_overlay != nullptr) {
    lv_obj_add_flag(flash_overlay, LV_OBJ_FLAG_HIDDEN);
  }
  (void)anim;
}

static void triggerNegativeFlash() {
  if (flash_overlay == nullptr) {
    return;
  }
  lv_obj_remove_flag(flash_overlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(flash_overlay);
  lv_anim_delete(flash_overlay, nullptr);
  lv_obj_set_style_bg_opa(flash_overlay, LV_OPA_70, 0);

  lv_anim_t anim;
  lv_anim_init(&anim);
  lv_anim_set_var(&anim, flash_overlay);
  lv_anim_set_exec_cb(&anim, flashOpacityAnim);
  lv_anim_set_values(&anim, LV_OPA_70, LV_OPA_TRANSP);
  lv_anim_set_time(&anim, 600);
  lv_anim_set_ready_cb(&anim, flashAnimReady);
  lv_anim_start(&anim);
}

static void checkNegativeEdge(float pence) {
  const bool isNegative = pence < 0.0f;
  if (!wasNegative && isNegative) {
    triggerNegativeFlash();
  }
  wasNegative = isNegative;
}

static void chartDrawCb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_DRAW_TASK_ADDED) {
    return;
  }

  lv_draw_task_t *draw_task = lv_event_get_draw_task(e);
  if (draw_task == nullptr || lv_draw_task_get_type(draw_task) != LV_DRAW_TASK_TYPE_FILL) {
    return;
  }

  lv_draw_fill_dsc_t *fill_dsc = lv_draw_task_get_fill_dsc(draw_task);
  if (fill_dsc == nullptr || fill_dsc->base.part != LV_PART_ITEMS) {
    return;
  }

  if (lv_chart_get_type((lv_obj_t *)lv_event_get_target(e)) != LV_CHART_TYPE_BAR) {
    return;
  }

  const uint32_t barIdx = fill_dsc->base.id2;
  if (barIdx >= chartBarCount) {
    return;
  }

  lv_color_t barColor = priceColor(chartBarRates[barIdx], chartBarMin, chartBarMax);
  if (static_cast<int>(barIdx) == chartCurrentIdx) {
    barColor = lv_color_mix(lv_color_hex(OCT_TEXT), barColor, 200);
  }
  fill_dsc->color = barColor;
}

static void updateRunScore(float pence, float median) {
  if (panel_run_score == nullptr || lbl_run_score == nullptr) {
    return;
  }

  const char *text = "WAIT";
  uint32_t bgColor = 0x1E1030;
  uint32_t textColor = OCT_PINK;
  uint32_t borderColor = OCT_PINK;

  if (pence < 0.0f) {
    text = "RUN NOW";
    bgColor = 0x0A3828;
    textColor = OCT_GREEN;
    borderColor = OCT_GREEN;
  } else if (median > 0.0f && pence < median) {
    text = "GOOD";
    bgColor = 0x0A2820;
    textColor = OCT_GREEN;
    borderColor = OCT_GREEN;
  } else if (median > 0.0f && pence <= median * 1.15f) {
    text = "OK";
    bgColor = 0x102030;
    textColor = OCT_CYAN;
    borderColor = OCT_CYAN;
  }

  lv_label_set_text(lbl_run_score, text);
  lv_obj_set_style_bg_color(panel_run_score, lv_color_hex(bgColor), 0);
  lv_obj_set_style_border_color(panel_run_score, lv_color_hex(borderColor), 0);
  lv_obj_set_style_text_color(lbl_run_score, lv_color_hex(textColor), 0);
}

static void updateStandingLabels() {
  char buf[48];
  if (g_tariffMeta.fetchOk) {
    snprintf(buf, sizeof(buf), "Standing charge %.1fp/day",
             g_tariffMeta.standingChargePencePerDay);
  } else {
    snprintf(buf, sizeof(buf), "Standing charge: --");
  }
  if (lbl_now_fixed != nullptr) {
    lv_label_set_text(lbl_now_fixed, buf);
  }
  if (lbl_tomorrow_fixed != nullptr) {
    lv_label_set_text(lbl_tomorrow_fixed, buf);
  }
}

static void readBatteryCached() {
  const time_t nowUtc = time(nullptr);
  if (nowUtc - lastBatteryReadUtc >= 30 || cachedBatteryPct < 0) {
    cachedBatteryPct = Axp2101_GetBatteryPercent();
    lastBatteryReadUtc = nowUtc;
  }
}

static void updateHeader(const AgileStore &store, bool showError) {
  char buf[48];
  const time_t nowUtc = time(nullptr);
  formatLocalTime(nowUtc, buf, sizeof(buf));
  lv_label_set_text(lbl_clock, buf);

  if (showError) {
    snprintf(buf, sizeof(buf), "Error: %s", store.lastError);
    lv_label_set_text(lbl_updated, buf);
    lv_obj_set_style_text_color(lbl_updated, lv_color_hex(OCT_RED), 0);
    lv_obj_add_flag(chip_battery, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  lv_obj_set_style_text_color(lbl_updated, lv_color_hex(OCT_TEXT_DIM), 0);
  if (store.fetchOk) {
    char timeBuf[16];
    formatLocalTime(store.fetchedAtUtc, timeBuf, sizeof(timeBuf));
    snprintf(buf, sizeof(buf), "Rates %s", timeBuf);
    lv_label_set_text(lbl_updated, buf);
  } else {
    lv_label_set_text(lbl_updated, "Waiting for rates...");
  }

  readBatteryCached();
  if (cachedBatteryPct >= 0) {
    lv_obj_remove_flag(chip_battery, LV_OBJ_FLAG_HIDDEN);
    snprintf(buf, sizeof(buf), "%d%%", cachedBatteryPct);
    lv_label_set_text(lbl_battery, buf);
    if (cachedBatteryPct <= 10) {
      lv_obj_set_style_bg_color(chip_battery, lv_color_hex(0x3A1018), 0);
      lv_obj_set_style_border_color(chip_battery, lv_color_hex(OCT_RED), 0);
      lv_obj_set_style_text_color(lbl_battery, lv_color_hex(OCT_RED), 0);
    } else if (cachedBatteryPct <= 20) {
      lv_obj_set_style_bg_color(chip_battery, lv_color_hex(0x2A2010), 0);
      lv_obj_set_style_border_color(chip_battery, lv_color_hex(OCT_AMBER), 0);
      lv_obj_set_style_text_color(lbl_battery, lv_color_hex(OCT_AMBER), 0);
    } else {
      lv_obj_set_style_bg_color(chip_battery, lv_color_hex(0x102030), 0);
      lv_obj_set_style_border_color(chip_battery, lv_color_hex(OCT_CYAN), 0);
      lv_obj_set_style_text_color(lbl_battery, lv_color_hex(OCT_CYAN), 0);
    }
  } else {
    lv_obj_add_flag(chip_battery, LV_OBJ_FLAG_HIDDEN);
  }
}

static bool isNightNow() {
  const time_t nowUtc = time(nullptr);
  struct tm tm = {};
  localtime_r(&nowUtc, &tm);
  const int minutes = tm.tm_hour * 60 + tm.tm_min;
  // Night window wraps midnight: [22:00, 24:00) and [00:00, 06:30).
  return (minutes >= NIGHT_START_MIN) || (minutes < DAY_START_MIN);
}

static void applyAutoBrightness(bool force) {
  if (!nightModeEnabled) {
    if (nightPhaseApplied == 1) {
      Lcd_SetBacklight(dayBrightness);  // we had dimmed; restore daytime level
    }
    nightPhaseApplied = -1;
    return;
  }

  const bool night = isNightNow();
  const int phase = night ? 1 : 0;
  if (!force && phase == nightPhaseApplied) {
    return;  // only act on a day<->night transition
  }
  nightPhaseApplied = phase;

  uint8_t target = dayBrightness;
  if (night) {
    target = (dayBrightness < NIGHT_BRIGHTNESS) ? dayBrightness : NIGHT_BRIGHTNESS;
  }
  Lcd_SetBacklight(target);
}

// A manual brightness change defines the new daytime baseline and is honoured
// until the next day/night boundary.
static void setDayBrightness(uint8_t val) {
  dayBrightness = val;
  Lcd_SetBacklight(val);
  nightPhaseApplied = isNightNow() ? 1 : 0;
}

static void nightSwitchCb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
    return;
  }
  lv_obj_t *sw = (lv_obj_t *)lv_event_get_target(e);
  nightModeEnabled = lv_obj_has_state(sw, LV_STATE_CHECKED);

  Preferences prefs;
  prefs.begin("agile", false);
  prefs.putUChar("nightmode", nightModeEnabled ? 1 : 0);
  prefs.end();

  applyAutoBrightness(true);
}

static void brightnessSliderCb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
    return;
  }

  lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(e);
  const int32_t val = lv_slider_get_value(slider);
  setDayBrightness(static_cast<uint8_t>(val));

  if (lbl_brightness != nullptr) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%ld%%", static_cast<long>(val));
    lv_label_set_text(lbl_brightness, buf);
  }

  Preferences prefs;
  prefs.begin("agile", false);
  prefs.putUChar("backlight", static_cast<uint8_t>(val));
  prefs.end();
}

static void styleCard(lv_obj_t *obj, bool accent) {
  lv_obj_set_style_bg_color(obj, lv_color_hex(OCT_NAVY_CARD), 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(obj, lv_color_hex(accent ? OCT_PURPLE : OCT_NAVY_TAB), 0);
  lv_obj_set_style_border_width(obj, 1, 0);
  lv_obj_set_style_border_opa(obj, accent ? LV_OPA_40 : LV_OPA_20, 0);
  lv_obj_set_style_radius(obj, 10, 0);
  lv_obj_set_style_pad_all(obj, 6, 0);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

static void styleCompactCard(lv_obj_t *obj) {
  styleCard(obj, false);
  lv_obj_set_style_pad_all(obj, 4, 0);
  lv_obj_set_style_pad_row(obj, 1, 0);
}

static lv_obj_t *makeLabel(lv_obj_t *parent, const char *text, uint32_t color,
                           const lv_font_t *font, lv_text_align_t align) {
  lv_obj_t *label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_align(label, align, 0);
  return label;
}

static void stylePlainRow(lv_obj_t *row) {
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_outline_width(row, 0, 0);
  lv_obj_set_style_shadow_width(row, 0, 0);
  lv_obj_set_style_radius(row, 0, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
}

static lv_obj_t *makeStatRow(lv_obj_t *parent, const char *key, lv_obj_t **valueOut) {
  lv_obj_t *row = lv_obj_create(parent);
  lv_obj_set_width(row, lv_pct(100));
  lv_obj_set_height(row, LV_SIZE_CONTENT);
  stylePlainRow(row);
  lv_obj_set_style_pad_column(row, 8, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  makeLabel(row, key, OCT_TEXT_DIM, &lv_font_montserrat_16, LV_TEXT_ALIGN_LEFT);

  lv_obj_t *val = lv_label_create(row);
  lv_label_set_text(val, "--");
  lv_obj_set_style_text_color(val, lv_color_hex(OCT_TEXT), 0);
  lv_obj_set_style_text_font(val, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_align(val, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_flex_grow(val, 1);
  if (valueOut != nullptr) {
    *valueOut = val;
  }
  lv_obj_set_flex_grow(row, 1);
  lv_obj_set_style_min_height(row, 34, 0);
  return row;
}

static lv_obj_t *makeSectionCard(lv_obj_t *parent, const char *title, uint32_t accent) {
  lv_obj_t *card = lv_obj_create(parent);
  lv_obj_set_width(card, lv_pct(100));
  lv_obj_set_height(card, LV_SIZE_CONTENT);
  styleCard(card, false);
  lv_obj_set_style_pad_row(card, 6, 0);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

  lv_obj_t *hdr = makeLabel(card, title, accent, &lv_font_montserrat_14, LV_TEXT_ALIGN_LEFT);
  (void)hdr;
  return card;
}

static void styleTabBar(lv_obj_t *tabview) {
  lv_obj_t *tab_bar = lv_tabview_get_tab_bar(tabview);
  lv_obj_set_style_bg_color(tab_bar, lv_color_hex(OCT_NAVY_BG), 0);
  lv_obj_set_style_pad_column(tab_bar, 3, 0);
  lv_obj_set_style_pad_hor(tab_bar, TAB_PAD_H, 0);
  lv_obj_set_style_pad_ver(tab_bar, 4, 0);
  lv_obj_set_style_border_width(tab_bar, 0, 0);

  const uint32_t tabCount = lv_tabview_get_tab_count(tabview);
  for (uint32_t i = 0; i < tabCount; ++i) {
    lv_obj_t *btn = lv_obj_get_child(tab_bar, i);
    if (btn == nullptr) {
      continue;
    }
    lv_obj_set_style_radius(btn, 16, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(OCT_NAVY_TAB), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, lv_color_hex(OCT_PURPLE), LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(btn, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(btn, 6, LV_PART_MAIN);

    lv_obj_t *lbl = lv_obj_get_child(btn, 0);
    if (lbl != nullptr) {
      lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, LV_PART_MAIN);
      lv_obj_set_style_text_color(lbl, lv_color_hex(OCT_TEXT_MUTED), LV_PART_MAIN);
      lv_obj_set_style_text_color(lbl, lv_color_hex(OCT_TEXT), LV_PART_MAIN | LV_STATE_CHECKED);
    }
  }
}

static void applySafeLayout() {
  if (safe_root == nullptr) {
    return;
  }

  lv_obj_set_size(safe_root, SCREEN_W, SCREEN_H);
  lv_obj_align(safe_root, LV_ALIGN_CENTER, 0, 0);

  if (header_bar != nullptr) {
    lv_obj_set_size(header_bar, SCREEN_W, HEADER_H);
    lv_obj_align(header_bar, LV_ALIGN_TOP_MID, 0, 0);
  }

  if (tabview != nullptr) {
    lv_obj_set_size(tabview, SCREEN_W, SCREEN_H - HEADER_H);
    lv_obj_align(tabview, LV_ALIGN_BOTTOM_MID, 0, 0);
  }
}

static void styleTabPage(lv_obj_t *tab) {
  lv_obj_set_style_bg_color(tab, lv_color_hex(OCT_NAVY_BG), 0);
  lv_obj_set_style_pad_hor(tab, TAB_PAD_H, 0);
  lv_obj_set_style_pad_ver(tab, TAB_PAD_V, 0);
  lv_obj_set_style_pad_row(tab, 4, 0);
  lv_obj_set_style_border_width(tab, 0, 0);
  lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(tab, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_remove_flag(tab, LV_OBJ_FLAG_SCROLLABLE);
}

static void styleNowTab(lv_obj_t *tab) {
  styleTabPage(tab);
  lv_obj_set_height(tab, lv_pct(100));
  lv_obj_set_flex_align(tab, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
}

static lv_obj_t *makeFlexRow(lv_obj_t *parent) {
  lv_obj_t *row = lv_obj_create(parent);
  lv_obj_set_width(row, lv_pct(100));
  lv_obj_set_height(row, LV_SIZE_CONTENT);
  stylePlainRow(row);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  return row;
}

static lv_obj_t *makeFlexCol(lv_obj_t *parent, lv_flex_align_t cross) {
  lv_obj_t *col = lv_obj_create(parent);
  lv_obj_set_width(col, LV_SIZE_CONTENT);
  lv_obj_set_height(col, LV_SIZE_CONTENT);
  stylePlainRow(col);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, cross, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(col, 0, 0);
  return col;
}

static void makeLegendChip(lv_obj_t *parent, uint32_t color, const char *text) {
  lv_obj_t *chip = lv_obj_create(parent);
  lv_obj_set_size(chip, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  stylePlainRow(chip);
  lv_obj_set_flex_flow(chip, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(chip, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(chip, 5, 0);

  lv_obj_t *dot = lv_obj_create(chip);
  lv_obj_set_size(dot, 13, 13);
  lv_obj_set_style_bg_color(dot, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(dot, 0, 0);
  lv_obj_set_style_radius(dot, 7, 0);
  lv_obj_remove_flag(dot, LV_OBJ_FLAG_SCROLLABLE);

  makeLabel(chip, text, OCT_TEXT_MUTED, &lv_font_montserrat_14, LV_TEXT_ALIGN_LEFT);
}

void agileUiInit() {
  Preferences nightPrefs;
  if (nightPrefs.begin("agile", true)) {
    nightModeEnabled = nightPrefs.getUChar("nightmode", 1) != 0;
    nightPrefs.end();
  }

  lv_obj_t *screen = lv_screen_active();
  lv_obj_set_style_bg_color(screen, lv_color_hex(OCT_NAVY_BG), 0);

  safe_root = lv_obj_create(screen);
  lv_obj_set_style_bg_opa(safe_root, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(safe_root, 0, 0);
  lv_obj_set_style_pad_all(safe_root, 0, 0);
  lv_obj_remove_flag(safe_root, LV_OBJ_FLAG_SCROLLABLE);

  header_bar = lv_obj_create(safe_root);
  lv_obj_set_size(header_bar, SCREEN_W, HEADER_H);
  lv_obj_align(header_bar, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_bg_color(header_bar, lv_color_hex(OCT_NAVY_PANEL), 0);
  lv_obj_set_style_bg_grad_color(header_bar, lv_color_hex(0x0C1A28), 0);
  lv_obj_set_style_bg_grad_dir(header_bar, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_border_side(header_bar, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_color(header_bar, lv_color_hex(OCT_PURPLE), 0);
  lv_obj_set_style_border_width(header_bar, 1, 0);
  lv_obj_set_style_border_opa(header_bar, LV_OPA_20, 0);
  lv_obj_set_style_radius(header_bar, 0, 0);
  lv_obj_set_style_pad_hor(header_bar, HEADER_PAD_H, 0);
  lv_obj_set_style_pad_ver(header_bar, 4, 0);
  lv_obj_remove_flag(header_bar, LV_OBJ_FLAG_SCROLLABLE);

  lbl_brand = makeLabel(header_bar, "Agile", OCT_PURPLE, &lv_font_montserrat_18, LV_TEXT_ALIGN_LEFT);
  lv_obj_align(lbl_brand, LV_ALIGN_LEFT_MID, 0, -10);

  lbl_region = makeLabel(header_bar, AGILE_REGION_LABEL, OCT_TEXT_MUTED, &lv_font_montserrat_14,
                         LV_TEXT_ALIGN_LEFT);
  lv_obj_align(lbl_region, LV_ALIGN_LEFT_MID, 0, 13);

  lbl_clock = makeLabel(header_bar, "--:--", OCT_TEXT, &lv_font_montserrat_24, LV_TEXT_ALIGN_RIGHT);
  lv_obj_align(lbl_clock, LV_ALIGN_RIGHT_MID, 0, -9);

  chip_battery = lv_obj_create(header_bar);
  lv_obj_set_size(chip_battery, 54, 22);
  lv_obj_align(chip_battery, LV_ALIGN_RIGHT_MID, -62, -9);
  lv_obj_set_style_bg_color(chip_battery, lv_color_hex(0x102030), 0);
  lv_obj_set_style_border_color(chip_battery, lv_color_hex(OCT_CYAN), 0);
  lv_obj_set_style_border_width(chip_battery, 1, 0);
  lv_obj_set_style_radius(chip_battery, 11, 0);
  lv_obj_set_style_pad_all(chip_battery, 0, 0);
  lv_obj_remove_flag(chip_battery, LV_OBJ_FLAG_SCROLLABLE);

  lbl_battery = makeLabel(chip_battery, "--%", OCT_CYAN, &lv_font_montserrat_14, LV_TEXT_ALIGN_CENTER);
  lv_obj_center(lbl_battery);

  lbl_updated = makeLabel(header_bar, "Connecting...", OCT_TEXT_DIM, &lv_font_montserrat_14,
                          LV_TEXT_ALIGN_RIGHT);
  lv_obj_align(lbl_updated, LV_ALIGN_RIGHT_MID, 0, 13);

  flash_overlay = lv_obj_create(screen);
  lv_obj_set_size(flash_overlay, 480, 480);
  lv_obj_align(flash_overlay, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(flash_overlay, lv_color_hex(OCT_GREEN), 0);
  lv_obj_set_style_bg_opa(flash_overlay, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(flash_overlay, 0, 0);
  lv_obj_add_flag(flash_overlay, LV_OBJ_FLAG_HIDDEN);

  tabview = lv_tabview_create(safe_root);
  lv_obj_set_style_bg_color(tabview, lv_color_hex(OCT_NAVY_BG), 0);
  lv_obj_set_style_pad_all(tabview, 0, 0);
  lv_obj_set_style_pad_top(tabview, 0, 0);

  tab_now = lv_tabview_add_tab(tabview, "Now");
  lv_obj_t *tab_today = lv_tabview_add_tab(tabview, "Today");
  tab_tomorrow = lv_tabview_add_tab(tabview, "Next");
  lv_obj_t *tab_settings = lv_tabview_add_tab(tabview, "Set");

  styleTabBar(tabview);
  styleNowTab(tab_now);
  lv_obj_t *tv_content = lv_tabview_get_content(tabview);
  if (tv_content != nullptr) {
    lv_obj_set_height(tv_content, lv_pct(100));
  }
  styleTabPage(tab_today);
  styleTabPage(tab_tomorrow);
  styleTabPage(tab_settings);
  lv_obj_add_flag(tab_tomorrow, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(tab_tomorrow, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(tab_tomorrow, LV_SCROLLBAR_MODE_AUTO);

  price_card = lv_obj_create(tab_now);
  lv_obj_set_width(price_card, lv_pct(100));
  lv_obj_set_flex_grow(price_card, 2);
  styleCard(price_card, true);
  lv_obj_set_flex_flow(price_card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(price_card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(price_card, 4, 0);
  lv_obj_add_flag(price_card, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

  lv_obj_t *hdr_row = makeFlexRow(price_card);
  makeLabel(hdr_row, "NOW", OCT_TEXT_DIM, &lv_font_montserrat_16, LV_TEXT_ALIGN_LEFT);

  panel_run_score = lv_obj_create(hdr_row);
  lv_obj_set_size(panel_run_score, 84, 28);
  lv_obj_set_style_bg_color(panel_run_score, lv_color_hex(0x1E1030), 0);
  lv_obj_set_style_border_color(panel_run_score, lv_color_hex(OCT_PINK), 0);
  lv_obj_set_style_border_width(panel_run_score, 1, 0);
  lv_obj_set_style_radius(panel_run_score, 14, 0);
  lv_obj_set_style_pad_all(panel_run_score, 0, 0);
  lv_obj_remove_flag(panel_run_score, LV_OBJ_FLAG_SCROLLABLE);

  lbl_run_score = makeLabel(panel_run_score, "WAIT", OCT_PINK, &lv_font_montserrat_16,
                            LV_TEXT_ALIGN_CENTER);
  lv_obj_center(lbl_run_score);

  lv_obj_t *main_row = lv_obj_create(price_card);
  lv_obj_set_width(main_row, lv_pct(100));
  lv_obj_set_flex_grow(main_row, 1);
  stylePlainRow(main_row);
  lv_obj_set_flex_flow(main_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(main_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_add_flag(main_row, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

  price_cluster = lv_obj_create(main_row);
  lv_obj_set_size(price_cluster, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  stylePlainRow(price_cluster);
  lv_obj_set_flex_flow(price_cluster, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(price_cluster, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
  lv_obj_set_style_pad_column(price_cluster, 4, 0);
  lv_obj_add_flag(price_cluster, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

  lbl_now_price = lv_label_create(price_cluster);
  lv_label_set_text(lbl_now_price, "--.-");
  lv_obj_set_style_text_font(lbl_now_price, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(lbl_now_price, lv_color_hex(OCT_TEXT), 0);
  lv_obj_set_style_text_align(lbl_now_price, LV_TEXT_ALIGN_LEFT, 0);

  lbl_now_unit = lv_label_create(price_cluster);
  lv_label_set_text(lbl_now_unit, "p");
  lv_obj_set_style_text_font(lbl_now_unit, &lv_font_montserrat_22, 0);
  lv_obj_set_style_text_color(lbl_now_unit, lv_color_hex(OCT_TEXT_MUTED), 0);
  lv_obj_set_style_translate_y(lbl_now_unit, -7, 0);

  lv_obj_t *next_col = makeFlexCol(main_row, LV_FLEX_ALIGN_END);
  lv_obj_set_width(next_col, lv_pct(46));
  lv_obj_set_height(next_col, LV_SIZE_CONTENT);

  lv_obj_t *lbl_next_hdr = makeLabel(next_col, "NEXT", OCT_TEXT_DIM, &lv_font_montserrat_16,
                                    LV_TEXT_ALIGN_RIGHT);
  lv_obj_set_width(lbl_next_hdr, lv_pct(100));

  lbl_now_next_price = lv_label_create(next_col);
  lv_label_set_text(lbl_now_next_price, "--.-");
  lv_obj_set_width(lbl_now_next_price, lv_pct(100));
  lv_obj_set_style_text_font(lbl_now_next_price, &lv_font_montserrat_32, 0);
  lv_obj_set_style_text_color(lbl_now_next_price, lv_color_hex(OCT_CYAN), 0);
  lv_obj_set_style_text_align(lbl_now_next_price, LV_TEXT_ALIGN_RIGHT, 0);
  lv_label_set_long_mode(lbl_now_next_price, LV_LABEL_LONG_CLIP);

  lbl_now_next_when = lv_label_create(next_col);
  lv_label_set_text(lbl_now_next_when, "from --:--");
  lv_obj_set_width(lbl_now_next_when, lv_pct(100));
  lv_obj_set_style_text_font(lbl_now_next_when, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(lbl_now_next_when, lv_color_hex(OCT_TEXT_MUTED), 0);
  lv_obj_set_style_text_align(lbl_now_next_when, LV_TEXT_ALIGN_RIGHT, 0);

  lbl_now_alert = lv_label_create(price_card);
  lv_label_set_text(lbl_now_alert, "");
  lv_obj_set_width(lbl_now_alert, lv_pct(100));
  lv_obj_set_style_text_align(lbl_now_alert, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(lbl_now_alert, lv_color_hex(OCT_GREEN), 0);
  lv_obj_set_style_text_font(lbl_now_alert, &lv_font_montserrat_14, 0);
  lv_obj_set_style_bg_color(lbl_now_alert, lv_color_hex(0x0A3828), 0);
  lv_obj_set_style_bg_opa(lbl_now_alert, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(lbl_now_alert, 6, 0);
  lv_obj_set_style_pad_ver(lbl_now_alert, 2, 0);
  lv_obj_align(lbl_now_alert, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_add_flag(lbl_now_alert, LV_OBJ_FLAG_HIDDEN);

  stats_card = lv_obj_create(tab_now);
  lv_obj_set_width(stats_card, lv_pct(100));
  lv_obj_set_flex_grow(stats_card, 3);
  styleCompactCard(stats_card);
  lv_obj_set_flex_flow(stats_card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(stats_card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(stats_card, 2, 0);

  makeStatRow(stats_card, "Until", &lbl_now_until);
  makeStatRow(stats_card, "Range", &lbl_now_range);
  makeStatRow(stats_card, "Median", &lbl_now_median);
  makeStatRow(stats_card, "Cheapest 6h", &lbl_now_cheap);
  lv_obj_set_style_text_color(lbl_now_cheap, lv_color_hex(OCT_GREEN), 0);

  lbl_now_fixed = makeLabel(stats_card, "Standing charge: --", OCT_TEXT_DIM, &lv_font_montserrat_14,
                            LV_TEXT_ALIGN_LEFT);
  lv_obj_set_width(lbl_now_fixed, lv_pct(100));
  lv_label_set_long_mode(lbl_now_fixed, LV_LABEL_LONG_WRAP);

  lv_obj_t *today_hdr = makeFlexRow(tab_today);
  lbl_today_title = makeLabel(today_hdr, "TODAY", OCT_CYAN, &lv_font_montserrat_18,
                              LV_TEXT_ALIGN_LEFT);
  lbl_chart_now = makeLabel(today_hdr, "Now --:--", OCT_AMBER, &lv_font_montserrat_16,
                            LV_TEXT_ALIGN_RIGHT);

  lbl_today_meta = makeLabel(tab_today, "Loading...", OCT_TEXT_MUTED, &lv_font_montserrat_14,
                             LV_TEXT_ALIGN_LEFT);
  lv_obj_set_width(lbl_today_meta, lv_pct(100));
  lv_label_set_long_mode(lbl_today_meta, LV_LABEL_LONG_WRAP);

  chart_today = lv_chart_create(tab_today);
  lv_obj_set_width(chart_today, lv_pct(100));
  lv_obj_set_height(chart_today, 196);
  lv_obj_set_flex_grow(chart_today, 1);
  lv_chart_set_type(chart_today, LV_CHART_TYPE_BAR);
  lv_chart_set_point_count(chart_today, 48);
  lv_chart_set_range(chart_today, LV_CHART_AXIS_PRIMARY_Y, 0, 400);
  lv_chart_set_div_line_count(chart_today, 3, 0);
  lv_obj_set_style_bg_color(chart_today, lv_color_hex(OCT_NAVY_CARD), 0);
  lv_obj_set_style_border_color(chart_today, lv_color_hex(OCT_PURPLE), 0);
  lv_obj_set_style_border_width(chart_today, 1, 0);
  lv_obj_set_style_border_opa(chart_today, LV_OPA_30, 0);
  lv_obj_set_style_radius(chart_today, 12, 0);
  lv_obj_set_style_pad_all(chart_today, 8, 0);
  lv_obj_set_style_pad_column(chart_today, 1, 0);
  lv_obj_set_style_line_opa(chart_today, LV_OPA_TRANSP, LV_PART_ITEMS);
  lv_obj_set_style_line_color(chart_today, lv_color_hex(OCT_NAVY_TAB), LV_PART_MAIN);
  series_today = lv_chart_add_series(chart_today, lv_color_hex(OCT_PURPLE), LV_CHART_AXIS_PRIMARY_Y);
  cursor_today = lv_chart_add_cursor(chart_today, lv_color_hex(OCT_AMBER), LV_DIR_VER);
  lv_obj_set_style_width(chart_today, 3, LV_PART_CURSOR);
  lv_obj_add_flag(chart_today, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
  lv_obj_add_event_cb(chart_today, chartDrawCb, LV_EVENT_DRAW_TASK_ADDED, nullptr);

  lv_obj_t *axis = makeFlexRow(tab_today);
  lv_obj_set_style_pad_hor(axis, 6, 0);
  makeLabel(axis, "00", OCT_TEXT_DIM, &lv_font_montserrat_12, LV_TEXT_ALIGN_LEFT);
  makeLabel(axis, "06", OCT_TEXT_DIM, &lv_font_montserrat_12, LV_TEXT_ALIGN_CENTER);
  makeLabel(axis, "12", OCT_TEXT_DIM, &lv_font_montserrat_12, LV_TEXT_ALIGN_CENTER);
  makeLabel(axis, "18", OCT_TEXT_DIM, &lv_font_montserrat_12, LV_TEXT_ALIGN_CENTER);
  makeLabel(axis, "24h", OCT_TEXT_DIM, &lv_font_montserrat_12, LV_TEXT_ALIGN_RIGHT);

  lv_obj_t *key = makeFlexRow(tab_today);
  lv_obj_set_flex_align(key, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  makeLegendChip(key, OCT_GREEN, "Cheap");
  makeLegendChip(key, OCT_CYAN, "Average");
  makeLegendChip(key, OCT_PINK, "Peak");

  lbl_tomorrow_title = makeLabel(tab_tomorrow, "TOMORROW", OCT_CYAN, &lv_font_montserrat_16,
                                 LV_TEXT_ALIGN_LEFT);

  card_tomorrow_summary = makeSectionCard(tab_tomorrow, "DAY RANGE", OCT_TEXT_DIM);
  lbl_tomorrow_summary = makeLabel(card_tomorrow_summary, "Waiting for prices...",
                                   OCT_TEXT, &lv_font_montserrat_18, LV_TEXT_ALIGN_LEFT);
  lv_obj_set_width(lbl_tomorrow_summary, lv_pct(100));
  lv_label_set_long_mode(lbl_tomorrow_summary, LV_LABEL_LONG_WRAP);

  card_tomorrow_window = makeSectionCard(tab_tomorrow, "BEST 2H WINDOW", OCT_GREEN);
  lbl_tomorrow_window = makeLabel(card_tomorrow_window, "--", OCT_GREEN, &lv_font_montserrat_18,
                                  LV_TEXT_ALIGN_LEFT);
  lv_obj_set_width(lbl_tomorrow_window, lv_pct(100));
  lv_label_set_long_mode(lbl_tomorrow_window, LV_LABEL_LONG_WRAP);

  card_tomorrow_cheapest = makeSectionCard(tab_tomorrow, "CHEAPEST SLOTS", OCT_GREEN);
  lbl_tomorrow_cheapest = makeLabel(card_tomorrow_cheapest, "--", OCT_TEXT, &lv_font_montserrat_18,
                                    LV_TEXT_ALIGN_LEFT);
  lv_obj_set_width(lbl_tomorrow_cheapest, lv_pct(100));
  lv_label_set_long_mode(lbl_tomorrow_cheapest, LV_LABEL_LONG_WRAP);

  card_tomorrow_peak = makeSectionCard(tab_tomorrow, "PEAK SLOT", OCT_PINK);
  lbl_tomorrow_peak = makeLabel(card_tomorrow_peak, "--", OCT_PINK, &lv_font_montserrat_18,
                                LV_TEXT_ALIGN_LEFT);

  lbl_tomorrow_fixed = makeLabel(tab_tomorrow, "Standing charge: --", OCT_TEXT_DIM,
                                 &lv_font_montserrat_14, LV_TEXT_ALIGN_LEFT);

  lv_obj_t *settings_card = lv_obj_create(tab_settings);
  lv_obj_set_width(settings_card, lv_pct(100));
  lv_obj_set_height(settings_card, LV_SIZE_CONTENT);
  styleCard(settings_card, true);
  lv_obj_set_flex_flow(settings_card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(settings_card, 12, 0);

  makeLabel(settings_card, "DISPLAY", OCT_TEXT_DIM, &lv_font_montserrat_14, LV_TEXT_ALIGN_LEFT);

  lv_obj_t *bright_row = lv_obj_create(settings_card);
  lv_obj_set_width(bright_row, lv_pct(100));
  lv_obj_set_height(bright_row, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(bright_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(bright_row, 0, 0);
  lv_obj_set_style_pad_all(bright_row, 0, 0);
  lv_obj_set_flex_flow(bright_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(bright_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_remove_flag(bright_row, LV_OBJ_FLAG_SCROLLABLE);

  makeLabel(bright_row, "Brightness", OCT_TEXT, &lv_font_montserrat_18, LV_TEXT_ALIGN_LEFT);
  lbl_brightness = makeLabel(bright_row, "80%", OCT_CYAN, &lv_font_montserrat_20,
                             LV_TEXT_ALIGN_RIGHT);

  slider_brightness = lv_slider_create(settings_card);
  lv_obj_set_width(slider_brightness, lv_pct(100));
  lv_slider_set_range(slider_brightness, 10, 100);
  lv_slider_set_value(slider_brightness, 80, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(slider_brightness, lv_color_hex(OCT_NAVY_TAB), LV_PART_MAIN);
  lv_obj_set_style_bg_color(slider_brightness, lv_color_hex(OCT_PURPLE), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(slider_brightness, lv_color_hex(OCT_CYAN), LV_PART_KNOB);
  lv_obj_set_style_height(slider_brightness, 8, LV_PART_MAIN);
  lv_obj_set_style_height(slider_brightness, 8, LV_PART_INDICATOR);
  lv_obj_set_style_pad_all(slider_brightness, 12, LV_PART_KNOB);
  lv_obj_add_event_cb(slider_brightness, brightnessSliderCb, LV_EVENT_VALUE_CHANGED, nullptr);

  lv_obj_t *night_row = lv_obj_create(settings_card);
  lv_obj_set_width(night_row, lv_pct(100));
  lv_obj_set_height(night_row, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(night_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(night_row, 0, 0);
  lv_obj_set_style_pad_all(night_row, 0, 0);
  lv_obj_set_flex_flow(night_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(night_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_remove_flag(night_row, LV_OBJ_FLAG_SCROLLABLE);

  makeLabel(night_row, "Auto night dim", OCT_TEXT, &lv_font_montserrat_18, LV_TEXT_ALIGN_LEFT);

  switch_night = lv_switch_create(night_row);
  lv_obj_set_style_bg_color(switch_night, lv_color_hex(OCT_NAVY_TAB), LV_PART_MAIN);
  lv_obj_set_style_bg_color(switch_night, lv_color_hex(OCT_PURPLE), LV_PART_INDICATOR | LV_STATE_CHECKED);
  lv_obj_set_style_bg_color(switch_night, lv_color_hex(OCT_CYAN), LV_PART_KNOB);
  lv_obj_add_event_cb(switch_night, nightSwitchCb, LV_EVENT_VALUE_CHANGED, nullptr);
  if (nightModeEnabled) {
    lv_obj_add_state(switch_night, LV_STATE_CHECKED);
  }

  lbl_night = makeLabel(settings_card, "Dim from 22:00, bright from 06:30", OCT_TEXT_DIM,
                        &lv_font_montserrat_14, LV_TEXT_ALIGN_LEFT);
  lv_obj_set_width(lbl_night, lv_pct(100));
  lv_label_set_long_mode(lbl_night, LV_LABEL_LONG_WRAP);

  lv_obj_t *info_card = lv_obj_create(tab_settings);
  lv_obj_set_width(info_card, lv_pct(100));
  lv_obj_set_height(info_card, LV_SIZE_CONTENT);
  styleCard(info_card, false);
  lv_obj_set_flex_flow(info_card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(info_card, 4, 0);

  makeLabel(info_card, "DEVICE", OCT_TEXT_DIM, &lv_font_montserrat_14, LV_TEXT_ALIGN_LEFT);
  char infoBuf[80];
  snprintf(infoBuf, sizeof(infoBuf), "Region: %s (%s)", AGILE_REGION_LABEL, AGILE_REGION);
  makeLabel(info_card, infoBuf, OCT_TEXT_MUTED, &lv_font_montserrat_14, LV_TEXT_ALIGN_LEFT);
  snprintf(infoBuf, sizeof(infoBuf), "Tariff: %s", AGILE_PRODUCT_CODE);
  makeLabel(info_card, infoBuf, OCT_TEXT_MUTED, &lv_font_montserrat_14, LV_TEXT_ALIGN_LEFT);

  lv_obj_t *btn_card = lv_obj_create(tab_settings);
  lv_obj_set_width(btn_card, lv_pct(100));
  lv_obj_set_height(btn_card, LV_SIZE_CONTENT);
  styleCard(btn_card, false);
  lv_obj_set_flex_flow(btn_card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(btn_card, 4, 0);

  makeLabel(btn_card, "BUTTONS", OCT_TEXT_DIM, &lv_font_montserrat_14, LV_TEXT_ALIGN_LEFT);
  makeLabel(btn_card, "PWR  hold ~4s power off", OCT_TEXT_MUTED, &lv_font_montserrat_14, LV_TEXT_ALIGN_LEFT);
  makeLabel(btn_card, "KEY (+)  brighter", OCT_TEXT_MUTED, &lv_font_montserrat_14, LV_TEXT_ALIGN_LEFT);
  makeLabel(btn_card, "BOOT (-)  dimmer", OCT_TEXT_MUTED, &lv_font_montserrat_14, LV_TEXT_ALIGN_LEFT);
  makeLabel(btn_card, "Hold KEY  rotate 360", OCT_TEXT_MUTED, &lv_font_montserrat_14, LV_TEXT_ALIGN_LEFT);
  makeLabel(btn_card, "Hold BOOT  next tab", OCT_TEXT_MUTED, &lv_font_montserrat_14, LV_TEXT_ALIGN_LEFT);
  makeLabel(btn_card, "Swipe tabs on screen", OCT_TEXT_DIM, &lv_font_montserrat_14, LV_TEXT_ALIGN_LEFT);

  applySafeLayout();
}

void agileUiRelayout() {
  if (!bsp_lvgl_lock(50)) {
    return;
  }
  applySafeLayout();
  bsp_lvgl_unlock();
}

void agileUiRefresh(const AgileStore &store) {
  if (!bsp_lvgl_lock(200)) {
    return;
  }

  char buf[160];
  const time_t nowUtc = time(nullptr);

  if (!store.fetchOk) {
    updateHeader(store, true);
    bsp_lvgl_unlock();
    return;
  }

  updateHeader(store, false);

  const RateSlot *current = store.currentSlot(nowUtc);
  const RateSlot *next = store.nextSlot(nowUtc);
  const RateSlot *cheap = store.cheapestInHours(nowUtc, 6);

  float minVal = 0.0f;
  float maxVal = 0.0f;
  const time_t todayStart = store.startOfLocalDay(nowUtc);
  const bool haveTodayRange = store.minMaxForDay(todayStart, &minVal, &maxVal);
  const float median = store.medianForDay(todayStart);

  if (current) {
    snprintf(buf, sizeof(buf), "%.1f", current->penceIncVat);
    lv_label_set_text(lbl_now_price, buf);
    checkNegativeEdge(current->penceIncVat);
    updateNegativeFx(current->penceIncVat);
    updateRunScore(current->penceIncVat, median);
    if (!negativeFxActive && haveTodayRange) {
      lv_obj_set_style_text_color(lbl_now_price, priceColor(current->penceIncVat, minVal, maxVal), 0);
    } else if (!negativeFxActive) {
      lv_obj_set_style_text_color(lbl_now_price, lv_color_hex(OCT_TEXT), 0);
    }
    char until[48];
    formatLocalTime(current->endUtc, until, sizeof(until));
    lv_label_set_text(lbl_now_until, until);
  } else {
    stopNegativeFx();
    wasNegative = false;
    updateRunScore(0.0f, median);
    lv_label_set_text(lbl_now_price, "--.-");
    lv_obj_set_style_text_color(lbl_now_price, lv_color_hex(OCT_TEXT_MUTED), 0);
    lv_label_set_text(lbl_now_until, "--");
  }

  if (next) {
    char tbuf[16];
    char pbuf[16];
    formatLocalTime(next->startUtc, tbuf, sizeof(tbuf));
    formatPence(next->penceIncVat, pbuf, sizeof(pbuf));
    lv_label_set_text(lbl_now_next_price, pbuf);
    snprintf(buf, sizeof(buf), "from %s", tbuf);
    lv_label_set_text(lbl_now_next_when, buf);
    lv_obj_set_style_text_color(lbl_now_next_price, lv_color_hex(OCT_CYAN), 0);
  } else {
    lv_label_set_text(lbl_now_next_price, "--.-");
    lv_label_set_text(lbl_now_next_when, "from --:--");
    lv_obj_set_style_text_color(lbl_now_next_price, lv_color_hex(OCT_TEXT_MUTED), 0);
  }

  if (haveTodayRange) {
    char minBuf[16];
    char maxBuf[16];
    char medBuf[16];
    formatPence(minVal, minBuf, sizeof(minBuf));
    formatPence(maxVal, maxBuf, sizeof(maxBuf));
    formatPence(median, medBuf, sizeof(medBuf));
    snprintf(buf, sizeof(buf), "%s - %s", minBuf, maxBuf);
    lv_label_set_text(lbl_now_range, buf);
    lv_label_set_text(lbl_now_median, medBuf);
    snprintf(buf, sizeof(buf), "Low %s  High %s  Med %s", minBuf, maxBuf, medBuf);
    lv_label_set_text(lbl_today_meta, buf);
  } else {
    lv_label_set_text(lbl_now_range, "--");
    lv_label_set_text(lbl_now_median, "--");
    lv_label_set_text(lbl_today_meta, "No data yet");
  }

  if (cheap) {
    char tbuf[16];
    char pbuf[16];
    formatLocalTime(cheap->startUtc, tbuf, sizeof(tbuf));
    formatPence(cheap->penceIncVat, pbuf, sizeof(pbuf));
    snprintf(buf, sizeof(buf), "%s at %s", pbuf, tbuf);
    lv_label_set_text(lbl_now_cheap, buf);
  } else {
    lv_label_set_text(lbl_now_cheap, "--");
  }

  updateStandingLabels();

  RateSlot daySlots[64];
  const size_t todayCount = store.slotsOnDay(todayStart, daySlots, 64);
  chartCurrentIdx = current ? store.indexOnDay(todayStart, current) : -1;
  if (todayCount > 0 && series_today != nullptr) {
    float chartMin = daySlots[0].penceIncVat;
    float chartMax = daySlots[0].penceIncVat;
    chartBarCount = todayCount;
    for (size_t i = 0; i < todayCount; ++i) {
      chartBarRates[i] = daySlots[i].penceIncVat;
      if (daySlots[i].penceIncVat < chartMin) chartMin = daySlots[i].penceIncVat;
      if (daySlots[i].penceIncVat > chartMax) chartMax = daySlots[i].penceIncVat;
    }
    chartBarMin = chartMin;
    chartBarMax = chartMax;
    const int32_t yMax = static_cast<int32_t>(chartMax + 5.0f);
    lv_chart_set_range(chart_today, LV_CHART_AXIS_PRIMARY_Y, 0, yMax);
    lv_chart_set_point_count(chart_today, todayCount);
    for (size_t i = 0; i < todayCount; ++i) {
      lv_chart_set_value_by_id(chart_today, series_today, i, static_cast<int32_t>(daySlots[i].penceIncVat));
    }
    if (cursor_today != nullptr && chartCurrentIdx >= 0) {
      lv_chart_set_cursor_point(chart_today, cursor_today, series_today,
                                static_cast<uint32_t>(chartCurrentIdx));
    }
    lv_chart_refresh(chart_today);
  }

  if (current) {
    char tbuf[16];
    formatLocalTime(current->startUtc, tbuf, sizeof(tbuf));
    snprintf(buf, sizeof(buf), "Now %s", tbuf);
    lv_label_set_text(lbl_chart_now, buf);
  } else {
    lv_label_set_text(lbl_chart_now, "Now --:--");
  }

  const time_t tomorrowStart = store.tomorrowStartUtc(nowUtc);
  RateSlot tomorrowSlots[64];
  const size_t tomorrowCount = store.slotsOnDay(tomorrowStart, tomorrowSlots, 64);

  struct tm tm = {};
  localtime_r(&tomorrowStart, &tm);
  char dayBuf[32];
  strftime(dayBuf, sizeof(dayBuf), "%A %d %b", &tm);
  snprintf(buf, sizeof(buf), "TOMORROW  %s", dayBuf);
  lv_label_set_text(lbl_tomorrow_title, buf);

  if (tomorrowCount < 40) {
    lv_label_set_text(lbl_tomorrow_summary, "Prices publish around 4pm UK time");
    lv_label_set_text(lbl_tomorrow_window, "Not available yet");
    lv_label_set_text(lbl_tomorrow_cheapest, "--");
    lv_label_set_text(lbl_tomorrow_peak, "--");
    lv_obj_add_flag(card_tomorrow_window, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(card_tomorrow_cheapest, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(card_tomorrow_peak, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_remove_flag(card_tomorrow_window, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(card_tomorrow_cheapest, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(card_tomorrow_peak, LV_OBJ_FLAG_HIDDEN);

    float tMin = 0.0f;
    float tMax = 0.0f;
    store.minMaxForDay(tomorrowStart, &tMin, &tMax);
    const float avg = store.averageForDay(tomorrowStart);
    const float tMed = store.medianForDay(tomorrowStart);
    char minBuf[16];
    char maxBuf[16];
    char avgBuf[16];
    char medBuf[16];
    formatPence(tMin, minBuf, sizeof(minBuf));
    formatPence(tMax, maxBuf, sizeof(maxBuf));
    formatPence(avg, avgBuf, sizeof(avgBuf));
    formatPence(tMed, medBuf, sizeof(medBuf));
    snprintf(buf, sizeof(buf), "%s to %s\nAvg %s  Med %s", minBuf, maxBuf, avgBuf, medBuf);
    lv_label_set_text(lbl_tomorrow_summary, buf);

    const RateSlot *windowStart = nullptr;
    float windowAvg = 0.0f;
    if (store.bestWindow(tomorrowStart, 4, &windowStart, &windowAvg) && windowStart != nullptr) {
      char startBuf[16];
      char endBuf[16];
      char avgWinBuf[16];
      formatLocalTime(windowStart->startUtc, startBuf, sizeof(startBuf));
      formatLocalTime(windowStart->startUtc + (4 * 1800), endBuf, sizeof(endBuf));
      formatPence(windowAvg, avgWinBuf, sizeof(avgWinBuf));
      snprintf(buf, sizeof(buf), "Run at %s - %s\nAverage %s", startBuf, endBuf, avgWinBuf);
      lv_label_set_text(lbl_tomorrow_window, buf);
    } else {
      lv_label_set_text(lbl_tomorrow_window, "Not enough slots");
    }

    RateSlot sortedSlots[64];
    for (size_t i = 0; i < tomorrowCount; ++i) {
      sortedSlots[i] = tomorrowSlots[i];
    }
    for (size_t i = 0; i < tomorrowCount - 1; ++i) {
      for (size_t j = i + 1; j < tomorrowCount; ++j) {
        if (sortedSlots[j].penceIncVat < sortedSlots[i].penceIncVat) {
          RateSlot tmp = sortedSlots[i];
          sortedSlots[i] = sortedSlots[j];
          sortedSlots[j] = tmp;
        }
      }
    }

    char lines[200] = {};
    const size_t show = tomorrowCount >= 5 ? 5 : tomorrowCount;
    for (size_t i = 0; i < show; ++i) {
      char tbuf[16];
      char pbuf[16];
      formatLocalTime(sortedSlots[i].startUtc, tbuf, sizeof(tbuf));
      formatPence(sortedSlots[i].penceIncVat, pbuf, sizeof(pbuf));
      char line[40];
      snprintf(line, sizeof(line), "%s  %s\n", tbuf, pbuf);
      strncat(lines, line, sizeof(lines) - strlen(lines) - 1);
    }
    lv_label_set_text(lbl_tomorrow_cheapest, lines);

    const RateSlot *peak = &sortedSlots[0];
    for (size_t i = 1; i < tomorrowCount; ++i) {
      if (sortedSlots[i].penceIncVat > peak->penceIncVat) {
        peak = &sortedSlots[i];
      }
    }
    char tbuf[16];
    char pbuf[16];
    formatLocalTime(peak->startUtc, tbuf, sizeof(tbuf));
    formatPence(peak->penceIncVat, pbuf, sizeof(pbuf));
    snprintf(buf, sizeof(buf), "%s at %s", pbuf, tbuf);
    lv_label_set_text(lbl_tomorrow_peak, buf);
  }

  bsp_lvgl_unlock();
}

void agileUiSetBrightness(uint8_t brightness) {
  if (brightness < 10) brightness = 10;
  if (brightness > 100) brightness = 100;
  dayBrightness = brightness;
  if (slider_brightness != nullptr && bsp_lvgl_lock(50)) {
    lv_slider_set_value(slider_brightness, brightness, LV_ANIM_OFF);
    if (lbl_brightness != nullptr) {
      char buf[16];
      snprintf(buf, sizeof(buf), "%u%%", brightness);
      lv_label_set_text(lbl_brightness, buf);
    }
    bsp_lvgl_unlock();
  }
  // Apply now so a boot during the night window dims immediately.
  nightPhaseApplied = -1;
  applyAutoBrightness(true);
}

void agileUiTick(const AgileStore &store) {
  if (!bsp_lvgl_lock(50)) {
    return;
  }

  updateHeader(store, !store.fetchOk);

  if (store.fetchOk) {
    const time_t nowUtc = time(nullptr);
    const RateSlot *current = store.currentSlot(nowUtc);
    if (current != nullptr) {
      checkNegativeEdge(current->penceIncVat);
      updateNegativeFx(current->penceIncVat);
      const float median = store.medianForDay(store.startOfLocalDay(nowUtc));
      updateRunScore(current->penceIncVat, median);
    }
  }

  applyAutoBrightness(false);

  bsp_lvgl_unlock();
}

void agileUiNextTab() {
  if (tabview == nullptr) {
    return;
  }
  if (!bsp_lvgl_lock(50)) {
    return;
  }
  const uint32_t count = lv_tabview_get_tab_count(tabview);
  uint32_t active = lv_tabview_get_tab_active(tabview);
  active = (active + 1) % count;
  lv_tabview_set_active(tabview, active, LV_ANIM_ON);
  bsp_lvgl_unlock();
}

void agileUiPrevTab() {
  if (tabview == nullptr) {
    return;
  }
  if (!bsp_lvgl_lock(50)) {
    return;
  }
  const uint32_t count = lv_tabview_get_tab_count(tabview);
  uint32_t active = lv_tabview_get_tab_active(tabview);
  active = (active + count - 1) % count;
  lv_tabview_set_active(tabview, active, LV_ANIM_ON);
  bsp_lvgl_unlock();
}

void agileUiBrightnessStep(int delta) {
  if (slider_brightness == nullptr) {
    return;
  }
  if (!bsp_lvgl_lock(50)) {
    return;
  }
  int32_t val = lv_slider_get_value(slider_brightness) + delta;
  if (val < 10) {
    val = 10;
  }
  if (val > 100) {
    val = 100;
  }
  lv_slider_set_value(slider_brightness, val, LV_ANIM_OFF);
  setDayBrightness(static_cast<uint8_t>(val));
  if (lbl_brightness != nullptr) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%ld%%", static_cast<long>(val));
    lv_label_set_text(lbl_brightness, buf);
  }
  Preferences prefs;
  prefs.begin("agile", false);
  prefs.putUChar("backlight", static_cast<uint8_t>(val));
  prefs.end();
  bsp_lvgl_unlock();
}
