#include "WebApp.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Update.h>
#include <WiFi.h>
#include "../drivers/profiles/ProfileRegistry.h"
#include "../sys/Logx.h"

using namespace iot;

void WebApp::begin(SettingsStore* settings, InverterEngine* engine, WsHub* ws, bool apMode)
{
    _settings = settings;
    _engine = engine;
    _ws = ws;
    _apMode = apMode;

    _ws->begin(&_server, engine);
    setupApi();
    setupOta();
    setupStatic();
    _server.begin();
    LOGI("web", "server started (ap=%d)", apMode);
}

bool WebApp::auth(AsyncWebServerRequest* req)
{
    if (!_settings->s.authEnabled) return true;
    if (req->authenticate(_settings->s.webUser.c_str(), _settings->s.webPass.c_str()))
        return true;
    req->requestAuthentication();
    return false;
}

void WebApp::setupApi()
{
    // ---- live state ------------------------------------------------------------
    _server.on("/api/state", HTTP_GET, [this](AsyncWebServerRequest* req) {
        JsonDocument doc;
        _engine->stateToJson(doc);
        String out;
        serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    // ---- device model: profile as JSON → UI renders everything from this --------
    _server.on("/api/device", HTTP_GET, [this](AsyncWebServerRequest* req) {
        const DeviceProfile* p = _engine->profile();
        JsonDocument doc;
        doc["id"] = p->id;
        doc["label"] = p->label;
        doc["svg"] = p->svgAsset;
        doc["deviceName"] = _settings->s.deviceName;
        doc["fw"] = FW_VERSION;

        JsonArray fields = doc["fields"].to<JsonArray>();
        for (uint8_t q = 0; q < p->nQueries; ++q) {
            for (uint8_t f = 0; f < p->queries[q].nFields; ++f) {
                const FieldDef& fd = p->queries[q].fields[f];
                JsonObject o = fields.add<JsonObject>();
                o["key"] = fd.key;
                o["label"] = fd.label;
                if (fd.unit[0]) o["unit"] = fd.unit;
            }
        }

        JsonArray sets = doc["sets"].to<JsonArray>();
        for (uint8_t i = 0; i < p->nSets; ++i) {
            const SetCommandDef& sd = p->sets[i];
            JsonObject o = sets.add<JsonObject>();
            o["key"] = sd.key;
            o["label"] = sd.label;
            o["group"] = sd.group;
            if (sd.unit[0]) o["unit"] = sd.unit;
            switch (sd.arg) {
            case ArgKind::Enum: {
                o["type"] = "enum";
                JsonArray opts = o["options"].to<JsonArray>();
                for (uint8_t e = 0; e < sd.nEnum; ++e) opts.add(sd.enumLabels[e]);
                break;
            }
            case ArgKind::Bool:   o["type"] = "bool"; break;
            case ArgKind::String: o["type"] = "text"; break;
            case ArgKind::None:   o["type"] = "action"; break;
            default:
                o["type"] = "number";
                o["min"] = sd.min;
                o["max"] = sd.max;
                o["step"] = sd.step > 0 ? sd.step : 1;
            }
        }

        JsonArray modes = doc["modes"].to<JsonArray>();
        for (uint8_t i = 0; i < p->nModes; ++i) {
            JsonObject o = modes.add<JsonObject>();
            o["code"] = p->modes[i].code;
            o["label"] = p->modes[i].label;
        }
        if (p->modeKey) doc["modeKey"] = p->modeKey;

        JsonArray profs = doc["profiles"].to<JsonArray>();
        uint8_t n;
        const DeviceProfile* const* reg = profileRegistry(n);
        for (uint8_t i = 0; i < n; ++i) {
            JsonObject o = profs.add<JsonObject>();
            o["id"] = reg[i]->id;
            o["label"] = reg[i]->label;
        }

        String out;
        serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    // ---- commands ----------------------------------------------------------------
    _server.on("/api/cmd", HTTP_POST, [this](AsyncWebServerRequest* req) {
        if (!auth(req)) return;
        if (!req->hasParam("key", true) || !req->hasParam("value", true)) {
            req->send(400, "application/json", "{\"error\":\"key and value required\"}");
            return;
        }
        String key = req->getParam("key", true)->value();
        String value = req->getParam("value", true)->value();
        uint32_t id = ++_cmdCounter;
        SetError err = _engine->submitSet(key.c_str(), value.c_str(), CmdOrigin::UserWeb, id);
        JsonDocument doc;
        doc["id"] = id;
        doc["accepted"] = (err == SetError::Ok);
        if (err != SetError::Ok) doc["error"] = setErrorText(err);
        String out;
        serializeJson(doc, out);
        req->send(err == SetError::Ok ? 202 : 400, "application/json", out);
    });

    _server.on("/api/raw", HTTP_POST, [this](AsyncWebServerRequest* req) {
        if (!auth(req)) return;
        if (!req->hasParam("cmd", true)) {
            req->send(400, "application/json", "{\"error\":\"cmd required\"}");
            return;
        }
        String cmd = req->getParam("cmd", true)->value();
        uint32_t id = ++_cmdCounter;
        bool ok = _engine->submitRaw(cmd.c_str(), CmdOrigin::UserWeb, id);
        JsonDocument doc;
        doc["id"] = id;
        doc["accepted"] = ok;
        String out;
        serializeJson(doc, out);
        req->send(ok ? 202 : 400, "application/json", out);
    });

    // ---- settings ------------------------------------------------------------------
    _server.on("/api/settings", HTTP_GET, [this](AsyncWebServerRequest* req) {
        if (!auth(req)) return;
        JsonDocument doc;
        _settings->toJson(doc, false);
        String out;
        serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    auto settingsPost = [this](AsyncWebServerRequest* req, uint8_t* data, size_t len,
                               size_t index, size_t total) {
        if (!auth(req)) return;
        if (index != 0 || len != total || len > 2048) {
            req->send(400, "application/json", "{\"error\":\"too large\"}");
            return;
        }
        JsonDocument doc;
        if (deserializeJson(doc, data, len)) {
            req->send(400, "application/json", "{\"error\":\"bad json\"}");
            return;
        }
        _settings->fromJson(doc);
        _settings->save();
        req->send(200, "application/json", "{\"ok\":true,\"note\":\"reboot to apply\"}");
    };
    _server.on("/api/settings", HTTP_POST,
               [](AsyncWebServerRequest*) {}, nullptr, settingsPost);

    _server.on("/api/settings/export", HTTP_GET, [this](AsyncWebServerRequest* req) {
        if (!auth(req)) return;
        JsonDocument doc;
        _settings->toJson(doc, true);
        String out;
        serializeJson(doc, out);
        AsyncWebServerResponse* res = req->beginResponse(200, "application/json", out);
        res->addHeader("Content-Disposition", "attachment; filename=solar-settings.json");
        req->send(res);
    });
    _server.on("/api/settings/import", HTTP_POST,
               [](AsyncWebServerRequest*) {}, nullptr, settingsPost);

    // ---- wifi scan (for the setup page) ----------------------------------------------
    _server.on("/api/scan", HTTP_GET, [](AsyncWebServerRequest* req) {
        int n = WiFi.scanComplete();
        if (n == WIFI_SCAN_FAILED) {
            WiFi.scanNetworks(true);
            req->send(202, "application/json", "{\"scanning\":true}");
            return;
        }
        if (n == WIFI_SCAN_RUNNING) {
            req->send(202, "application/json", "{\"scanning\":true}");
            return;
        }
        JsonDocument doc;
        JsonArray arr = doc["networks"].to<JsonArray>();
        for (int i = 0; i < n; ++i) {
            JsonObject o = arr.add<JsonObject>();
            o["ssid"] = WiFi.SSID(i);
            o["rssi"] = WiFi.RSSI(i);
            o["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
        }
        WiFi.scanDelete();
        WiFi.scanNetworks(true);   // keep results fresh for the next call
        String out;
        serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    // ---- diagnostics -------------------------------------------------------------------
    _server.on("/api/log", HTTP_GET, [this](AsyncWebServerRequest* req) {
        if (!auth(req)) return;
        static char buf[4096];
        size_t n = logx::dump(buf, sizeof(buf) - 1);
        buf[n] = 0;
        req->send(200, "text/plain", buf);
    });

    _server.on("/api/reboot", HTTP_POST, [this](AsyncWebServerRequest* req) {
        if (!auth(req)) return;
        req->send(200, "application/json", "{\"ok\":true}");
        _rebootAt = millis() + 500;
    });

    _server.on("/api/factoryreset", HTTP_POST, [this](AsyncWebServerRequest* req) {
        if (!auth(req)) return;
        _settings->factoryReset();
        req->send(200, "application/json", "{\"ok\":true}");
        _rebootAt = millis() + 500;
    });
}

void WebApp::setupOta()
{
    // Browser OTA: POST /api/ota?type=fw|fs with the binary as file upload.
    _server.on("/api/ota", HTTP_POST,
        [this](AsyncWebServerRequest* req) {
            bool ok = !Update.hasError();
            req->send(ok ? 200 : 500, "application/json",
                      ok ? "{\"ok\":true,\"note\":\"rebooting\"}"
                         : "{\"ok\":false}");
            if (ok) _rebootAt = millis() + 1000;
        },
        [this](AsyncWebServerRequest* req, const String& filename, size_t index,
               uint8_t* data, size_t len, bool final) {
            if (index == 0) {
                if (!auth(req)) return;
                bool fs = req->hasParam("type") && req->getParam("type")->value() == "fs";
                LOGI("ota", "web upload start: %s (%s)", filename.c_str(), fs ? "fs" : "fw");
                if (fs) Update.begin(UPDATE_SIZE_UNKNOWN, U_SPIFFS);
                else    Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH);
            }
            if (len) Update.write(data, len);
            if (final) {
                if (Update.end(true)) LOGI("ota", "web upload done (%u bytes)", (unsigned)(index + len));
                else LOGI("ota", "web upload failed: %s", Update.errorString());
            }
        });
}

void WebApp::setupStatic()
{
    // Captive portal: unknown hosts get redirected to the device in AP mode.
    if (_apMode) {
        _server.on("/generate_204", [](AsyncWebServerRequest* req) {  // Android
            req->redirect("/setup.html");
        });
        _server.on("/hotspot-detect.html", [](AsyncWebServerRequest* req) {  // Apple
            req->redirect("/setup.html");
        });
    }

    // Browsers auto-request /favicon.ico; redirect to the SVG we ship so it
    // doesn't fall through to a filesystem probe + 404.
    _server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->redirect("/favicon.svg");
    });

    // no-cache: the browser must revalidate each load, so assets updated via
    // OTA (uploadfs) appear immediately instead of being served stale for
    // minutes. Files are small + gzipped, so the re-fetch cost is negligible.
    _server.serveStatic("/", LittleFS, "/")
        .setDefaultFile("index.html")
        .setCacheControl("no-cache");

    _server.onNotFound([this](AsyncWebServerRequest* req) {
        if (_apMode) {
            req->redirect("/setup.html");
            return;
        }
        // Recovery path: OTA + setup remain reachable even with an empty FS
        if (req->url() == "/") {
            req->send(200, "text/html",
                "<h1>SolarInverterController</h1>"
                "<p>Web assets missing — upload the filesystem image via "
                "<code>pio run -t uploadfs</code> or POST /api/ota?type=fs</p>");
            return;
        }
        req->send(404, "text/plain", "not found");
    });
}
