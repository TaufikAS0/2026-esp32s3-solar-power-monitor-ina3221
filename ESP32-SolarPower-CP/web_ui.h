#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include "backend_sender.h"
#include "history_buffer.h"
#include "i2c_scanner.h"
#include "ina_sensors.h"
#include "led_pwm_controller.h"
#include "raw_history_buffer.h"
#include "analysis_engine.h"
#include "ota_service.h"
#include "system_state.h"
#include "wifi_service.h"

class WebUi {
public:
  WebUi(WifiService& wifiService,
        OtaService& otaService,
        BackendSender& backendSender,
        InaSensors& sensors,
        LedPwmController& ledPwmController,
        AnalysisSnapshot& analysis,
        I2cScanner& i2cScanner,
        RawHistoryBuffer& rawHistoryBuffer,
        HistoryBuffer& historyBuffer,
        HistoryBuffer& minuteHistoryBuffer,
        PowerSystemState& currentState);

  void begin();
  void update();

private:
  WebServer server_{80};
  WifiService& wifiService_;
  OtaService& otaService_;
  BackendSender& backendSender_;
  InaSensors& sensors_;
  LedPwmController& ledPwmController_;
  AnalysisSnapshot& analysis_;
  I2cScanner& i2cScanner_;
  RawHistoryBuffer& rawHistoryBuffer_;
  HistoryBuffer& historyBuffer_;
  HistoryBuffer& minuteHistoryBuffer_;
  PowerSystemState& currentState_;

  void registerRoutes_();

  void handleRoot_();
  void handleHealth_();
  void handleStatus_();
  void handleHistory_();
  void handleLiveHistory_();
  void handleI2cScan_();
  void handleConfig_();
  void handleSaveWifi_();
  void handleSaveIna_();
  void handleSaveDevice_();
  void handleSaveFlow_();
  void handleSaveServer_();
  void handleSaveOta_();
  void handleSaveRuntime_();
  void handleSaveLedPwm_();
  void handleControlLedPwm_();
  void handleSaveBattery_();
  void handleAnalysisReset_();
  void handleReboot_();
  void handleNotFound_();

  String buildDashboardHtml_() const;
  String buildSetupHtml_() const;
};

