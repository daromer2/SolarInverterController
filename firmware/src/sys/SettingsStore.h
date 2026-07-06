#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// All persistent configuration, backed by NVS (Preferences).
// Plain struct + explicit load/save keeps flash writes deliberate.

struct Settings {
    // network
    String wifiSsid;
    String wifiPass;
    String deviceName = "solar";     // hostname + MQTT topic segment

    // mqtt
    bool   mqttEnabled = true;
    String mqttServer;
    uint16_t mqttPort = 1883;
    String mqttUser;
    String mqttPass;
    String baseTopic = "solar";      // topics: <baseTopic>/<deviceName>/...
    uint16_t publishIntervalS = 5;
    bool   perFieldTopics = true;    // publish .../sensor/<key> topics
    bool   legacyCompat = false;     // replay exact legacy topic names
    bool   haDiscovery = true;
    String haPrefix = "homeassistant";

    // inverter
    String profileId = "PIP";        // "MPI" / "PIP" / "PCM"
    uint32_t baudOverride = 0;       // 0 = profile default
    uint8_t uartRx = 16, uartTx = 17;

    // web
    bool   authEnabled = true;
    String webUser = "admin";
    String webPass = "admin";

    // leds
    bool   ledEnabled = true;
    uint8_t ledRedPin = 25, ledGreenPin = 26;
    bool   ledActiveLow = false;

    // SDM630 feed-in failsafe (MPI only)
    bool   feedGuardEnabled = false;
    uint16_t feedGuardTimeoutS = 60;
    uint16_t feedGuardFailsafeW = 0;

    // ota
    String otaPassword;
};

class SettingsStore {
public:
    void load();
    void save() const;
    void factoryReset();

    // JSON round-trip for the web UI / backup. Passwords are exported —
    // the endpoint is auth-protected.
    void toJson(JsonDocument& doc, bool includeSecrets) const;
    // Applies only keys present in the document. Returns false on bad input.
    bool fromJson(JsonDocument& doc);

    Settings s;
};
