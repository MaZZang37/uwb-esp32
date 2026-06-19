/*
  Multi-Anchor Single-Tag Example — Anchor Side
  Based on: Firmware V1.1.6/esp32s3_at_a0

  Usage:
    1. Change UWB_INDEX for each anchor board (0, 1, 2 ...)
    2. Keep PAN_INDEX the same on all devices (anchors + tag)
    3. Flash this sketch to every anchor board

  Libraries required:
    Wire          2.0.0
    Adafruit_GFX  1.11.7
    Adafruit_BusIO 1.14.4
    Adafruit_SSD1306 2.5.7
*/

// ── User config ───────────────────────────────────────────────────────────────
#define UWB_INDEX     0   // Change per anchor: 0, 1, 2 ...
#define PAN_INDEX     0   // Must match on all devices
// ─────────────────────────────────────────────────────────────────────────────

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>

#define SERIAL_LOG Serial
HardwareSerial SERIAL_AT(2);

#define RESET    16
#define IO_RXD2  18
#define IO_TXD2  17
#define I2C_SDA  39
#define I2C_SCL  38

Adafruit_SSD1306 display(128, 64, &Wire, -1);

// ── Helpers ──────────────────────────────────────────────────────────────────

String sendData(String command, const int timeout, boolean debug = true)
{
    String response = "";
    SERIAL_LOG.println(command);
    SERIAL_AT.println(command);

    long int t = millis();
    while ((t + timeout) > millis()) {
        while (SERIAL_AT.available())
            response += (char)SERIAL_AT.read();
    }
    if (debug) SERIAL_LOG.println(response);
    return response;
}

void logoshow()
{
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(F("MaUWB DW3000"));

    display.setTextSize(2);
    display.setCursor(0, 20);
    display.print(F("Anchor "));
    display.println(UWB_INDEX);

    display.setTextSize(1);
    display.setCursor(0, 52);
    display.println(F("6.8M  1 Tag"));

    display.display();
    delay(2000);
}

// ── Setup ────────────────────────────────────────────────────────────────────

void setup()
{
    pinMode(RESET, OUTPUT);
    digitalWrite(RESET, HIGH);

    SERIAL_LOG.begin(250000);
    SERIAL_AT.begin(115200, SERIAL_8N1, IO_RXD2, IO_TXD2);

    Wire.begin(I2C_SDA, I2C_SCL);
    delay(1000);

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        SERIAL_LOG.println(F("SSD1306 allocation failed"));
        for (;;);
    }
    display.clearDisplay();
    logoshow();

    sendData("AT?",        2000);
    sendData("AT+RESTORE", 5000);

    // id=UWB_INDEX, role=1(Anchor), freq=1(6.8M), range_filter=0(off — required for multi-anchor)
    sendData("AT+SETCFG=" + String(UWB_INDEX) + ",1,1,0", 2000);

    // tag_count=1, slot_time=10ms, extMode=1
    sendData("AT+SETCAP=1,10,1", 2000);

    sendData("AT+SETRPT=1",                      2000);
    sendData("AT+SETPAN=" + String(PAN_INDEX),   2000);
    sendData("AT+SAVE",    2000);
    sendData("AT+RESTART", 2000);
}

// ── Loop ─────────────────────────────────────────────────────────────────────

void loop()
{
    // Pass-through: Serial Monitor ↔ UWB module
    while (SERIAL_LOG.available() > 0) {
        SERIAL_AT.write(SERIAL_LOG.read());
        yield();
    }
    while (SERIAL_AT.available() > 0) {
        SERIAL_LOG.write(SERIAL_AT.read());
    }
}
