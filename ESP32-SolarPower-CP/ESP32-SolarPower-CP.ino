#include "analysis_engine.h"
#include "backend_sender.h"
#include "config.h"
#include "history_buffer.h"
#include "i2c_scanner.h"
#include "ina_sensors.h"
#include "led_pwm_controller.h"
#include "ota_service.h"
#include "power_logic.h"
#include "serial_reporter.h"
#include "system_state.h"
#include "web_ui.h"
#include "wifi_service.h"

namespace {

WifiService gWifiService;
InaSensors gInaSensors;
AnalysisSnapshot gAnalysis;
I2cScanner gI2cScanner;
LedPwmController gLedPwmController;
HistoryBuffer gHistoryBuffer;
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
             gHistoryBuffer,
             gCurrentState);

uint32_t gLastHistorySampleMs = 0;
uint32_t gLastBackendCaptureMs = 0;

void updateHistory(uint32_t nowMs) {
  if (nowMs - gLastHistorySampleMs < gWifiService.config().historyIntervalMs) {
    return;
  }

  gLastHistorySampleMs = nowMs;
  gHistoryBuffer.add(nowMs,
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
  gInaSensors.update(nowMs);
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
