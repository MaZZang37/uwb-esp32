/*
  Multi-Anchor Single-Tag Example — Tag Side
  Based on: Firmware V1.1.6/esp32s3_at_t0 + Indoor positioning/get_range

  This tag receives range measurements from all anchors and outputs JSON
  compatible with Indoor positioning/position.py.

  Output example (Serial @ 250000 baud):
    {"id":0,"range":[120,85,203,0,0,0,0,0]}
    └── range[i] = distance to Anchor i in cm (0 = anchor not responding)

  Libraries required:
    Wire           2.0.0
    Adafruit_GFX   1.11.7
    Adafruit_BusIO 1.14.4
    Adafruit_SSD1306 2.5.7
*/

// ── User config ───────────────────────────────────────────────────────────────
#define UWB_INDEX  0   // Tag ID (usually 0 for a single tag)
#define PAN_INDEX  0   // Must match all anchors
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

String response = "";
const String REC_HEAD = "AT+RANGE";

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
    display.print(F("Tag "));
    display.println(UWB_INDEX);

    display.setTextSize(1);
    display.setCursor(0, 52);
    display.println(F("6.8M  multi-anchor"));

    display.display();
    delay(2000);
}

// AT+RANGE=tid:0,mask:07,seq:5,range:(120,85,203,0,0,0,0,0),rssi:(...)
void range_analy(String data)
{
    String id_str    = data.substring(data.indexOf("tid:")  + 4, data.indexOf(",mask:"));
    String range_str = data.substring(data.indexOf("range:"),    data.indexOf(",rssi:"));
    String rssi_str  = data.substring(data.indexOf("rssi:"));

    int    range_list[8];
    double rssi_list[8];

    int count = sscanf(range_str.c_str(), "range:(%d,%d,%d,%d,%d,%d,%d,%d)",
                       &range_list[0], &range_list[1], &range_list[2], &range_list[3],
                       &range_list[4], &range_list[5], &range_list[6], &range_list[7]);
    if (count != 8) { SERIAL_LOG.println(F("RANGE PARSE ERROR")); return; }

    count = sscanf(rssi_str.c_str(), "rssi:(%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf)",
                   &rssi_list[0], &rssi_list[1], &rssi_list[2], &rssi_list[3],
                   &rssi_list[4], &rssi_list[5], &rssi_list[6], &rssi_list[7]);
    if (count != 8) { SERIAL_LOG.println(F("RSSI PARSE ERROR")); return; }

    // JSON output — compatible with Indoor positioning/position.py
    String json = "{\"id\":" + id_str + ",\"range\":[";
    for (int i = 0; i < 8; i++) {
        json += range_list[i];
        if (i < 7) json += ",";
    }
    json += "]}";
    SERIAL_LOG.println(json);

    // OLED: show distances to first 3 anchors
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(F("Tag -> Anchors (cm)"));
    display.setTextSize(2);
    for (int i = 0; i < 3; i++) {
        display.setCursor(0, 16 + i * 16);
        display.print("A");
        display.print(i);
        display.print(":");
        if (range_list[i] > 0)
            display.println(range_list[i]);
        else
            display.println(F(" --"));
    }
    display.display();
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

    // id=UWB_INDEX, role=0(Tag), freq=1(6.8M), range_filter=0(off — required for multi-anchor)
    sendData("AT+SETCFG=" + String(UWB_INDEX) + ",0,1,0", 2000);

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
    while (SERIAL_LOG.available() > 0) {
        SERIAL_AT.write(SERIAL_LOG.read());
        yield();
    }
    while (SERIAL_AT.available() > 0) {
        char c = SERIAL_AT.read();
        if (c == '\r') continue;
        if (c == '\n') {
            if (response.indexOf(REC_HEAD) != -1)
                range_analy(response);
            else
                SERIAL_LOG.println(response);
            response = "";
        } else {
            response += c;
        }
    }
}
