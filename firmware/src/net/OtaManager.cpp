#include "OtaManager.h"
#include <ArduinoOTA.h>
#include "../sys/Logx.h"

void OtaManager::begin(const String& hostname, const String& password)
{
    ArduinoOTA.setHostname(hostname.c_str());
    if (password.length()) ArduinoOTA.setPassword(password.c_str());

    ArduinoOTA.onStart([this]() {
        _active = true;
        LOGI("ota", "start (%s)", ArduinoOTA.getCommand() == U_FLASH ? "firmware" : "filesystem");
    });
    ArduinoOTA.onEnd([this]() {
        _active = false;
        LOGI("ota", "done");
    });
    ArduinoOTA.onError([this](ota_error_t error) {
        _active = false;
        LOGI("ota", "error %u", error);
    });
    ArduinoOTA.begin();
}

void OtaManager::tick()
{
    ArduinoOTA.handle();
}
