/********************************************************************************************************
    File Name:  BTADeviceDriver.h

    Notes:      BTA Device Driver is an abstract class that is used to control BTA Devices that have a serial interface
                There are command sets that are common to all devices, but there are also device specific commands
                that are not common to all devices. This class is used to abstract the common commands and provide
                a common interface to the device. The derived classes will implement the device specific commands.
                The derived classes will also implement the device specific commands that are not common to all devices.

********************************************************************************************************/

#pragma once

#include "types.h"

#include "BTAPairingManager.h"
#include "BTASerialDevice.h"
#include "BTAVersionInfo.h"
#include "ToneControl.h"

typedef enum
{
    BTA_DEVICE_MODE_INPUT,
    BTA_DEVICE_MODE_OUTPUT,
} BTADeviceMode_t;

class IBTADeviceDriver
{
  public:
    IBTADeviceDriver();

    // Determines if the device will be used as an input or output module
    virtual void SetDeviceMode(BTADeviceMode_t mode);

    // Configures the serial device and opens the port with the default baudrate
    virtual ERROR_CODE_T SetAndOpenBtaSerialDevice(shared_ptr<BTASerialDevice> pBTASerialDevice);

    // Attempts to read device version, which contains hardware and firmware version
    virtual ERROR_CODE_T GetDeviceVersion(shared_ptr<CBTAVersionInfo_t> &version);

    // Is communication between the device over UART open and valid?
    virtual bool IsCommEnabled(void);

    // Blocking call that goes over all device configurations and verfies that they're in the correct state
    // This slightly differs between FW version of the unit and also HW
    virtual ERROR_CODE_T InitializeDeviceConfiguration();

    // Device is fully configured
    virtual bool IsDeviceReadyForUse();

    // virtual ERROR_CODE_T ResetAndEstablishSerialConnection(shared_ptr<BTASerialDevice> pBtaSerialDevice = NULL);
    virtual ERROR_CODE_T ResetAndEstablishSerialConnection(shared_ptr<BTASerialDevice> pBtaSerialDevice);
    virtual ERROR_CODE_T ResetAndEstablishSerialConnection();

    // Pets the uart watchdog on the BT adapter
    virtual ERROR_CODE_T WatchdogPet(bool flushBuffer = false);

    // Sets Local Address - Read only attribute
    virtual ERROR_CODE_T GetDeviceCfgLocalAddress(void);
    // Read only attribute to the device. A unique address for each module
    virtual string GetPublicAddress(void)
    {
        return m_LocalAddress;
    }

    // An inquiry is a scan for devices around us. This is an async call
    virtual ERROR_CODE_T SendInquiry(INT32U timeout);

    // Changes the devices name, which is seen via bluetooth discovery and when connecting.
    virtual ERROR_CODE_T SetDeviceName(string deviceName);

    // Sends the play command to a given bluetooth link
    virtual ERROR_CODE_T SendPlayCommand(string linkId);

    // Starts play tones
    virtual ERROR_CODE_T PlayNextMusicSequence(void)
    {
        return m_ToneControl.PlayNextMusicSequence(m_pBTASerialDevice);
    }

    //////////////////////////////////////////////////////////////////////////
    // Monitoring - Intended to be called periodically by the controller
    virtual ERROR_CODE_T MonitorStatus(void);

    //////////////////////////////////////////////////////////////////////////
    // Pairing Manager pass through

    virtual ERROR_CODE_T GetConnectedDeviceName(string btAddress, string &nameString)
    {
        return m_PairingManager->GetConnectedDeviceName(btAddress, nameString);
    }

    virtual ERROR_CODE_T ScanForBtDevices(list<shared_ptr<CBTEADetectedDevice> > &detectedDeviceList, INT8U timeoutSec)
    {
        return m_PairingManager->ScanForBtDevices(detectedDeviceList, ShouldScanForAllDevices(), timeoutSec);
    }

    virtual ERROR_CODE_T AbortBtDeviceScan(void)
    {
        return m_PairingManager->AbortBtDeviceScan();
    }

    virtual ERROR_CODE_T PairDevice(string btAddress, string &linkIdOut)
    {
        return m_PairingManager->PairDevice(btAddress, linkIdOut);
    }
    virtual ERROR_CODE_T UnpairAllDevices();

    virtual ERROR_CODE_T OpenConnection(string btAddress, string &linkIdOut)
    {
        return m_PairingManager->OpenConnection(btAddress, linkIdOut);
    }
    virtual ERROR_CODE_T CloseConnection(string linkId)
    {
        return m_PairingManager->CloseConnection(linkId);
    }

    virtual BOOLEAN IsNewDeviceConnected(void)
    {
        return m_PairingManager->IsNewDeviceConnected();
    }
    virtual BOOLEAN IsDeviceConnected()
    {
        return m_PairingManager->IsDeviceConnected();
    }
    virtual BOOLEAN IsPairedWithDevice()
    {
        return m_PairingManager->IsPairedWithDevice();
    }

    Observable<BOOLEAN> BTConnectionChangeState;

  protected:
    shared_ptr<BTASerialDevice> m_pBTASerialDevice;
    shared_ptr<CBTAPairingManager> m_PairingManager;

    CToneControl m_ToneControl;

    CBTAVersionInfo_t m_BtFwVersion;

    // Used as the BT Long Name, which is what is advertised to the world
    string m_DeviceName;
    string m_BluetoothAddress;
    // Read only attribute to the device. A unique address for each module
    string m_LocalAddress;
    string m_PublicAddress;

    bool isCommEnabled;
    bool m_waitingForPreviousDeviceConnection;

    // Device that we're going to autoconnect to after power up
    string m_PairedDevice;

    INT8U m_PacketVerbosity;
    string m_DebugId;

    BTADeviceMode_t m_DeviceMode;
    bool m_deviceReadyForUse;

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

    SspMode_t m_configuredSSPMode;

    // Any writes to the configuration requires a write command for it to persist
    // This flag is used to keep track if we need to write that
    bool m_configWritePending;

    bool m_needToUnpairAllDevices;
    void SetFlagUnpairAllDevices(bool unpair);

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

    virtual ERROR_CODE_T VerifyConfigSetting(UniqueConfigSettings_t setting, bool *optionWasSet = NULL);
    virtual ERROR_CODE_T VerifyConfigSetting(string configSetting, UniqueConfigSettings_t setting, bool *optionWasSet = NULL);
    virtual ERROR_CODE_T VerifyConfigSetting(string configSetting, string expectedResult, bool *optionWasSet = NULL);

    // Pure Virtual Methods

    // Each device needs to specify their acceptable baud rates. The first entry is always the default.
    // As we try to talk to each device, we'll go through the list of acceptable baud rates until we give up.
    virtual vector<BAUDRATE> GetBaudrateList() = 0;
    virtual BAUDRATE GetDefaultBaudRate();
    virtual ERROR_CODE_T GetBaudrate(BAUDRATE &baudrate);

    // Gets the preferred string for the config setting.
    // Also checks if the config setting is implemented or not
    virtual string GetUniqueConfigExpectedString(UniqueConfigSettings_t configOption, bool *notImplemented) = 0;

    // Gets setting string for the config setting, which may change from device to device or Fw to Fw
    virtual string GetUniqueConfigSettingString(UniqueConfigSettings_t configOption, bool *notImplemented) = 0;

    // Each device has its own version return. After this, we expect the m_BtFwVersion to be valid
    virtual void ParseVersionStrings(const vector<string> &retStrings) = 0;

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

    virtual ERROR_CODE_T SetDeviceCfgRemoteAddress(string remoteAddress) = 0;

    virtual ERROR_CODE_T GetDeviceCfgRemoteAddress(string &remoteAddress) = 0;

    virtual ERROR_CODE_T SetBluetoothDiscoverabilityState(bool connectable, bool discoverable) = 0;

    virtual bool ShouldScanForAllDevices() = 0;

    // notify
    virtual ERROR_CODE_T NotifyConnection(void);

    // Devices operate in two different modes, command mode and data mode
    // We always want command mode
    virtual ERROR_CODE_T EnterCommandMode();

    // Sends Reset Command over UART
    virtual ERROR_CODE_T SendReset();
    virtual void SetDeviceReadyForUse(bool isReady);
    // Cycles through viable baudrates until we get a response.
    // Baud rate lists are found from GetBaudrateList
    virtual ERROR_CODE_T TryNextBaudrate();

    // See m_configWritePending
    virtual void SetConfigWritePending(bool pending);
    virtual bool IsConfigWritePending()
    {
        return m_configWritePending;
    }
    virtual ERROR_CODE_T WriteConfigToFlash();

    //////////////////////////////////////////////////////////////////////////
    // Monitoring
    CTimeDeltaSec m_MonitorTimer;
    CTimeDeltaSec m_StartupTimer;
    virtual ERROR_CODE_T MonitorInputStatus(vector<string> statusStrings);
    virtual ERROR_CODE_T MonitorOutputStatus(vector<string> statusStrings);

    int m_ShouldScanForAllDevices;

    virtual void GotoOfflineState(void);

    virtual string DeviceModeToString(BTADeviceMode_t mode);

    virtual void ChangeBluetoothConfigSetupState(int &state, int desiredState);
    virtual string StateToString(int state);
};
