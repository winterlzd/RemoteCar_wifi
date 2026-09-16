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
#include <WebSocketsServer.h>
#include "config.h"
#include "webpage.h"

/* ---- globals ---- */
WebServer server(80);
WebSocketsServer control(81);

static volatile uint32_t lastCmdMs  = 0;
static volatile bool     braked     = false;   /* prevent brake spam */
static volatile int16_t  curL = 0, curR = 0;
static volatile uint8_t  curBrake   = 0;
static uint32_t          heartbeatMs = 0;
static uint32_t          lastControlLogMs = 0;

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

    /* USB monitor only. Keep logging bounded so it cannot slow the control loop. */
    uint32_t now = millis();
    if (Serial && now - lastControlLogMs >= SERIAL_LOG_INTERVAL_MS) {
        lastControlLogMs = now;
        Serial.printf("[CTRL] L=%d R=%d brake=%d\n", left, right, brake);
    }

    curL      = (int16_t)left;
    curR      = (int16_t)right;
    curBrake  = (uint8_t)brake;
    lastCmdMs = now;
    braked    = false;
}

/* ============================================================
 *  WebSocket control: one short frame per current joystick state.
 * ============================================================ */
static bool parseCommand(const uint8_t *payload, size_t length, int &l, int &r, int &brake)
{
    if (length == 0 || length > 16) return false;
    char frame[17];
    memcpy(frame, payload, length);
    frame[length] = '\0';
    char *end = nullptr;
    long values[3];
    char *pos = frame;
    for (int i = 0; i < 3; ++i) {
        values[i] = strtol(pos, &end, 10);
        if (end == pos || (i < 2 ? *end != ',' : *end != '\0')) return false;
        pos = end + 1;
    }
    if (values[0] < -PWM_MAX || values[0] > PWM_MAX ||
        values[1] < -PWM_MAX || values[1] > PWM_MAX ||
        (values[2] != 0 && values[2] != 1)) return false;
    l = values[0]; r = values[1]; brake = values[2];
    return true;
}

static void onControl(uint8_t client, WStype_t type, uint8_t *payload, size_t length)
{
    if (type == WStype_DISCONNECTED) {
        sendToSTM32(0, 0, 1);
        braked = true;
    } else if (type == WStype_TEXT) {
        int l, r, brake;
        if (parseCommand(payload, length, l, r, brake)) {
            sendToSTM32(l, r, brake);
        }
    }
}

/* ============================================================
 *  HTTP handlers (page and optional status only)
 * ============================================================ */

/* GET /  – serve gamepad page */
static void handleRoot()
{
    server.send_P(200, "text/html", HTML_PAGE);
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
    server.on("/status", HTTP_GET,     handleStatus);
    server.begin();
    control.begin();
    control.onEvent(onControl);
    Serial.println("[HTTP] page on port 80; control on WebSocket port 81");

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
    control.loop();

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
