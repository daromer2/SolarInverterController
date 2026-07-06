#include "MqttBridge.h"
#include "MqttLegacyCompat.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include "../AppConfig.h"
#include "../sys/Logx.h"

using namespace iot;

static const uint32_t CONNECT_RETRY_MS = 5000;
static const uint32_t DEVICE_INFO_MS = 60000;

void MqttBridge::begin(SettingsStore* settings, InverterEngine* engine, FeedInGuard* guard)
{
    _settings = settings;
    _engine = engine;
    _guard = guard;

    const Settings& s = settings->s;
    _base = s.baseTopic + "/" + s.deviceName;
    _availTopic = _base + "/availability";
    _clientId = "ESP-" + s.deviceName;

    _client.setServer(s.mqttServer.c_str(), s.mqttPort);
    if (s.mqttUser.length())
        _client.setCredentials(s.mqttUser.c_str(), s.mqttPass.c_str());
    _client.setClientId(_clientId.c_str());
    _client.setWill(_availTopic.c_str(), 0, true, "offline");
    _client.setCleanSession(true);

    _client.onConnect([this](bool sp) { onConnect(sp); });
    _client.onMessage([this](const espMqttClientTypes::MessageProperties& p,
                             const char* topic, const uint8_t* payload,
                             size_t len, size_t index, size_t total) {
        onMessage(p, topic, payload, len, index, total);
    });
    _client.onDisconnect([](espMqttClientTypes::DisconnectReason reason) {
        LOGI("mqtt", "disconnected (%u)", (uint8_t)reason);
    });
}

void MqttBridge::connect()
{
    if (!_settings->s.mqttEnabled || _settings->s.mqttServer.length() == 0) return;
    LOGI("mqtt", "connecting to %s:%u", _settings->s.mqttServer.c_str(), _settings->s.mqttPort);
    _client.connect();
}

void MqttBridge::onConnect(bool)
{
    LOGI("mqtt", "connected");
    _client.publish(_availTopic.c_str(), 0, true, "online");

    _client.subscribe((_base + "/cmd/+").c_str(), 0);
    _client.subscribe((_base + "/heartbeat").c_str(), 0);
    if (_settings->s.legacyCompat)
        _client.subscribe((_base + "/code").c_str(), 0);

    publishDeviceInfo();
    if (_settings->s.legacyCompat) {
        // legacy "Im alive" info message
        String info = String("{\"Status\":\"Im alive!\", \"DeviceType\": \"") +
                      _settings->s.profileId + "\",\"IP \":\"" +
                      WiFi.localIP().toString() + "\"}";
        _client.publish((_base + "/Info").c_str(), 0, false, info.c_str());
    }
    if (_settings->s.haDiscovery) _haCursor = 0;  // start incremental discovery
    _lastPublishedSeq = 0;                         // republish everything
}

void MqttBridge::tick(uint32_t nowMs, bool wifiUp)
{
    if (!_settings->s.mqttEnabled) return;

    if (!_client.connected()) {
        if (wifiUp && (nowMs - _lastConnAttempt) > CONNECT_RETRY_MS) {
            _lastConnAttempt = nowMs;
            connect();
        }
        return;
    }

    if (_haCursor >= 0) publishHaDiscoveryChunk();

    if ((nowMs - _lastPublish) >= (uint32_t)_settings->s.publishIntervalS * 1000) {
        _lastPublish = nowMs;
        publishTelemetry(nowMs);
        if (_settings->s.legacyCompat) publishLegacy(nowMs);
    }

    if ((nowMs - _lastDevice) >= DEVICE_INFO_MS) {
        _lastDevice = nowMs;
        publishDeviceInfo();
    }
}

void MqttBridge::publishTelemetry(uint32_t nowMs)
{
    JsonDocument doc;
    _engine->stateToJson(doc);
    String out;
    serializeJson(doc, out);
    _client.publish((_base + "/state").c_str(), 0, false, out.c_str());

    const DeviceProfile* prof = _engine->profile();

    // per-field topics: only values that changed since last publish
    if (_settings->s.perFieldTopics) {
        JsonDocument delta;
        _engine->stateToJson(delta, _lastPublishedSeq);
        for (JsonPair kv : delta.as<JsonObject>()) {
            const char* key = kv.key().c_str();
            if (key[0] == '_') continue;
            char val[32];
            if (kv.value().is<const char*>())
                snprintf(val, sizeof(val), "%s", kv.value().as<const char*>());
            else
                snprintf(val, sizeof(val), "%g", kv.value().as<float>());
            _client.publish((_base + "/sensor/" + key).c_str(), 0, false, val);
        }
    }
    _lastPublishedSeq = doc["_seq"] | 0;

    // mode with label
    if (prof && prof->modeKey) {
        JsonDocument st;
        _engine->stateToJson(st);
        const char* code = st[prof->modeKey] | (const char*)nullptr;
        if (code) {
            const char* label = code;
            for (uint8_t i = 0; i < prof->nModes; ++i)
                if (strcmp(prof->modes[i].code, code) == 0) { label = prof->modes[i].label; break; }
            _client.publish((_base + "/mode").c_str(), 0, true, label);
        }
    }

    // active warnings
    {
        JsonDocument st;
        _engine->stateToJson(st);
        JsonDocument warnDoc;
        JsonArray warns = warnDoc.to<JsonArray>();
        if (prof) {
            for (uint8_t q = 0; q < prof->nQueries; ++q) {
                for (uint8_t f = 0; f < prof->queries[q].nFields; ++f) {
                    const FieldDef& fd = prof->queries[q].fields[f];
                    if (strncmp(fd.key, "warn_", 5) != 0) continue;
                    if ((st[fd.key] | 0.0f) >= 1.0f) warns.add(fd.label);
                }
            }
        }
        String w;
        serializeJson(warnDoc, w);
        _client.publish((_base + "/warnings").c_str(), 0, false, w.c_str());
    }
}

void MqttBridge::publishDeviceInfo()
{
    JsonDocument doc;
    doc["deviceType"] = _settings->s.profileId;
    doc["fw"] = FW_VERSION;
    doc["ip"] = WiFi.localIP().toString();
    doc["rssi"] = WiFi.RSSI();
    doc["uptime_s"] = millis() / 1000;
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["minHeap"] = ESP.getMinFreeHeap();
    doc["link"] = _engine->linkUp();
    String out;
    serializeJson(doc, out);
    _client.publish((_base + "/device").c_str(), 0, true, out.c_str());

    if (_settings->s.legacyCompat) {
        // legacy /uptime and /wifi
        String up = String("{\"human\":\"") + String(millis() / 1000) +
                    "s\", \"seconds\":" + String(millis() / 1000) + "}";
        _client.publish((_base + "/uptime").c_str(), 0, false, up.c_str());
        int rssi = WiFi.RSSI();
        int q = rssi <= -100 ? 0 : (rssi >= -50 ? 100 : 2 * (rssi + 100));
        String wifi = String("{ \"FreeRam\": ") + ESP.getFreeHeap() +
                      ", \"rssi\": " + rssi + ", \"dbm\": " + q + "}";
        _client.publish((_base + "/wifi").c_str(), 0, false, wifi.c_str());
    }
}

void MqttBridge::publishLegacy(uint32_t)
{
    const legacy::TopicMap* map = legacy::topicMap(_settings->s.profileId.c_str());
    if (!map) return;

    JsonDocument st;
    _engine->stateToJson(st);

    JsonDocument statusDoc;
    for (const legacy::TopicMap* m = map; m->legacyName; ++m) {
        float num = 0;
        const char* strVal = nullptr;
        if (m->fieldKey) {
            if (st[m->fieldKey].isNull()) continue;
            if (st[m->fieldKey].is<const char*>()) strVal = st[m->fieldKey];
            else num = st[m->fieldKey];
        } else {
            // computed legacy solar1w/solar2w = V * A from GS
            if (strcmp(m->legacyName, "solar1w") == 0)
                num = (st["pv1_voltage"] | 0.0f) * (st["pv1_current"] | 0.0f);
            else if (strcmp(m->legacyName, "solar2w") == 0)
                num = (st["pv2_voltage"] | 0.0f) * (st["pv2_current"] | 0.0f);
        }
        char val[32];
        if (strVal) snprintf(val, sizeof(val), "%s", strVal);
        else snprintf(val, sizeof(val), "%.2f", num);   // legacy String(float) = 2 decimals
        _client.publish((_base + "/" + m->legacyName).c_str(), 0, false, val);

        const char* jsonKey = legacy::statusJsonKey(_settings->s.profileId.c_str(), m->legacyName);
        if (strVal) statusDoc[jsonKey] = strVal;
        else statusDoc[jsonKey] = num;
    }
    String out;
    serializeJson(statusDoc, out);
    _client.publish((_base + "/status").c_str(), 0, false, out.c_str());
}

// Home Assistant discovery, generated from the profile tables. Published a
// few entities per tick to keep the TX buffer happy.
void MqttBridge::publishHaDiscoveryChunk()
{
    const DeviceProfile* prof = _engine->profile();
    if (!prof) { _haCursor = -1; return; }

    // entity list = all fields (sensors), then all sets (number/select/switch)
    int totalFields = 0;
    for (uint8_t q = 0; q < prof->nQueries; ++q) totalFields += prof->queries[q].nFields;
    int total = totalFields + prof->nSets;

    String uniqBase = _settings->s.deviceName;

    int published = 0;
    while (_haCursor < total && published < 4) {
        JsonDocument doc;
        JsonObject dev = doc["device"].to<JsonObject>();
        JsonArray ids = dev["identifiers"].to<JsonArray>();
        ids.add(_clientId);
        dev["name"] = _settings->s.deviceName;
        dev["manufacturer"] = "MPP Solar";
        dev["model"] = prof->label;
        dev["sw_version"] = FW_VERSION;
        doc["availability_topic"] = _availTopic;

        String cfgTopic;

        if (_haCursor < totalFields) {
            // sensor
            int idx = _haCursor;
            const FieldDef* fd = nullptr;
            for (uint8_t q = 0; q < prof->nQueries && !fd; ++q) {
                if (idx < prof->queries[q].nFields) fd = &prof->queries[q].fields[idx];
                else idx -= prof->queries[q].nFields;
            }
            // skip reserved/no-label plumbing fields
            if (strncmp(fd->key, "bats_reserved", 13) == 0 ||
                strstr(fd->key, "reserved") != nullptr) {
                ++_haCursor;
                continue;
            }
            doc["name"] = fd->label;
            doc["state_topic"] = _base + "/sensor/" + fd->key;
            doc["unique_id"] = uniqBase + "_" + fd->key;
            if (fd->unit[0]) doc["unit_of_measurement"] = fd->unit;
            if (fd->haClass) doc["device_class"] = fd->haClass;
            if (strncmp(fd->key, "warn_", 5) == 0 || fd->type == FieldType::Bit) {
                cfgTopic = _settings->s.haPrefix + "/binary_sensor/" + uniqBase + "/" + fd->key + "/config";
                doc.remove("unit_of_measurement");
                doc["payload_on"] = "1";
                doc["payload_off"] = "0";
            } else {
                cfgTopic = _settings->s.haPrefix + "/sensor/" + uniqBase + "/" + fd->key + "/config";
            }
        } else {
            const SetCommandDef& sd = prof->sets[_haCursor - totalFields];
            doc["name"] = sd.label;
            doc["unique_id"] = uniqBase + "_set_" + sd.key;
            doc["command_topic"] = _base + "/cmd/" + sd.key;
            switch (sd.arg) {
            case ArgKind::Enum: {
                cfgTopic = _settings->s.haPrefix + "/select/" + uniqBase + "/" + sd.key + "/config";
                JsonArray opts = doc["options"].to<JsonArray>();
                for (uint8_t i = 0; i < sd.nEnum; ++i) opts.add(sd.enumLabels[i]);
                break;
            }
            case ArgKind::Bool:
                cfgTopic = _settings->s.haPrefix + "/switch/" + uniqBase + "/" + sd.key + "/config";
                doc["payload_on"] = "1";
                doc["payload_off"] = "0";
                break;
            case ArgKind::Int:
            case ArgKind::Deci:
            case ArgKind::Fixed1:
                cfgTopic = _settings->s.haPrefix + "/number/" + uniqBase + "/" + sd.key + "/config";
                doc["min"] = sd.min;
                doc["max"] = sd.max;
                doc["step"] = sd.step > 0 ? sd.step : 1;
                if (sd.unit[0]) doc["unit_of_measurement"] = sd.unit;
                break;
            default:
                // String args (advanced) get a text entity
                cfgTopic = _settings->s.haPrefix + "/text/" + uniqBase + "/" + sd.key + "/config";
                break;
            }
        }

        String out;
        serializeJson(doc, out);
        _client.publish(cfgTopic.c_str(), 0, true, out.c_str());
        ++_haCursor;
        ++published;
    }
    if (_haCursor >= total) {
        LOGI("mqtt", "HA discovery published (%d entities)", total);
        _haCursor = -1;
    }
}

void MqttBridge::handleCommand(const char* suffix, const char* value)
{
    uint32_t id = ++_cmdCounter;

    if (strcmp(suffix, "raw") == 0) {
        _engine->submitRaw(value, CmdOrigin::UserMqtt, id);
        return;
    }
    if (strcmp(suffix, "feed_power") == 0) {
        // fast path for the SDM630 controller + guard keepalive
        if (_guard) _guard->notifyController(millis());
        SetError err = _engine->submitSet("feed_grid_max_power", value, CmdOrigin::UserMqtt, id);
        if (err != SetError::Ok) {
            CmdResult r{};
            r.id = id;
            snprintf(r.key, sizeof(r.key), "feed_power");
            snprintf(r.value, sizeof(r.value), "%s", value);
            snprintf(r.status, sizeof(r.status), "rejected");
            snprintf(r.response, sizeof(r.response), "%s", setErrorText(err));
            publishResult(r);
        }
        return;
    }

    SetError err = _engine->submitSet(suffix, value, CmdOrigin::UserMqtt, id);
    if (err != SetError::Ok) {
        CmdResult r{};
        r.id = id;
        snprintf(r.key, sizeof(r.key), "%s", suffix);
        snprintf(r.value, sizeof(r.value), "%s", value);
        snprintf(r.status, sizeof(r.status), "rejected");
        snprintf(r.response, sizeof(r.response), "%s", setErrorText(err));
        publishResult(r);
    }
}

void MqttBridge::onMessage(const espMqttClientTypes::MessageProperties&,
                           const char* topic, const uint8_t* payload,
                           size_t len, size_t index, size_t total)
{
    if (index != 0 || len != total || len > 128) return;  // no chunked commands

    char value[129];
    memcpy(value, payload, len);
    value[len] = 0;
    // trim whitespace (legacy did too)
    while (len && (value[len - 1] == ' ' || value[len - 1] == '\n' || value[len - 1] == '\r'))
        value[--len] = 0;

    String t(topic);
    if (!t.startsWith(_base)) return;
    String suffix = t.substring(_base.length() + 1);

    LOGI("mqtt", "rx %s = %s", suffix.c_str(), value);

    if (suffix == "heartbeat") {
        if (_guard) _guard->notifyController(millis());
        return;
    }
    if (suffix == "code") {   // legacy raw passthrough
        _engine->submitRaw(value, CmdOrigin::UserMqtt, ++_cmdCounter);
        return;
    }
    if (suffix.startsWith("cmd/")) {
        handleCommand(suffix.c_str() + 4, value);
        return;
    }
}

void MqttBridge::publishResult(const CmdResult& r)
{
    if (!_client.connected()) return;

    JsonDocument doc;
    doc["id"] = r.id;
    doc["key"] = r.key;
    doc["wire"] = r.wire;
    doc["value"] = r.value;
    doc["status"] = r.status;
    if (r.response[0]) doc["response"] = r.response;
    String out;
    serializeJson(doc, out);
    _client.publish((_base + "/cmd/result").c_str(), 0, false, out.c_str());

    if (_settings->s.legacyCompat && r.key[0] == 0) {
        // legacy raw command debug topics
        _client.publish((_base + "/debug/sent").c_str(), 0, false, r.wire);
        _client.publish((_base + "/debug/recieved").c_str(), 0, false, r.response);
        JsonDocument dbg;
        dbg["sentMessage"] = r.wire;
        dbg["recievedMessage"] = r.response;
        String dj;
        serializeJson(dbg, dj);
        _client.publish((_base + "/debug/json").c_str(), 0, false, dj.c_str());
    }
}

void MqttBridge::publishEvent(const char* message)
{
    if (_client.connected())
        _client.publish((_base + "/event").c_str(), 0, false, message);
}
