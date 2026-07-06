#include "WsHub.h"
#include <ArduinoJson.h>
#include <WiFi.h>

static const uint32_t PUSH_MS = 1000;
static const size_t MAX_CLIENTS = 4;

void WsHub::begin(AsyncWebServer* server, InverterEngine* engine)
{
    _engine = engine;
    _ws.onEvent([this](AsyncWebSocket*, AsyncWebSocketClient* client,
                       AwsEventType type, void*, uint8_t*, size_t) {
        if (type == WS_EVT_CONNECT && _ws.count() > MAX_CLIENTS)
            client->close();
        // full state is sent by the next tick (seq forced to 0)
        if (type == WS_EVT_CONNECT) _lastSentSeq = 0;
    });
    server->addHandler(&_ws);
}

void WsHub::tick(uint32_t nowMs, bool mqttUp, bool wifiUp)
{
    if ((nowMs - _lastCleanup) > 5000) {
        _lastCleanup = nowMs;
        _ws.cleanupClients(MAX_CLIENTS);
    }
    if (_ws.count() == 0) return;

    bool link = _engine->linkUp();
    bool connChanged = (link != _lastLink) || (mqttUp != _lastMqtt);

    if ((nowMs - _lastPush) < PUSH_MS && !connChanged) return;
    _lastPush = nowMs;
    _lastLink = link;
    _lastMqtt = mqttUp;

    uint32_t seq = _engine->stateSeq();
    if (seq != _lastSentSeq) {
        JsonDocument doc;
        doc["t"] = "state";
        JsonObject d = doc["d"].to<JsonObject>();
        JsonDocument state;
        _engine->stateToJson(state, _lastSentSeq);
        for (JsonPair kv : state.as<JsonObject>())
            if (kv.key().c_str()[0] != '_') d[kv.key()] = kv.value();
        String out;
        serializeJson(doc, out);
        _ws.textAll(out);
        _lastSentSeq = seq;
    }

    JsonDocument conn;
    conn["t"] = "conn";
    conn["inv"] = link;
    conn["mqtt"] = mqttUp;
    conn["wifi"] = wifiUp;
    conn["rssi"] = WiFi.RSSI();
    conn["heap"] = ESP.getFreeHeap();
    conn["uptime"] = nowMs / 1000;
    String out;
    serializeJson(conn, out);
    _ws.textAll(out);
}

void WsHub::broadcastResult(const CmdResult& r)
{
    if (_ws.count() == 0) return;
    JsonDocument doc;
    doc["t"] = "cmd";
    doc["id"] = r.id;
    doc["key"] = r.key;
    doc["wire"] = r.wire;
    doc["value"] = r.value;
    doc["status"] = r.status;
    if (r.response[0]) doc["response"] = r.response;
    String out;
    serializeJson(doc, out);
    _ws.textAll(out);
}

void WsHub::broadcastEvent(const char* message)
{
    if (_ws.count() == 0) return;
    JsonDocument doc;
    doc["t"] = "event";
    doc["message"] = message;
    String out;
    serializeJson(doc, out);
    _ws.textAll(out);
}
