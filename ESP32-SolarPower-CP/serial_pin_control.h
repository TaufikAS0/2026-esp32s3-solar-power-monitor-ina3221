#pragma once

#include <Arduino.h>

class SerialPinControl {
public:
  void begin();
  void update();
  void setHigh(bool high);

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
};
