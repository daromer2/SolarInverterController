#pragma once
#include <Arduino.h>
#include "../drivers/Protocol.h"

// UART2 wrapper: writes frames, assembles RX lines terminated by CR.
// NULs and LFs are dropped (matches legacy behavior).
class InverterPort {
public:
    void begin(uint32_t baud, int8_t rxPin, int8_t txPin);
    void end();

    void send(const iot::TxFrame& frame);
    void flushInput();

    // Pumps RX. Returns true when a complete line is available in line/len
    // (without the CR). The buffer stays valid until the next service() call.
    bool service(const char*& line, uint16_t& len);

private:
    HardwareSerial _serial{2};
    char _buf[iot::RX_LINE_MAX];
    uint16_t _len = 0;
    bool _ready = false;
};
