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

TvIrConfig tvConfig = {
  NEC,     // Protocol
  0x04,    // A from Flipper.
  0x02,    // Voume Up hex code
  0x03,    // Voume Down hex code
  3        // Try 0, 1, 2, or 3 if the TV does not respond
};

void sendTvCommand(uint16_t command) {
  Serial.print(F("Sending address=0x"));
  Serial.print(tvConfig.address, HEX);
  Serial.print(F(" command=0x"));
  Serial.print(command, HEX);
  Serial.print(F(" repeats="));
  Serial.println(tvConfig.repeats);

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
  Serial.println();
  Serial.println(F("Commands:"));
  Serial.println(F("  u = volume up"));
  Serial.println(F("  d = volume down"));
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  IrSender.begin(kIrSendPin);

  Serial.println(F("Dynamic TV Volume Adjuster - IR test"));
  Serial.print(F("IR transmitter pin: "));
  Serial.println(kIrSendPin);

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