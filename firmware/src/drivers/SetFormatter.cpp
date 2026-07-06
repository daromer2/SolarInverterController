#include "SetFormatter.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

namespace iot {

static bool parseBool(const char* v, bool& out)
{
    if (!strcmp(v, "1") || !strcasecmp(v, "true") || !strcasecmp(v, "on"))  { out = true;  return true; }
    if (!strcmp(v, "0") || !strcasecmp(v, "false") || !strcasecmp(v, "off")) { out = false; return true; }
    return false;
}

static bool numericInRange(const SetCommandDef& d, float v)
{
    if (v < d.min || v > d.max) return false;
    if (d.step > 0) {
        float steps = (v - d.min) / d.step;
        if (fabsf(steps - roundf(steps)) > 0.001f) return false;
    }
    return true;
}

SetError formatSetCommand(const SetCommandDef& def, const char* value, char out[SET_WIRE_MAX])
{
    out[0] = 0;
    switch (def.arg) {
    case ArgKind::None:
        snprintf(out, SET_WIRE_MAX, "%s", def.fmt);
        return SetError::Ok;

    case ArgKind::Int:
    case ArgKind::Deci:
    case ArgKind::Fixed1: {
        char* end = nullptr;
        float v = strtof(value, &end);
        if (end == value || (end && *end != 0)) return SetError::BadValue;
        if (!numericInRange(def, v)) return SetError::OutOfRange;
        if (def.arg == ArgKind::Fixed1)
            snprintf(out, SET_WIRE_MAX, def.fmt, (double)v);
        else if (def.arg == ArgKind::Deci)
            snprintf(out, SET_WIRE_MAX, def.fmt, (int)lroundf(v * 10.0f));
        else
            snprintf(out, SET_WIRE_MAX, def.fmt, (int)lroundf(v));
        return SetError::Ok;
    }

    case ArgKind::Enum: {
        int idx = -1;
        for (uint8_t i = 0; i < def.nEnum; ++i) {
            if (strcasecmp(value, def.enumLabels[i]) == 0) { idx = i; break; }
        }
        if (idx < 0) {  // allow a numeric index too
            char* end = nullptr;
            long n = strtol(value, &end, 10);
            if (end != value && *end == 0 && n >= 0 && n < def.nEnum) idx = (int)n;
        }
        if (idx < 0) return SetError::BadEnum;
        int wire = def.enumValues ? def.enumValues[idx] : idx;
        snprintf(out, SET_WIRE_MAX, def.fmt, wire);
        return SetError::Ok;
    }

    case ArgKind::Bool: {
        bool b;
        if (!parseBool(value, b)) return SetError::BadValue;
        // fmt = "trueCmd|falseCmd"
        const char* sep = strchr(def.fmt, '|');
        if (!sep) return SetError::BadValue;
        if (b) {
            size_t n = (size_t)(sep - def.fmt);
            if (n >= SET_WIRE_MAX) return SetError::TooLong;
            memcpy(out, def.fmt, n);
            out[n] = 0;
        } else {
            snprintf(out, SET_WIRE_MAX, "%s", sep + 1);
        }
        return SetError::Ok;
    }

    case ArgKind::String: {
        if (strlen(value) == 0) return SetError::BadValue;
        int n = snprintf(out, SET_WIRE_MAX, def.fmt, value);
        if (n < 0 || n >= SET_WIRE_MAX) return SetError::TooLong;
        return SetError::Ok;
    }
    }
    return SetError::BadValue;
}

const char* setErrorText(SetError e)
{
    switch (e) {
    case SetError::Ok:             return "ok";
    case SetError::UnknownCommand: return "unknown command";
    case SetError::OutOfRange:     return "value out of range";
    case SetError::BadEnum:        return "invalid option";
    case SetError::BadValue:       return "invalid value";
    case SetError::TooLong:        return "value too long";
    }
    return "error";
}

} // namespace iot
