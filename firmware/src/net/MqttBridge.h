#pragma once
#include <Arduino.h>
#include <espMqttClient.h>
#include "../serial/InverterEngine.h"
#include "../sys/SettingsStore.h"
#include "../sys/FeedInGuard.h"

// MQTT: new structured schema + optional exact legacy topics + Home
// Assistant discovery, all generated from the active profile's tables.
//
// Topics (base = <baseTopic>/<deviceName>):
//   base/availability          online|offline (retained, LWT)
//   base/state                 full JSON blob
//   base/sensor/<key>          per-datapoint values (on change)
//   base/mode                  mode label (retained)
//   base/warnings              JSON array of active warning labels
//   base/device                identity/diagnostics JSON (retained)
//   base/event                 guard/failsafe events
//   base/cmd/result            JSON result of every executed command
// Subscribed:
//   base/cmd/<setKey>          value payload
//   base/cmd/raw               raw command passthrough
//   base/cmd/feed_power        watts (fast path for the SDM630 controller)
//   base/heartbeat             controller keepalive (feeds the guard)
//   legacy: <base>/<name>/code (compat toggle)
class MqttBridge {
public:
    void begin(SettingsStore* settings, InverterEngine* engine, FeedInGuard* guard);
    void tick(uint32_t nowMs, bool wifiUp);
    bool connected() { return _client.connected(); }

    void publishEvent(const char* message);
    void publishResult(const CmdResult& r);

private:
    void connect();
    void onConnect(bool sessionPresent);
    void onMessage(const espMqttClientTypes::MessageProperties& props,
                   const char* topic, const uint8_t* payload,
                   size_t len, size_t index, size_t total);
    void publishTelemetry(uint32_t nowMs);
    void publishDeviceInfo();
    void publishLegacy(uint32_t nowMs);
    void publishHaDiscoveryChunk();   // incremental, a few configs per tick
    void handleCommand(const char* suffix, const char* value);

    espMqttClient _client;
    SettingsStore* _settings = nullptr;
    InverterEngine* _engine = nullptr;
    FeedInGuard* _guard = nullptr;

    String _base;          // <baseTopic>/<deviceName>
    String _availTopic;
    String _clientId;
    uint32_t _lastConnAttempt = 0;
    uint32_t _lastPublish = 0;
    uint32_t _lastDevice = 0;
    uint32_t _lastPublishedSeq = 0;
    int16_t _haCursor = -1;   // -1 = idle, else next discovery entity index
    uint32_t _cmdCounter = 1000;  // ids for MQTT-originated commands
};
