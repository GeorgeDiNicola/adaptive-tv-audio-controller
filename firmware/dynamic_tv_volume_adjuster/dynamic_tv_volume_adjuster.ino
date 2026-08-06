#include <Arduino.h>

#define DISABLE_CODE_FOR_RECEIVER
#include <IRremote.hpp>

struct TvIrConfig {
  decode_type_t protocol;
  uint16_t address;
  uint16_t volumeUpCommand;
  uint16_t volumeDownCommand;
  int_fast8_t repeats;
};

const uint8_t kIrSendPin = 3;

const TvIrConfig kDefaultConfig = {
  NEC,
  0x04,
  0x02,
  0x03,
  2
};

TvIrConfig tvConfig = kDefaultConfig;

String readSerialLine() {
  while (!Serial.available()) {
    delay(25);
  }

  String value = Serial.readStringUntil('\n');
  value.trim();
  return value;
}

String readRequiredSerialLine() {
  String value = readSerialLine();

  while (value.length() == 0) {
    Serial.println(F("Please enter a value."));
    value = readSerialLine();
  }

  return value;
}

bool isYes(String value) {
  value.trim();
  value.toUpperCase();
  return value == "Y" || value == "YES";
}

bool isNo(String value) {
  value.trim();
  value.toUpperCase();
  return value == "N" || value == "NO";
}

uint16_t readHexValue(const __FlashStringHelper *prompt) {
  Serial.println(prompt);
  Serial.println(F("Enter the value in hex, for example: 0x01 or 01"));

  String value = readRequiredSerialLine();
  value.toUpperCase();

  if (value.startsWith("0X")) {
    value.remove(0, 2);
  }

  char buffer[16];
  value.toCharArray(buffer, sizeof(buffer));
  return static_cast<uint16_t>(strtoul(buffer, nullptr, 16));
}

int_fast8_t readRepeatValue() {
  Serial.println(F("Repeat value: how many extra times Arduino repeats the IR command after the first send."));
  Serial.println(F("Use the lowest value that works reliably. Enter one of the following: {0, 1, 2, 3}"));

  String value = readRequiredSerialLine();
  return static_cast<int_fast8_t>(value.toInt());
}

decode_type_t protocolFromName(String value) {
  value.trim();
  value.toUpperCase();

  if (value == "NEC") {
    return NEC;
  }

  if (value == "NECEXT" || value == "NEC_EXT" || value == "ONKYO") {
    return ONKYO;
  }

  if (value == "SAMSUNG" || value == "SAMSUNG32") {
    return SAMSUNG;
  }

  if (value == "SONY" || value == "SIRC") {
    return SONY;
  }

  if (value == "LG") {
    return LG;
  }

  if (value == "PANASONIC" || value == "KASEIKYO") {
    return PANASONIC;
  }

  if (value == "RC5") {
    return RC5;
  }

  if (value == "RC6") {
    return RC6;
  }

  Serial.println(F("Unknown protocol. Using the default: NEC."));
  return NEC;
}

void printConfig() {
  Serial.println();
  Serial.println(F("Current IR configuration:"));
  Serial.print(F("  Address: 0x"));
  Serial.println(tvConfig.address, HEX);
  Serial.print(F("  Volume up command: 0x"));
  Serial.println(tvConfig.volumeUpCommand, HEX);
  Serial.print(F("  Volume down command: 0x"));
  Serial.println(tvConfig.volumeDownCommand, HEX);
  Serial.print(F("  Repeats: "));
  Serial.println(tvConfig.repeats);
  Serial.println();
}

void configureFromSerial() {
  Serial.println(F("Use default TV IR configuration?"));
  Serial.println(F("Default: protocol=NEC, address=0x04, volume up=0x02, volume down=0x03, repeats=3"));
  Serial.println(F("Type Y/YES to use defaults, or N/NO to enter custom values."));

  String useDefault = readRequiredSerialLine();

  while (!isYes(useDefault) && !isNo(useDefault)) {
    Serial.println(F("Please type Y/YES or N/NO."));
    useDefault = readRequiredSerialLine();
  }

  if (isYes(useDefault)) {
    tvConfig = kDefaultConfig;
    printConfig();
    return;
  }

  Serial.println(F("Protocol: the IR format your TV remote uses."));
  Serial.println(F("Enter protocol:"));
  tvConfig.protocol = protocolFromName(readRequiredSerialLine());

  tvConfig.address = readHexValue(
    F("Address: this is the remote/device hex code address.")
  );
  Serial.println(F("The address tells the TV which remote/device family the command belongs to."));

  tvConfig.volumeUpCommand = readHexValue(
    F("Volume up command: this is the command hex code for the TV's volume up command.")
  );

  tvConfig.volumeDownCommand = readHexValue(
    F("Volume down command: this is the command hex code for the TV's volume down command.")
  );

  tvConfig.repeats = readRepeatValue();
  printConfig();
}

void sendTvCommand(uint16_t command) {
  Serial.print(F("Sending command 0x"));
  Serial.println(command, HEX);

  size_t sent = IrSender.write(
    tvConfig.protocol,
    tvConfig.address,
    command,
    tvConfig.repeats
  );

  if (sent == 0) {
    Serial.println(F("IRremote could not send this protocol."));
  }
}

void printHelp() {
  Serial.println(F("Commands after setup:"));
  Serial.println(F("  u = send volume up"));
  Serial.println(F("  d = send volume down"));
  Serial.println(F("  h = show this help"));
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(30000);

  unsigned long serialStartTime = millis();
  while (!Serial && millis() - serialStartTime < 4000) {
    delay(25);
  }

  IrSender.begin(kIrSendPin);

  Serial.println(F("Dynamic TV Volume Adjuster - IR transmitter test"));
  Serial.print(F("IR transmitter signal pin: "));
  Serial.println(kIrSendPin);
  Serial.println();

  configureFromSerial();
  printHelp();
}

void loop() {
  if (!Serial.available()) {
    return;
  }

  char input = Serial.read();

  if (input == 'u' || input == 'U') {
    sendTvCommand(tvConfig.volumeUpCommand);
  } else if (input == 'd' || input == 'D') {
    sendTvCommand(tvConfig.volumeDownCommand);
  } else if (input == 'h' || input == 'H') {
    printHelp();
  }
}