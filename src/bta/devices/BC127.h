#pragma once

#include "types.h"
#include "BTADeviceDriver.h"
#include "BTASerialDevice.h"

class BC127 : public BTADeviceDriver
{
public:
    BC127();
    ~BC127();

    ERROR_CODE_T SetAndOpenBtaSerialDevice(shared_ptr<BTASerialDevice> pBTASerialDevice);
    ERROR_CODE_T GetDeviceVersion(shared_ptr<BTAVersionInfo_t>& version);
private:
    const string m_debugId = "BC127";
    void ParseVersionStrings(const vector<string>& retStrings);
    BAUDRATE* GetBaudrateList(INT32U* length);
};
