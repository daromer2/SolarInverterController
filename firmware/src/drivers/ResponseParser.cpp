#include "ResponseParser.h"
#include <stdlib.h>

namespace iot {

static bool isNumChar(char c)
{
    return (c >= '0' && c <= '9') || c == '.' || c == '+' || c == '-';
}

static uint8_t parsePositional(const QueryCommandDef& query, const char* p, uint16_t len,
                               DeviceState& state, uint32_t nowMs)
{
    uint16_t i = 0;
    uint8_t stored = 0;
    for (uint8_t f = 0; f < query.nFields; ++f) {
        const FieldDef& fd = query.fields[f];
        if (i >= len) break;

        switch (fd.type) {
        case FieldType::Float:
        case FieldType::Int: {
            char tok[20];
            uint8_t t = 0;
            while (i < len && isNumChar(p[i]) && t < sizeof(tok) - 1) tok[t++] = p[i++];
            tok[t] = 0;
            if (i < len) ++i;  // consume the delimiter
            // empty token (",," in PI17 GS) → field not reported, keep stale
            if (t > 0) {
                float v = strtof(tok, nullptr);
                if (fd.scale != 0 && fd.scale != 1) v /= fd.scale;
                state.setNum(fd.key, v, nowMs);
                ++stored;
            }
            break;
        }
        case FieldType::Bit:
            state.setNum(fd.key, p[i] == '1' ? 1.0f : 0.0f, nowMs);
            ++i;
            // PI17 sends option bits comma-separated; PI30 QPIWS packs them.
            if (i < len && (p[i] == ',' || p[i] == ' ')) ++i;
            ++stored;
            break;
        case FieldType::Str: {
            uint16_t start = i;
            while (i < len && p[i] != ',' && p[i] != ' ') ++i;
            state.setStr(fd.key, p + start, i - start, nowMs);
            if (i < len) ++i;  // consume the delimiter
            ++stored;
            break;
        }
        }
    }
    return stored;
}

// PI30 QFLAG payload: "EakxyDbjuvz" — letters after E are enabled flags,
// letters after D are disabled. Fields match by their FieldDef::code letter.
static uint8_t parseFlagLetters(const QueryCommandDef& query, const char* p, uint16_t len,
                                DeviceState& state, uint32_t nowMs)
{
    uint8_t stored = 0;
    bool enabled = true;
    for (uint16_t i = 0; i < len; ++i) {
        char c = p[i];
        if (c == 'E') { enabled = true; continue; }
        if (c == 'D') { enabled = false; continue; }
        for (uint8_t f = 0; f < query.nFields; ++f) {
            if (query.fields[f].code == c) {
                state.setNum(query.fields[f].key, enabled ? 1.0f : 0.0f, nowMs);
                ++stored;
                break;
            }
        }
    }
    return stored;
}

uint8_t parseFields(const QueryCommandDef& query, const char* payload, uint16_t len,
                    DeviceState& state, uint32_t nowMs)
{
    if (len < query.minLen) return 0;
    switch (query.parse) {
    case ParseKind::FlagLetters: return parseFlagLetters(query, payload, len, state, nowMs);
    case ParseKind::Positional:
    default:                     return parsePositional(query, payload, len, state, nowMs);
    }
}

} // namespace iot
