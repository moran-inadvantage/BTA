#include "bta/devices/BT12.h"

ERROR_CODE_T BT12::SetBtaSerialDevice(shared_ptr<BTASerialDevice> pBTASerialDevice)
{
    RETURN_EC_IF_NULL(ERROR_FAILED, pBTASerialDevice);

    RETURN_IF_FAILED(pBTASerialDevice->SetBaudrate(BAUDRATE_9600));

    m_pBTASerialDevice = pBTASerialDevice;

    return STATUS_SUCCESS;
}


ERROR_CODE_T BT12::GetDeviceVersion(weak_ptr<BTAVersionInfo_t> version)
{
    vector<string> retStrings;
    RETURN_EC_IF_NULL(ERROR_FAILED, m_pBTASerialDevice);
    RETURN_EC_IF_NULL(ERROR_FAILED, version.lock());

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

    // Assert ret strings first stringf contains IOT747
    if (retStrings[0].find(expectedDevice) == string::npos) {
        DebugPrintf(m_PacketVerbosity, DEBUG_NO_LOGGING, m_DebugID, "Expected to find %s in %s\n", expectedDevice.c_str(), retStrings[0].c_str());
        return ERROR_FAILED;
    }
    if (retStrings[1].find("MELODY AUDIO") != string::npos) {
        version.lock()->hardware = BTA_HW_BT12;
        version.lock()->BT12FwRev = BT12_FW_REV_UNKNOWN;
    } else {
        DebugPrintf(m_PacketVerbosity, DEBUG_NO_LOGGING, m_DebugID, "Expected to find %s in %s\n", expectedDevice.c_str(), retStrings[1].c_str());
        return ERROR_FAILED;
    }

    return ERROR_FAILED;
}