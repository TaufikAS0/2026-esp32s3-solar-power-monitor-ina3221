#include "battery_profile.h"

#include "config.h"

namespace {

constexpr BatteryProfilePreset kPresets[] = {
    {"liion_1s_4p20", "Li-ion 1S", 4200, 3600},
    {"lihv_1s_4p35", "LiHV 1S 4.35V", 4350, 3000},
    {"lihv_1s_4p40", "LiHV 1S 4.40V", 4400, 3000},
    {"liion_2s_8p40", "Li-ion 2S", 8400, 6000},
    {"liion_3s_12p60", "Li-ion 3S", 12600, 9000},
    {"liion_4s_16p80", "Li-ion 4S", 16800, 12000},
    {"lifepo4_1s_3p65", "LiFePO4 1S", 3650, 2500},
    {"lifepo4_4s_14p60", "LiFePO4 12V (4S)", 14600, 10000},
    {"lifepo4_8s_29p20", "LiFePO4 24V (8S)", 29200, 20000},
    {"lifepo4_16s_58p40", "LiFePO4 48V (16S)", 58400, 40000},
    {"leadacid_agm_gel_12v", "Lead-Acid / AGM / Gel 12V", 12700, 11000},
};

}  // namespace

const BatteryProfilePreset* batteryProfilePresets(size_t& count) {
  count = sizeof(kPresets) / sizeof(kPresets[0]);
  return kPresets;
}

const BatteryProfilePreset* findBatteryProfilePreset(const String& id) {
  size_t count = 0;
  const BatteryProfilePreset* presets = batteryProfilePresets(count);
  for (size_t index = 0; index < count; ++index) {
    if (id == presets[index].id) {
      return &presets[index];
    }
  }
  return nullptr;
}

String batteryProfileLabel(const String& id) {
  const BatteryProfilePreset* preset = findBatteryProfilePreset(id);
  if (preset != nullptr) {
    return String(preset->label);
  }
  return "Custom";
}

bool isBatteryProfileRangeValid(uint32_t fullMv, uint32_t emptyMv) {
  if (fullMv < Config::kBatteryProfileMinMv || fullMv > Config::kBatteryProfileMaxMv) {
    return false;
  }

  if (emptyMv < Config::kBatteryProfileMinMv || emptyMv > Config::kBatteryProfileMaxMv) {
    return false;
  }

  return fullMv > (emptyMv + 100U);
}

float computeBatteryPercent(float voltageV, uint32_t fullMv, uint32_t emptyMv) {
  if (!isBatteryProfileRangeValid(fullMv, emptyMv)) {
    return 0.0f;
  }

  const float fullV = static_cast<float>(fullMv) / 1000.0f;
  const float emptyV = static_cast<float>(emptyMv) / 1000.0f;
  float percent = ((voltageV - emptyV) / (fullV - emptyV)) * 100.0f;
  if (percent < 0.0f) {
    percent = 0.0f;
  }
  if (percent > 100.0f) {
    percent = 100.0f;
  }
  return percent;
}
