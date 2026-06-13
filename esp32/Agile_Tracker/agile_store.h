#pragma once

#include <Arduino.h>
#include <time.h>

#include "agile_config.h"

struct RateSlot {
  time_t startUtc;
  time_t endUtc;
  float penceIncVat;
};

class AgileStore {
 public:
  RateSlot slots[MAX_RATE_SLOTS];
  size_t slotCount = 0;
  bool fetchOk = false;
  char lastError[64] = {};
  time_t fetchedAtUtc = 0;

  void clear();
  void addSlot(time_t startUtc, time_t endUtc, float pence);

  const RateSlot *currentSlot(time_t nowUtc) const;
  const RateSlot *nextSlot(time_t nowUtc) const;
  const RateSlot *cheapestInHours(time_t nowUtc, int hours) const;

  size_t slotsOnDay(time_t dayStartUtc, RateSlot *out, size_t maxOut) const;
  bool minMaxForDay(time_t dayStartUtc, float *minOut, float *maxOut) const;
  float averageForDay(time_t dayStartUtc) const;
  float medianForDay(time_t dayStartUtc) const;
  int indexOnDay(time_t dayStartUtc, const RateSlot *slot) const;
  bool bestWindow(time_t dayStartUtc, size_t slotCount, const RateSlot **outStart,
                  float *outAvg) const;

  time_t startOfLocalDay(time_t nowUtc) const;
  time_t tomorrowStartUtc(time_t nowUtc) const;
};

extern AgileStore g_agileStore;
