#ifndef CONFIG_H
#define CONFIG_H

/* ============================================================
 *  RC Car – Upper Controller (RP2350 W) Configuration
 * ============================================================ */

/* ---- WiFi Access Point ---- */
#define WIFI_SSID       "RC_Car"
#define WIFI_PASS       "12345678"      /* WPA2 minimum 8 chars */
#define WIFI_CHANNEL    6
#define WIFI_MAX_CONN   1               /* single client */

/* ---- UART to STM32F103 ---- */
/*  Serial1 = UART0 : GP0(TX) → STM32 PA10(RX)
 *                     GP1(RX) ← STM32 PA9(TX)            */
#define STM32_SERIAL    Serial1
#define STM32_BAUD      115200

/* ---- Safety ---- */
#define CMD_TIMEOUT_MS  500             /* auto-brake if no cmd (was 300, too tight) */
#define HEARTBEAT_MS    200             /* web-side keepalive (match user request) */
#define PWM_MAX         255

/* ---- LED ---- */
#define LED_PIN         LED_BUILTIN     /* CYW43 WL_GPIO on Pico W */

#endif /* CONFIG_H */
