
#include "IDC777.h"

void IDC777::ParseVersionStrings(const vector<string> &retStrings)
{
    for (vector<string>::const_iterator it = retStrings.begin(); it != retStrings.end(); ++it)
    {
        string line = *it;
        if (line.find("IDC777") != string::npos && line.find("V") != string::npos)
        {
            m_BtFwVersion.hardware = BTA_HW_IDC777;
            m_BtFwVersion.versionString = line;

            // Extract version numbers after 'V'
            size_t vPos = line.find("V");
            if (vPos != string::npos)
            {
                continue;
            }

            string versionPart = line.substr(vPos + 1);
            size_t dot1 = versionPart.find('.');
            size_t dot2 = versionPart.find('.', dot1 + 1);

            if (dot1 == string::npos || dot2 == string::npos)
            {
                continue;
            }

            m_BtFwVersion.major = static_cast<INT8U>(stoi(versionPart.substr(0, dot1)));
            m_BtFwVersion.minor = static_cast<INT8U>(stoi(versionPart.substr(dot1 + 1, dot2 - dot1 - 1)));
            m_BtFwVersion.patch = static_cast<INT8U>(stoi(versionPart.substr(dot2 + 1)));
        }
        // make sure it starts with "Build:"
        else if (line.find("Build:") == 0)
        {
            m_BtFwVersion.buildNumber = (line.length() > 7) ? line.substr(7) : line;
        }
        else if (line.find("Bluetooth address") != string::npos)
        {
            m_BluetoothAddress = line.substr(line.find("Bluetooth address") + 18);
        }
    }
}

vector<BAUDRATE> IDC777::GetBaudrateList()
{
    vector<BAUDRATE> rates;
    rates.push_back(BAUDRATE_115200);
    return rates;
}

string IDC777::GetExpectedDigitalAudioParamsString()
{
    // Example: 0 44100 64 01100100 00000000

    // IDC7x7 Uart Command Page 54-55
    // 0 - I2S, 1-PCM
    string format = "0";

    // Valid rates, 8000, 16000, 32000, 44100, 48000
    string rate = "48000";

    // These next settings depend on if its 'in PCM or I2S. Our HW is only I2S
    // So those are the only ones described

    // <Param 1>
    // Rate Parameter - I2S Scaling Factor. Values 64, 128, 256
    string rateParam = "64";
    string param1 = rateParam;

    // <Param 2>
    // 00 - Slave, 01 - Master
    string masterOrSlave = "00";
    // Bits Per Sample. 10, 14, or 18
    string bitsPerSample = "14";
    // left justify bit delay. 00 - no, 01 - yes
    string leftJustifyBitDelay = "01";
    // enable 24-bit audio output. 00 - no,  01  -yes
    string enable24BitAudio = "00";
    string param2 = masterOrSlave + bitsPerSample + leftJustifyBitDelay + enable24BitAudio;

    // <Param 3>
    // Audio Atten Enable.  00 - no, 01 - yes
    string audioAttenEnable = "00";
    // Crop Enable.  00 - no, 01 - yes
    string cropEnable = "00";
    // TX Start Sample. 00 -  low clock phase, 01 - high clock phase
    string txStartSample = "00";
    // RX Start Sample. 00 - low clock phase, 01 - high clock phase
    string rxStartSample = "00";
    string param3 = audioAttenEnable + cropEnable + txStartSample + rxStartSample;

    return "" + format + " " + rate + " " + param1 + " " + param2 + " " + param3;
}

string IDC777::GetUniqueConfigExpectedString(UniqueConfigSettings_t configOption, bool *notImplemented)
{
    *notImplemented = false;

    switch (configOption)
    {
        case UNIQUE_CONFIG_SETTING_DIGITAL_AUDIO_PARAMS:
        {
            return GetExpectedDigitalAudioParamsString();
        }
        case UNIQUE_CONFIG_SETTING_AUTO_CONNECTION:
        {
            // 0 -  No Auto Connect, 1 - Auto Connect to all devices in paired device list
            // 3 - Number of times we try to auto connect. 1-7, 7 means unlimited
            return "0 3";
        }
        case UNIQUE_CONFIG_SETTTING_CODEC:
        {
            return string("") + // APTX Classic in Rx Mode A2DP Sink
                   "ON" + " " +
                   // APT HD Codec in Receive Mode - A2DP Sink
                   "ON" + " " +
                   // Enabled APTX Adaptive (Lossless for IDC777) in Rx Mode
                   "ON" + " " +
                   // Aptx Classic in Transmit
                   "ON" + " " +
                   // APTX HD Codec in Transmit Mode
                   "ON" + " " +
                   // Enabled APTX Adaptive (Lossless for IDC777) in Tx Mode
                   "ON";
        }
        case UNIQUE_CONFIG_SETTING_GPIO_CONFIG:
        {
            // See GPIO Functionality for more details.
            return "ON";
        }
        case UNIQUE_CONFIG_SETTING_UI_CONFIG:
        {
            // By Turning this On, we have the following behavior
            // Power On : LED 5, 4, and 2 turn sequentially twice for 100ms
            // Discoverable State - LED 2 flashes every 200ms
            return "ON";
        }
        case UNIQUE_CONFIG_SETTING_SHORT_NAME:
        {
            // Not sure why - This will be removed later for storing serial number, rev, and mod
            return "IA " + string(&m_PublicAddress[7]);
        }
        case UNIQUE_CONFIG_SETTING_PROFILES:
        {
            string multipleConnections = "OFF";
            string maxHfpConnections = "0";
            string maxAghfpConnections = "0";
            string maxA2dpSinkConnections = (m_DeviceMode == BTA_DEVICE_MODE_INPUT) ? "2" : "0";
            string maxA2dpSourceConnections = (m_DeviceMode == BTA_DEVICE_MODE_INPUT) ? "0" : "1";
            string maxAvrcpConnections = "0";
            string maxBleConnections = "0";
            string maxSppConnections = "0";

            return multipleConnections + " " +
                   maxHfpConnections + " " +
                   maxAghfpConnections + " " +
                   maxA2dpSinkConnections + " " +
                   maxA2dpSourceConnections + " " +
                   maxAvrcpConnections + " " +
                   maxBleConnections + " " +
                   maxSppConnections;
        }
        case UNIQUE_CONFIG_SETTINGS_BT_STATE:
        {
            return "0 0 0";
        }
        case UNIQUE_CONFIG_SETTING_LED_ENABLE:
        case UNIQUE_CONFIG_SETTING_DEVICE_ID:
        case UNIQUE_CONFIG_SETTING_BT_VOLUME:
        case UNIQUE_CONFIG_SETTING_VREG_ROLE:
        default:
        {
            *notImplemented = true;
            return "";
        }
    }
}

string IDC777::GetUniqueConfigSettingString(UniqueConfigSettings_t configOption, bool *notImplemented)
{
    if (notImplemented)
    {
        *notImplemented = false;
    }

    switch (configOption)
    {
        case UNIQUE_CONFIG_SETTINGS_BT_STATE:
            return "BT_CONFIG";
        default:
            *notImplemented = true;
            return "";
    }
}

ERROR_CODE_T IDC777::GetDeviceCfgRemoteAddress(string &remoteAddress)
{
    DebugPrintf(DEBUG_TRACE, DEBUG_TRACE, m_DebugId.c_str(), "Get Paired Device Function not Implemented\n");
    remoteAddress = "";
    return STATUS_SUCCESS;
}

ERROR_CODE_T IDC777::SetDeviceCfgRemoteAddress(string remoteAddress)
{
    // This is a placeholder function.
    return STATUS_SUCCESS;
}

ERROR_CODE_T IDC777::SetBluetoothDiscoverabilityState(bool connectable, bool discoverable)
{
    RETURN_IF_FAILED(m_pBTASerialDevice->WriteData("CONNECTABLE " + connectable ? "ON" : "OFF"));
    RETURN_IF_FAILED(m_pBTASerialDevice->WriteData("DISCOVERABLE " + discoverable ? "ON" : "OFF"));

    return STATUS_SUCCESS;
}

bool IDC777::ShouldScanForAllDevices()
{
    return m_ShouldScanForAllDevices;
}
