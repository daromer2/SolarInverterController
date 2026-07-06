#include "InverterPort.h"

void InverterPort::begin(uint32_t baud, int8_t rxPin, int8_t txPin)
{
    _serial.begin(baud, SERIAL_8N1, rxPin, txPin);
    _len = 0;
    _ready = false;
}

void InverterPort::end()
{
    _serial.end();
}

void InverterPort::send(const iot::TxFrame& frame)
{
    _serial.write(frame.buf, frame.len);
}

void InverterPort::flushInput()
{
    while (_serial.available()) _serial.read();
    _len = 0;
    _ready = false;
}

bool InverterPort::service(const char*& line, uint16_t& len)
{
    if (_ready) {   // previous line consumed
        _len = 0;
        _ready = false;
    }
    while (_serial.available()) {
        char c = (char)_serial.read();
        if (c == '\r') {
            if (_len > 0) {
                _buf[_len] = 0;
                _ready = true;
                line = _buf;
                len = _len;
                return true;
            }
        } else if (c != 0 && c != '\n') {
            if (_len < sizeof(_buf) - 1) _buf[_len++] = c;
        }
    }
    return false;
}
