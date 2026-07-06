#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "InverterPort.h"
#include "../drivers/CommandDefs.h"
#include "../drivers/DeviceState.h"
#include "../drivers/PollScheduler.h"
#include "../drivers/SetFormatter.h"

// Owns the serial link to the inverter. Runs as its own FreeRTOS task on
// core 1 so 2400-baud pacing never contends with WiFi (core 0).
//
// Thread contract: submitSet/submitRaw/popResult and the JSON snapshot
// methods are safe from any task; everything else runs inside the engine
// task. Web/MQTT callbacks must only ever call the safe methods.

struct CmdResult {
    uint32_t id = 0;
    iot::CmdOrigin origin = iot::CmdOrigin::Poll;
    char key[40] = {0};      // set-command key ("" for raw)
    char wire[64] = {0};     // what was sent
    char value[24] = {0};    // requested value
    char status[12] = {0};   // "ok" | "nak" | "timeout" | "rejected"
    char response[96] = {0}; // raw payload for raw commands
};

class InverterEngine {
public:
    // Starts the engine task. profile must outlive the engine (profiles are
    // static const data).
    void begin(const iot::DeviceProfile* profile, uint32_t baud,
               int8_t rxPin, int8_t txPin);

    // --- thread-safe API -----------------------------------------------------
    iot::SetError submitSet(const char* key, const char* value,
                            iot::CmdOrigin origin, uint32_t id);
    bool submitRaw(const char* cmd, iot::CmdOrigin origin, uint32_t id);
    bool popResult(CmdResult& out);          // non-blocking

    bool linkUp() const { return _linkUp; }
    uint32_t lastGoodRxMs() const { return _lastGoodRx; }
    uint32_t stateSeq();
    const iot::DeviceProfile* profile() const { return _profile; }

    // Full state as {key: value, ...} plus meta. sinceSeq>0 → only fields
    // changed after that sequence number.
    void stateToJson(JsonDocument& doc, uint32_t sinceSeq = 0);
    // Current value lookup for the control panel (numeric or string).
    void datapointsToJson(JsonDocument& doc);

private:
    static void taskEntry(void* self);
    void run();
    void startCommand(const iot::CmdRequest& req, uint32_t now);
    void finishCommand(bool ok, const char* status, const char* payload, uint16_t len, uint32_t now);
    void handleLine(const char* line, uint16_t len, uint32_t now);
    void pushResult(const CmdResult& r);

    InverterPort _port;
    const iot::DeviceProfile* _profile = nullptr;
    iot::DeviceState _state;
    iot::PollScheduler _sched;

    SemaphoreHandle_t _stateMux = nullptr;
    QueueHandle_t _userQueue = nullptr;    // CmdRequest from other tasks
    QueueHandle_t _resultQueue = nullptr;  // CmdResult to main loop

    iot::CmdRequest _inflight;
    bool _busy = false;
    uint8_t _attempts = 0;
    uint32_t _sentAt = 0;
    uint32_t _lastDoneAt = 0;
    volatile bool _linkUp = false;
    volatile uint32_t _lastGoodRx = 0;
    uint8_t _consecFails = 0;
};
