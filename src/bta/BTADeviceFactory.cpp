#include "types.h"

#include "BTADeviceFactory.h"
#include "BT12.h"
#include "IDC777.h"

shared_ptr<BTADeviceDriver> BTADeviceFactory::CreateBTADeviceDriver(shared_ptr<BTASerialDevice> pBTASerialDevice)
{
    RETURN_NULL_IF_NULL(pBTASerialDevice);

    shared_ptr<BTAVersionInfo_t> versionInfo = make_shared<BTAVersionInfo_t>();
    RETURN_NULL_IF_NULL(versionInfo);

    auto tryCreateDriver = [&](shared_ptr<BTADeviceDriver> driver, int expectedHardware) -> shared_ptr<BTADeviceDriver> {
        RETURN_NULL_IF_NULL(driver);
        
        RETURN_NULL_IF_FAILED(driver->SetBtaSerialDevice(pBTASerialDevice));
        RETURN_NULL_IF_FAILED(driver->EnterCommandMode());
        RETURN_NULL_IF_FAILED(driver->SendReset());

        if (driver->GetDeviceVersion(versionInfo) != STATUS_SUCCESS) 
        {
            return NULL;
        }

        if (versionInfo->hardware != expectedHardware)
        {
            return NULL;
        }

        return driver;
    };


    if (auto driver = tryCreateDriver(make_shared<BT12>(), BTA_HW_BT12))
    {
        DebugPrintf(DEBUG_TRACE_INFO, DEBUG_TRACE_INFO, "BTADeviceFactory", "Creating a BT12 device\n");
        return driver;
    }

    if (auto driver = tryCreateDriver(make_shared<IDC777>(), BTA_HW_IDC777))
    {
        DebugPrintf(DEBUG_TRACE_INFO, DEBUG_TRACE_INFO, "BTADeviceFactory", "Creating a IDC777 device\n");
        return driver;
    }

    return NULL;
}
