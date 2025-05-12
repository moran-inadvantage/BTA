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
    BTAHardware_t hardware;
    union {
        BT12FirmwareRevision_t BT12FwRev;
        IDC777FirmwareRevision_t IDC777fwRev;
    };
    INT8U major;
    INT8U minor;
    INT8U patch;
    INT8U build;

    string PrintBuildInfo()
    {
        return string("Build: ") + to_string(major) + "." + to_string(minor) + "." + to_string(patch) + "." + to_string(build);
    }
};


class BTADeviceDriver
{
public:
    BTADeviceDriver();
    ~BTADeviceDriver();

    virtual ERROR_CODE_T SetBtaSerialDevice(shared_ptr<BTASerialDevice> pBTASerialDevice) = 0;
    virtual ERROR_CODE_T GetDeviceVersion(shared_ptr<BTAVersionInfo_t>& version) = 0;

    virtual ERROR_CODE_T EnterCommandMode();
    virtual ERROR_CODE_T SendReset();
protected:
    shared_ptr<BTASerialDevice> m_pBTASerialDevice;
    shared_ptr<BTAVersionInfo_t> m_versionInfo;

    BTAVersionInfo_t version;
    INT8U m_PacketVerbosity;
    string m_DebugID;
};