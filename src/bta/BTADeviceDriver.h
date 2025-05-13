#pragma once

#include <memory>

#include "BTASerialDevice.h"

typedef enum
{
    BTA_HW_BT12,
    BTA_HW_IDC777,
    BTA_HW_UNKNOWN,
} BTAHardware_t;

typedef enum
{
    IDC777_FW_REV_UNKNOWN,
} IDC777FirmwareRevision_t;

typedef enum
{
    BT12_FW_REV_UNKNOWN,
    BT12_FW_REV_6_1_2,
    BT12_FW_REV_6_1_5,
    BT12_FW_REV_7_0,
    BT12_FW_REV_7_1,
    BT12_FW_REV_7_2,
    BT12_FW_REV_7_3,
} BT12FirmwareRevision_t;

class BTAVersionInfo_t
{
public:
    BTAVersionInfo_t()
        : hardware(BTA_HW_UNKNOWN), major(0), minor(0), patch(0)
    {
    }
    BTAHardware_t hardware;
    union {
        BT12FirmwareRevision_t BT12FwRev;
        IDC777FirmwareRevision_t IDC777fwRev;
    };
    INT8U major;
    INT8U minor;
    INT8U patch;
    string buildNumber;
    string versionString;

    string GetHardwareVersionString()
    {
        switch (hardware)
        {
        case BTA_HW_BT12:
            return "BT12";
        case BTA_HW_IDC777:
            return "IDC777";
        default:
            return "Unknown";
        }
    }

    string PrintBuildInfo()
    {
        return string("BTDevice: ") \
            + GetHardwareVersionString() \
            + ": " + to_string(major) \
            + "." + to_string(minor) \
            + "." + to_string(patch) \
            + ":" + buildNumber;
    }

    bool IsValid()
    {
        return hardware != BTA_HW_UNKNOWN && !versionString.empty();
    }

    void CopyInto(shared_ptr<BTAVersionInfo_t>& version)
    {
        if (!version) return;

        version->hardware = this->hardware;
        version->major = this->major;
        version->minor = this->minor;
        version->patch = this->patch;
        version->versionString = this->versionString;
        version->buildNumber = this->buildNumber;

        // Optionally copy union data if needed
        // E.g. if you know which hardware, copy the correct union part
        if (hardware == BTA_HW_IDC777)
        {
            version->IDC777fwRev = this->IDC777fwRev;
        } else if (hardware == BTA_HW_BT12)
        {
            version->BT12FwRev = this->BT12FwRev;
        }
    }
};


class BTADeviceDriver
{
public:
    BTADeviceDriver();
    ~BTADeviceDriver();

    virtual ERROR_CODE_T SetAndOpenBtaSerialDevice(shared_ptr<BTASerialDevice> pBTASerialDevice) = 0;
    virtual ERROR_CODE_T GetDeviceVersion(shared_ptr<BTAVersionInfo_t>& version) = 0;
    virtual ERROR_CODE_T TryNextBaudrate() = 0;

    virtual ERROR_CODE_T EnterCommandMode();
    virtual ERROR_CODE_T SendReset();
    virtual ERROR_CODE_T GetBaudrate(BAUDRATE &baudrate);
    virtual bool IsCommEnabled(void);

    virtual ERROR_CODE_T WatchdogPet();

    ERROR_CODE_T GetLocalAddress(void);
protected:
    shared_ptr<BTASerialDevice> m_pBTASerialDevice;
    shared_ptr<BTAVersionInfo_t> m_versionInfo;

    BTAVersionInfo_t m_BtFwVersion;
    string m_BluetoothAddress;
    string m_PublicAddress;

    INT8U m_PacketVerbosity;
    string m_DebugID;

    static BAUDRATE m_BaudRatePreferences[];
};