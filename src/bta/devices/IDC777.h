#pragma once

#include <memory>

#include "types.h"
#include "BTADeviceDriver.h"
#include "BTASerialDevice.h"

class IDC777 : public BTADeviceDriver
{
protected:
    const string m_debugId = "IDC777";

    BAUDRATE* GetBaudrateList(INT32U* length);
    void ParseVersionStrings(const vector<string>& retStrings);
    string GetUniqueConfigExpectedString(UniqueConfigSettings_t configOption, bool* notImplemented);
    virtual string GetUniqueConfigSettingString(UniqueConfigSettings_t configOption, bool* notImplemented);
    string GetExpectedDigitalAudioParamsString();
    ERROR_CODE_T GetDeviceCfgPairedDevice();
};