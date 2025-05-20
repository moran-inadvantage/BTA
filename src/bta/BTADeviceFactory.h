/********************************************************************************************************
    File Name:  BTADeviceFactory.h

    Notes:      This class is a factory class used to create instances of IBTADeviceDriver.
                It will determine what type of BT device we're connected to and then return the valid instance.

********************************************************************************************************/

#pragma once

#include <memory>

#ifdef __x86_64__
#include "iuart.h"
#else
#include "CPPInterfaces.h"
#endif

#include "BTADeviceDriver.h"

class BTADeviceFactory
{
  public:
    // Factory method to create a IBTADeviceDriver instance. Will determine what type of BT device we're connected
    // to and then return the valid instance
    static shared_ptr<IBTADeviceDriver> CreateBTADeviceDriver(shared_ptr<IUart> uart);

  private:
    static shared_ptr<IBTADeviceDriver> TryCreateDriver(
        shared_ptr<IBTADeviceDriver> driver,
        int expectedHardware,
        shared_ptr<BTASerialDevice> serialDevice,
        shared_ptr<CBTAVersionInfo_t> versionInfo);
};
