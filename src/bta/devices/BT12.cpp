#include "BT12.h"

BT12::BT12()
{
    m_PacketVerbosity = 0;
}
BT12::~BT12()
{
    // Destructor implementation if needed
}

ERROR_CODE_T BT12::SetBtaSerialDevice(shared_ptr<BTASerialDevice> pBTASerialDevice)
{
    RETURN_EC_IF_NULL(ERROR_FAILED, pBTASerialDevice);

    ERROR_CODE_T status = pBTASerialDevice->SetBaudrate(BAUDRATE_9600);
    RETURN_EC_IF_FAILED(status);

    m_pBTASerialDevice = pBTASerialDevice;

    return STATUS_SUCCESS;
}

 
ERROR_CODE_T BT12::GetDeviceVersion(shared_ptr<BTAVersionInfo_t>& version)
{
    if (m_versionInfo.get() != NULL) 
    {
        version = m_versionInfo;
        return STATUS_SUCCESS;
    }

    vector<string> retStrings;
    shared_ptr<BTAVersionInfo_t> localVersion = make_shared<BTAVersionInfo_t>();

    RETURN_EC_IF_NULL(ERROR_FAILED, m_pBTASerialDevice);

    /*
        Sierra Wireless Copyright 2018
        Melody Audio V7.0
        Build: 1522931717
        Bluetooth addresses: 20FABB000160 7191D978637D
        Profiles: A2DP AVRCP HFP BLE SPP PBAP MAP TWS
        Codecs: SBC
        OK
    */
    RETURN_IF_FAILED(m_pBTASerialDevice->ReadData(retStrings, "VERSION"));
    RETURN_EC_IF_TRUE(ERROR_FAILED, retStrings.size() < 5);
    const string expectedDevice = "Sierra Wireless";

    // Assert ret strings first string contains Sierra Wireless
    if (retStrings[0].find(expectedDevice) == string::npos)
    {
        DebugPrintf(m_PacketVerbosity, DEBUG_NO_LOGGING, m_DebugID, "Expected to find %s in %s\n", expectedDevice.c_str(), retStrings[0].c_str());
        return ERROR_FAILED;
    }

    // Now that we've identified that it is the BT12, we can safely set our version
    localVersion->hardware = BTA_HW_BT12;
    version = localVersion;

    return ERROR_FAILED;
}
