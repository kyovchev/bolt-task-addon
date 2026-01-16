#include <M5StickCPlus2.h>
#include <Wire.h>

#define GROVE_SDA 32
#define GROVE_SCL 33

#define PAHUB_ADDR 0x70

TwoWire I2C_Grove = TwoWire(0);

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  M5.Display.setRotation(1);
  M5.Display.fillScreen(BLACK);
  M5.Display.setTextColor(WHITE);
  M5.Display.setTextSize(1);

  Serial.begin(115200);
  while (!Serial)
    ;

  I2C_Grove.begin(GROVE_SDA, GROVE_SCL, 100000);

  delay(100);

  M5.Display.println("I2C Scanner");
  M5.Display.println("with PaHUB");
  Serial.println("\n=== I2C Scanner with PaHUB ===");

  scanAllChannels();

  M5.Display.println("\nBtnA: rescan");
  Serial.println("\nPress BtnA to rescan");
}

void loop() {
  M5.update();

  if (M5.BtnA.wasPressed()) {
    M5.Display.fillScreen(BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.println("Rescanning...");
    Serial.println("\n=== Rescanning ===");
    scanAllChannels();
    M5.Display.println("\nBtnA: rescan");
  }

  delay(100);
}

void selectPaHubChannel(uint8_t channel) {
  if (channel > 5) return;

  I2C_Grove.beginTransmission(PAHUB_ADDR);
  I2C_Grove.write(1 << channel);
  I2C_Grove.endTransmission();
  delay(10);
}

void scanAllChannels() {
  M5.Display.println("Main I2C Bus:");
  Serial.println("\n--- Main I2C Bus ---");
  scanI2C();

  I2C_Grove.beginTransmission(PAHUB_ADDR);
  byte error = I2C_Grove.endTransmission();

  if (error == 0) {
    Serial.println("\nPaHUB detected! Scanning all channels...");

    for (int ch = 0; ch < 6; ch++) {
      String msg = "\n--- PaHUB CH" + String(ch) + " ---";
      M5.Display.println(msg);
      Serial.println(msg);

      selectPaHubChannel(ch);
      scanI2C();
    }
  } else {
    M5.Display.println("\nNo PaHUB found");
    Serial.println("\nNo PaHUB found on main bus");
  }
}

void scanI2C() {
  byte error, address;
  int deviceCount = 0;

  for (address = 1; address < 127; address++) {
    if (address == PAHUB_ADDR) continue;

    I2C_Grove.beginTransmission(address);
    error = I2C_Grove.endTransmission();

    if (error == 0) {
      deviceCount++;

      String msg = "0x";
      if (address < 16) msg += "0";
      msg += String(address, HEX);
      msg += " " + getDeviceName(address);

      M5.Display.println(msg);
      Serial.println("  Device: " + msg);
    }
  }

  if (deviceCount == 0) {
    M5.Display.println("  (empty)");
    Serial.println("  No devices");
  }
}

String getDeviceName(byte address) {
  switch (address) {
    case 0x26: return "Mini Scale";
    case 0x56: return "PbHUB";
    case 0x61: return "PbHUB";
    case 0x70: return "PaHUB";
    case 0x68: return "MPU6886/IMU";
    case 0x51: return "BM8563 RTC";
    case 0x34: return "AXP192 PMU";
    case 0x38: return "FT6336";
    default: return "";
  }
}