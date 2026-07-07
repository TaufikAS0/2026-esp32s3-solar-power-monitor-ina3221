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

struct InaTimingProfile {
  uint32_t averagingSamples = Config::kDefaultInaAveragingSamples;
  uint32_t busConvTimeUs = Config::kDefaultInaBusConvTimeUs;
  uint32_t shuntConvTimeUs = Config::kDefaultInaShuntConvTimeUs;
  uint16_t configRegister = Config::kDefaultIna3221ConfigValue;
  uint32_t frameTimeUs = 0;
  float estimatedChannelRateHz = 0.0f;
};

uint32_t normalizeIna3221AveragingSamples(uint32_t value);
uint32_t normalizeIna3221ConversionTimeUs(uint32_t value);
uint16_t buildIna3221ConfigRegister(uint32_t averagingSamples,
                                    uint32_t busConvTimeUs,
                                    uint32_t shuntConvTimeUs);
InaTimingProfile describeIna3221Timing(uint32_t averagingSamples,
                                       uint32_t busConvTimeUs,
                                       uint32_t shuntConvTimeUs);

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
  uint32_t totalReadCount() const;
  float measuredReadRateHz() const;
  InaTimingProfile timingProfile() const;

private:
  InaReading solar_;
  InaReading battery_;
  InaReading load_;
  bool solarAvailable_ = false;
  bool batteryAvailable_ = false;
  bool loadAvailable_ = false;
  uint32_t lastReadMs_ = 0;
  uint32_t sampleIntervalMs_ = 1000;
  uint32_t totalReadCount_ = 0;
  uint32_t rateWindowStartMs_ = 0;
  uint32_t rateWindowReadCount_ = 0;
  float measuredReadRateHz_ = 0.0f;
  float solarShuntMilliOhms_ = 0.0f;
  float batteryShuntMilliOhms_ = 0.0f;
  float loadShuntMilliOhms_ = 0.0f;
  uint32_t averagingSamples_ = Config::kDefaultInaAveragingSamples;
  uint32_t busConvTimeUs_ = Config::kDefaultInaBusConvTimeUs;
  uint32_t shuntConvTimeUs_ = Config::kDefaultInaShuntConvTimeUs;
  uint16_t configRegister_ = Config::kDefaultIna3221ConfigValue;
  bool configured_ = false;

  void readAll_();
  void writeConfig_();
  void noteRead_(uint32_t nowMs);
  void resetReading_(InaReading& reading);
};

