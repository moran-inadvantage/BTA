#include "BC127.h"

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

string BC127::GetUniqueConfigExpectedString(UniqueConfigSettings_t configOption, bool* notImplemented)
{
    // By default, assume it's implemented. It's the exception that it is not.
    *notImplemented = false;

    switch(configOption)
    {
        case UNIQUE_CONFIG_SETTING_DIGITAL_AUDIO_PARAMS: 
        {
            if (m_BtFwVersion.BC127FwRev == BC127_FW_REV_6_1_2)
            {
                return "0 48000 64 140300";
            }

            return "0 48000 64 140300 OFF";
            break;
        }
        case UNIQUE_CONFIG_SETTING_AUTO_CONNECTION:
        {
            // 0 -  No Auto Connect, 1 - Auto Connect to all devices in paired device list
            // 2 - Auto-connect to specific device in the REMOTE_ADDR config setting
            return "0";
            break;
        }
        case UNIQUE_CONFIG_SETTING_BT_VOLUME:
        {
            switch (m_BtFwVersion.BC127FwRev)
            {
                case BC127_FW_REV_6_1_2:
                    return "A A 10";
                case BC127_FW_REV_6_1_5:
                    return "A A 10 1";
                default:
                    // [0] - Default HFP Volume <Hex>
                    // [1] - Default A2DP Volume <0-100>
                    // [2] - Default A2DP Volume Steps <1-255>
                    // [3] - Volume Scaling Method <0 - HW, 1 - DSP>
                    return "A 70 10 1";
            }
            break;
        }
        case UNIQUE_CONFIG_SETTTING_CODEC:
        {
            // Bit 0 - ACC
            // Bit 1 - AptX
            // Bit 2 - AptX Low Latency
            // Bit 3 - AptX HD <Not Supported on our HW>
            // OFF - Disabled, ON - Enabled
            return "7 OFF";
            break;
        }
        case UNIQUE_CONFIG_SETTING_DEVICE_ID:
        {
            return "0001 0002 0003 0004 0005 0006 0007 0008";
            break;
        }
        case UNIQUE_CONFIG_SETTING_LED_ENABLE:
        {
            // I wonder what this does?
            return "ON";
            break;
        }
        case UNIQUE_CONFIG_SETTING_GPIO_CONFIG:
        {
            if (m_BtFwVersion.BC127FwRev < BC127_FW_REV_7_0)
            {
                return "ON 0 254";
            }
            
            return "ON 0 254 0";
            break;
        }
        case UNIQUE_CONFIG_SETTING_SHORT_NAME:
        {
            int indexIntoPublicAddress = 0;

            switch (m_BtFwVersion.BC127FwRev)
            {
                case BC127_FW_REV_6_1_2:
                case BC127_FW_REV_6_1_5:
                    indexIntoPublicAddress = 7;
                    break;
                case BC127_FW_REV_7_0:
                case BC127_FW_REV_7_1:
                case BC127_FW_REV_7_2:
                case BC127_FW_REV_7_3:
                default:
                    indexIntoPublicAddress = 6;
                    break;
            }

            if (m_PublicAddress.length() < indexIntoPublicAddress)
            {
                return "IA ";
            }

            return "IA " + string(&m_PublicAddress[indexIntoPublicAddress]);
            break;
        }
        case UNIQUE_CONFIG_SETTING_VREG_ROLE:
        {
            return "0";
            break;
        }
        case UNIQUE_CONFIG_SETTING_PROFILES:
        {
            string maxHfpConnections = "0";
            string maxAghfpConnections = "0";
            string maxA2dpSinkConnections = m_isInputDevice ? "2" : "0";
            string maxA2dpSourceConnections = m_isInputDevice ? "0" : "1";
            string maxAvrcpConnections = "0";
            string maxBleConnections = "0";
            string maxSppConnections = "0";
            string maxPbapConnections = "0";
            string maxHidDeviceConnections = "0";
            string maxHidHost = "0";
            string maxMapConnections = "0";
            string maxIapConnections = "0";

            return maxHfpConnections + " " +
                   maxAghfpConnections + " " +
                   maxA2dpSinkConnections + " " +
                   maxA2dpSourceConnections + " " +
                   maxAvrcpConnections + " " +
                   maxBleConnections + " " +
                   maxSppConnections + " " +
                   maxPbapConnections + " " +
                   maxHidDeviceConnections + " " +
                   maxHidHost + " " +
                   maxMapConnections + " " +
                   maxIapConnections;

            break;
        }
        case UNIQUE_CONFIG_SETTING_UI_CONFIG:
        default:
            *notImplemented = true;
            return "";
            break;
    }
}

string BC127::GetUniqueConfigSettingString(UniqueConfigSettings_t configOption, bool* notImplemented)
{
    if (notImplemented)
    {
        *notImplemented = false;
    }

    switch (configOption)
    {
        case UNIQUE_CONFIG_SETTINGS_BT_STATE:
        {
            switch (m_BtFwVersion.BC127FwRev)
            {
                case BC127_FW_REV_6_1_2:
                case BC127_FW_REV_6_1_5:
                    return "DISCOVERABLE";
                    break;
                case BC127_FW_REV_7_0:
                case BC127_FW_REV_7_1:
                case BC127_FW_REV_7_2:
                case BC127_FW_REV_7_3:
                default:
                    return "BT_STATE_CONFIG";
                    break;
            }
            break;
        }
        default:
        {
            *notImplemented = true;
            return "";
            break;
        }
    }
}

ERROR_CODE_T BC127::GetDeviceCfgPairedDevice()
{
    string remoteAddress;
    RETURN_IF_FAILED(m_pBTASerialDevice->GetCfgValue(remoteAddress, "REMOTE_ADDR"));

    // There is a bug where the stored value is corrupted. This makes the
    // REMOTE_ADDR useless for auto connecting to the given device, but it
    // can be used the store that address. We just need to modify the
    // returned address to return it correctly.
    if (m_BtFwVersion.BC127FwRev == BC127_FW_REV_7_2)
    {
        if (remoteAddress.size() < 14)
        {
            remoteAddress.insert(6, "000000000000", 14 - remoteAddress.size());
        }
        remoteAddress.insert(6, remoteAddress, 12, 2);
        m_PairedDevice = remoteAddress.substr(0, 12);
    }
    else
    {
        m_PairedDevice = remoteAddress;
    }

    if (m_PairedDevice == "000000000000")
    {
        m_PairedDevice = "";
    }

    return STATUS_SUCCESS;
}