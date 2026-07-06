#pragma once
#include <stdint.h>
#include "CommandDefs.h"
#include "DeviceState.h"

namespace iot {

// Parses a deframed payload according to a query's field table and stores
// the results in DeviceState. Returns the number of fields stored.
// Tokenization matches the proven legacy behavior: numeric tokens accept
// [0-9 . + -], any other character terminates the token; missing trailing
// tokens leave their datapoints untouched (stale, not invalidated).
uint8_t parseFields(const QueryCommandDef& query, const char* payload, uint16_t len,
                    DeviceState& state, uint32_t nowMs);

} // namespace iot
