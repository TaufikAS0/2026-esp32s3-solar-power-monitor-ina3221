#pragma once

#include <Arduino.h>

struct SamplingProfilePreset {
  const char* id;
  const char* label;
  const char* summary;
  uint32_t targetHz;
  uint32_t sampleIntervalMs;
  uint32_t sensorPollIntervalMs;
  uint32_t historyIntervalMs;
  uint32_t chartPointLimit;
  uint32_t inaAveragingSamples;
  uint32_t inaBusConvTimeUs;
  uint32_t inaShuntConvTimeUs;
  bool beginnerSafe;
};

const SamplingProfilePreset* samplingProfilePresets(size_t& count);
const SamplingProfilePreset* findSamplingProfilePreset(const String& id);
const SamplingProfilePreset* matchSamplingProfilePreset(uint32_t sampleIntervalMs,
                                                        uint32_t sensorPollIntervalMs,
                                                        uint32_t historyIntervalMs,
                                                        uint32_t chartPointLimit,
                                                        uint32_t inaAveragingSamples,
                                                        uint32_t inaBusConvTimeUs,
                                                        uint32_t inaShuntConvTimeUs);
bool looksLikeLegacyAggressiveSamplingConfig(uint32_t sampleIntervalMs,
                                             uint32_t sensorPollIntervalMs,
                                             uint32_t historyIntervalMs,
                                             uint32_t chartPointLimit,
                                             uint32_t inaAveragingSamples,
                                             uint32_t inaBusConvTimeUs,
                                             uint32_t inaShuntConvTimeUs);
