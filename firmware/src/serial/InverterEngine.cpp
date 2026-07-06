#include "InverterEngine.h"
#include "../drivers/Protocol.h"
#include "../drivers/ResponseParser.h"
#include "../AppConfig.h"
#include "../sys/Logx.h"
#include <string.h>

using namespace iot;

void InverterEngine::begin(const DeviceProfile* profile, uint32_t baud,
                           int8_t rxPin, int8_t txPin)
{
    _profile = profile;
    _stateMux = xSemaphoreCreateMutex();
    _userQueue = xQueueCreate(8, sizeof(CmdRequest));
    _resultQueue = xQueueCreate(8, sizeof(CmdResult));

    _state.bindProfile(profile);
    _sched.bindProfile(profile, millis());
    _port.begin(baud ? baud : profile->baud, rxPin, txPin);

    xTaskCreatePinnedToCore(taskEntry, "inverter", 6144, this, 2, nullptr, 1);
    LOGI("inv", "engine started: profile=%s baud=%lu",
         profile->id, (unsigned long)(baud ? baud : profile->baud));
}

SetError InverterEngine::submitSet(const char* key, const char* value,
                                   CmdOrigin origin, uint32_t id)
{
    if (!_profile) return SetError::UnknownCommand;
    const SetCommandDef* def = nullptr;
    for (uint8_t i = 0; i < _profile->nSets; ++i)
        if (strcmp(_profile->sets[i].key, key) == 0) { def = &_profile->sets[i]; break; }
    if (!def) return SetError::UnknownCommand;

    char wire[SET_WIRE_MAX];
    SetError err = formatSetCommand(*def, value, wire);
    if (err != SetError::Ok) return err;

    CmdRequest req;
    snprintf(req.wire, sizeof(req.wire), "%s", wire);
    snprintf(req.value, sizeof(req.value), "%s", value);
    req.id = id;
    req.origin = origin;
    req.isSetter = true;
    req.set = def;
    if (xQueueSend(_userQueue, &req, 0) != pdTRUE) return SetError::TooLong; // queue full
    return SetError::Ok;
}

bool InverterEngine::submitRaw(const char* cmd, CmdOrigin origin, uint32_t id)
{
    if (!cmd || strlen(cmd) == 0 || strlen(cmd) >= CMD_WIRE_MAX) return false;
    CmdRequest req;
    snprintf(req.wire, sizeof(req.wire), "%s", cmd);
    req.id = id;
    req.origin = origin;
    // Raw commands starting with Q (PI30) or matching a known query parse as
    // data; setter-style raw commands still get their ACK/NAK reported.
    req.isSetter = false;
    return xQueueSend(_userQueue, &req, 0) == pdTRUE;
}

bool InverterEngine::popResult(CmdResult& out)
{
    return _resultQueue && xQueueReceive(_resultQueue, &out, 0) == pdTRUE;
}

void InverterEngine::pushResult(const CmdResult& r)
{
    xQueueSend(_resultQueue, &r, 0);  // drop when full — results are advisory
}

uint32_t InverterEngine::stateSeq()
{
    xSemaphoreTake(_stateMux, portMAX_DELAY);
    uint32_t s = _state.seq();
    xSemaphoreGive(_stateMux);
    return s;
}

void InverterEngine::stateToJson(JsonDocument& doc, uint32_t sinceSeq)
{
    xSemaphoreTake(_stateMux, portMAX_DELAY);
    uint32_t now = millis();
    for (uint8_t i = 0; i < _state.count(); ++i) {
        const Datapoint& p = _state.at(i);
        if (!p.valid) continue;
        if (sinceSeq && p.changeSeq <= sinceSeq) continue;
        if (p.def->type == FieldType::Str)
            doc[p.def->key] = p.str;
        else
            doc[p.def->key] = p.num;
    }
    doc["_seq"] = _state.seq();
    doc["_link"] = _linkUp;
    doc["_age_ms"] = now - _lastGoodRx;
    xSemaphoreGive(_stateMux);
}

void InverterEngine::datapointsToJson(JsonDocument& doc)
{
    xSemaphoreTake(_stateMux, portMAX_DELAY);
    JsonArray arr = doc["datapoints"].to<JsonArray>();
    for (uint8_t i = 0; i < _state.count(); ++i) {
        const Datapoint& p = _state.at(i);
        JsonObject o = arr.add<JsonObject>();
        o["key"] = p.def->key;
        o["label"] = p.def->label;
        o["unit"] = p.def->unit;
        if (p.valid) {
            if (p.def->type == FieldType::Str) o["value"] = p.str;
            else o["value"] = p.num;
        }
    }
    xSemaphoreGive(_stateMux);
}

void InverterEngine::taskEntry(void* self)
{
    static_cast<InverterEngine*>(self)->run();
}

void InverterEngine::run()
{
    for (;;) {
        uint32_t now = millis();

        // RX pump
        const char* line;
        uint16_t len;
        while (_port.service(line, len)) {
            if (_busy) handleLine(line, len, now);
        }

        // timeout / retry
        if (_busy && (now - _sentAt) > INV_TIMEOUT_MS) {
            if (_attempts <= INV_RETRIES) {
                LOGI("inv", "timeout, retry %u: %s", _attempts, _inflight.wire);
                startCommand(_inflight, now);   // resend, attempts increments
            } else {
                LOGI("inv", "timeout, giving up: %s", _inflight.wire);
                finishCommand(false, "timeout", nullptr, 0, now);
            }
        }

        // next command
        if (!_busy && (now - _lastDoneAt) >= INV_GAP_MS) {
            CmdRequest next;
            // external user commands drain into the scheduler's user queue
            CmdRequest fromQueue;
            while (xQueueReceive(_userQueue, &fromQueue, 0) == pdTRUE) {
                if (!_sched.pushUser(fromQueue)) {
                    CmdResult r{};
                    r.id = fromQueue.id;
                    r.origin = fromQueue.origin;
                    snprintf(r.wire, sizeof(r.wire), "%s", fromQueue.wire);
                    snprintf(r.status, sizeof(r.status), "rejected");
                    snprintf(r.response, sizeof(r.response), "queue full");
                    pushResult(r);
                }
            }
            if (_sched.next(now, next)) {
                _attempts = 0;
                startCommand(next, now);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void InverterEngine::startCommand(const CmdRequest& req, uint32_t now)
{
    TxFrame frame;
    if (!frameCommand(_profile->protocol, _profile->txCrc, req.isSetter, req.wire, frame)) {
        _inflight = req;
        finishCommand(false, "rejected", nullptr, 0, now);
        return;
    }
    _inflight = req;
    _busy = true;
    ++_attempts;
    _sentAt = now;
    _port.flushInput();
    _port.send(frame);
}

void InverterEngine::handleLine(const char* line, uint16_t len, uint32_t now)
{
    RxView v;
    RespStatus st = parseResponse(_profile->protocol, line, len, v);

    switch (st) {
    case RespStatus::Ack:
        finishCommand(true, "ok", nullptr, 0, now);
        break;
    case RespStatus::Nak:
        finishCommand(false, "nak", nullptr, 0, now);
        break;
    case RespStatus::Data: {
        // find the query table: explicit (poll/readback) or matching raw query
        const QueryCommandDef* q = _inflight.query;
        if (!q) {
            for (uint8_t i = 0; i < _profile->nQueries; ++i) {
                if (strcmp(_profile->queries[i].cmd, _inflight.wire) == 0) {
                    q = &_profile->queries[i];
                    break;
                }
            }
        }
        if (q) {
            xSemaphoreTake(_stateMux, portMAX_DELAY);
            parseFields(*q, v.payload, v.len, _state, now);
            xSemaphoreGive(_stateMux);
        }
        finishCommand(true, "ok", v.payload, v.len, now);
        break;
    }
    case RespStatus::BadCrc:
    case RespStatus::TooShort:
    case RespStatus::Malformed:
        LOGI("inv", "bad response (%d) for %s", (int)st, _inflight.wire);
        // wait for timeout/retry rather than failing instantly: some devices
        // echo garbage before the real response
        break;
    }
}

void InverterEngine::finishCommand(bool ok, const char* status,
                                   const char* payload, uint16_t len, uint32_t now)
{
    if (ok) {
        _lastGoodRx = now;
        _consecFails = 0;
        _linkUp = true;
    } else if (strcmp(status, "timeout") == 0) {
        if (++_consecFails >= INV_LINK_LOST_AFTER) {
            if (_linkUp) LOGI("inv", "link lost");
            _linkUp = false;
            _sched.backoff(now, INV_BACKOFF_MS);
        }
    }

    if (_inflight.origin == CmdOrigin::Poll || _inflight.origin == CmdOrigin::Readback) {
        if (_inflight.query) _sched.onPollDone(_inflight.query, now);
    } else {
        // user/feed-guard command → report the outcome
        CmdResult r{};
        r.id = _inflight.id;
        r.origin = _inflight.origin;
        if (_inflight.set) snprintf(r.key, sizeof(r.key), "%s", _inflight.set->key);
        snprintf(r.wire, sizeof(r.wire), "%s", _inflight.wire);
        snprintf(r.value, sizeof(r.value), "%s", _inflight.value);
        snprintf(r.status, sizeof(r.status), "%s", status);
        if (payload && len)
            snprintf(r.response, sizeof(r.response), "%.*s", (int)len, payload);
        pushResult(r);

        // successful setter → schedule its readback query
        if (ok && _inflight.set && _inflight.set->readback)
            _sched.pushReadback(_inflight.set->readback);
    }

    _busy = false;
    _lastDoneAt = now;
}
