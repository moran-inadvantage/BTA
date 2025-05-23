#include "BTADeviceDriver.h"
#include "BTASerialDevice.h"
#include "ExtIO.h"
#include "types.h"

IBTADeviceDriver::IBTADeviceDriver()
{
    m_PacketVerbosity = 0;
    m_configuredSSPMode = SSP_NO_DISPLAY_NO_KEYBOARD;
}

void IBTADeviceDriver::SetDeviceReadyForUse(bool isReady)
{
    if (isReady != m_deviceReadyForUse)
    {
        DebugPrintf(DEBUG_TRACE, DEBUG_TRACE, m_DebugId.c_str(), "Device ready for use: %s\n", isReady ? "true" : "false");
        m_deviceReadyForUse = isReady;
    }
}

bool IBTADeviceDriver::IsDeviceReadyForUse()
{
    return m_deviceReadyForUse;
}

ERROR_CODE_T IBTADeviceDriver::SetAndOpenBtaSerialDevice(shared_ptr<CBTASerialDevice> pBTASerialDevice)
{
    RETURN_EC_IF_NULL(ERROR_FAILED, pBTASerialDevice);

    BAUDRATE defaultBaudRate = GetDefaultBaudRate();

    RETURN_IF_FAILED(pBTASerialDevice->SetBaudrate(defaultBaudRate));

    m_pBTASerialDevice = pBTASerialDevice;

    // Create the pairing manager now
    m_PairingManager = make_shared<CBTAPairingManager>(m_pBTASerialDevice);
    RETURN_EC_IF_NULL(ERROR_FAILED, m_PairingManager);

    m_pBTASerialDevice->OnOpenNotificationReceived.registerObserver<CBTAPairingManager>(
        m_PairingManager,
        &CBTAPairingManager::OnOpenNotificationReceived);

    return STATUS_SUCCESS;
}

ERROR_CODE_T IBTADeviceDriver::SetDeviceName(string deviceName)
{
    if (deviceName.compare(m_DeviceName) == 0)
    {
        DebugPrintf(DEBUG_TRACE, DEBUG_TRACE, m_DebugId.c_str(), "Changing device name for %s to %s\n", m_DeviceName.c_str(), deviceName.c_str());
    }

    return STATUS_SUCCESS;
}

ERROR_CODE_T IBTADeviceDriver::GetDeviceVersion(shared_ptr<CBTAVersionInfo_t> &version)
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

    RETURN_EC_IF_TRUE(ERROR_FAILED, !m_BtFwVersion.IsValid());

    m_BtFwVersion.CopyInto(version);

    isCommEnabled = true;

    return STATUS_SUCCESS;
}

ERROR_CODE_T IBTADeviceDriver::ResetAndEstablishSerialConnection(shared_ptr<CBTASerialDevice> pBtaSerialDevice)
{
    if (pBtaSerialDevice != NULL)
    {
        RETURN_IF_FAILED(SetAndOpenBtaSerialDevice(pBtaSerialDevice));
    }

    return ResetAndEstablishSerialConnection();
}

ERROR_CODE_T IBTADeviceDriver::ResetAndEstablishSerialConnection()
{
    shared_ptr<CBTAVersionInfo_t> versionInfo = make_shared<CBTAVersionInfo_t>();

    // Try various baud rates until we find the correct one.
    // Even if we find the right one, may not be our device
    do
    {
        BAUDRATE rate;
        GetBaudrate(rate);
        DebugPrintf(DEBUG_TRACE, DEBUG_TRACE, m_DebugId.c_str(), "Trying next baud rate: %d\n", rate);
        // These can fail at bad baud rates - don't bother error checking
        EnterCommandMode();
        SendReset();

        // Expected to fail if baud rate is incorrect
        if (FAILED(GetDeviceVersion(versionInfo)))
        {
            continue;
        }

        if (versionInfo->hardware != BTA_HW_UNKNOWN)
        {
            return STATUS_SUCCESS;
        }
    } while (SUCCEEDED(TryNextBaudrate()));

    return ERROR_FAILED;
}

ERROR_CODE_T IBTADeviceDriver::EnterCommandMode()
{
    RETURN_EC_IF_NULL(ERROR_FAILED, m_pBTASerialDevice);

    // Will not get a response if we're already in data mode
    m_pBTASerialDevice->WriteData("$$$$");

    return STATUS_SUCCESS;
}

ERROR_CODE_T IBTADeviceDriver::SendReset()
{
    RETURN_EC_IF_NULL(ERROR_FAILED, m_pBTASerialDevice);

    // Will not get a valid response, just resets us
    m_pBTASerialDevice->WriteData("RESET");

    RETURN_IF_FAILED(m_pBTASerialDevice->FlushRxBuffer(100));

    return STATUS_SUCCESS;
}

ERROR_CODE_T IBTADeviceDriver::GetBaudrate(BAUDRATE &baudrate)
{
    RETURN_EC_IF_NULL(ERROR_FAILED, m_pBTASerialDevice);
    RETURN_IF_FAILED(m_pBTASerialDevice->GetBaudrate(baudrate));

    return STATUS_SUCCESS;
}

bool IBTADeviceDriver::IsCommEnabled(void)
{
    return m_pBTASerialDevice->IsCommEnabled();
}

ERROR_CODE_T IBTADeviceDriver::UnpairAllDevices()
{
    ERROR_CODE_T status = m_PairingManager->UnpairAllDevices();

    RETURN_IF_FAILED(SetDeviceCfgRemoteAddress(""));
    RETURN_IF_FAILED(WriteConfigToFlash());

    return status;
}

ERROR_CODE_T IBTADeviceDriver::SendPlayCommand(string linkId)
{
    RETURN_EC_IF_FALSE(ERROR_NOT_INITIALIZED, m_pBTASerialDevice->IsCommEnabled());
    RETURN_IF_FAILED(m_pBTASerialDevice->WriteData("MUSIC " + linkId + " PLAY"));
    return STATUS_SUCCESS;
}

ERROR_CODE_T IBTADeviceDriver::WatchdogPet(bool flushBuffer)
{
    RETURN_EC_IF_NULL(ERROR_FAILED, m_pBTASerialDevice);

    RETURN_IF_FAILED(m_pBTASerialDevice->PetBTAdapterWatchdog(flushBuffer));

    return STATUS_SUCCESS;
}

BAUDRATE IBTADeviceDriver::GetDefaultBaudRate()
{
    vector<BAUDRATE> preferredBaudRates = GetBaudrateList();
    return (preferredBaudRates.size() != 0) ? preferredBaudRates[0] : BAUDRATE_9600;
}

ERROR_CODE_T IBTADeviceDriver::TryNextBaudrate()
{
    RETURN_EC_IF_NULL(ERROR_FAILED, m_pBTASerialDevice);

    vector<BAUDRATE> preferredBaudRates = GetBaudrateList();
    INT32U baudRateListLength = preferredBaudRates.size();

    BAUDRATE baudrate = BAUDRATE_UNKNOWN;
    // Ignores a false warning of unreferenced variable?
    (void)baudrate;
    RETURN_IF_FAILED(m_pBTASerialDevice->GetBaudrate(baudrate));

    for (int i = 0; i < baudRateListLength; i++)
    {
        if (preferredBaudRates[i] == baudrate)
        {
            if (i + 1 < baudRateListLength)
            {
                BAUDRATE nextRate = preferredBaudRates[i + 1];
                RETURN_IF_FAILED(m_pBTASerialDevice->SetBaudrate(nextRate));
                return STATUS_SUCCESS;
            }
        }
    }

    return ERROR_FAILED;
}

ERROR_CODE_T IBTADeviceDriver::SendInquiry(INT32U timeout)
{
    RETURN_EC_IF_TRUE(ERROR_INVALID_PARAMETER, timeout < 1 || 48 < timeout);
    string inqueryCommand = "INQUIRY " + to_string(timeout);
    RETURN_IF_FAILED(m_pBTASerialDevice->WriteData(inqueryCommand));
    return STATUS_SUCCESS;
}

void IBTADeviceDriver::SetConfigWritePending(bool pending)
{
    if (pending != m_configWritePending)
    {
        DebugPrintf(DEBUG_TRACE, DEBUG_TRACE, m_DebugId.c_str(), "Config write pending: %s\n", pending ? "true" : "false");
        m_configWritePending = pending;
    }
}

string IBTADeviceDriver::DeviceModeToString(BTADeviceMode_t mode)
{
    switch (mode)
    {
        case BTA_DEVICE_MODE_INPUT:
            return "Input";
        case BTA_DEVICE_MODE_OUTPUT:
            return "Output";
        default:
            return "Unknown";
    }
}

void IBTADeviceDriver::SetDeviceMode(BTADeviceMode_t mode)
{
    if (mode != m_DeviceMode)
    {
        DebugPrintf(DEBUG_TRACE, DEBUG_TRACE, m_DebugId.c_str(), "Switching device mode from %s to %s\n",
                    DeviceModeToString(m_DeviceMode).c_str(),
                    DeviceModeToString(mode).c_str());
        m_DeviceMode = mode;
        m_PairingManager->SetDeviceMode(m_DeviceMode);
    }
}

void IBTADeviceDriver::SetFlagUnpairAllDevices(bool unpair)
{
    if (m_needToUnpairAllDevices != unpair)
    {
        DebugPrintf(DEBUG_TRACE, DEBUG_TRACE, m_DebugId.c_str(), "Unpair all devices: %s\n", unpair ? "true" : "false");
        m_needToUnpairAllDevices = unpair;
    }
}

ERROR_CODE_T IBTADeviceDriver::GetDeviceCfgLocalAddress(void)
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

ERROR_CODE_T IBTADeviceDriver::WriteConfigToFlash(void)
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

string IBTADeviceDriver::StateToString(int state)
{
    switch (static_cast<BluetoothConfigSetupStates_t>(state))
    {
        case BT_CONFIG_GET_VERSION:
            return "BT_CONFIG_GET_VERSION";
        case BT_CONFIG_GET_LOCAL_ADDR:
            return "BT_CONFIG_GET_LOCAL_ADDR";
        case BT_CONFIG_SET_AUDIO_MODE:
            return "BT_CONFIG_SET_AUDIO_MODE";
        case BT_CONFIG_SET_DIGITAL_AUDIO_PARAMS:
            return "BT_CONFIG_SET_DIGITAL_AUDIO_PARAMS";
        case BT_CONFIG_SET_AUTOCONN:
            return "BT_CONFIG_SET_AUTOCONN";
        case BT_CONFIG_SET_BT_VOLUME:
            return "BT_CONFIG_SET_BT_VOLUME";
        case BT_CONFIG_SET_COD:
            return "BT_CONFIG_SET_COD";
        case BT_CONFIG_SET_CODEC:
            return "BT_CONFIG_SET_CODEC";
        case BT_CONFIG_SET_DEVICE_ID:
            return "BT_CONFIG_SET_DEVICE_ID";
        case BT_CONFIG_SET_LED_ENABLE:
            return "BT_CONFIG_SET_LED_ENABLE";
        case BT_CONFIG_SET_GPIO_CONFIG:
            return "BT_CONFIG_SET_GPIO_CONFIG";
        case BT_CONFIG_SET_UI_CONFIG:
            return "BT_CONFIG_SET_UI_CONFIG";
        case BT_CONFIG_SET_PROFILES:
            return "BT_CONFIG_SET_PROFILES";
        case BT_CONFIG_SET_VREG_ROLE:
            return "BT_CONFIG_SET_VREG_ROLE";
        case BT_CONFIG_SET_SSP_CAPS:
            return "BT_CONFIG_SET_SSP_CAPS";
        case BT_CONFIG_SET_BT_STATE:
            return "BT_CONFIG_SET_BT_STATE";
        case BT_CONFIG_SET_LONG_NAME:
            return "BT_CONFIG_SET_LONG_NAME";
        case BT_CONFIG_SET_SHORT_NAME:
            return "BT_CONFIG_SET_SHORT_NAME";
        case BT_CONFIG_GET_PAIRED_DEVICE:
            return "BT_CONFIG_GET_PAIRED_DEVICE";
        case BT_CONFIG_WRITE_CFG:
            return "BT_CONFIG_WRITE_CFG";
        case BT_CONFIG_RESTART:
            return "BT_CONFIG_RESTART";
        case BT_CONFIG_DONE:
            return "BT_CONFIG_DONE";
        default:
            return "UNKNOWN_STATE";
    }
}

void IBTADeviceDriver::ChangeBluetoothConfigSetupState(int &state, int desiredState = BT_STATE_NOT_USED)
{
    BluetoothConfigSetupStates_t newState = (desiredState == BT_STATE_NOT_USED)
                                                ? static_cast<BluetoothConfigSetupStates_t>(state + 1)
                                                : static_cast<BluetoothConfigSetupStates_t>(desiredState);

    DebugPrintf(DEBUG_TRACE_MESSAGE, DEBUG_TRACE_MESSAGE, m_DebugId.c_str(),
                "Moving from %s to %s\n", StateToString(state).c_str(), StateToString(newState).c_str());

    state = newState;
}

ERROR_CODE_T IBTADeviceDriver::InitializeDeviceConfiguration()
{
    int state = BT_CONFIG_GET_VERSION;

    while (state != BT_CONFIG_DONE)
    {
        switch (state)
        {
            case BT_CONFIG_GET_VERSION:
            {
                shared_ptr<CBTAVersionInfo_t> versionInfo = make_shared<CBTAVersionInfo_t>();
                RETURN_IF_FAILED(GetDeviceVersion(versionInfo));
                ChangeBluetoothConfigSetupState(state);
                break;
            }
            case BT_CONFIG_GET_LOCAL_ADDR:
            {
                RETURN_IF_FAILED(GetDeviceCfgLocalAddress());
                ChangeBluetoothConfigSetupState(state);
                break;
            }
            case BT_CONFIG_SET_AUDIO_MODE:
            {
                RETURN_IF_FAILED(SetDeviceCfgDigitalAudioMode());
                ChangeBluetoothConfigSetupState(state);
                break;
            }
            case BT_CONFIG_SET_DIGITAL_AUDIO_PARAMS:
            {
                RETURN_IF_FAILED(SetDeviceCfgDigitalAudioParams());
                ChangeBluetoothConfigSetupState(state);
                break;
            }
            case BT_CONFIG_SET_AUTOCONN:
            {
                RETURN_IF_FAILED(SetDeviceCfgAutoConnect());
                ChangeBluetoothConfigSetupState(state);
                break;
            }
            case BT_CONFIG_SET_BT_VOLUME:
            {
                RETURN_IF_FAILED(SetDeviceCfgVolume());
                ChangeBluetoothConfigSetupState(state);
                break;
            }
            case BT_CONFIG_SET_COD:
            {
                RETURN_IF_FAILED(SetDeviceCfgCOD());
                ChangeBluetoothConfigSetupState(state);
                break;
            }
            case BT_CONFIG_SET_CODEC:
            {
                RETURN_IF_FAILED(SetDeviceCfgCodecs());
                ChangeBluetoothConfigSetupState(state);
                break;
            }
            case BT_CONFIG_SET_DEVICE_ID:
            {
                RETURN_IF_FAILED(SetDeviceCfgDeviceId());
                ChangeBluetoothConfigSetupState(state);
                break;
            }
            case BT_CONFIG_SET_LED_ENABLE:
            {
                RETURN_IF_FAILED(SetDeviceCfgLedEnable());
                ChangeBluetoothConfigSetupState(state);
                break;
            }
            case BT_CONFIG_SET_GPIO_CONFIG:
            {
                RETURN_IF_FAILED(SetDeviceCfgGpioConfig());
                ChangeBluetoothConfigSetupState(state);
                break;
            }
            case BT_CONFIG_SET_UI_CONFIG:
            {
                RETURN_IF_FAILED(SetDeviceCfgUiConfig());
                ChangeBluetoothConfigSetupState(state);
                break;
            }
            case BT_CONFIG_SET_PROFILES:
            {
                RETURN_IF_FAILED(SetDeviceCfgProfiles());
                ChangeBluetoothConfigSetupState(state);
                break;
            }
            case BT_CONFIG_SET_VREG_ROLE:
            {
                RETURN_IF_FAILED(SetDeviceCfgVregRole());
                ChangeBluetoothConfigSetupState(state);
                break;
            }
            case BT_CONFIG_SET_SSP_CAPS:
            {
                RETURN_IF_FAILED(SetDeviceCfgSspCaps());
                ChangeBluetoothConfigSetupState(state);
                break;
            }
            case BT_CONFIG_SET_BT_STATE:
            {
                RETURN_IF_FAILED(SetDeviceCfgBluetoothState());
                ChangeBluetoothConfigSetupState(state);
                break;
            }
            case BT_CONFIG_SET_LONG_NAME:
            {
                RETURN_IF_FAILED(SetDeviceCfgLongName());
                ChangeBluetoothConfigSetupState(state);
                break;
            }
            case BT_CONFIG_SET_SHORT_NAME:
            {
                RETURN_IF_FAILED(SetDeviceCfgShortName());
                ChangeBluetoothConfigSetupState(state);
                break;
            }
            case BT_CONFIG_GET_PAIRED_DEVICE:
            {
                RETURN_IF_FAILED(GetDeviceCfgRemoteAddress(m_PairedDevice));
                ChangeBluetoothConfigSetupState(state);
                break;
            }
            case BT_CONFIG_WRITE_CFG:
            {
                if (IsConfigWritePending())
                {
                    RETURN_IF_FAILED(WriteConfigToFlash());
                    ChangeBluetoothConfigSetupState(state);
                }
                else
                {
                    ChangeBluetoothConfigSetupState(state, BT_CONFIG_DONE);
                }

                break;
            }
            case BT_CONFIG_RESTART:
            {
                m_waitingForPreviousDeviceConnection = true;
                ChangeBluetoothConfigSetupState(state, BT_CONFIG_GET_VERSION);
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

ERROR_CODE_T IBTADeviceDriver::VerifyConfigSetting(UniqueConfigSettings_t setting, bool *optionWasSet)
{
    bool configNotImplemented = false;
    string settingString = GetUniqueConfigSettingString(setting, &configNotImplemented);
    return VerifyConfigSetting(settingString, setting, optionWasSet);
}

ERROR_CODE_T IBTADeviceDriver::VerifyConfigSetting(string configSetting, UniqueConfigSettings_t setting, bool *optionWasSet)
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

ERROR_CODE_T IBTADeviceDriver::VerifyConfigSetting(string configSetting, string expectedResult, bool *optionWasSet)
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

        // Re-read this back so we verify it was actually written
        RETURN_IF_FAILED(m_pBTASerialDevice->GetCfgValue(retString, configSetting));
    }

    return (retString.find(expectedResult) != string::npos) ? STATUS_SUCCESS : ERROR_FAILED;
}

ERROR_CODE_T IBTADeviceDriver::SetDeviceCfgDigitalAudioMode()
{
    return VerifyConfigSetting("AUDIO", "1 1");
}
ERROR_CODE_T IBTADeviceDriver::SetDeviceCfgDigitalAudioParams()
{
    return VerifyConfigSetting("AUDIO_DIGITAL", UNIQUE_CONFIG_SETTING_DIGITAL_AUDIO_PARAMS);
}

ERROR_CODE_T IBTADeviceDriver::SetDeviceCfgAutoConnect()
{
    return VerifyConfigSetting("AUTOCONN", UNIQUE_CONFIG_SETTING_AUTO_CONNECTION);
}

ERROR_CODE_T IBTADeviceDriver::SetDeviceCfgVolume()
{
    return VerifyConfigSetting("BT_VOL_CONFIG", UNIQUE_CONFIG_SETTING_BT_VOLUME);
}

ERROR_CODE_T IBTADeviceDriver::SetDeviceCfgCOD()
{
    string cod = (m_DeviceMode == BTA_DEVICE_MODE_INPUT) ? "240414" : "20041C";
    return VerifyConfigSetting("COD", cod);
}

ERROR_CODE_T IBTADeviceDriver::SetDeviceCfgCodecs()
{
    return VerifyConfigSetting("CODEC", UNIQUE_CONFIG_SETTING_BT_VOLUME);
}

ERROR_CODE_T IBTADeviceDriver::SetDeviceCfgDeviceId()
{
    return VerifyConfigSetting("DEVICE_ID", UNIQUE_CONFIG_SETTING_DEVICE_ID);
}

ERROR_CODE_T IBTADeviceDriver::SetDeviceCfgLedEnable()
{
    return VerifyConfigSetting("ENABLE_LED", UNIQUE_CONFIG_SETTING_LED_ENABLE);
}

ERROR_CODE_T IBTADeviceDriver::SetDeviceCfgGpioConfig()
{
    return VerifyConfigSetting("GPIO_CONFIG", UNIQUE_CONFIG_SETTING_DEVICE_ID);
}

ERROR_CODE_T IBTADeviceDriver::SetDeviceCfgUiConfig()
{
    return VerifyConfigSetting("UI_CONFIG", UNIQUE_CONFIG_SETTING_UI_CONFIG);
}

ERROR_CODE_T IBTADeviceDriver::SetDeviceCfgLongName()
{
    return VerifyConfigSetting("NAME", m_DeviceName);
}

ERROR_CODE_T IBTADeviceDriver::SetDeviceCfgShortName()
{
    return VerifyConfigSetting("NAME_SHORT", UNIQUE_CONFIG_SETTING_SHORT_NAME);
}

ERROR_CODE_T IBTADeviceDriver::SetDeviceCfgVregRole()
{
    return VerifyConfigSetting("VREG_ROLE", UNIQUE_CONFIG_SETTING_VREG_ROLE);
}

ERROR_CODE_T IBTADeviceDriver::SetDeviceCfgProfiles()
{
    bool optionWasSet = false;

    ERROR_CODE_T ret = VerifyConfigSetting("PROFILES", UNIQUE_CONFIG_SETTING_PROFILES, &optionWasSet);

    if (optionWasSet)
    {
        DebugPrintf(DEBUG_TRACE, DEBUG_TRACE, m_DebugId.c_str(), "Device profiles changed. Flagging to unpair all devices");
        SetFlagUnpairAllDevices(true);
    }

    return ret;
}

ERROR_CODE_T IBTADeviceDriver::SetDeviceCfgSspCaps()
{
    return VerifyConfigSetting("SSP_CAPS", to_string(static_cast<int>(m_configuredSSPMode)));
}

ERROR_CODE_T IBTADeviceDriver::SetDeviceCfgBluetoothState()
{
    return VerifyConfigSetting(UNIQUE_CONFIG_SETTINGS_BT_STATE);
}

ERROR_CODE_T IBTADeviceDriver::NotifyConnection(void)
{
    BTConnectionChangeState.notifyObservers(m_PairingManager->IsDeviceConnected());
    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////
// Monitoring

#define STANDARD_MONITOR_TIMEOUT_S 10
#define NEW_LINK_MONITOR_TIMEOUT_S 3

ERROR_CODE_T IBTADeviceDriver::MonitorStatus(void)
{
    string startLinkBTAddr = m_PairingManager->GetCurrentLinkID();
    m_MonitorTimer.ResetTime(STANDARD_MONITOR_TIMEOUT_S);

    vector<string> statusStrings;
    RETURN_IF_FAILED(m_pBTASerialDevice->ReadData(statusStrings, "STATUS"));

    if (m_DeviceMode == BTA_DEVICE_MODE_INPUT)
    {
        if (m_StartupTimer.IsTimeExpired())
        {
            m_waitingForPreviousDeviceConnection = false;
        }

        RETURN_IF_FAILED(MonitorInputStatus(statusStrings));
    }
    else
    {
        RETURN_IF_FAILED(MonitorOutputStatus(statusStrings));
    }

    // if we have a new connection that has not been written to the REMOTE_ADDR cfg option, see if we can write it now.
    if (IsConfigWritePending())
    {
        string defaultConnectAddr;
        string currentConnectedAddress = m_PairingManager->GetCurrentConnectedDeviceAddress();
        RETURN_IF_FAILED(GetDeviceCfgRemoteAddress(defaultConnectAddr));
        if (defaultConnectAddr.compare(currentConnectedAddress))
        {
            // Setup to auto-connect to this newly connected device.
            RETURN_IF_FAILED(SetDeviceCfgRemoteAddress(currentConnectedAddress));
            RETURN_IF_FAILED(WriteConfigToFlash());
        }
    }

    string updatedLinkAddress = m_PairingManager->GetCurrentLinkID();
    if (startLinkBTAddr.compare(updatedLinkAddress) != 0)
    {
        if (!startLinkBTAddr.empty())
        {
            // forcing false here ensures the CMS see a change in the connection status.
            BTConnectionChangeState.notifyObservers(false);
        }

        // It can take several tries to get the name of the connected device. Since we want to report
        // all of the data at once, we will just wait until we get the device name to notify of a connection
        // change.
        string deviceName = "";
        if (m_PairingManager->IsDeviceConnected())
        {
            string currentAddress = m_PairingManager->GetCurrentConnectedDeviceAddress();
            if (FAILED(m_PairingManager->GetConnectedDeviceName(currentAddress, deviceName)))
            {
                DebugPrintf(DEBUG_NORMAL_ERROR, DEBUG_NORMAL_ERROR, m_DebugId.c_str(), "Failed to get device name for %s\n", currentAddress.c_str());
                return ERROR_FAILED;
            }

            m_PairingManager->SetLinkedName(deviceName);
            m_PairingManager->SetPairedDeviceName(deviceName);

            string connectedAddr = m_PairingManager->GetCurrentConnectedDeviceAddress();

            shared_ptr<CBTEAPairedDevice> pDevice = m_PairingManager->FindPairedDevice(connectedAddr);
            if (pDevice != NULL)
            {
                pDevice->m_btDeviceName = deviceName;
            }
        }

        NotifyConnection();
        m_MonitorTimer.ResetTime(NEW_LINK_MONITOR_TIMEOUT_S);
    }

    return STATUS_SUCCESS;
}

/********************************************************************************************************
********************************************************************************************************/
ERROR_CODE_T IBTADeviceDriver::MonitorInputStatus(vector<string> statusStrings)
{
    bool newLinkConnected = false;
    bool connectionStatus = false;

    // Make input devices always discoverable and connectable. This will allow a new device to connect,
    // bumping the old one off.
    SetBluetoothDiscoverabilityState(true, true);

    vector<string>::iterator it;
    for (it = statusStrings.begin(); it != statusStrings.end(); it++)
    {
        DebugPrintf(m_PacketVerbosity, DEBUG_NO_LOGGING, m_DebugId.c_str(), "Processing: %s", it->c_str());
        if (it->find("LINK") != string::npos)
        {
            // This is connection information.
            // The LINK response is formatted as follows:
            // LINK 10 CONNECTED A2DP D81C79B9933D SUSPENDED SBC SNK 44100
            vector<string> linkStrings;
            RETURN_IF_FAILED(SplitString(linkStrings, *it, ' ', true));
            RETURN_EC_IF_TRUE(ERROR_FAILED, linkStrings.size() != 9);

            if (linkStrings[2].compare("CONNECTED") == 0)
            {
                connectionStatus = true;
                string tempBtAddr = linkStrings[4];
                string tempBtLinkId = linkStrings[1];
                string currentLinkId = m_PairingManager->GetCurrentLinkID();

                if (!m_PairingManager->IsCurrentConnectedDevice(tempBtAddr))
                {
                    if (!newLinkConnected)
                    {
                        if (m_PairingManager->IsDeviceConnected())
                        {
                            // we previously had a connection, but we just established a new connection.
                            // So now we should terminate the previous one.
                            m_PairingManager->CloseConnection(currentLinkId);
                        }

                        m_PairingManager->SetCurrentConnectedDevice(tempBtAddr, tempBtLinkId);
                        m_waitingForPreviousDeviceConnection = false;

                        SetConfigWritePending(true);
                        newLinkConnected = true;
                    }
                    else
                    {
                        // We have more than one new link, we already connected to the first,
                        // so just close the second.
                        m_PairingManager->CloseConnection(tempBtLinkId);
                    }
                }
            }
        }
        else if (it->find("OK") != string::npos)
        {
            // This should indicate the end of the data.
            if (!connectionStatus)
            {
                if (m_PairingManager->IsDeviceConnected())
                {
                    m_PairingManager->ClearCurrentConnectedDevice();
                    SetConfigWritePending(true);
                }

                if (m_waitingForPreviousDeviceConnection)
                {
                    RETURN_IF_FAILED(m_PairingManager->RequestConnectionToDefaultDevice());
                }
            }
            break;
        }
    }

    return STATUS_SUCCESS;
}

/********************************************************************************************************
********************************************************************************************************/
ERROR_CODE_T IBTADeviceDriver::MonitorOutputStatus(vector<string> statusStrings)
{
    bool connectionStatus = false;

    vector<string>::iterator it;
    for (it = statusStrings.begin(); it != statusStrings.end(); it++)
    {
        if (it->find("LINK") != string::npos)
        {
            // This is connection information.
            // The LINK response is formatted as follows:
            // LINK 10 CONNECTED A2DP D81C79B9933D SUSPENDED SBC SNK 44100
            vector<string> linkStrings;
            RETURN_IF_FAILED(SplitString(linkStrings, *it, ' ', true));
            RETURN_EC_IF_TRUE(ERROR_FAILED, linkStrings.size() < 5);
            string tempBtAddr = linkStrings[4];
            string tempBtLinkId = linkStrings[1];

            if (linkStrings[2].compare("CONNECTED") != 0)
            {
                string linkIdOut = "";

                RETURN_IF_FAILED(m_PairingManager->OpenConnection(tempBtAddr, linkIdOut));
                SendPlayCommand(tempBtLinkId);
                SetConfigWritePending(true);
            }
            else if (linkStrings[5].compare("SUSPENDED") == 0)
            {
                RETURN_IF_FAILED(SendPlayCommand(tempBtLinkId));
            }
            else
            {
                m_PairingManager->SetCurrentConnectedDevice(tempBtAddr, tempBtLinkId);
            }

            connectionStatus = true;
        }
        else if (it->find("OK") != string::npos)
        {
            // This should indicate the end of the data.
            if (!connectionStatus)
            {
                if (m_PairingManager->IsPairedWithDevice())
                {
                    string pairedDevice = m_PairingManager->GetCurrentConnectedDevice();
                    string linkId = m_PairingManager->GetCurrentLinkID();
                    string btAddress = m_PairingManager->GetCurrentConnectedDeviceAddress();
                    shared_ptr<CBTEAPairedDevice> pDevice = m_PairingManager->FindPairedDevice(pairedDevice);

                    if (pDevice != NULL)
                    {
                        RETURN_IF_FAILED(m_PairingManager->OpenConnection(btAddress, linkId));
                        m_PairingManager->SetCurrentConnectedDevice(btAddress, linkId);
                        connectionStatus = true;
                    }
                }

                // Output devices are only connectable when there is no other connection.
                if (!connectionStatus)
                {
                    m_PairingManager->ClearCurrentConnectedDevice();
                    SetBluetoothDiscoverabilityState(true, false);
                }
            }
            break;
        }
    }

    // If we're pairing with a device, connect to the paired device
    if (m_PairingManager->IsPairedWithDevice())
    {
        // Connect to paired device. If that fails, reset and try state machine again.
        if (FAILED(m_PairingManager->PairToConnectedDevice()))
        {
            if (FAILED(ResetAndEstablishSerialConnection()))
            {
                // Sometimes the module hangs in the opening state and won't even listen to a reset command.
                // If this happens, we need to reset everything to get it back.
                DebugPrintf(DEBUG_TRACE_MESSAGE, DEBUG_NO_LOGGING, m_DebugId.c_str(), "Performing full reset");
                GotoOfflineState();
            }

            return ERROR_FAILED;
        }

        string linkId = m_PairingManager->GetCurrentLinkID();
        SendPlayCommand(linkId);
        SetConfigWritePending(true);
    }

    return STATUS_SUCCESS;
}

void IBTADeviceDriver::GotoOfflineState()
{
    m_BtFwVersion.Clear();
    m_PairedDevice.clear();
    m_waitingForPreviousDeviceConnection = false;
    SetDeviceReadyForUse(false);
    m_PairingManager->ClearCurrentConnectedDevice();
}
