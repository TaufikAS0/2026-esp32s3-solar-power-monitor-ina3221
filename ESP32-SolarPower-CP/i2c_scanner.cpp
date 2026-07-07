#include "i2c_scanner.h"

#include <Wire.h>

size_t I2cScanner::scan(I2cScanResult* outResults, size_t maxResults) const {
  if (outResults == nullptr || maxResults == 0U) {
    return 0;
  }

  size_t count = 0;
  for (uint8_t address = 1; address < 127; ++address) {
    Wire.beginTransmission(address);
    const uint8_t error = Wire.endTransmission();
    if (error != 0U) {
      continue;
    }

    if (count >= maxResults) {
      break;
    }

    char addressHex[6];
    snprintf(addressHex, sizeof(addressHex), "0x%02X", address);

    outResults[count].address = address;
    outResults[count].addressHex = String(addressHex);
    outResults[count].label = labelForAddress_(address);
    ++count;
  }

  return count;
}

String I2cScanner::labelForAddress_(uint8_t address) const {
  switch (address) {
    case 0x40:
      return "INA3221 Solar/Battery/Load";
    default:
      return "Unknown device";
  }
}
