#pragma once
#include <Arduino.h>
#include <DNSServer.h>

// Non-blocking WiFi state machine:
//   no SSID configured        -> AP with captive portal
//   SSID configured           -> STA; after WIFI_STA_ATTEMPT_MS without a
//                                connection -> AP fallback, retrying STA in
//                                the background every WIFI_RETRY_WHILE_AP_MS.
// Settings are never erased by fallback (unlike the legacy restart loop).
class WifiManager {
public:
    void begin(const String& ssid, const String& pass, const String& hostname);
    void tick(uint32_t nowMs);

    bool isConnected() const;
    bool isApMode() const { return _apActive; }
    String apSsid() const { return _apSsid; }

    // Reconnect with new credentials (after settings change).
    void reconfigure(const String& ssid, const String& pass);

private:
    void startSta(uint32_t nowMs);
    void startAp();

    String _ssid, _pass, _hostname, _apSsid;
    DNSServer _dns;
    bool _apActive = false;
    bool _staConfigured = false;
    uint32_t _staStartedAt = 0;
    uint32_t _lastStaRetry = 0;
};
