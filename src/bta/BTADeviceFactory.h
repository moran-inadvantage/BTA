/********************************************************************************************************
    File Name:  BTADeviceFactory.h

    Notes:      This class is a factory class used to create instances of IBTADeviceDriver.
                It will determine what type of BT device we're connected to and then return the valid instance.

********************************************************************************************************/

#pragma once

#include <memory>

#include "iuart.h"
#include "BTADeviceDriver.h"

class BTADeviceFactory
{
public:
    // Factory method to create a IBTADeviceDriver instance. Will determine what type of BT device we're connected
    // to and then return the valid instance
    static shared_ptr<IBTADeviceDriver> CreateBTADeviceDriver(shared_ptr<IUart> uart);
};
