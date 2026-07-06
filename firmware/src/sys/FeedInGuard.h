#pragma once
#include <Arduino.h>
#include "../serial/InverterEngine.h"

// Failsafe for external feed-in control (the SDM630 metering ESP32 sends
// solar/<name>/cmd/feed_power). If controller messages stop for the
// configured timeout, feed-in power is set once to the failsafe value and
// the guard latches until the controller talks again — it never fights an
// alive controller and never persists setpoints to NVS.
class FeedInGuard {
public:
    using EventFn = void (*)(const char* message);

    void begin(InverterEngine* engine, bool enabled, uint16_t timeoutS,
               uint16_t failsafeW, EventFn onEvent);

    void notifyController(uint32_t nowMs);  // any feed_power/heartbeat message
    void tick(uint32_t nowMs);

    bool isLatched() const { return _latched; }
    bool isEnabled() const { return _enabled; }

private:
    InverterEngine* _engine = nullptr;
    EventFn _onEvent = nullptr;
    bool _enabled = false;
    uint32_t _timeoutMs = 60000;
    uint16_t _failsafeW = 0;
    uint32_t _lastMsg = 0;
    bool _seenController = false;
    bool _latched = false;
};
