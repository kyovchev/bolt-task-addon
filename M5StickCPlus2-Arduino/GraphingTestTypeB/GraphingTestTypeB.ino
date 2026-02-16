#include <M5StickCPlus2.h>
#include <Wire.h>
#include "porthub.h"
#include "UNIT_SCALES.h"

UNIT_SCALES scales;

#define WEIGHT_NORMAL 0
#define WEIGHT_WARNING 1
#define WEIGHT_MAXED 2

#define VIEW_MAIN 0
#define VIEW_GRAPH 1

float MAX_WEIGHT = 1000;
float WARNING_WEIGHT = 750;
int weight_State = WEIGHT_NORMAL;

#define GRAPH_SAMPLES 100
#define GRAPH_INTERVAL_MS 200
#define GRAPH_X_OFFSET 30
#define GRAPH_Y_OFFSET 10
#define GRAPH_BAR_COLOR 0x07FF  // cyan

PortHub porthub;
uint8_t HUB_ADDR[6] = { HUB1_ADDR, HUB2_ADDR, HUB3_ADDR,
                        HUB4_ADDR, HUB5_ADDR, HUB6_ADDR };

float weight = 0;
int boltStartState = -1;
int boltGoalState = -1;

float prev_weight_State = 0;
int prev_boltStartState = -1;
int prev_boltGoalState = -1;

int currentView = VIEW_MAIN;
bool viewChanged = true;

float weightHistory[GRAPH_SAMPLES];
int8_t boltStartHistory[GRAPH_SAMPLES];  // 0 = active (touching), 1 = not present, -1 = unknown
int8_t boltGoalHistory[GRAPH_SAMPLES];
int historyHead = 0;
int historySamples = 0;


// ─── Circular buffer ─────────────────────────────────────────────────────────

void pushToHistory(float w, int8_t bs, int8_t bg) {
  weightHistory[historyHead] = w;
  boltStartHistory[historyHead] = bs;
  boltGoalHistory[historyHead] = bg;
  historyHead = (historyHead + 1) % GRAPH_SAMPLES;
  if (historySamples < GRAPH_SAMPLES) historySamples++;
}

// Returns logical index → real buffer index
int historyRealIdx(int i) {
  return (historyHead - historySamples + i + GRAPH_SAMPLES * 2) % GRAPH_SAMPLES;
}

float historyAt(int i) {
  return weightHistory[historyRealIdx(i)];
}
int8_t historyBoltStart(int i) {
  return boltStartHistory[historyRealIdx(i)];
}
int8_t historyBoltGoal(int i) {
  return boltGoalHistory[historyRealIdx(i)];
}


// ─── Input ───────────────────────────────────────────────────────────────────

void updateInputs() {
  weight = scales.getWeight();

  if (weight >= MAX_WEIGHT) weight_State = WEIGHT_MAXED;
  else if (weight >= WARNING_WEIGHT) weight_State = WEIGHT_WARNING;
  else weight_State = WEIGHT_NORMAL;

  porthub.hub_d_wire_value_A(HUB_ADDR[3], 1);
  porthub.hub_d_wire_value_B(HUB_ADDR[3], 1);
  boltStartState = porthub.hub_d_read_value_A(HUB_ADDR[3]);
  boltGoalState = porthub.hub_d_read_value_B(HUB_ADDR[3]);
}


// ─── Serial data output ──────────────────────────────────────────────────────
// One CSV line per reading:
//   DATA,<millis_ms>,<weight_g>,<boltStart>,<boltGoal>
//
//   boltStart / boltGoal:  0 = touching (active), 1 = not present
//
// Special messages:
//   READY          — sent once after setup
//   TARE           — sent when BtnB is pressed (scale zeroed)

void sendDataLine() {
  Serial.print("DATA,");
  Serial.print(millis());
  Serial.print(",");
  Serial.print(weight, 1);
  Serial.print(",");
  Serial.print(boltStartState);
  Serial.print(",");
  Serial.println(boltGoalState);
}


// ─── Main display ────────────────────────────────────────────────────────────

void updateDisplayMain() {
  int screenWidth = M5.Display.width();
  int screenHeight = M5.Display.height();
  int halfWidth = screenWidth / 2;

  bool stateChanged = (prev_weight_State != weight_State || boltStartState != prev_boltStartState || prev_boltGoalState != boltGoalState);

  if (viewChanged || stateChanged) {
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
    if (boltStartState == 0)
      M5.Display.fillRect(0, 30, halfWidth, screenHeight - 30, BLUE);
    else if (boltGoalState == 0)
      M5.Display.fillRect(halfWidth, 30, screenWidth, screenHeight - 30, DARKGREEN);

    prev_weight_State = weight_State;
    prev_boltStartState = boltStartState;
    prev_boltGoalState = boltGoalState;
  }

  M5.Display.fillRect(0, 0, screenWidth, 30, BLACK);
  M5.Display.setTextColor(WHITE);
  M5.Display.setCursor(10, 10);
  M5.Display.printf("Weight: %.1fg", weight);
}


// ─── Graph display ───────────────────────────────────────────────────────────

void updateDisplayGraph() {
  int screenWidth = M5.Display.width();
  int screenHeight = M5.Display.height();

  int plotX = GRAPH_X_OFFSET;
  int plotY = GRAPH_Y_OFFSET + 20;
  int plotW = screenWidth - plotX - 4;
  int plotH = screenHeight - plotY - 16;

  if (viewChanged) {
    M5.Display.fillScreen(BLACK);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(0xAD75);
    M5.Display.setCursor(plotX, GRAPH_Y_OFFSET + 4);
    M5.Display.print("WEIGHT GRAPH");
    M5.Display.drawLine(plotX, plotY, plotX, plotY + plotH, WHITE);
    M5.Display.drawLine(plotX, plotY + plotH, plotX + plotW, plotY + plotH, WHITE);
  }

  float minW = 0.0f, maxW = MAX_WEIGHT;
  if (historySamples > 0) {
    float dataMax = historyAt(0), dataMin = historyAt(0);
    for (int i = 1; i < historySamples; i++) {
      float v = historyAt(i);
      if (v > dataMax) dataMax = v;
      if (v < dataMin) dataMin = v;
    }
    float range = dataMax - dataMin;
    if (range < 1.0f) range = 1.0f;
    minW = dataMin - range * 0.1f;
    maxW = dataMax + range * 0.1f;
    if (minW < 0) minW = 0;
    if (maxW < 10) maxW = 10;
  }
  float yRange = maxW - minW;

  M5.Display.setTextSize(1);
  M5.Display.setTextColor(0xC618);
  M5.Display.fillRect(0, plotY - 4, plotX - 1, plotH + 8, BLACK);
  M5.Display.setCursor(0, plotY);
  M5.Display.printf("%4.0f", maxW);
  M5.Display.setCursor(0, plotY + plotH / 2 - 4);
  M5.Display.printf("%4.0f", (minW + maxW) / 2.0f);
  M5.Display.setCursor(0, plotY + plotH - 8);
  M5.Display.printf("%4.0f", minW);

  auto weightToY = [&](float w) -> int {
    return plotY + plotH - (int)((w - minW) / yRange * plotH);
  };

  M5.Display.fillRect(plotX + 1, plotY, plotW, plotH, BLACK);

  // ── Bolt state background bands ──────────────────────────────────────────
  // Walk history and paint a 1-pixel-wide vertical strip per sample.
  // Blue  = boltStart active (==0)
  // Green = boltGoal  active (==0)
  // Both active at once → blue takes priority (matches main view behaviour)
  if (historySamples > 1) {
    float xStep = (float)plotW / (float)(GRAPH_SAMPLES - 1);
    for (int i = 0; i < historySamples; i++) {
      int logicalIdx = (GRAPH_SAMPLES - historySamples) + i;
      int px = plotX + (int)(logicalIdx * xStep);

      int8_t bs = historyBoltStart(i);
      int8_t bg = historyBoltGoal(i);

      uint16_t bandCol = 0;                // 0 means no band
      if (bs == 0) bandCol = 0x0318;       // dark blue  (#001830)
      else if (bg == 0) bandCol = 0x0300;  // dark green (#003000)

      if (bandCol) {
        M5.Display.drawFastVLine(px, plotY, plotH, bandCol);
      }
    }
  }

  if (WARNING_WEIGHT >= minW && WARNING_WEIGHT <= maxW)
    M5.Display.drawLine(plotX + 1, weightToY(WARNING_WEIGHT), plotX + plotW, weightToY(WARNING_WEIGHT), YELLOW);
  if (MAX_WEIGHT >= minW && MAX_WEIGHT <= maxW)
    M5.Display.drawLine(plotX + 1, weightToY(MAX_WEIGHT), plotX + plotW, weightToY(MAX_WEIGHT), RED);

  if (historySamples > 1) {
    float xStep = (float)plotW / (float)(GRAPH_SAMPLES - 1);
    int prevPx = -1, prevPy = -1;
    for (int i = 0; i < historySamples; i++) {
      int logicalIdx = (GRAPH_SAMPLES - historySamples) + i;
      int px = plotX + (int)(logicalIdx * xStep);
      int py = constrain(weightToY(historyAt(i)), plotY, plotY + plotH);
      float v = historyAt(i);
      uint16_t col = (v >= MAX_WEIGHT) ? RED : (v >= WARNING_WEIGHT) ? YELLOW
                                                                     : GRAPH_BAR_COLOR;
      if (prevPx >= 0) M5.Display.drawLine(prevPx, prevPy, px, py, col);
      M5.Display.drawPixel(px, py, col);
      prevPx = px;
      prevPy = py;
    }
  }

  M5.Display.fillRect(0, plotY + plotH + 1, screenWidth, screenHeight - (plotY + plotH + 1), BLACK);
  M5.Display.setTextColor(WHITE);
  M5.Display.setCursor(plotX, plotY + plotH + 3);
  M5.Display.printf("%.1fg  [-%ds]", weight,
                    (int)((GRAPH_SAMPLES * GRAPH_INTERVAL_MS) / 1000.0f));
}


// ─── Setup ───────────────────────────────────────────────────────────────────

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
  memset(weightHistory, 0, sizeof(weightHistory));
  memset(boltStartHistory, -1, sizeof(boltStartHistory));
  memset(boltGoalHistory, -1, sizeof(boltGoalHistory));

  delay(1000);
  M5.Display.fillScreen(BLACK);
  M5.Display.setTextSize(2);

  Serial.println("READY");
}


// ─── Loop ────────────────────────────────────────────────────────────────────

void loop() {
  M5.update();

  if (M5.BtnA.wasPressed()) {
    currentView = (currentView == VIEW_MAIN) ? VIEW_GRAPH : VIEW_MAIN;
    viewChanged = true;
    M5.Display.fillScreen(BLACK);
    M5.Display.setTextSize(2);
  }

  if (M5.BtnB.wasPressed()) {
    scales.setOffset();
    Serial.println("TARE");
  }

  updateInputs();
  pushToHistory(weight, (int8_t)boltStartState, (int8_t)boltGoalState);
  sendDataLine();

  if (currentView == VIEW_MAIN) updateDisplayMain();
  else updateDisplayGraph();

  viewChanged = false;

  delay(GRAPH_INTERVAL_MS);
}
