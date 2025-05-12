#pragma once

#include <memory>

#include "types.h"
#include "bta/BTADeviceDriver.h"
#include "bta/BTASerialDevice.h"

class IDC777 : public BTADeviceDriver
{
public:
    IDC777();
    ~IDC777();

    ERROR_CODE_T SetBtaSerialDevice(shared_ptr<BTASerialDevice> pBTASerialDevice);
    ERROR_CODE_T GetDeviceVersion(shared_ptr<BTAVersionInfo_t>& version);
private:
    const string m_debugId = "IDC777";
};