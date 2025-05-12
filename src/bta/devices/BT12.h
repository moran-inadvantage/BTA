#pragma once

#include "types.h"
#include "bta/BTADeviceDriver.h"
#include "bta/BTASerialDevice.h"

class BT12 : public BTADeviceDriver
{
public:
    BT12();
    ~BT12();

    ERROR_CODE_T SetBtaSerialDevice(shared_ptr<BTASerialDevice> pBTASerialDevice);
    ERROR_CODE_T GetDeviceVersion(weak_ptr<BTAVersionInfo_t> version);
private:
    const string m_debugId = "BT12";
};