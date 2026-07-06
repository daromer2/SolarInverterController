#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "../serial/InverterEngine.h"

// WebSocket hub at /ws. Pushes throttled state deltas; sends the full state
// on connect. Command results and connection status are broadcast as they
// happen. All sends happen from the main loop (never from async callbacks).
class WsHub {
public:
    void begin(AsyncWebServer* server, InverterEngine* engine);
    void tick(uint32_t nowMs, bool mqttUp, bool wifiUp);
    void broadcastResult(const CmdResult& r);
    void broadcastEvent(const char* message);

private:
    AsyncWebSocket _ws{"/ws"};
    InverterEngine* _engine = nullptr;
    uint32_t _lastPush = 0;
    uint32_t _lastSentSeq = 0;
    uint32_t _lastCleanup = 0;
    bool _lastLink = false, _lastMqtt = false;
};
