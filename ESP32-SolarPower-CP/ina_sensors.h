#pragma once

#include <Arduino.h>

#include "wifi_service.h"

struct InaReading {
  bool ok = false;
  float busVoltageV = 0.0f;
  float shuntVoltageMv = 0.0f;
  float loadVoltageV = 0.0f;
  float currentMa = 0.0f;
  float powerMw = 0.0f;
};

class InaSensors {
public:
  void applyConfig(const DeviceConfig& config);
  void begin();
  void refreshNow();
  void update(uint32_t nowMs);
  void setSampleIntervalMs(uint32_t sampleIntervalMs);

  const InaReading& solar() const;
  const InaReading& battery() const;
  const InaReading& load() const;
  bool allHealthy() const;
  bool anyHealthy() const;
  bool loadHealthy() const;
  uint32_t lastReadMs() const;

private:
  InaReading solar_;
  InaReading battery_;
  InaReading load_;
  bool solarAvailable_ = false;
  bool batteryAvailable_ = false;
  bool loadAvailable_ = false;
  uint32_t lastReadMs_ = 0;
  uint32_t sampleIntervalMs_ = 1000;
  float solarShuntMilliOhms_ = 0.0f;
  float batteryShuntMilliOhms_ = 0.0f;
  float loadShuntMilliOhms_ = 0.0f;

  void readAll_();
  void resetReading_(InaReading& reading);
};

