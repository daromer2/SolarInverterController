#include "SettingsStore.h"
#include <Preferences.h>

static const char* NS = "sic";

void SettingsStore::load()
{
    Preferences p;
    if (!p.begin(NS, true)) return;  // nothing stored yet

    s.wifiSsid   = p.getString("wssid", s.wifiSsid);
    s.wifiPass   = p.getString("wpass", s.wifiPass);
    s.deviceName = p.getString("name", s.deviceName);

    s.mqttEnabled = p.getBool("men", s.mqttEnabled);
    s.mqttServer  = p.getString("msrv", s.mqttServer);
    s.mqttPort    = p.getUShort("mprt", s.mqttPort);
    s.mqttUser    = p.getString("musr", s.mqttUser);
    s.mqttPass    = p.getString("mpwd", s.mqttPass);
    s.baseTopic   = p.getString("mtop", s.baseTopic);
    s.publishIntervalS = p.getUShort("mint", s.publishIntervalS);
    s.perFieldTopics = p.getBool("mpf", s.perFieldTopics);
    s.legacyCompat   = p.getBool("mlc", s.legacyCompat);
    s.haDiscovery    = p.getBool("mha", s.haDiscovery);
    s.haPrefix       = p.getString("mhap", s.haPrefix);

    s.profileId    = p.getString("prof", s.profileId);
    s.baudOverride = p.getULong("baud", s.baudOverride);
    s.uartRx       = p.getUChar("urx", s.uartRx);
    s.uartTx       = p.getUChar("utx", s.uartTx);

    s.authEnabled = p.getBool("wen", s.authEnabled);
    s.webUser     = p.getString("wusr", s.webUser);
    s.webPass     = p.getString("wpwd", s.webPass);

    s.ledEnabled   = p.getBool("len", s.ledEnabled);
    s.ledRedPin    = p.getUChar("lred", s.ledRedPin);
    s.ledGreenPin  = p.getUChar("lgrn", s.ledGreenPin);
    s.ledActiveLow = p.getBool("llow", s.ledActiveLow);

    s.feedGuardEnabled   = p.getBool("fgen", s.feedGuardEnabled);
    s.feedGuardTimeoutS  = p.getUShort("fgto", s.feedGuardTimeoutS);
    s.feedGuardFailsafeW = p.getUShort("fgw", s.feedGuardFailsafeW);

    s.otaPassword = p.getString("opwd", s.otaPassword);
    p.end();
}

void SettingsStore::save() const
{
    Preferences p;
    if (!p.begin(NS, false)) return;

    p.putString("wssid", s.wifiSsid);
    p.putString("wpass", s.wifiPass);
    p.putString("name", s.deviceName);

    p.putBool("men", s.mqttEnabled);
    p.putString("msrv", s.mqttServer);
    p.putUShort("mprt", s.mqttPort);
    p.putString("musr", s.mqttUser);
    p.putString("mpwd", s.mqttPass);
    p.putString("mtop", s.baseTopic);
    p.putUShort("mint", s.publishIntervalS);
    p.putBool("mpf", s.perFieldTopics);
    p.putBool("mlc", s.legacyCompat);
    p.putBool("mha", s.haDiscovery);
    p.putString("mhap", s.haPrefix);

    p.putString("prof", s.profileId);
    p.putULong("baud", s.baudOverride);
    p.putUChar("urx", s.uartRx);
    p.putUChar("utx", s.uartTx);

    p.putBool("wen", s.authEnabled);
    p.putString("wusr", s.webUser);
    p.putString("wpwd", s.webPass);

    p.putBool("len", s.ledEnabled);
    p.putUChar("lred", s.ledRedPin);
    p.putUChar("lgrn", s.ledGreenPin);
    p.putBool("llow", s.ledActiveLow);

    p.putBool("fgen", s.feedGuardEnabled);
    p.putUShort("fgto", s.feedGuardTimeoutS);
    p.putUShort("fgw", s.feedGuardFailsafeW);

    p.putString("opwd", s.otaPassword);
    p.end();
}

void SettingsStore::factoryReset()
{
    Preferences p;
    if (p.begin(NS, false)) {
        p.clear();
        p.end();
    }
    s = Settings{};
}

void SettingsStore::toJson(JsonDocument& doc, bool includeSecrets) const
{
    doc["wifiSsid"] = s.wifiSsid;
    if (includeSecrets) doc["wifiPass"] = s.wifiPass;
    doc["deviceName"] = s.deviceName;

    doc["mqttEnabled"] = s.mqttEnabled;
    doc["mqttServer"] = s.mqttServer;
    doc["mqttPort"] = s.mqttPort;
    doc["mqttUser"] = s.mqttUser;
    if (includeSecrets) doc["mqttPass"] = s.mqttPass;
    doc["baseTopic"] = s.baseTopic;
    doc["publishIntervalS"] = s.publishIntervalS;
    doc["perFieldTopics"] = s.perFieldTopics;
    doc["legacyCompat"] = s.legacyCompat;
    doc["haDiscovery"] = s.haDiscovery;
    doc["haPrefix"] = s.haPrefix;

    doc["profileId"] = s.profileId;
    doc["baudOverride"] = s.baudOverride;
    doc["uartRx"] = s.uartRx;
    doc["uartTx"] = s.uartTx;

    doc["authEnabled"] = s.authEnabled;
    doc["webUser"] = s.webUser;
    if (includeSecrets) doc["webPass"] = s.webPass;

    doc["ledEnabled"] = s.ledEnabled;
    doc["ledRedPin"] = s.ledRedPin;
    doc["ledGreenPin"] = s.ledGreenPin;
    doc["ledActiveLow"] = s.ledActiveLow;

    doc["feedGuardEnabled"] = s.feedGuardEnabled;
    doc["feedGuardTimeoutS"] = s.feedGuardTimeoutS;
    doc["feedGuardFailsafeW"] = s.feedGuardFailsafeW;

    if (includeSecrets) doc["otaPassword"] = s.otaPassword;
    doc["fwVersion"] = FW_VERSION;
}

// Apply a key only when present, keeping current value otherwise.
template <typename T>
static void applyIf(JsonDocument& doc, const char* key, T& target)
{
    if (!doc[key].isNull()) target = doc[key].as<T>();
}

bool SettingsStore::fromJson(JsonDocument& doc)
{
    applyIf(doc, "wifiSsid", s.wifiSsid);
    applyIf(doc, "wifiPass", s.wifiPass);
    applyIf(doc, "deviceName", s.deviceName);

    applyIf(doc, "mqttEnabled", s.mqttEnabled);
    applyIf(doc, "mqttServer", s.mqttServer);
    applyIf(doc, "mqttPort", s.mqttPort);
    applyIf(doc, "mqttUser", s.mqttUser);
    applyIf(doc, "mqttPass", s.mqttPass);
    applyIf(doc, "baseTopic", s.baseTopic);
    applyIf(doc, "publishIntervalS", s.publishIntervalS);
    applyIf(doc, "perFieldTopics", s.perFieldTopics);
    applyIf(doc, "legacyCompat", s.legacyCompat);
    applyIf(doc, "haDiscovery", s.haDiscovery);
    applyIf(doc, "haPrefix", s.haPrefix);

    applyIf(doc, "profileId", s.profileId);
    applyIf(doc, "baudOverride", s.baudOverride);
    applyIf(doc, "uartRx", s.uartRx);
    applyIf(doc, "uartTx", s.uartTx);

    applyIf(doc, "authEnabled", s.authEnabled);
    applyIf(doc, "webUser", s.webUser);
    applyIf(doc, "webPass", s.webPass);

    applyIf(doc, "ledEnabled", s.ledEnabled);
    applyIf(doc, "ledRedPin", s.ledRedPin);
    applyIf(doc, "ledGreenPin", s.ledGreenPin);
    applyIf(doc, "ledActiveLow", s.ledActiveLow);

    applyIf(doc, "feedGuardEnabled", s.feedGuardEnabled);
    applyIf(doc, "feedGuardTimeoutS", s.feedGuardTimeoutS);
    applyIf(doc, "feedGuardFailsafeW", s.feedGuardFailsafeW);

    applyIf(doc, "otaPassword", s.otaPassword);

    if (s.deviceName.length() == 0) s.deviceName = "solar";
    if (s.publishIntervalS < 1) s.publishIntervalS = 1;
    return true;
}
