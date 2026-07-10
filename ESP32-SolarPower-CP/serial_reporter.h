#pragma once

#include <Arduino.h>

#include "backend_sender.h"
#include "ina_sensors.h"
#include "system_state.h"
#include "wifi_service.h"

class SerialReporter {
public:
  void begin();
  bool shouldReport(uint32_t nowMs) const;
  void update(uint32_t nowMs,
              const WifiService& wifiService,
              const BackendSenderRuntime& backendRuntime,
              const InaReading& solar,
              const InaReading& battery,
              PowerSystemState state,
              uint8_t controlPin,
              bool controlPinHigh);

private:
  uint32_t lastReportMs_ = 0;
};

