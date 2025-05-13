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

    printf("Discovering BTA Device\r\n");

    shared_ptr<BTADeviceDriver> pBtaDeviceDriver = BTADeviceFactory::CreateBTADeviceDriver(pBtaSerialDevice);
    if (pBtaDeviceDriver == NULL)
    {
        printf("Failed to create BTADeviceDriver\n");
        return -1;
    }

    printf("Running BTADeviceDriver\r\n");

    return simulateCardClass(pBtaDeviceDriver);
}

typedef enum
{
    BT_CONFIG_GET_VERSION,
    BT_CONFIG_GET_LOCAL_ADDR,
    BT_CONFIG_SET_AUDIO_MODE,
    BT_CONFIG_SET_DIGITAL_AUDIO_PARAMS,
    BT_CONFIG_SET_AUTOCONN,
    BT_CONFIG_SET_BT_VOLUME,
    BT_CONFIG_SET_COD,
    BT_CONFIG_SET_CODEC,
    BT_CONFIG_SET_DEVICE_ID,
    BT_CONFIG_SET_LED_ENABLE,
    BT_CONFIG_SET_GPIO_CONFIG,
    BT_CONFIG_SET_LONG_NAME,
    BT_CONFIG_SET_SHORT_NAME,
    BT_CONFIG_GET_PAIRED_DEVICE,
    BT_CONFIG_SET_PROFILES,
    BT_CONFIG_SET_VREG_ROLE,
    BT_CONFIG_SET_SSP_CAPS,
    BT_CONFIG_SET_BT_STATE,
    BT_CONFIG_WRITE_CFG,
    BT_CONFIG_RESTART,
    BT_CONFIG_DONE,
} CardClassStates_t;

static string StateToString(CardClassStates_t state)
{
    switch (state) {
        case BT_CONFIG_GET_VERSION: return "BT_CONFIG_GET_VERSION";
        case BT_CONFIG_GET_LOCAL_ADDR: return "BT_CONFIG_GET_LOCAL_ADDR";
        case BT_CONFIG_SET_AUDIO_MODE: return "BT_CONFIG_SET_AUDIO_MODE";
        case BT_CONFIG_SET_DIGITAL_AUDIO_PARAMS: return "BT_CONFIG_SET_DIGITAL_AUDIO_PARAMS";
        case BT_CONFIG_SET_AUTOCONN: return "BT_CONFIG_SET_AUTOCONN";
        case BT_CONFIG_SET_BT_VOLUME: return "BT_CONFIG_SET_BT_VOLUME";
        case BT_CONFIG_SET_COD: return "BT_CONFIG_SET_COD";
        case BT_CONFIG_SET_CODEC: return "BT_CONFIG_SET_CODEC";
        case BT_CONFIG_SET_DEVICE_ID: return "BT_CONFIG_SET_DEVICE_ID";
        case BT_CONFIG_SET_LED_ENABLE: return "BT_CONFIG_SET_LED_ENABLE";
        case BT_CONFIG_SET_GPIO_CONFIG: return "BT_CONFIG_SET_GPIO_CONFIG";
        case BT_CONFIG_SET_LONG_NAME: return "BT_CONFIG_SET_LONG_NAME";
        case BT_CONFIG_SET_SHORT_NAME: return "BT_CONFIG_SET_SHORT_NAME";
        case BT_CONFIG_GET_PAIRED_DEVICE: return "BT_CONFIG_GET_PAIRED_DEVICE";
        case BT_CONFIG_SET_PROFILES: return "BT_CONFIG_SET_PROFILES";
        case BT_CONFIG_SET_VREG_ROLE: return "BT_CONFIG_SET_VREG_ROLE";
        case BT_CONFIG_SET_SSP_CAPS: return "BT_CONFIG_SET_SSP_CAPS";
        case BT_CONFIG_SET_BT_STATE: return "BT_CONFIG_SET_BT_STATE";
        case BT_CONFIG_WRITE_CFG: return "BT_CONFIG_WRITE_CFG";
        case BT_CONFIG_RESTART: return "BT_CONFIG_RESTART";
        case BT_CONFIG_DONE: return "BT_CONFIG_DONE";
        default:
            break;
    }
    return "STATE NOT FOUND";
}


static void handleStateGetVersion(shared_ptr<BTADeviceDriver> pBtaDeviceDriver)
{
    shared_ptr<BTAVersionInfo_t> versionInfo = make_shared<BTAVersionInfo_t>();
    ERROR_CODE_T status = pBtaDeviceDriver->GetDeviceVersion(versionInfo);
    if (status == STATUS_SUCCESS)
    {
        printf("%s", versionInfo->PrintBuildInfo().c_str());
    }
    else
    {
        printf("Failed to get device version\n");
    }
}

static void handleStateGetLocalAddr(shared_ptr<BTADeviceDriver> pBtaDeviceDriver)
{
    if (SUCCEEDED(pBtaDeviceDriver->GetDeviceCfgLocalAddress()))
    {
        printf("Local Address: %s\n", pBtaDeviceDriver->GetPublicAddress().c_str());
    }
    else
    {
        printf("Failed to get local address\n");
    }
}

static void handleStateSetAudioMode(shared_ptr<BTADeviceDriver> pBtaDeviceDriver)
{
    if (SUCCEEDED(pBtaDeviceDriver->SetDeviceCfgDigitalAudioMode()))
    {
        printf("Set audio mode successfully\n");
    }
    else
    {
        printf("Failed to set audio mode\n");
    }
}

static void incState(CardClassStates_t& state)
{
    CardClassStates_t newState = static_cast<CardClassStates_t>(static_cast<int>(state) + 1);
    printf("Moving from %s to %s\n", StateToString(state).c_str(), StateToString(newState).c_str());
    state = newState;
}

int simulateCardClass(shared_ptr<BTADeviceDriver> pBtaDeviceDriver)
{
    CardClassStates_t state = BT_CONFIG_GET_VERSION;

    while (state != BT_CONFIG_DONE)
    {
        switch (state) {
            case BT_CONFIG_GET_VERSION:
            {
                handleStateGetVersion(pBtaDeviceDriver);
                incState(state);
                break;
            }
            case BT_CONFIG_GET_LOCAL_ADDR:
            {
                handleStateGetLocalAddr(pBtaDeviceDriver);
                incState(state);
                break;
            }
            case BT_CONFIG_SET_AUDIO_MODE:
            {
                handleStateSetAudioMode(pBtaDeviceDriver);
                incState(state);
                break;
            }
            default:
            {
                printf("Unhandled state: %s\n", StateToString(state).c_str());
                incState(state);
                break;
            }
        }
    }

    printf("Finished card class!\r\n");

    return 0;
}