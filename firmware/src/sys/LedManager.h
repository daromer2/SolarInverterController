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

    // Blink both LEDs 5 times on the given pins/polarity (may differ from the
    // configured ones, so unsaved settings can be tried). Works even when LEDs
    // are disabled. Safe to call from the async web task: tick() does the I/O.
    void requestTest(uint8_t redPin, uint8_t greenPin, bool activeLow);

private:
    void write(uint8_t pin, bool on);
    uint8_t _red = 0, _green = 0;
    bool _activeLow = false, _enabled = false;
    bool _invFault = true, _apMode = false, _wifiUp = false, _mqttUp = false;

    // test request (written by web task, consumed by tick)
    volatile bool _testPending = false;
    volatile uint8_t _reqRed = 0, _reqGreen = 0;
    volatile bool _reqActiveLow = false;
    // running test
    bool _testing = false;
    uint8_t _tRed = 0, _tGreen = 0;
    bool _tActiveLow = false;
    uint32_t _testStart = 0;
};
