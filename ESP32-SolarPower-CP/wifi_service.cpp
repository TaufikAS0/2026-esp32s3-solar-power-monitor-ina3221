#include "wifi_service.h"

#include <math.h>
#include <WiFi.h>

#include "battery_profile.h"
#include "config.h"
#include "ina_sensors.h"
#include "sampling_profile.h"

void WifiService::begin() {
  preferences_.begin(Config::kPrefsNamespace, false);
  loadConfig_();
  runtime_ = WifiRuntime{};
  lastWifiStatus_ = 0U;
  WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) { handleWifiEvent_(event, info); });

  if (config_.wifiSsid.isEmpty()) {
    config_.wifiSsid = Config::kDefaultWifiSsid;
    config_.wifiPass = Config::kDefaultWifiPass;
    saveString_("wifi_ssid", config_.wifiSsid);
    saveString_("wifi_pass", config_.wifiPass);
  }

  if (config_.deviceId.isEmpty()) {
    config_.deviceId = buildDefaultDeviceId_();
    saveString_("device_id", config_.deviceId);
  }

  if (config_.apiBase.isEmpty()) {
    config_.apiBase = Config::kDefaultApiBase;
    saveString_("api_base", config_.apiBase);
  }

  startStation_();
}

void WifiService::update(uint32_t nowMs) {
  if (rebootScheduled_ && static_cast<int32_t>(nowMs - rebootAtMs_) >= 0) {
    ESP.restart();
  }

  const uint8_t wifiStatus = static_cast<uint8_t>(WiFi.status());
  if (!apMode_) {
    if (wifiStatus == static_cast<uint8_t>(WL_CONNECTED)) {
      if (lastWifiStatus_ != wifiStatus) {
        runtime_.staConnectedOnce = true;
        runtime_.lastStaConnectMs = nowMs;
      }
      lastWifiStatus_ = wifiStatus;
      return;
    }
  }

  lastWifiStatus_ = wifiStatus;

  if (apMode_) {
    return;
  }

  if (!runtime_.staConnectedOnce) {
    if (nowMs - wifiConnectStartedMs_ >= Config::kWifiConnectTimeoutMs) {
      startAccessPoint_();
    }
    return;
  }

  if (runtime_.lastStaReconnectAttemptMs == 0U ||
      nowMs - runtime_.lastStaReconnectAttemptMs >= Config::kWifiReconnectIntervalMs) {
    runtime_.lastStaReconnectAttemptMs = nowMs;
    ++runtime_.staReconnectAttempts;
    WiFi.reconnect();
  }
}

bool WifiService::isApMode() const {
  return apMode_;
}

bool WifiService::isStaConnected() const {
  return !apMode_ && WiFi.status() == WL_CONNECTED;
}

bool WifiService::hasWifiCredentials() const {
  return !config_.wifiSsid.isEmpty();
}

const DeviceConfig& WifiService::config() const {
  return config_;
}

const WifiRuntime& WifiService::runtime() const {
  return runtime_;
}

const char* WifiService::lastWifiEventName() const {
  return WiFi.eventName(static_cast<arduino_event_id_t>(runtime_.lastWifiEventId));
}

const char* WifiService::lastStaDisconnectReasonName() const {
  if (runtime_.lastStaDisconnectReason == 0U) {
    return "NONE";
  }
  return WiFi.disconnectReasonName(
      static_cast<wifi_err_reason_t>(runtime_.lastStaDisconnectReason));
}

String WifiService::ipAddress() const {
  if (apMode_) {
    return WiFi.softAPIP().toString();
  }

  if (WiFi.status() == WL_CONNECTED) {
    return WiFi.localIP().toString();
  }

  return "0.0.0.0";
}

String WifiService::wifiModeName() const {
  return apMode_ ? "ap" : "sta";
}

String WifiService::accessPointSsid() const {
  return accessPointSsid_;
}

bool WifiService::saveWifiConfig(const String& ssid, const String& password) {
  config_.wifiSsid = ssid;
  config_.wifiPass = password;
  saveString_("wifi_ssid", config_.wifiSsid);
  saveString_("wifi_pass", config_.wifiPass);
  return true;
}

bool WifiService::saveDeviceConfig(const String& deviceId, const String& lineId) {
  config_.deviceId = deviceId.isEmpty() ? buildDefaultDeviceId_() : deviceId;
  config_.lineId = lineId;
  saveString_("device_id", config_.deviceId);
  saveString_("line_id", config_.lineId);
  return true;
}

bool WifiService::saveOtaConfig(bool enabled) {
  config_.otaEnabled = enabled;
  saveBool_("ota_enabled", config_.otaEnabled);
  return true;
}

bool WifiService::saveRuntimeConfig(uint32_t sampleIntervalMs,
                                    uint32_t sensorPollIntervalMs,
                                    uint32_t historyIntervalMs,
                                    uint32_t chartPointLimit,
                                    uint32_t inaAveragingSamples,
                                    uint32_t inaBusConvTimeUs,
                                    uint32_t inaShuntConvTimeUs) {
  config_.sampleIntervalMs = normalizeSampleIntervalMs_(sampleIntervalMs);
  config_.sensorPollIntervalMs = normalizeSensorPollIntervalMs_(sensorPollIntervalMs);
  config_.historyIntervalMs = normalizeHistoryIntervalMs_(historyIntervalMs);
  config_.chartPointLimit = normalizeChartPointLimit_(chartPointLimit);
  config_.inaAveragingSamples = normalizeInaAveragingSamples_(inaAveragingSamples);
  config_.inaBusConvTimeUs = normalizeInaConversionTimeUs_(inaBusConvTimeUs);
  config_.inaShuntConvTimeUs = normalizeInaConversionTimeUs_(inaShuntConvTimeUs);
  saveUInt_("sample_interval_ms", config_.sampleIntervalMs);
  saveUInt_("sensor_poll_ms", config_.sensorPollIntervalMs);
  saveUInt_("history_interval_ms", config_.historyIntervalMs);
  saveUInt_("chart_point_limit", config_.chartPointLimit);
  saveUInt_("ina_avg_samples", config_.inaAveragingSamples);
  saveUInt_("ina_bus_conv_us", config_.inaBusConvTimeUs);
  saveUInt_("ina_shunt_conv_us", config_.inaShuntConvTimeUs);
  return true;
}

bool WifiService::saveBatteryConfig(const String& profileId, uint32_t fullMv, uint32_t emptyMv) {
  if (!isBatteryProfileRangeValid(fullMv, emptyMv)) {
    return false;
  }

  config_.batteryProfileId =
      profileId.isEmpty() ? String(Config::kDefaultBatteryProfileId) : profileId;
  config_.batteryFullMv = fullMv;
  config_.batteryEmptyMv = emptyMv;
  saveString_("battery_profile_id", config_.batteryProfileId);
  saveUInt_("battery_full_mv", config_.batteryFullMv);
  saveUInt_("battery_empty_mv", config_.batteryEmptyMv);
  return true;
}

bool WifiService::saveFlowConfig(uint32_t solarActiveThresholdMw,
                                 uint32_t loadActiveThresholdMw,
                                 uint32_t batteryFlowThresholdMw) {
  config_.solarActiveThresholdMw = normalizeFlowThresholdMw_(solarActiveThresholdMw);
  config_.loadActiveThresholdMw = normalizeFlowThresholdMw_(loadActiveThresholdMw);
  config_.batteryFlowThresholdMw = normalizeFlowThresholdMw_(batteryFlowThresholdMw);
  saveUInt_("flow_solar_mw", config_.solarActiveThresholdMw);
  saveUInt_("flow_load_mw", config_.loadActiveThresholdMw);
  saveUInt_("flow_battery_mw", config_.batteryFlowThresholdMw);
  return true;
}

bool WifiService::saveInaConfig(float solarShuntMilliOhms,
                                float batteryShuntMilliOhms,
                                float loadShuntMilliOhms) {
  config_.solarShuntMilliOhms =
      normalizeShuntMilliOhms_(solarShuntMilliOhms, Config::kSolarShuntMilliOhms);
  config_.batteryShuntMilliOhms =
      normalizeShuntMilliOhms_(batteryShuntMilliOhms, Config::kBatteryShuntMilliOhms);
  config_.loadShuntMilliOhms =
      normalizeShuntMilliOhms_(loadShuntMilliOhms, Config::kLoadShuntMilliOhms);
  saveFloat_("shunt_solar_mo", config_.solarShuntMilliOhms);
  saveFloat_("shunt_batt_mo", config_.batteryShuntMilliOhms);
  saveFloat_("shunt_load_mo", config_.loadShuntMilliOhms);
  return true;
}

bool WifiService::applyLedPwmConfig(bool enabled,
                                    uint32_t frequencyHz,
                                    uint32_t dutyPercent,
                                    bool inverted,
                                    bool flashEnabled,
                                    uint32_t flashOnMs,
                                    uint32_t flashPeriodMs) {
  config_.ledPwmEnabled = enabled;
  config_.ledPwmInverted = inverted;
  config_.ledPwmFrequencyHz = normalizeLedPwmFrequencyHz_(frequencyHz);
  config_.ledPwmDutyPercent = normalizeLedPwmDutyPercent_(dutyPercent);
  config_.ledPwmFlashEnabled = flashEnabled;
  config_.ledPwmFlashPeriodMs = normalizeLedPwmFlashPeriodMs_(flashPeriodMs);
  config_.ledPwmFlashOnMs = normalizeLedPwmFlashOnMs_(flashOnMs);
  if (config_.ledPwmFlashOnMs > config_.ledPwmFlashPeriodMs) {
    config_.ledPwmFlashOnMs = config_.ledPwmFlashPeriodMs;
  }
  return true;
}

bool WifiService::saveLedPwmConfig(bool enabled,
                                   uint32_t frequencyHz,
                                   uint32_t dutyPercent,
                                   bool inverted,
                                   bool flashEnabled,
                                   uint32_t flashOnMs,
                                   uint32_t flashPeriodMs) {
  applyLedPwmConfig(enabled,
                    frequencyHz,
                    dutyPercent,
                    inverted,
                    flashEnabled,
                    flashOnMs,
                    flashPeriodMs);
  saveBool_("led_pwm_en", config_.ledPwmEnabled);
  saveBool_("led_pwm_inv", config_.ledPwmInverted);
  saveUInt_("led_pwm_hz", config_.ledPwmFrequencyHz);
  saveUInt_("led_pwm_pct", config_.ledPwmDutyPercent);
  saveBool_("led_pwm_flash_en", config_.ledPwmFlashEnabled);
  saveUInt_("led_pwm_flash_on", config_.ledPwmFlashOnMs);
  saveUInt_("led_pwm_flash_period", config_.ledPwmFlashPeriodMs);
  return true;
}

bool WifiService::saveControlPinScheduleConfig(bool enabled,
                                               uint32_t offHour,
                                               uint32_t offMinute) {
  config_.controlPinScheduleEnabled = enabled;
  config_.controlPinOffHour = normalizeControlPinOffHour_(offHour);
  config_.controlPinOffMinute = normalizeControlPinOffMinute_(offMinute);
  saveBool_("ctrl_pin_sched_en", config_.controlPinScheduleEnabled);
  saveUInt_("ctrl_pin_off_h", config_.controlPinOffHour);
  saveUInt_("ctrl_pin_off_m", config_.controlPinOffMinute);
  return true;
}

bool WifiService::saveBackendConfig(bool enabled,
                                    const String& apiBase,
                                    const String& apiKey,
                                    bool clearApiKey) {
  config_.serverEnabled = enabled;
  config_.apiBase = normalizeApiBase_(apiBase);
  saveBool_("server_enabled", config_.serverEnabled);
  saveString_("api_base", config_.apiBase);

  if (clearApiKey) {
    config_.apiKey = "";
    saveString_("api_key", config_.apiKey);
    return true;
  }

  if (!apiKey.isEmpty()) {
    config_.apiKey = apiKey;
    saveString_("api_key", config_.apiKey);
    return true;
  }

  return true;
}

void WifiService::scheduleReboot(uint32_t nowMs, uint32_t delayMs) {
  rebootScheduled_ = true;
  rebootAtMs_ = nowMs + delayMs;
}

void WifiService::loadConfig_() {
  const uint32_t storedLedPwmFrequencyHz = preferences_.getUInt("led_pwm_hz", UINT32_MAX);
  const uint32_t storedLedPwmDutyPercent = preferences_.getUInt("led_pwm_pct", UINT32_MAX);
  const bool storedLedPwmEnabled =
      preferences_.getBool("led_pwm_en", Config::kLedPwmDefaultEnabled);
  const uint32_t storedLedPwmFlashOnMs =
      preferences_.getUInt("led_pwm_flash_on", UINT32_MAX);
  const uint32_t storedLedPwmFlashPeriodMs =
      preferences_.getUInt("led_pwm_flash_period", UINT32_MAX);

  config_.wifiSsid = preferences_.getString("wifi_ssid", "");
  config_.wifiPass = preferences_.getString("wifi_pass", "");
  config_.deviceId = preferences_.getString("device_id", "");
  config_.lineId = preferences_.getString("line_id", "");
  config_.sensorPollIntervalMs =
      normalizeSensorPollIntervalMs_(preferences_.getUInt("sensor_poll_ms",
                                                          Config::kDefaultSensorPollIntervalMs));
  config_.sampleIntervalMs =
      normalizeSampleIntervalMs_(preferences_.getUInt("sample_interval_ms",
                                                      Config::kDefaultSampleIntervalMs));
  config_.historyIntervalMs =
      normalizeHistoryIntervalMs_(preferences_.getUInt("history_interval_ms",
                                                       Config::kDefaultHistoryIntervalMs));
  config_.chartPointLimit =
      normalizeChartPointLimit_(preferences_.getUInt("chart_point_limit",
                                                     Config::kDefaultChartPointLimit));
  config_.inaAveragingSamples =
      normalizeInaAveragingSamples_(preferences_.getUInt("ina_avg_samples",
                                                         Config::kDefaultInaAveragingSamples));
  config_.inaBusConvTimeUs =
      normalizeInaConversionTimeUs_(preferences_.getUInt("ina_bus_conv_us",
                                                         Config::kDefaultInaBusConvTimeUs));
  config_.inaShuntConvTimeUs =
      normalizeInaConversionTimeUs_(preferences_.getUInt("ina_shunt_conv_us",
                                                         Config::kDefaultInaShuntConvTimeUs));
  config_.serverEnabled = preferences_.getBool("server_enabled", true);
  config_.otaEnabled = preferences_.getBool("ota_enabled", false);
  config_.apiBase = preferences_.getString("api_base", "");
  config_.apiKey = preferences_.getString("api_key", "");
  config_.batteryProfileId =
      preferences_.getString("battery_profile_id", Config::kDefaultBatteryProfileId);
  config_.batteryFullMv =
      preferences_.getUInt("battery_full_mv", Config::kDefaultBatteryFullMv);
  config_.batteryEmptyMv =
      preferences_.getUInt("battery_empty_mv", Config::kDefaultBatteryEmptyMv);
  config_.solarActiveThresholdMw =
      normalizeFlowThresholdMw_(preferences_.getUInt("flow_solar_mw",
                                                     Config::kDefaultSolarActiveThresholdMw));
  config_.loadActiveThresholdMw =
      normalizeFlowThresholdMw_(preferences_.getUInt("flow_load_mw",
                                                     Config::kDefaultLoadActiveThresholdMw));
  config_.batteryFlowThresholdMw =
      normalizeFlowThresholdMw_(preferences_.getUInt("flow_battery_mw",
                                                     Config::kDefaultBatteryFlowThresholdMw));
  config_.solarShuntMilliOhms =
      normalizeShuntMilliOhms_(preferences_.getFloat("shunt_solar_mo",
                                                     Config::kSolarShuntMilliOhms),
                               Config::kSolarShuntMilliOhms);
  config_.batteryShuntMilliOhms =
      normalizeShuntMilliOhms_(preferences_.getFloat("shunt_batt_mo",
                                                     Config::kBatteryShuntMilliOhms),
                               Config::kBatteryShuntMilliOhms);
  config_.loadShuntMilliOhms =
      normalizeShuntMilliOhms_(preferences_.getFloat("shunt_load_mo",
                                                     Config::kLoadShuntMilliOhms),
                               Config::kLoadShuntMilliOhms);
  config_.ledPwmEnabled = storedLedPwmEnabled;
  config_.ledPwmInverted =
      preferences_.getBool("led_pwm_inv", Config::kLedPwmDefaultInverted);
  config_.ledPwmFrequencyHz =
      normalizeLedPwmFrequencyHz_(storedLedPwmFrequencyHz == UINT32_MAX
                                      ? Config::kLedPwmDefaultFrequencyHz
                                      : storedLedPwmFrequencyHz);
  config_.ledPwmDutyPercent =
      normalizeLedPwmDutyPercent_(storedLedPwmDutyPercent == UINT32_MAX
                                      ? Config::kLedPwmDefaultDutyPercent
                                      : storedLedPwmDutyPercent);
  config_.ledPwmFlashEnabled =
      preferences_.getBool("led_pwm_flash_en", Config::kLedPwmFlashDefaultEnabled);
  config_.ledPwmFlashOnMs =
      normalizeLedPwmFlashOnMs_(storedLedPwmFlashOnMs == UINT32_MAX
                                    ? Config::kLedPwmFlashDefaultOnMs
                                    : storedLedPwmFlashOnMs);
  config_.ledPwmFlashPeriodMs =
      normalizeLedPwmFlashPeriodMs_(storedLedPwmFlashPeriodMs == UINT32_MAX
                                        ? Config::kLedPwmFlashDefaultPeriodMs
                                        : storedLedPwmFlashPeriodMs);
  config_.controlPinScheduleEnabled =
      preferences_.getBool("ctrl_pin_sched_en", Config::kControlPinScheduleDefaultEnabled);
  config_.controlPinOffHour =
      normalizeControlPinOffHour_(preferences_.getUInt("ctrl_pin_off_h",
                                                       Config::kControlPinScheduleDefaultOffHour));
  config_.controlPinOffMinute =
      normalizeControlPinOffMinute_(preferences_.getUInt("ctrl_pin_off_m",
                                                         Config::kControlPinScheduleDefaultOffMinute));
  if (config_.ledPwmFlashOnMs > config_.ledPwmFlashPeriodMs) {
    config_.ledPwmFlashOnMs = config_.ledPwmFlashPeriodMs;
  }

  if (config_.batteryProfileId.isEmpty()) {
    config_.batteryProfileId = Config::kDefaultBatteryProfileId;
  }

  if (!isBatteryProfileRangeValid(config_.batteryFullMv, config_.batteryEmptyMv)) {
    config_.batteryFullMv = Config::kDefaultBatteryFullMv;
    config_.batteryEmptyMv = Config::kDefaultBatteryEmptyMv;
    saveUInt_("battery_full_mv", config_.batteryFullMv);
    saveUInt_("battery_empty_mv", config_.batteryEmptyMv);
  }

  config_.apiBase = normalizeApiBase_(config_.apiBase);
  saveString_("api_base", config_.apiBase);

  const uint32_t ledPwmRolloutVersion = preferences_.getUInt("led_pwm_rollout_v1", 0);
  const bool looksLikeLegacyLedPwmDefault =
      storedLedPwmFrequencyHz == 10000U &&
      !storedLedPwmEnabled &&
      (storedLedPwmDutyPercent == UINT32_MAX ||
       storedLedPwmDutyPercent == Config::kLedPwmDefaultDutyPercent);
  if (ledPwmRolloutVersion < 1U) {
    if (looksLikeLegacyLedPwmDefault) {
      config_.ledPwmEnabled = Config::kLedPwmDefaultEnabled;
      config_.ledPwmFrequencyHz = Config::kLedPwmDefaultFrequencyHz;
      saveBool_("led_pwm_en", config_.ledPwmEnabled);
      saveUInt_("led_pwm_hz", config_.ledPwmFrequencyHz);
    }
    saveUInt_("led_pwm_rollout_v1", 1U);
  }

  const uint32_t rolloutVersion = preferences_.getUInt("sender_rollout_v1", 0);
  if (rolloutVersion < Config::kBackendSenderRolloutVersion) {
    config_.serverEnabled = true;
    saveBool_("server_enabled", true);
    saveUInt_("sender_rollout_v1", Config::kBackendSenderRolloutVersion);
  }

  const uint32_t shuntRolloutVersion = preferences_.getUInt("shunt_runtime_rollout_v1", 0);
  if (shuntRolloutVersion < Config::kShuntRuntimeRolloutVersion) {
    const bool looksLikeLegacyShuntDefaults =
        fabsf(config_.solarShuntMilliOhms - 5.0f) < 0.01f &&
        fabsf(config_.loadShuntMilliOhms - 10.0f) < 0.01f;
    if (looksLikeLegacyShuntDefaults) {
      config_.solarShuntMilliOhms = Config::kSolarShuntMilliOhms;
      config_.loadShuntMilliOhms = Config::kLoadShuntMilliOhms;
      saveFloat_("shunt_solar_mo", config_.solarShuntMilliOhms);
      saveFloat_("shunt_load_mo", config_.loadShuntMilliOhms);
    }
    saveUInt_("shunt_runtime_rollout_v1", Config::kShuntRuntimeRolloutVersion);
  }

  const uint32_t inaRolloutVersion = preferences_.getUInt("ina_runtime_rollout_v1", 0);
  if (inaRolloutVersion < Config::kInaRuntimeRolloutVersion) {
    config_.inaAveragingSamples = Config::kDefaultInaAveragingSamples;
    saveUInt_("ina_avg_samples", config_.inaAveragingSamples);
    saveUInt_("ina_runtime_rollout_v1", Config::kInaRuntimeRolloutVersion);
  }

  const uint32_t samplingRolloutVersion =
      preferences_.getUInt("sampling_runtime_rollout_v1", 0);
  if (samplingRolloutVersion < Config::kSamplingRuntimeRolloutVersion) {
    const bool looksLikeFormerSafePreset =
        (config_.sampleIntervalMs == 2000U && config_.sensorPollIntervalMs == 19U &&
         config_.historyIntervalMs == 1000U && config_.chartPointLimit == 120U &&
         config_.inaAveragingSamples == 1U && config_.inaBusConvTimeUs == 140U &&
         config_.inaShuntConvTimeUs == 140U) ||
        (config_.sampleIntervalMs == 1500U && config_.sensorPollIntervalMs == 10U &&
         config_.historyIntervalMs == 500U && config_.chartPointLimit == 160U &&
         config_.inaAveragingSamples == 1U && config_.inaBusConvTimeUs == 140U &&
         config_.inaShuntConvTimeUs == 140U);
    if (looksLikeLegacyAggressiveSamplingConfig(config_.sampleIntervalMs,
                                                config_.sensorPollIntervalMs,
                                                config_.historyIntervalMs,
                                                config_.chartPointLimit,
                                                config_.inaAveragingSamples,
                                                config_.inaBusConvTimeUs,
                                                config_.inaShuntConvTimeUs) ||
        looksLikeFormerSafePreset) {
      const SamplingProfilePreset* preset =
          findSamplingProfilePreset(Config::kDefaultSamplingProfileId);
      if (preset != nullptr) {
        saveRuntimeConfig(preset->sampleIntervalMs,
                          preset->sensorPollIntervalMs,
                          preset->historyIntervalMs,
                          preset->chartPointLimit,
                          preset->inaAveragingSamples,
                          preset->inaBusConvTimeUs,
                          preset->inaShuntConvTimeUs);
      }
    }
    saveUInt_("sampling_runtime_rollout_v1", Config::kSamplingRuntimeRolloutVersion);
  }
}

void WifiService::saveString_(const char* key, const String& value) {
  preferences_.putString(key, value);
}

void WifiService::saveBool_(const char* key, bool value) {
  preferences_.putBool(key, value);
}

void WifiService::saveUInt_(const char* key, uint32_t value) {
  preferences_.putUInt(key, value);
}

void WifiService::saveFloat_(const char* key, float value) {
  preferences_.putFloat(key, value);
}

void WifiService::handleWifiEvent_(WiFiEvent_t event, WiFiEventInfo_t info) {
  const uint32_t nowMs = millis();
  runtime_.lastWifiEventId = static_cast<uint32_t>(event);
  runtime_.lastWifiEventMs = nowMs;

  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP: {
      runtime_.staConnectedOnce = true;
      runtime_.lastStaConnectMs = nowMs;
      runtime_.lastStaGotIpMs = nowMs;
      const String ipText = IPAddress(info.got_ip.ip_info.ip.addr).toString();
      Serial.printf("[WiFi-event][%lu] %s ip=%s rssi=%d\n",
                    static_cast<unsigned long>(nowMs),
                    lastWifiEventName(),
                    ipText.c_str(),
                    WiFi.RSSI());
      break;
    }

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      ++runtime_.staDisconnectCount;
      runtime_.lastStaDisconnectMs = nowMs;
      runtime_.lastStaDisconnectReason = info.wifi_sta_disconnected.reason;
      runtime_.lastStaDisconnectRssi = info.wifi_sta_disconnected.rssi;
      Serial.printf("[WiFi-event][%lu] %s reason=%u(%s) rssi=%d status=%d\n",
                    static_cast<unsigned long>(nowMs),
                    lastWifiEventName(),
                    static_cast<unsigned int>(runtime_.lastStaDisconnectReason),
                    lastStaDisconnectReasonName(),
                    static_cast<int>(runtime_.lastStaDisconnectRssi),
                    static_cast<int>(WiFi.status()));
      break;

    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
    case ARDUINO_EVENT_WIFI_STA_START:
    case ARDUINO_EVENT_WIFI_STA_STOP:
    case ARDUINO_EVENT_WIFI_AP_START:
    case ARDUINO_EVENT_WIFI_AP_STOP:
      Serial.printf("[WiFi-event][%lu] %s\n",
                    static_cast<unsigned long>(nowMs),
                    lastWifiEventName());
      break;

    default:
      break;
  }
}

void WifiService::startStation_() {
  apMode_ = false;
  accessPointSsid_ = "";

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  WiFi.persistent(false);
  WiFi.begin(config_.wifiSsid.c_str(), config_.wifiPass.c_str());
  wifiConnectStartedMs_ = millis();
  Serial.printf("[WiFi][%lu] start sta ssid=%s\n",
                static_cast<unsigned long>(wifiConnectStartedMs_),
                config_.wifiSsid.c_str());
}

void WifiService::startAccessPoint_() {
  apMode_ = true;
  accessPointSsid_ = buildAccessPointSsid_();

  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(accessPointSsid_.c_str());
  Serial.printf("[WiFi][%lu] fallback ap ssid=%s\n",
                static_cast<unsigned long>(millis()),
                accessPointSsid_.c_str());
}

String WifiService::buildDefaultDeviceId_() const {
  const uint64_t chipId = ESP.getEfuseMac();
  char buffer[24];
  snprintf(buffer,
           sizeof(buffer),
           "solar-%06llX",
           static_cast<unsigned long long>(chipId & 0xFFFFFFULL));
  return String(buffer);
}

String WifiService::buildAccessPointSsid_() const {
  const uint64_t chipId = ESP.getEfuseMac();
  char buffer[40];
  snprintf(buffer,
           sizeof(buffer),
           "SolarMonitor-Setup-%06llX",
           static_cast<unsigned long long>(chipId & 0xFFFFFFULL));
  return String(buffer);
}

String WifiService::normalizeApiBase_(String value) const {
  value.trim();
  if (value.isEmpty()) {
    value = Config::kDefaultApiBase;
  }

  if (value.indexOf("://") < 0) {
    value = String("http://") + value;
  }

  while (value.endsWith("/")) {
    value.remove(value.length() - 1);
  }

  if (!value.endsWith("/api/v1")) {
    value += "/api/v1";
  }

  return value;
}

uint32_t WifiService::normalizeSensorPollIntervalMs_(uint32_t value) const {
  if (value < Config::kMinSensorPollIntervalMs) {
    return Config::kMinSensorPollIntervalMs;
  }

  if (value > Config::kMaxSensorPollIntervalMs) {
    return Config::kMaxSensorPollIntervalMs;
  }

  return value;
}

uint32_t WifiService::normalizeSampleIntervalMs_(uint32_t value) const {
  if (value < Config::kMinSampleIntervalMs) {
    return Config::kMinSampleIntervalMs;
  }

  if (value > Config::kMaxSampleIntervalMs) {
    return Config::kMaxSampleIntervalMs;
  }

  return value;
}

uint32_t WifiService::normalizeHistoryIntervalMs_(uint32_t value) const {
  if (value < Config::kMinHistoryIntervalMs) {
    return Config::kMinHistoryIntervalMs;
  }

  if (value > Config::kMaxHistoryIntervalMs) {
    return Config::kMaxHistoryIntervalMs;
  }

  return value;
}

uint32_t WifiService::normalizeChartPointLimit_(uint32_t value) const {
  if (value < Config::kMinChartPointLimit) {
    return Config::kMinChartPointLimit;
  }

  if (value > Config::kMaxChartPointLimit) {
    return Config::kMaxChartPointLimit;
  }

  return value;
}

uint32_t WifiService::normalizeInaAveragingSamples_(uint32_t value) const {
  if (value < 1U) {
    return 1U;
  }

  return value > 1024U ? 1024U : value;
}

uint32_t WifiService::normalizeInaConversionTimeUs_(uint32_t value) const {
  return normalizeIna3221ConversionTimeUs(value);
}

uint32_t WifiService::normalizeFlowThresholdMw_(uint32_t value) const {
  if (value < Config::kMinFlowThresholdMw) {
    return Config::kMinFlowThresholdMw;
  }

  if (value > Config::kMaxFlowThresholdMw) {
    return Config::kMaxFlowThresholdMw;
  }

  return value;
}

uint32_t WifiService::normalizeLedPwmFrequencyHz_(uint32_t value) const {
  if (value < Config::kLedPwmMinFrequencyHz) {
    return Config::kLedPwmMinFrequencyHz;
  }

  if (value > Config::kLedPwmMaxFrequencyHz) {
    return Config::kLedPwmMaxFrequencyHz;
  }

  return value;
}

uint32_t WifiService::normalizeLedPwmDutyPercent_(uint32_t value) const {
  if (value < Config::kLedPwmMinDutyPercent) {
    return Config::kLedPwmMinDutyPercent;
  }

  if (value > Config::kLedPwmMaxDutyPercent) {
    return Config::kLedPwmMaxDutyPercent;
  }

  return value;
}

uint32_t WifiService::normalizeLedPwmFlashOnMs_(uint32_t value) const {
  if (value < Config::kLedPwmFlashMinOnMs) {
    return Config::kLedPwmFlashMinOnMs;
  }

  if (value > Config::kLedPwmFlashMaxOnMs) {
    return Config::kLedPwmFlashMaxOnMs;
  }

  return value;
}

uint32_t WifiService::normalizeLedPwmFlashPeriodMs_(uint32_t value) const {
  if (value < Config::kLedPwmFlashMinPeriodMs) {
    return Config::kLedPwmFlashMinPeriodMs;
  }

  if (value > Config::kLedPwmFlashMaxPeriodMs) {
    return Config::kLedPwmFlashMaxPeriodMs;
  }

  return value;
}

uint32_t WifiService::normalizeControlPinOffHour_(uint32_t value) const {
  if (value < Config::kControlPinScheduleMinHour) {
    return Config::kControlPinScheduleMinHour;
  }

  if (value > Config::kControlPinScheduleMaxHour) {
    return Config::kControlPinScheduleMaxHour;
  }

  return value;
}

uint32_t WifiService::normalizeControlPinOffMinute_(uint32_t value) const {
  if (value < Config::kControlPinScheduleMinMinute) {
    return Config::kControlPinScheduleMinMinute;
  }

  if (value > Config::kControlPinScheduleMaxMinute) {
    return Config::kControlPinScheduleMaxMinute;
  }

  return value;
}

float WifiService::normalizeShuntMilliOhms_(float value, float fallbackValue) const {
  if (isnan(value) || isinf(value)) {
    return fallbackValue;
  }

  if (value < Config::kMinShuntMilliOhms) {
    return Config::kMinShuntMilliOhms;
  }

  if (value > Config::kMaxShuntMilliOhms) {
    return Config::kMaxShuntMilliOhms;
  }

  return value;
}
