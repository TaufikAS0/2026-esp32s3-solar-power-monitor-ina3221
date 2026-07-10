#pragma once

#include <Arduino.h>
#include <time.h>

#include "wifi_service.h"

struct InternetTimeStatus {
  bool wifiConnected = false;
  bool ntpConfigured = false;
  bool timeValid = false;
  uint32_t lastConfigureMs = 0;
  uint32_t lastValidSyncMs = 0;
  uint32_t configureAttempts = 0;
  time_t lastEpoch = 0;
};

class InternetTimeService {
public:
  explicit InternetTimeService(const WifiService& wifiService);

  void begin();
  void update(uint32_t nowMs);

  bool isTimeValid() const;
  bool getLocalTimeStruct(tm& out) const;
  String currentLocalTimeText() const;
  const char* statusText() const;
  const InternetTimeStatus& runtime() const;

private:
  const WifiService& wifiService_;
  InternetTimeStatus runtime_;
  bool lastWifiConnected_ = false;
  bool hadValidTime_ = false;

  void configureNtp_(uint32_t nowMs);
};
