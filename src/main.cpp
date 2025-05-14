#include <memory>

#include "uart.h"
#include "BTADeviceFactory.h"
#include "BTADeviceDriver.h"
#include "BTASerialDevice.h"

int simulateCardClass(shared_ptr<BTADeviceDriver> pBtaDeviceDriver);

int main()
{
    shared_ptr<BTASerialDevice> pBtaSerialDevice = make_shared<BTASerialDevice>();
    shared_ptr<IUart> uart = make_shared<CuArt>(1);
 
    RETURN_EC_IF_FAILED(pBtaSerialDevice->SetUArt(uart));
    RETURN_EC_IF_FAILED(pBtaSerialDevice->SetCommEnable(true));

    printf("Discovering BTA Device\r\n");

    shared_ptr<BTADeviceDriver> pBtaDeviceDriver = BTADeviceFactory::CreateBTADeviceDriver(pBtaSerialDevice);
    if (pBtaDeviceDriver == NULL)
    {
        printf("Failed to create BTADeviceDriver\n");
        return -1;
    }

    printf("Running BTADeviceDriver\r\n");

    pBtaDeviceDriver->InitializeDeviceConfiguration();
}
