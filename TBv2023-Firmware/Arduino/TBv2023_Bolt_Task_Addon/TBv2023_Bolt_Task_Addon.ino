#include <M5Unified.h>
#include <Wire.h>
#include "PbHubController.h"

PbHubController pb_hub;

void setup()
{
    M5.begin();
    delay(500);

    M5.Power.setExtOutput(true);
    delay(200);

    pb_hub.begin();
    delay(100);

    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(WHITE, BLACK);

    // I2C scan
    M5.Lcd.println("Scanning...");
    int found = 0;
    for (uint8_t addr = 1; addr < 127; addr++)
    {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0)
        {
            M5.Lcd.printf("0x%02X ", addr);
            found++;
        }
    }
    if (found == 0) M5.Lcd.println("Nothing found!");
    delay(2000);
    M5.Lcd.fillScreen(BLACK);

    if (!pb_hub.check_status())
    {
        M5.Lcd.println("PbHub not found!");
        while (1) delay(500);
    }

    M5.Lcd.println("PbHub OK!");
    delay(500);
    M5.Lcd.fillScreen(BLACK);
}

void loop()
{
    M5.update();

    float weight = pb_hub.read_scale_weight_g();
    if (M5.BtnA.wasPressed()) pb_hub.tare_scale();

    bool blue_button = !pb_hub.read_digital_IO0(PbHubController::Channel::CHANNEL_0);
    bool red_button  = !pb_hub.read_digital_IO1(PbHubController::Channel::CHANNEL_0);

    bool light_left  =  pb_hub.read_digital_IO0(PbHubController::Channel::CHANNEL_1);
    bool light_right =  pb_hub.read_digital_IO0(PbHubController::Channel::CHANNEL_2);

    pb_hub.write_digital_IO0(PbHubController::Channel::CHANNEL_3, true);
    pb_hub.write_digital_IO1(PbHubController::Channel::CHANNEL_3, true);
    delay(2);
    bool probe_goal    = !pb_hub.read_digital_IO0(PbHubController::Channel::CHANNEL_3);
    bool probe_plugged = !pb_hub.read_digital_IO1(PbHubController::Channel::CHANNEL_3);

    float door_angle = pb_hub.read_analog_IO0(PbHubController::Channel::CHANNEL_4) / 4095.0f;
    float fader      = pb_hub.read_analog_IO0(PbHubController::Channel::CHANNEL_5) / 4095.0f;

    bool bolt_start = pb_hub.is_bolt_at_start();
    bool bolt_end   = pb_hub.is_bolt_at_end();

    // ── Display — fixed row positions, no \n ──────────────────────────────────
    const int COL = 0;
    const int ROW_H = 10;  // pixels per row at textSize(1)
    int row = 0;

    M5.Lcd.setTextSize(1);

    // Helper lambda: draw one fixed-width row
    auto drawRow = [&](int r, uint16_t color, const char* fmt, ...)
    {
        char buf[32];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);

        // Pad to 26 chars to overwrite any leftover characters
        char padded[32];
        snprintf(padded, sizeof(padded), "%-26s", buf);

        M5.Lcd.setCursor(COL, r * ROW_H);
        M5.Lcd.setTextColor(color, BLACK);
        M5.Lcd.print(padded);
    };

    drawRow(row++, WHITE,                          "Weight:    %7.2fg", weight);
    drawRow(row++, blue_button   ? GREEN : RED,    "BLUE:      %s", blue_button   ? "PRESSED" : "released");
    drawRow(row++, red_button    ? GREEN : RED,    "RED:       %s", red_button    ? "PRESSED" : "released");
    drawRow(row++, light_left    ? GREEN : RED,    "L_LEFT:    %s", light_left    ? "ON     " : "off    ");
    drawRow(row++, light_right   ? GREEN : RED,    "L_RIGHT:   %s", light_right   ? "ON     " : "off    ");
    drawRow(row++, probe_plugged ? GREEN : RED,    "PROBE:     %s", probe_plugged ? "IN     " : "out    ");
    drawRow(row++, probe_goal    ? GREEN : RED,    "P_GOAL:    %s", probe_goal    ? "YES    " : "no     ");
    drawRow(row++, WHITE,                          "DOOR:      %5.3f", door_angle);
    drawRow(row++, WHITE,                          "FADER:     %5.3f", fader);
    drawRow(row++, bolt_start    ? GREEN : RED,    "BOLT_START:%s", bolt_start    ? "YES    " : "no     ");
    drawRow(row++, bolt_end      ? GREEN : RED,    "BOLT_END:  %s", bolt_end      ? "YES    " : "no     ");

    delay(50);
}