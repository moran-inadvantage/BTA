#include <memory>

#include "bta/devices/IDC777.h"
#include "bta/BTADeviceFactory.h"
#include "bta/BTADeviceDriver.h"
#include "bta/BTASerialDevice.h"
#include "port/linux_x86/uart.h"


int main()
{
    
    shared_ptr<BTASerialDevice> pBtaSerialDevice = make_shared<BTASerialDevice>();
    shared_ptr<IUart> uart = make_shared<CuArt>(0);
    RETURN_EC_IF_FAILED(pBtaSerialDevice->SetUArt(uart));
    RETURN_EC_IF_FAILED(pBtaSerialDevice->SetCommEnable(true));

    shared_ptr<BTADeviceDriver> pBtaDeviceDriver = BTADeviceFactory::CreateBTADeviceDriver(pBtaSerialDevice);

    shared_ptr<BTAVersionInfo_t> version = make_shared<BTAVersionInfo_t>();
    RETURN_EC_IF_FAILED(pBtaDeviceDriver->GetDeviceVersion(version));
    printf("BTA Version: %d.%d.%d\n", version->major, version->minor, version->patch);

    return 0;
}