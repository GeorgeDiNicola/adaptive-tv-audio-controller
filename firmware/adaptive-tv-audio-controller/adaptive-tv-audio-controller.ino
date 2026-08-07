#include <Arduino.h>

#define DISABLE_CODE_FOR_RECEIVER

// Comment out this one line for battery operation.
#define ENABLE_SERIAL_CONFIGURATION

#include <IRremote.hpp>

struct TvIrConfig {
  decode_type_t protocol;
  uint16_t address;
  uint16_t volumeDownCommand;
};

struct SoundMeasurement {
  float standardDeviationLevel;
  float smoothedLevel;
};

const float kDefaultTooLoudThreshold = 0.32;
// Eight 250 ms windows equals 2 seconds.
const uint8_t kLoudWindowsRequired = 8;
const uint8_t kNotchesPerAdjustment = 2;
const unsigned long kDelayBetweenCommandsMs = 150;

const uint8_t kIrSendPin = 3;
const uint8_t kSoundPin = A0;

const float kMaxAnalogReading = 1023.0;

const unsigned long kSampleWindowMs = 250;
const float kSmoothingFactor = 0.10;
const unsigned long kAdjustmentCooldownMs = 1500;
const int_fast8_t kIrRepeats = 0;

const TvIrConfig kDefaultConfig = {
  NEC,
  0x04,
  0x03
};

TvIrConfig tvConfig = kDefaultConfig;

float smoothedLevel = 0.0;
float tooLoudThreshold = kDefaultTooLoudThreshold;

bool hasSmoothedLevel = false;
bool automationEnabled = true;

uint8_t loudWindowCount = 0;
unsigned long lastAdjustmentAt = 0;

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
    Serial.println(
      F("Please enter a value.")
    );

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

bool isHexDigit(char value) {
  return (
    (value >= '0' && value <= '9') ||
    (value >= 'A' && value <= 'F') ||
    (value >= 'a' && value <= 'f')
  );
}

bool tryParseHexValue(String value, uint16_t *parsedValue) {
  value.trim();
  value.toUpperCase();

  if (value.startsWith("0X")) {
    value.remove(0, 2);
  }

  if (
    value.length() == 0 ||
    value.length() > 4
  ) {
    return false;
  }

  for (uint8_t index = 0; index < value.length(); index++) {
    if (!isHexDigit(value.charAt(index))) {
      return false;
    }
  }

  char buffer[5];

  value.toCharArray(buffer, sizeof(buffer));

  *parsedValue = static_cast<uint16_t>(
    strtoul(buffer, nullptr, 16)
  );

  return true;
}

uint16_t readHexValue(const __FlashStringHelper *prompt) {
  while (true) {
    Serial.println(prompt);

    Serial.println(
      F("Enter hex, for example: 0x04 or 04")
    );

    String value =
      readRequiredSerialLine();

    uint16_t parsedValue = 0;

    if (
      tryParseHexValue(
        value,
        &parsedValue
      )
    ) {
      return parsedValue;
    }

    Serial.println(
      F("Invalid hex. Enter 0x0000 through 0xFFFF.")
    );
  }
}

float readNormalizedLevel(const __FlashStringHelper *prompt) {
  while (true) {
    Serial.println(prompt);

    Serial.println(
      F("Enter a decimal from 0.01 through 1.00.")
    );

    String value =
      readRequiredSerialLine();

    float parsedValue =
      value.toFloat();

    if (
      parsedValue > 0.0 &&
      parsedValue <= 1.0
    ) {
      return parsedValue;
    }

    Serial.println(
      F("The threshold must be > 0.00 and <= 1.00.")
    );
  }
}

decode_type_t protocolFromName(String value) {
  value.trim();
  value.toUpperCase();

  if (value == "NEC") {
    return NEC;
  }

  if (
    value == "NECEXT" ||
    value == "NEC_EXT" ||
    value == "ONKYO"
  ) {
    return ONKYO;
  }

  if (
    value == "SAMSUNG" ||
    value == "SAMSUNG32"
  ) {
    return SAMSUNG;
  }

  if (
    value == "SONY" ||
    value == "SIRC"
  ) {
    return SONY;
  }

  if (value == "LG") {
    return LG;
  }

  if (
    value == "PANASONIC" ||
    value == "KASEIKYO"
  ) {
    return PANASONIC;
  }

  if (value == "RC5") {
    return RC5;
  }

  if (value == "RC6") {
    return RC6;
  }

  Serial.println(
    F("Unknown protocol. Falling back to NEC.")
  );

  return NEC;
}

void printConfig() {
  Serial.println();
  Serial.println(
    F("Current IR configuration:")
  );

  Serial.print(F("  Address: 0x"));
  Serial.println(
    tvConfig.address,
    HEX
  );

  Serial.print(
    F("  Volume down command: 0x")
  );
  Serial.println(
    tvConfig.volumeDownCommand,
    HEX
  );

  Serial.print(
    F("  Automatic notches: ")
  );
  Serial.println(
    kNotchesPerAdjustment
  );

  Serial.println();
}

void configureIrFromSerial() {
  Serial.println(
    F("Use default TV IR configuration?")
  );

  Serial.println(
    F(
      "Default: NEC, address=0x04, "
      "volume down=0x03"
    )
  );

  Serial.println(
    F(
      "Type Y/YES for defaults or "
      "N/NO for custom values."
    )
  );

  String useDefault =
    readRequiredSerialLine();

  while (
    !isYes(useDefault) &&
    !isNo(useDefault)
  ) {
    Serial.println(
      F("Please type Y/YES or N/NO.")
    );

    useDefault = readRequiredSerialLine();
  }

  if (isYes(useDefault)) {
    tvConfig = kDefaultConfig;
    printConfig();
    return;
  }

  Serial.println(
    F(
      "Protocol: the IR format "
      "shown by Flipper Zero."
    )
  );

  Serial.println(
    F(
      "Examples: NEC, Samsung32, Sony, "
      "LG, Panasonic, RC5, RC6"
    )
  );

  Serial.println(F("Enter protocol:"));

  tvConfig.protocol =
    protocolFromName(
      readRequiredSerialLine()
    );

  tvConfig.address =
    readHexValue(
      F(
        "Address: value marked A."
      )
    );

  tvConfig.volumeDownCommand =
    readHexValue(
      F(
        "Volume down command: value marked C"
      )
    );

  printConfig();
}

void printSoundThreshold() {
  Serial.println();

  Serial.print(
    F("Too-loud threshold: ")
  );

  Serial.println(tooLoudThreshold, 3);

  Serial.println();
}

void configureSoundThresholdFromSerial() {
  Serial.println(
    F("Use the default too-loud threshold?")
  );

  Serial.println(
    F("Default: 0.32")
  );

  Serial.println(
    F(
      "Type Y/YES for the default or "
      "N/NO for a custom value."
    )
  );

  String useDefault = readRequiredSerialLine();

  while (
    !isYes(useDefault) &&
    !isNo(useDefault)
  ) {
    Serial.println(
      F("Please type Y/YES or N/NO.")
    );

    useDefault =
      readRequiredSerialLine();
  }

  if (isYes(useDefault)) {
    tooLoudThreshold = kDefaultTooLoudThreshold;
  } else {
    tooLoudThreshold =
      readNormalizedLevel(
        F(
          "Too-loud threshold: volume goes "
          "down after the level remains above "
          "this value for 2 seconds."
        )
      );
  }

  printSoundThreshold();
}

void sendTvCommand(uint16_t command) {
  Serial.print(
    F("Sending command 0x")
  );

  Serial.print(command, HEX);

  Serial.print(
    F(" with repeats=")
  );

  Serial.println(kIrRepeats);

  size_t sent = IrSender.write(
    tvConfig.protocol,
    tvConfig.address,
    command,
    kIrRepeats
  );

  if (sent == 0) {
    Serial.println(
      F(
        "IRremote could not send "
        "this protocol."
      )
    );
  }
}

void sendVolumeDown() {
  Serial.print(
    F("Volume-down notches: ")
  );

  Serial.println(
    kNotchesPerAdjustment
  );

  for (uint8_t notch = 0; notch < kNotchesPerAdjustment; notch++) {
    sendTvCommand(
      tvConfig.volumeDownCommand
    );

    if (notch + 1 < kNotchesPerAdjustment) {
      delay(kDelayBetweenCommandsMs);
    }
  }
}

SoundMeasurement measureSound() {
  unsigned long sampleCount = 0;

  float total = 0.0;
  float totalSquared = 0.0;

  unsigned long startTime = millis();

  while (millis() - startTime < kSampleWindowMs) {
    int sample = analogRead(kSoundPin);

    total += sample;
    totalSquared += static_cast<float>(sample) * sample;
    sampleCount++;
  }

  float average = total / sampleCount;

  float variance = (totalSquared / sampleCount) - (average * average);

  if (variance < 0.0) {
    variance = 0.0;
  }

  float standardDeviation = sqrt(variance);

  float standardDeviationLevel =
    standardDeviation / kMaxAnalogReading;

  if (!hasSmoothedLevel) {
    smoothedLevel = standardDeviationLevel;
    hasSmoothedLevel = true;
  } else {
    smoothedLevel =
      (
        kSmoothingFactor *
        standardDeviationLevel
      ) +
      (
        (1.0 - kSmoothingFactor) *
        smoothedLevel
      );
  }

  return {
    standardDeviationLevel,
    smoothedLevel
  };
}

bool cooldownHasElapsed() {
  return (
    millis() - lastAdjustmentAt >=
    kAdjustmentCooldownMs
  );
}

void updateAutomaticControl(float level) {
  bool sentVolumeDown = false;

  if (level > tooLoudThreshold) {
    if (loudWindowCount < kLoudWindowsRequired) {
      loudWindowCount++;
    }
  } else {
    loudWindowCount = 0;
  }

  if (loudWindowCount >= kLoudWindowsRequired &&
    cooldownHasElapsed()
  ) {
    sendVolumeDown();

    hasSmoothedLevel = false;
    lastAdjustmentAt = millis();
    sentVolumeDown = true;
  }

  Serial.print(F(" state="));

  Serial.print(
    level > tooLoudThreshold
      ? F("LOUD")
      : F("OK")
  );

  Serial.print(
    F(" loudCount=")
  );

  Serial.print(
    loudWindowCount
  );

  Serial.print(
    F(" action=")
  );

  Serial.println(
    sentVolumeDown
      ? F("VOLUME_DOWN")
      : F("none")
  );
}

void printHelp() {
  Serial.println(F("Commands:"));

  Serial.println(
    F("  a = toggle automatic adjustment")
  );

  Serial.println(
    F("  t = change the too-loud threshold")
  );

  Serial.println(
    F("  h = show help")
  );

  Serial.println();
}

void handleSerialCommand() {
  if (!Serial.available()) {
    return;
  }

  char input = Serial.read();

  if (input == 'a' || input == 'A') {
    automationEnabled = !automationEnabled;

    loudWindowCount = 0;

    Serial.print(
      F("Automatic adjustment: ")
    );

    Serial.println(
      automationEnabled
        ? F("ON")
        : F("OFF")
    );
  } else if (input == 't' || input == 'T') {
    bool wasEnabled =
      automationEnabled;

    automationEnabled = false;
    loudWindowCount = 0;

    configureSoundThresholdFromSerial();

    automationEnabled = wasEnabled;
  } else if (input == 'h' || input == 'H') {
    printHelp();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(1000);

#ifdef ENABLE_SERIAL_CONFIGURATION
  unsigned long serialStartTime =
    millis();

  while (
    !Serial &&
    millis() - serialStartTime < 4000
  ) {
    delay(25);
  }
#endif

  IrSender.begin(kIrSendPin);

  Serial.println(
    F(
      "Dynamic TV Volume Adjuster "
      "- volume down only"
    )
  );

  Serial.print(
    F("IR signal pin: D")
  );
  Serial.println(kIrSendPin);

  Serial.print(
    F("Sound sensor analog pin: A")
  );
  Serial.println(kSoundPin - A0);

  Serial.println();

#ifdef ENABLE_SERIAL_CONFIGURATION
  configureIrFromSerial();
  configureSoundThresholdFromSerial();
#else
  tvConfig = kDefaultConfig;
  tooLoudThreshold = kDefaultTooLoudThreshold;

  printConfig();
  printSoundThreshold();
#endif

  printHelp();
}

void loop() {
  handleSerialCommand();

  SoundMeasurement measurement = measureSound();

  Serial.print(F("level="));
  Serial.print(measurement.smoothedLevel, 3);

  Serial.print(F(" standardDeviation="));
  Serial.print(measurement.standardDeviationLevel, 3);

  if (automationEnabled) {
    updateAutomaticControl(measurement.smoothedLevel);
  } else {
    Serial.println(
      F(
        " state=DISABLED "
        "action=none"
      )
    );
  }
}
