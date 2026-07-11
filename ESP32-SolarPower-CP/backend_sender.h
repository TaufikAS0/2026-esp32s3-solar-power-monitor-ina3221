#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "analysis_engine.h"
#include "config.h"
#include "ina_sensors.h"
#include "ota_service.h"
#include "system_state.h"
#include "telemetry_snapshot.h"
#include "wifi_service.h"

struct BackendSenderRuntime {
  bool enabled = false;
  bool lastSendOk = false;
  uint32_t lastSendAtMs = 0;
  uint32_t lastHeartbeatAtMs = 0;
  uint32_t lastTelemetryAttemptMs = 0;
  uint32_t lastHeartbeatAttemptMs = 0;
  uint32_t telemetryDueAtMs = 0;
  uint32_t nextRetryInMs = 0;
  uint32_t droppedSamples = 0;
  size_t queueDepth = 0;
  size_t queueCapacity = 0;
  size_t retryBatchDepth = 0;
  uint16_t lastHttpStatus = 0;
  String state = "disabled";
  String lastError = "backend sender disabled";
};

class BackendSender {
public:
  BackendSender(WifiService& wifiService,
                OtaService& otaService,
                InaSensors& sensors,
                AnalysisSnapshot& analysis,
                PowerSystemState& currentState);

  void begin();
  void captureSample(uint32_t nowMs);
  void update(uint32_t nowMs);
  void noteWebActivity(uint32_t nowMs);

  BackendSenderRuntime runtimeSnapshot() const;

private:
  WifiService& wifiService_;
  OtaService& otaService_;
  InaSensors& sensors_;
  AnalysisSnapshot& analysis_;
  PowerSystemState& currentState_;
  BackendSenderRuntime runtime_;
  TelemetryFrame queue_[Config::kBackendQueueCapacity];
  TelemetryFrame retryBatch_[Config::kBackendBatchMaxSamples];
  size_t queueHead_ = 0;
  size_t queueCount_ = 0;
  size_t retryBatchCount_ = 0;
  uint32_t retryBatchSentAtMs_ = 0;
  uint32_t retryBatchLastSequence_ = 0;
  uint32_t nextSequence_ = 1;
  uint32_t nextTelemetryAttemptAtMs_ = 0;
  uint32_t lastHeartbeatAttemptMs_ = 0;
  uint8_t telemetryFailureStreak_ = 0;
  bool senderWasEnabled_ = false;
  volatile uint32_t lastWebActivityMs_ = 0;
  String observedDeviceId_;
  String observedLineId_;
  String observedApiBase_;
  String observedApiKey_;
  SemaphoreHandle_t mutex_ = nullptr;
  TaskHandle_t taskHandle_ = nullptr;

  static void taskEntryPoint_(void* argument);
  void runTaskLoop_();
  void clearQueue_();
  void clearRetryBatch_();
  void resetTransportState_(const String& message, bool clearQueue);
  void observeConfigChanges_();
  void refreshRuntime_(uint32_t nowMs);
  void enqueueFrame_(const TelemetryFrame& frame);
  void prepareRetryBatch_(uint32_t nowMs);
  void discardQueuedThroughSequence_(uint32_t lastSequence);
  uint32_t nextRetryDelayMs_() const;
  String normalizedBase_(String value) const;
  String resolvedLineId_(const DeviceConfig& config) const;
  String telemetryEndpointUrl_(const DeviceConfig& config) const;
  String heartbeatEndpointUrl_(const DeviceConfig& config) const;
  bool canTransport_() const;
  bool isWebActivityRecent_(uint32_t nowMs) const;
  bool postJson_(const String& url,
                 const String& payload,
                 uint16_t& statusCode,
                 String& responseBody);
  void sendTelemetry_(uint32_t nowMs);
  void sendHeartbeat_(uint32_t nowMs);
};
