#include "FeedInGuard.h"
#include "Logx.h"
#include <string.h>

void FeedInGuard::begin(InverterEngine* engine, bool enabled, uint16_t timeoutS,
                        uint16_t failsafeW, EventFn onEvent)
{
    _engine = engine;
    _onEvent = onEvent;
    _timeoutMs = (uint32_t)timeoutS * 1000;
    _failsafeW = failsafeW;
    // only meaningful on profiles that support feed_grid_max_power
    _enabled = enabled && engine && engine->profile() &&
               strcmp(engine->profile()->id, "MPI") == 0;
    _lastMsg = millis();
}

void FeedInGuard::notifyController(uint32_t nowMs)
{
    _lastMsg = nowMs;
    _seenController = true;
    if (_latched) {
        _latched = false;
        LOGI("guard", "controller back, guard released");
        if (_onEvent) _onEvent("feed-in controller back online, failsafe released");
    }
}

void FeedInGuard::tick(uint32_t nowMs)
{
    if (!_enabled || _latched || !_seenController) return;
    if ((nowMs - _lastMsg) < _timeoutMs) return;

    _latched = true;
    char val[12];
    snprintf(val, sizeof(val), "%u", _failsafeW);
    _engine->submitSet("feed_grid_max_power", val, iot::CmdOrigin::FeedGuard, 0);
    LOGI("guard", "controller silent %lus -> feed-in forced to %uW",
         (unsigned long)(_timeoutMs / 1000), _failsafeW);
    if (_onEvent) _onEvent("feed-in controller silent — failsafe engaged");
}
