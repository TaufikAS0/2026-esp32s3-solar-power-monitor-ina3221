#pragma once

#include <Arduino.h>

struct BatteryProfilePreset {
  const char* id;
  const char* label;
  uint32_t fullMv;
  uint32_t emptyMv;
};

const BatteryProfilePreset* batteryProfilePresets(size_t& count);
const BatteryProfilePreset* findBatteryProfilePreset(const String& id);
String batteryProfileLabel(const String& id);
bool isBatteryProfileRangeValid(uint32_t fullMv, uint32_t emptyMv);
float computeBatteryPercent(float voltageV, uint32_t fullMv, uint32_t emptyMv);
