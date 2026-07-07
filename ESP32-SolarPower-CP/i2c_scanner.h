#pragma once

#include <Arduino.h>

struct I2cScanResult {
  uint8_t address = 0;
  String addressHex;
  String label;
};

class I2cScanner {
public:
  static constexpr size_t kMaxResults = 16;

  size_t scan(I2cScanResult* outResults, size_t maxResults) const;

private:
  String labelForAddress_(uint8_t address) const;
};

