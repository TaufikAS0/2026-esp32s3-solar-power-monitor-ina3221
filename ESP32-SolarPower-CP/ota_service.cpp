#include "ota_service.h"

#include <ArduinoOTA.h>

#include "config.h"

OtaService::OtaService(WifiService& wifiService) : wifiService_(wifiService) {}

void OtaService::begin() {
  tryStart_();
}

void OtaService::update() {
  const bool shouldRun = wifiService_.config().otaEnabled && wifiService_.isStaConnected();
  if (!shouldRun) {
    if (started_) {
      ArduinoOTA.end();
      started_ = false;
      transferInProgress_ = false;
    }
    state_ = wifiService_.config().otaEnabled ? "waiting" : "off";
    message_ = wifiService_.config().otaEnabled ? "OTA enabled, waiting for STA connection."
                                                : "OTA disabled in config.";
    return;
  }

  if (!started_) {
    tryStart_();
    return;
  }

  ArduinoOTA.handle();
}

bool OtaService::enable() {
  if (!wifiService_.isStaConnected()) {
    state_ = "waiting";
    message_ = "Wi-Fi STA not connected yet. OTA will start after network is ready.";
    return false;
  }

  tryStart_();
  return started_;
}

void OtaService::disable(const String& reason) {
  if (started_) {
    ArduinoOTA.end();
  }

  started_ = false;
  transferInProgress_ = false;
  state_ = "off";
  message_ = reason;
}

bool OtaService::isConfiguredEnabled() const {
  return wifiService_.config().otaEnabled;
}

bool OtaService::isRuntimeActive() const {
  return started_ && wifiService_.isStaConnected() && wifiService_.config().otaEnabled;
}

bool OtaService::isTransferInProgress() const {
  return transferInProgress_;
}

uint16_t OtaService::port() const {
  return Config::kArduinoOtaPort;
}

String OtaService::hostname() const {
  return hostname_;
}

String OtaService::stateText() const {
  return state_;
}

String OtaService::messageText() const {
  return message_;
}

void OtaService::tryStart_() {
  if (started_ || !wifiService_.config().otaEnabled || !wifiService_.isStaConnected()) {
    if (wifiService_.config().otaEnabled && !wifiService_.isStaConnected()) {
      state_ = "waiting";
      message_ = "OTA enabled, waiting for STA connection.";
    }
    return;
  }

  configure_();
  ArduinoOTA.begin();
  started_ = true;
  state_ = "advertising";

  message_ = String("Arduino OTA active at ") + hostname_ + ".local:" + Config::kArduinoOtaPort;

  Serial.print(F("[OTA] Ready at "));
  Serial.print(hostname_);
  Serial.print(F(":"));
  Serial.println(Config::kArduinoOtaPort);
}

void OtaService::configure_() {
  hostname_ = buildHostname_();
  ArduinoOTA.setHostname(hostname_.c_str());
  ArduinoOTA.setPort(Config::kArduinoOtaPort);
  ArduinoOTA.setMdnsEnabled(true);
  ArduinoOTA.setRebootOnSuccess(true);

  ArduinoOTA.onStart([this]() {
    transferInProgress_ = true;
    state_ = "transferring";
    message_ = "Arduino OTA transfer in progress.";
    Serial.println(F("[OTA] Start"));
  });

  ArduinoOTA.onEnd([this]() {
    transferInProgress_ = false;
    state_ = "advertising";
    message_ = String("Arduino OTA active at ") + hostname_ + ".local:" + Config::kArduinoOtaPort;
    Serial.println(F("[OTA] End"));
  });

  ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) {
    const unsigned int percent = (total == 0U) ? 0U : ((progress * 100U) / total);
    transferInProgress_ = true;
    state_ = "transferring";
    message_ = String("Arduino OTA transfer ") + percent + "%";
    Serial.printf("[OTA] Progress: %u%%\n", percent);
  });

  ArduinoOTA.onError([this](ota_error_t error) {
    transferInProgress_ = false;
    state_ = "error";
    message_ = String("Arduino OTA error ") + static_cast<unsigned int>(error);
    Serial.printf("[OTA] Error[%u]\n", static_cast<unsigned int>(error));
  });
}

String OtaService::buildHostname_() const {
  String hostname = wifiService_.config().deviceId;
  if (hostname.isEmpty()) {
    hostname = "esp32-solar-monitor";
  }

  hostname.replace(" ", "-");
  hostname.replace("_", "-");
  hostname.toLowerCase();
  return hostname;
}
