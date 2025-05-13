#include <memory>

#include "uart.h"
#include "BTADeviceFactory.h"
#include "BTADeviceDriver.h"
#include "BTASerialDevice.h"

int main()
{
    shared_ptr<BTASerialDevice> pBtaSerialDevice = make_shared<BTASerialDevice>();
    shared_ptr<IUart> uart = make_shared<CuArt>(0);
 
    RETURN_EC_IF_FAILED(pBtaSerialDevice->SetUArt(uart));
    RETURN_EC_IF_FAILED(pBtaSerialDevice->SetCommEnable(true));

    shared_ptr<BTADeviceDriver> pBtaDeviceDriver = BTADeviceFactory::CreateBTADeviceDriver(pBtaSerialDevice);
    if (pBtaDeviceDriver == NULL)
    {
        printf("Failed to create BTADeviceDriver\n");
        return -1;
    }

    shared_ptr<BTAVersionInfo_t> versionInfo = make_shared<BTAVersionInfo_t>();
    if (versionInfo == NULL)
    {
        printf("Failed to create versionInfo\n");
        return -1;
    }
    pBtaDeviceDriver->GetDeviceVersion(versionInfo);

    if (versionInfo->hardware == BTA_HW_BT12)
    {
        printf("BT12 device detected\n");
    }
    else if (versionInfo->hardware == BTA_HW_IDC777)
    {
        printf("IDC777 device detected\n");
    }
    else
    {
        printf("Unknown device detected\n");
    }

    return 0;
}