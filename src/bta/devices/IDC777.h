#pragma once

#include <memory>

#include "types.h"
#include "BTADeviceDriver.h"
#include "BTASerialDevice.h"

class IDC777 : public BTADeviceDriver
{
public:
    IDC777();
    ~IDC777();

    ERROR_CODE_T SetAndOpenBtaSerialDevice(shared_ptr<BTASerialDevice> pBTASerialDevice);
    ERROR_CODE_T GetDeviceVersion(shared_ptr<BTAVersionInfo_t>& version);
    ERROR_CODE_T TryNextBaudrate();
private:
    const string m_debugId = "IDC777";
private:
    void ParseVersionStrings(const vector<string>& retStrings);
};