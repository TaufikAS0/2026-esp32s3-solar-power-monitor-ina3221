#include "led_pwm_controller.h"

#include <esp32-hal-ledc.h>

bool LedPwmController::begin(const DeviceConfig& config) {
  runtime_.pin = Config::kLedPwmPin;
  runtime_.channel = Config::kLedPwmChannel;
  runtime_.resolutionBits = Config::kLedPwmResolutionBits;
  runtime_.dutyRawMax = maxDuty_();
  flashCycleStartedMs_ = millis();
  return applyConfig(config);
}

bool LedPwmController::applyConfig(const DeviceConfig& config) {
  runtime_.enabled = config.ledPwmEnabled;
  runtime_.inverted = config.ledPwmInverted;
  runtime_.frequencyHz = config.ledPwmFrequencyHz;
  runtime_.dutyPercent = config.ledPwmDutyPercent;
  runtime_.flashEnabled = config.ledPwmFlashEnabled;
  runtime_.flashOnMs = config.ledPwmFlashOnMs;
  runtime_.flashPeriodMs = config.ledPwmFlashPeriodMs;
  if (runtime_.flashOnMs > runtime_.flashPeriodMs) {
    runtime_.flashOnMs = runtime_.flashPeriodMs;
  }
  runtime_.dutyRawMax = maxDuty_();
  runtime_.flashCycleMs = 0;
  runtime_.brightnessActive = !runtime_.enabled;
  runtime_.flashOutputOn = false;
  flashCycleStartedMs_ = millis();

  ledcDetach(runtime_.pin);
  runtime_.attached =
      ledcAttachChannel(runtime_.pin,
                        runtime_.frequencyHz,
                        runtime_.resolutionBits,
                        runtime_.channel);
  if (!runtime_.attached) {
    runtime_.dutyRaw = 0;
    runtime_.signalDutyPercent = 0.0f;
    return false;
  }

  update(flashCycleStartedMs_);
  return true;
}

void LedPwmController::update(uint32_t nowMs) {
  if (!runtime_.attached) {
    runtime_.brightnessActive = false;
    runtime_.flashOutputOn = false;
    runtime_.flashCycleMs = 0;
    return;
  }

  bool brightnessActive = runtime_.enabled;
  runtime_.flashCycleMs = 0;
  runtime_.flashOutputOn = false;

  if (runtime_.enabled && runtime_.flashEnabled && runtime_.flashPeriodMs > 0) {
    runtime_.flashCycleMs = (nowMs - flashCycleStartedMs_) % runtime_.flashPeriodMs;
    runtime_.flashOutputOn = runtime_.flashCycleMs < runtime_.flashOnMs;
    brightnessActive = runtime_.flashOutputOn;
  }

  if (runtime_.brightnessActive == brightnessActive &&
      (!runtime_.flashEnabled || runtime_.flashOutputOn == brightnessActive)) {
    return;
  }

  writeOutput_(brightnessActive);
}

const LedPwmRuntime& LedPwmController::runtime() const {
  return runtime_;
}

uint32_t LedPwmController::maxDuty_() const {
  return (1UL << Config::kLedPwmResolutionBits) - 1UL;
}

void LedPwmController::writeOutput_(bool brightnessActive) {
  runtime_.brightnessActive = brightnessActive;
  runtime_.flashOutputOn = runtime_.flashEnabled && runtime_.enabled && brightnessActive;
  const uint32_t brightnessDuty =
      brightnessActive ? ((runtime_.dutyPercent * runtime_.dutyRawMax) / 100UL) : 0UL;
  runtime_.dutyRaw =
      runtime_.inverted ? (runtime_.dutyRawMax - brightnessDuty) : brightnessDuty;
  runtime_.signalDutyPercent =
      runtime_.dutyRawMax > 0
          ? (static_cast<float>(runtime_.dutyRaw) * 100.0f) /
                static_cast<float>(runtime_.dutyRawMax)
          : 0.0f;
  ledcWrite(runtime_.pin, runtime_.dutyRaw);
}
