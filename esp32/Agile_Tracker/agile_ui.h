#pragma once

#include "agile_store.h"

void agileUiInit();
void agileUiRelayout();
void agileUiRefresh(const AgileStore &store);
void agileUiTick(const AgileStore &store);
void agileUiSetBrightness(uint8_t brightness);
void agileUiNextTab();
void agileUiPrevTab();
void agileUiBrightnessStep(int delta);
