#pragma once
#include <stdint.h>
#include "CommandDefs.h"

// Decides which command to send next. Pure logic (time passed in), unit
// tested on the host. Priorities: USER (web/MQTT setters + raw commands)
// beat READBACK (post-ACK refresh queries) beat POLL (periodic queries by
// due time). One command is in flight at a time — the engine calls next()
// only when idle.

namespace iot {

enum class CmdOrigin : uint8_t { Poll, Readback, UserWeb, UserMqtt, FeedGuard };

constexpr uint8_t CMD_WIRE_MAX = 64;  // raw passthrough commands included

struct CmdRequest {
    char     wire[CMD_WIRE_MAX] = {0};      // unframed command string
    uint32_t id = 0;                        // correlation id for results
    CmdOrigin origin = CmdOrigin::Poll;
    bool     isSetter = false;              // expect ACK/NAK, frame with ^S on PI17
    const QueryCommandDef* query = nullptr; // parse response with this table
    const SetCommandDef*   set = nullptr;   // originating setter (for readback/result)
    char     value[24] = {0};               // human value, echoed in results
};

class PollScheduler {
public:
    static constexpr uint8_t USER_QUEUE = 6;
    static constexpr uint8_t READBACK_QUEUE = 6;

    void bindProfile(const DeviceProfile* profile, uint32_t nowMs);

    bool pushUser(const CmdRequest& req);          // false if queue full
    void pushReadback(const char* queryCmd);       // by wire name, dedup'd

    // Returns true and fills `out` when a command should be sent now.
    bool next(uint32_t nowMs, CmdRequest& out);

    // Reschedule the poll timer after a poll query completes (ok or not).
    void onPollDone(const QueryCommandDef* query, uint32_t nowMs);

    // When the link is lost, back off all polling until nowMs + backoff.
    void backoff(uint32_t nowMs, uint32_t backoffMs);

    uint8_t userQueueDepth() const { return _userCount; }

private:
    const DeviceProfile* _profile = nullptr;
    CmdRequest _user[USER_QUEUE];
    uint8_t _userHead = 0, _userCount = 0;
    const QueryCommandDef* _readback[READBACK_QUEUE] = {nullptr};
    uint8_t _rbHead = 0, _rbCount = 0;
    uint32_t _pollDue[32] = {0};   // per-query next due time
    bool _bootDone[32] = {false};  // pollMs==0 queries run once
};

} // namespace iot
