#pragma once

/********************************************************************************************************
    File Name:  BTAPairingManager.h

    Notes:      This class is used to manage the pairing of Bluetooth devices.
                It handles scanning, pairing, and connecting Bluetooth devices,
                as well as maintaining the list of paired devices and their names.
********************************************************************************************************/

//--------------------------------------------------------------------------------------------------
// Platform-specific includes
//--------------------------------------------------------------------------------------------------
#ifdef __x86_64__
using namespace std;
#endif

//--------------------------------------------------------------------------------------------------
// Common includes
//--------------------------------------------------------------------------------------------------
#include <list>
#include <map>
#include <string>
#include <vector>

#include "BTASerialDevice.h"
#include "CriticalSection.h"
#include "Observable.h"
#include "TimeDelta.h"
#include "types.h"

//--------------------------------------------------------------------------------------------------
// Bluetooth Device Representations
//--------------------------------------------------------------------------------------------------

// Represents a previously paired Bluetooth device
class CBTEAPairedDevice
{
  public:
    CBTEAPairedDevice()
        : m_btAddress(""), m_btDeviceName(""), m_found(false)
    {
    }

    CBTEAPairedDevice(string btAddress, string btDeviceName)
        : m_btAddress(btAddress),
          m_btDeviceName(btDeviceName), m_found(true), m_nameRequested(false)
    {
    }

    string m_btAddress;
    string m_btDeviceName;
    bool m_found;
    bool m_nameRequested;
};

// Represents a device detected during scanning
class CBTEADetectedDevice
{
  public:
    static const INT32U RELOAD_TIMER = 2;

    CBTEADetectedDevice()
        : m_btAddress(""), m_btDeviceName("UNKNOWN"), m_btCOD(""), m_btRSSI(""),
          timeoutCounter(RELOAD_TIMER), notificationSent(false)
    {
    }

    string m_btAddress;
    string m_btDeviceName;
    string m_btCOD;
    string m_btRSSI;
    INT32U timeoutCounter;
    bool notificationSent;
};

//--------------------------------------------------------------------------------------------------
// Pairing Manager
//--------------------------------------------------------------------------------------------------
class CBTAPairingManager
{
  public:
    CBTAPairingManager(shared_ptr<CBTASerialDevice> BTASerialDevice)
        : m_pBTASerialDevice(BTASerialDevice),
          m_scanAbort(false), m_receivedOpenNotification(false), m_currentCOD(""),
          m_scanState(SS_INIT), m_scanTimer(0), m_LogId("BTAPairingManager")
    {
    }

    // Connection Status
    virtual BOOLEAN IsNewDeviceConnected(void);

    // Device Info Retrieval
    virtual ERROR_CODE_T GetConnectedDeviceName(string btAddress, string &nameString);

    // Device Scanning
    virtual ERROR_CODE_T ScanForBtDevices(
        list<shared_ptr<CBTEADetectedDevice> > &detectedDeviceList,
        bool scanAllDevices,
        INT8U timeoutSec);
    virtual ERROR_CODE_T AbortBtDeviceScan(void);

    // Device Management
    virtual shared_ptr<CBTEAPairedDevice> FindPairedDevice(string btAddress);
    virtual ERROR_CODE_T UnpairAllDevices(void);
    virtual ERROR_CODE_T UpdatePairedDeviceList(void);

    // Pairing / Connection
    virtual ERROR_CODE_T OpenConnection(string btAddress, string &linkIdOut);
    virtual ERROR_CODE_T PairDevice(string btAddress, string &linkIdOut);
    virtual ERROR_CODE_T PairToConnectedDevice(void);
    virtual ERROR_CODE_T CloseConnection(string linkId);
    virtual ERROR_CODE_T RequestConnectionToDefaultDevice(void);

    // Event Handling
    virtual ERROR_CODE_T OnOpenNotificationReceived(BOOLEAN state);

    // State Management
    virtual void ResetStateMachine();

    // Inline Helper Methods
    inline bool IsDefaultDevice(string btAddr)
    {
        return m_PairedBtDeviceAddress.compare(btAddr) == 0;
    }

    inline bool IsPairedWithDevice(void)
    {
        return !m_PairedBtDeviceAddress.empty() && !m_CurrentLinkID.empty();
    }

    inline bool IsCurrentConnectedDevice(string btAddr)
    {
        return m_CurrentLinkBTAddr.compare(btAddr) == 0;
    }

    inline bool IsDeviceConnected(void)
    {
        return !m_CurrentLinkID.empty();
    }

    inline void SetCurrentConnectedDevice(string btAddr, string linkId)
    {
        m_CurrentLinkBTAddr = btAddr;
        m_CurrentLinkID = linkId;
    }

    inline void ClearCurrentConnectedDevice(void)
    {
        m_CurrentLinkBTAddr.clear();
        m_CurrentLinkID.clear();
        m_CurrentLinkName.clear();
    }

    inline string GetCurrentConnectedDevice()
    {
        return m_PairedBtDeviceAddress;
    }

    inline string GetCurrentLinkID()
    {
        return m_CurrentLinkID;
    }

    inline string GetCurrentConnectedDeviceAddress()
    {
        return m_CurrentLinkBTAddr;
    }

    inline void SetLinkedName(string name)
    {
        m_CurrentLinkName = name;
    }

    inline void SetPairedDeviceName(string name)
    {
        m_PairedBtDeviceAddressName = name;
    }

    inline void SetDeviceMode(int mode)
    {
        m_DeviceMode = mode;
    }

    // Observable for external updates
    Observable<void> PairedDeviceListUpdated;

  private:
    // BT Serial Device Interface
    shared_ptr<CBTASerialDevice> m_pBTASerialDevice;

    // Device name cache
    map<string, string> m_deviceNameMap;

    // Scan state enum
    typedef enum
    {
        SS_INIT,
        SS_SCAN_NEXT_DEVICE,
        SS_WAIT_FOR_SCAN_RESPONSE,
        SS_CLEANUP,
    } SCAN_STATE;

    // Static configuration
    static const CHAR8 *s_InquireCODList[];

    // Scan control
    bool m_scanAbort;
    BOOLEAN m_receivedOpenNotification;
    INT32U m_nextInquireIndex;

    // Current scan context
    string m_currentCOD;
    SCAN_STATE m_scanState;
    CTimeDeltaSec m_scanTimer;

    // Connection state
    int m_DeviceMode;
    string m_CurrentLinkID;
    string m_CurrentLinkBTAddr;
    string m_CurrentLinkName;

    // Paired device state
    string m_PairedBtDeviceAddress;
    string m_PairedBtDeviceAddressName;

    list<shared_ptr<CBTEAPairedDevice> > m_pairedDeviceInfo;
    list<shared_ptr<CBTEADetectedDevice> > m_detectedDeviceList;

    // Logger ID
    string m_LogId;

    // Thread safety
    CCriticalSection m_CS;

    // Internal helpers
    void ClearPairedDeviceFoundFlags(void);
    ERROR_CODE_T PerformBackgroundDeviceNameRetrieval(void);
    bool IsValidAudioRenderingDevice(INT32U deviceCODValue);
};
