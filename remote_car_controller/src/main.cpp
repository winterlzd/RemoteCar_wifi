/*
 * ============================================================
 *  RC Car – Upper Controller  (RP2350 W / Pico 2 W)
 *
 *  Role : WiFi AP + Web Gamepad → UART bridge to STM32F103
 *  Protocol to STM32 : "M,<left>,<right>,<brake>\n"
 *  Framework : Arduino (earlephilhower core)
 * ============================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "config.h"
#include "webpage.h"

/* ---- globals ---- */
WebServer server(80);

static volatile uint32_t lastCmdMs  = 0;
static volatile bool     braked     = false;   /* prevent brake spam */
static volatile int16_t  curL = 0, curR = 0;
static volatile uint8_t  curBrake   = 0;
static uint32_t          heartbeatMs = 0;

/* ============================================================
 *  UART bridge  →  STM32
 * ============================================================ */
static void sendToSTM32(int left, int right, int brake)
{
    left  = constrain(left,  -PWM_MAX, PWM_MAX);
    right = constrain(right, -PWM_MAX, PWM_MAX);
    brake = brake ? 1 : 0;

    char buf[32];
    int n = snprintf(buf, sizeof(buf), "M,%d,%d,%d\n", left, right, brake);
    STM32_SERIAL.write(buf, n);
    Serial.printf("[TX→STM32] %s", buf);            // ← 调试：确认是否发出

    curL      = (int16_t)left;
    curR      = (int16_t)right;
    curBrake  = (uint8_t)brake;
    lastCmdMs = millis();
    braked    = false;
}

/* ============================================================
 *  HTTP handlers
 * ============================================================ */

/* GET /  – serve gamepad page */
static void handleRoot()
{
    server.send_P(200, "text/html", HTML_PAGE);
}

/* POST /cmd – receive joystick command
 *  Body: {"l":180,"r":80,"brake":0}
 */
static void handleCmd()
{
    if (server.method() != HTTP_POST) {
        server.send(405, "application/json", "{\"err\":\"POST only\"}");
        return;
    }

    String body = server.arg("plain");
    int l = 0, r = 0, brk = 0;

    /* lightweight JSON parse (no ArduinoJson dependency) */
    int idx;
    idx = body.indexOf("\"l\":");
    if (idx >= 0) l = body.substring(idx + 4).toInt();

    idx = body.indexOf("\"r\":");
    if (idx >= 0) r = body.substring(idx + 4).toInt();

    idx = body.indexOf("\"brake\":");
    if (idx >= 0) brk = body.substring(idx + 7).toInt();

    sendToSTM32(l, r, brk);
    server.send(200, "application/json", "{\"ok\":1}");
}

/* GET /status – lightweight health check */
static void handleStatus()
{
    char buf[64];
    snprintf(buf, sizeof(buf),
             "{\"up\":%lu,\"l\":%d,\"r\":%d}",
             (unsigned long)(millis() / 1000), (int)curL, (int)curR);
    server.send(200, "application/json", buf);
}

/* handle CORS preflight if browser sends OPTIONS */
static void handleOptions()
{
    server.sendHeader("Access-Control-Allow-Origin",  "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    server.send(204);
}

/* ============================================================
 *  WiFi Access Point
 * ============================================================ */
static void startWiFiAP()
{
    Serial.println("[WiFi] Configuring AP...");

    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_SSID, WIFI_PASS, WIFI_CHANNEL, 0, WIFI_MAX_CONN);
    delay(200);

    IPAddress ip = WiFi.softAPIP();
    Serial.printf("[WiFi] SSID : %s\n", WIFI_SSID);
    Serial.printf("[WiFi] Pass : %s\n", WIFI_PASS);
    Serial.printf("[WiFi] IP   : %s\n", ip.toString().c_str());
    Serial.printf("[WiFi] Ch   : %d\n", WIFI_CHANNEL);
}

/* ============================================================
 *  LED helpers
 * ============================================================ */
static void ledBlink(uint8_t times, uint16_t onMs, uint16_t offMs)
{
    for (uint8_t i = 0; i < times; i++) {
        digitalWrite(LED_PIN, HIGH);
        delay(onMs);
        digitalWrite(LED_PIN, LOW);
        delay(offMs);
    }
}

/* ============================================================
 *  Arduino entry points
 * ============================================================ */
void setup()
{
    /* USB serial for debug */
    Serial.begin(115200);
    delay(400);
    Serial.println("\n=============================");
    Serial.println("  RC Car – Upper Controller");
    Serial.println("=============================");

    /* LED */
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    /* UART to STM32 */
    STM32_SERIAL.begin(STM32_BAUD);
    Serial.printf("[UART] STM32 %d baud (Serial1)\n", STM32_BAUD);

    /* WiFi AP */
    startWiFiAP();

    /* Web server routes */
    server.on("/",       HTTP_GET,     handleRoot);
    server.on("/cmd",    HTTP_POST,    handleCmd);
    server.on("/status", HTTP_GET,     handleStatus);
    server.on("/cmd",    HTTP_OPTIONS, handleOptions);
    server.begin();
    Serial.println("[HTTP] Server started on port 80");

    /* send initial brake to STM32 */
    sendToSTM32(0, 0, 1);
    Serial.println("[SYS] Ready – open http://192.168.4.1");

    /* blink LED to signal ready */
    ledBlink(3, 80, 80);
    digitalWrite(LED_PIN, HIGH);
}

void loop()
{
    server.handleClient();

    /* ---- safety: auto-brake on timeout ---- */
    if (!braked && (millis() - lastCmdMs > CMD_TIMEOUT_MS)) {
        sendToSTM32(0, 0, 1);
        braked = true;
        Serial.println("[SAFETY] Timeout – brake");
    }

    /* ---- heartbeat (optional debug) ---- */
    if (millis() - heartbeatMs > 5000) {
        heartbeatMs = millis();
        Serial.printf("[HB] up=%lus  AP clients=%d\n",
                      (unsigned long)(millis() / 1000),
                      WiFi.softAPgetStationNum());
    }
}