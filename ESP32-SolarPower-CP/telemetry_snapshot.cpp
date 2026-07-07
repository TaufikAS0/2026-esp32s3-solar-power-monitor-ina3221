#include "telemetry_snapshot.h"

#include <math.h>
#include <stdio.h>

#include "battery_profile.h"
#include "config.h"
#include "firmware_version.h"

namespace {

void copyBounded(char* target, size_t size, const String& value) {
  if (size == 0) {
    return;
  }
  snprintf(target, size, "%s", value.c_str());
}

String frameString(const char* value) {
  return value[0] == '\0' ? String("") : String(value);
}

String resolvedLineId(const DeviceConfig& config) {
  if (!config.lineId.isEmpty()) {
    return config.lineId;
  }
  return config.deviceId;
}

}  // namespace

void appendReading(JsonObject object, const InaReading& reading) {
  object["ok"] = reading.ok;
  object["voltage"] = reading.loadVoltageV;
  object["bus_voltage"] = reading.busVoltageV;
  object["load_voltage"] = reading.loadVoltageV;
  object["shunt_mv"] = reading.shuntVoltageMv;
  object["current_ma"] = reading.currentMa;
  object["power_mw"] = reading.powerMw;
}

void appendAnalysis(JsonObject object, const AnalysisSnapshot& analysis) {
  object["analysis_mode"] = analysisModeToText(analysis.mode);
  object["p_loss_mw"] = analysis.pLossMw;
  object["efficiency_pct"] = analysis.efficiencyPct;
  object["pct_load"] = analysis.pctLoad;
  object["pct_bat"] = analysis.pctBat;
  object["pct_loss"] = analysis.pctLoss;
  object["input_power_mw"] = analysis.inputPowerMw;
  object["useful_output_mw"] = analysis.usefulOutputMw;
  object["c_rate"] = analysis.cRate;
  object["est_full_h"] = analysis.estFullH;
  object["est_runtime_h"] = analysis.estRuntimeH;
  object["session_solar_wh"] = analysis.sessionSolarWh;
  object["session_load_wh"] = analysis.sessionLoadWh;
  object["session_bat_wh"] = analysis.sessionBatInWh;
  object["session_bat_in_wh"] = analysis.sessionBatInWh;
  object["session_bat_out_wh"] = analysis.sessionBatOutWh;
  object["session_loss_wh"] = analysis.sessionLossWh;
  object["session_start_ms"] = analysis.sessionStartMs;
  object["session_duration_ms"] = analysis.sessionDurationMs;
  object["balance_valid"] = analysis.balanceValid;
  object["battery_estimate_valid"] = analysis.batteryEstimateValid;
}

void appendBatteryProfileFields(JsonObject object, const DeviceConfig& config) {
  object["battery_profile_id"] = config.batteryProfileId;
  object["battery_profile_label"] = batteryProfileLabel(config.batteryProfileId);
  object["battery_full_voltage_v"] = static_cast<float>(config.batteryFullMv) / 1000.0f;
  object["battery_empty_voltage_v"] = static_cast<float>(config.batteryEmptyMv) / 1000.0f;
}

void appendBatteryProfileFields(JsonObject object, const TelemetryFrame& frame) {
  const String profileId = frameString(frame.batteryProfileId);
  object["battery_profile_id"] = profileId;
  object["battery_profile_label"] = batteryProfileLabel(profileId);
  object["battery_full_voltage_v"] = static_cast<float>(frame.batteryFullMv) / 1000.0f;
  object["battery_empty_voltage_v"] = static_cast<float>(frame.batteryEmptyMv) / 1000.0f;
}

String sensorHealthText(const InaReading& solar, const InaReading& battery, const InaReading& load) {
  String text = "INA3221 @0x40";
  text += solar.ok ? " | Solar CH1 OK" : " | Solar CH1 error";
  text += battery.ok ? " | Battery CH2 OK" : " | Battery CH2 error";
  text += load.ok ? " | Load CH3 OK" : " | Load CH3 error";
  return text;
}

String visualWarningText(const InaReading& solar, const InaReading& battery) {
  if (!solar.ok && !battery.ok) {
    return "INA3221 CH1 solar unavailable | CH2 battery unavailable";
  }
  if (!solar.ok && battery.ok) {
    return "INA3221 CH1 solar unavailable, battery flow inferred";
  }
  if (solar.ok && !battery.ok) {
    return "INA3221 CH2 battery unavailable, battery flow unavailable";
  }
  return "";
}

void captureTelemetryFrame(TelemetryFrame& frame,
                           uint32_t timestampMs,
                           const DeviceConfig& config,
                           const InaReading& solar,
                           const InaReading& battery,
                           const InaReading& load,
                           const AnalysisSnapshot& analysis,
                           PowerSystemState state) {
  frame.timestampMs = timestampMs;
  frame.solar = solar;
  frame.battery = battery;
  frame.load = load;
  frame.analysis = analysis;
  frame.state = state;
  frame.solarActive = isSolarActive(solar, config);
  frame.loadActive = isLoadActive(load, config);
  frame.batteryDirection = evaluateBatteryDirection(battery, config);
  frame.batteryPowerSignedMw = computeBatteryPowerSignedMw(battery, config);
  frame.visualMode = evaluateVisualMode(solar, battery, config);
  frame.sampleIntervalMs = config.sampleIntervalMs;
  frame.batteryFullMv = config.batteryFullMv;
  frame.batteryEmptyMv = config.batteryEmptyMv;
  frame.batteryPercentValid =
      battery.ok && isBatteryProfileRangeValid(config.batteryFullMv, config.batteryEmptyMv);
  frame.batteryPercent =
      frame.batteryPercentValid
          ? computeBatteryPercent(battery.loadVoltageV, config.batteryFullMv, config.batteryEmptyMv)
          : 0.0f;

  copyBounded(frame.deviceId, sizeof(frame.deviceId), config.deviceId);
  copyBounded(frame.lineId, sizeof(frame.lineId), resolvedLineId(config));
  copyBounded(frame.batteryProfileId, sizeof(frame.batteryProfileId), config.batteryProfileId);
}

void appendTelemetrySample(JsonObject object, const TelemetryFrame& frame) {
  object["timestamp_ms"] = frame.timestampMs;
  object["firmware_version"] = FirmwareInfo::kVersion;
  object["release_label"] = FirmwareInfo::kReleaseLabel;
  object["device_id"] = frame.deviceId;
  object["line_id"] = frame.lineId;
  object["state"] = powerSystemStateToText(frame.state);
  object["state_label"] = powerSystemStateToLabel(frame.state);
  object["sensor_health"] = sensorHealthText(frame.solar, frame.battery, frame.load);
  object["solar_active"] = frame.solarActive;
  object["load_active"] = frame.loadActive;
  object["battery_direction"] = batteryDirectionToText(frame.batteryDirection);
  object["battery_power_signed_mw"] = frame.batteryPowerSignedMw;
  object["visual_mode"] = visualModeToText(frame.visualMode);
  object["visual_warning"] = visualWarningText(frame.solar, frame.battery);
  object["sample_interval_ms"] = frame.sampleIntervalMs;
  object["display_unit_mode"] = "milli";
  object["battery_capacity_mah"] = Config::kBatteryCapacityMah;
  object["battery_percent"] = frame.batteryPercent;
  object["battery_percent_valid"] = frame.batteryPercentValid;
  appendBatteryProfileFields(object, frame);

  JsonObject solarObject = object.createNestedObject("solar");
  appendReading(solarObject, frame.solar);

  JsonObject batteryObject = object.createNestedObject("battery");
  appendReading(batteryObject, frame.battery);

  JsonObject loadObject = object.createNestedObject("load");
  appendReading(loadObject, frame.load);

  JsonObject analysisObject = object.createNestedObject("analysis");
  appendAnalysis(analysisObject, frame.analysis);
}
