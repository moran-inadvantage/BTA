/********************************************************************************************************
    File Name:  IDC777.h

    Notes:      This class is used to control the IDC777 BTA device, which is in the Bluetooth Adapter Max (BAM)
                It inherits from IBTADeviceDriver and implements the device specific commands for the IDC777.

********************************************************************************************************/

#pragma once

#include "BTADeviceDriver.h"
#include "BTASerialDevice.h"
#include "types.h"

class IDC777 : public IBTADeviceDriver
{
  public:
    IDC777()
    {
        m_DebugId = "IDC777";
    }

  protected:
    void ParseVersionStrings(const vector<string> &retStrings);
    string GetUniqueConfigExpectedString(UniqueConfigSettings_t configOption, bool *notImplemented);
    string GetUniqueConfigSettingString(UniqueConfigSettings_t configOption, bool *notImplemented);

    ERROR_CODE_T GetDeviceCfgRemoteAddress(string &remoteAddress);
    ERROR_CODE_T SetDeviceCfgRemoteAddress(string remoteAddress);

    string GetExpectedDigitalAudioParamsString();
    ERROR_CODE_T SetBluetoothDiscoverabilityState(bool connectable, bool discoverable);
    bool ShouldScanForAllDevices();
    vector<BAUDRATE> GetBaudrateList();
};
