#include <memory>

#include "types.h"
#include "BTADeviceDriver.h"
#include "BTASerialDevice.h"

BTADeviceDriver::BTADeviceDriver()
{
    m_PacketVerbosity = 0;
    m_configuredSSPMode = SSP_NO_DISPLAY_NO_KEYBOARD;
}

BTADeviceDriver::~BTADeviceDriver()
{

}

void BTADeviceDriver::SetDeviceReadyForUse(bool isReady)
{
    if (isReady != m_deviceReadyForUse)
    {
        DebugPrintf(DEBUG_TRACE_INFO, DEBUG_TRACE_INFO, m_DebugID, "Device ready for use: %s\n", isReady ? "true" : "false");
        m_deviceReadyForUse = isReady;
    }
}

bool BTADeviceDriver::IsDeviceReadyForUse()
{
    return m_deviceReadyForUse;
}

ERROR_CODE_T BTADeviceDriver::SetAndOpenBtaSerialDevice(shared_ptr<BTASerialDevice> pBTASerialDevice)
{
    RETURN_EC_IF_NULL(ERROR_FAILED, pBTASerialDevice);

    BAUDRATE defaultBaudRate = GetDefaultBaudRate();

    ERROR_CODE_T status = pBTASerialDevice->SetBaudrate(defaultBaudRate);
    RETURN_EC_IF_FAILED(status);

    m_pBTASerialDevice = pBTASerialDevice;

    return STATUS_SUCCESS;
}

ERROR_CODE_T BTADeviceDriver::SetDeviceName(string deviceName)
{
    if (deviceName.compare(m_DeviceName) == 0)
    {
        DebugPrintf(DEBUG_TRACE_INFO, DEBUG_TRACE_INFO, m_DebugID, "Changing device name for %s to %s\n", m_DeviceName.c_str(), deviceName.c_str());
    }
    
    return STATUS_SUCCESS;
}

ERROR_CODE_T BTADeviceDriver::GetDeviceVersion(shared_ptr<BTAVersionInfo_t>& version)
{
    if (m_BtFwVersion.IsValid()) 
    {
        m_BtFwVersion.CopyInto(version);
        return STATUS_SUCCESS;
    }

    RETURN_EC_IF_NULL(ERROR_FAILED, m_pBTASerialDevice);

    vector<string> retStrings;

    RETURN_IF_FAILED(m_pBTASerialDevice->ReadData(retStrings, "VERSION"));

    RETURN_EC_IF_TRUE(ERROR_FAILED, retStrings.size() < 4);

    ParseVersionStrings(retStrings);

    RETURN_EC_IF_TRUE(ERROR_FAILED,!m_BtFwVersion.IsValid());

    m_BtFwVersion.CopyInto(version);

    return STATUS_SUCCESS;
}

ERROR_CODE_T BTADeviceDriver::EnterCommandMode()
{
    RETURN_EC_IF_NULL(ERROR_FAILED, m_pBTASerialDevice);

    // Will not get a response if we're already in data mode
    m_pBTASerialDevice->WriteData("$$$$");

    return STATUS_SUCCESS;
}

ERROR_CODE_T BTADeviceDriver::SendReset()
{
    RETURN_EC_IF_NULL(ERROR_FAILED, m_pBTASerialDevice);

    // Will not get a valid response, just resets us
    m_pBTASerialDevice->WriteData("RESET");

    RETURN_IF_FAILED(m_pBTASerialDevice->FlushRxBuffer(100));

    return STATUS_SUCCESS;
}

ERROR_CODE_T BTADeviceDriver::GetBaudrate(BAUDRATE &baudrate)
{
    RETURN_EC_IF_NULL(ERROR_FAILED, m_pBTASerialDevice);
    RETURN_IF_FAILED(m_pBTASerialDevice->GetBaudrate(baudrate));

    return STATUS_SUCCESS;
}

bool BTADeviceDriver::IsCommEnabled(void)
{
    return true;
}

ERROR_CODE_T BTADeviceDriver::WatchdogPet()
{
    RETURN_EC_IF_NULL(ERROR_FAILED, m_pBTASerialDevice);

    RETURN_IF_FAILED(m_pBTASerialDevice->PetBTAdapterWatchdog());

    return STATUS_SUCCESS;
}

BAUDRATE BTADeviceDriver::GetDefaultBaudRate()
{
    INT32U baudRateListLength;
    BAUDRATE* preferredBaudRates = GetBaudrateList(&baudRateListLength);
    return (baudRateListLength != 0) ? preferredBaudRates[0] : BAUDRATE_9600;
}

ERROR_CODE_T BTADeviceDriver::TryNextBaudrate()
{
    RETURN_EC_IF_NULL(ERROR_FAILED, m_pBTASerialDevice);

    INT32U baudRateListLength;
    BAUDRATE* preferredBaudRates = GetBaudrateList(&baudRateListLength);

    BAUDRATE baudrate;
    ERROR_CODE_T status = m_pBTASerialDevice->GetBaudrate(baudrate);
    RETURN_EC_IF_FAILED(status);

    for (int i = 0; i < baudRateListLength; i++)
    {
        if (preferredBaudRates[i] == baudrate)
        {
            if (i + 1 < baudRateListLength)
            {
                RETURN_EC_IF_FAILED(m_pBTASerialDevice->SetBaudrate(preferredBaudRates[i + 1]));
                return STATUS_SUCCESS;
            }
        }
    }

    return ERROR_FAILED;
}

void BTADeviceDriver::SetConfigWritePending(bool pending)
{
    if (pending != m_configWritePending)
    {
        DebugPrintf(DEBUG_TRACE_INFO, DEBUG_TRACE_INFO, m_DebugID, "Config write pending: %s\n", pending ? "true" : "false");
        m_configWritePending = pending;
    }
}

void BTADeviceDriver::SetDeviceDirection(bool isInputDevice)
{
    if (isInputDevice != m_isInputDevice)
    {
        DebugPrintf(DEBUG_TRACE_INFO, DEBUG_TRACE_INFO, m_DebugID, "Configuring device to be a %s\n", isInputDevice ? "input" : "output");
        m_isInputDevice = isInputDevice;
    }
}

void BTADeviceDriver::SetFlagUnpairAllDevices(bool unpair)
{
    if (m_needToUnpairAllDevices != unpair)
    {
        DebugPrintf(DEBUG_TRACE_INFO, DEBUG_TRACE_INFO, m_DebugID, "Unpair all devices: %s\n", unpair ? "true" : "false");
        m_needToUnpairAllDevices = unpair;
    }
}

ERROR_CODE_T BTADeviceDriver::GetDeviceCfgLocalAddress(void)
{
    string retString;

    RETURN_EC_IF_NULL(ERROR_FAILED, m_pBTASerialDevice);

    RETURN_IF_FAILED(m_pBTASerialDevice->GetCfgValue(retString, "LOCAL_ADDR"));

    // get first 12 bytes of ret string and put it input public address
    if (retString.length() < 12)
    {
        return ERROR_FAILED;
    }

    m_LocalAddress = retString.substr(0, 12);

    return STATUS_SUCCESS;
}

ERROR_CODE_T BTADeviceDriver::UnpairAllDevices()
{
    return STATUS_SUCCESS;
}


ERROR_CODE_T BTADeviceDriver::WriteConfigToFlash(void)
{
    if (!IsConfigWritePending())
    {
        return STATUS_SUCCESS;
    }

    BAUDRATE defaultRate = GetDefaultBaudRate();
    BAUDRATE currentRate;
    m_pBTASerialDevice->GetBaudrate(currentRate);

    RETURN_IF_FAILED(m_pBTASerialDevice->SetBaudrate(defaultRate));
    RETURN_IF_FAILED(m_pBTASerialDevice->WriteData("WRITE"));
    RETURN_IF_FAILED(m_pBTASerialDevice->SetBaudrate(currentRate));

    SetConfigWritePending(false);

    return STATUS_SUCCESS;
}


typedef enum
{
    BT_CONFIG_GET_VERSION,
    BT_CONFIG_GET_LOCAL_ADDR,
    BT_CONFIG_SET_AUDIO_MODE,
    BT_CONFIG_SET_DIGITAL_AUDIO_PARAMS,
    BT_CONFIG_SET_AUTOCONN,
    BT_CONFIG_SET_BT_VOLUME,
    BT_CONFIG_SET_COD,
    BT_CONFIG_SET_CODEC,
    BT_CONFIG_SET_DEVICE_ID,
    BT_CONFIG_SET_LED_ENABLE,
    BT_CONFIG_SET_GPIO_CONFIG,
    BT_CONFIG_SET_UI_CONFIG,
    BT_CONFIG_SET_PROFILES,
    BT_CONFIG_SET_VREG_ROLE,
    BT_CONFIG_SET_SSP_CAPS,
    BT_CONFIG_SET_BT_STATE,
    BT_CONFIG_SET_LONG_NAME,
    BT_CONFIG_SET_SHORT_NAME,
    BT_CONFIG_GET_PAIRED_DEVICE,
    BT_CONFIG_WRITE_CFG,
    BT_CONFIG_RESTART,
    BT_CONFIG_DONE,
    BT_STATE_NOT_USED,
} BluetoothConfigSetupStates_t;

static string StateToString(BluetoothConfigSetupStates_t state)
{
    switch (state) {
        case BT_CONFIG_GET_VERSION: return "BT_CONFIG_GET_VERSION";
        case BT_CONFIG_GET_LOCAL_ADDR: return "BT_CONFIG_GET_LOCAL_ADDR";
        case BT_CONFIG_SET_AUDIO_MODE: return "BT_CONFIG_SET_AUDIO_MODE";
        case BT_CONFIG_SET_DIGITAL_AUDIO_PARAMS: return "BT_CONFIG_SET_DIGITAL_AUDIO_PARAMS";
        case BT_CONFIG_SET_AUTOCONN: return "BT_CONFIG_SET_AUTOCONN";
        case BT_CONFIG_SET_BT_VOLUME: return "BT_CONFIG_SET_BT_VOLUME";
        case BT_CONFIG_SET_COD: return "BT_CONFIG_SET_COD";
        case BT_CONFIG_SET_CODEC: return "BT_CONFIG_SET_CODEC";
        case BT_CONFIG_SET_DEVICE_ID: return "BT_CONFIG_SET_DEVICE_ID";
        case BT_CONFIG_SET_LED_ENABLE: return "BT_CONFIG_SET_LED_ENABLE";
        case BT_CONFIG_SET_GPIO_CONFIG: return "BT_CONFIG_SET_GPIO_CONFIG";
        case BT_CONFIG_SET_UI_CONFIG: return "BT_CONFIG_SET_UI_CONFIG";
        case BT_CONFIG_SET_LONG_NAME: return "BT_CONFIG_SET_LONG_NAME";
        case BT_CONFIG_SET_SHORT_NAME: return "BT_CONFIG_SET_SHORT_NAME";
        case BT_CONFIG_GET_PAIRED_DEVICE: return "BT_CONFIG_GET_PAIRED_DEVICE";
        case BT_CONFIG_SET_PROFILES: return "BT_CONFIG_SET_PROFILES";
        case BT_CONFIG_SET_VREG_ROLE: return "BT_CONFIG_SET_VREG_ROLE";
        case BT_CONFIG_SET_SSP_CAPS: return "BT_CONFIG_SET_SSP_CAPS";
        case BT_CONFIG_SET_BT_STATE: return "BT_CONFIG_SET_BT_STATE";
        case BT_CONFIG_WRITE_CFG: return "BT_CONFIG_WRITE_CFG";
        case BT_CONFIG_RESTART: return "BT_CONFIG_RESTART";
        case BT_CONFIG_DONE: return "BT_CONFIG_DONE";
        default:
            break;
    }
    return "STATE NOT FOUND";
}

static void changeBluetoothConfigSetupState(
    BluetoothConfigSetupStates_t& state,
    BluetoothConfigSetupStates_t desiredState = BT_STATE_NOT_USED
)
{
    BluetoothConfigSetupStates_t newState = (desiredState == BT_STATE_NOT_USED) ?
        static_cast<BluetoothConfigSetupStates_t>(static_cast<int>(state) + 1) : 
        desiredState;

    DebugPrintf(DEBUG_TRACE_DEBUG, DEBUG_TRACE_DEBUG, m_debugId, "Moving from %s to %s\n", StateToString(state).c_str(), StateToString(newState).c_str());

    state = newState;
}

ERROR_CODE_T BTADeviceDriver::InitializeDeviceConfiguration()
{
    BluetoothConfigSetupStates_t state = BT_CONFIG_GET_VERSION;

    while (state != BT_CONFIG_DONE)
    {
        switch (state) {
            case BT_CONFIG_GET_VERSION:
            {
                shared_ptr<BTAVersionInfo_t> versionInfo = make_shared<BTAVersionInfo_t>();
                RETURN_EC_IF_FAILED(GetDeviceVersion(versionInfo));
                changeBluetoothConfigSetupState(state);
                break;
            }
            case BT_CONFIG_GET_LOCAL_ADDR:
            {
                RETURN_IF_FAILED(GetDeviceCfgLocalAddress());
                changeBluetoothConfigSetupState(state);
                break;
            }
            case BT_CONFIG_SET_AUDIO_MODE:
            {
                RETURN_IF_FAILED(SetDeviceCfgDigitalAudioMode());
                changeBluetoothConfigSetupState(state);
                break;
            }
            case BT_CONFIG_SET_DIGITAL_AUDIO_PARAMS:
            {
                RETURN_IF_FAILED(SetDeviceCfgDigitalAudioParams());
                changeBluetoothConfigSetupState(state);
                break;
            }
            case BT_CONFIG_SET_AUTOCONN:
            {
                RETURN_EC_IF_FAILED(SetDeviceCfgAutoConnect());
                changeBluetoothConfigSetupState(state);
                break;
            }
            case BT_CONFIG_SET_BT_VOLUME:
            {
                RETURN_EC_IF_FAILED(SetDeviceCfgVolume());
                changeBluetoothConfigSetupState(state);
                break;
            }
            case BT_CONFIG_SET_COD:
            {
                RETURN_EC_IF_FAILED(SetDeviceCfgCOD());
                changeBluetoothConfigSetupState(state);
                break;
            }
            case BT_CONFIG_SET_CODEC:
            {
                RETURN_EC_IF_FAILED(SetDeviceCfgCodecs());
                changeBluetoothConfigSetupState(state);
                break;
            }
            case BT_CONFIG_SET_DEVICE_ID:
            {
                RETURN_EC_IF_FAILED(SetDeviceCfgDeviceId());
                changeBluetoothConfigSetupState(state);
                break;
            }
            case BT_CONFIG_SET_LED_ENABLE:
            {
                RETURN_EC_IF_FAILED(SetDeviceCfgLedEnable());
                changeBluetoothConfigSetupState(state);
                break;
            }
            case BT_CONFIG_SET_GPIO_CONFIG:
            {
                RETURN_EC_IF_FAILED(SetDeviceCfgGpioConfig());
                changeBluetoothConfigSetupState(state);
                break;
            }
            case BT_CONFIG_SET_UI_CONFIG:
            {
                RETURN_EC_IF_FAILED(SetDeviceCfgUiConfig());
                changeBluetoothConfigSetupState(state);
                break;
            }
            case BT_CONFIG_SET_PROFILES:
            {
                RETURN_EC_IF_FAILED(SetDeviceCfgProfiles());
                changeBluetoothConfigSetupState(state);
                break;
            }
            case BT_CONFIG_SET_VREG_ROLE:
            {
                RETURN_EC_IF_FAILED(SetDeviceCfgVregRole());
                changeBluetoothConfigSetupState(state);
                break;
            }
            case BT_CONFIG_SET_SSP_CAPS:
            {
                RETURN_EC_IF_FAILED(SetDeviceCfgSspCaps());
                changeBluetoothConfigSetupState(state);
                break;
            }
            case BT_CONFIG_SET_BT_STATE:
            {
                RETURN_EC_IF_FAILED(SetDeviceCfgBluetoothState());
                changeBluetoothConfigSetupState(state);
                break;
            }
            case BT_CONFIG_SET_LONG_NAME:
            {
                RETURN_EC_IF_FAILED(SetDeviceCfgLongName());
                changeBluetoothConfigSetupState(state);
                break;
            }
            case BT_CONFIG_SET_SHORT_NAME:
            {
                RETURN_EC_IF_FAILED(SetDeviceCfgShortName());
                changeBluetoothConfigSetupState(state);
                break;
            }
            case BT_CONFIG_GET_PAIRED_DEVICE:
            {
                RETURN_EC_IF_FAILED(GetDeviceCfgPairedDevice());
                changeBluetoothConfigSetupState(state);
                break;
            }
            case BT_CONFIG_WRITE_CFG:
            {
                if (IsConfigWritePending())
                {
                    RETURN_EC_IF_FAILED(WriteConfigToFlash());
                    changeBluetoothConfigSetupState(state);
                } else
                {
                    changeBluetoothConfigSetupState(state, BT_CONFIG_DONE);
                }

                break;
            }
            case BT_CONFIG_RESTART:
            {
                changeBluetoothConfigSetupState(state, BT_CONFIG_GET_VERSION);
                break;
            }
            default:
            {
                printf("Unhandled state: %s\n", StateToString(state).c_str());
                return ERROR_FAILED;
            }
        }
    }

    SetDeviceReadyForUse(true);

    return STATUS_SUCCESS;
}

ERROR_CODE_T BTADeviceDriver::VerifyConfigSetting(UniqueConfigSettings_t setting, bool* optionWasSet)
{
    bool configNotImplemented = false;
    string settingString = GetUniqueConfigSettingString(setting, &configNotImplemented);
    return VerifyConfigSetting(settingString, setting, optionWasSet);
}

ERROR_CODE_T BTADeviceDriver::VerifyConfigSetting(string configSetting, UniqueConfigSettings_t setting, bool* optionWasSet)
{
    bool configNotImplemented = false;

    // In case we return early, or we don't set the option
    // we want to make sure we set this to false
    if (optionWasSet)
    {
        *optionWasSet = false;
    }

    string expectedString = GetUniqueConfigExpectedString(setting, &configNotImplemented);

    // Not all hardware supports all config settings
    if (configNotImplemented)
    {
        return STATUS_SUCCESS;
    }

    return VerifyConfigSetting(configSetting, expectedString, optionWasSet);
}

ERROR_CODE_T BTADeviceDriver::VerifyConfigSetting(string configSetting, string expectedResult, bool* optionWasSet)
{
    string retString;
    // In case we return early, or we don't set the option
    // we want to make sure we set this to false
    if (optionWasSet)
    {
        *optionWasSet = false;
    }

    RETURN_EC_IF_NULL(ERROR_FAILED, m_pBTASerialDevice);

    RETURN_IF_FAILED(m_pBTASerialDevice->GetCfgValue(retString, configSetting));
    
    if (retString.find(expectedResult) == string::npos)
    {
        m_pBTASerialDevice->SetCfgValue(configSetting, expectedResult);
        SetConfigWritePending(true);
        if (optionWasSet)
        {
            *optionWasSet = true;
        }
    }

    // Verify we set it correctly
    RETURN_IF_FAILED(m_pBTASerialDevice->GetCfgValue(retString, configSetting));

    return (retString.find(expectedResult) != string::npos) ? STATUS_SUCCESS : ERROR_FAILED;
}


ERROR_CODE_T BTADeviceDriver::SetDeviceCfgDigitalAudioMode()
{
    return VerifyConfigSetting("AUDIO", "1 1");
}

ERROR_CODE_T BTADeviceDriver::SetDeviceCfgDigitalAudioParams()
{
    return VerifyConfigSetting("AUDIO_DIGITAL", UNIQUE_CONFIG_SETTING_DIGITAL_AUDIO_PARAMS);
}

ERROR_CODE_T BTADeviceDriver::SetDeviceCfgAutoConnect()
{
    return VerifyConfigSetting("AUTOCONN", UNIQUE_CONFIG_SETTING_AUTO_CONNECTION);
}

ERROR_CODE_T BTADeviceDriver::SetDeviceCfgVolume()
{
    return VerifyConfigSetting("BT_VOL_CONFIG", UNIQUE_CONFIG_SETTING_BT_VOLUME);
}

ERROR_CODE_T BTADeviceDriver::SetDeviceCfgCOD()
{
    string cod = m_isInputDevice ? "240414" : "20041C";
    return VerifyConfigSetting("COD", cod);
}

ERROR_CODE_T BTADeviceDriver::SetDeviceCfgCodecs()
{
    return VerifyConfigSetting("CODEC", UNIQUE_CONFIG_SETTING_BT_VOLUME);
}

ERROR_CODE_T BTADeviceDriver::SetDeviceCfgDeviceId()
{
    return VerifyConfigSetting("DEVICE_ID", UNIQUE_CONFIG_SETTING_DEVICE_ID);
}

ERROR_CODE_T BTADeviceDriver::SetDeviceCfgLedEnable()
{
    return VerifyConfigSetting("ENABLE_LED", UNIQUE_CONFIG_SETTING_LED_ENABLE);
}

ERROR_CODE_T BTADeviceDriver::SetDeviceCfgGpioConfig()
{
    return VerifyConfigSetting("GPIO_CONFIG", UNIQUE_CONFIG_SETTING_DEVICE_ID);
}

ERROR_CODE_T BTADeviceDriver::SetDeviceCfgUiConfig()
{
    return VerifyConfigSetting("UI_CONFIG", UNIQUE_CONFIG_SETTING_UI_CONFIG);
}

ERROR_CODE_T BTADeviceDriver::SetDeviceCfgLongName()
{
    return VerifyConfigSetting("NAME", m_DeviceName);
}

ERROR_CODE_T BTADeviceDriver::SetDeviceCfgShortName()
{
    return VerifyConfigSetting("NAME_SHORT", UNIQUE_CONFIG_SETTING_SHORT_NAME);
}

ERROR_CODE_T BTADeviceDriver::SetDeviceCfgVregRole()
{
    return VerifyConfigSetting("VREG_ROLE", UNIQUE_CONFIG_SETTING_VREG_ROLE);
}

ERROR_CODE_T BTADeviceDriver::SetDeviceCfgProfiles()
{
    bool optionWasSet = false;

    ERROR_CODE_T ret = VerifyConfigSetting("PROFILES", UNIQUE_CONFIG_SETTING_PROFILES, &optionWasSet);

    if (optionWasSet)
    {
        DebugPrintf(DEBUG_TRACE_INFO, DEBUG_TRACE_INFO, m_DebugID, "Device profiles changed. Flagging to unpair all devices");
        SetFlagUnpairAllDevices(true);
    }

    return ret;
}

ERROR_CODE_T BTADeviceDriver::SetDeviceCfgSspCaps()
{
    return VerifyConfigSetting("SSP_CAPS", "" + static_cast<int>(m_configuredSSPMode));
}

ERROR_CODE_T BTADeviceDriver::SetDeviceCfgBluetoothState()
{
    return VerifyConfigSetting("BT_STATE", UNIQUE_CONFIG_SETTINGS_BT_STATE);
}