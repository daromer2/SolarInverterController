#include "Protocol.h"
#include "Crc.h"
#include <string.h>
#include <stdio.h>

namespace iot {

bool frameCommand(ProtocolKind proto, bool txCrc, bool isSetter,
                  const char* cmd, TxFrame& out)
{
    size_t cmdLen = strlen(cmd);
    if (cmdLen == 0) return false;

    out.len = 0;
    if (proto == ProtocolKind::PI17) {
        // ^Pnnn / ^Snnn where nnn counts <cmd> plus the trailing CR
        if (cmdLen + 8 > FRAME_MAX || cmdLen + 1 > 999) return false;
        char prefix[8];
        snprintf(prefix, sizeof(prefix), "^%c%03u", isSetter ? 'S' : 'P',
                 (unsigned)(cmdLen + 1));
        memcpy(out.buf, prefix, 5);
        memcpy(out.buf + 5, cmd, cmdLen);
        out.len = 5 + cmdLen;
        if (txCrc) {  // not used by known PI17 devices, kept configurable
            uint16_t crc = mppCrc(out.buf, out.len);
            out.buf[out.len++] = (uint8_t)(crc >> 8);
            out.buf[out.len++] = (uint8_t)crc;
        }
    } else {
        if (cmdLen + 3 > FRAME_MAX) return false;
        memcpy(out.buf, cmd, cmdLen);
        out.len = cmdLen;
        if (txCrc) {
            uint16_t crc = mppCrc(out.buf, out.len);
            out.buf[out.len++] = (uint8_t)(crc >> 8);
            out.buf[out.len++] = (uint8_t)crc;
        }
    }
    out.buf[out.len++] = '\r';
    return true;
}

static bool crcOk(const char* line, uint16_t len)
{
    if (len < 3) return false;
    uint16_t calc = mppCrc((const uint8_t*)line, len - 2);
    uint16_t rx = ((uint16_t)(uint8_t)line[len - 2] << 8) | (uint8_t)line[len - 1];
    return calc == rx;
}

RespStatus parseResponse(ProtocolKind proto, const char* line, uint16_t len, RxView& out)
{
    out.payload = nullptr;
    out.len = 0;
    if (len < 3) return RespStatus::TooShort;

    if (proto == ProtocolKind::PI30) {
        if (line[0] != '(') return RespStatus::Malformed;
        if (!crcOk(line, len)) return RespStatus::BadCrc;
        out.payload = line + 1;
        out.len = len - 3;  // minus '(' and 2 CRC bytes
        if (out.len == 3 && memcmp(out.payload, "ACK", 3) == 0) return RespStatus::Ack;
        if (out.len == 3 && memcmp(out.payload, "NAK", 3) == 0) return RespStatus::Nak;
        return RespStatus::Data;
    }

    // PI17
    if (line[0] != '^') return RespStatus::Malformed;
    if (!crcOk(line, len)) return RespStatus::BadCrc;
    if (line[1] == '1') return RespStatus::Ack;
    if (line[1] == '0') return RespStatus::Nak;
    if (line[1] == 'D') {
        if (len < 7) return RespStatus::TooShort;  // ^Dnnn + 2 CRC
        out.payload = line + 5;
        out.len = len - 7;  // minus ^Dnnn header and 2 CRC bytes
        return RespStatus::Data;
    }
    return RespStatus::Malformed;
}

} // namespace iot
