// MPP Solar MPI Hybrid 10KW (3-phase) — PI17 protocol.
// Query layouts follow jblance/mpp-solar pi17.py cross-checked against the
// proven legacy parser. NOTE: power values (PS) use the legacy /10 scaling
// which was validated on real hardware; verify against your firmware
// revision if numbers look 10x off. PI17 commands are sent WITHOUT a CRC
// (txCrc=false) — this matches both the legacy code and jblance.

#include "ProfileRegistry.h"

namespace iot {
namespace {

#define FLD(key, label, unit, type, scale, ha) {key, label, unit, FieldType::type, scale, ha, 0}

// --- GS: general status ------------------------------------------------------
const FieldDef MPI_GS_F[] = {
    FLD("pv1_voltage",        "PV1 voltage",          "V",  Float, 10,  "voltage"),
    FLD("pv2_voltage",        "PV2 voltage",          "V",  Float, 10,  "voltage"),
    FLD("pv1_current",        "PV1 current",          "A",  Float, 100, "current"),
    FLD("pv2_current",        "PV2 current",          "A",  Float, 100, "current"),
    FLD("battery_voltage",    "Battery voltage",      "V",  Float, 10,  "voltage"),
    FLD("battery_capacity",   "Battery capacity",     "%",  Int,   1,   "battery"),
    FLD("battery_current",    "Battery current",      "A",  Float, 10,  "current"),
    FLD("grid_voltage_r",     "Grid voltage R",       "V",  Float, 10,  "voltage"),
    FLD("grid_voltage_s",     "Grid voltage S",       "V",  Float, 10,  "voltage"),
    FLD("grid_voltage_t",     "Grid voltage T",       "V",  Float, 10,  "voltage"),
    FLD("grid_frequency",     "Grid frequency",       "Hz", Float, 100, "frequency"),
    FLD("grid_current_r",     "Grid current R",       "A",  Float, 10,  "current"),
    FLD("grid_current_s",     "Grid current S",       "A",  Float, 10,  "current"),
    FLD("grid_current_t",     "Grid current T",       "A",  Float, 10,  "current"),
    FLD("ac_output_voltage_r","AC output voltage R",  "V",  Float, 10,  "voltage"),
    FLD("ac_output_voltage_s","AC output voltage S",  "V",  Float, 10,  "voltage"),
    FLD("ac_output_voltage_t","AC output voltage T",  "V",  Float, 10,  "voltage"),
    FLD("ac_output_frequency","AC output frequency",  "Hz", Float, 100, "frequency"),
    FLD("ac_output_current_r","AC output current R",  "A",  Float, 10,  "current"),
    FLD("ac_output_current_s","AC output current S",  "A",  Float, 10,  "current"),
    FLD("ac_output_current_t","AC output current T",  "A",  Float, 10,  "current"),
    FLD("inner_temp",         "Inner temperature",    "°C", Int,   1,   "temperature"),
    FLD("component_temp",     "Component max temperature", "°C", Int, 1, "temperature"),
    FLD("external_battery_temp","External battery temperature", "°C", Int, 1, "temperature"),
    FLD("setting_changed",    "Settings changed",     "",   Bit,   1,   nullptr),
};

// --- PS: power status --------------------------------------------------------
// Powers/VA are reported in whole watts (scale 1). The legacy firmware
// divided these by 10, which read 10x low on real hardware — do not restore.
const FieldDef MPI_PS_F[] = {
    FLD("pv1_power",             "PV1 power",                 "W",  Float, 1, "power"),
    FLD("pv2_power",             "PV2 power",                 "W",  Float, 1, "power"),
    FLD("battery_power",         "Battery power",             "W",  Float, 1, "power"),
    FLD("grid_in_power_r",       "AC input power R",          "W",  Float, 1, "power"),
    FLD("grid_in_power_s",       "AC input power S",          "W",  Float, 1, "power"),
    FLD("grid_in_power_t",       "AC input power T",          "W",  Float, 1, "power"),
    FLD("grid_in_power_total",   "AC input power total",      "W",  Float, 1, "power"),
    FLD("ac_output_power_r",     "AC output power R",         "W",  Float, 1, "power"),
    FLD("ac_output_power_s",     "AC output power S",         "W",  Float, 1, "power"),
    FLD("ac_output_power_t",     "AC output power T",         "W",  Float, 1, "power"),
    FLD("ac_output_power_total", "AC output power total",     "W",  Float, 1, "power"),
    FLD("ac_output_va_r",        "AC output apparent power R","VA", Float, 1, "apparent_power"),
    FLD("ac_output_va_s",        "AC output apparent power S","VA", Float, 1, "apparent_power"),
    FLD("ac_output_va_t",        "AC output apparent power T","VA", Float, 1, "apparent_power"),
    FLD("ac_output_va_total",    "AC output apparent power total","VA",Float,1,"apparent_power"),
    FLD("ac_output_percent",     "AC output load",            "%",  Int,   1,  nullptr),
    FLD("ac_output_connected",   "AC output connected",       "",   Bit,   1,  nullptr),
    FLD("pv1_working",           "PV1 working",               "",   Bit,   1,  nullptr),
    FLD("pv2_working",           "PV2 working",               "",   Bit,   1,  nullptr),
    FLD("battery_direction",     "Battery direction",         "",   Int,   1,  nullptr), // 0 idle 1 charge 2 discharge
    FLD("dcac_direction",        "DC/AC direction",           "",   Int,   1,  nullptr), // 0 idle 1 AC->DC 2 DC->AC
    FLD("line_direction",        "Line direction",            "",   Int,   1,  nullptr), // 0 idle 1 input 2 output
};

// --- MOD / WS / FLAG ----------------------------------------------------------
const FieldDef MPI_MOD_F[] = { FLD("mode", "Working mode", "", Str, 1, nullptr) };

const FieldDef MPI_WS_F[] = {
    FLD("warn_solar1_loss",          "Solar input 1 loss",          "", Bit, 1, nullptr),
    FLD("warn_solar2_loss",          "Solar input 2 loss",          "", Bit, 1, nullptr),
    FLD("warn_solar1_voltage_high",  "Solar input 1 voltage high",  "", Bit, 1, nullptr),
    FLD("warn_solar2_voltage_high",  "Solar input 2 voltage high",  "", Bit, 1, nullptr),
    FLD("warn_battery_under_voltage","Battery under voltage",       "", Bit, 1, nullptr),
    FLD("warn_battery_low_voltage",  "Battery low voltage",         "", Bit, 1, nullptr),
    FLD("warn_battery_over_voltage", "Battery over voltage",        "", Bit, 1, nullptr),
    FLD("warn_battery_low_hybrid",   "Battery low in hybrid mode",  "", Bit, 1, nullptr),
    FLD("warn_grid_voltage_high",    "Grid voltage high loss",      "", Bit, 1, nullptr),
    FLD("warn_grid_voltage_low",     "Grid voltage low loss",       "", Bit, 1, nullptr),
    FLD("warn_grid_freq_high",       "Grid frequency high loss",    "", Bit, 1, nullptr),
    FLD("warn_grid_freq_low",        "Grid frequency low loss",     "", Bit, 1, nullptr),
    FLD("warn_ac_avg_voltage_over",  "AC input avg voltage over",   "", Bit, 1, nullptr),
    FLD("warn_ac_input_voltage_loss","AC input voltage loss",       "", Bit, 1, nullptr),
    FLD("warn_ac_input_freq_loss",   "AC input frequency loss",     "", Bit, 1, nullptr),
    FLD("warn_over_temperature",     "Over temperature",            "", Bit, 1, nullptr),
    FLD("warn_overload",             "Overload",                    "", Bit, 1, nullptr),
    FLD("warn_ac_wave_loss",         "AC input wave loss",          "", Bit, 1, nullptr),
};

const FieldDef MPI_FLAG_F[] = {
    FLD("flag_mute_buzzer",          "Mute buzzer",                    "", Int, 1, nullptr),
    FLD("flag_mute_buzzer_standby",  "Mute buzzer in standby",         "", Int, 1, nullptr),
    FLD("flag_mute_buzzer_battery",  "Mute buzzer on battery",         "", Int, 1, nullptr),
    FLD("flag_generator_as_ac",      "Generator as AC input",          "", Int, 1, nullptr),
    FLD("flag_wide_ac_range",        "Wide AC input range",            "", Int, 1, nullptr),
    FLD("flag_ng_relay",             "N/G relay function",             "", Int, 1, nullptr),
};

// --- HECS: energy distribution enables ----------------------------------------
const FieldDef MPI_HECS_F[] = {
    FLD("solar_energy_priority", "Solar energy priority",  "", Str, 1, nullptr), // 00 BLG / 01 LBG / 02 LGB
    FLD("en_solar_charge",       "Solar charges battery",  "", Int, 1, nullptr),
    FLD("en_ac_charge",          "AC charges battery",     "", Int, 1, nullptr),
    FLD("en_feed_to_grid",       "Feed power to grid",     "", Int, 1, nullptr),
    FLD("en_batt_to_loads_solar_normal", "Battery discharge to loads (solar normal)", "", Int, 1, nullptr),
    FLD("en_batt_to_loads_solar_loss",   "Battery discharge to loads (solar loss)",   "", Int, 1, nullptr),
    FLD("en_batt_to_grid_solar_normal",  "Battery feeds grid (solar normal)",         "", Int, 1, nullptr),
    FLD("en_batt_to_grid_solar_loss",    "Battery feeds grid (solar loss)",           "", Int, 1, nullptr),
};

// --- BATS: battery settings (readback for the Battery group setters) -----------
const FieldDef MPI_BATS_F[] = {
    FLD("bats_max_charge_current",   "Battery max charge current",     "A", Float, 10, "current"),
    FLD("bats_cv_voltage",           "Battery C.V. voltage",           "V", Float, 10, "voltage"),
    FLD("bats_float_voltage",        "Battery float voltage",          "V", Float, 10, "voltage"),
    FLD("bats_stop_charge_current",  "Float stop-charge current",      "A", Float, 10, "current"),
    FLD("bats_keep_charged_minutes", "Keep charged time",              "min", Int, 1, nullptr),
    FLD("bats_recover_voltage",      "Recover-to-charge voltage",      "V", Float, 10, "voltage"),
    FLD("bats_under_voltage",        "Battery under voltage",          "V", Float, 10, "voltage"),
    FLD("bats_under_back_voltage",   "Battery under back voltage",     "V", Float, 10, "voltage"),
    FLD("bats_weak_voltage",         "Battery weak voltage (hybrid)",  "V", Float, 10, "voltage"),
    FLD("bats_weak_back_voltage",    "Battery weak back voltage",      "V", Float, 10, "voltage"),
    FLD("bats_battery_type",         "Battery type",                   "",  Int,   1,  nullptr), // 0 ordinary 1 li-fe
    FLD("bats_reserved_1",           "Reserved",                       "",  Str,   1,  nullptr),
    FLD("bats_reserved_2",           "Reserved",                       "",  Str,   1,  nullptr),
    FLD("bats_ac_keep_enabled",      "AC charger keep battery voltage","",  Int,   1,  nullptr),
    FLD("bats_ac_keep_voltage",      "AC keep battery voltage",        "V", Float, 10, "voltage"),
    FLD("bats_temp_compensation",    "Temperature compensation",       "mV",Float, 10, nullptr),
    FLD("bats_max_ac_charge_current","Max AC charge current",          "A", Float, 10, "current"),
    FLD("bats_discharge_max_current","Discharge max current (hybrid)", "A", Int,   1,  "current"),
};

// --- FPADJ: feed-to-grid calibration (legacy P006FPADJ, verified on hardware) --
const FieldDef MPI_FPADJ_F[] = {
    FLD("feed_direction",      "Feed power direction",    "",  Int, 1, nullptr),
    FLD("feed_power_watt",     "Feed power adjust",       "W", Int, 1, "power"),
    FLD("feed_dir_r",          "Feed direction R",        "",  Int, 1, nullptr),
    FLD("feed_calibration_r",  "Feed calibration R",      "W", Int, 1, "power"),
    FLD("feed_dir_s",          "Feed direction S",        "",  Int, 1, nullptr),
    FLD("feed_calibration_s",  "Feed calibration S",      "W", Int, 1, "power"),
    FLD("feed_dir_t",          "Feed direction T",        "",  Int, 1, nullptr),
    FLD("feed_calibration_t",  "Feed calibration T",      "W", Int, 1, "power"),
};

const FieldDef MPI_GPMP_F[] = { FLD("feed_grid_max_power", "Max feed-to-grid power", "W", Int, 1, "power") };
const FieldDef MPI_ET_F[]   = { FLD("total_generated_energy", "Total generated energy", "Wh", Int, 1, "energy") };
const FieldDef MPI_ID_F[]   = { FLD("serial_number", "Serial number", "", Str, 1, nullptr) };
const FieldDef MPI_VFW_F[]  = { FLD("cpu_version", "CPU version", "", Str, 1, nullptr) };

const QueryCommandDef MPI_QUERIES[] = {
    {"GS",    90, 3000,  ParseKind::Positional, MPI_GS_F,    sizeof(MPI_GS_F)/sizeof(FieldDef)},
    {"PS",    60, 3000,  ParseKind::Positional, MPI_PS_F,    sizeof(MPI_PS_F)/sizeof(FieldDef)},
    {"MOD",   2,  10000, ParseKind::Positional, MPI_MOD_F,   1},
    {"WS",    20, 15000, ParseKind::Positional, MPI_WS_F,    sizeof(MPI_WS_F)/sizeof(FieldDef)},
    {"FLAG",  8,  30000, ParseKind::Positional, MPI_FLAG_F,  sizeof(MPI_FLAG_F)/sizeof(FieldDef)},
    {"HECS",  10, 15000, ParseKind::Positional, MPI_HECS_F,  sizeof(MPI_HECS_F)/sizeof(FieldDef)},
    {"BATS",  40, 60000, ParseKind::Positional, MPI_BATS_F,  sizeof(MPI_BATS_F)/sizeof(FieldDef)},
    {"FPADJ", 20, 10000, ParseKind::Positional, MPI_FPADJ_F, sizeof(MPI_FPADJ_F)/sizeof(FieldDef)},
    {"GPMP",  3,  30000, ParseKind::Positional, MPI_GPMP_F,  1},
    {"ET",    5,  60000, ParseKind::Positional, MPI_ET_F,    1},
    {"ID",    10, 0,     ParseKind::Positional, MPI_ID_F,    1},
    {"VFW",   5,  0,     ParseKind::Positional, MPI_VFW_F,   1},
};

const char* const SEP_PRIO[]  = {"Battery-Load-Grid", "Load-Battery-Grid", "Load-Grid-Battery"};
const char* const MPI_BATT[]  = {"Ordinary", "Li-Fe"};
const char* const LST_OPTS[]  = {"Always on", "30 s", "60 s", "5 min", "10 min"};
const int16_t     LST_VALS[]  = {0, 1, 2, 10, 20};

const SetCommandDef MPI_SETS[] = {
    // Grid / energy routing — the SDM630 controller drives feed_grid_max_power
    {"feed_grid_max_power", "Max feed-to-grid power", "Grid", "W", "GPMP0%05d", ArgKind::Int,
     0, 15000, 1, nullptr, nullptr, 0, "GPMP"},
    {"feed_to_grid", "Feed power to grid", "Grid", "", "EDC1|EDC0", ArgKind::Bool,
     0, 0, 0, nullptr, nullptr, 0, "HECS"},
    {"batt_to_grid_solar_normal", "Battery feeds grid (solar normal)", "Grid", "", "EDF1|EDF0", ArgKind::Bool,
     0, 0, 0, nullptr, nullptr, 0, "HECS"},
    {"batt_to_grid_solar_loss", "Battery feeds grid (solar loss)", "Grid", "", "EDG1|EDG0", ArgKind::Bool,
     0, 0, 0, nullptr, nullptr, 0, "HECS"},
    {"feed_power_adjust", "Feed power adjust (advanced, raw args)", "Grid", "", "FPADJ%s", ArgKind::String,
     0, 0, 0, nullptr, nullptr, 0, "FPADJ"},

    // Mode / energy distribution
    {"load_output", "Supply power to loads", "Mode", "", "LON1|LON0", ArgKind::Bool,
     0, 0, 0, nullptr, nullptr, 0, "MOD"},
    {"solar_energy_priority", "Solar energy priority", "Mode", "", "SEP0%d", ArgKind::Enum,
     0, 0, 0, SEP_PRIO, nullptr, 3, "HECS"},
    {"solar_charge", "Solar charges battery", "Mode", "", "EDA1|EDA0", ArgKind::Bool,
     0, 0, 0, nullptr, nullptr, 0, "HECS"},
    {"ac_charge", "AC charges battery", "Mode", "", "EDB1|EDB0", ArgKind::Bool,
     0, 0, 0, nullptr, nullptr, 0, "HECS"},
    {"batt_to_loads_solar_normal", "Battery discharge to loads (solar normal)", "Mode", "", "EDD1|EDD0", ArgKind::Bool,
     0, 0, 0, nullptr, nullptr, 0, "HECS"},
    {"batt_to_loads_solar_loss", "Battery discharge to loads (solar loss)", "Mode", "", "EDE1|EDE0", ArgKind::Bool,
     0, 0, 0, nullptr, nullptr, 0, "HECS"},

    // Battery
    {"battery_type", "Battery type", "Battery", "", "BT%d", ArgKind::Enum,
     0, 0, 0, MPI_BATT, nullptr, 2, "BATS"},
    {"max_charge_current", "Battery max charge current", "Battery", "A", "MCHGC%04d", ArgKind::Deci,
     0, 200, 0.1f, nullptr, nullptr, 0, "BATS"},
    {"max_ac_charge_current", "Max AC charge current", "Battery", "A", "MUCHGC%04d", ArgKind::Deci,
     0, 200, 0.1f, nullptr, nullptr, 0, "BATS"},
    {"charge_voltages", "Charge voltages (CV,float in 0.1V: 0576,0566)", "Battery", "", "MCHGV%s", ArgKind::String,
     0, 0, 0, nullptr, nullptr, 0, "BATS"},
    {"battery_discharge_max_current", "Discharge max current (hybrid)", "Battery", "A", "BDCM%04d", ArgKind::Int,
     0, 999, 1, nullptr, nullptr, 0, "BATS"},
    {"ac_charge_time", "AC charge time window (HHMM-HHMM)", "Battery", "", "ACCT%s", ArgKind::String,
     0, 0, 0, nullptr, nullptr, 0, nullptr},

    // Flags
    {"mute_buzzer", "Mute buzzer", "Flags", "", "PA1|PA0", ArgKind::Bool,
     0, 0, 0, nullptr, nullptr, 0, "FLAG"},
    {"mute_buzzer_standby", "Mute buzzer in standby", "Flags", "", "PB1|PB0", ArgKind::Bool,
     0, 0, 0, nullptr, nullptr, 0, "FLAG"},
    {"mute_buzzer_battery", "Mute buzzer on battery", "Flags", "", "PC1|PC0", ArgKind::Bool,
     0, 0, 0, nullptr, nullptr, 0, "FLAG"},
    {"generator_as_ac", "Generator as AC input", "Flags", "", "PD1|PD0", ArgKind::Bool,
     0, 0, 0, nullptr, nullptr, 0, "FLAG"},
    {"wide_ac_range", "Wide AC input range", "Flags", "", "PE1|PE0", ArgKind::Bool,
     0, 0, 0, nullptr, nullptr, 0, "FLAG"},
    {"ng_relay", "N/G relay function", "Flags", "", "PF1|PF0", ArgKind::Bool,
     0, 0, 0, nullptr, nullptr, 0, "FLAG"},

    // System
    {"lcd_sleep", "LCD sleep time", "System", "", "LST%02d", ArgKind::Enum,
     0, 0, 0, LST_OPTS, LST_VALS, 5, nullptr},
    {"date_time", "Set date/time (YYMMDDHHMMSS)", "System", "", "DAT%s", ArgKind::String,
     0, 0, 0, nullptr, nullptr, 0, nullptr},
};

const ModeDef MPI_MODES[] = {
    {"00", "Power on"}, {"01", "Standby"}, {"02", "Bypass"}, {"03", "Battery"},
    {"04", "Fault"}, {"05", "Hybrid (Grid)"}, {"06", "Charge"},
};

} // namespace

const DeviceProfile PROFILE_MPI10K = {
    "MPI", "MPP Solar MPI Hybrid 10KW (PI17)",
    ProtocolKind::PI17, 2400, false,
    MPI_QUERIES, sizeof(MPI_QUERIES)/sizeof(QueryCommandDef),
    MPI_SETS,    sizeof(MPI_SETS)/sizeof(SetCommandDef),
    MPI_MODES,   sizeof(MPI_MODES)/sizeof(ModeDef),
    "mode",
    "/svg/mpi.svg",
};

} // namespace iot
