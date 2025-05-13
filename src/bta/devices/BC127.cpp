#include "BC127.h"

BC127::BC127()
{
    m_PacketVerbosity = 0;
}
BC127::~BC127()
{
    // Destructor implementation if needed
}

ERROR_CODE_T BC127::SetAndOpenBtaSerialDevice(shared_ptr<BTASerialDevice> pBTASerialDevice)
{
    RETURN_EC_IF_NULL(ERROR_FAILED, pBTASerialDevice);

    ERROR_CODE_T status = pBTASerialDevice->SetBaudrate(BAUDRATE_9600);
    RETURN_EC_IF_FAILED(status);

    m_pBTASerialDevice = pBTASerialDevice;

    return STATUS_SUCCESS;
}

ERROR_CODE_T BC127::GetDeviceVersion(shared_ptr<BTAVersionInfo_t>& version)
{
    if (m_versionInfo.get() != NULL) 
    {
        version = m_versionInfo;
        return STATUS_SUCCESS;
    }

    vector<string> retStrings;
    shared_ptr<BTAVersionInfo_t> localVersion = make_shared<BTAVersionInfo_t>();

    RETURN_EC_IF_NULL(ERROR_FAILED, m_pBTASerialDevice);

    RETURN_IF_FAILED(m_pBTASerialDevice->ReadData(retStrings, "VERSION"));
    RETURN_EC_IF_TRUE(ERROR_FAILED, retStrings.size() < 5);

    ParseVersionStrings(retStrings);

    RETURN_EC_IF_FALSE(ERROR_FAILED,!m_BtFwVersion.IsValid());

    m_BtFwVersion.CopyInto(version);

    return STATUS_SUCCESS;
}

void BC127::ParseVersionStrings(const vector<string>& retStrings)
{
    for (const auto& line : retStrings)
    {
        if (line.find("Melody Audio") != string::npos && line.find("V") != string::npos)
        {
            m_BtFwVersion.hardware = BTA_HW_BC127;
            m_BtFwVersion.versionString = line;

            size_t vPos = line.find("V");
            if (vPos != string::npos)
            {
                string versionPart = line.substr(vPos + 1);
                size_t dot1 = versionPart.find('.');
                size_t dot2 = versionPart.find('.', dot1 + 1);

                if (dot1 != string::npos)
                {
                    m_BtFwVersion.major = static_cast<INT8U>(stoi(versionPart.substr(0, dot1)));
                    if (dot2 != string::npos)
                    {
                        m_BtFwVersion.minor = static_cast<INT8U>(stoi(versionPart.substr(dot1 + 1, dot2 - dot1 - 1)));
                        m_BtFwVersion.patch = static_cast<INT8U>(stoi(versionPart.substr(dot2 + 1)));
                    }
                    else
                    {
                        m_BtFwVersion.minor = static_cast<INT8U>(stoi(versionPart.substr(dot1 + 1)));
                        m_BtFwVersion.patch = 0;
                    }
                }
            }
        }
        else if (line.find("Build:") == 0)
        {
            m_BtFwVersion.buildNumber = (line.length() > 7) ? line.substr(7) : line;
        }
        else if (line.find("Bluetooth addresses:") != string::npos)
        {
            size_t pos = line.find("Bluetooth addresses:");
            if (pos != string::npos)
            {
                string addresses = line.substr(pos + 21);
                size_t firstSpace = addresses.find(' ');
                m_BluetoothAddress = (firstSpace != string::npos)
                    ? addresses.substr(0, firstSpace)
                    : addresses;
            }
        }
    }
}

BAUDRATE* BC127::GetBaudrateList(INT32U* length)
{
    static BAUDRATE preferredBaudRates[] = { 
        BAUDRATE_9600,
        BAUDRATE_115200,
        BAUDRATE_57600,
        BAUDRATE_38400,
        BAUDRATE_19200,
    };

    *length = ARRAY_SIZE(preferredBaudRates);
    return preferredBaudRates;
}
