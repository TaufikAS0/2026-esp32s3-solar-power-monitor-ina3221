#include "ina_sensors.h"

#include <math.h>
#include <Wire.h>

#include "config.h"

namespace {

constexpr uint8_t kIna3221ConfigRegister = 0x00;
constexpr uint8_t kIna3221Channel1ShuntRegister = 0x01;
constexpr uint8_t kIna3221Channel1BusRegister = 0x02;
constexpr uint8_t kIna3221Channel2ShuntRegister = 0x03;
constexpr uint8_t kIna3221Channel2BusRegister = 0x04;
constexpr uint8_t kIna3221Channel3ShuntRegister = 0x05;
constexpr uint8_t kIna3221Channel3BusRegister = 0x06;
constexpr float kIna3221ShuntVoltageLsbMv = 0.04f;
constexpr float kIna3221BusVoltageLsbV = 0.008f;

struct Ina3221ChannelMap {
  uint8_t shuntRegister;
  uint8_t busRegister;
};

constexpr Ina3221ChannelMap kSolarChannel{
    kIna3221Channel1ShuntRegister, kIna3221Channel1BusRegister};
constexpr Ina3221ChannelMap kBatteryChannel{
    kIna3221Channel2ShuntRegister, kIna3221Channel2BusRegister};
constexpr Ina3221ChannelMap kLoadChannel{
    kIna3221Channel3ShuntRegister, kIna3221Channel3BusRegister};

bool writeRegister(uint8_t reg, uint16_t value) {
  Wire.beginTransmission(Config::kIna3221Address);
  Wire.write(reg);
  Wire.write(static_cast<uint8_t>((value >> 8) & 0xFFU));
  Wire.write(static_cast<uint8_t>(value & 0xFFU));
  return Wire.endTransmission() == 0U;
}

bool readRegister(uint8_t reg, uint16_t& value) {
  Wire.beginTransmission(Config::kIna3221Address);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0U) {
    return false;
  }

  const uint8_t bytesRead = Wire.requestFrom(static_cast<int>(Config::kIna3221Address), 2);
  if (bytesRead != 2U) {
    return false;
  }

  value = static_cast<uint16_t>(Wire.read()) << 8;
  value |= static_cast<uint16_t>(Wire.read());
  return true;
}

int16_t unpackSigned13(uint16_t raw) {
  int16_t value = static_cast<int16_t>(raw >> 3);
  if ((raw & 0x8000U) != 0U) {
    value |= static_cast<int16_t>(0xE000U);
  }
  return value;
}

bool readChannel(const Ina3221ChannelMap& channel, float shuntMilliOhms, InaReading& outReading) {
  uint16_t rawShunt = 0;
  uint16_t rawBus = 0;
  if (!readRegister(channel.shuntRegister, rawShunt) ||
      !readRegister(channel.busRegister, rawBus)) {
    return false;
  }

  const int16_t shuntCounts = unpackSigned13(rawShunt);
  const int16_t busCounts = unpackSigned13(rawBus);

  outReading.shuntVoltageMv = static_cast<float>(shuntCounts) * kIna3221ShuntVoltageLsbMv;
  outReading.busVoltageV = static_cast<float>(busCounts) * kIna3221BusVoltageLsbV;
  // INA3221 bus voltage is measured at IN- relative to GND, which is the
  // load-side node we want to show in the dashboard.
  outReading.loadVoltageV = outReading.busVoltageV;
  outReading.currentMa =
      shuntMilliOhms > 0.0f
          ? (outReading.shuntVoltageMv * 1000.0f) / shuntMilliOhms
          : 0.0f;
  outReading.powerMw = outReading.busVoltageV * outReading.currentMa;
  outReading.ok =
      !isnan(outReading.busVoltageV) &&
      !isnan(outReading.shuntVoltageMv) &&
      !isnan(outReading.currentMa) &&
      !isnan(outReading.powerMw) &&
      outReading.busVoltageV >= 0.0f;
  return outReading.ok;
}

void normalizeLoadReading(InaReading& reading) {
  if (reading.currentMa < 0.0f) {
    reading.currentMa = fabsf(reading.currentMa);
  }

  if (reading.powerMw < 0.0f) {
    reading.powerMw = fabsf(reading.powerMw);
  }
}

}  // namespace

void InaSensors::applyConfig(const DeviceConfig& config) {
  solarShuntMilliOhms_ = config.solarShuntMilliOhms;
  batteryShuntMilliOhms_ = config.batteryShuntMilliOhms;
  loadShuntMilliOhms_ = config.loadShuntMilliOhms;
}

void InaSensors::begin() {
  Wire.begin(Config::kI2cSdaPin, Config::kI2cSclPin);

  const bool sensorReady = writeRegister(kIna3221ConfigRegister, Config::kIna3221ConfigValue);
  solarAvailable_ = sensorReady;
  batteryAvailable_ = sensorReady;
  loadAvailable_ = sensorReady;

  resetReading_(solar_);
  resetReading_(battery_);
  resetReading_(load_);

  readAll_();
  lastReadMs_ = millis();
}

void InaSensors::refreshNow() {
  readAll_();
  lastReadMs_ = millis();
}

void InaSensors::update(uint32_t nowMs) {
  if (nowMs - lastReadMs_ < sampleIntervalMs_) {
    return;
  }

  readAll_();
  lastReadMs_ = nowMs;
}

void InaSensors::setSampleIntervalMs(uint32_t sampleIntervalMs) {
  if (sampleIntervalMs < Config::kMinSampleIntervalMs) {
    sampleIntervalMs_ = Config::kMinSampleIntervalMs;
    return;
  }

  if (sampleIntervalMs > Config::kMaxSampleIntervalMs) {
    sampleIntervalMs_ = Config::kMaxSampleIntervalMs;
    return;
  }

  sampleIntervalMs_ = sampleIntervalMs;
}

const InaReading& InaSensors::solar() const {
  return solar_;
}

const InaReading& InaSensors::battery() const {
  return battery_;
}

const InaReading& InaSensors::load() const {
  return load_;
}

bool InaSensors::allHealthy() const {
  return solar_.ok && battery_.ok && load_.ok;
}

bool InaSensors::anyHealthy() const {
  return solar_.ok || battery_.ok || load_.ok;
}

bool InaSensors::loadHealthy() const {
  return load_.ok;
}

uint32_t InaSensors::lastReadMs() const {
  return lastReadMs_;
}

void InaSensors::readAll_() {
  if (solarAvailable_) {
    if (!readChannel(kSolarChannel, solarShuntMilliOhms_, solar_)) {
      resetReading_(solar_);
    }
  } else {
    resetReading_(solar_);
  }

  if (batteryAvailable_) {
    if (!readChannel(kBatteryChannel, batteryShuntMilliOhms_, battery_)) {
      resetReading_(battery_);
    }
  } else {
    resetReading_(battery_);
  }

  if (loadAvailable_) {
    if (readChannel(kLoadChannel, loadShuntMilliOhms_, load_)) {
      normalizeLoadReading(load_);
    } else {
      resetReading_(load_);
    }
  } else {
    resetReading_(load_);
  }
}

void InaSensors::resetReading_(InaReading& reading) {
  reading.ok = false;
  reading.busVoltageV = 0.0f;
  reading.shuntVoltageMv = 0.0f;
  reading.currentMa = 0.0f;
  reading.powerMw = 0.0f;
  reading.loadVoltageV = 0.0f;
}
