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

void LedManager::tick(uint32_t nowMs)
{
    if (!_enabled) return;
    bool blink = (nowMs / 500) & 1;

    if (_apMode)          write(_red, blink);
    else                  write(_red, _invFault);

    if (_mqttUp)          write(_green, true);
    else if (_wifiUp)     write(_green, blink);
    else                  write(_green, false);
}
