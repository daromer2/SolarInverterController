// SolarInverterController v2 — ESP32 firmware for MPP Solar inverters
// (MPI 10KW / PIP series / PCM60X). Successor of InverterOfThings.
//
// Core layout: WiFi + async web server run on core 0 (SDK default), the
// inverter serial engine runs as its own task on core 1, and this loop()
// (also core 1) pumps MQTT, LEDs, guard and result dispatch.

#include <Arduino.h>
#include <LittleFS.h>
#include "AppConfig.h"
#include "drivers/profiles/ProfileRegistry.h"
#include "serial/InverterEngine.h"
#include "sys/SettingsStore.h"
#include "sys/LedManager.h"
#include "sys/FeedInGuard.h"
#include "sys/Logx.h"
#include "net/WifiManager.h"
#include "net/MqttBridge.h"
#include "net/WebApp.h"
#include "net/WsHub.h"
#include "net/OtaManager.h"

#ifdef DISABLE_BROWNOUT
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#endif

static SettingsStore settings;
static InverterEngine engine;
static LedManager leds;
static FeedInGuard guard;
static WifiManager wifi;
static MqttBridge mqtt;
static WebApp web;
static WsHub wsHub;
static OtaManager ota;

static void onGuardEvent(const char* message)
{
    mqtt.publishEvent(message);
    wsHub.broadcastEvent(message);
}

void setup()
{
#ifdef DISABLE_BROWNOUT
    // Escape hatch for boards whose regulator dips at WiFi radio power-on.
    // Trades brownout protection for the ability to run — only enable after
    // ruling out cable/supply/board, and prefer a 470uF cap on 3V3 instead.
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
#endif

    logx::begin();
    LOGI("main", "SolarInverterController v%s booting", FW_VERSION);

    settings.load();
    const Settings& s = settings.s;

    if (!LittleFS.begin(true))
        LOGI("main", "LittleFS mount failed");

    leds.begin(s.ledRedPin, s.ledGreenPin, s.ledActiveLow, s.ledEnabled);

    const iot::DeviceProfile* profile = iot::findProfile(s.profileId.c_str());
    if (!profile) {
        LOGI("main", "unknown profile '%s', falling back to PIP", s.profileId.c_str());
        profile = &iot::PROFILE_PIP;
    }
    engine.begin(profile, s.baudOverride, s.uartRx, s.uartTx);
    engine.setPvPowerFloor(s.pvMinPowerW);

    wifi.begin(s.wifiSsid, s.wifiPass, "ESP-" + s.deviceName);

    guard.begin(&engine, s.feedGuardEnabled, s.feedGuardTimeoutS,
                s.feedGuardFailsafeW, onGuardEvent);

    mqtt.begin(&settings, &engine, &guard);
    web.begin(&settings, &engine, &wsHub, &leds, wifi.isApMode());
    ota.begin("ESP-" + s.deviceName, s.otaPassword);
}

void loop()
{
    uint32_t now = millis();

    wifi.tick(now);
    ota.tick();

    bool wifiUp = wifi.isConnected();
    mqtt.tick(now, wifiUp);
    guard.tick(now);
    wsHub.tick(now, mqtt.connected(), wifiUp);

    // dispatch command results from the engine to MQTT + WebSocket
    CmdResult r;
    while (engine.popResult(r)) {
        LOGI("cmd", "result id=%lu key=%s wire=%s -> %s",
             (unsigned long)r.id, r.key, r.wire, r.status);
        mqtt.publishResult(r);
        wsHub.broadcastResult(r);
    }

    // PV power floor applies live (no reboot) when changed in settings
    static uint16_t pvFloor = settings.s.pvMinPowerW;
    if (settings.s.pvMinPowerW != pvFloor) {
        pvFloor = settings.s.pvMinPowerW;
        engine.setPvPowerFloor(pvFloor);
        LOGI("main", "PV power floor = %u W", pvFloor);
    }

    leds.setInverterFault(!engine.linkUp());
    leds.setApMode(wifi.isApMode());
    leds.setWifiUp(wifiUp);
    leds.setMqttUp(mqtt.connected());
    leds.tick(now);

    if (web.rebootRequested()) {
        LOGI("main", "rebooting");
        delay(100);
        ESP.restart();
    }

    delay(5);
}
