#pragma once

#include <Arduino.h>

#include "config.h"

struct HistoryPoint {
  uint32_t timestampMs = 0;
  float solarPowerMw = 0.0f;
  float batteryPowerMw = 0.0f;
  float batteryPowerSignedMw = 0.0f;
  float loadPowerMw = 0.0f;
  float solarVoltageV = 0.0f;
  float batteryVoltageV = 0.0f;
  float loadVoltageV = 0.0f;
  float solarCurrentMa = 0.0f;
  float batteryCurrentMa = 0.0f;
  float loadCurrentMa = 0.0f;
};

class HistoryBuffer {
public:
  void add(uint32_t timestampMs,
           float solarPowerMw,
           float batteryPowerMw,
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
  bool getOrdered(size_t orderedIndex, HistoryPoint& outPoint) const;

private:
  HistoryPoint points_[Config::kHistoryCapacity];
  size_t count_ = 0;
  size_t head_ = 0;
};

