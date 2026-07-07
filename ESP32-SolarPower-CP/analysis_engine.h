#pragma once

#include <Arduino.h>

#include "ina_sensors.h"
#include "wifi_service.h"

enum class AnalysisMode : uint8_t {
  Unavailable,
  SolarInput,
  BatteryInput,
  MixedInput
};

struct AnalysisSnapshot {
  AnalysisMode mode = AnalysisMode::Unavailable;
  float pLossMw = 0.0f;
  float efficiencyPct = 0.0f;
  float pctLoad = 0.0f;
  float pctBat = 0.0f;
  float pctLoss = 0.0f;
  float inputPowerMw = 0.0f;
  float usefulOutputMw = 0.0f;
  float cRate = 0.0f;
  float estFullH = 0.0f;
  float estRuntimeH = 0.0f;
  float sessionSolarWh = 0.0f;
  float sessionLoadWh = 0.0f;
  float sessionBatInWh = 0.0f;
  float sessionBatOutWh = 0.0f;
  float sessionLossWh = 0.0f;
  uint32_t sessionStartMs = 0;
  uint32_t sessionDurationMs = 0;
  bool balanceValid = false;
  bool batteryEstimateValid = false;
};

const char* analysisModeToText(AnalysisMode mode);
void analysisInit(AnalysisSnapshot& data, uint32_t nowMs);
void analysisResetSession(AnalysisSnapshot& data, uint32_t nowMs);
void analysisUpdate(const InaReading& solar,
                    const InaReading& battery,
                    const InaReading& load,
                    const DeviceConfig& config,
                    uint32_t nowMs,
                    AnalysisSnapshot& data);
