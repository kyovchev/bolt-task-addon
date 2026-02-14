#include <M5StickCPlus2.h>
#include <Wire.h>
#include "porthub.h"
#include "UNIT_SCALES.h"

UNIT_SCALES scales;

#define WEIGHT_NORMAL 0
#define WEIGHT_WARNING 1
#define WEIGHT_MAXED 2

float MAX_WEIGHT = 500;
float WARNING_WEIGHT = 250;
int weight_State = WEIGHT_NORMAL;


PortHub porthub;
uint8_t HUB_ADDR[6] = { HUB1_ADDR, HUB2_ADDR, HUB3_ADDR, HUB4_ADDR, HUB5_ADDR, HUB6_ADDR };

float weight = 0;        // weight on scale
int boltStartState = -1;  // test bolt in start position (0 - touching, 1 - not there)
int boltGoalState = -1;   // test bolt in goal position (0 - touching, 1 - not there)


float prev_weight_State = 0;
int prev_boltStartState = -1;
int prev_boltGoalState = -1;


void updateInputs() {
  weight = scales.getWeight();

  if (weight >= MAX_WEIGHT) {
    weight_State = WEIGHT_MAXED;
  } else if (weight >= WARNING_WEIGHT) {
    weight_State = WEIGHT_WARNING;
  } else {
    weight_State = WEIGHT_NORMAL;
  }

  porthub.hub_d_wire_value_A(HUB_ADDR[3], 1); // write value high, this is the solution that fixed the floating values from the new STM vs MEGA chips on the PbHub
  porthub.hub_d_wire_value_B(HUB_ADDR[3], 1); // write value high, this is the solution that fixed the floating values from the new STM vs MEGA chips on the PbHub
  boltStartState = porthub.hub_d_read_value_A(HUB_ADDR[3]);
  boltGoalState = porthub.hub_d_read_value_B(HUB_ADDR[3]);
}

void updateDisplay() {
  int screenWidth = M5.Display.width();
  int screenHeight = M5.Display.height();
  int halfWidth = screenWidth / 2;

  if (prev_weight_State != weight_State || boltStartState != prev_boltStartState || prev_boltGoalState != boltGoalState) {
    if (weight_State == WEIGHT_NORMAL) {
      M5.Display.fillRect(0, 30, screenWidth, screenHeight - 30, BLACK);
      scales.setLEDColor(0x000500);
    } else if (weight_State == WEIGHT_WARNING) {
      M5.Display.fillRect(0, 30, screenWidth, screenHeight - 30, YELLOW);
      scales.setLEDColor(0x010500);
    } else {
      M5.Display.fillRect(0, 30, screenWidth, screenHeight - 30, RED);
      scales.setLEDColor(0x010000);
    }
    if (boltStartState == 0) {
      M5.Display.fillRect(0, 30, halfWidth, screenHeight - 30, BLUE);
    } else if (boltGoalState == 0) {
      M5.Display.fillRect(halfWidth, 30, screenWidth, screenHeight - 30, DARKGREEN);
    }
    prev_weight_State = weight_State;
    prev_boltStartState = boltStartState;
    prev_boltGoalState = boltGoalState;
  }

  M5.Display.fillRect(0, 0, screenWidth, 30, BLACK);
  M5.Display.setTextColor(WHITE);
  M5.Display.setCursor(10, 10);
  M5.Display.printf("Weight: %.1fg", weight);
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  M5.Display.setRotation(3);
  M5.Display.fillScreen(BLACK);
  M5.Display.setTextSize(2);
  M5.Display.drawString("Init...", 10, 10);
  while (!scales.begin(&Wire, 32, 33, DEVICE_DEFAULT_ADDR)) {
    Serial.println("scales connect error");

    delay(2000);
  }
  scales.setOffset();
  scales.setLEDColor(0x000500);

  porthub.begin();
  delay(1000);
}

void loop() {
  Serial.println(weight);
  Serial.println(boltStartState);
  Serial.println(boltGoalState);

  M5.update();
  if (M5.BtnB.wasPressed()) {
    scales.setOffset();
  }

  updateInputs();

  updateDisplay();

  delay(200);
}