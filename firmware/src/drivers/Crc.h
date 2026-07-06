#pragma once
#include <stdint.h>
#include <stddef.h>

namespace iot {

// CRC16-XMODEM variant used by all MPP Solar protocols (PI30/PI17 responses).
// After the plain XMODEM calculation, any CRC byte equal to 0x28 '(',
// 0x0D CR or 0x0A LF is incremented by one so framing characters never
// appear inside the CRC. Ported from the proven legacy implementation
// (old_inverterofthings/src/inverter.cpp cal_crc_half) — a stock XMODEM
// library will NOT talk to these inverters.
uint16_t mppCrc(const uint8_t* data, size_t len);

} // namespace iot
