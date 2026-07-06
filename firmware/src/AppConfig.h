#pragma once

// Compile-time defaults. Everything runtime-configurable lives in
// SettingsStore; these are only the fallbacks for a fresh device.

#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif

// Inverter UART (UART2). GPIO16/17 are free on classic WROOM-32 boards.
#define PIN_INV_RX 16
#define PIN_INV_TX 17

// Status LEDs. Legacy used GPIO5/4 but GPIO5 is an ESP32 strapping pin,
// so the defaults move to safe pins; both are runtime-configurable.
#define PIN_LED_RED   25
#define PIN_LED_GREEN 26

// Access point for first-run setup / STA fallback.
#define AP_SSID_PREFIX "SolarSetup-"
#define AP_IP "192.168.4.1"

// Serial engine timing.
#define INV_TIMEOUT_MS       2500   // per command (QPIGS ~460ms on wire @2400)
#define INV_RETRIES          1      // retries per command before giving up
#define INV_GAP_MS           150    // quiet gap between commands
#define INV_LINK_LOST_AFTER  3      // consecutive failures -> link down
#define INV_BACKOFF_MS       5000   // poll backoff while link down

#define WIFI_STA_ATTEMPT_MS  45000  // STA connect budget before AP fallback
#define WIFI_RETRY_WHILE_AP_MS 300000 // retry STA every 5 min while in AP mode
