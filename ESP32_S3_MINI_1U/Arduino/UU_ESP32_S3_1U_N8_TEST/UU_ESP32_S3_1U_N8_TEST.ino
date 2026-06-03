#include <Wire.h>

// Pin mapping for your custom ESP32-S3-1U-N8 board
static const int PIN_LED = 2;      // Single LED on IO1
static const int PIN_RGB = 48;     // WS2812/NeoPixel data on IO48
static const int RGB_COUNT = 1;    // One onboard RGB LED
static const int PIN_I2C_SDA = 8;   // I2C SDA on IO8
static const int PIN_I2C_SCL = 9;   // I2C SCL on IO9

// TMP102 with ADR tied to GND -> I2C address 0x48
static const uint8_t TMP102_ADDR = 0x48;
static const uint8_t TMP102_REG_TEMP = 0x00;

// Scheduler constants (milliseconds)
static const uint32_t LED_ON_TIME_MS = 120;
static const uint32_t LED_OFF_TIME_MS = 380;
static const uint32_t RGB_UPDATE_INTERVAL_MS = 20;
static const uint32_t TEMP_READ_INTERVAL_MS = 2000;
static const size_t MEM_TEST_BYTES = 8192;
static const uint32_t CDC_FLOOD_TEST_MS = 5000;
static const size_t CDC_FLOOD_CHUNK_BYTES = 256;

// RGB animation constants
static const uint8_t RGB_LED_BRIGHTNESS = 48;
static const uint16_t HUE_MAX = 359;
static const uint16_t HUE_STEP = 2;

bool tmp102Found = false;
bool ledState = false;
uint32_t nextLedEventMs = 0;
uint32_t nextRgbEventMs = 0;
uint32_t nextTempEventMs = 0;
uint16_t hueDeg = 0;

void printBanner() {
  Serial.println();
  Serial.println("==============================================");
  Serial.println(" ESP32-S3-1U-N8 Hardware Test (USB Serial)");
  Serial.println("==============================================");
  Serial.println("Tests:");
  Serial.println("  1) IO1 LED blink test");
  Serial.println("  2) IO48 RGB LED color test");
  Serial.println("  3) MEM heap read/write test");
  Serial.print("  4) I2C scan on IO");
  Serial.print(PIN_I2C_SDA);
  Serial.print("/IO");
  Serial.println(PIN_I2C_SCL);
  Serial.println("  5) USB CDC flood throughput test");
  Serial.println("  6) TMP102 detect + temperature read (0x48)");
  Serial.println("----------------------------------------------");
}

bool runMemTest() {
  Serial.print("[MEM] Running heap test with ");
  Serial.print(MEM_TEST_BYTES);
  Serial.println(" bytes...");

  uint8_t *buffer = (uint8_t *)malloc(MEM_TEST_BYTES);
  if (!buffer) {
    Serial.println("[MEM] Allocation failed.");
    return false;
  }

  bool ok = true;
  for (size_t i = 0; i < MEM_TEST_BYTES; ++i) {
    buffer[i] = (uint8_t)(i ^ 0xA5);
  }

  for (size_t i = 0; i < MEM_TEST_BYTES; ++i) {
    uint8_t expected = (uint8_t)(i ^ 0xA5);
    if (buffer[i] != expected) {
      Serial.print("[MEM] Mismatch at byte ");
      Serial.print(i);
      Serial.print(": expected 0x");
      if (expected < 0x10) Serial.print('0');
      Serial.print(expected, HEX);
      Serial.print(", got 0x");
      if (buffer[i] < 0x10) Serial.print('0');
      Serial.println(buffer[i], HEX);
      ok = false;
      break;
    }
  }

  free(buffer);

  Serial.print("[MEM] Heap test ");
  Serial.println(ok ? "PASSED." : "FAILED.");
  Serial.print("[MEM] Free heap: ");
  Serial.println(ESP.getFreeHeap());
  return ok;
}

void scanI2CDevices() {
  uint8_t foundCount = 0;

  Serial.print("[I2C] Scanning SDA=IO");
  Serial.print(PIN_I2C_SDA);
  Serial.print(" SCL=IO");
  Serial.print(PIN_I2C_SCL);
  Serial.println("...");

  for (uint8_t address = 1; address < 127; ++address) {
    Wire.beginTransmission(address);
    uint8_t error = Wire.endTransmission();
    if (error == 0) {
      Serial.print("[I2C] Device found at 0x");
      if (address < 0x10) Serial.print('0');
      Serial.println(address, HEX);
      ++foundCount;
    }
  }

  if (foundCount == 0) {
    Serial.println("[I2C] No I2C devices found.");
  }
}

void runCdcFloodTest() {
  static const uint8_t pattern[CDC_FLOOD_CHUNK_BYTES] = {
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55
  };

  Serial.print("[CDC] Flooding USB CDC for ");
  Serial.print(CDC_FLOOD_TEST_MS);
  Serial.println(" ms...");

  uint32_t startMs = millis();
  uint32_t bytesWritten = 0;

  while ((millis() - startMs) < CDC_FLOOD_TEST_MS) {
    size_t written = Serial.write(pattern, CDC_FLOOD_CHUNK_BYTES);
    bytesWritten += written;

    if (written == 0) {
      delay(1);
    }
  }

  Serial.flush();

  uint32_t elapsedMs = millis() - startMs;
  float throughputMbps = (bytesWritten * 8.0f) / (elapsedMs / 1000.0f) / 1000000.0f;

  Serial.println();
  Serial.print("[CDC] Sent ");
  Serial.print(bytesWritten);
  Serial.print(" bytes in ");
  Serial.print(elapsedMs);
  Serial.print(" ms -> ");
  Serial.print(throughputMbps, 2);
  Serial.println(" Mbit/s");
}

void setRgb(uint8_t r, uint8_t g, uint8_t b) {
  // ESP32 Arduino core builtin for WS2812-style RGB LEDs.
  neopixelWrite(PIN_RGB, r, g, b);
}

void hsvToRgb(uint16_t hue, uint8_t sat, uint8_t val, uint8_t &r, uint8_t &g, uint8_t &b) {
  if (sat == 0) {
    r = val;
    g = val;
    b = val;
    return;
  }

  uint16_t h = hue % 360;
  uint8_t region = h / 60;
  uint16_t remainder = (h % 60) * 255 / 60;

  uint16_t p = (uint16_t)val * (255 - sat) / 255;
  uint16_t q = (uint16_t)val * (255 - ((uint16_t)sat * remainder / 255)) / 255;
  uint16_t t = (uint16_t)val * (255 - ((uint16_t)sat * (255 - remainder) / 255)) / 255;

  switch (region) {
    case 0:
      r = val; g = t; b = p;
      break;
    case 1:
      r = q; g = val; b = p;
      break;
    case 2:
      r = p; g = val; b = t;
      break;
    case 3:
      r = p; g = q; b = val;
      break;
    case 4:
      r = t; g = p; b = val;
      break;
    default:
      r = val; g = p; b = q;
      break;
  }
}

bool tmp102Detect() {
  Wire.beginTransmission(TMP102_ADDR);
  return (Wire.endTransmission() == 0);
}

bool tmp102ReadCelsius(float &tempC) {
  Wire.beginTransmission(TMP102_ADDR);
  Wire.write(TMP102_REG_TEMP);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  if (Wire.requestFrom((int)TMP102_ADDR, 2) != 2) {
    return false;
  }

  uint8_t msb = Wire.read();
  uint8_t lsb = Wire.read();
  int16_t raw = ((int16_t)msb << 8) | lsb;

  // TMP102 temperature is in bits [15:4], signed 12-bit, 0.0625 C/LSB.
  raw >>= 4;
  if (raw & 0x0800) {
    raw |= 0xF000;
  }

  tempC = raw * 0.0625f;
  return true;
}

void setup() {
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  pinMode(PIN_RGB, OUTPUT);
  setRgb(0, 0, 0);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

  Serial.begin(115200);
  unsigned long startMs = millis();
  while (!Serial && (millis() - startMs < 5000)) {}

  printBanner();

  scanI2CDevices();

  tmp102Found = tmp102Detect();
  if (tmp102Found) {
    Serial.println("[TMP102] Found at I2C address 0x48.");
  } else {
    Serial.println("[TMP102] NOT found at 0x48. Check SDA/SCL wiring and power.");
  }

  runMemTest();
  runCdcFloodTest();

  uint32_t now = millis();
  nextLedEventMs = now;
  nextRgbEventMs = now;
  nextTempEventMs = now;

  Serial.println("Entering continuous monitor loop...");
}

void loop() {
  uint32_t now = millis();

  if ((int32_t)(now - nextLedEventMs) >= 0) {
    ledState = !ledState;
    digitalWrite(PIN_LED, ledState ? HIGH : LOW);
    nextLedEventMs = now + (ledState ? LED_ON_TIME_MS : LED_OFF_TIME_MS);
  }

  if ((int32_t)(now - nextRgbEventMs) >= 0) {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    hsvToRgb(hueDeg, 255, RGB_LED_BRIGHTNESS, r, g, b);
    setRgb(r, g, b);
    hueDeg = (hueDeg + HUE_STEP) % (HUE_MAX + 1);
    nextRgbEventMs = now + RGB_UPDATE_INTERVAL_MS;
  }

  if ((int32_t)(now - nextTempEventMs) >= 0) {
    float tC = 0.0f;
    if (tmp102Found && tmp102ReadCelsius(tC)) {
      float tF = (tC * 9.0f / 5.0f) + 32.0f;
      Serial.print("T[°C]:");
      Serial.print(tC, 2);
      Serial.print(" T[°F]: ");
      Serial.println(tF, 2);
    } else {
      Serial.println("[TMP102] Read failed.");
    }

    nextTempEventMs = now + TEMP_READ_INTERVAL_MS;
  }
}
