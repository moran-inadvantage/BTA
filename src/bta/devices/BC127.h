#pragma once

#include "types.h"
#include "BTADeviceDriver.h"
#include "BTASerialDevice.h"

class BC127 : public BTADeviceDriver
{
private:
    const string m_debugId = "BC127";
    void ParseVersionStrings(const vector<string>& retStrings);
    BAUDRATE* GetBaudrateList(INT32U* length);
    string GetUniqueConfigExpectedString(UniqueConfigSettings_t configOption, bool* notImplemented);
    string GetUniqueConfigSettingString(UniqueConfigSettings_t configOption, bool* notImplemented);
    ERROR_CODE_T GetDeviceCfgPairedDevice();
};
