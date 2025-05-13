#pragma once

#include <memory>

#include "BTADeviceDriver.h"
#include "BTASerialDevice.h"

class BTADeviceFactory
{
public:
    BTADeviceFactory(){};
    virtual ~BTADeviceFactory(){};

    // Factory method to create a BTADeviceDriver instance. Will determine what type of BT device we're connected
    // to and then return the valid instance
    static shared_ptr<BTADeviceDriver> CreateBTADeviceDriver(shared_ptr<BTASerialDevice> pBTASerialDevice);
};
