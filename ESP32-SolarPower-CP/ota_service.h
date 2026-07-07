#pragma once

#include <Arduino.h>

#include "wifi_service.h"

class OtaService {
public:
  explicit OtaService(WifiService& wifiService);

  void begin();
  void update();
  bool enable();
  void disable(const String& reason);

  bool isConfiguredEnabled() const;
  bool isRuntimeActive() const;
  bool isTransferInProgress() const;
  uint16_t port() const;
  String hostname() const;
  String stateText() const;
  String messageText() const;

private:
  WifiService& wifiService_;
  bool started_ = false;
  bool transferInProgress_ = false;
  String hostname_;
  String state_ = "off";
  String message_ = "Arduino OTA disabled.";

  void tryStart_();
  void configure_();
  String buildHostname_() const;
};
