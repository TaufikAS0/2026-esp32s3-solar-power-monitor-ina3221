#include "internet_time_service.h"

#include <time.h>

#include "config.h"

namespace {

constexpr time_t kMinValidEpoch = 1704067200;  // 2024-01-01T00:00:00Z

String formatLocalTimeText(const tm& value) {
  char buffer[32];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S WIB", &value);
  return String(buffer);
}

}  // namespace

InternetTimeService::InternetTimeService(const WifiService& wifiService)
    : wifiService_(wifiService) {}

void InternetTimeService::begin() {
  runtime_ = InternetTimeStatus{};
  lastWifiConnected_ = false;
  hadValidTime_ = false;
  setenv("TZ", Config::kTimeZoneTz, 1);
  tzset();
}

void InternetTimeService::update(uint32_t nowMs) {
  runtime_.wifiConnected = wifiService_.isStaConnected();
  if (runtime_.wifiConnected) {
    if (!lastWifiConnected_ ||
        (!isTimeValid() &&
         (runtime_.lastConfigureMs == 0U ||
          nowMs - runtime_.lastConfigureMs >= Config::kTimeSyncRetryMs))) {
      configureNtp_(nowMs);
    }
  } else {
    lastWifiConnected_ = false;
    runtime_.timeValid = isTimeValid();
    return;
  }

  const bool valid = isTimeValid();
  runtime_.timeValid = valid;
  if (valid) {
    runtime_.lastEpoch = time(nullptr);
    if (!hadValidTime_) {
      runtime_.lastValidSyncMs = nowMs;
    }
    hadValidTime_ = true;
  } else {
    hadValidTime_ = false;
  }

  lastWifiConnected_ = runtime_.wifiConnected;
}

bool InternetTimeService::isTimeValid() const {
  return time(nullptr) >= kMinValidEpoch;
}

bool InternetTimeService::getLocalTimeStruct(tm& out) const {
  if (!isTimeValid()) {
    return false;
  }

  const time_t epoch = time(nullptr);
  localtime_r(&epoch, &out);
  return true;
}

String InternetTimeService::currentLocalTimeText() const {
  tm localTime;
  if (!getLocalTimeStruct(localTime)) {
    return "sync pending";
  }

  return formatLocalTimeText(localTime);
}

const char* InternetTimeService::statusText() const {
  if (runtime_.timeValid) {
    return "synced";
  }

  if (!runtime_.wifiConnected) {
    return "waiting_wifi";
  }

  return runtime_.ntpConfigured ? "syncing" : "idle";
}

const InternetTimeStatus& InternetTimeService::runtime() const {
  return runtime_;
}

void InternetTimeService::configureNtp_(uint32_t nowMs) {
  configTzTime(Config::kTimeZoneTz,
               Config::kNtpServerPrimary,
               Config::kNtpServerSecondary,
               Config::kNtpServerTertiary);
  runtime_.ntpConfigured = true;
  runtime_.lastConfigureMs = nowMs;
  ++runtime_.configureAttempts;

  Serial.printf("[time-sync][%lu] ntp tz=%s servers=%s,%s,%s\n",
                static_cast<unsigned long>(nowMs),
                Config::kTimeZoneTz,
                Config::kNtpServerPrimary,
                Config::kNtpServerSecondary,
                Config::kNtpServerTertiary);
}
