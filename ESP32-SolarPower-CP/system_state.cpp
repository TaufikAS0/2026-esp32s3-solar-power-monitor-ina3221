#include "system_state.h"

const char* powerSystemStateToText(PowerSystemState state) {
  switch (state) {
    case PowerSystemState::SolarCharging:
      return "SOLAR_CHARGING";
    case PowerSystemState::SolarDirect:
      return "SOLAR_DIRECT";
    case PowerSystemState::SolarAndBattery:
      return "SOLAR_AND_BATTERY";
    case PowerSystemState::BatteryOnly:
      return "BATTERY_ONLY";
    case PowerSystemState::Idle:
      return "IDLE";
    case PowerSystemState::SensorError:
    default:
      return "SENSOR_ERROR";
  }
}

const char* powerSystemStateToLabel(PowerSystemState state) {
  switch (state) {
    case PowerSystemState::SolarCharging:
      return "Solar charging battery";
    case PowerSystemState::SolarDirect:
      return "Solar powering load directly";
    case PowerSystemState::SolarAndBattery:
      return "Solar and battery supplying load";
    case PowerSystemState::BatteryOnly:
      return "Battery supplying load";
    case PowerSystemState::Idle:
      return "System idle";
    case PowerSystemState::SensorError:
    default:
      return "Sensor read error";
  }
}

