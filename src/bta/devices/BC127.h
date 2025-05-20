#pragma once

/********************************************************************************************************
    File Name:  BC127.h

    Notes:      This class is used to control the BC127 BTA device, which is in the first gen of the BTA
                It inherits from IBTADeviceDriver and implements the device specific commands for the BC127.

********************************************************************************************************/

#include "BTADeviceDriver.h"
#include "BTASerialDevice.h"
#include "types.h"

class BC127 : public IBTADeviceDriver
{
  public:
    BC127()
    {
        m_DebugId = "BC127";
    }

  private:
    void ParseVersionStrings(const vector<string> &retStrings);
    string GetUniqueConfigExpectedString(UniqueConfigSettings_t configOption, bool *notImplemented);
    string GetUniqueConfigSettingString(UniqueConfigSettings_t configOption, bool *notImplemented);
    ERROR_CODE_T GetDeviceCfgRemoteAddress(string &remoteAddress);
    ERROR_CODE_T SetDeviceCfgRemoteAddress(string remoteAddress);
    ERROR_CODE_T SetBluetoothDiscoverabilityState(bool connectable, bool discoverable);
    bool ShouldScanForAllDevices();
    vector<BAUDRATE> GetBaudrateList();
};
