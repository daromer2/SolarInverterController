#include "ProfileRegistry.h"
#include <string.h>

namespace iot {

static const DeviceProfile* const REGISTRY[] = {
    &PROFILE_MPI10K,
    &PROFILE_PIP,
    &PROFILE_PCM60X,
};

const DeviceProfile* const* profileRegistry(uint8_t& count)
{
    count = sizeof(REGISTRY) / sizeof(REGISTRY[0]);
    return REGISTRY;
}

const DeviceProfile* findProfile(const char* id)
{
    if (!id) return nullptr;
    uint8_t n;
    const DeviceProfile* const* reg = profileRegistry(n);
    for (uint8_t i = 0; i < n; ++i)
        if (strcmp(reg[i]->id, id) == 0) return reg[i];
    return nullptr;
}

} // namespace iot
