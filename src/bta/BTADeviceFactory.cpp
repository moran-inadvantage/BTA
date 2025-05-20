#include "types.h"

#include "BTADeviceFactory.h"
#include "BC127.h"
#include "IDC777.h"

shared_ptr<IBTADeviceDriver> BTADeviceFactory::CreateBTADeviceDriver(shared_ptr<IUart> uart)
{
    RETURN_NULL_IF_NULL(uart);

    shared_ptr<BTASerialDevice> pBtaSerialDevice = make_shared<BTASerialDevice>();
    RETURN_NULL_IF_FAILED(pBtaSerialDevice->SetUArt(uart));
    RETURN_NULL_IF_FAILED(pBtaSerialDevice->SetCommEnable(true));

    shared_ptr<CBTAVersionInfo_t> versionInfo = make_shared<CBTAVersionInfo_t>();
    RETURN_NULL_IF_NULL(versionInfo);

    // Create a lambda function to try creating a driver. We should be able to read the HW ID out of the version and have it match
    auto tryCreateDriver = [&](shared_ptr<IBTADeviceDriver> driver, int expectedHardware) -> shared_ptr<IBTADeviceDriver> {
        RETURN_NULL_IF_NULL(driver);

        // Attempts to open serial connection
        RETURN_NULL_IF_FAILED(driver->ResetAndEstablishSerialConnection(pBtaSerialDevice));
        // After it's open, lets get the device version and validate it
        RETURN_NULL_IF_FAILED(driver->GetDeviceVersion(versionInfo));

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
