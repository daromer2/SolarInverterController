#include "PollScheduler.h"
#include <string.h>
#include <stdio.h>

namespace iot {

void PollScheduler::bindProfile(const DeviceProfile* profile, uint32_t nowMs)
{
    _profile = profile;
    _userHead = _userCount = 0;
    _rbHead = _rbCount = 0;
    if (!profile) return;
    for (uint8_t i = 0; i < profile->nQueries && i < 32; ++i) {
        _pollDue[i] = nowMs;   // everything due immediately on bind
        _bootDone[i] = false;
    }
}

bool PollScheduler::pushUser(const CmdRequest& req)
{
    if (_userCount >= USER_QUEUE) return false;
    _user[(_userHead + _userCount) % USER_QUEUE] = req;
    ++_userCount;
    return true;
}

void PollScheduler::pushReadback(const char* queryCmd)
{
    if (!_profile || !queryCmd) return;
    const QueryCommandDef* q = nullptr;
    for (uint8_t i = 0; i < _profile->nQueries; ++i) {
        if (strcmp(_profile->queries[i].cmd, queryCmd) == 0) {
            q = &_profile->queries[i];
            break;
        }
    }
    if (!q) return;
    for (uint8_t i = 0; i < _rbCount; ++i)   // dedup
        if (_readback[(_rbHead + i) % READBACK_QUEUE] == q) return;
    if (_rbCount >= READBACK_QUEUE) return;
    _readback[(_rbHead + _rbCount) % READBACK_QUEUE] = q;
    ++_rbCount;
}

bool PollScheduler::next(uint32_t nowMs, CmdRequest& out)
{
    if (_userCount > 0) {
        out = _user[_userHead];
        _userHead = (_userHead + 1) % USER_QUEUE;
        --_userCount;
        return true;
    }

    if (_rbCount > 0) {
        const QueryCommandDef* q = _readback[_rbHead];
        _rbHead = (_rbHead + 1) % READBACK_QUEUE;
        --_rbCount;
        out = CmdRequest{};
        snprintf(out.wire, sizeof(out.wire), "%s", q->cmd);
        out.origin = CmdOrigin::Readback;
        out.query = q;
        return true;
    }

    if (!_profile) return false;

    // earliest-due poll query wins; boot-only queries (pollMs==0) run once
    int8_t best = -1;
    for (uint8_t i = 0; i < _profile->nQueries && i < 32; ++i) {
        if (_profile->queries[i].pollMs == 0 && _bootDone[i]) continue;
        if ((int32_t)(nowMs - _pollDue[i]) < 0) continue;
        if (best < 0 || (int32_t)(_pollDue[i] - _pollDue[best]) < 0) best = (int8_t)i;
    }
    if (best < 0) return false;

    const QueryCommandDef* q = &_profile->queries[best];
    out = CmdRequest{};
    snprintf(out.wire, sizeof(out.wire), "%s", q->cmd);
    out.origin = CmdOrigin::Poll;
    out.query = q;
    return true;
}

void PollScheduler::onPollDone(const QueryCommandDef* query, uint32_t nowMs)
{
    if (!_profile || !query) return;
    for (uint8_t i = 0; i < _profile->nQueries && i < 32; ++i) {
        if (&_profile->queries[i] == query) {
            if (query->pollMs == 0) _bootDone[i] = true;
            else _pollDue[i] = nowMs + query->pollMs;
            return;
        }
    }
}

void PollScheduler::backoff(uint32_t nowMs, uint32_t backoffMs)
{
    if (!_profile) return;
    for (uint8_t i = 0; i < _profile->nQueries && i < 32; ++i) {
        uint32_t due = nowMs + backoffMs;
        if ((int32_t)(due - _pollDue[i]) > 0) _pollDue[i] = due;
    }
}

} // namespace iot
