#pragma once

/********************************************************************************************************
    File Name:  BTAPairingManager.h

    Notes:      This class is used to manage the pairing of Bluetooth devices.
                It handles the scanning, pairing, and connection of Bluetooth devices.
                It also manages the list of paired devices and their names.

********************************************************************************************************/

#ifdef __x86_64__
using namespace std;
#endif

#include <list>
#include <map>
#include <string>
#include <vector>

#include "BTASerialDevice.h"
#include "CriticalSection.h"
#include "Observable.h"
#include "TimeDelta.h"
#include "types.h"

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

class CBTEADetectedDevice
{
  public:
    static const INT32U RELOAD_TIMER = 2;

    CBTEADetectedDevice()
        : m_btAddress(""), m_btDeviceName("UNKNOWN"), m_btCOD(""), m_btRSSI(""), timeoutCounter(RELOAD_TIMER), notificationSent(false)
    {
    }
    string m_btAddress;
    string m_btDeviceName;
    string m_btCOD;
    string m_btRSSI;
    INT32U timeoutCounter;
    bool notificationSent;
};

class CBTAPairingManager
{
  public:
    CBTAPairingManager(shared_ptr<BTASerialDevice> BTASerialDevice)
        : m_pBTASerialDevice(BTASerialDevice),
          m_scanAbort(false), m_receivedOpenNotification(false), m_currentCOD(""),
          m_scanState(SS_INIT), m_scanTimer(0), m_LogId("BTAPairingManager")
    {
    }

    // Have we received an open notification from the BTASerialDevice since our last check
    virtual BOOLEAN IsNewDeviceConnected(void);

    // Finds the name of the device whos address is btAddress
    virtual ERROR_CODE_T GetConnectedDeviceName(string btAddress, string &nameString);

    // Starts a scan for BT devices around the unit. Will place the result in the detectedDeviceList.
    virtual ERROR_CODE_T ScanForBtDevices(list<shared_ptr<CBTEADetectedDevice> > &detectedDeviceList, bool scanAllDevices, INT8U timeoutSec);

    // If we're in the middle of scanning for devices, this will stop the scan
    virtual ERROR_CODE_T AbortBtDeviceScan(void);

    // Attempts to find a device in our paired device list
    virtual shared_ptr<CBTEAPairedDevice> FindPairedDevice(string btAddress);

    // Removes all of our paired devices and resets our paired device list
    virtual ERROR_CODE_T UnpairAllDevices();

    // Attempts to open a connection with the specifed bt address, and returns the link id
    virtual ERROR_CODE_T OpenConnection(string btAddress, string &linkIdOut);

    // Attempts to pair with the specifed bt address, and returns the link id
    virtual ERROR_CODE_T PairDevice(string btAddress, string &linkIdOut);

    // Pairs with a device that is in the connected device list
    virtual ERROR_CODE_T PairToConnectedDevice();

    // Closes the connection with the link id specified
    virtual ERROR_CODE_T CloseConnection(string linkId);

    virtual ERROR_CODE_T UpdatePairedDeviceList(void);

    virtual ERROR_CODE_T RequestConnectionToDefaultDevice(void);

    virtual ERROR_CODE_T OnOpenNotificationReceived(BOOLEAN state);

    virtual void ResetStateMachine();

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
        m_CurrentLinkBTAddr = "";
        m_CurrentLinkID = "";
        m_CurrentLinkName = "";
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

    Observable<void> PairedDeviceListUpdated;

  private:
    shared_ptr<BTASerialDevice> m_pBTASerialDevice;
    map<string, string> m_deviceNameMap;
    static const CHAR8 *s_InquireCODList[];

    typedef enum
    {
        SS_INIT,
        SS_SCAN_NEXT_DEVICE,
        SS_WAIT_FOR_SCAN_RESPONSE,
        SS_CLEANUP,
    } SCAN_STATE;

    bool m_scanAbort;
    BOOLEAN m_receivedOpenNotification;
    INT32U m_nextInquireIndex;

    string m_currentCOD;
    SCAN_STATE m_scanState;
    CTimeDeltaSec m_scanTimer;

    int m_DeviceMode;

    string m_CurrentLinkID;
    string m_CurrentLinkBTAddr;
    string m_CurrentLinkName;

    string m_PairedBtDeviceAddress;
    string m_PairedBtDeviceAddressName;

    list<shared_ptr<CBTEAPairedDevice> > m_pairedDeviceInfo;
    list<shared_ptr<CBTEADetectedDevice> > m_detectedDeviceList;

    string m_LogId;

    CCriticalSection m_CS;

    void ClearPairedDeviceFoundFlags(void);
    ERROR_CODE_T PerformBackgroundDeviceNameRetrieval(void);
    bool IsValidAudioRenderingDevice(INT32U deviceCODValue);
};
