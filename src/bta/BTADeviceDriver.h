#pragma once

#include <memory>

#include "BTASerialDevice.h"
#include "BTAVersionInfo.h"

class BTADeviceDriver
{
public:
    BTADeviceDriver();
    ~BTADeviceDriver();

    virtual ERROR_CODE_T SetAndOpenBtaSerialDevice(shared_ptr<BTASerialDevice> pBTASerialDevice) = 0;
    virtual ERROR_CODE_T GetDeviceVersion(shared_ptr<BTAVersionInfo_t>& version) = 0;

    virtual ERROR_CODE_T EnterCommandMode();
    virtual ERROR_CODE_T SendReset();
    virtual ERROR_CODE_T GetBaudrate(BAUDRATE &baudrate);
    virtual bool IsCommEnabled(void);
    virtual ERROR_CODE_T TryNextBaudrate();

    // Pets the uart watchdog on the BT adapter
    virtual ERROR_CODE_T WatchdogPet();

    // Sets Local Address - Read only attribute
    ERROR_CODE_T GetDeviceCfgLocalAddress(void);
    string GetPublicAddress(void) { return m_LocalAddress; }

    // Sets the Digital Audio Mode - We always want to be in this mode
    ERROR_CODE_T SetDeviceCfgDigitalAudioMode();

    // See m_confgiWritePending
    void SetConfigWritePending(bool pending);
    bool IsConfigWritePending() { return m_configWritePending; }

protected:
    shared_ptr<BTASerialDevice> m_pBTASerialDevice;
    shared_ptr<BTAVersionInfo_t> m_versionInfo;

    BTAVersionInfo_t m_BtFwVersion;
    string m_BluetoothAddress;
    string m_LocalAddress;

    INT8U m_PacketVerbosity;
    string m_DebugID;

    // Any writes to the configuration requires a write command for it to persist
    // This flag is used to keep track if we need to write that
    bool m_configWritePending;

    virtual BAUDRATE* GetBaudrateList(INT32U* length) = 0;
};