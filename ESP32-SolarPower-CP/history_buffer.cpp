#include "history_buffer.h"

void HistoryBuffer::add(uint32_t timestampMs,
                        float solarPowerMw,
                        float batteryPowerMw,
                        float batteryPowerSignedMw,
                        float loadPowerMw,
                        float solarVoltageV,
                        float batteryVoltageV,
                        float loadVoltageV,
                        float solarCurrentMa,
                        float batteryCurrentMa,
                        float loadCurrentMa) {
  points_[head_].timestampMs = timestampMs;
  points_[head_].solarPowerMw = solarPowerMw;
  points_[head_].batteryPowerMw = batteryPowerMw;
  points_[head_].batteryPowerSignedMw = batteryPowerSignedMw;
  points_[head_].loadPowerMw = loadPowerMw;
  points_[head_].solarVoltageV = solarVoltageV;
  points_[head_].batteryVoltageV = batteryVoltageV;
  points_[head_].loadVoltageV = loadVoltageV;
  points_[head_].solarCurrentMa = solarCurrentMa;
  points_[head_].batteryCurrentMa = batteryCurrentMa;
  points_[head_].loadCurrentMa = loadCurrentMa;

  head_ = (head_ + 1U) % Config::kHistoryCapacity;
  if (count_ < Config::kHistoryCapacity) {
    ++count_;
  }
}

size_t HistoryBuffer::count() const {
  return count_;
}

size_t HistoryBuffer::capacity() const {
  return Config::kHistoryCapacity;
}

bool HistoryBuffer::getOrdered(size_t orderedIndex, HistoryPoint& outPoint) const {
  if (orderedIndex >= count_) {
    return false;
  }

  const size_t start = (count_ == Config::kHistoryCapacity) ? head_ : 0U;
  const size_t actualIndex = (start + orderedIndex) % Config::kHistoryCapacity;
  outPoint = points_[actualIndex];
  return true;
}

