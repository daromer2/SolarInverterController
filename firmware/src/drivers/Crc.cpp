#include "Crc.h"

namespace iot {

uint16_t mppCrc(const uint8_t* data, size_t len)
{
    static const uint16_t crcTable[16] = {
        0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
        0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef
    };

    uint16_t crc = 0;
    while (len-- != 0) {
        uint8_t da = ((uint8_t)(crc >> 8)) >> 4;
        crc <<= 4;
        crc ^= crcTable[da ^ (*data >> 4)];
        da = ((uint8_t)(crc >> 8)) >> 4;
        crc <<= 4;
        crc ^= crcTable[da ^ (*data & 0x0f)];
        data++;
    }

    uint8_t lo = (uint8_t)crc;
    uint8_t hi = (uint8_t)(crc >> 8);
    if (lo == 0x28 || lo == 0x0d || lo == 0x0a) lo++;
    if (hi == 0x28 || hi == 0x0d || hi == 0x0a) hi++;
    return ((uint16_t)hi << 8) | lo;
}

} // namespace iot
