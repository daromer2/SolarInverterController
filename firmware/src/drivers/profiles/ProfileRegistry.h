#pragma once
#include "../CommandDefs.h"

namespace iot {

extern const DeviceProfile PROFILE_PIP;     // PI30 hybrid inverters (PIP series)
extern const DeviceProfile PROFILE_PCM60X;  // PI30 MPPT charger
extern const DeviceProfile PROFILE_MPI10K;  // PI17 MPI hybrid 10KW

// All registered profiles, in UI order.
const DeviceProfile* const* profileRegistry(uint8_t& count);

// Lookup by id ("PIP", "PCM", "MPI" — matches the legacy deviceType strings).
// Returns nullptr when unknown.
const DeviceProfile* findProfile(const char* id);

} // namespace iot
