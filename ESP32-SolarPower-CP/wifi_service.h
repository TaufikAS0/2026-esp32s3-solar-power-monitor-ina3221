#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include "config.h"

struct DeviceConfig {
  String wifiSsid;
  String wifiPass;
  String deviceId;
  String lineId;
  uint32_t sampleIntervalMs = 1000;
  bool serverEnabled = true;
  bool otaEnabled = false;
  String apiBase;
  String apiKey;
  String batteryProfileId;
  uint32_t batteryFullMv = 4200;
  uint32_t batteryEmptyMv = 3000;
  uint32_t solarActiveThresholdMw = Config::kDefaultSolarActiveThresholdMw;
  uint32_t loadActiveThresholdMw = Config::kDefaultLoadActiveThresholdMw;
  uint32_t batteryFlowThresholdMw = Config::kDefaultBatteryFlowThresholdMw;
  float solarShuntMilliOhms = Config::kSolarShuntMilliOhms;
  float batteryShuntMilliOhms = Config::kBatteryShuntMilliOhms;
  float loadShuntMilliOhms = Config::kLoadShuntMilliOhms;
  bool ledPwmEnabled = Config::kLedPwmDefaultEnabled;
  bool ledPwmInverted = Config::kLedPwmDefaultInverted;
  uint32_t ledPwmFrequencyHz = Config::kLedPwmDefaultFrequencyHz;
  uint32_t ledPwmDutyPercent = Config::kLedPwmDefaultDutyPercent;
  bool ledPwmFlashEnabled = Config::kLedPwmFlashDefaultEnabled;
  uint32_t ledPwmFlashOnMs = Config::kLedPwmFlashDefaultOnMs;
  uint32_t ledPwmFlashPeriodMs = Config::kLedPwmFlashDefaultPeriodMs;
};

class WifiService {
public:
  void begin();
  void update(uint32_t nowMs);

  bool isApMode() const;
  bool isStaConnected() const;
  bool hasWifiCredentials() const;
  const DeviceConfig& config() const;

  String ipAddress() const;
  String wifiModeName() const;
  String accessPointSsid() const;

  bool saveWifiConfig(const String& ssid, const String& password);
  bool saveDeviceConfig(const String& deviceId, const String& lineId);
  bool saveOtaConfig(bool enabled);
  bool saveRuntimeConfig(uint32_t sampleIntervalMs);
  bool saveBatteryConfig(const String& profileId, uint32_t fullMv, uint32_t emptyMv);
  bool saveFlowConfig(uint32_t solarActiveThresholdMw,
                      uint32_t loadActiveThresholdMw,
                      uint32_t batteryFlowThresholdMw);
  bool saveInaConfig(float solarShuntMilliOhms,
                     float batteryShuntMilliOhms,
                     float loadShuntMilliOhms);
  bool applyLedPwmConfig(bool enabled,
                         uint32_t frequencyHz,
                         uint32_t dutyPercent,
                         bool inverted,
                         bool flashEnabled,
                         uint32_t flashOnMs,
                         uint32_t flashPeriodMs);
  bool saveLedPwmConfig(bool enabled,
                        uint32_t frequencyHz,
                        uint32_t dutyPercent,
                        bool inverted,
                        bool flashEnabled,
                        uint32_t flashOnMs,
                        uint32_t flashPeriodMs);
  bool saveBackendConfig(bool enabled,
                         const String& apiBase,
                         const String& apiKey,
                         bool clearApiKey);
  void scheduleReboot(uint32_t nowMs, uint32_t delayMs);

private:
  Preferences preferences_;
  DeviceConfig config_;

  bool apMode_ = false;
  bool rebootScheduled_ = false;
  uint32_t wifiConnectStartedMs_ = 0;
  uint32_t rebootAtMs_ = 0;
  String accessPointSsid_;

  void loadConfig_();
  void saveString_(const char* key, const String& value);
  void saveBool_(const char* key, bool value);
  void saveUInt_(const char* key, uint32_t value);
  void saveFloat_(const char* key, float value);
  void startStation_();
  void startAccessPoint_();
  String buildDefaultDeviceId_() const;
  String buildAccessPointSsid_() const;
  String normalizeApiBase_(String value) const;
  uint32_t normalizeSampleIntervalMs_(uint32_t value) const;
  uint32_t normalizeFlowThresholdMw_(uint32_t value) const;
  uint32_t normalizeLedPwmFrequencyHz_(uint32_t value) const;
  uint32_t normalizeLedPwmDutyPercent_(uint32_t value) const;
  uint32_t normalizeLedPwmFlashOnMs_(uint32_t value) const;
  uint32_t normalizeLedPwmFlashPeriodMs_(uint32_t value) const;
  float normalizeShuntMilliOhms_(float value, float fallbackValue) const;
};

