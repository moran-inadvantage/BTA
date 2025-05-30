#include <memory>

#include "../External/cxxopts/include/cxxopts.hpp"

#include "BTADeviceDriver.h"
#include "BTADeviceFactory.h"
#include "BTASerialDevice.h"
#include "uart.h"

typedef enum
{
    QualMode = 0,
    PlayActiveSong = 1,
    InputDevice = 2,
    OutputDevice = 3,
} AppState_t;

void doMainTask(shared_ptr<IBTADeviceDriver> pBtaDeviceDriver, AppState_t state);
CTimeDeltaSec m_TestModeTimer;

bool m_inquiryActive;
bool m_isAutoConnecting;
string m_connectDeviceAddr;
CTimeDelta m_autoConnectTimer;
list<shared_ptr<CBTEADetectedDevice> > detectedDeviceList;

void doAppSetup()
{

    m_TestModeTimer.ResetTime(0);
    m_inquiryActive = false;
}
static int port;
static AppState_t appMode = OutputDevice;

void doArgParse(int argc, char *argv[])
{
    cxxopts::Options options("MyApp", "Bluetooth Audio Device Controller");

    options.add_options()("p,port", "Port number", cxxopts::value<int>()->default_value("0"))("m,mode", "Operating mode: input, output, qual, play", cxxopts::value<std::string>())("h,help", "Print usage");

    auto result = options.parse(argc, argv);

    if (result.count("help"))
    {
        std::cout << options.help() << std::endl;
        return;
    }

    port = result["port"].as<int>();
    appMode = OutputDevice;

    if (result.count("mode"))
    {
        string modeStr = result["mode"].as<std::string>();
        if (modeStr == "input")
            appMode = InputDevice;
        if (modeStr == "output")
            appMode = OutputDevice;
        if (modeStr == "qual")
            appMode = QualMode;
        if (modeStr == "play")
            appMode = PlayActiveSong;
    }

    std::cout << "Using port: " << port << std::endl;
    std::cout << "Using mode: " << appMode << std::endl;
}

int main(int argc, char *argv[])
{
    doArgParse(argc, argv);

    doAppSetup();

    printf("Creating UART on port %d\r\n", port);
    shared_ptr<IUart> uart = make_shared<CuArt>(port);

    printf("Discovering BTA Device\r\n");
    shared_ptr<IBTADeviceDriver> pBtaDeviceDriver;
    if (FAILED(CBTADeviceFactory::CreateBTADeviceDriver(uart, pBtaDeviceDriver)))
    {
        printf("Failed to create IBTADeviceDriver\n");
        return -1;
    }

    printf("Performing factory reset\r\n");
    pBtaDeviceDriver->FactoryReset();

    printf("Running IBTADeviceDriver\r\n");
    pBtaDeviceDriver->InitializeDeviceConfiguration();
    pBtaDeviceDriver->SetDeviceMode(BTA_DEVICE_MODE_INPUT);

    while (true)
    {
        if (!pBtaDeviceDriver->IsDeviceReadyForUse())
        {
            printf("This shouldn't happen!\r\n");
            return 0;
        }

        doMainTask(pBtaDeviceDriver, OutputDevice);

        OSTimeDly(10);
    }
}

static void NotifyConnection()
{
    printf("Connected to device: %s\r\n", m_connectDeviceAddr.c_str());
}
static void NotifyDisconnection()
{
    printf("Disconnected from device: %s\r\n", m_connectDeviceAddr.c_str());
}

static void NotifyDetectedDevices()
{
    printf("Detected devices:\r\n");
    for (auto device : detectedDeviceList)
    {
        printf("Device: %s, Name: %s\r\n", device->m_btAddress.c_str(), device->m_btDeviceName.c_str());
    }
    printf("End of detected devices\r\n");
}

void doMainTask(shared_ptr<IBTADeviceDriver> pBtaDeviceDriver, AppState_t state)
{

    if (state == QualMode)
    {
        m_TestModeTimer.GetElapsedTime();
        if (m_TestModeTimer.IsTimeExpired())
        {
            printf("Sending out inquiry command\r\n");
            pBtaDeviceDriver->SendInquiry(10);
            m_TestModeTimer.ResetTime(14);
        }
    }

    if (state == QualMode || state == PlayActiveSong)
    {
        if (pBtaDeviceDriver->PlayNextMusicSequence() != STATUS_SUCCESS)
        {
            printf("Failed to play next music sequence\r\n");
        }
    }

    if (state == OutputDevice)
    {
        if (m_inquiryActive || !pBtaDeviceDriver->IsDeviceConnected())
        {
            ERROR_CODE_T result = pBtaDeviceDriver->ScanForBtDevices(detectedDeviceList, 5);

            m_inquiryActive = (result != STATUS_SUCCESS);
            if (!m_inquiryActive)
            {
                if (m_isAutoConnecting)
                {
                    if (m_autoConnectTimer.IsTimeExpired())
                    {
                        m_isAutoConnecting = false;
                        NotifyConnection();
                    }
                    else if (!detectedDeviceList.empty())
                    {
                        m_connectDeviceAddr = detectedDeviceList.front()->m_btAddress;
                        m_isAutoConnecting = false;
                    }
                }

                NotifyDetectedDevices();
            }
        }
        else
        {
            if (!pBtaDeviceDriver->IsPairedWithDevice())
            {
                detectedDeviceList.clear();
                NotifyDetectedDevices();
            }
            pBtaDeviceDriver->WatchdogPet(true);
        }
    }
    else
    {
        pBtaDeviceDriver->WatchdogPet(true);
    }
}