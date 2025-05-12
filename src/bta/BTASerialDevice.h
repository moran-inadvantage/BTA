/********************************************************************************************************
    File Name:  BtaSerialDevice.h

    Notes:      This class is an abstract class that defines the interface for a serial device.
                It is used to communicate with a serial device over Bluetooth.

********************************************************************************************************/
#pragma once

#ifdef __x86_64__
using namespace std;
#endif

#include <string>
#include <map>
#include <list>
#include <vector>
#include <list>

#include "types.h"
#include "iuart.h"
#include "TimeDelta.h"
#include "CriticalSection.h"
#include "Observable.h"

class CBTEAPairedDevice;
class CBTEADetectedDevice;

class BTASerialDevice
{
public:
    BTASerialDevice(void);
    ~BTASerialDevice();

    ERROR_CODE_T SetUArt(weak_ptr<IUart> puArt);
    ERROR_CODE_T GetBaudrate(BAUDRATE &baudrate);
    ERROR_CODE_T SetBaudrate(BAUDRATE baudrate);
    ERROR_CODE_T SetCommEnable(bool enabled);
    BOOLEAN IsCommEnabled(void);
    ERROR_CODE_T SetPacketVerbosity(INT8U verbosity);
    INT8U GetPacketVerbosity(void);
    ERROR_CODE_T SetDebugID(CHAR8 (&debugID)[OS_TASK_NAME_SIZE]);
    ERROR_CODE_T SetCardNumber(INT8U cardNumber);
    BOOLEAN IsNewDeviceConnected(void);

    ERROR_CODE_T PetBTAdapterWatchdog(bool flushRxBuffer = false);
    ERROR_CODE_T WriteData(const CHAR8 *pFormatMsg, ...);
    ERROR_CODE_T WriteData(const string &formatMsg, ...);
    ERROR_CODE_T WriteDataIgnoreResponse(const CHAR8 *pFormatMsg, ...);
    ERROR_CODE_T WriteDataIgnoreResponse(const string &formatMsg, ...);
    ERROR_CODE_T ReadData(vector<string> &outStrings, const CHAR8 *pFormatMsg, ...);
    ERROR_CODE_T ReadData(vector<string> &outStrings, const string &formatMsg, ...);
    ERROR_CODE_T SetCfgValue(CHAR8 *pCfgOption, const CHAR8 *pFormatStr, ...);
    ERROR_CODE_T SetCfgValue(CHAR8 *pCfgOption, const string &formatStr, ...);
    ERROR_CODE_T SetCfgValueIgnoreResponse(CHAR8 *pCfgOption, const CHAR8 *pFormatStr, ...);
    ERROR_CODE_T SetCfgValueIgnoreResponse(CHAR8 *pCfgOption, const string &formatStr, ...);
    ERROR_CODE_T GetCfgValue(string &outString, const CHAR8 *pCfgOption);
    ERROR_CODE_T GetCfgValue(string &outString, const string &cfgOption);
    ERROR_CODE_T GetConnectedDeviceName(string btAddress, string &nameString);
    ERROR_CODE_T ScanForBTDevices(list<shared_ptr<CBTEADetectedDevice> > &detectedDeviceList, bool scanAllDevices, INT8U timeoutSec);
    ERROR_CODE_T AbortBtDeviceScan(void);
    ERROR_CODE_T OpenConnection(string btAddress, string &linkIdOut);
    ERROR_CODE_T PairDevice(string btAddress, string &linkIdOut);
    ERROR_CODE_T CloseConnection(string linkId);
    ERROR_CODE_T SendPlayCommand(string btAddress);
    ERROR_CODE_T FlushRxBuffer(INT32U timeoutMS = 50);
    ERROR_CODE_T CancelCurrentCommand(void);
    ERROR_CODE_T SimulateConnectivityLoss(bool connectionLost);
    Observable<string> OnUnsolicitedMessageReceived;

    private:
    static list<string> GetDefaultList(void)
    {
        list<string> defaultList;
        return defaultList;
    }

    ERROR_CODE_T WriteData(const CHAR8 *pFormatMsg, va_list args);
    ERROR_CODE_T WriteData(const CHAR8 *pFormatMsg, va_list args, bool ignoreResponse);
    ERROR_CODE_T ReceiveData(list<string> &responses, INT32U timeoutMS = 450, const list<string> &expectedResponses = GetDefaultList());
    ERROR_CODE_T ProcessUnsolicitedMessage(string command);
    string Utf8Decode(string utf8EncodedString);
    CHAR8 Utf8DecodeChar(string utf8EncodedChar);
    bool CommandIsInList(const string &command, const list<string> &commandList);

    /****************************************************************************************************
        Melody Audio Error Code Translation
        ****************************************************************************************************/
    typedef struct
    {
        INT16U errorCode;
        ERROR_CODE_T avdsCode;
        const CHAR8 *pMsgString;
    } BT_AUDIO_ERROR_CODE;

    map<string, string> m_deviceNameMap;
    CTimeDeltaSec m_CommEnableTimer;

    static const BT_AUDIO_ERROR_CODE s_ErrorCodeList[];
    static const CHAR8 *s_UnsolictedCommands[];
    static const CHAR8 *s_InquireCODList[];
    static const INT32U MAX_PKT_LEN = 512;
    static const INT32U COMM_ENABLE_DELAY = 8;
    static const INT32U s_defaultResponseTimeout = 1000;    // 1 second
    static const INT32U s_defaultInterByteGapTimeout = 100; // 100 ms

    ICriticalSection m_CS;
    INT32U m_nextInquireIndex;
    INT8U m_CardNumber;
    bool m_IsCommEnabled;

    bool m_CancelCurrentCommand;
    bool m_DiscardReceivedData;
    bool m_receivedOpenNotification;
    INT8U m_PacketVerbosity;
    CHAR8 m_DebugID[OS_TASK_NAME_SIZE];
    CHAR8 m_TxBuffer[MAX_PKT_LEN];
    CHAR8 m_RxBuffer[MAX_PKT_LEN];
    INT32U m_rxBytesProcessed;
    INT32U m_rxBytesReceived;
    weak_ptr<IUart> m_pUart;
    BAUDRATE m_curBaudrate;

    typedef enum
    {
        SS_INIT,
        SS_SCAN_NEXT_DEVICE,
        SS_WAIT_FOR_SCAN_RESPONSE,
        SS_CLEANUP,
    } SCAN_STATE;

    bool m_scanAbort;
    string m_currentCOD;
    SCAN_STATE m_scanState;
    CTimeDeltaSec m_scanTimer;
};

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

