#pragma once
#include <Arduino.h>

// ArduinoOTA (PlatformIO/IDE network uploads). The browser-based OTA upload
// endpoint lives in WebApp (Update.h); both firmware and LittleFS supported.
class OtaManager {
public:
    void begin(const String& hostname, const String& password);
    void tick();
    bool inProgress() const { return _active; }

private:
    bool _active = false;
};
