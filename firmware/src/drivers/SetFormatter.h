#pragma once
#include <stdint.h>
#include "CommandDefs.h"

namespace iot {

enum class SetError : uint8_t {
    Ok,
    UnknownCommand,
    OutOfRange,
    BadEnum,
    BadValue,
    TooLong,
};

constexpr uint8_t SET_WIRE_MAX = 48;

// Validates `value` (textual, as received from MQTT/web) against the set
// command's constraints and renders the unframed wire command.
// Enum accepts either the label ("SBU") or a numeric index.
// Bool accepts 1/0/true/false/on/off.
SetError formatSetCommand(const SetCommandDef& def, const char* value,
                          char out[SET_WIRE_MAX]);

const char* setErrorText(SetError e);

} // namespace iot
