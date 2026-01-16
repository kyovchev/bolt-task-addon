#include <M5StickCPlus2.h>

#include "UNIT_SCALES.h"

UNIT_SCALES scales;

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  M5.Display.setRotation(1);
  M5.Display.fillScreen(BLACK);
  M5.Display.setTextSize(2);
  M5.Display.drawString("Init...", 10, 10);
  while (!scales.begin(&Wire, 32, 33, DEVICE_DEFAULT_ADDR)) {
    Serial.println("scales connect error");

    delay(1000);
  }
  scales.setLEDColor(0x001010);
}

void loop() {
  float weight = scales.getWeight();
  float gap = scales.getGapValue();
  int adc = scales.getRawADC();
  Serial.println(weight);
  Serial.println(gap);
  Serial.println(adc);

  M5.update();
  if (M5.BtnB.wasPressed()) {
    scales.setOffset();
  }

  M5.Display.fillScreen(BLACK);
  M5.Display.setTextColor(WHITE);
  M5.Display.setCursor(10, 10);
  M5.Display.printf("Weight: %.1fg", weight);

  delay(100);
}