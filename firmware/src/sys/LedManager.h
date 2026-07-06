#pragma once
#include <Arduino.h>

// Status LEDs, preserving the legacy semantics:
//   red   = inverter comms fault (solid), blinking = AP/config mode
//   green = MQTT connected (solid), blinking = WiFi up but MQTT down
class LedManager {
public:
    void begin(uint8_t redPin, uint8_t greenPin, bool activeLow, bool enabled);

    void setInverterFault(bool fault) { _invFault = fault; }
    void setApMode(bool ap)           { _apMode = ap; }
    void setWifiUp(bool up)           { _wifiUp = up; }
    void setMqttUp(bool up)           { _mqttUp = up; }

    void tick(uint32_t nowMs);  // call from loop

private:
    void write(uint8_t pin, bool on);
    uint8_t _red = 0, _green = 0;
    bool _activeLow = false, _enabled = false;
    bool _invFault = true, _apMode = false, _wifiUp = false, _mqttUp = false;
};
