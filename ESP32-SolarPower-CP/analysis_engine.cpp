#include "analysis_engine.h"

#include <math.h>

#include "config.h"
#include "power_logic.h"

namespace {

constexpr float kEstimateCurrentThresholdMa = 10.0f;
uint32_t gLastCalcMs = 0;

void resetDerived(AnalysisSnapshot& data) {
  data.mode = AnalysisMode::Unavailable;
  data.pLossMw = 0.0f;
  data.efficiencyPct = 0.0f;
  data.pctLoad = 0.0f;
  data.pctBat = 0.0f;
  data.pctLoss = 0.0f;
  data.inputPowerMw = 0.0f;
  data.usefulOutputMw = 0.0f;
  data.cRate = 0.0f;
  data.estFullH = 0.0f;
  data.estRuntimeH = 0.0f;
  data.balanceValid = false;
  data.batteryEstimateValid = false;
}

}  // namespace

const char* analysisModeToText(AnalysisMode mode) {
  switch (mode) {
    case AnalysisMode::SolarInput:
      return "solar_input";
    case AnalysisMode::BatteryInput:
      return "battery_input";
    case AnalysisMode::MixedInput:
      return "mixed_input";
    case AnalysisMode::Unavailable:
    default:
      return "unavailable";
  }
}

void analysisInit(AnalysisSnapshot& data, uint32_t nowMs) {
  data = {};
  data.sessionStartMs = nowMs;
  data.sessionDurationMs = 0;
  gLastCalcMs = nowMs;
}

void analysisResetSession(AnalysisSnapshot& data, uint32_t nowMs) {
  resetDerived(data);
  data.sessionSolarWh = 0.0f;
  data.sessionLoadWh = 0.0f;
  data.sessionBatInWh = 0.0f;
  data.sessionBatOutWh = 0.0f;
  data.sessionLossWh = 0.0f;
  data.sessionStartMs = nowMs;
  data.sessionDurationMs = 0;
  gLastCalcMs = nowMs;
}

void analysisUpdate(const InaReading& solar,
                    const InaReading& battery,
                    const InaReading& load,
                    const DeviceConfig& config,
                    uint32_t nowMs,
                    AnalysisSnapshot& data) {
  const uint32_t stepMs = nowMs - gLastCalcMs;
  data.sessionDurationMs = nowMs - data.sessionStartMs;
  const float dtHours = static_cast<float>(stepMs) / 3600000.0f;

  resetDerived(data);

  const BatteryDirection batteryDirection = evaluateBatteryDirection(battery, config);
  const float batteryChargeMw =
      (battery.ok && batteryDirection == BatteryDirection::Charging) ? fabsf(battery.powerMw) : 0.0f;
  const float batteryDischargeMw =
      (battery.ok && batteryDirection == BatteryDirection::Discharging) ? fabsf(battery.powerMw) : 0.0f;
  const bool solarActiveForAnalysis = isSolarActive(solar, config);
  const float solarInputMw = solarActiveForAnalysis ? solar.powerMw : 0.0f;
  const float effectiveLoadMw = isLoadActive(load, config) ? load.powerMw : 0.0f;

  data.batteryEstimateValid = battery.ok;

  if (solarInputMw > 0.0f && batteryDischargeMw > 0.0f && effectiveLoadMw > 0.0f) {
    data.mode = AnalysisMode::MixedInput;
    data.inputPowerMw = solarInputMw + batteryDischargeMw;
    data.usefulOutputMw = effectiveLoadMw;
  } else if (solarInputMw > 0.0f && (effectiveLoadMw > 0.0f || batteryChargeMw > 0.0f)) {
    data.mode = AnalysisMode::SolarInput;
    data.inputPowerMw = solarInputMw;
    data.usefulOutputMw = effectiveLoadMw + batteryChargeMw;
  } else if (batteryDischargeMw > 0.0f && effectiveLoadMw > 0.0f) {
    data.mode = AnalysisMode::BatteryInput;
    data.inputPowerMw = batteryDischargeMw;
    data.usefulOutputMw = effectiveLoadMw;
  }

  data.balanceValid = data.mode != AnalysisMode::Unavailable;

  if (data.balanceValid && data.inputPowerMw > 0.0f) {
    data.pLossMw = data.inputPowerMw - data.usefulOutputMw;
    if (data.pLossMw < 0.0f) {
      data.pLossMw = 0.0f;
    }

    data.efficiencyPct = (data.usefulOutputMw / data.inputPowerMw) * 100.0f;
    data.efficiencyPct = constrain(data.efficiencyPct, 0.0f, 100.0f);
    data.pctLoad = constrain((effectiveLoadMw / data.inputPowerMw) * 100.0f, 0.0f, 100.0f);
    data.pctBat = constrain((batteryChargeMw / data.inputPowerMw) * 100.0f, 0.0f, 100.0f);
    data.pctLoss = constrain((data.pLossMw / data.inputPowerMw) * 100.0f, 0.0f, 100.0f);
  }

  if (battery.ok && Config::kBatteryCapacityMah > 0U) {
    data.cRate = fabsf(battery.currentMa) / static_cast<float>(Config::kBatteryCapacityMah);

    if (batteryDirection == BatteryDirection::Charging &&
        fabsf(battery.currentMa) > kEstimateCurrentThresholdMa) {
      data.estFullH = static_cast<float>(Config::kBatteryCapacityMah) / fabsf(battery.currentMa);
    } else if (batteryDirection == BatteryDirection::Discharging &&
               !solarActiveForAnalysis &&
               battery.currentMa > kEstimateCurrentThresholdMa) {
      data.estRuntimeH = static_cast<float>(Config::kBatteryCapacityMah) / battery.currentMa;
    }
  }

  if (dtHours <= 0.0f) {
    return;
  }

  if (solar.ok) {
    data.sessionSolarWh += (solar.powerMw * dtHours) / 1000.0f;
  }

  if (effectiveLoadMw > 0.0f) {
    data.sessionLoadWh += (effectiveLoadMw * dtHours) / 1000.0f;
  }

  if (batteryChargeMw > 0.0f) {
    data.sessionBatInWh += (batteryChargeMw * dtHours) / 1000.0f;
  }

  if (batteryDischargeMw > 0.0f) {
    data.sessionBatOutWh += (batteryDischargeMw * dtHours) / 1000.0f;
  }

  if (data.balanceValid) {
    data.sessionLossWh += (data.pLossMw * dtHours) / 1000.0f;
  }

  gLastCalcMs = nowMs;
}
