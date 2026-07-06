#include <unity.h>
#include <cstring>
#include <cstdio>
#include "drivers/ResponseParser.h"
#include "drivers/SetFormatter.h"
#include "drivers/PollScheduler.h"
#include "drivers/profiles/ProfileRegistry.h"

using namespace iot;

static DeviceState state;

static const QueryCommandDef* query(const DeviceProfile& p, const char* cmd)
{
    for (uint8_t i = 0; i < p.nQueries; ++i)
        if (strcmp(p.queries[i].cmd, cmd) == 0) return &p.queries[i];
    return nullptr;
}

static const SetCommandDef* setter(const DeviceProfile& p, const char* key)
{
    for (uint8_t i = 0; i < p.nSets; ++i)
        if (strcmp(p.sets[i].key, key) == 0) return &p.sets[i];
    return nullptr;
}

// ---- PI17 GS: real captured payload (jblance test_responses, deframed) -------
void test_mpi_gs_parse()
{
    state.bindProfile(&PROFILE_MPI10K);
    const char* payload = "3462,3468,0040,0035,0503,071,+00000,2369,2367,2350,5000,"
                          "0000,0000,0000,2371,2365,2352,5001,,,,025,028,000,0";
    const QueryCommandDef* q = query(PROFILE_MPI10K, "GS");
    TEST_ASSERT_NOT_NULL(q);
    uint8_t n = parseFields(*q, payload, strlen(payload), state, 1000);
    TEST_ASSERT_GREATER_THAN(15, n);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 346.2, state.find("pv1_voltage")->num);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 0.40,  state.find("pv1_current")->num);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 50.3,  state.find("battery_voltage")->num);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 71,    state.find("battery_capacity")->num);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 236.9, state.find("grid_voltage_r")->num);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 50.0,  state.find("grid_frequency")->num);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 50.01, state.find("ac_output_frequency")->num);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 25,    state.find("inner_temp")->num);
    // empty tokens (,,,) leave AC output currents invalid — not zeroed
    TEST_ASSERT_FALSE(state.find("ac_output_current_r")->valid);
}

// ---- PI17 PS: legacy captured payload ----------------------------------------
void test_mpi_ps_parse()
{
    state.bindProfile(&PROFILE_MPI10K);
    const char* payload = "00139,00122,,,,,,0502,0973,0385,01860,0593,1040,0399,02032,031,1,1,1,1,2,1";
    const QueryCommandDef* q = query(PROFILE_MPI10K, "PS");
    TEST_ASSERT_NOT_NULL(q);
    parseFields(*q, payload, strlen(payload), state, 1000);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 13.9, state.find("pv1_power")->num);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 12.2, state.find("pv2_power")->num);
    TEST_ASSERT_FALSE(state.find("battery_power")->valid);   // empty token
    TEST_ASSERT_FLOAT_WITHIN(0.01, 50.2, state.find("ac_output_power_r")->num);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 186.0, state.find("ac_output_power_total")->num);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 31, state.find("ac_output_percent")->num);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 1, state.find("ac_output_connected")->num);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 2, state.find("dcac_direction")->num);
}

// ---- PI30 QPIGS (PIP variant) --------------------------------------------------
void test_pip_qpigs_parse()
{
    state.bindProfile(&PROFILE_PIP);
    const char* payload = "230.1 49.9 230.1 49.9 0435 0300 003 460 57.50 012 100 0069 "
                          "0014 103.8 57.45 00000 00110110 00 00 00856 010";
    const QueryCommandDef* q = query(PROFILE_PIP, "QPIGS");
    TEST_ASSERT_NOT_NULL(q);
    uint8_t n = parseFields(*q, payload, strlen(payload), state, 1000);
    TEST_ASSERT_EQUAL_UINT8(21, n);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 230.1, state.find("grid_voltage")->num);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 49.9,  state.find("grid_frequency")->num);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 300,   state.find("ac_output_power")->num);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 57.50, state.find("battery_voltage")->num);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 100,   state.find("battery_capacity")->num);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 103.8, state.find("pv_input_voltage")->num);
    TEST_ASSERT_EQUAL_STRING("00110110", state.find("device_status")->str);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 856, state.find("pv_charging_power")->num);
}

// ---- PI30 QMOD / QFLAG ----------------------------------------------------------
void test_pip_qmod_and_qflag()
{
    state.bindProfile(&PROFILE_PIP);
    const QueryCommandDef* qm = query(PROFILE_PIP, "QMOD");
    parseFields(*qm, "B", 1, state, 5);
    TEST_ASSERT_EQUAL_STRING("B", state.find("mode")->str);

    const QueryCommandDef* qf = query(PROFILE_PIP, "QFLAG");
    parseFields(*qf, "EakxyDbjuvz", 11, state, 6);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 1, state.find("flag_silence_buzzer")->num);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 1, state.find("flag_lcd_escape")->num);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 0, state.find("flag_overload_bypass")->num);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 0, state.find("flag_fault_record")->num);
}

// ---- PCM60X QPIGS (legacy field layout) -----------------------------------------
void test_pcm_qpigs_parse()
{
    state.bindProfile(&PROFILE_PCM60X);
    const char* payload = "087.6 51.18 08.81 04.71 04.10 0450";
    const QueryCommandDef* q = query(PROFILE_PCM60X, "QPIGS");
    parseFields(*q, payload, strlen(payload), state, 1000);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 87.6,  state.find("pv_input_voltage")->num);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 51.18, state.find("battery_voltage")->num);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 8.81,  state.find("battery_charge_current")->num);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 450,   state.find("charging_power")->num);
}

// ---- Set command formatting -------------------------------------------------------
void test_set_formatting()
{
    char out[SET_WIRE_MAX];

    // Enum by label and by index
    const SetCommandDef* pop = setter(PROFILE_PIP, "output_priority");
    TEST_ASSERT_EQUAL(int(SetError::Ok), int(formatSetCommand(*pop, "SBU (Solar-Battery-Utility)", out)));
    TEST_ASSERT_EQUAL_STRING("POP02", out);
    TEST_ASSERT_EQUAL(int(SetError::Ok), int(formatSetCommand(*pop, "0", out)));
    TEST_ASSERT_EQUAL_STRING("POP00", out);
    TEST_ASSERT_EQUAL(int(SetError::BadEnum), int(formatSetCommand(*pop, "bogus", out)));

    // Enum with wire values (frequency)
    const SetCommandDef* f = setter(PROFILE_PIP, "output_frequency");
    TEST_ASSERT_EQUAL(int(SetError::Ok), int(formatSetCommand(*f, "50 Hz", out)));
    TEST_ASSERT_EQUAL_STRING("F50", out);

    // Fixed1 with leading zero
    const SetCommandDef* v = setter(PROFILE_PIP, "battery_bulk_voltage");
    TEST_ASSERT_EQUAL(int(SetError::Ok), int(formatSetCommand(*v, "56.4", out)));
    TEST_ASSERT_EQUAL_STRING("PCVV56.4", out);
    TEST_ASSERT_EQUAL(int(SetError::OutOfRange), int(formatSetCommand(*v, "99.9", out)));

    // Int with range + step validation
    const SetCommandDef* c = setter(PROFILE_PIP, "max_charge_current");
    TEST_ASSERT_EQUAL(int(SetError::Ok), int(formatSetCommand(*c, "60", out)));
    TEST_ASSERT_EQUAL_STRING("MNCHGC0060", out);
    TEST_ASSERT_EQUAL(int(SetError::OutOfRange), int(formatSetCommand(*c, "65", out)));  // step 10

    // Bool
    const SetCommandDef* b = setter(PROFILE_PIP, "flag_silence_buzzer");
    TEST_ASSERT_EQUAL(int(SetError::Ok), int(formatSetCommand(*b, "on", out)));
    TEST_ASSERT_EQUAL_STRING("PEa", out);
    TEST_ASSERT_EQUAL(int(SetError::Ok), int(formatSetCommand(*b, "0", out)));
    TEST_ASSERT_EQUAL_STRING("PDa", out);

    // MPI feed power (the SDM630 fast path)
    const SetCommandDef* g = setter(PROFILE_MPI10K, "feed_grid_max_power");
    TEST_ASSERT_EQUAL(int(SetError::Ok), int(formatSetCommand(*g, "4500", out)));
    TEST_ASSERT_EQUAL_STRING("GPMP004500", out);

    // MPI Deci (0.1A units)
    const SetCommandDef* mc = setter(PROFILE_MPI10K, "max_charge_current");
    TEST_ASSERT_EQUAL(int(SetError::Ok), int(formatSetCommand(*mc, "120", out)));
    TEST_ASSERT_EQUAL_STRING("MCHGC1200", out);
}

// ---- Scheduler priorities and timing -----------------------------------------------
void test_scheduler_priorities()
{
    PollScheduler s;
    s.bindProfile(&PROFILE_PIP, 0);

    CmdRequest out;
    // all polls due at t=0: first query wins (QPIGS)
    TEST_ASSERT_TRUE(s.next(0, out));
    TEST_ASSERT_EQUAL_STRING("QPIGS", out.wire);
    s.onPollDone(out.query, 0);

    // user command preempts remaining due polls
    CmdRequest user;
    snprintf(user.wire, sizeof(user.wire), "POP02");
    user.isSetter = true;
    user.origin = CmdOrigin::UserMqtt;
    TEST_ASSERT_TRUE(s.pushUser(user));
    TEST_ASSERT_TRUE(s.next(1, out));
    TEST_ASSERT_EQUAL_STRING("POP02", out.wire);
    TEST_ASSERT_TRUE(out.isSetter);

    // readback beats polls
    s.pushReadback("QPIRI");
    TEST_ASSERT_TRUE(s.next(2, out));
    TEST_ASSERT_EQUAL_STRING("QPIRI", out.wire);
    TEST_ASSERT_EQUAL(int(CmdOrigin::Readback), int(out.origin));

    // boot-only queries run once
    s.bindProfile(&PROFILE_PIP, 1000);
    int qidSeen = 0;
    for (int i = 0; i < 20; ++i) {
        if (!s.next(1000 + i, out)) break;
        if (strcmp(out.wire, "QID") == 0) ++qidSeen;
        s.onPollDone(out.query, 1000 + i);
    }
    TEST_ASSERT_EQUAL_INT(1, qidSeen);
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_mpi_gs_parse);
    RUN_TEST(test_mpi_ps_parse);
    RUN_TEST(test_pip_qpigs_parse);
    RUN_TEST(test_pip_qmod_and_qflag);
    RUN_TEST(test_pcm_qpigs_parse);
    RUN_TEST(test_set_formatting);
    RUN_TEST(test_scheduler_priorities);
    return UNITY_END();
}
