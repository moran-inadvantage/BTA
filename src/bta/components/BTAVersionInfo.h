#pragma once

#include "types.h"

#include <string>

#ifdef __86_64__
using namespace std;
#include <memory>
#else

#endif

typedef enum
{
    BTA_HW_BC127,
    BTA_HW_IDC777,
    BTA_HW_UNKNOWN,
} BTAHardware_t;

typedef enum
{
    IDC777_FW_REV_UNKNOWN,
} IDC777FirmwareRevision_t;

typedef enum
{
    BC127_FW_REV_UNKNOWN,
    BC127_FW_REV_6_1_2,
    BC127_FW_REV_6_1_5,
    BC127_FW_REV_7_0,
    BC127_FW_REV_7_1,
    BC127_FW_REV_7_2,
    BC127_FW_REV_7_3,
} BC127FirmwareRevision_t;

class CBTAVersionInfo_t
{
  public:
    CBTAVersionInfo_t()
        : hardware(BTA_HW_UNKNOWN), major(0), minor(0), patch(0)
    {
    }
    // If you add anything below, update CopyInto
    BTAHardware_t hardware;
    union
    {
        BC127FirmwareRevision_t BC127FwRev;
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
            case BTA_HW_BC127:
                return "BC127";
            case BTA_HW_IDC777:
                return "IDC777";
            default:
                return "Unknown";
        }
    }

    string PrintBuildInfo()
    {
        return string("BTDevice: ") + GetHardwareVersionString() + ": " + to_string(major) + "." + to_string(minor) + "." + to_string(patch) + ":" + buildNumber;
    }

    bool IsValid()
    {
        return hardware != BTA_HW_UNKNOWN && !versionString.empty();
    }

    void CopyInto(shared_ptr<CBTAVersionInfo_t> &version)
    {
        if (!version)
            return;

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
        }
        else if (hardware == BTA_HW_BC127)
        {
            version->BC127FwRev = this->BC127FwRev;
        }
    }

    void Clear()
    {
        hardware = BTA_HW_UNKNOWN;
        major = 0;
        minor = 0;
        patch = 0;
        buildNumber.clear();
        versionString.clear();
    }
};
