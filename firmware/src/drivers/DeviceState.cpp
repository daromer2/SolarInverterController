#include "DeviceState.h"
#include <string.h>

namespace iot {

void DeviceState::clear()
{
    _n = 0;
    _seq = 0;
}

void DeviceState::bindProfile(const DeviceProfile* profile)
{
    clear();
    if (!profile) return;
    for (uint8_t q = 0; q < profile->nQueries; ++q) {
        const QueryCommandDef& qc = profile->queries[q];
        for (uint8_t f = 0; f < qc.nFields; ++f) {
            if (_n >= MAX_DATAPOINTS) return;
            _pts[_n] = Datapoint{};
            _pts[_n].def = &qc.fields[f];
            ++_n;
        }
    }
}

Datapoint* DeviceState::find(const char* key)
{
    for (uint8_t i = 0; i < _n; ++i)
        if (strcmp(_pts[i].def->key, key) == 0) return &_pts[i];
    return nullptr;
}

const Datapoint* DeviceState::find(const char* key) const
{
    return const_cast<DeviceState*>(this)->find(key);
}

void DeviceState::setNum(const char* key, float value, uint32_t nowMs)
{
    Datapoint* p = find(key);
    if (!p) return;
    p->updatedMs = nowMs;
    if (!p->valid || p->num != value) {
        p->num = value;
        p->valid = true;
        p->changeSeq = ++_seq;
    }
}

void DeviceState::setStr(const char* key, const char* value, uint16_t len, uint32_t nowMs)
{
    Datapoint* p = find(key);
    if (!p) return;
    if (len >= DP_STR_MAX) len = DP_STR_MAX - 1;
    p->updatedMs = nowMs;
    if (!p->valid || strncmp(p->str, value, len) != 0 || p->str[len] != 0) {
        memcpy(p->str, value, len);
        p->str[len] = 0;
        p->valid = true;
        p->changeSeq = ++_seq;
    }
}

} // namespace iot
