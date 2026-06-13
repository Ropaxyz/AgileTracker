#pragma once

#include "agile_store.h"

struct TariffMeta {
  float standingChargePencePerDay = 0.0f;
  bool fetchOk = false;
  char lastError[64] = {};
  time_t fetchedAtUtc = 0;
};

extern TariffMeta g_tariffMeta;

bool agileFetchRates(AgileStore &store);
bool agileFetchStandingCharge(TariffMeta &meta, const AgileStore &store);
void agileInitTime();
