#include "LedManager.h"

void LedManager::begin(uint8_t redPin, uint8_t greenPin, bool activeLow, bool enabled)
{
    _red = redPin;
    _green = greenPin;
    _activeLow = activeLow;
    _enabled = enabled;
    if (!_enabled) return;
    pinMode(_red, OUTPUT);
    pinMode(_green, OUTPUT);
    write(_red, true);    // fault until proven otherwise
    write(_green, false);
}

void LedManager::write(uint8_t pin, bool on)
{
    digitalWrite(pin, (on ^ _activeLow) ? HIGH : LOW);
}

void LedManager::requestTest(uint8_t redPin, uint8_t greenPin, bool activeLow)
{
    _reqRed = redPin;
    _reqGreen = greenPin;
    _reqActiveLow = activeLow;
    _testPending = true;
}

static constexpr uint32_t TEST_HALF_MS = 300;   // on 300 ms, off 300 ms
static constexpr uint32_t TEST_BLINKS = 5;

void LedManager::tick(uint32_t nowMs)
{
    if (_testPending) {
        _testPending = false;
        _tRed = _reqRed;
        _tGreen = _reqGreen;
        _tActiveLow = _reqActiveLow;
        pinMode(_tRed, OUTPUT);
        pinMode(_tGreen, OUTPUT);
        _testStart = nowMs;
        _testing = true;
    }
    if (_testing) {
        uint32_t el = nowMs - _testStart;
        bool done = el >= TEST_BLINKS * 2 * TEST_HALF_MS;
        bool on = !done && ((el / TEST_HALF_MS) & 1) == 0;
        digitalWrite(_tRed,   (on ^ _tActiveLow) ? HIGH : LOW);
        digitalWrite(_tGreen, (on ^ _tActiveLow) ? HIGH : LOW);
        if (!done) return;
        _testing = false;   // test pins left off; normal status resumes below
    }

    if (!_enabled) return;
    bool blink = (nowMs / 500) & 1;

    if (_apMode)          write(_red, blink);
    else                  write(_red, _invFault);

    if (_mqttUp)          write(_green, true);
    else if (_wifiUp)     write(_green, blink);
    else                  write(_green, false);
}
