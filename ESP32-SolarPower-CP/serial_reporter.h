#pragma once

#include <Arduino.h>

#include "ina_sensors.h"
#include "system_state.h"
#include "wifi_service.h"

class SerialReporter {
public:
  void begin();
  void update(uint32_t nowMs,
              const WifiService& wifiService,
              const InaReading& solar,
              const InaReading& battery,
              PowerSystemState state);

private:
  uint32_t lastReportMs_ = 0;
};

