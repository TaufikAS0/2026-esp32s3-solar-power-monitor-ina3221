#include "analysis_engine.h"
#include "backend_sender.h"
#include "config.h"
#include "history_buffer.h"
#include "i2c_scanner.h"
#include "ina_sensors.h"
#include "led_pwm_controller.h"
#include "ota_service.h"
#include "power_logic.h"
#include "raw_history_buffer.h"
#include "serial_reporter.h"
#include "system_state.h"
#include "web_ui.h"
#include "wifi_service.h"

namespace {

WifiService gWifiService;
#include <algorithm>

InaSensors gInaSensors;
AnalysisSnapshot gAnalysis;
I2cScanner gI2cScanner;
LedPwmController gLedPwmController;
RawHistoryBuffer gRawHistoryBuffer;
HistoryBuffer gHistoryBuffer;
HistoryBuffer gMinuteHistoryBuffer;
SerialReporter gSerialReporter;
PowerSystemState gCurrentState = PowerSystemState::SensorError;
OtaService gOtaService(gWifiService);
BackendSender gBackendSender(gWifiService, gOtaService, gInaSensors, gAnalysis, gCurrentState);
WebUi gWebUi(gWifiService,
             gOtaService,
             gBackendSender,
             gInaSensors,
             gLedPwmController,
             gAnalysis,
             gI2cScanner,
             gRawHistoryBuffer,
             gHistoryBuffer,
             gMinuteHistoryBuffer,
             gCurrentState);

uint32_t gLastHistorySampleMs = 0;
uint32_t gLastMinuteHistorySampleMs = 0;
uint32_t gLastBackendCaptureMs = 0;

void captureHistoryPoint(HistoryBuffer& buffer, uint32_t nowMs) {
  buffer.add(nowMs,
             gInaSensors.solar().powerMw,
             gInaSensors.battery().powerMw,
             computeBatteryPowerSignedMw(gInaSensors.battery(), gWifiService.config()),
             gInaSensors.load().powerMw,
             gInaSensors.solar().loadVoltageV,
             gInaSensors.battery().loadVoltageV,
             gInaSensors.load().loadVoltageV,
             gInaSensors.solar().currentMa,
             gInaSensors.battery().currentMa,
             gInaSensors.load().currentMa);
}

float representativeHistoryValue(const HistoryBuffer& buffer,
                                 uint32_t windowStartMs,
                                 float HistoryPoint::* field,
                                 bool* hasSamples = nullptr) {
  float values[Config::kHistoryCapacity];
  size_t sampleCount = 0U;
  const size_t availableCount = buffer.count();
  HistoryPoint point;

  for (size_t index = 0U; index < availableCount; ++index) {
    if (!buffer.getOrdered(index, point)) {
      continue;
    }
    if (point.timestampMs < windowStartMs) {
      continue;
    }
    values[sampleCount++] = point.*field;
  }

  if (hasSamples != nullptr) {
    *hasSamples = sampleCount > 0U;
  }

  if (sampleCount == 0U) {
    return 0.0f;
  }

  std::sort(values, values + sampleCount);
  return values[sampleCount / 2U];
}

void captureMinuteHistoryPoint(uint32_t nowMs) {
  const uint32_t windowStartMs =
      nowMs > Config::kMinuteHistoryIntervalMs ? nowMs - Config::kMinuteHistoryIntervalMs : 0U;
  bool hasSamples = false;
  const float solarPowerMw =
      representativeHistoryValue(gHistoryBuffer, windowStartMs, &HistoryPoint::solarPowerMw, &hasSamples);
  if (!hasSamples) {
    captureHistoryPoint(gMinuteHistoryBuffer, nowMs);
    return;
  }

  gMinuteHistoryBuffer.add(
      nowMs,
      solarPowerMw,
      representativeHistoryValue(gHistoryBuffer, windowStartMs, &HistoryPoint::batteryPowerMw),
      representativeHistoryValue(gHistoryBuffer, windowStartMs, &HistoryPoint::batteryPowerSignedMw),
      representativeHistoryValue(gHistoryBuffer, windowStartMs, &HistoryPoint::loadPowerMw),
      representativeHistoryValue(gHistoryBuffer, windowStartMs, &HistoryPoint::solarVoltageV),
      representativeHistoryValue(gHistoryBuffer, windowStartMs, &HistoryPoint::batteryVoltageV),
      representativeHistoryValue(gHistoryBuffer, windowStartMs, &HistoryPoint::loadVoltageV),
      representativeHistoryValue(gHistoryBuffer, windowStartMs, &HistoryPoint::solarCurrentMa),
      representativeHistoryValue(gHistoryBuffer, windowStartMs, &HistoryPoint::batteryCurrentMa),
      representativeHistoryValue(gHistoryBuffer, windowStartMs, &HistoryPoint::loadCurrentMa));
}

void captureRawHistoryPoint(RawHistoryBuffer& buffer, uint32_t nowMs) {
  buffer.add(nowMs,
             gInaSensors.solar().loadVoltageV,
             gInaSensors.battery().loadVoltageV,
             gInaSensors.load().loadVoltageV,
             gInaSensors.solar().currentMa,
             gInaSensors.battery().currentMa,
             gInaSensors.load().currentMa);
}

void updateHistory(uint32_t nowMs) {
  if (nowMs - gLastHistorySampleMs >= gWifiService.config().historyIntervalMs) {
    gLastHistorySampleMs = nowMs;
    captureHistoryPoint(gHistoryBuffer, nowMs);
  }

  if (nowMs - gLastMinuteHistorySampleMs >= Config::kMinuteHistoryIntervalMs) {
    gLastMinuteHistorySampleMs = nowMs;
    captureMinuteHistoryPoint(nowMs);
  }
}

void updateBackendCapture(uint32_t nowMs) {
  if (nowMs - gLastBackendCaptureMs < gWifiService.config().sampleIntervalMs) {
    return;
  }

  gLastBackendCaptureMs = nowMs;
  gBackendSender.captureSample(nowMs);
}

void updateSystemState() {
  gCurrentState =
      evaluatePowerSystemState(gInaSensors.solar(), gInaSensors.battery(), gWifiService.config());
}

}  // namespace

void setup() {
  gSerialReporter.begin();
  gWifiService.begin();
  gLedPwmController.begin(gWifiService.config());
  gInaSensors.applyConfig(gWifiService.config());
  gInaSensors.setSampleIntervalMs(gWifiService.config().sensorPollIntervalMs);
  gInaSensors.begin();
  captureRawHistoryPoint(gRawHistoryBuffer, gInaSensors.lastReadMs());
  analysisInit(gAnalysis, millis());
  updateSystemState();
  gOtaService.begin();
  gBackendSender.begin();
  gWebUi.begin();
}

void loop() {
  const uint32_t nowMs = millis();

  gWifiService.update(nowMs);
  gLedPwmController.update(nowMs);
  const uint32_t previousReadMs = gInaSensors.lastReadMs();
  gInaSensors.update(nowMs);
  const uint32_t latestReadMs = gInaSensors.lastReadMs();
  if (latestReadMs != previousReadMs) {
    captureRawHistoryPoint(gRawHistoryBuffer, latestReadMs);
  }
  analysisUpdate(gInaSensors.solar(),
                 gInaSensors.battery(),
                 gInaSensors.load(),
                 gWifiService.config(),
                 nowMs,
                 gAnalysis);
  updateSystemState();
  updateHistory(nowMs);
  updateBackendCapture(nowMs);
  gOtaService.update();
  gWebUi.update();
  gSerialReporter.update(nowMs, gWifiService, gInaSensors.solar(), gInaSensors.battery(), gCurrentState);
  gBackendSender.update(nowMs);
}
