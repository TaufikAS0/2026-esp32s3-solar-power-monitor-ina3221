#include "backend_sender.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClient.h>

#include "config.h"
#include "firmware_version.h"

namespace {

bool isUnreservedPathChar(char value) {
  return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') ||
         (value >= '0' && value <= '9') || value == '-' || value == '.' || value == '_' ||
         value == '~';
}

String urlEncodePathSegment(const String& value) {
  static constexpr char kHex[] = "0123456789ABCDEF";

  String encoded;
  encoded.reserve(value.length() * 3);
  for (size_t index = 0; index < value.length(); ++index) {
    const uint8_t ch = static_cast<uint8_t>(value.charAt(index));
    if (isUnreservedPathChar(static_cast<char>(ch))) {
      encoded += static_cast<char>(ch);
      continue;
    }

    encoded += '%';
    encoded += kHex[(ch >> 4) & 0x0F];
    encoded += kHex[ch & 0x0F];
  }

  return encoded;
}

bool isSuccessStatus(uint16_t statusCode) {
  return statusCode >= 200 && statusCode < 300;
}

String summarizeTransportResult(uint16_t statusCode, const String& responseBody) {
  String summary;
  if (statusCode > 0) {
    summary = "HTTP ";
    summary += statusCode;
    if (!responseBody.isEmpty()) {
      summary += ": ";
    }
  }

  summary += responseBody;
  summary.replace("\r", " ");
  summary.replace("\n", " ");
  summary.trim();
  if (summary.length() > 180) {
    summary.remove(180);
    summary += "...";
  }
  return summary.isEmpty() ? String("request failed") : summary;
}

}  // namespace

BackendSender::BackendSender(WifiService& wifiService,
                             OtaService& otaService,
                             InaSensors& sensors,
                             AnalysisSnapshot& analysis,
                             PowerSystemState& currentState)
    : wifiService_(wifiService),
      otaService_(otaService),
      sensors_(sensors),
      analysis_(analysis),
      currentState_(currentState) {}

void BackendSender::begin() {
  if (mutex_ == nullptr) {
    mutex_ = xSemaphoreCreateMutex();
  }

  clearQueue_();
  clearRetryBatch_();
  runtime_ = BackendSenderRuntime{};
  runtime_.queueCapacity = Config::kBackendQueueCapacity;
  senderWasEnabled_ = wifiService_.config().serverEnabled;
  observedDeviceId_ = wifiService_.config().deviceId;
  observedLineId_ = resolvedLineId_(wifiService_.config());
  observedApiBase_ = normalizedBase_(wifiService_.config().apiBase);
  observedApiKey_ = wifiService_.config().apiKey;

  if (taskHandle_ == nullptr) {
    xTaskCreatePinnedToCore(taskEntryPoint_,
                            "backendSender",
                            12288,
                            this,
                            1,
                            &taskHandle_,
                            0);
  }
}

void BackendSender::captureSample(uint32_t nowMs) {
  if (!wifiService_.config().serverEnabled || mutex_ == nullptr) {
    return;
  }

  TelemetryFrame frame;
  captureTelemetryFrame(frame,
                        nowMs,
                        wifiService_.config(),
                        sensors_.solar(),
                        sensors_.battery(),
                        sensors_.load(),
                        analysis_,
                        currentState_);

  if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
    return;
  }

  frame.sequence = nextSequence_++;
  enqueueFrame_(frame);
  if (nextTelemetryAttemptAtMs_ == 0 && retryBatchCount_ == 0) {
    nextTelemetryAttemptAtMs_ = nowMs + Config::kBackendTelemetryAttemptMs;
  }
  runtime_.queueDepth = queueCount_;
  runtime_.queueCapacity = Config::kBackendQueueCapacity;
  runtime_.retryBatchDepth = retryBatchCount_;
  runtime_.telemetryDueAtMs = nextTelemetryAttemptAtMs_;
  runtime_.lastHeartbeatAttemptMs = lastHeartbeatAttemptMs_;
  runtime_.nextRetryInMs =
      nextTelemetryAttemptAtMs_ > nowMs ? nextTelemetryAttemptAtMs_ - nowMs : 0;
  xSemaphoreGive(mutex_);
}

void BackendSender::update(uint32_t) {
  // Transport work runs on the sender task so the main loop stays responsive.
}

BackendSenderRuntime BackendSender::runtimeSnapshot() const {
  BackendSenderRuntime snapshot = runtime_;
  if (mutex_ == nullptr) {
    return snapshot;
  }

  if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
    snapshot = runtime_;
    snapshot.enabled = wifiService_.config().serverEnabled;
    snapshot.queueDepth = queueCount_;
    snapshot.queueCapacity = Config::kBackendQueueCapacity;
    snapshot.retryBatchDepth = retryBatchCount_;
    snapshot.telemetryDueAtMs = nextTelemetryAttemptAtMs_;
    snapshot.lastHeartbeatAttemptMs = lastHeartbeatAttemptMs_;
    const uint32_t nowMs = millis();
    snapshot.nextRetryInMs =
        (nextTelemetryAttemptAtMs_ != 0 &&
         static_cast<int32_t>(nextTelemetryAttemptAtMs_ - nowMs) > 0)
            ? nextTelemetryAttemptAtMs_ - nowMs
            : 0;
    xSemaphoreGive(mutex_);
  }

  return snapshot;
}

void BackendSender::taskEntryPoint_(void* argument) {
  static_cast<BackendSender*>(argument)->runTaskLoop_();
}

void BackendSender::runTaskLoop_() {
  while (true) {
    const uint32_t nowMs = millis();
    observeConfigChanges_();
    refreshRuntime_(nowMs);

    if (!wifiService_.config().serverEnabled) {
      if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        runtime_.state = "disabled";
        runtime_.lastError = "backend sender disabled";
        xSemaphoreGive(mutex_);
      }
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    if (otaService_.isTransferInProgress()) {
      if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        runtime_.state = "ota_paused";
        runtime_.lastError = "paused during ota transfer";
        xSemaphoreGive(mutex_);
      }
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    if (!canTransport_()) {
      if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        if (wifiService_.config().apiBase.isEmpty()) {
          runtime_.state = "config";
          runtime_.lastError = "backend api base not configured";
        } else if (!wifiService_.isStaConnected()) {
          runtime_.state = "waiting_wifi";
          runtime_.lastError = "waiting for sta connection";
        } else {
          runtime_.state = "waiting_sample";
          runtime_.lastError = "waiting for first sample";
        }
        xSemaphoreGive(mutex_);
      }
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    bool telemetryReady = false;
    bool heartbeatReady = false;
    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
      telemetryReady =
          queueCount_ > 0 && nextTelemetryAttemptAtMs_ != 0 &&
          static_cast<int32_t>(nowMs - nextTelemetryAttemptAtMs_) >= 0;
      heartbeatReady =
          static_cast<int32_t>(nowMs - lastHeartbeatAttemptMs_) >=
          static_cast<int32_t>(Config::kBackendHeartbeatIntervalMs);
      xSemaphoreGive(mutex_);
    }

    if (telemetryReady) {
      sendTelemetry_(nowMs);
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    if (heartbeatReady) {
      sendHeartbeat_(nowMs);
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
      if (queueCount_ > 0) {
        runtime_.state = telemetryFailureStreak_ > 0 ? "backoff" : "queueing";
      } else {
        runtime_.state = "idle";
        if (runtime_.lastError.isEmpty()) {
          runtime_.lastError = "sender idle";
        }
      }
      xSemaphoreGive(mutex_);
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void BackendSender::clearQueue_() {
  queueHead_ = 0;
  queueCount_ = 0;
}

void BackendSender::clearRetryBatch_() {
  retryBatchCount_ = 0;
  retryBatchSentAtMs_ = 0;
  retryBatchLastSequence_ = 0;
}

void BackendSender::resetTransportState_(const String& message, bool clearQueue) {
  if (clearQueue) {
    clearQueue_();
  }
  clearRetryBatch_();
  nextTelemetryAttemptAtMs_ = 0;
  telemetryFailureStreak_ = 0;
  runtime_.nextRetryInMs = 0;
  runtime_.state = wifiService_.config().serverEnabled ? "idle" : "disabled";
  runtime_.lastError = message;
}

void BackendSender::observeConfigChanges_() {
  const DeviceConfig& config = wifiService_.config();
  const String currentLineId = resolvedLineId_(config);
  const String currentBase = normalizedBase_(config.apiBase);
  const bool enabled = config.serverEnabled;
  const bool changed =
      enabled != senderWasEnabled_ || config.deviceId != observedDeviceId_ ||
      currentLineId != observedLineId_ || currentBase != observedApiBase_ ||
      config.apiKey != observedApiKey_;

  if (!changed || mutex_ == nullptr) {
    return;
  }

  if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
    if (!enabled) {
      resetTransportState_("backend sender disabled", true);
    } else if (senderWasEnabled_) {
      resetTransportState_("sender queue reset after config change", true);
    }
    xSemaphoreGive(mutex_);
  }

  senderWasEnabled_ = enabled;
  observedDeviceId_ = config.deviceId;
  observedLineId_ = currentLineId;
  observedApiBase_ = currentBase;
  observedApiKey_ = config.apiKey;
}

void BackendSender::refreshRuntime_(uint32_t nowMs) {
  if (mutex_ == nullptr) {
    return;
  }

  if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
    return;
  }

  runtime_.enabled = wifiService_.config().serverEnabled;
  runtime_.queueDepth = queueCount_;
  runtime_.queueCapacity = Config::kBackendQueueCapacity;
  runtime_.retryBatchDepth = retryBatchCount_;
  runtime_.telemetryDueAtMs = nextTelemetryAttemptAtMs_;
  runtime_.lastHeartbeatAttemptMs = lastHeartbeatAttemptMs_;
  if (runtime_.enabled && queueCount_ > 0 && nextTelemetryAttemptAtMs_ == 0) {
    nextTelemetryAttemptAtMs_ =
        nowMs + (retryBatchCount_ > 0 ? Config::kBackendRetryMinMs
                                      : Config::kBackendTelemetryAttemptMs);
  }
  runtime_.telemetryDueAtMs = nextTelemetryAttemptAtMs_;
  runtime_.nextRetryInMs =
      (nextTelemetryAttemptAtMs_ != 0 &&
       static_cast<int32_t>(nextTelemetryAttemptAtMs_ - nowMs) > 0)
          ? nextTelemetryAttemptAtMs_ - nowMs
          : 0;

  xSemaphoreGive(mutex_);
}

void BackendSender::enqueueFrame_(const TelemetryFrame& frame) {
  if (queueCount_ == Config::kBackendQueueCapacity) {
    queueHead_ = (queueHead_ + 1U) % Config::kBackendQueueCapacity;
    --queueCount_;
    ++runtime_.droppedSamples;
  }

  const size_t tail = (queueHead_ + queueCount_) % Config::kBackendQueueCapacity;
  queue_[tail] = frame;
  ++queueCount_;
}

void BackendSender::prepareRetryBatch_(uint32_t nowMs) {
  if (retryBatchCount_ > 0 || queueCount_ == 0) {
    return;
  }

  retryBatchCount_ =
      queueCount_ < Config::kBackendBatchMaxSamples ? queueCount_ : Config::kBackendBatchMaxSamples;
  for (size_t index = 0; index < retryBatchCount_; ++index) {
    const size_t queueIndex = (queueHead_ + index) % Config::kBackendQueueCapacity;
    retryBatch_[index] = queue_[queueIndex];
  }

  retryBatchSentAtMs_ = nowMs;
  retryBatchLastSequence_ = retryBatch_[retryBatchCount_ - 1].sequence;
}

void BackendSender::discardQueuedThroughSequence_(uint32_t lastSequence) {
  while (queueCount_ > 0 && queue_[queueHead_].sequence <= lastSequence) {
    queueHead_ = (queueHead_ + 1U) % Config::kBackendQueueCapacity;
    --queueCount_;
  }
}

uint32_t BackendSender::nextRetryDelayMs_() const {
  switch (telemetryFailureStreak_) {
    case 0:
    case 1:
      return 3000;
    case 2:
      return 6000;
    case 3:
      return 12000;
    default:
      return 30000;
  }
}

String BackendSender::normalizedBase_(String value) const {
  value.trim();
  if (value.isEmpty()) {
    value = Config::kDefaultApiBase;
  }

  if (value.indexOf("://") < 0) {
    value = String("http://") + value;
  }

  while (value.endsWith("/")) {
    value.remove(value.length() - 1);
  }

  if (!value.endsWith("/api/v1")) {
    value += "/api/v1";
  }

  return value;
}

String BackendSender::resolvedLineId_(const DeviceConfig& config) const {
  if (!config.lineId.isEmpty()) {
    return config.lineId;
  }
  return config.deviceId;
}

String BackendSender::telemetryEndpointUrl_(const DeviceConfig& config) const {
  String url = normalizedBase_(config.apiBase);
  url += "/devices/";
  url += urlEncodePathSegment(config.deviceId);
  url += "/telemetry/batch";
  return url;
}

String BackendSender::heartbeatEndpointUrl_(const DeviceConfig& config) const {
  String url = normalizedBase_(config.apiBase);
  url += "/devices/";
  url += urlEncodePathSegment(config.deviceId);
  url += "/heartbeat";
  return url;
}

bool BackendSender::canTransport_() const {
  return wifiService_.isStaConnected() && !wifiService_.config().apiBase.isEmpty() &&
         sensors_.lastReadMs() != 0;
}

bool BackendSender::postJson_(const String& url,
                              const String& payload,
                              uint16_t& statusCode,
                              String& responseBody) {
  WiFiClient client;
  HTTPClient http;

  http.setConnectTimeout(Config::kBackendConnectTimeoutMs);
  http.setTimeout(Config::kBackendResponseTimeoutMs);
  http.setReuse(false);
  http.useHTTP10(true);

  if (!http.begin(client, url)) {
    statusCode = 0;
    responseBody = "http begin failed";
    return false;
  }

  http.addHeader("Content-Type", "application/json");
  if (!wifiService_.config().apiKey.isEmpty()) {
    http.addHeader("X-API-Key", wifiService_.config().apiKey);
  }

  const int httpCode = http.POST(payload);
  statusCode = httpCode > 0 ? static_cast<uint16_t>(httpCode) : 0;
  if (httpCode > 0) {
    const int bodySize = http.getSize();
    if (httpCode == HTTP_CODE_NO_CONTENT || bodySize == 0) {
      responseBody = "";
    } else {
      responseBody = http.getString();
    }
  } else {
    responseBody = http.errorToString(httpCode);
  }
  http.end();

  return httpCode > 0;
}

void BackendSender::sendTelemetry_(uint32_t nowMs) {
  TelemetryFrame batch[Config::kBackendBatchMaxSamples];
  size_t batchCount = 0;
  uint32_t sentAtMs = 0;
  uint32_t lastSequence = 0;

  if (mutex_ == nullptr) {
    return;
  }

  if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
    return;
  }

  prepareRetryBatch_(nowMs);
  if (retryBatchCount_ == 0) {
    nextTelemetryAttemptAtMs_ = 0;
    xSemaphoreGive(mutex_);
    return;
  }

  batchCount = retryBatchCount_;
  sentAtMs = retryBatchSentAtMs_;
  lastSequence = retryBatchLastSequence_;
  for (size_t index = 0; index < batchCount; ++index) {
    batch[index] = retryBatch_[index];
  }

  runtime_.state = "sending_telemetry";
  runtime_.lastTelemetryAttemptMs = nowMs;
  xSemaphoreGive(mutex_);

  const DeviceConfig config = wifiService_.config();
  DynamicJsonDocument doc(12288);
  doc["schema_version"] = Config::kBackendSchemaVersion;
  doc["source_mode"] = Config::kBackendSourceMode;
  doc["device_id"] = config.deviceId;
  doc["line_id"] = resolvedLineId_(config);
  doc["firmware_version"] = FirmwareInfo::kVersion;
  doc["release_label"] = FirmwareInfo::kReleaseLabel;
  doc["sent_at_ms"] = sentAtMs;

  JsonArray samples = doc.createNestedArray("samples");
  for (size_t index = 0; index < batchCount; ++index) {
    JsonObject sample = samples.createNestedObject();
    appendTelemetrySample(sample, batch[index]);
  }

  String payload;
  payload.reserve(4096);
  serializeJson(doc, payload);

  uint16_t statusCode = 0;
  String responseBody;
  const bool requestSent =
      postJson_(telemetryEndpointUrl_(config), payload, statusCode, responseBody);
  const bool success = requestSent && isSuccessStatus(statusCode);

  if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
    return;
  }

  runtime_.lastHttpStatus = statusCode;
  runtime_.lastSendOk = success;
  runtime_.lastError =
      success ? String("telemetry sent (") + batchCount + " samples)"
              : summarizeTransportResult(statusCode, responseBody);

  if (success) {
    runtime_.lastSendAtMs = nowMs;
    discardQueuedThroughSequence_(lastSequence);
    clearRetryBatch_();
    telemetryFailureStreak_ = 0;
    nextTelemetryAttemptAtMs_ =
        queueCount_ > 0 ? nowMs + Config::kBackendTelemetryAttemptMs : 0;
    runtime_.state = queueCount_ > 0 ? "queueing" : "idle";
    xSemaphoreGive(mutex_);
    return;
  }

  if (telemetryFailureStreak_ < 4) {
    ++telemetryFailureStreak_;
  }
  nextTelemetryAttemptAtMs_ = nowMs + nextRetryDelayMs_();
  runtime_.state = "backoff";
  xSemaphoreGive(mutex_);
}

void BackendSender::sendHeartbeat_(uint32_t nowMs) {
  if (mutex_ == nullptr) {
    return;
  }

  bool lastSendOk = false;
  uint32_t lastSendAtMs = 0;
  if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
    lastHeartbeatAttemptMs_ = nowMs;
    runtime_.lastHeartbeatAttemptMs = nowMs;
    runtime_.state = "sending_heartbeat";
    lastSendOk = runtime_.lastSendOk;
    lastSendAtMs = runtime_.lastSendAtMs;
    xSemaphoreGive(mutex_);
  }

  const DeviceConfig config = wifiService_.config();
  DynamicJsonDocument doc(1024);
  doc["schema_version"] = Config::kBackendSchemaVersion;
  doc["source_mode"] = Config::kBackendSourceMode;
  doc["device_id"] = config.deviceId;
  doc["line_id"] = resolvedLineId_(config);
  doc["running"] = true;
  doc["active_scenario"] = Config::kBackendHeartbeatScenario;
  doc["backend_base_url"] = normalizedBase_(config.apiBase);
  doc["last_send_ok"] = lastSendOk;
  doc["last_send_at_ms"] = lastSendAtMs;

  String payload;
  serializeJson(doc, payload);

  uint16_t statusCode = 0;
  String responseBody;
  const bool requestSent =
      postJson_(heartbeatEndpointUrl_(config), payload, statusCode, responseBody);
  const bool success = requestSent && isSuccessStatus(statusCode);

  if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
    return;
  }

  runtime_.lastHttpStatus = statusCode;
  runtime_.lastSendOk = success;
  runtime_.lastError = success ? "heartbeat sent" : summarizeTransportResult(statusCode, responseBody);
  if (success) {
    runtime_.lastSendAtMs = nowMs;
    runtime_.lastHeartbeatAtMs = nowMs;
  }
  runtime_.state = queueCount_ > 0 ? "queueing" : "idle";

  xSemaphoreGive(mutex_);
}
