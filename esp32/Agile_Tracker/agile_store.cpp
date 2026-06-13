#include "agile_store.h"

AgileStore g_agileStore;

void AgileStore::clear() {
  slotCount = 0;
  fetchOk = false;
  lastError[0] = '\0';
  fetchedAtUtc = 0;
}

void AgileStore::addSlot(time_t startUtc, time_t endUtc, float pence) {
  if (slotCount >= MAX_RATE_SLOTS) {
    return;
  }
  slots[slotCount++] = {startUtc, endUtc, pence};
}

const RateSlot *AgileStore::currentSlot(time_t nowUtc) const {
  for (size_t i = 0; i < slotCount; ++i) {
    if (slots[i].startUtc <= nowUtc && nowUtc < slots[i].endUtc) {
      return &slots[i];
    }
  }
  return nullptr;
}

const RateSlot *AgileStore::nextSlot(time_t nowUtc) const {
  for (size_t i = 0; i < slotCount; ++i) {
    if (slots[i].startUtc > nowUtc) {
      return &slots[i];
    }
  }
  return nullptr;
}

const RateSlot *AgileStore::cheapestInHours(time_t nowUtc, int hours) const {
  const RateSlot *best = nullptr;
  const time_t horizon = nowUtc + (hours * 3600);
  for (size_t i = 0; i < slotCount; ++i) {
    if (slots[i].startUtc < nowUtc || slots[i].startUtc >= horizon) {
      continue;
    }
    if (best == nullptr || slots[i].penceIncVat < best->penceIncVat) {
      best = &slots[i];
    }
  }
  return best;
}

time_t AgileStore::startOfLocalDay(time_t nowUtc) const {
  struct tm localTm = {};
  localtime_r(&nowUtc, &localTm);
  localTm.tm_hour = 0;
  localTm.tm_min = 0;
  localTm.tm_sec = 0;
  return mktime(&localTm);
}

time_t AgileStore::tomorrowStartUtc(time_t nowUtc) const {
  return startOfLocalDay(nowUtc) + (24 * 3600);
}

size_t AgileStore::slotsOnDay(time_t dayStartUtc, RateSlot *out, size_t maxOut) const {
  const time_t dayEndUtc = dayStartUtc + (24 * 3600);
  size_t written = 0;
  for (size_t i = 0; i < slotCount && written < maxOut; ++i) {
    if (slots[i].startUtc >= dayStartUtc && slots[i].startUtc < dayEndUtc) {
      out[written++] = slots[i];
    }
  }
  return written;
}

bool AgileStore::minMaxForDay(time_t dayStartUtc, float *minOut, float *maxOut) const {
  RateSlot daySlots[64];
  const size_t count = slotsOnDay(dayStartUtc, daySlots, 64);
  if (count == 0) {
    return false;
  }
  float minVal = daySlots[0].penceIncVat;
  float maxVal = daySlots[0].penceIncVat;
  for (size_t i = 1; i < count; ++i) {
    if (daySlots[i].penceIncVat < minVal) minVal = daySlots[i].penceIncVat;
    if (daySlots[i].penceIncVat > maxVal) maxVal = daySlots[i].penceIncVat;
  }
  *minOut = minVal;
  *maxOut = maxVal;
  return true;
}

float AgileStore::averageForDay(time_t dayStartUtc) const {
  RateSlot daySlots[64];
  const size_t count = slotsOnDay(dayStartUtc, daySlots, 64);
  if (count == 0) {
    return 0.0f;
  }
  float sum = 0.0f;
  for (size_t i = 0; i < count; ++i) {
    sum += daySlots[i].penceIncVat;
  }
  return sum / static_cast<float>(count);
}

float AgileStore::medianForDay(time_t dayStartUtc) const {
  RateSlot daySlots[64];
  const size_t count = slotsOnDay(dayStartUtc, daySlots, 64);
  if (count == 0) {
    return 0.0f;
  }

  float values[64];
  for (size_t i = 0; i < count; ++i) {
    values[i] = daySlots[i].penceIncVat;
  }

  for (size_t i = 1; i < count; ++i) {
    float key = values[i];
    size_t j = i;
    while (j > 0 && values[j - 1] > key) {
      values[j] = values[j - 1];
      --j;
    }
    values[j] = key;
  }

  const size_t mid = count / 2;
  if (count % 2 == 1) {
    return values[mid];
  }
  return (values[mid - 1] + values[mid]) / 2.0f;
}

int AgileStore::indexOnDay(time_t dayStartUtc, const RateSlot *slot) const {
  if (slot == nullptr) {
    return -1;
  }

  RateSlot daySlots[64];
  const size_t count = slotsOnDay(dayStartUtc, daySlots, 64);
  for (size_t i = 0; i < count; ++i) {
    if (daySlots[i].startUtc == slot->startUtc) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

bool AgileStore::bestWindow(time_t dayStartUtc, size_t slotCount, const RateSlot **outStart,
                            float *outAvg) const {
  if (outStart == nullptr || outAvg == nullptr || slotCount == 0) {
    return false;
  }

  RateSlot daySlots[64];
  const size_t count = slotsOnDay(dayStartUtc, daySlots, 64);
  if (count < slotCount) {
    return false;
  }

  const RateSlot *best = nullptr;
  float bestAvg = 0.0f;
  bool found = false;

  for (size_t i = 0; i + slotCount <= count; ++i) {
    float sum = 0.0f;
    for (size_t j = 0; j < slotCount; ++j) {
      sum += daySlots[i + j].penceIncVat;
    }
    const float avg = sum / static_cast<float>(slotCount);
    if (!found || avg < bestAvg) {
      found = true;
      bestAvg = avg;
      best = &daySlots[i];
    }
  }

  if (!found) {
    return false;
  }

  *outStart = best;
  *outAvg = bestAvg;
  return true;
}
