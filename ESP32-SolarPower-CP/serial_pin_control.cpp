#include "serial_pin_control.h"

#include "config.h"
#include "internet_time_service.h"
#include "wifi_service.h"

namespace {

bool isOnCommand(const String& command) {
  return command == "ON" || command == "HIGH" || command == "1" || command == "PIN ON" ||
         command == "GPIO ON" || command == "GPIO HIGH";
}

bool isOffCommand(const String& command) {
  return command == "OFF" || command == "LOW" || command == "0" || command == "PIN OFF" ||
         command == "GPIO OFF" || command == "GPIO LOW";
}

bool isStatusCommand(const String& command) {
  return command == "STATUS" || command == "STATE" || command == "PIN?" ||
         command == "GPIO?" || command == "?";
}

bool isHelpCommand(const String& command) {
  return command == "HELP" || command == "CMD" || command == "COMMANDS";
}

int32_t buildScheduleDayKey(const tm& localTime) {
  return (localTime.tm_year + 1900) * 1000 + localTime.tm_yday;
}

}  // namespace

void SerialPinControl::begin() {
  inputBuffer_.reserve(32);
  pinMode(Config::kSerialControlPin, OUTPUT);
  digitalWrite(Config::kSerialControlPin, LOW);
  stateHigh_ = false;

  Serial.print(F("Serial pin control ready on GPIO"));
  Serial.println(Config::kSerialControlPin);
  printHelp_();
  printStatus_();
}

void SerialPinControl::update() {
  while (Serial.available() > 0) {
    const char incoming = static_cast<char>(Serial.read());
    if (incoming == '\r') {
      continue;
    }

    if (incoming == '\n') {
      handleInputLine_(inputBuffer_);
      inputBuffer_ = "";
      continue;
    }

    if (incoming >= 32 && incoming <= 126 && inputBuffer_.length() < 31) {
      inputBuffer_ += incoming;
    }
  }
}

void SerialPinControl::setHigh(bool high) {
  setState_(high);
}

void SerialPinControl::applyScheduledOff(const DeviceConfig& config,
                                         const InternetTimeService& timeService,
                                         uint32_t nowMs) {
  if (!config.controlPinScheduleEnabled || !timeService.isTimeValid()) {
    return;
  }

  tm localTime;
  if (!timeService.getLocalTimeStruct(localTime)) {
    return;
  }

  const int32_t dayKey = buildScheduleDayKey(localTime);
  if (dayKey == lastScheduleDayKey_) {
    return;
  }

  const uint32_t currentMinuteOfDay =
      static_cast<uint32_t>(localTime.tm_hour * 60 + localTime.tm_min);
  const uint32_t targetMinuteOfDay = config.controlPinOffHour * 60U + config.controlPinOffMinute;
  if (currentMinuteOfDay < targetMinuteOfDay) {
    return;
  }

  lastScheduleDayKey_ = dayKey;
  if (!stateHigh_) {
    Serial.printf("[serial-pin][%lu] schedule %02lu:%02lu reached, GPIO%u already OFF\n",
                  static_cast<unsigned long>(nowMs),
                  static_cast<unsigned long>(config.controlPinOffHour),
                  static_cast<unsigned long>(config.controlPinOffMinute),
                  static_cast<unsigned int>(Config::kSerialControlPin));
    return;
  }

  setState_(false);
  Serial.printf("[serial-pin][%lu] auto OFF schedule %02lu:%02lu local time reached on GPIO%u\n",
                static_cast<unsigned long>(nowMs),
                static_cast<unsigned long>(config.controlPinOffHour),
                static_cast<unsigned long>(config.controlPinOffMinute),
                static_cast<unsigned int>(Config::kSerialControlPin));
}

uint8_t SerialPinControl::pin() const {
  return Config::kSerialControlPin;
}

bool SerialPinControl::isHigh() const {
  return stateHigh_;
}

const char* SerialPinControl::stateText() const {
  return stateHigh_ ? "ON" : "OFF";
}

void SerialPinControl::handleInputLine_(String line) {
  line.trim();
  line.toUpperCase();
  if (line.isEmpty()) {
    return;
  }

  Serial.print(F("[serial-pin] cmd="));
  Serial.println(line);

  if (isOnCommand(line)) {
    setState_(true);
    return;
  }

  if (isOffCommand(line)) {
    setState_(false);
    return;
  }

  if (isStatusCommand(line)) {
    printStatus_();
    return;
  }

  if (isHelpCommand(line)) {
    printHelp_();
    return;
  }

  Serial.println(F("[serial-pin] unknown command"));
  printHelp_();
}

void SerialPinControl::setState_(bool high) {
  digitalWrite(Config::kSerialControlPin, high ? HIGH : LOW);
  stateHigh_ = high;
  Serial.print(F("[serial-pin] GPIO"));
  Serial.print(Config::kSerialControlPin);
  Serial.print(F(" -> "));
  Serial.println(stateText());
}

void SerialPinControl::printHelp_() const {
  Serial.print(F("[serial-pin] commands: ON, OFF, STATUS, HELP | GPIO"));
  Serial.println(Config::kSerialControlPin);
}

void SerialPinControl::printStatus_() const {
  Serial.print(F("[serial-pin] status GPIO"));
  Serial.print(Config::kSerialControlPin);
  Serial.print(F("="));
  Serial.println(stateText());
}
