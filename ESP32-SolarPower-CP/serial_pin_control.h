#pragma once

#include <Arduino.h>

struct DeviceConfig;
class InternetTimeService;

class SerialPinControl {
public:
  void begin();
  void update();
  void setHigh(bool high);
  void applyScheduledOff(const DeviceConfig& config,
                         const InternetTimeService& timeService,
                         uint32_t nowMs);

  uint8_t pin() const;
  bool isHigh() const;
  const char* stateText() const;

private:
  void handleInputLine_(String line);
  void setState_(bool high);
  void printHelp_() const;
  void printStatus_() const;

  bool stateHigh_ = false;
  String inputBuffer_;
  int32_t lastScheduleDayKey_ = -1;
};
