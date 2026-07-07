#pragma once

#include <Arduino.h>
#include "firmware_version.h"

namespace Config {

constexpr uint32_t kSerialBaudRate = 115200;

// ESP32-S3 target wiring:
// INA3221 @0x40 with CH1=Solar (5 mOhm), CH2=Battery (5 mOhm), CH3=Load (R010 = 10 mOhm)
constexpr uint8_t kI2cSdaPin = 21;
constexpr uint8_t kI2cSclPin = 20;
constexpr uint8_t kIna3221Address = 0x40;
constexpr uint16_t kIna3221ConfigValue = 0x7127;
constexpr float kSolarShuntMilliOhms = 5.0f;
constexpr float kBatteryShuntMilliOhms = 5.0f;
constexpr float kLoadShuntMilliOhms = 10.0f;
constexpr float kMinShuntMilliOhms = 0.1f;
constexpr float kMaxShuntMilliOhms = 1000.0f;

constexpr char kDefaultWifiSsid[] = "HardwareTest";
constexpr char kDefaultWifiPass[] = "jayaabadi100";

// Dedicated PWM output for LT3478 LED dim/control.
constexpr uint8_t kLedPwmPin = 18;
constexpr uint8_t kLedPwmChannel = 0;
constexpr uint8_t kLedPwmResolutionBits = 12;
constexpr uint32_t kLedPwmDefaultFrequencyHz = 100;
constexpr uint32_t kLedPwmMinFrequencyHz = 100;
constexpr uint32_t kLedPwmMaxFrequencyHz = 20000;
constexpr uint32_t kLedPwmDefaultDutyPercent = 0;
constexpr uint32_t kLedPwmMinDutyPercent = 0;
constexpr uint32_t kLedPwmMaxDutyPercent = 100;
constexpr bool kLedPwmDefaultEnabled = false;
constexpr bool kLedPwmDefaultInverted = true;
constexpr bool kLedPwmFlashDefaultEnabled = false;
constexpr uint32_t kLedPwmFlashDefaultOnMs = 100;
constexpr uint32_t kLedPwmFlashMinOnMs = 1;
constexpr uint32_t kLedPwmFlashMaxOnMs = 60000;
constexpr uint32_t kLedPwmFlashDefaultPeriodMs = 500;
constexpr uint32_t kLedPwmFlashMinPeriodMs = 1;
constexpr uint32_t kLedPwmFlashMaxPeriodMs = 60000;

constexpr uint32_t kDefaultSampleIntervalMs = 1000;
constexpr uint32_t kMinSampleIntervalMs = 200;
constexpr uint32_t kMaxSampleIntervalMs = 60000;
constexpr uint32_t kSerialReportIntervalMs = 1000;
constexpr uint32_t kWifiConnectTimeoutMs = 15000;
constexpr uint32_t kRebootDelayMs = 1200;
constexpr uint16_t kArduinoOtaPort = 3232;

constexpr size_t kHistoryCapacity = 120;
constexpr size_t kBackendQueueCapacity = 32;
constexpr size_t kBackendBatchMaxSamples = 8;

constexpr uint32_t kBackendTelemetryAttemptMs = 5000;
constexpr uint32_t kBackendHeartbeatIntervalMs = 15000;
constexpr uint32_t kBackendConnectTimeoutMs = 350;
constexpr uint32_t kBackendResponseTimeoutMs = 650;
constexpr uint32_t kBackendRetryMinMs = 3000;
constexpr uint32_t kBackendRetryMaxMs = 30000;
constexpr uint32_t kBackendSenderRolloutVersion = 2;

constexpr uint16_t kBatteryCapacityMah = 2000;
constexpr uint32_t kBatteryProfileMinMv = 500;
constexpr uint32_t kBatteryProfileMaxMv = 60000;
constexpr uint32_t kDefaultBatteryFullMv = 4200;
constexpr uint32_t kDefaultBatteryEmptyMv = 3000;
constexpr char kDefaultBatteryProfileId[] = "liion_1s_4p20";

constexpr uint32_t kMinFlowThresholdMw = 0;
constexpr uint32_t kMaxFlowThresholdMw = 100000;
constexpr uint32_t kDefaultSolarActiveThresholdMw = 50;
constexpr uint32_t kDefaultLoadActiveThresholdMw = 25;
constexpr uint32_t kDefaultBatteryFlowThresholdMw = 20;

constexpr char kPrefsNamespace[] = "solar-mon";
constexpr char kDefaultApiBase[] = "http://127.0.0.1:8000/api/v1";
constexpr char kBackendSchemaVersion[] = "solar-backend-r01";
constexpr char kBackendSourceMode[] = "esp32";
constexpr char kBackendHeartbeatScenario[] = "live_device";

}  // namespace Config

