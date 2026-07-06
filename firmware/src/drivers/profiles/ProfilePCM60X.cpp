// MPP Solar PCM60X MPPT solar charge controller — PI30-style protocol.
// QPIGS layout follows the proven legacy parser. QPIRI/QPIWS follow the
// PCM60X manual; unknown trailing tokens are simply left unparsed, so a
// firmware variant with fewer fields degrades gracefully.

#include "ProfileRegistry.h"

namespace iot {
namespace {

#define FLD(key, label, unit, type, scale, ha) {key, label, unit, FieldType::type, scale, ha, 0}

const FieldDef PCM_QPIGS_F[] = {
    FLD("pv_input_voltage",       "PV input voltage",        "V", Float, 1, "voltage"),
    FLD("battery_voltage",        "Battery voltage",         "V", Float, 1, "voltage"),
    FLD("battery_charge_current", "Battery charge current",  "A", Float, 1, "current"),
    FLD("pv_input_current",       "PV input current 1",      "A", Float, 1, "current"),
    FLD("pv2_input_current",      "PV input current 2",      "A", Float, 1, "current"),
    FLD("charging_power",         "Charging power",          "W", Int,   1, "power"),
};

const FieldDef PCM_QPIRI_F[] = {
    FLD("max_output_power",       "Max output power",        "W", Int,   1, "power"),
    FLD("nominal_battery_voltage","Nominal battery voltage", "V", Int,   1, "voltage"),
    FLD("nominal_charge_current", "Nominal charge current",  "A", Float, 1, "current"),
    FLD("absorption_voltage",     "Absorption voltage",      "V", Float, 1, "voltage"),
    FLD("float_voltage",          "Float voltage",           "V", Float, 1, "voltage"),
    FLD("battery_type",           "Battery type",            "",  Int,   1, nullptr),
    FLD("remote_batt_voltage_detect", "Remote battery voltage detect", "", Int, 1, nullptr),
    FLD("batt_temp_compensation", "Battery temp compensation", "mV", Float, 1, nullptr),
    FLD("remote_temp_detect",     "Remote temperature detect", "", Int,  1, nullptr),
    FLD("batt_rated_voltage_set", "Battery rated voltage setting", "", Int, 1, nullptr),
    FLD("batt_in_serial",         "Batteries in serial",     "",  Int,   1, nullptr),
    FLD("batt_low_warning_voltage","Battery low warning voltage", "V", Float, 1, "voltage"),
    FLD("batt_low_shutdown_detect","Battery low shutdown detect", "", Int, 1, nullptr),
};

const FieldDef PCM_QPIWS_F[] = {
    FLD("warn_over_charge_current", "Over charge current",     "", Bit, 1, nullptr),
    FLD("warn_over_temperature",    "Over temperature",        "", Bit, 1, nullptr),
    FLD("warn_battery_voltage_under","Battery voltage under",  "", Bit, 1, nullptr),
    FLD("warn_battery_voltage_high","Battery voltage high",    "", Bit, 1, nullptr),
    FLD("warn_pv_high_loss",        "PV voltage high loss",    "", Bit, 1, nullptr),
    FLD("warn_battery_temp_low",    "Battery temperature low", "", Bit, 1, nullptr),
    FLD("warn_battery_temp_high",   "Battery temperature high","", Bit, 1, nullptr),
    FLD("warn_pv_low_loss",         "PV energy low",           "", Bit, 1, nullptr),
    FLD("warn_pv_high_derating",    "PV high derating",        "", Bit, 1, nullptr),
    FLD("warn_temp_high_derating",  "Temperature high derating","", Bit, 1, nullptr),
    FLD("warn_batt_temp_high_derating","Battery temp high derating","",Bit,1, nullptr),
    FLD("warn_batt_low_warning",    "Battery low",             "", Bit, 1, nullptr),
};

const FieldDef PCM_QID_F[]  = { FLD("serial_number", "Serial number", "", Str, 1, nullptr) };
const FieldDef PCM_QVFW_F[] = { FLD("fw_version", "Firmware version", "", Str, 1, nullptr) };

const QueryCommandDef PCM_QUERIES[] = {
    {"QPIGS", 25, 2000,  ParseKind::Positional, PCM_QPIGS_F, sizeof(PCM_QPIGS_F)/sizeof(FieldDef)},
    {"QPIRI", 20, 60000, ParseKind::Positional, PCM_QPIRI_F, sizeof(PCM_QPIRI_F)/sizeof(FieldDef)},
    {"QPIWS", 10, 15000, ParseKind::Positional, PCM_QPIWS_F, sizeof(PCM_QPIWS_F)/sizeof(FieldDef)},
    {"QID",   8,  0,     ParseKind::Positional, PCM_QID_F,   1},
    {"QVFW",  5,  0,     ParseKind::Positional, PCM_QVFW_F,  1},
};

const char* const BATT_TYPE[] = {"AGM", "Flooded", "User"};

const SetCommandDef PCM_SETS[] = {
    {"battery_type", "Battery type", "Battery", "", "PBT%02d", ArgKind::Enum,
     0, 0, 0, BATT_TYPE, nullptr, 3, "QPIRI"},
    {"max_charge_current", "Max charge current", "Battery", "A", "MCHGC%03d", ArgKind::Int,
     10, 60, 10, nullptr, nullptr, 0, "QPIRI"},
    {"absorption_voltage", "Absorption (C.V.) voltage", "Battery", "V", "PCVV%04.1f", ArgKind::Fixed1,
     10, 62, 0.1f, nullptr, nullptr, 0, "QPIRI"},
    {"float_voltage", "Float voltage", "Battery", "V", "PBFT%04.1f", ArgKind::Fixed1,
     10, 62, 0.1f, nullptr, nullptr, 0, "QPIRI"},
};

} // namespace

const DeviceProfile PROFILE_PCM60X = {
    "PCM", "MPP Solar PCM60X MPPT charger (PI30)",
    ProtocolKind::PI30, 2400, true,
    PCM_QUERIES, sizeof(PCM_QUERIES)/sizeof(QueryCommandDef),
    PCM_SETS,    sizeof(PCM_SETS)/sizeof(SetCommandDef),
    nullptr, 0,
    nullptr,
    "/svg/pcm.svg",
};

} // namespace iot
