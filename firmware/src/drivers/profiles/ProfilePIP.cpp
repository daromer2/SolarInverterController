// MPP Solar / Voltronic PIP series (Axpert clones) — PI30 protocol.
// Field layouts follow the proven legacy parser (old_inverterofthings) and
// jblance/mpp-solar pi30.py. Pure data: no code changes needed to extend.

#include "ProfileRegistry.h"

namespace iot {
namespace {

#define FLD(key, label, unit, type, scale, ha) {key, label, unit, FieldType::type, scale, ha, 0}
#define FLG(key, label, code) {key, label, "", FieldType::Bit, 1, nullptr, code}

// --- QPIGS: general status -------------------------------------------------
const FieldDef PIP_QPIGS_F[] = {
    FLD("grid_voltage",             "Grid voltage",              "V",  Float, 1, "voltage"),
    FLD("grid_frequency",           "Grid frequency",            "Hz", Float, 1, "frequency"),
    FLD("ac_output_voltage",        "AC output voltage",         "V",  Float, 1, "voltage"),
    FLD("ac_output_frequency",      "AC output frequency",       "Hz", Float, 1, "frequency"),
    FLD("ac_output_va",             "AC output apparent power",  "VA", Int,   1, "apparent_power"),
    FLD("ac_output_power",          "AC output power",           "W",  Int,   1, "power"),
    FLD("load_percent",             "Output load",               "%",  Int,   1, nullptr),
    FLD("bus_voltage",              "Bus voltage",               "V",  Int,   1, "voltage"),
    FLD("battery_voltage",          "Battery voltage",           "V",  Float, 1, "voltage"),
    FLD("battery_charge_current",   "Battery charge current",    "A",  Int,   1, "current"),
    FLD("battery_capacity",         "Battery capacity",          "%",  Int,   1, "battery"),
    FLD("heatsink_temp",            "Heatsink temperature",      "°C", Int,   1, "temperature"),
    FLD("pv_input_current",         "PV input current",          "A",  Float, 1, "current"),
    FLD("pv_input_voltage",         "PV input voltage",          "V",  Float, 1, "voltage"),
    FLD("battery_voltage_scc",      "Battery voltage (SCC)",     "V",  Float, 1, "voltage"),
    FLD("battery_discharge_current","Battery discharge current", "A",  Int,   1, "current"),
    FLD("device_status",            "Device status bits",        "",   Str,   1, nullptr),
    FLD("battery_voltage_offset",   "Battery voltage offset",    "10mV", Int, 1, nullptr),
    FLD("eeprom_version",           "EEPROM version",            "",   Int,   1, nullptr),
    FLD("pv_charging_power",        "PV charging power",         "W",  Int,   1, "power"),
    FLD("device_status2",           "Device status bits 2",      "",   Str,   1, nullptr),
};

// --- QMOD: working mode ----------------------------------------------------
const FieldDef PIP_QMOD_F[] = {
    FLD("mode", "Working mode", "", Str, 1, nullptr),
};

// --- QPIWS: warning status (32 positional bits, legacy order) ---------------
const FieldDef PIP_QPIWS_F[] = {
    FLD("warn_reserved_0",          "Reserved",                   "", Bit, 1, nullptr),
    FLD("warn_inverter_fault",      "Inverter fault",             "", Bit, 1, nullptr),
    FLD("warn_bus_over",            "Bus over voltage",           "", Bit, 1, nullptr),
    FLD("warn_bus_under",           "Bus under voltage",          "", Bit, 1, nullptr),
    FLD("warn_bus_soft_fail",       "Bus soft fail",              "", Bit, 1, nullptr),
    FLD("warn_line_fail",           "Line fail",                  "", Bit, 1, nullptr),
    FLD("warn_opv_short",           "Output short",               "", Bit, 1, nullptr),
    FLD("warn_over_temperature",    "Over temperature",           "", Bit, 1, nullptr),
    FLD("warn_fan_locked",          "Fan locked",                 "", Bit, 1, nullptr),
    FLD("warn_battery_voltage_high","Battery voltage high",       "", Bit, 1, nullptr),
    FLD("warn_battery_low_alarm",   "Battery low alarm",          "", Bit, 1, nullptr),
    FLD("warn_reserved_11",         "Reserved",                   "", Bit, 1, nullptr),
    FLD("warn_battery_under_shutdown","Battery under shutdown",   "", Bit, 1, nullptr),
    FLD("warn_reserved_13",         "Reserved",                   "", Bit, 1, nullptr),
    FLD("warn_overload",            "Overload",                   "", Bit, 1, nullptr),
    FLD("warn_eeprom_fault",        "EEPROM fault",               "", Bit, 1, nullptr),
    FLD("warn_inverter_over_current","Inverter over current",     "", Bit, 1, nullptr),
    FLD("warn_inverter_soft_fail",  "Inverter soft fail",         "", Bit, 1, nullptr),
    FLD("warn_self_test_fail",      "Self test fail",             "", Bit, 1, nullptr),
    FLD("warn_op_dc_voltage_over",  "Output DC voltage over",     "", Bit, 1, nullptr),
    FLD("warn_battery_open",        "Battery open",               "", Bit, 1, nullptr),
    FLD("warn_current_sensor_fail", "Current sensor fail",        "", Bit, 1, nullptr),
    FLD("warn_battery_short",       "Battery short",              "", Bit, 1, nullptr),
    FLD("warn_power_limit",         "Power limit",                "", Bit, 1, nullptr),
    FLD("warn_pv_voltage_high",     "PV voltage high",            "", Bit, 1, nullptr),
    FLD("warn_mppt_overload_fault", "MPPT overload fault",        "", Bit, 1, nullptr),
    FLD("warn_mppt_overload_warning","MPPT overload warning",     "", Bit, 1, nullptr),
    FLD("warn_battery_too_low_to_charge","Battery too low to charge","",Bit,1, nullptr),
    FLD("warn_reserved_28",         "Reserved",                   "", Bit, 1, nullptr),
    FLD("warn_reserved_29",         "Reserved",                   "", Bit, 1, nullptr),
};

// --- QFLAG: option flags (letter-coded) --------------------------------------
const FieldDef PIP_QFLAG_F[] = {
    FLG("flag_silence_buzzer",        "Silence buzzer",                     'a'),
    FLG("flag_overload_bypass",       "Overload bypass",                    'b'),
    FLG("flag_power_saving",          "Power saving",                       'j'),
    FLG("flag_lcd_escape",            "LCD escape to default page",         'k'),
    FLG("flag_overload_restart",      "Restart on overload",                'u'),
    FLG("flag_overtemp_restart",      "Restart on over temperature",        'v'),
    FLG("flag_backlight",             "LCD backlight",                      'x'),
    FLG("flag_alarm_primary_interrupt","Alarm on primary source interrupt", 'y'),
    FLG("flag_fault_record",          "Fault code recording",               'z'),
};

// --- QPIRI: ratings & current settings ---------------------------------------
const FieldDef PIP_QPIRI_F[] = {
    FLD("grid_rating_voltage",        "Grid rating voltage",        "V",  Float, 1, "voltage"),
    FLD("grid_rating_current",        "Grid rating current",        "A",  Float, 1, "current"),
    FLD("ac_output_rating_voltage",   "AC output rating voltage",   "V",  Float, 1, "voltage"),
    FLD("ac_output_rating_frequency", "AC output rating frequency", "Hz", Float, 1, "frequency"),
    FLD("ac_output_rating_current",   "AC output rating current",   "A",  Float, 1, "current"),
    FLD("ac_output_rating_va",        "AC output rating apparent power", "VA", Int, 1, "apparent_power"),
    FLD("ac_output_rating_power",     "AC output rating power",     "W",  Int,   1, "power"),
    FLD("battery_rating_voltage",     "Battery rating voltage",     "V",  Float, 1, "voltage"),
    FLD("battery_recharge_voltage",   "Battery re-charge voltage",  "V",  Float, 1, "voltage"),
    FLD("battery_under_voltage",      "Battery under voltage",      "V",  Float, 1, "voltage"),
    FLD("battery_bulk_voltage",       "Battery bulk voltage",       "V",  Float, 1, "voltage"),
    FLD("battery_float_voltage",      "Battery float voltage",      "V",  Float, 1, "voltage"),
    FLD("battery_type",               "Battery type",               "",   Int,   1, nullptr),
    FLD("max_ac_charge_current",      "Max AC charge current",      "A",  Int,   1, "current"),
    FLD("max_charge_current",         "Max charge current",         "A",  Int,   1, "current"),
    FLD("input_voltage_range",        "Input voltage range",        "",   Int,   1, nullptr),
    FLD("output_priority",            "Output source priority",     "",   Int,   1, nullptr),
    FLD("charger_priority",           "Charger source priority",    "",   Int,   1, nullptr),
    FLD("parallel_max",               "Parallel max number",        "",   Int,   1, nullptr),
    FLD("machine_type",               "Machine type",               "",   Str,   1, nullptr),
    FLD("topology",                   "Topology",                   "",   Int,   1, nullptr),
    FLD("output_mode",                "Output mode",                "",   Int,   1, nullptr),
    FLD("battery_redischarge_voltage","Battery re-discharge voltage","V", Float, 1, "voltage"),
    FLD("pv_ok_parallel",             "PV OK condition for parallel","",  Int,   1, nullptr),
    FLD("pv_power_balance",           "PV power balance",           "",   Int,   1, nullptr),
};

// --- boot-once identity -------------------------------------------------------
const FieldDef PIP_QID_F[]  = { FLD("serial_number", "Serial number", "", Str, 1, nullptr) };
const FieldDef PIP_QVFW_F[] = { FLD("fw_version", "Firmware version", "", Str, 1, nullptr) };

const QueryCommandDef PIP_QUERIES[] = {
    {"QPIGS", 60, 3000,  ParseKind::Positional, PIP_QPIGS_F, sizeof(PIP_QPIGS_F)/sizeof(FieldDef)},
    {"QMOD",  1,  10000, ParseKind::Positional, PIP_QMOD_F,  sizeof(PIP_QMOD_F)/sizeof(FieldDef)},
    {"QPIWS", 30, 15000, ParseKind::Positional, PIP_QPIWS_F, sizeof(PIP_QPIWS_F)/sizeof(FieldDef)},
    {"QFLAG", 6,  30000, ParseKind::FlagLetters, PIP_QFLAG_F, sizeof(PIP_QFLAG_F)/sizeof(FieldDef)},
    {"QPIRI", 70, 60000, ParseKind::Positional, PIP_QPIRI_F, sizeof(PIP_QPIRI_F)/sizeof(FieldDef)},
    {"QID",   8,  0,     ParseKind::Positional, PIP_QID_F,   1},
    {"QVFW",  5,  0,     ParseKind::Positional, PIP_QVFW_F,  1},
};

// --- set commands ---------------------------------------------------------------
const char* const OUTPUT_PRIO[]  = {"Utility first", "Solar first", "SBU (Solar-Battery-Utility)"};
const char* const CHARGER_PRIO[] = {"Utility first", "Solar first", "Solar + Utility", "Solar only"};
const char* const BATT_TYPE[]    = {"AGM", "Flooded", "User"};
const char* const GRID_RANGE[]   = {"Appliance", "UPS"};
const char* const OUT_FREQ[]     = {"50 Hz", "60 Hz"};
const int16_t     OUT_FREQ_V[]   = {50, 60};
const char* const PVOK[]         = {"As is", "All must be OK"};
const char* const PVBAL[]        = {"PV max current", "PV max power"};

#define SET_NUM(key, label, group, unit, fmt, kind, mn, mx, st, rb) \
    {key, label, group, unit, fmt, ArgKind::kind, mn, mx, st, nullptr, nullptr, 0, rb}
#define SET_ENUM(key, label, group, fmt, labels, values, rb) \
    {key, label, group, "", fmt, ArgKind::Enum, 0, 0, 0, labels, values, (uint8_t)(sizeof(labels)/sizeof(labels[0])), rb}
#define SET_BOOL(key, label, group, fmt, rb) \
    {key, label, group, "", fmt, ArgKind::Bool, 0, 0, 0, nullptr, nullptr, 0, rb}
#define SET_STR(key, label, group, fmt, rb) \
    {key, label, group, "", fmt, ArgKind::String, 0, 0, 0, nullptr, nullptr, 0, rb}

const SetCommandDef PIP_SETS[] = {
    SET_ENUM("output_priority",  "Output source priority",  "Mode", "POP%02d", OUTPUT_PRIO,  nullptr, "QPIRI"),
    SET_ENUM("charger_priority", "Charger source priority", "Mode", "PCP%02d", CHARGER_PRIO, nullptr, "QPIRI"),
    SET_ENUM("grid_range",       "Input voltage range",     "Mode", "PGR%02d", GRID_RANGE,   nullptr, "QPIRI"),
    SET_ENUM("output_frequency", "Output frequency",        "Mode", "F%02d",   OUT_FREQ,     OUT_FREQ_V, "QPIRI"),

    SET_NUM("battery_recharge_voltage",   "Battery re-charge voltage",   "Battery", "V", "PBCV%04.1f", Fixed1, 10, 62, 0.1f, "QPIRI"),
    SET_NUM("battery_redischarge_voltage","Battery re-discharge voltage","Battery", "V", "PBDV%04.1f", Fixed1, 0,  62, 0.1f, "QPIRI"),
    SET_NUM("battery_float_voltage",      "Battery float voltage",       "Battery", "V", "PBFT%04.1f", Fixed1, 10, 62, 0.1f, "QPIRI"),
    SET_NUM("battery_bulk_voltage",       "Battery bulk (C.V.) voltage", "Battery", "V", "PCVV%04.1f", Fixed1, 10, 62, 0.1f, "QPIRI"),
    SET_NUM("battery_cutoff_voltage",     "Battery cut-off voltage",     "Battery", "V", "PSDV%04.1f", Fixed1, 10, 62, 0.1f, "QPIRI"),
    SET_ENUM("battery_type",              "Battery type",                "Battery", "PBT%02d", BATT_TYPE, nullptr, "QPIRI"),
    SET_NUM("max_charge_current",         "Max charge current",          "Battery", "A", "MNCHGC0%03d", Int, 10, 150, 10, "QPIRI"),
    SET_NUM("max_ac_charge_current",      "Max AC charge current",       "Battery", "A", "MUCHGC0%02d", Int, 2,  99,  1,  "QPIRI"),

    SET_ENUM("pv_ok_parallel",   "PV OK condition for parallel", "Solar", "PPVOKC%d", PVOK,  nullptr, "QPIRI"),
    SET_ENUM("pv_power_balance", "PV power balance",             "Solar", "PSPB%d",   PVBAL, nullptr, "QPIRI"),

    SET_BOOL("flag_silence_buzzer",        "Silence buzzer",                    "Flags", "PEa|PDa", "QFLAG"),
    SET_BOOL("flag_overload_bypass",       "Overload bypass",                   "Flags", "PEb|PDb", "QFLAG"),
    SET_BOOL("flag_power_saving",          "Power saving",                      "Flags", "PEj|PDj", "QFLAG"),
    SET_BOOL("flag_lcd_escape",            "LCD escape to default page",        "Flags", "PEk|PDk", "QFLAG"),
    SET_BOOL("flag_overload_restart",      "Restart on overload",               "Flags", "PEu|PDu", "QFLAG"),
    SET_BOOL("flag_overtemp_restart",      "Restart on over temperature",       "Flags", "PEv|PDv", "QFLAG"),
    SET_BOOL("flag_backlight",             "LCD backlight",                     "Flags", "PEx|PDx", "QFLAG"),
    SET_BOOL("flag_alarm_primary_interrupt","Alarm on primary source interrupt","Flags", "PEy|PDy", "QFLAG"),
    SET_BOOL("flag_fault_record",          "Fault code recording",              "Flags", "PEz|PDz", "QFLAG"),
};

const ModeDef PIP_MODES[] = {
    {"P", "Power on"}, {"S", "Standby"}, {"L", "Line"}, {"B", "Battery"},
    {"F", "Fault"}, {"H", "Power saving"}, {"D", "Shutdown"},
};

} // namespace

const DeviceProfile PROFILE_PIP = {
    "PIP", "MPP Solar PIP / Axpert (PI30)",
    ProtocolKind::PI30, 2400, true,
    PIP_QUERIES, sizeof(PIP_QUERIES)/sizeof(QueryCommandDef),
    PIP_SETS,    sizeof(PIP_SETS)/sizeof(SetCommandDef),
    PIP_MODES,   sizeof(PIP_MODES)/sizeof(ModeDef),
    "mode",
    "/svg/pip.svg",
};

} // namespace iot
