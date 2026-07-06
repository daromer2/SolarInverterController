#pragma once
#include <stdint.h>
#include "CommandDefs.h"

// Pure framing/deframing for the MPP Solar wire protocols. No Arduino
// dependencies — unit tested on the host.
//
// PI30 (PIP, PCM60X):  TX  <cmd><crcHi><crcLo>\r
//                      RX  (<payload><crcHi><crcLo>\r     data
//                          (ACK<crc>\r / (NAK<crc>\r      setter result
//
// PI17 (MPI):          TX  ^Pnnn<cmd>\r   query   (nnn = len(cmd)+1, NO CRC)
//                          ^Snnn<cmd>\r   setter
//                      RX  ^Dnnn<payload><crcHi><crcLo>\r data (nnn = payload+3)
//                          ^1<crc>\r / ^0<crc>\r          setter ACK / NAK
// Response CRC is always validated for both protocols.

namespace iot {

constexpr uint16_t FRAME_MAX = 96;     // longest TX frame
constexpr uint16_t RX_LINE_MAX = 300;  // longest RX line (QPIGS ~110, PI17 GS ~120)

struct TxFrame {
    uint8_t buf[FRAME_MAX];
    uint16_t len = 0;
};

// Build the wire frame for an unframed command string ("QPIGS", "GS",
// "POP02", "GPMP015000"). isSetter selects ^S over ^P for PI17.
// Returns false if the command doesn't fit.
bool frameCommand(ProtocolKind proto, bool txCrc, bool isSetter,
                  const char* cmd, TxFrame& out);

enum class RespStatus : uint8_t {
    Data,      // payload extracted
    Ack,       // setter accepted
    Nak,       // setter rejected
    BadCrc,
    TooShort,
    Malformed, // framing didn't match the protocol
};

struct RxView {
    const char* payload = nullptr;  // points into the caller's line buffer
    uint16_t    len = 0;
};

// Validate CRC and strip framing from a received line (without trailing CR).
// The line buffer is not modified; RxView points into it.
RespStatus parseResponse(ProtocolKind proto, const char* line, uint16_t len, RxView& out);

} // namespace iot
