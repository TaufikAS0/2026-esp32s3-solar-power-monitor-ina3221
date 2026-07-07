#pragma once

#include "ina_sensors.h"
#include "system_state.h"
#include "wifi_service.h"

enum class BatteryDirection : uint8_t {
  Charging,
  Discharging,
  Idle,
  Unknown
};

enum class VisualMode : uint8_t {
  Charging,
  Discharging,
  SolarDirect,
  Combo,
  Idle,
  Degraded,
  Error
};

PowerSystemState evaluatePowerSystemState(const InaReading& solar,
                                          const InaReading& battery,
                                          const DeviceConfig& config);
bool isSolarActive(const InaReading& solar, const DeviceConfig& config);
bool isLoadActive(const InaReading& load, const DeviceConfig& config);
BatteryDirection evaluateBatteryDirection(const InaReading& battery, const DeviceConfig& config);
float computeBatteryPowerSignedMw(const InaReading& battery, const DeviceConfig& config);
VisualMode evaluateVisualMode(const InaReading& solar,
                              const InaReading& battery,
                              const DeviceConfig& config);
const char* batteryDirectionToText(BatteryDirection direction);
const char* visualModeToText(VisualMode mode);

