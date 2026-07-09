#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include "analysis_engine.h"
#include "ina_sensors.h"
#include "power_logic.h"
#include "system_state.h"
#include "wifi_service.h"

struct TelemetryFrame {
  uint32_t sequence = 0;
  uint32_t timestampMs = 0;
  InaReading solar;
  InaReading battery;
  InaReading load;
  AnalysisSnapshot analysis;
  PowerSystemState state = PowerSystemState::SensorError;
  BatteryDirection batteryDirection = BatteryDirection::Unknown;
  VisualMode visualMode = VisualMode::Error;
  bool solarActive = false;
  bool loadActive = false;
  float batteryPowerSignedMw = 0.0f;
  bool batteryPercentValid = false;
  float batteryPercent = 0.0f;
  uint32_t sampleIntervalMs = 1000;
  uint32_t batteryFullMv = 4200;
  uint32_t batteryEmptyMv = Config::kDefaultBatteryEmptyMv;
  char deviceId[48] = {0};
  char lineId[48] = {0};
  char batteryProfileId[32] = {0};
};

void appendReading(JsonObject object, const InaReading& reading);
void appendAnalysis(JsonObject object, const AnalysisSnapshot& analysis);
void appendBatteryProfileFields(JsonObject object, const DeviceConfig& config);
void appendBatteryProfileFields(JsonObject object, const TelemetryFrame& frame);
String sensorHealthText(const InaReading& solar, const InaReading& battery, const InaReading& load);
String visualWarningText(const InaReading& solar, const InaReading& battery);
void captureTelemetryFrame(TelemetryFrame& frame,
                           uint32_t timestampMs,
                           const DeviceConfig& config,
                           const InaReading& solar,
                           const InaReading& battery,
                           const InaReading& load,
                           const AnalysisSnapshot& analysis,
                           PowerSystemState state);
void appendTelemetrySample(JsonObject object, const TelemetryFrame& frame);
