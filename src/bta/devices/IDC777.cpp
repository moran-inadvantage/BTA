#include "bta/devices/IDC777.h"

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


ERROR_CODE_T BTADeviceDriver::GetDeviceVersion(weak_ptr<BTAVersionInfo_t> version)
{
    vector<string> retStrings;

    RETURN_EC_IF_NULL(ERROR_FAILED, m_pBTASerialDevice);
    RETURN_EC_IF_NULL(ERROR_FAILED, version.lock());


    RETURN_IF_FAILED(m_pBTASerialDevice->ReadData(retStrings, "VERSION"));

    /*
        "IOT747 Copyright 2022"
        "AudioAgent IDC777 V3.1.21"
        "Build: 0400427b"
        "Bluetooth address 245DFC020196"
        "OK"
    */
    RETURN_EC_IF_TRUE(ERROR_FAILED, retStrings.size() < 4);

    const string expectedDevice = "IOT747";

    // Assert ret strings first stringf contains IOT747
    if (retStrings[0].find(expectedDevice) == string::npos) 
    {
        DebugPrintf(m_PacketVerbosity, DEBUG_NO_LOGGING, m_DebugID, "Expected to find %s in %s\n", expectedDevice.c_str(), retStrings[0].c_str());
        return ERROR_FAILED;
    }

   if (retStrings[1].find("AUDIO AGENT IDC777") != string::npos) {
        version.lock()->hardware = BTA_HW_IDC777;
        version.lock()->IDC777fwRev = IDC777_FW_REV_UNKNOWN;
    } else if (retStrings[1].find("AUDIO AGENT BT12") != string::npos) {
        version.lock()->hardware = BTA_HW_BT12;
        version.lock()->BT12FwRev = BT12_FW_REV_UNKNOWN;
    } else {
        return ERROR_FAILED;
    }

    return STATUS_SUCCESS;
}