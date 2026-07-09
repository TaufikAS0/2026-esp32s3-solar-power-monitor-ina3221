#include "sampling_profile.h"

namespace {

constexpr SamplingProfilePreset kSamplingProfiles[] = {
    {"stable_53hz",
     "Stable 53 Hz",
     "Mode aman untuk soak telemetry, raw history browser, dan setup beginner.",
     53U,
     2000U,
     19U,
     1000U,
     120U,
     1U,
     140U,
     140U,
     true},
    {"stable_100hz",
     "Balanced 100 Hz",
     "Masih aman, tetapi lebih responsif untuk inspeksi raw jangka pendek.",
     100U,
     1500U,
     10U,
     500U,
     160U,
     1U,
     140U,
     140U,
     true},
};

bool samplingProfileMatches(const SamplingProfilePreset& preset,
                            uint32_t sampleIntervalMs,
                            uint32_t sensorPollIntervalMs,
                            uint32_t historyIntervalMs,
                            uint32_t chartPointLimit,
                            uint32_t inaAveragingSamples,
                            uint32_t inaBusConvTimeUs,
                            uint32_t inaShuntConvTimeUs) {
  return preset.sampleIntervalMs == sampleIntervalMs &&
         preset.sensorPollIntervalMs == sensorPollIntervalMs &&
         preset.historyIntervalMs == historyIntervalMs &&
         preset.chartPointLimit == chartPointLimit &&
         preset.inaAveragingSamples == inaAveragingSamples &&
         preset.inaBusConvTimeUs == inaBusConvTimeUs &&
         preset.inaShuntConvTimeUs == inaShuntConvTimeUs;
}

}  // namespace

const SamplingProfilePreset* samplingProfilePresets(size_t& count) {
  count = sizeof(kSamplingProfiles) / sizeof(kSamplingProfiles[0]);
  return kSamplingProfiles;
}

const SamplingProfilePreset* findSamplingProfilePreset(const String& id) {
  size_t count = 0U;
  const SamplingProfilePreset* presets = samplingProfilePresets(count);
  for (size_t index = 0; index < count; ++index) {
    if (id == presets[index].id) {
      return &presets[index];
    }
  }

  return nullptr;
}

const SamplingProfilePreset* matchSamplingProfilePreset(uint32_t sampleIntervalMs,
                                                        uint32_t sensorPollIntervalMs,
                                                        uint32_t historyIntervalMs,
                                                        uint32_t chartPointLimit,
                                                        uint32_t inaAveragingSamples,
                                                        uint32_t inaBusConvTimeUs,
                                                        uint32_t inaShuntConvTimeUs) {
  size_t count = 0U;
  const SamplingProfilePreset* presets = samplingProfilePresets(count);
  for (size_t index = 0; index < count; ++index) {
    if (samplingProfileMatches(presets[index],
                               sampleIntervalMs,
                               sensorPollIntervalMs,
                               historyIntervalMs,
                               chartPointLimit,
                               inaAveragingSamples,
                               inaBusConvTimeUs,
                               inaShuntConvTimeUs)) {
      return &presets[index];
    }
  }

  return nullptr;
}

bool looksLikeLegacyAggressiveSamplingConfig(uint32_t sampleIntervalMs,
                                             uint32_t sensorPollIntervalMs,
                                             uint32_t historyIntervalMs,
                                             uint32_t chartPointLimit,
                                             uint32_t inaAveragingSamples,
                                             uint32_t inaBusConvTimeUs,
                                             uint32_t inaShuntConvTimeUs) {
  return sampleIntervalMs == 1000U && sensorPollIntervalMs == 1U &&
         historyIntervalMs == 100U && chartPointLimit == 300U &&
         inaAveragingSamples == 1U && inaBusConvTimeUs == 140U &&
         inaShuntConvTimeUs == 140U;
}
