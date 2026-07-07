#include "raw_history_buffer.h"

void RawHistoryBuffer::add(uint32_t timestampMs,
                           float solarVoltageV,
                           float batteryVoltageV,
                           float loadVoltageV,
                           float solarCurrentMa,
                           float batteryCurrentMa,
                           float loadCurrentMa) {
  RawHistoryPoint& point = points_[head_];
  point.sequence = nextSequence_++;
  point.timestampMs = timestampMs;
  point.solarVoltageV = solarVoltageV;
  point.batteryVoltageV = batteryVoltageV;
  point.loadVoltageV = loadVoltageV;
  point.solarCurrentMa = solarCurrentMa;
  point.batteryCurrentMa = batteryCurrentMa;
  point.loadCurrentMa = loadCurrentMa;

  head_ = (head_ + 1U) % Config::kRawHistoryCapacity;
  if (count_ < Config::kRawHistoryCapacity) {
    ++count_;
  }
}

size_t RawHistoryBuffer::count() const {
  return count_;
}

size_t RawHistoryBuffer::capacity() const {
  return Config::kRawHistoryCapacity;
}

uint32_t RawHistoryBuffer::oldestSequence() const {
  if (count_ == 0U) {
    return 0U;
  }

  const size_t start = (count_ == Config::kRawHistoryCapacity) ? head_ : 0U;
  return points_[start].sequence;
}

uint32_t RawHistoryBuffer::latestSequence() const {
  if (count_ == 0U) {
    return 0U;
  }

  const size_t lastIndex =
      (head_ + Config::kRawHistoryCapacity - 1U) % Config::kRawHistoryCapacity;
  return points_[lastIndex].sequence;
}

bool RawHistoryBuffer::getOrdered(size_t orderedIndex, RawHistoryPoint& outPoint) const {
  if (orderedIndex >= count_) {
    return false;
  }

  const size_t start = (count_ == Config::kRawHistoryCapacity) ? head_ : 0U;
  const size_t actualIndex = (start + orderedIndex) % Config::kRawHistoryCapacity;
  outPoint = points_[actualIndex];
  return true;
}
