#include "IDC777.h"

IDC777::IDC777()
{

}

IDC777::~IDC777()
{

}

ERROR_CODE_T IDC777::SetBtaSerialDevice(shared_ptr<BTASerialDevice> pBTASerialDevice)
{
    RETURN_EC_IF_NULL(ERROR_FAILED, pBTASerialDevice);

    // Default baudrate for IDC777
    RETURN_IF_FAILED(pBTASerialDevice->SetBaudrate(BAUDRATE_115200));

    m_pBTASerialDevice = pBTASerialDevice;

    return STATUS_SUCCESS;
}

ERROR_CODE_T IDC777::GetDeviceVersion(shared_ptr<BTAVersionInfo_t>& version)
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
        Expected Output

        "IOT747 Copyright 2022"
        "AudioAgent IDC777 V3.1.21"
        "Build: 0400427b"
        "Bluetooth address 245DFC020196"
        "OK"
    */

    RETURN_IF_FAILED(m_pBTASerialDevice->ReadData(retStrings, "VERSION"));
    RETURN_EC_IF_TRUE(ERROR_FAILED, retStrings.size() < 4);

    const string expectedDevice = "IOT747";
    // Assert ret strings first string contains IOT747
    if (retStrings[0].find(expectedDevice) == string::npos) 
    {
        DebugPrintf(m_PacketVerbosity, DEBUG_NO_LOGGING, m_DebugID, "Expected to find %s in %s\n", expectedDevice.c_str(), retStrings[0].c_str());
        return ERROR_FAILED;
    }

    // Now that we've identified that it is the IDC777, we can safely set our version
    localVersion->hardware = BTA_HW_IDC777;
    version = localVersion;

    return STATUS_SUCCESS;
}
