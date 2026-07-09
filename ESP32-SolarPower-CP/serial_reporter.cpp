#include "serial_reporter.h"

#include <esp_system.h>

#include "config.h"

namespace {

const char* resetReasonName(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_UNKNOWN:
      return "UNKNOWN";
    case ESP_RST_POWERON:
      return "POWERON";
    case ESP_RST_EXT:
      return "EXT_PIN";
    case ESP_RST_SW:
      return "SOFTWARE";
    case ESP_RST_PANIC:
      return "PANIC";
    case ESP_RST_INT_WDT:
      return "INT_WDT";
    case ESP_RST_TASK_WDT:
      return "TASK_WDT";
    case ESP_RST_WDT:
      return "OTHER_WDT";
    case ESP_RST_DEEPSLEEP:
      return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:
      return "BROWNOUT";
    case ESP_RST_SDIO:
      return "SDIO";
    case ESP_RST_USB:
      return "USB";
    case ESP_RST_JTAG:
      return "JTAG";
    case ESP_RST_EFUSE:
      return "EFUSE";
    case ESP_RST_PWR_GLITCH:
      return "POWER_GLITCH";
    case ESP_RST_CPU_LOCKUP:
      return "CPU_LOCKUP";
    default:
      return "OTHER";
  }
}

}  // namespace

void SerialReporter::begin() {
  Serial.begin(Config::kSerialBaudRate);
  Serial.println();
  Serial.print(F("ESP32-S3 Solar Power Monitor "));
  Serial.println(FirmwareInfo::kVersion);
  Serial.print(F("Release: "));
  Serial.println(FirmwareInfo::kReleaseLabel);
  Serial.print(F("Build: "));
  Serial.println(FirmwareInfo::kBuildStamp);
  const esp_reset_reason_t resetReason = esp_reset_reason();
  Serial.print(F("Reset: "));
  Serial.print(resetReasonName(resetReason));
  Serial.print(F(" ("));
  Serial.print(static_cast<int>(resetReason));
  Serial.println(F(")"));
}

bool SerialReporter::shouldReport(uint32_t nowMs) const {
  return nowMs - lastReportMs_ >= Config::kSerialReportIntervalMs;
}

void SerialReporter::update(uint32_t nowMs,
                            const WifiService& wifiService,
                            const BackendSenderRuntime& backendRuntime,
                            const InaReading& solar,
                            const InaReading& battery,
                            PowerSystemState state) {
  lastReportMs_ = nowMs;
  const WifiRuntime& wifiRuntime = wifiService.runtime();

  Serial.print(F("["));
  Serial.print(nowMs);
  Serial.print(F("] mode="));
  Serial.print(wifiService.wifiModeName());
  Serial.print(F(" ip="));
  Serial.print(wifiService.ipAddress());
  Serial.print(F(" state="));
  Serial.print(powerSystemStateToText(state));
  Serial.print(F(" heap="));
  Serial.print(ESP.getFreeHeap());
  Serial.print(F("/"));
  Serial.print(ESP.getMinFreeHeap());
  Serial.print(F(" wifi_evt="));
  Serial.print(wifiService.lastWifiEventName());
  Serial.print(F(" wifi_drop="));
  Serial.print(wifiRuntime.staDisconnectCount);
  Serial.print(F(" wifi_reconn="));
  Serial.print(wifiRuntime.staReconnectAttempts);
  if (wifiRuntime.lastStaDisconnectReason != 0U) {
    Serial.print(F(" wifi_reason="));
    Serial.print(wifiRuntime.lastStaDisconnectReason);
    Serial.print(F("("));
    Serial.print(wifiService.lastStaDisconnectReasonName());
    Serial.print(F(")"));
  }
  Serial.print(F(" backend="));
  Serial.print(backendRuntime.state);
  Serial.print(F(" q="));
  Serial.print(backendRuntime.queueDepth);
  Serial.print(F("/"));
  Serial.print(backendRuntime.queueCapacity);
  if (backendRuntime.lastHttpStatus > 0U) {
    Serial.print(F(" http="));
    Serial.print(backendRuntime.lastHttpStatus);
  }

  Serial.print(F(" | solar Bus="));
  Serial.print(solar.busVoltageV, 2);
  Serial.print(F("V Load="));
  Serial.print(solar.loadVoltageV, 2);
  Serial.print(F(" I="));
  Serial.print(solar.currentMa, 1);
  Serial.print(F("mA P="));
  Serial.print(solar.powerMw, 1);
  Serial.print(F("mW"));

  Serial.print(F(" | battery Bus="));
  Serial.print(battery.busVoltageV, 2);
  Serial.print(F("V Load="));
  Serial.print(battery.loadVoltageV, 2);
  Serial.print(F("V Shunt="));
  Serial.print(battery.shuntVoltageMv, 1);
  Serial.print(F("mV"));
  Serial.print(F(" I="));
  Serial.print(battery.currentMa, 1);
  Serial.print(F("mA P="));
  Serial.print(battery.powerMw, 1);
  Serial.println(F("mW"));
}

