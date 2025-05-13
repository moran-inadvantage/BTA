#include "IDC777.h"


IDC777::IDC777()
{

}

IDC777::~IDC777()
{

}


ERROR_CODE_T IDC777::SetAndOpenBtaSerialDevice(shared_ptr<BTASerialDevice> pBTASerialDevice)
{
    RETURN_EC_IF_NULL(ERROR_FAILED, pBTASerialDevice);

    // Default baudrate for IDC777
    RETURN_IF_FAILED(pBTASerialDevice->SetBaudrate(BAUDRATE_115200));

    m_pBTASerialDevice = pBTASerialDevice;

    return STATUS_SUCCESS;
}

ERROR_CODE_T IDC777::GetDeviceVersion(shared_ptr<BTAVersionInfo_t>& version)
{
    if (m_BtFwVersion.IsValid()) 
    {
        m_BtFwVersion.CopyInto(version);
        return STATUS_SUCCESS;
    }

    RETURN_EC_IF_NULL(ERROR_FAILED, m_pBTASerialDevice);

    /*
        Expected Output

        "IOT747 Copyright 2022"
        "AudioAgent IDC777 V3.1.21"
        "Build: 0400427b"
        "Bluetooth address 245DFC020196"
        "OK"
    */

    vector<string> retStrings;
    RETURN_IF_FAILED(m_pBTASerialDevice->ReadData(retStrings, "VERSION"));
    RETURN_EC_IF_TRUE(ERROR_FAILED, retStrings.size() < 4);

    ParseVersionStrings(retStrings);

    RETURN_EC_IF_TRUE(ERROR_FAILED,!m_BtFwVersion.IsValid());

    m_BtFwVersion.CopyInto(version);

    return STATUS_SUCCESS;
}

void IDC777::ParseVersionStrings(const vector<string>& retStrings)
{
    for (const auto& line : retStrings)
    {
        if (line.find("IDC777") != string::npos && line.find("V") != string::npos)
        {
            m_BtFwVersion.hardware = BTA_HW_IDC777;
            m_BtFwVersion.versionString = line;

            // Extract version numbers after 'V'
            size_t vPos = line.find("V");
            if (vPos != string::npos) 
            {
                string versionPart = line.substr(vPos + 1);
                size_t dot1 = versionPart.find('.');
                size_t dot2 = versionPart.find('.', dot1 + 1);

                if (dot1 != string::npos && dot2 != string::npos)
                {
                    m_BtFwVersion.major = static_cast<INT8U>(stoi(versionPart.substr(0, dot1)));
                    m_BtFwVersion.minor = static_cast<INT8U>(stoi(versionPart.substr(dot1 + 1, dot2 - dot1 - 1)));
                    m_BtFwVersion.patch = static_cast<INT8U>(stoi(versionPart.substr(dot2 + 1)));
                }
            }
        }
        // make sure it starts with "Build:"
        else if (line.find("Build:") == 0)
        { 
            m_BtFwVersion.buildNumber = (line.length() > 7) ? line.substr(7) : line; 
        } else if (line.find("Bluetooth address") != string::npos)
        {
            m_BluetoothAddress = line.substr(line.find("Bluetooth address") + 18);
        }
    }
}

BAUDRATE* IDC777::GetBaudrateList(INT32U* length)
{
    static BAUDRATE preferredBaudRates[] = { 
        BAUDRATE_115200,
    };

    *length = ARRAY_SIZE(preferredBaudRates);
    return preferredBaudRates;
}
