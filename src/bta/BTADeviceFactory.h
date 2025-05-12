#pragma once

#include <memory>

#include "src/bta/BTADeviceDriver.h"
#include "src/bta/BTASerialDevice.h"

class BTADeviceFactory
{
public:
    static shared_ptr<BTADeviceDriver> CreateBTADeviceDriver(shared_ptr<BTASerialDevice> pBTASerialDevice);
};