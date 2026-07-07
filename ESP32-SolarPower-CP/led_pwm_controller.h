#pragma once

#include <Arduino.h>

#include "config.h"
#include "wifi_service.h"

struct LedPwmRuntime {
  bool attached = false;
  bool enabled = Config::kLedPwmDefaultEnabled;
  bool inverted = Config::kLedPwmDefaultInverted;
  uint8_t pin = Config::kLedPwmPin;
  uint8_t channel = Config::kLedPwmChannel;
  uint8_t resolutionBits = Config::kLedPwmResolutionBits;
  uint32_t frequencyHz = Config::kLedPwmDefaultFrequencyHz;
  uint32_t dutyPercent = Config::kLedPwmDefaultDutyPercent;
  bool flashEnabled = Config::kLedPwmFlashDefaultEnabled;
  uint32_t flashOnMs = Config::kLedPwmFlashDefaultOnMs;
  uint32_t flashPeriodMs = Config::kLedPwmFlashDefaultPeriodMs;
  bool brightnessActive = false;
  bool flashOutputOn = false;
  uint32_t flashCycleMs = 0;
  uint32_t dutyRaw = 0;
  uint32_t dutyRawMax = 0;
  float signalDutyPercent = 0.0f;
};

class LedPwmController {
public:
  bool begin(const DeviceConfig& config);
  bool applyConfig(const DeviceConfig& config);
  void update(uint32_t nowMs);

  const LedPwmRuntime& runtime() const;

private:
  LedPwmRuntime runtime_;
  uint32_t flashCycleStartedMs_ = 0;

  uint32_t maxDuty_() const;
  void writeOutput_(bool brightnessActive);
};
