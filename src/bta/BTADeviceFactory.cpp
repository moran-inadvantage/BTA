#include "types.h"

#include "bta/BTADeviceFactory.h"
#include "bta/devices/BT12.h"
#include "bta/devices/IDC777.h"

shared_ptr<BTADeviceDriver> BTADeviceFactory::CreateBTADeviceDriver(shared_ptr<BTASerialDevice> pBTASerialDevice)
{
    RETURN_EC_IF_NULL(NULL, pBTASerialDevice);

    shared_ptr<BTAVersionInfo_t> versionInfo = make_shared<BTAVersionInfo_t>();
    RETURN_EC_IF_NULL(NULL, versionInfo);

    // Lambda to attempt driver creation and version check
    auto tryCreateDriver = [&](shared_ptr<BTADeviceDriver> driver, int expectedHardware) -> shared_ptr<BTADeviceDriver> {
        if (!driver || !driver->SetBtaSerialDevice(pBTASerialDevice)) 
        {
            return NULL;
        }

        RETURN_EC_IF_NULL(NULL, driver->EnterCommandMode());
        RETURN_EC_IF_NULL(NULL, driver->SendReset());

        if (driver->GetDeviceVersion(versionInfo) != STATUS_SUCCESS) 
        {
            DebugPrintf(DEBUG_TRACE_WARNING, DEBUG_TRACE_WARNING, "BTADeviceFactory", "Failed to get device version\n");
            return NULL;
        }

        if (versionInfo->hardware == expectedHardware) 
        {
            return driver;
        }

        return NULL;
    };

    // Try BT12
    if (auto driver = tryCreateDriver(make_shared<BT12>(), BTA_HW_BT12)) return driver;

    // Try IDC777
    if (auto driver = tryCreateDriver(make_shared<IDC777>(), BTA_HW_IDC777)) return driver;

    return NULL;
}

