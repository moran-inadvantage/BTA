#include "types.h"

#include "BTADeviceFactory.h"
#include "BC127.h"
#include "IDC777.h"

shared_ptr<BTADeviceDriver> BTADeviceFactory::CreateBTADeviceDriver(shared_ptr<BTASerialDevice> pBTASerialDevice)
{
    RETURN_NULL_IF_NULL(pBTASerialDevice);

    shared_ptr<BTAVersionInfo_t> versionInfo = make_shared<BTAVersionInfo_t>();
    RETURN_NULL_IF_NULL(versionInfo);

    auto tryCreateDriver = [&](shared_ptr<BTADeviceDriver> driver, int expectedHardware) -> shared_ptr<BTADeviceDriver> {
        RETURN_NULL_IF_NULL(driver);

        RETURN_NULL_IF_FAILED(driver->SetAndOpenBtaSerialDevice(pBTASerialDevice));

        // Try various baud rates until we find the correct one.
        // Even if we find the right one, may not be our device
        do
        {
            driver->EnterCommandMode();
            driver->SendReset();

            // Expected to fail if baud rate is incorrect
            if (FAILED(driver->GetDeviceVersion(versionInfo)))
            {
                continue;
            }

            if (versionInfo->hardware == expectedHardware)
            {
                break;
            }
        } while (SUCCEEDED(driver->TryNextBaudrate()));

        return (versionInfo->hardware == expectedHardware) ? driver : NULL;
    };

    if (auto driver = tryCreateDriver(make_shared<IDC777>(), BTA_HW_IDC777))
    {
        DebugPrintf(DEBUG_TRACE_INFO, DEBUG_TRACE_INFO, "BTADeviceFactory", "Creating a IDC777 device\n");
        return driver;
    }

    if (auto driver = tryCreateDriver(make_shared<BC127>(), BTA_HW_BC127))
    {
        DebugPrintf(DEBUG_TRACE_INFO, DEBUG_TRACE_INFO, "BTADeviceFactory", "Creating a BC127 device\n");
        return driver;
    }

    return NULL;
}
