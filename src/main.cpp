#include <memory>

#include "uart.h"
#include "BTADeviceFactory.h"
#include "BTADeviceDriver.h"
#include "BTASerialDevice.h"

int simulateCardClass(shared_ptr<BTADeviceDriver> pBtaDeviceDriver);

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


    return simulateCardClass(pBtaDeviceDriver);
}

typedef enum
{

    GET_VERSION,
    DONE,
} CardClassStates_t;

void handleStateGetVersion(shared_ptr<BTADeviceDriver> pBtaDeviceDriver)
{
    shared_ptr<BTAVersionInfo_t> versionInfo = make_shared<BTAVersionInfo_t>();
    ERROR_CODE_T status = pBtaDeviceDriver->GetDeviceVersion(versionInfo);
    if (status == STATUS_SUCCESS)
    {
        printf("Device Version: %s\n", versionInfo->PrintBuildInfo().c_str());
    }
    else
    {
        printf("Failed to get device version\n");
    }
}

static void incState(CardClassStates_t& state)
{
    state = static_cast<CardClassStates_t>(static_cast<int>(state) + 1);
}

int simulateCardClass(shared_ptr<BTADeviceDriver> pBtaDeviceDriver)
{
    CardClassStates_t state = GET_VERSION;

    while (state != DONE)
    {
        switch (state) {
            case GET_VERSION:
            {
                handleStateGetVersion(pBtaDeviceDriver);
                incState(state);
                break;
            }
        }
    }

    printf("Finished card class!\r\n");

    return 0;
}