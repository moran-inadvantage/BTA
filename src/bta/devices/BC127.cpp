#include "BC127.h"

#include <string>

void BC127::ParseVersionStrings(const vector<string> &retStrings)
{
    for (vector<string>::const_iterator it = retStrings.begin(); it != retStrings.end(); ++it)
    {
        string line = *it;
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

vector<BAUDRATE> BC127::GetBaudrateList()
{
    vector<BAUDRATE> rates;
    rates.push_back(BAUDRATE_9600);
#ifndef __x86_64__
    rates.push_back(BAUDRATE_115200);
    rates.push_back(BAUDRATE_57600);
    rates.push_back(BAUDRATE_38400);
    rates.push_back(BAUDRATE_19200);
#endif
    return rates;
}

string BC127::GetUniqueConfigExpectedString(UniqueConfigSettings_t configOption, bool *notImplemented)
{
    // By default, assume it's implemented. It's the exception that it is not.
    *notImplemented = false;

    switch (configOption)
    {
        case UNIQUE_CONFIG_SETTING_DIGITAL_AUDIO_PARAMS:
        {
            if (m_BtFwVersion.BC127FwRev == BC127_FW_REV_6_1_2)
            {
                return "0 48000 64 140300";
            }

            return "0 48000 64 140300 OFF";
        }
        case UNIQUE_CONFIG_SETTING_AUTO_CONNECTION:
        {
            // 0 -  No Auto Connect, 1 - Auto Connect to all devices in paired device list
            // 2 - Auto-connect to specific device in the REMOTE_ADDR config setting
            return "0";
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
        }
        case UNIQUE_CONFIG_SETTTING_CODEC:
        {
            // Bit 0 - ACC
            // Bit 1 - AptX
            // Bit 2 - AptX Low Latency
            // Bit 3 - AptX HD <Not Supported on our HW>
            // OFF - Disabled, ON - Enabled
            return "7 OFF";
        }
        case UNIQUE_CONFIG_SETTING_DEVICE_ID:
        {
            return "0001 0002 0003 0004 0005 0006 0007 0008";
        }
        case UNIQUE_CONFIG_SETTING_LED_ENABLE:
        {
            // I wonder what this does?
            return "ON";
        }
        case UNIQUE_CONFIG_SETTING_GPIO_CONFIG:
        {
            if (m_BtFwVersion.BC127FwRev < BC127_FW_REV_7_0)
            {
                return "ON 0 254";
            }

            return "ON 0 254 0";
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
        }
        case UNIQUE_CONFIG_SETTING_VREG_ROLE:
        {
            return "0";
        }
        case UNIQUE_CONFIG_SETTING_PROFILES:
        {
            string maxHfpConnections = "0";
            string maxAghfpConnections = "0";
            string maxA2dpSinkConnections = (m_DeviceMode == BTA_DEVICE_MODE_INPUT) ? "2" : "0";
            string maxA2dpSourceConnections = (m_DeviceMode == BTA_DEVICE_MODE_INPUT) ? "0" : "1";
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
        }
        case UNIQUE_CONFIG_SETTING_UI_CONFIG:
        default:
            *notImplemented = true;
            return "";
    }
}

string BC127::GetUniqueConfigSettingString(UniqueConfigSettings_t configOption, bool *notImplemented)
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
                case BC127_FW_REV_7_0:
                case BC127_FW_REV_7_1:
                case BC127_FW_REV_7_2:
                case BC127_FW_REV_7_3:
                default:
                    return "BT_STATE_CONFIG";
            }
        }
        default:
        {
            *notImplemented = true;
            return "";
        }
    }
}

ERROR_CODE_T BC127::GetDeviceCfgRemoteAddress(string &remote)
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
        remote = remoteAddress.substr(0, 12);
    }
    else
    {
        remote = remoteAddress;
    }

    if (remote == "000000000000")
    {
        remote = "";
    }

    return STATUS_SUCCESS;
}

ERROR_CODE_T BC127::SetDeviceCfgRemoteAddress(string remoteAddress)
{
    m_pBTASerialDevice->SetCfgValue("REMOTE_ADDR", remoteAddress);

    return STATUS_SUCCESS;
}

ERROR_CODE_T BC127::SetBluetoothDiscoverabilityState(bool connectable, bool discoverable)
{
    if (m_BtFwVersion.BC127FwRev <= BC127_FW_REV_6_1_5)
    {
        RETURN_IF_FAILED(m_pBTASerialDevice->WriteData("CONNECTABLE " + connectable ? "ON" : "OFF"));
        RETURN_IF_FAILED(m_pBTASerialDevice->WriteData("DISCOVERABLE " + discoverable ? "ON" : "OFF"));
    }
    else
    {
        string connect = connectable ? "ON" : "OFF";
        string discover = discoverable ? "ON" : "OFF";
        RETURN_IF_FAILED(m_pBTASerialDevice->WriteData("BT_STATE " + connect + " " + discover));
    }
    return STATUS_SUCCESS;
}

bool BC127::ShouldScanForAllDevices()
{
    if (m_BtFwVersion.BC127FwRev < BC127_FW_REV_7_0)
    {
        return true;
    }

    return m_ShouldScanForAllDevices;
}
