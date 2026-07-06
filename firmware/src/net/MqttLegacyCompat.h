#pragma once
#include <stdint.h>

// Maps legacy topic names (exactly as published by old_inverterofthings
// src.ino) to the new datapoint keys, so existing dashboards and the
// current SDM630 controller keep working with legacyCompat enabled.

namespace legacy {

struct TopicMap {
    const char* legacyName;  // topic suffix AND key in the legacy /status JSON
    const char* fieldKey;    // new DeviceState key; nullptr => computed
};

// Returns the map for a profile id ("MPI"/"PIP"/"PCM"), nullptr-terminated
// by legacyName == nullptr.
const TopicMap* topicMap(const char* profileId);

// Key used in the legacy MPI /status JSON where it differed from the topic
// name (acOutputProcentage vs ac_output_procent).
const char* statusJsonKey(const char* profileId, const char* legacyName);

} // namespace legacy
