#pragma once
#include <stdint.h>

// Table-driven device model. A DeviceProfile is pure const data: query
// commands with positional field lists (driving the parser, MQTT keys,
// WebSocket JSON, HA discovery and the web UI) and set commands with
// validation ranges (driving MQTT command topics, HA entities and the
// auto-generated control panel). Adding a new inverter model means adding
// one profile data file — no new parsing code.

namespace iot {

enum class FieldType : uint8_t {
    Float,   // numeric token, scaled
    Int,     // numeric token, scaled, published without decimals
    Bit,     // single '0'/'1' character
    Str,     // token up to the next delimiter (serial numbers, status bits, mode)
};

struct FieldDef {
    const char* key;      // machine name: "battery_voltage"
    const char* label;    // human name:   "Battery voltage"
    const char* unit;     // "V", "W", "%", "" ...
    FieldType   type;
    float       scale;    // raw numeric is divided by this (1 / 10 / 100)
    const char* haClass;  // Home Assistant device_class, nullptr if none
    char        code;     // flag letter for ParseKind::FlagLetters, else 0
};

enum class ParseKind : uint8_t {
    Positional,   // comma/space separated tokens in field order (QPIGS, GS, ...)
    FlagLetters,  // PI30 QFLAG style: letters after 'E' enabled, after 'D' disabled
};

struct QueryCommandDef {
    const char* cmd;      // wire command without framing: "QPIGS", "GS"
    uint16_t    minLen;   // minimum payload length sanity check
    uint32_t    pollMs;   // polling period; 0 = query once after connect
    ParseKind   parse;
    const FieldDef* fields;
    uint8_t     nFields;
};

enum class ArgKind : uint8_t {
    None,     // command takes no argument
    Int,      // integer, fmt like "GPMP0%05d"
    Deci,     // value in engineering units, sent as value*10 integer ("MCHGC%04d" @ 0.1A)
    Fixed1,   // one decimal, fmt like "PBCV%04.1f"
    Enum,     // index (or enumValues[index]) formatted into fmt: "POP%02d"
    Bool,     // fmt is "trueCmd|falseCmd", e.g. "PEa|PDa" or "LON1|LON0"
    String,   // free-form argument appended via "%s" (advanced commands)
};

struct SetCommandDef {
    const char* key;        // "output_priority"
    const char* label;      // "Output source priority"
    const char* group;      // UI grouping: "Mode", "Battery", "Grid", "Flags", ...
    const char* unit;       // for numeric args
    const char* fmt;        // see ArgKind
    ArgKind     arg;
    float       min, max, step;          // numeric validation (Int/Deci/Fixed1)
    const char* const* enumLabels;       // Enum only
    const int16_t*     enumValues;       // Enum: wire values; nullptr = use index
    uint8_t     nEnum;
    const char* readback;   // query cmd to re-poll after ACK, nullptr = none
};

enum class ProtocolKind : uint8_t { PI30, PI17 };

// Maps a mode token (value of the profile's modeKey field) to a label.
struct ModeDef {
    const char* code;   // "L" (PI30 QMOD) or "05" (PI17 MOD)
    const char* label;  // "Line mode"
};

struct DeviceProfile {
    const char* id;        // "MPI" / "PIP" / "PCM" (matches legacy deviceType)
    const char* label;     // "MPP Solar MPI 10KW (PI17)"
    ProtocolKind protocol;
    uint32_t    baud;      // 2400 for all current devices
    bool        txCrc;     // append CRC to transmitted commands (PI30 yes, PI17 no)
    const QueryCommandDef* queries; uint8_t nQueries;
    const SetCommandDef*   sets;    uint8_t nSets;
    const ModeDef* modes;  uint8_t nModes;
    const char* modeKey;   // field key holding the mode token, nullptr if none
    const char* svgAsset;  // dashboard SVG path in LittleFS: "/svg/mpi.svg"
};

} // namespace iot
