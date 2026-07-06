#include "WifiManager.h"
#include <WiFi.h>
#include "../AppConfig.h"
#include "../sys/Logx.h"

void WifiManager::begin(const String& ssid, const String& pass, const String& hostname)
{
    _ssid = ssid;
    _pass = pass;
    _hostname = hostname.length() ? hostname : "solar";
    _apSsid = String(AP_SSID_PREFIX) + String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFF), HEX);

    WiFi.persistent(false);   // NVS settings are ours, not the SDK's
    WiFi.setHostname(_hostname.c_str());

    if (_ssid.length() == 0) {
        LOGI("wifi", "no SSID configured, starting AP %s", _apSsid.c_str());
        startAp();
    } else {
        startSta(millis());
    }
}

void WifiManager::startSta(uint32_t nowMs)
{
    _staConfigured = true;
    _staStartedAt = nowMs;
    _lastStaRetry = nowMs;
    WiFi.mode(_apActive ? WIFI_AP_STA : WIFI_STA);
    WiFi.setTxPower(WIFI_TX_POWER);   // limit radio inrush (brownout guard)
    WiFi.begin(_ssid.c_str(), _pass.c_str());
    LOGI("wifi", "connecting to %s", _ssid.c_str());
}

void WifiManager::startAp()
{
    WiFi.mode(_staConfigured ? WIFI_AP_STA : WIFI_AP);
    WiFi.setTxPower(WIFI_TX_POWER);   // limit radio inrush (brownout guard)
    WiFi.softAP(_apSsid.c_str());
    _dns.start(53, "*", WiFi.softAPIP());   // captive portal: answer everything
    _apActive = true;
}

bool WifiManager::isConnected() const
{
    return WiFi.status() == WL_CONNECTED;
}

void WifiManager::reconfigure(const String& ssid, const String& pass)
{
    _ssid = ssid;
    _pass = pass;
    WiFi.disconnect();
    if (_ssid.length()) startSta(millis());
    else if (!_apActive) startAp();
}

void WifiManager::tick(uint32_t nowMs)
{
    if (_apActive) _dns.processNextRequest();

    if (!_staConfigured || _ssid.length() == 0) return;

    if (isConnected()) {
        if (_apActive) {
            LOGI("wifi", "connected (%s), stopping AP", WiFi.localIP().toString().c_str());
            _dns.stop();
            WiFi.softAPdisconnect(true);
            WiFi.mode(WIFI_STA);
            _apActive = false;
        }
        return;
    }

    // not connected: fall back to AP after the attempt budget
    if (!_apActive && (nowMs - _staStartedAt) > WIFI_STA_ATTEMPT_MS) {
        LOGI("wifi", "STA failed, enabling fallback AP %s", _apSsid.c_str());
        startAp();
        return;
    }

    // in AP fallback: retry STA periodically without dropping the AP
    if (_apActive && (nowMs - _lastStaRetry) > WIFI_RETRY_WHILE_AP_MS) {
        _lastStaRetry = nowMs;
        LOGI("wifi", "retrying STA while in AP mode");
        WiFi.begin(_ssid.c_str(), _pass.c_str());
    }
}
