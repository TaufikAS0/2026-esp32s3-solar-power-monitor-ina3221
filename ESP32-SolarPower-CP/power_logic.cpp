#include "power_logic.h"

#include <math.h>

bool isSolarActive(const InaReading& solar, const DeviceConfig& config) {
  return solar.ok &&
         (solar.powerMw >= static_cast<float>(config.solarActiveThresholdMw));
}

bool isLoadActive(const InaReading& load, const DeviceConfig& config) {
  return load.ok &&
         (fabsf(load.powerMw) >= static_cast<float>(config.loadActiveThresholdMw));
}

BatteryDirection evaluateBatteryDirection(const InaReading& battery,
                                          const DeviceConfig& config) {
  if (!battery.ok) {
    return BatteryDirection::Unknown;
  }

  if (fabsf(battery.powerMw) < static_cast<float>(config.batteryFlowThresholdMw)) {
    return BatteryDirection::Idle;
  }

  if (battery.currentMa < 0.0f || battery.shuntVoltageMv < 0.0f) {
    return BatteryDirection::Charging;
  }

  if (battery.currentMa > 0.0f || battery.shuntVoltageMv > 0.0f) {
    return BatteryDirection::Discharging;
  }

  return BatteryDirection::Idle;
}

float computeBatteryPowerSignedMw(const InaReading& battery, const DeviceConfig& config) {
  const BatteryDirection direction = evaluateBatteryDirection(battery, config);
  if (direction == BatteryDirection::Charging) {
    return -fabsf(battery.powerMw);
  }

  if (direction == BatteryDirection::Discharging) {
    return fabsf(battery.powerMw);
  }

  return 0.0f;
}

VisualMode evaluateVisualMode(const InaReading& solar,
                              const InaReading& battery,
                              const DeviceConfig& config) {
  if (!solar.ok && !battery.ok) {
    return VisualMode::Error;
  }

  if (!solar.ok || !battery.ok) {
    return VisualMode::Degraded;
  }

  const bool solarActive = isSolarActive(solar, config);
  const BatteryDirection batteryDirection = evaluateBatteryDirection(battery, config);

  if (batteryDirection == BatteryDirection::Charging) {
    return VisualMode::Charging;
  }

  if (solarActive && batteryDirection == BatteryDirection::Discharging) {
    return VisualMode::Combo;
  }

  if (batteryDirection == BatteryDirection::Discharging) {
    return VisualMode::Discharging;
  }

  if (solarActive) {
    return VisualMode::SolarDirect;
  }

  return VisualMode::Idle;
}

const char* batteryDirectionToText(BatteryDirection direction) {
  switch (direction) {
    case BatteryDirection::Charging:
      return "charging";
    case BatteryDirection::Discharging:
      return "discharging";
    case BatteryDirection::Idle:
      return "idle";
    case BatteryDirection::Unknown:
    default:
      return "unknown";
  }
}

const char* visualModeToText(VisualMode mode) {
  switch (mode) {
    case VisualMode::Charging:
      return "charging";
    case VisualMode::Discharging:
      return "discharging";
    case VisualMode::SolarDirect:
      return "solar_direct";
    case VisualMode::Combo:
      return "combo";
    case VisualMode::Idle:
      return "idle";
    case VisualMode::Degraded:
      return "degraded";
    case VisualMode::Error:
    default:
      return "error";
  }
}

PowerSystemState evaluatePowerSystemState(const InaReading& solar,
                                          const InaReading& battery,
                                          const DeviceConfig& config) {
  if (!solar.ok && !battery.ok) {
    return PowerSystemState::SensorError;
  }

  const bool solarPresent = isSolarActive(solar, config);
  const BatteryDirection batteryDirection = evaluateBatteryDirection(battery, config);
  const bool batteryCharging = batteryDirection == BatteryDirection::Charging;
  const bool batteryDischarging = batteryDirection == BatteryDirection::Discharging;

  if (solarPresent && batteryCharging) {
    return PowerSystemState::SolarCharging;
  }

  if (solarPresent && batteryDischarging) {
    return PowerSystemState::SolarAndBattery;
  }

  if (solarPresent) {
    return PowerSystemState::SolarDirect;
  }

  if (batteryDischarging) {
    return PowerSystemState::BatteryOnly;
  }

  return PowerSystemState::Idle;
}

