#include "agile_api.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "agile_config.h"

TariffMeta g_tariffMeta;

static time_t parseIsoUtc(const char *value) {
  if (value == nullptr) {
    return 0;
  }

  int year = 0;
  int mon = 0;
  int day = 0;
  int hour = 0;
  int min = 0;
  int sec = 0;
  if (sscanf(value, "%d-%d-%dT%d:%d:%d", &year, &mon, &day, &hour, &min, &sec) != 6) {
    return 0;
  }

  struct tm tm = {};
  tm.tm_year = year - 1900;
  tm.tm_mon = mon - 1;
  tm.tm_mday = day;
  tm.tm_hour = hour;
  tm.tm_min = min;
  tm.tm_sec = sec;
  tm.tm_isdst = 0;

  // ESP32 Arduino has no timegm(); treat parsed fields as UTC via TZ.
  setenv("TZ", "UTC0", 1);
  tzset();
  const time_t utc = mktime(&tm);
  setenv("TZ", "GMT0BST,M3.5.0/1,M10.5.0/2", 1);
  tzset();
  return utc;
}

static String buildRatesUrl(time_t periodFromUtc, time_t periodToUtc) {
  char fromBuf[32];
  char toBuf[32];
  struct tm fromTm = {};
  struct tm toTm = {};
  gmtime_r(&periodFromUtc, &fromTm);
  gmtime_r(&periodToUtc, &toTm);
  strftime(fromBuf, sizeof(fromBuf), "%Y-%m-%dT%H:%M:%SZ", &fromTm);
  strftime(toBuf, sizeof(toBuf), "%Y-%m-%dT%H:%M:%SZ", &toTm);

  String url = "https://api.octopus.energy/v1/products/";
  url += AGILE_PRODUCT_CODE;
  url += "/electricity-tariffs/";
  url += AGILE_TARIFF_CODE;
  url += "/standard-unit-rates/?period_from=";
  url += fromBuf;
  url += "&period_to=";
  url += toBuf;
  url += "&page_size=1500";
  return url;
}

static bool extractQuotedField(const String &object, const char *key, char *out, size_t outLen) {
  const String needle = String("\"") + key + "\":\"";
  const int keyPos = object.indexOf(needle);
  if (keyPos < 0) {
    return false;
  }

  const int start = keyPos + needle.length();
  const int end = object.indexOf('"', start);
  if (end < 0) {
    return false;
  }

  object.substring(start, end).toCharArray(out, outLen);
  return true;
}

static bool extractFloatField(const String &object, const char *key, float &out) {
  const String needle = String("\"") + key + "\":";
  const int keyPos = object.indexOf(needle);
  if (keyPos < 0) {
    return false;
  }

  const int start = keyPos + needle.length();
  out = object.substring(start).toFloat();
  return true;
}

static bool parseResultsArray(const String &body, AgileStore &store) {
  const int resultsPos = body.indexOf("\"results\":[");
  if (resultsPos < 0) {
    snprintf(store.lastError, sizeof(store.lastError), "No results");
    return false;
  }

  int pos = resultsPos + 11;
  const int resultsEnd = body.indexOf(']', pos);
  if (resultsEnd < 0) {
    snprintf(store.lastError, sizeof(store.lastError), "Bad JSON");
    return false;
  }

  while (pos < resultsEnd) {
    const int objectStart = body.indexOf('{', pos);
    if (objectStart < 0 || objectStart >= resultsEnd) {
      break;
    }

    const int objectEnd = body.indexOf('}', objectStart);
    if (objectEnd < 0 || objectEnd > resultsEnd) {
      break;
    }

    const String object = body.substring(objectStart, objectEnd + 1);
    char validFrom[32] = {};
    char validTo[32] = {};
    float pence = 0.0f;

    if (extractQuotedField(object, "valid_from", validFrom, sizeof(validFrom)) &&
        extractQuotedField(object, "valid_to", validTo, sizeof(validTo)) &&
        extractFloatField(object, "value_inc_vat", pence)) {
      const time_t startUtc = parseIsoUtc(validFrom);
      const time_t endUtc = parseIsoUtc(validTo);
      if (startUtc > 0 && endUtc > 0) {
        store.addSlot(startUtc, endUtc, pence);
      }
    }

    pos = objectEnd + 1;
  }

  return store.slotCount > 0;
}

static void sortSlots(AgileStore &store) {
  for (size_t i = 0; i + 1 < store.slotCount; ++i) {
    for (size_t j = i + 1; j < store.slotCount; ++j) {
      if (store.slots[j].startUtc < store.slots[i].startUtc) {
        RateSlot tmp = store.slots[i];
        store.slots[i] = store.slots[j];
        store.slots[j] = tmp;
      }
    }
  }
}

static String buildStandingChargeUrl(time_t periodFromUtc, time_t periodToUtc) {
  char fromBuf[32];
  char toBuf[32];
  struct tm fromTm = {};
  struct tm toTm = {};
  gmtime_r(&periodFromUtc, &fromTm);
  gmtime_r(&periodToUtc, &toTm);
  strftime(fromBuf, sizeof(fromBuf), "%Y-%m-%dT%H:%M:%SZ", &fromTm);
  strftime(toBuf, sizeof(toBuf), "%Y-%m-%dT%H:%M:%SZ", &toTm);

  String url = "https://api.octopus.energy/v1/products/";
  url += AGILE_PRODUCT_CODE;
  url += "/electricity-tariffs/";
  url += AGILE_TARIFF_CODE;
  url += "/standing-charges/?period_from=";
  url += fromBuf;
  url += "&period_to=";
  url += toBuf;
  url += "&page_size=10";
  return url;
}

static bool fetchUrlBody(const String &url, char *lastError, size_t lastErrorLen, String &bodyOut) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, url)) {
    snprintf(lastError, lastErrorLen, "HTTP begin failed");
    return false;
  }

  http.setTimeout(20000);
  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    snprintf(lastError, lastErrorLen, "HTTP %d", code);
    http.end();
    return false;
  }

  bodyOut = http.getString();
  http.end();

  if (bodyOut.length() < 32) {
    snprintf(lastError, lastErrorLen, "Empty response");
    return false;
  }
  return true;
}

static bool fetchUrl(const String &url, AgileStore &store) {
  String body;
  if (!fetchUrlBody(url, store.lastError, sizeof(store.lastError), body)) {
    return false;
  }
  return parseResultsArray(body, store);
}

static bool parseStandingCharge(const String &body, TariffMeta &meta, time_t nowUtc) {
  const int resultsPos = body.indexOf("\"results\":[");
  if (resultsPos < 0) {
    snprintf(meta.lastError, sizeof(meta.lastError), "No results");
    return false;
  }

  int pos = resultsPos + 11;
  const int resultsEnd = body.indexOf(']', pos);
  if (resultsEnd < 0) {
    snprintf(meta.lastError, sizeof(meta.lastError), "Bad JSON");
    return false;
  }

  float bestCharge = 0.0f;
  time_t bestFrom = 0;
  bool found = false;
  float fallbackCharge = 0.0f;
  bool haveFallback = false;

  while (pos < resultsEnd) {
    const int objectStart = body.indexOf('{', pos);
    if (objectStart < 0 || objectStart >= resultsEnd) {
      break;
    }

    const int objectEnd = body.indexOf('}', objectStart);
    if (objectEnd < 0 || objectEnd > resultsEnd) {
      break;
    }

    const String object = body.substring(objectStart, objectEnd + 1);
    char validFrom[32] = {};
    float pence = 0.0f;

    if (extractQuotedField(object, "valid_from", validFrom, sizeof(validFrom)) &&
        extractFloatField(object, "value_inc_vat", pence)) {
      if (!haveFallback) {
        fallbackCharge = pence;
        haveFallback = true;
      }
      const time_t fromUtc = parseIsoUtc(validFrom);
      if (fromUtc > 0 && fromUtc <= nowUtc && fromUtc >= bestFrom) {
        bestFrom = fromUtc;
        bestCharge = pence;
        found = true;
      }
    }

    pos = objectEnd + 1;
  }

  if (!found && haveFallback) {
    bestCharge = fallbackCharge;
    found = true;
  }

  if (!found) {
    snprintf(meta.lastError, sizeof(meta.lastError), "No standing charge");
    return false;
  }

  meta.standingChargePencePerDay = bestCharge;
  return true;
}

void agileInitTime() {
  setenv("TZ", "GMT0BST,M3.5.0/1,M10.5.0/2", 1);
  tzset();
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  for (int i = 0; i < 20; ++i) {
    if (time(nullptr) > 1700000000) {
      return;
    }
    delay(500);
  }
}

bool agileFetchRates(AgileStore &store) {
  store.clear();

  const time_t nowUtc = time(nullptr);
  if (nowUtc < 1700000000) {
    snprintf(store.lastError, sizeof(store.lastError), "Clock not synced");
    return false;
  }

  const time_t dayStart = store.startOfLocalDay(nowUtc);
  const time_t periodTo = dayStart + (48 * 3600);
  const String url = buildRatesUrl(dayStart, periodTo);

  if (!fetchUrl(url, store)) {
    return false;
  }

  sortSlots(store);
  store.fetchOk = true;
  store.fetchedAtUtc = nowUtc;
  return true;
}

bool agileFetchStandingCharge(TariffMeta &meta, const AgileStore &store) {
  meta.fetchOk = false;
  meta.lastError[0] = '\0';

  const time_t nowUtc = time(nullptr);
  if (nowUtc < 1700000000) {
    snprintf(meta.lastError, sizeof(meta.lastError), "Clock not synced");
    return false;
  }

  const time_t dayStart = store.startOfLocalDay(nowUtc);
  const time_t periodTo = dayStart + (48 * 3600);
  const String url = buildStandingChargeUrl(dayStart, periodTo);

  String body;
  if (!fetchUrlBody(url, meta.lastError, sizeof(meta.lastError), body)) {
    return false;
  }

  if (!parseStandingCharge(body, meta, nowUtc)) {
    return false;
  }

  meta.fetchOk = true;
  meta.fetchedAtUtc = nowUtc;
  return true;
}
