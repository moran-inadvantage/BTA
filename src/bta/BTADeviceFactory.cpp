#include "types.h"

#include "BC127.h"
#include "BTADeviceFactory.h"
#include "IDC777.h"

shared_ptr<IBTADeviceDriver> BTADeviceFactory::CreateBTADeviceDriver(shared_ptr<IUart> uart)
{
    if (uart == NULL)
        return shared_ptr<IBTADeviceDriver>();

    shared_ptr<CBTASerialDevice> pBtaSerialDevice = make_shared<CBTASerialDevice>();
    if (pBtaSerialDevice->SetUArt(uart) != 0)
        return shared_ptr<IBTADeviceDriver>();
    if (pBtaSerialDevice->SetCommEnable(true) != 0)
        return shared_ptr<IBTADeviceDriver>();

    shared_ptr<CBTAVersionInfo_t> versionInfo = make_shared<CBTAVersionInfo_t>();
    if (versionInfo == NULL)
        return shared_ptr<IBTADeviceDriver>();

    shared_ptr<IBTADeviceDriver> driver;

    driver = TryCreateDriver(make_shared<BC127>(), BTA_HW_BC127, pBtaSerialDevice, versionInfo);
    if (driver)
    {
        DebugPrintf(DEBUG_TRACE, DEBUG_TRACE, "BTADeviceFactory", "Creating a BC127 device\n");
        return driver;
    }

    driver = TryCreateDriver(make_shared<IDC777>(), BTA_HW_IDC777, pBtaSerialDevice, versionInfo);
    if (driver)
    {
        DebugPrintf(DEBUG_TRACE, DEBUG_TRACE, "BTADeviceFactory", "Creating a IDC777 device\n");
        return driver;
    }

    return shared_ptr<IBTADeviceDriver>();
}

shared_ptr<IBTADeviceDriver> BTADeviceFactory::TryCreateDriver(
    shared_ptr<IBTADeviceDriver> driver,
    int expectedHardware,
    shared_ptr<CBTASerialDevice> serialDevice,
    shared_ptr<CBTAVersionInfo_t> versionInfo)
{
    if (driver == NULL)
        return shared_ptr<IBTADeviceDriver>();

    if (driver->ResetAndEstablishSerialConnection(serialDevice) != 0)
        return shared_ptr<IBTADeviceDriver>();

    if (driver->GetDeviceVersion(versionInfo) != 0)
        return shared_ptr<IBTADeviceDriver>();

    if (versionInfo->hardware != expectedHardware)
        return shared_ptr<IBTADeviceDriver>();

    return driver;
}
