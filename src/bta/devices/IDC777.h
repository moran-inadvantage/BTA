#pragma once

#include "types.h"
#include "BTADeviceDriver.h"
#include "BTASerialDevice.h"

class IDC777 : public IBTADeviceDriver
{
protected:
    const string m_debugId = "IDC777";

    BAUDRATE* GetBaudrateList(INT32U* length);
    void ParseVersionStrings(const vector<string>& retStrings);
    string GetUniqueConfigExpectedString(UniqueConfigSettings_t configOption, bool* notImplemented);
    string GetUniqueConfigSettingString(UniqueConfigSettings_t configOption, bool* notImplemented);

    ERROR_CODE_T GetDeviceCfgRemoteAddress(string &remoteAddress);
    ERROR_CODE_T SetDeviceCfgRemoteAddress(string remoteAddress);
    
    string GetExpectedDigitalAudioParamsString();
    ERROR_CODE_T SetBluetoothDiscoverabilityState(bool connectable, bool discoverable);
    bool ShouldScanForAllDevices();
};