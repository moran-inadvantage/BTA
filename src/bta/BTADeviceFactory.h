/********************************************************************************************************
    File Name:  BTADeviceFactory.h

    Notes:      This class is a factory class used to create instances of IBTADeviceDriver.
                It will determine what type of BT device we're connected to and then return the valid instance.
********************************************************************************************************/

#pragma once

//--------------------------------------------------------------------------------------------------
// Platform-specific includes
//--------------------------------------------------------------------------------------------------
#ifdef __IAR_SYSTEMS_ICC__
#include "CPPInterfaces.h"
#endif

#ifdef __x86_64__
#include "uart.h"
#include <memory>
#endif

//--------------------------------------------------------------------------------------------------
// Project includes
//--------------------------------------------------------------------------------------------------
#include "BTADeviceDriver.h"

//--------------------------------------------------------------------------------------------------
// BTADeviceFactory Class
//--------------------------------------------------------------------------------------------------
class BTADeviceFactory
{
  public:
    // Factory method to create a IBTADeviceDriver instance. Will determine what type of BT device we're connected
    // to and then return the valid instance
    static shared_ptr<IBTADeviceDriver> CreateBTADeviceDriver(shared_ptr<IUart> uart);

  private:
    // Helper to validate and return a specific IBTADeviceDriver type if the expected hardware matches
    static shared_ptr<IBTADeviceDriver> TryCreateDriver(
        shared_ptr<IBTADeviceDriver> driver,
        int expectedHardware,
        shared_ptr<CBTASerialDevice> serialDevice,
        shared_ptr<CBTAVersionInfo_t> versionInfo);
};
