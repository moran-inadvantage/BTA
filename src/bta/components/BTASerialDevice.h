#pragma once

/********************************************************************************************************
    File Name:  BtaSerialDevice.h

    Notes:      This class is an abstract class that defines the interface for a
serial device. It is used to communicate with a serial device over Bluetooth.

********************************************************************************************************/

#ifdef __x86_64__
using namespace std;
#include "iuart.h"
#include <memory>
#else
#include "CPPInterfaces.h"
#include <weak_ptr.hpp>
#endif

#include <list>
#include <map>
#include <string>
#include <vector>

#include "CriticalSection.h"
#include "Observable.h"
#include "TimeDelta.h"
#include "types.h"

class BTASerialDevice
{
  public:
    BTASerialDevice(void);
    ~BTASerialDevice();

    virtual ERROR_CODE_T SetUArt(weak_ptr<IUart> puArt);
    virtual ERROR_CODE_T GetBaudrate(BAUDRATE &baudrate);
    virtual ERROR_CODE_T SetBaudrate(BAUDRATE baudrate);
    virtual ERROR_CODE_T SetCommEnable(bool enabled);
    virtual BOOLEAN IsCommEnabled(void);
    virtual ERROR_CODE_T SetPacketVerbosity(INT8U verbosity);
    virtual INT8U GetPacketVerbosity(void);
    virtual ERROR_CODE_T SetDebugID(CHAR8 (&debugID)[OS_TASK_NAME_SIZE]);
    virtual ERROR_CODE_T SetCardNumber(INT8U cardNumber);

    virtual ERROR_CODE_T PetBTAdapterWatchdog(bool flushRxBuffer = false);

    virtual ERROR_CODE_T ReadData(vector<string> &outStrings, string message);
    virtual ERROR_CODE_T WriteData(string message, bool ignoreResponse = false);

    // Will read the config option, verify the response, and write the data back
    // to the device if it's not set Will verify that it was set correctly
    virtual ERROR_CODE_T ReadVerifyWriteCfgData(const string cfgOption,
                                                string expectedResult,
                                                bool *optionWasSet);

    virtual ERROR_CODE_T GetCfgValue(string &outString, const string cfgOption);
    virtual ERROR_CODE_T SetCfgValue(string cfgOption, string value,
                                     bool ignoreResponse = false);

    virtual ERROR_CODE_T FlushRxBuffer(INT32U timeoutMS = 50);
    virtual ERROR_CODE_T CancelCurrentCommand(void);
    virtual ERROR_CODE_T SimulateConnectivityLoss(bool connectionLost);

    Observable<string> OnUnsolicitedMessageReceived;
    Observable<BOOLEAN> OnOpenNotificationReceived;

    virtual bool GetCancelCurrentCommand(void)
    {
        return m_CancelCurrentCommand;
    }
    virtual void SetCancelCurrentCommand(bool cancel)
    {
        m_CancelCurrentCommand = cancel;
    }

    virtual ERROR_CODE_T
    ReceiveData(list<string> &responses, INT32U timeoutMS = 450,
                const list<string> &expectedResponses = GetDefaultList());
    virtual string Utf8Decode(string utf8EncodedString);

  private:
    static list<string> GetDefaultList(void)
    {
        list<string> defaultList;
        return defaultList;
    }

    ERROR_CODE_T ProcessUnsolicitedMessage(string command);

    CHAR8 Utf8DecodeChar(string utf8EncodedChar);
    BOOLEAN CommandIsInList(const string &command,
                            const list<string> &commandList);

    /****************************************************************************************************
        Melody Audio Error Code Translation
        ****************************************************************************************************/
    typedef struct
    {
        INT16U errorCode;
        ERROR_CODE_T avdsCode;
        const CHAR8 *pMsgString;
    } BT_AUDIO_ERROR_CODE;

    CTimeDeltaSec m_CommEnableTimer;

    static const BT_AUDIO_ERROR_CODE s_ErrorCodeList[];
    static const CHAR8 *s_UnsolictedCommands[];

    static const INT32U MAX_PKT_LEN = 512;
    static const INT32U COMM_ENABLE_DELAY = 8;
    static const INT32U s_defaultResponseTimeout = 1000;    // 1 second
    static const INT32U s_defaultInterByteGapTimeout = 100; // 100 ms

    CCriticalSection m_CS;

    INT8U m_CardNumber;
    BOOLEAN m_IsCommEnabled;
    BOOLEAN m_CancelCurrentCommand;
    BOOLEAN m_DiscardReceivedData;

    INT8U m_PacketVerbosity;
    CHAR8 m_DebugID[OS_TASK_NAME_SIZE];
    CHAR8 m_TxBuffer[MAX_PKT_LEN];
    CHAR8 m_RxBuffer[MAX_PKT_LEN];
    INT32U m_rxBytesProcessed;
    INT32U m_rxBytesReceived;
    weak_ptr<IUart> m_pUart;
    BAUDRATE m_curBaudrate;
};
