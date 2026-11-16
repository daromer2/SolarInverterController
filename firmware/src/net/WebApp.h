#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "../serial/InverterEngine.h"
#include "../sys/LedManager.h"
#include "../sys/SettingsStore.h"
#include "WsHub.h"

// HTTP server: static SPA from LittleFS, REST API, browser OTA (firmware +
// filesystem), captive portal redirect in AP mode. Async handlers never
// touch the serial engine directly — only its thread-safe queue API.
class WebApp {
public:
    void begin(SettingsStore* settings, InverterEngine* engine, WsHub* ws,
               LedManager* leds, bool apMode);
    bool rebootRequested() const { return _rebootAt && (int32_t)(millis() - _rebootAt) > 0; }

private:
    bool auth(AsyncWebServerRequest* req);
    void setupApi();
    void setupOta();
    void setupStatic();

    AsyncWebServer _server{80};
    SettingsStore* _settings = nullptr;
    InverterEngine* _engine = nullptr;
    WsHub* _ws = nullptr;
    LedManager* _leds = nullptr;
    bool _apMode = false;
    volatile uint32_t _rebootAt = 0;
    uint32_t _cmdCounter = 1;
};
