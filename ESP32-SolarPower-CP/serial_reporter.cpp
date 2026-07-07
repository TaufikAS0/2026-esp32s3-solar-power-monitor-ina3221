#include "serial_reporter.h"

#include "config.h"

void SerialReporter::begin() {
  Serial.begin(Config::kSerialBaudRate);
  Serial.println();
  Serial.print(F("ESP32-S3 Solar Power Monitor "));
  Serial.println(FirmwareInfo::kVersion);
  Serial.print(F("Release: "));
  Serial.println(FirmwareInfo::kReleaseLabel);
  Serial.print(F("Build: "));
  Serial.println(FirmwareInfo::kBuildStamp);
}

void SerialReporter::update(uint32_t nowMs,
                            const WifiService& wifiService,
                            const InaReading& solar,
                            const InaReading& battery,
                            PowerSystemState state) {
  if (nowMs - lastReportMs_ < Config::kSerialReportIntervalMs) {
    return;
  }

  lastReportMs_ = nowMs;

  Serial.print(F("["));
  Serial.print(nowMs);
  Serial.print(F("] mode="));
  Serial.print(wifiService.wifiModeName());
  Serial.print(F(" ip="));
  Serial.print(wifiService.ipAddress());
  Serial.print(F(" state="));
  Serial.print(powerSystemStateToText(state));

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

