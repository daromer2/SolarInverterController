#include "MqttLegacyCompat.h"
#include <string.h>

namespace legacy {

// --- PCM (src.ino:277-292) ----------------------------------------------------
static const TopicMap PCM_MAP[] = {
    {"battv",   "battery_voltage"},
    {"solarv",  "pv_input_voltage"},
    {"batta",   "battery_charge_current"},
    {"wattage", "charging_power"},
    {"solara",  "pv_input_current"},
    {nullptr, nullptr},
};

// --- PIP (src.ino:296-321) ----------------------------------------------------
static const TopicMap PIP_MAP[] = {
    {"GridV",    "grid_voltage"},
    {"GridF",    "grid_frequency"},
    {"AcV",      "ac_output_voltage"},
    {"AcF",      "ac_output_frequency"},
    {"AcVA",     "ac_output_va"},
    {"AcW",      "ac_output_power"},
    {"Load",     "load_percent"},
    {"BattV",    "battery_voltage"},
    {"ChgeA",    "battery_charge_current"},
    {"BattC",    "battery_capacity"},
    {"Temp",     "heatsink_temp"},
    {"PVbattA",  "pv_input_current"},
    {"PVV",      "pv_input_voltage"},
    {"BattDisA", "battery_discharge_current"},
    {"Stat",     "device_status"},
    {"PVchW",    "pv_charging_power"},
    {nullptr, nullptr},
};

// --- MPI (src.ino:353-389). solar1w/solar2w were computed V*A in legacy. ------
static const TopicMap MPI_MAP[] = {
    {"solar1w",           nullptr},  // computed: pv1_voltage * pv1_current
    {"solar2w",           nullptr},  // computed: pv2_voltage * pv2_current
    {"solarInputV1",      "pv1_voltage"},
    {"solarInputV2",      "pv2_voltage"},
    {"solarInputA1",      "pv1_current"},
    {"solarInputA2",      "pv2_current"},
    {"battV",             "battery_voltage"},
    {"battA",             "battery_current"},
    {"acInputVoltageR",   "grid_voltage_r"},
    {"acInputVoltageS",   "grid_voltage_s"},
    {"acInputVoltageT",   "grid_voltage_t"},
    {"acInputCurrentR",   "grid_current_r"},
    {"acInputCurrentS",   "grid_current_s"},
    {"acInputCurrentT",   "grid_current_t"},
    {"acOutputCurrentR",  "ac_output_current_r"},
    {"acOutputCurrentS",  "ac_output_current_s"},
    {"acOutputCurrentT",  "ac_output_current_t"},
    {"innerTemp",         "inner_temp"},
    {"componentTemp",     "component_temp"},
    {"acWattageR",        "ac_output_power_r"},
    {"acWattageS",        "ac_output_power_s"},
    {"acWattageT",        "ac_output_power_t"},
    {"acWattageTotal",    "ac_output_power_total"},
    {"ac_output_procent", "ac_output_percent"},
    {"feedingGridDirectionR", "feed_dir_r"},
    {"calibrationWattR",  "feed_calibration_r"},
    {"feedingGridDirectionS", "feed_dir_s"},
    {"calibrationWattS",  "feed_calibration_s"},
    {"feedingGridDirectionT", "feed_dir_t"},
    {"calibrationWattT",  "feed_calibration_t"},
    {nullptr, nullptr},
};

const TopicMap* topicMap(const char* profileId)
{
    if (strcmp(profileId, "MPI") == 0) return MPI_MAP;
    if (strcmp(profileId, "PIP") == 0) return PIP_MAP;
    if (strcmp(profileId, "PCM") == 0) return PCM_MAP;
    return nullptr;
}

const char* statusJsonKey(const char* profileId, const char* legacyName)
{
    // legacy src.ino used acOutputProcentage in the JSON but
    // ac_output_procent as topic name
    if (strcmp(profileId, "MPI") == 0 && strcmp(legacyName, "ac_output_procent") == 0)
        return "acOutputProcentage";
    return legacyName;
}

} // namespace legacy
