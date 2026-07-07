#pragma once

#include <Arduino.h>

enum class PowerSystemState : uint8_t {
  SolarCharging,
  SolarDirect,
  SolarAndBattery,
  BatteryOnly,
  Idle,
  SensorError
};

const char* powerSystemStateToText(PowerSystemState state);
const char* powerSystemStateToLabel(PowerSystemState state);

