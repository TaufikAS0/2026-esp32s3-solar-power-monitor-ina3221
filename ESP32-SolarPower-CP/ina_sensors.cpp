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
constexpr uint32_t kIna3221EnabledChannelCount = 3;
constexpr uint32_t kIna3221AveragingOptions[] = {1, 4, 16, 64, 128, 256, 512, 1024};
constexpr uint32_t kIna3221ConversionTimeOptionsUs[] = {140, 204, 332, 588, 1100, 2116, 4156, 8244};

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

uint32_t nearestOption(uint32_t value, const uint32_t* options, size_t count) {
  if (count == 0U) {
    return value;
  }

  uint32_t bestValue = options[0];
  uint32_t bestDelta = value > bestValue ? value - bestValue : bestValue - value;
  for (size_t index = 1; index < count; ++index) {
    const uint32_t candidate = options[index];
    const uint32_t delta = value > candidate ? value - candidate : candidate - value;
    if (delta < bestDelta) {
      bestDelta = delta;
      bestValue = candidate;
    }
  }

  return bestValue;
}

uint8_t optionIndex(uint32_t value, const uint32_t* options, size_t count) {
  for (size_t index = 0; index < count; ++index) {
    if (options[index] == value) {
      return static_cast<uint8_t>(index);
    }
  }
  return 0U;
}

uint32_t frameTimeUsFor(uint32_t averagingSamples,
                        uint32_t busConvTimeUs,
                        uint32_t shuntConvTimeUs) {
  return (busConvTimeUs + shuntConvTimeUs) * kIna3221EnabledChannelCount * averagingSamples;
}

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

uint32_t normalizeIna3221AveragingSamples(uint32_t value) {
  return nearestOption(value,
                       kIna3221AveragingOptions,
                       sizeof(kIna3221AveragingOptions) / sizeof(kIna3221AveragingOptions[0]));
}

uint32_t normalizeIna3221ConversionTimeUs(uint32_t value) {
  return nearestOption(value,
                       kIna3221ConversionTimeOptionsUs,
                       sizeof(kIna3221ConversionTimeOptionsUs) /
                           sizeof(kIna3221ConversionTimeOptionsUs[0]));
}

uint16_t buildIna3221ConfigRegister(uint32_t averagingSamples,
                                    uint32_t busConvTimeUs,
                                    uint32_t shuntConvTimeUs) {
  const uint32_t normalizedAverage = normalizeIna3221AveragingSamples(averagingSamples);
  const uint32_t normalizedBus = normalizeIna3221ConversionTimeUs(busConvTimeUs);
  const uint32_t normalizedShunt = normalizeIna3221ConversionTimeUs(shuntConvTimeUs);
  const uint16_t averageBits =
      static_cast<uint16_t>(optionIndex(normalizedAverage,
                                        kIna3221AveragingOptions,
                                        sizeof(kIna3221AveragingOptions) /
                                            sizeof(kIna3221AveragingOptions[0])));
  const uint16_t busBits =
      static_cast<uint16_t>(optionIndex(normalizedBus,
                                        kIna3221ConversionTimeOptionsUs,
                                        sizeof(kIna3221ConversionTimeOptionsUs) /
                                            sizeof(kIna3221ConversionTimeOptionsUs[0])));
  const uint16_t shuntBits =
      static_cast<uint16_t>(optionIndex(normalizedShunt,
                                        kIna3221ConversionTimeOptionsUs,
                                        sizeof(kIna3221ConversionTimeOptionsUs) /
                                            sizeof(kIna3221ConversionTimeOptionsUs[0])));
  return static_cast<uint16_t>((0x7U << 12) | (averageBits << 9) | (busBits << 6) |
                               (shuntBits << 3) | 0x7U);
}

InaTimingProfile describeIna3221Timing(uint32_t averagingSamples,
                                       uint32_t busConvTimeUs,
                                       uint32_t shuntConvTimeUs) {
  InaTimingProfile profile;
  profile.averagingSamples = normalizeIna3221AveragingSamples(averagingSamples);
  profile.busConvTimeUs = normalizeIna3221ConversionTimeUs(busConvTimeUs);
  profile.shuntConvTimeUs = normalizeIna3221ConversionTimeUs(shuntConvTimeUs);
  profile.configRegister =
      buildIna3221ConfigRegister(profile.averagingSamples,
                                 profile.busConvTimeUs,
                                 profile.shuntConvTimeUs);
  profile.frameTimeUs =
      frameTimeUsFor(profile.averagingSamples, profile.busConvTimeUs, profile.shuntConvTimeUs);
  profile.estimatedChannelRateHz =
      profile.frameTimeUs > 0U ? (1000000.0f / static_cast<float>(profile.frameTimeUs)) : 0.0f;
  return profile;
}

void InaSensors::applyConfig(const DeviceConfig& config) {
  solarShuntMilliOhms_ = config.solarShuntMilliOhms;
  batteryShuntMilliOhms_ = config.batteryShuntMilliOhms;
  loadShuntMilliOhms_ = config.loadShuntMilliOhms;
  averagingSamples_ = normalizeIna3221AveragingSamples(config.inaAveragingSamples);
  busConvTimeUs_ = normalizeIna3221ConversionTimeUs(config.inaBusConvTimeUs);
  shuntConvTimeUs_ = normalizeIna3221ConversionTimeUs(config.inaShuntConvTimeUs);
  configRegister_ =
      buildIna3221ConfigRegister(averagingSamples_, busConvTimeUs_, shuntConvTimeUs_);
  if (configured_) {
    writeConfig_();
  }
}

void InaSensors::begin() {
  Wire.begin(Config::kI2cSdaPin, Config::kI2cSclPin);
  Wire.setClock(Config::kI2cClockHz);

  configured_ = true;
  const bool sensorReady = writeRegister(kIna3221ConfigRegister, configRegister_);
  solarAvailable_ = sensorReady;
  batteryAvailable_ = sensorReady;
  loadAvailable_ = sensorReady;
  totalReadCount_ = 0;
  rateWindowStartMs_ = millis();
  rateWindowReadCount_ = 0;
  measuredReadRateHz_ = 0.0f;

  resetReading_(solar_);
  resetReading_(battery_);
  resetReading_(load_);

  readAll_();
  lastReadMs_ = millis();
  noteRead_(lastReadMs_);
}

void InaSensors::refreshNow() {
  readAll_();
  lastReadMs_ = millis();
  noteRead_(lastReadMs_);
}

void InaSensors::update(uint32_t nowMs) {
  if (nowMs - lastReadMs_ < sampleIntervalMs_) {
    return;
  }

  readAll_();
  lastReadMs_ = nowMs;
  noteRead_(nowMs);
}

void InaSensors::setSampleIntervalMs(uint32_t sampleIntervalMs) {
  if (sampleIntervalMs < Config::kMinSensorPollIntervalMs) {
    sampleIntervalMs_ = Config::kMinSensorPollIntervalMs;
    return;
  }

  if (sampleIntervalMs > Config::kMaxSensorPollIntervalMs) {
    sampleIntervalMs_ = Config::kMaxSensorPollIntervalMs;
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

uint32_t InaSensors::totalReadCount() const {
  return totalReadCount_;
}

float InaSensors::measuredReadRateHz() const {
  return measuredReadRateHz_;
}

InaTimingProfile InaSensors::timingProfile() const {
  return describeIna3221Timing(averagingSamples_, busConvTimeUs_, shuntConvTimeUs_);
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

void InaSensors::writeConfig_() {
  writeRegister(kIna3221ConfigRegister, configRegister_);
}

void InaSensors::noteRead_(uint32_t nowMs) {
  ++totalReadCount_;
  ++rateWindowReadCount_;
  if (rateWindowStartMs_ == 0U) {
    rateWindowStartMs_ = nowMs;
    return;
  }

  const uint32_t elapsedMs = nowMs - rateWindowStartMs_;
  if (elapsedMs < 1000U) {
    return;
  }

  measuredReadRateHz_ =
      static_cast<float>(rateWindowReadCount_) * 1000.0f / static_cast<float>(elapsedMs);
  rateWindowStartMs_ = nowMs;
  rateWindowReadCount_ = 0;
}

void InaSensors::resetReading_(InaReading& reading) {
  reading.ok = false;
  reading.busVoltageV = 0.0f;
  reading.shuntVoltageMv = 0.0f;
  reading.currentMa = 0.0f;
  reading.powerMw = 0.0f;
  reading.loadVoltageV = 0.0f;
}
