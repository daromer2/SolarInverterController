#pragma once
#include <stdint.h>
#include "CommandDefs.h"

// Keyed datapoint store for everything parsed from the inverter. One entry
// per FieldDef of the bound profile. Change tracking is sequence-number
// based so WebSocket clients and MQTT can send deltas cheaply.
//
// Thread safety is the caller's job (InverterEngine wraps access in a mutex
// on the ESP32); the class itself stays pure for host-side tests.

namespace iot {

constexpr uint8_t MAX_DATAPOINTS = 128;
constexpr uint8_t DP_STR_MAX = 24;

struct Datapoint {
    const FieldDef* def = nullptr;
    float    num = 0;
    char     str[DP_STR_MAX] = {0};
    uint32_t updatedMs = 0;   // last write (even if value unchanged)
    uint32_t changeSeq = 0;   // global sequence at last value change
    bool     valid = false;
};

class DeviceState {
public:
    // Registers one datapoint per field of every query in the profile.
    void bindProfile(const DeviceProfile* profile);
    void clear();

    Datapoint* find(const char* key);
    const Datapoint* find(const char* key) const;

    void setNum(const char* key, float value, uint32_t nowMs);
    void setStr(const char* key, const char* value, uint16_t len, uint32_t nowMs);

    uint8_t count() const { return _n; }
    const Datapoint& at(uint8_t i) const { return _pts[i]; }
    uint32_t seq() const { return _seq; }

private:
    Datapoint _pts[MAX_DATAPOINTS];
    uint8_t _n = 0;
    uint32_t _seq = 0;
};

} // namespace iot
