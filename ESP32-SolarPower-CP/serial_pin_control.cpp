#include "serial_pin_control.h"

#include "config.h"

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
