#pragma once

#include <Arduino.h>

#include "config.h"

struct RawHistoryPoint {
  uint32_t sequence = 0;
  uint32_t timestampMs = 0;
  float solarPowerMw = 0.0f;
  float batteryPowerSignedMw = 0.0f;
  float loadPowerMw = 0.0f;
  float solarVoltageV = 0.0f;
  float batteryVoltageV = 0.0f;
  float loadVoltageV = 0.0f;
  float solarCurrentMa = 0.0f;
  float batteryCurrentMa = 0.0f;
  float loadCurrentMa = 0.0f;
};

class RawHistoryBuffer {
public:
  void add(uint32_t timestampMs,
           float solarPowerMw,
           float batteryPowerSignedMw,
           float loadPowerMw,
           float solarVoltageV,
           float batteryVoltageV,
           float loadVoltageV,
           float solarCurrentMa,
           float batteryCurrentMa,
           float loadCurrentMa);
  size_t count() const;
  size_t capacity() const;
  uint32_t oldestSequence() const;
  uint32_t latestSequence() const;
  bool getOrdered(size_t orderedIndex, RawHistoryPoint& outPoint) const;

private:
  RawHistoryPoint points_[Config::kRawHistoryCapacity];
  size_t count_ = 0;
  size_t head_ = 0;
  uint32_t nextSequence_ = 1;
};
