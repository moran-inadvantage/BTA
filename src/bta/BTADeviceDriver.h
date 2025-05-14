#pragma once

#include <memory>

#include "BTASerialDevice.h"
#include "BTAVersionInfo.h"

typedef enum
{
    UNIQUE_CONFIG_SETTING_DIGITAL_AUDIO_PARAMS,
    UNIQUE_CONFIG_SETTING_AUTO_CONNECTION,
    UNIQUE_CONFIG_SETTING_BT_VOLUME,
    UNIQUE_CONFIG_SETTTING_CODEC,
    UNIQUE_CONFIG_SETTING_DEVICE_ID,
    UNIQUE_CONFIG_SETTING_LED_ENABLE,
    UNIQUE_CONFIG_SETTING_GPIO_CONFIG,
    UNIQUE_CONFIG_SETTING_UI_CONFIG,
    UNIQUE_CONFIG_SETTING_SHORT_NAME,
    UNIQUE_CONFIG_SETTING_PAIRED_DEVICE,
    UNIQUE_CONFIG_SETTING_VREG_ROLE,
    UNIQUE_CONFIG_SETTING_PROFILES,
    UNIQUE_CONFIG_SETTINGS_BT_STATE,
} UniqueConfigSettings_t;

typedef enum
{
    SSP_DISPLAY_ONLY,
    SSP_DISPLAY_YES_NO,
    SSP_KEYBOARD_ONLY,
    SSP_NO_DISPLAY_NO_KEYBOARD,
    SSP_DISPLAY_AND_KEYBOARD,
    SSP_REJECT_ALL,
    SSP_INVALID = 0xFFFFFFFE,
    SSP_FORCE_INT32 = 0xFFFFFFFF,
} SspMode_t;

class BTADeviceDriver
{
public:
    BTADeviceDriver();
    ~BTADeviceDriver();

    // Configures the serial device and opens the port with the default baudrate
    virtual ERROR_CODE_T SetAndOpenBtaSerialDevice(shared_ptr<BTASerialDevice> pBTASerialDevice);

    // Attempts to read device version, which contains hardware and firmware version
    virtual ERROR_CODE_T GetDeviceVersion(shared_ptr<BTAVersionInfo_t>& version);

    // Devices operate in two different modes, command mode and data mode
    // We always want command mode
    virtual ERROR_CODE_T EnterCommandMode();
    virtual bool IsCommEnabled(void);

    // Sends Reset Command over UART
    virtual ERROR_CODE_T SendReset();

    // Running Baudrate of the Serial
    virtual ERROR_CODE_T GetBaudrate(BAUDRATE &baudrate);

    virtual ERROR_CODE_T InitializeDeviceConfiguration();

    // Device is fully configured
    virtual bool IsDeviceReadyForUse();
    virtual void SetDeviceReadyForUse(bool isReady);

    // Cycles through viable baudrates until we get a response.
    // Baud rate lists are found from GetBaudrateList
    virtual ERROR_CODE_T TryNextBaudrate();

    // Pets the uart watchdog on the BT adapter
    virtual ERROR_CODE_T WatchdogPet();

    // Sets Local Address - Read only attribute
    virtual ERROR_CODE_T GetDeviceCfgLocalAddress(void);
    virtual string GetPublicAddress(void) { return m_LocalAddress; }

    // See m_configWritePending
    virtual void SetConfigWritePending(bool pending);
    virtual bool IsConfigWritePending() { return m_configWritePending; }
    virtual ERROR_CODE_T WriteConfigToFlash();

    //
    virtual void SetDeviceDirection(bool isInputDevice);
    virtual bool IsInputDevice() { return m_isInputDevice; }
    virtual bool IsOutputDevice() { return !m_isInputDevice; }

    virtual ERROR_CODE_T SetDeviceName(string deviceName);

    virtual ERROR_CODE_T UnpairAllDevices();


protected:
    shared_ptr<BTASerialDevice> m_pBTASerialDevice;
    shared_ptr<BTAVersionInfo_t> m_versionInfo;

    BTAVersionInfo_t m_BtFwVersion;

    // Used as the BT Long Name, which is what is advertised to the world
    string m_DeviceName;
    string m_BluetoothAddress;
    string m_LocalAddress;
    string m_PublicAddress;

    // Device that we're going to autoconnect to after power up
    string m_PairedDevice;

    INT8U m_PacketVerbosity;
    string m_DebugID;

    bool m_isInputDevice;

    bool m_deviceReadyForUse;

    SspMode_t m_configuredSSPMode;

    // Any writes to the configuration requires a write command for it to persist
    // This flag is used to keep track if we need to write that
    bool m_configWritePending;

    bool m_needToUnpairAllDevices;
    void SetFlagUnpairAllDevices(bool unpair);


    virtual ERROR_CODE_T VerifyConfigSetting(UniqueConfigSettings_t setting, bool* optionWasSet);
    virtual ERROR_CODE_T VerifyConfigSetting(string configSetting, UniqueConfigSettings_t setting, bool* optionWasSet = NULL);
    virtual ERROR_CODE_T VerifyConfigSetting(string configSetting, string expectedResult, bool* optionWasSet = NULL);

    // Pure Virtual Methods

    // Each device needs to specify their acceptable baud rates. The first entry is always the default.
    // As we try to talk to each device, we'll go through the list of acceptable baud rates until we give up.
    virtual BAUDRATE* GetBaudrateList(INT32U* length) = 0;
    virtual BAUDRATE GetDefaultBaudRate();

    // Gets the preferred string for the config setting.
    // Also checks if the config setting is implemented or not
    virtual string GetUniqueConfigExpectedString(UniqueConfigSettings_t configOption, bool* notImplemented) = 0;

    // Gets setting string for the config setting, which may change from device to device or Fw to Fw
    virtual string GetUniqueConfigSettingString(UniqueConfigSettings_t configOption, bool* notImplemented) = 0;

    // Each device has its own version return. After this, we expect the m_BtFwVersion to be valid
    virtual void ParseVersionStrings(const vector<string>& retStrings) = 0;

    virtual ERROR_CODE_T SetDeviceCfgDigitalAudioMode();
    virtual ERROR_CODE_T SetDeviceCfgDigitalAudioParams();
    virtual ERROR_CODE_T SetDeviceCfgAutoConnect();
    virtual ERROR_CODE_T SetDeviceCfgVolume();
    virtual ERROR_CODE_T SetDeviceCfgCOD();
    virtual ERROR_CODE_T SetDeviceCfgCodecs();
    virtual ERROR_CODE_T SetDeviceCfgDeviceId();
    virtual ERROR_CODE_T SetDeviceCfgLedEnable();
    virtual ERROR_CODE_T SetDeviceCfgGpioConfig();
    virtual ERROR_CODE_T SetDeviceCfgUiConfig();
    virtual ERROR_CODE_T SetDeviceCfgLongName();
    virtual ERROR_CODE_T SetDeviceCfgShortName();
    virtual ERROR_CODE_T SetDeviceCfgVregRole();
    virtual ERROR_CODE_T SetDeviceCfgProfiles();
    virtual ERROR_CODE_T SetDeviceCfgSspCaps();
    virtual ERROR_CODE_T SetDeviceCfgBluetoothState();

    virtual ERROR_CODE_T GetDeviceCfgPairedDevice() = 0;
};