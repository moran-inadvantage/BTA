#pragma once

#include "types.h"
#include "BTADeviceDriver.h"
#include "BTASerialDevice.h"

class BT12 : public BTADeviceDriver
{
public:
    BT12();
    ~BT12();

    ERROR_CODE_T SetAndOpenBtaSerialDevice(shared_ptr<BTASerialDevice> pBTASerialDevice);
    ERROR_CODE_T GetDeviceVersion(shared_ptr<BTAVersionInfo_t>& version);
    ERROR_CODE_T TryNextBaudrate();
private:
    const string m_debugId = "BT12";
    void ParseVersionStrings(const vector<string>& retStrings);
};
