#include <string.h>
#include <cstdarg>

#include "BTASerialDevice.h"
#include "BTAStatusTable.h"
#include "ExtIO.h"
#include "IO.h"
#include "ScopeExit.h"

/********************************************************************************************************
                                    Constructor
********************************************************************************************************/
BTASerialDevice::BTASerialDevice()
    : m_IsCommEnabled(false), m_CancelCurrentCommand(false), m_DiscardReceivedData(false),
      m_receivedOpenNotification(false), m_PacketVerbosity(DEBUG_NO_LOGGING), m_rxBytesProcessed(0),
      m_rxBytesReceived(0), m_CardNumber(0), m_nextInquireIndex(0), m_curBaudrate(BAUDRATE_9600)
{
    m_DebugID[0] = '\0';
}

/********************************************************************************************************
                                    Destructor
********************************************************************************************************/
BTASerialDevice::~BTASerialDevice()
{
    shared_ptr<IUart> pUart = m_pUart.lock();
    if (pUart.get() != NULL)
    {
        pUart->Close();
    }
}

const CHAR8 *BTASerialDevice::s_InquireCODList[] =
    {
        "240404", // Audio Rendering, Wearable Headset Device
        //"240414", // Audio Rendering, Loudspeaker
        "240418", // Audio Rendering, Headphones
        "2C0404", // Audio Rendering/Capturing, Wearable Headset Device
};

const BTASerialDevice::BT_AUDIO_ERROR_CODE BTASerialDevice::s_ErrorCodeList[] =
    {
        {0x0003, ERROR_FAILED, "Unknown Error"},
        {0x0011, ERROR_INVALID_CONFIGURATION, "Command not allowed with the current configuration"},
        {0x0012, ERROR_FAILED, "Command not found"},
        {0x0013, ERROR_INVALID_PARAMETER, "Wrong parameter"},
        {0x0014, ERROR_INVALID_PARAMETER, "Wrong number of parameters"},
        {0x0015, ERROR_FAILED, "Command not allowed in current state"},
        {0x0016, ERROR_FAILED, "Device already connected"},
        {0x0017, ERROR_FAILED, "Device not connected"},
        {0x0018, ERROR_FAILED, "Command is too long"},
        {0x0019, ERROR_FAILED, "Name not found"},
        {0x001A, ERROR_FAILED, "Configuration not found"},
        {0x0100, ERROR_FAILED, "Failed to read battery voltage"},
        {0x1002, ERROR_FAILED, "Failed to communicate with the Apple MFI Co-processor"},
        {0x1004, ERROR_FAILED, "Failed to register/unregister device"},
        {0x1005, ERROR_FAILED, "BLE request failed"},
        {0x1006, ERROR_FAILED, "Insufficient encryption"},
        {0x1007, ERROR_FAILED, "Insufficient authentication"},
        {0x1008, ERROR_FAILED, "Operation not permitted"},
        {0x1009, ERROR_INVALID_HANDLE, "Invalid handle"},
        {0xF000, ERROR_FAILED, "Critical Error"},
};

const CHAR8 *BTASerialDevice::s_UnsolictedCommands[] =
    {
        "A2DP_STREAM_START",
        "A2DP_STREAM_SUSPEND",
        "ABS_VOL",
        "ASSOCIATION",
        "ASSOCIATION_IN_PROGRESS",
        "AT",
        "AVRCP_BACKWARD",
        "AVRCP_FORWARD",
        "AVRCP_MEDIA",
        "AVRCP_PAUSE",
        "AVRCP_PLAY",
        "AVRCP_STOP",
        "BA_BROADCASTER_START",
        "BA_BROADCASTER_STOP",
        "BC_SMART_CMD",
        "BC_SMART_CMD_RESP",
        "BLE_INDICATION",
        "BLE_PAIR_ERROR",
        "BLE_PAIR_OK",
        "BLE_READ",
        "BLE_WRITE",
        "CALL_ACTIVE",
        "CALL_DIAL",
        "CALL_END",
        "CALL_INCOMING",
        "CALL_MEMORY",
        "CALL_OUTGOING",
        "CALL_REDIAL",
        "CALLER_NUMBER",
        "CHARGING_IN_PROGRESS",
        "CHARGING_COMPLETE",
        "CHARGER_DISCONNECTED",
        "CLOSE_OK",
        "DTMF",
        "IAP_CLOSE_SESSION",
        "IAP_OPEN_SESSION",
        "INBAND_RING",
        "INQU_OK",
        "LINK_LOSS",
        "MAP_NEW_MSG",
        "OPEN_OK",
        "PAIR_ERROR",
        "PAIR_OK",
        "PAIR_PASSKEY",
        "PAIR_PENDING",
        "RECV",
        "REMOTE_VOLUME",
        "ROLE",
        "ROLE_OK",
        "ROLE_NOT_ALLOWED",
        "SCO_OPEN",
        "SCO_CLOSE",
        "SR",
};


/********************************************************************************************************
                               ERROR_CODE_T SetUArt(IUart *pUart)
    Sets the IUart that will be used for communication.
    If there was a previous connection it will be closed.
********************************************************************************************************/
ERROR_CODE_T BTASerialDevice::SetUArt(weak_ptr<IUart> pUart)
{
    CSimpleLock myLock(&m_CS);
    shared_ptr<IUart> pOldUart = m_pUart.lock();
    if (pOldUart != NULL)
    {
        pOldUart->Close();
    }

    m_pUart = pUart;
    shared_ptr<IUart> pNewUart = m_pUart.lock();

    RETURN_EC_IF_NULL(ERROR_NOT_INITIALIZED, pNewUart);

    return pNewUart->Open(m_curBaudrate, BYTE_SZ_8, NO_PARITY, STOP_BITS_1);
}

/********************************************************************************************************
********************************************************************************************************/
ERROR_CODE_T BTASerialDevice::GetBaudrate(BAUDRATE &baudrate)
{
    baudrate = m_curBaudrate;
    return STATUS_SUCCESS;
}

/********************************************************************************************************
********************************************************************************************************/
ERROR_CODE_T BTASerialDevice::SetBaudrate(BAUDRATE baudrate)
{
    shared_ptr<IUart> pUart = m_pUart.lock();
    RETURN_EC_IF_NULL(ERROR_NOT_INITIALIZED, pUart);
    
    if (m_curBaudrate == baudrate)
    {
        return STATUS_SUCCESS;
    }
    
    RETURN_IF_FAILED(pUart->Close());
    RETURN_IF_FAILED(pUart->Open(baudrate, BYTE_SZ_8, NO_PARITY, STOP_BITS_1));
    m_curBaudrate = baudrate;
    return STATUS_SUCCESS;
}

/********************************************************************************************************
                            ERROR_CODE_T SetCommEnable(bool enabled)
    Sets a flag that will enable/disable all serial communication. Since the watchdog is reset when
    activity on the serial port is detected, disabling the communciation will allow the card to control
    the reset.
********************************************************************************************************/
ERROR_CODE_T BTASerialDevice::SetCommEnable(bool enabled)
{
    CSimpleLock myLock(&m_CS);
    m_CommEnableTimer.ResetTime(COMM_ENABLE_DELAY);
    m_IsCommEnabled = enabled;
    m_nextInquireIndex = 0;
    m_scanState = SS_INIT;
    m_scanAbort = false;
    m_DiscardReceivedData = false;
    return STATUS_SUCCESS;
}

/********************************************************************************************************
                                bool IsCommEnabled( void )
    Returns the state of the Comm Enabled flag.
********************************************************************************************************/
bool BTASerialDevice::IsCommEnabled(void)
{
    CSimpleLock myLock(&m_CS);

    if (!m_CommEnableTimer.IsTimeExpired())
    {
        PetBTAdapterWatchdog(true);
        return false;
    }

    return m_IsCommEnabled;
}

/********************************************************************************************************
                        ERROR_CODE_T SetPacketVerbosity( INT8U verbosity )
    Sets the packet verbosity, should be DEBUG_TRACE_MESSAGE or DEBUG_NO_LOGGING.
********************************************************************************************************/
ERROR_CODE_T BTASerialDevice::SetPacketVerbosity(INT8U verbosity)
{
    RETURN_EC_IF_FALSE(ERROR_INVALID_PARAMETER, verbosity == DEBUG_TRACE_MESSAGE || verbosity == DEBUG_NO_LOGGING);
    m_PacketVerbosity = verbosity;
    return STATUS_SUCCESS;
}

/********************************************************************************************************
                            INT8U GetPacketVerbosity( void )
    Returns the current packet verbosity.
********************************************************************************************************/
INT8U BTASerialDevice::GetPacketVerbosity(void)
{
    return m_PacketVerbosity;
}

/********************************************************************************************************
                   ERROR_CODE_T SetDebugID(CHAR8 (&debugID)[OS_TASK_NAME_SIZE])
    Sets the ID used for debug messages.
********************************************************************************************************/
ERROR_CODE_T BTASerialDevice::SetDebugID(CHAR8 (&debugID)[OS_TASK_NAME_SIZE])
{
    CSimpleLock myLock(&m_CS);
    memset(m_DebugID, 0, OS_TASK_NAME_SIZE);
    strncpy(m_DebugID, debugID, OS_TASK_NAME_SIZE);
    return STATUS_SUCCESS;
}

/********************************************************************************************************
********************************************************************************************************/
ERROR_CODE_T BTASerialDevice::SetCardNumber(INT8U cardNumber)
{
    m_CardNumber = cardNumber;
    return STATUS_SUCCESS;
}

/********************************************************************************************************
********************************************************************************************************/
bool BTASerialDevice::IsNewDeviceConnected(void)
{
    CSimpleLock myLock(&m_CS);
    bool temp = m_receivedOpenNotification;
    m_receivedOpenNotification = false;
    return temp;
}

/********************************************************************************************************
                     ERROR_CODE_T PetBTAdapterWatchdog(bool flushRxBuffer = false)
    Sends a dummy byte to the adapter to keep it alive.
********************************************************************************************************/
ERROR_CODE_T BTASerialDevice::PetBTAdapterWatchdog(bool flushRxBuffer)
{
    CSimpleLock myLock(&m_CS);
    shared_ptr<IUart> pUart = m_pUart.lock();
    RETURN_EC_IF_NULL(ERROR_NOT_INITIALIZED, pUart.get());
    RETURN_EC_IF_FALSE(ERROR_NOT_INITIALIZED, m_IsCommEnabled);

    INT8U dummyByte = 0;
    pUart->WritePort(&dummyByte, 1, NULL);

    if (flushRxBuffer)
    {
        dummyByte = (INT8U)'\r';
        pUart->WritePort(&dummyByte, 1, NULL);

        FlushRxBuffer();
    }

    return STATUS_SUCCESS;
}

/********************************************************************************************************
                    ERROR_CODE_T WriteData( const CHAR8 *pFormatMsg, ... )
    Sends the command in the pFormatMsg.
********************************************************************************************************/
ERROR_CODE_T BTASerialDevice::WriteData(const CHAR8 *pFormatMsg, ...)
{
    CSimpleLock myLock(&m_CS);
    ERROR_CODE_T result = ERROR_FAILED;

    va_list args;
    va_start(args, pFormatMsg);
    result = WriteData(pFormatMsg, args);
    va_end(args);

    return result;
}

/********************************************************************************************************
                    ERROR_CODE_T WriteData( const string &formatMsg, ... )
    Sends the command in the pFormatMsg.
********************************************************************************************************/
ERROR_CODE_T BTASerialDevice::WriteData(const string &formatMsg, ...)
{
    CSimpleLock myLock(&m_CS);
    const CHAR8 * pFormatMsg = formatMsg.c_str();

    va_list args;
    va_start(args, pFormatMsg);
    ERROR_CODE_T result = WriteData(pFormatMsg, args);
    va_end(args);

    return result;
}

/********************************************************************************************************
                    ERROR_CODE_T WriteDataIgnoreResponse( const CHAR8 *pFormatMsg, ... )
    Sends the command in the pFormatMsg, ignoring the response. This should be used for
    commands that do not return a response.
********************************************************************************************************/
ERROR_CODE_T BTASerialDevice::WriteDataIgnoreResponse(const CHAR8 *pFormatMsg, ...)
{
    CSimpleLock myLock(&m_CS);

    va_list args;
    va_start(args, pFormatMsg);
    ERROR_CODE_T result = WriteData(pFormatMsg, args, true);
    va_end(args);

    return result;
}

/********************************************************************************************************
                    ERROR_CODE_T WriteDataIgnoreResponse( const string &formatMsg, ... )
    Sends the command in the pFormatMsg, ignoring the response. This should be used for
    commands that do not return a response.
********************************************************************************************************/
ERROR_CODE_T BTASerialDevice::WriteDataIgnoreResponse(const string &formatMsg, ...)
{
    CSimpleLock myLock(&m_CS);
    const CHAR8 *pFormatMsg = formatMsg.c_str();

    va_list args;
    va_start(args, pFormatMsg);
    ERROR_CODE_T result = WriteData(pFormatMsg, args, true);
    va_end(args);

    RETURN_IF_FAILED(result);
    return STATUS_SUCCESS;
}

/********************************************************************************************************
          ERROR_CODE_T ReadData( vector<string> &outStrings, const CHAR8 *pFormatMsg, ... )
    Sends the command in pFormatMsg and returns the response in the outStrings vector.
********************************************************************************************************/
ERROR_CODE_T BTASerialDevice::ReadData(vector<string> &outStrings, const CHAR8 *pFormatMsg, ...)
{
    CSimpleLock myLock(&m_CS);

    va_list args;
    va_start(args, pFormatMsg);
    ERROR_CODE_T result = WriteData(pFormatMsg, args, true);
    va_end(args);
    RETURN_IF_FAILED(result);

    list<string> responseStrings;
    RETURN_IF_FAILED(ReceiveData(responseStrings, 100));
    outStrings = vector<string>(responseStrings.begin(), responseStrings.end());
    return STATUS_SUCCESS;
}

ERROR_CODE_T BTASerialDevice::ReadDataSimple(vector<string>& outStrings, const std::string& command)
{
   return ReadData(outStrings, command);
}

ERROR_CODE_T BTASerialDevice::ReadVerifyWriteCfgData(const string &cfgOption, string expectedResult, bool* optionWasSet)
{
    string retString;
    *optionWasSet = false;
    RETURN_IF_FAILED(GetCfgValue(retString, cfgOption));

    if (retString.find(expectedResult) == string::npos)
    {
        SetCfgValue(const_cast<CHAR8*>(cfgOption.c_str()), expectedResult.c_str());
    }

    // Verify we set it correctly
    RETURN_IF_FAILED(GetCfgValue(retString, cfgOption));

    RETURN_EC_IF_TRUE(ERROR_FAILED, retString.find(expectedResult) == string::npos);

    *optionWasSet = true;

    return STATUS_SUCCESS;
}

/********************************************************************************************************
          ERROR_CODE_T ReadData( vector<string> &outStrings, const string &formatMsg, ... )
    Sends the command in pFormatMsg and returns the response in the outStrings vector.
********************************************************************************************************/
ERROR_CODE_T BTASerialDevice::ReadData(vector<string> &outStrings, const string &formatMsg, ...)
{
    CSimpleLock myLock(&m_CS);
    INT32U numBytesReceived = 0;
    const CHAR8 * pFormatMsg = formatMsg.c_str();

    va_list args;
    va_start(args, pFormatMsg);
    ERROR_CODE_T result = WriteData(pFormatMsg, args, numBytesReceived);
    va_end(args);
    RETURN_IF_FAILED(result);

    list<string> responseStrings;
    RETURN_IF_FAILED(ReceiveData(responseStrings, 100));
    outStrings = vector<string>(responseStrings.begin(), responseStrings.end());
    return STATUS_SUCCESS;
}

/********************************************************************************************************
********************************************************************************************************/
ERROR_CODE_T BTASerialDevice::GetConnectedDeviceName(string btAddress, string &nameString)
{
    CSimpleLock myLock(&m_CS);
    nameString = "";
    RETURN_EC_IF_FALSE(ERROR_NOT_INITIALIZED, m_IsCommEnabled);

    if (m_deviceNameMap.find(btAddress) != m_deviceNameMap.end())
    {
        nameString = m_deviceNameMap[btAddress];
    }

    if (nameString.empty())
    {
        FlushRxBuffer();
        RETURN_IF_FAILED(WriteDataIgnoreResponse("NAME %s", btAddress.c_str()));

        CTimeDeltaSec messageTimer(10);
        while (!messageTimer.IsTimeExpired() && nameString.empty())
        {
            PetWatchdog();
            RETURN_EC_IF_TRUE(ERROR_FAILED, m_CancelCurrentCommand);

            list<string> responseStrings;
            if (SUCCEEDED(ReceiveData(responseStrings, 100)))
            {
                list<string>::iterator it;
                for (it = responseStrings.begin(); it != responseStrings.end(); it++)
                {
                    vector<string> nameResults;
                    RETURN_IF_FAILED(SplitString(nameResults, *it, ' ', true));

                    if (nameResults.size() >= 3 && nameResults[0] == "NAME")
                    {
                        if (nameResults[1] == btAddress)
                        {
                            // The BT device name can have  a space in it, which throws the parsing off.
                            vector<string> deviceNameStrings;
                            RETURN_IF_FAILED(SplitString(deviceNameStrings, *it, '\"', true));
                            nameString = Utf8Decode(deviceNameStrings[1]);
                            m_deviceNameMap.insert(pair<string, string>(btAddress, nameString));
                            break;
                        }
                    }
                }
            }

            OSTimeDly(OS_TICKS_PER_SEC);
            PetBTAdapterWatchdog();
        }

        PetBTAdapterWatchdog(true);
    }

    RETURN_EC_IF_TRUE(ERROR_FAILED, nameString.empty());
    return STATUS_SUCCESS;
}


/********************************************************************************************************
********************************************************************************************************/
ERROR_CODE_T BTASerialDevice::ScanForBTDevices(list<shared_ptr<CBTEADetectedDevice> > &detectedDeviceList, bool scanAllDevices, INT8U timeoutSec)
{
    CSimpleLock myLock(&m_CS);
    BTASerialDevice *pThis = this;

    ScopeExit(
        scanCleanupSe, {
            pThis->m_scanState = SS_INIT;
            pThis->m_scanAbort = false;
        },
        BTASerialDevice *, pThis);

    RETURN_EC_IF_FALSE(ERROR_NOT_INITIALIZED, m_IsCommEnabled);
    RETURN_EC_IF_TRUE(STATUS_SUCCESS, m_scanAbort);
    list<shared_ptr<CBTEADetectedDevice> >::iterator it;

    switch (m_scanState)
    {
        case SS_INIT:
            for (it = detectedDeviceList.begin(); it != detectedDeviceList.end(); it++)
            {
                if (scanAllDevices || s_InquireCODList[m_nextInquireIndex] == (*it)->m_btCOD)
                {
                    if ((*it)->timeoutCounter != 0)
                    {
                        (*it)->timeoutCounter--;
                    }
                }
            }
            m_scanState = SS_SCAN_NEXT_DEVICE;
            break;

        case SS_SCAN_NEXT_DEVICE:
            PetBTAdapterWatchdog(true);

            if (!scanAllDevices)
            {
                RETURN_IF_FAILED(WriteDataIgnoreResponse("INQUIRY %d 2 %s", (INT32U)(timeoutSec / 1.28f), s_InquireCODList[m_nextInquireIndex]));
                m_currentCOD = s_InquireCODList[m_nextInquireIndex];
                m_nextInquireIndex++;
                if (m_nextInquireIndex >= sizeof(s_InquireCODList) / sizeof(CHAR8 *))
                {
                    m_nextInquireIndex = 0;
                }
            }
            else
            {
                RETURN_IF_FAILED(WriteDataIgnoreResponse("INQUIRY %d", (INT32U)(timeoutSec / 1.28f)));
            }
            m_scanTimer.ResetTime(timeoutSec + 1);
            m_scanState = SS_WAIT_FOR_SCAN_RESPONSE;
            break;

        case SS_WAIT_FOR_SCAN_RESPONSE:
            RETURN_EC_IF_TRUE(ERROR_OPERATION_TIMED_OUT, m_scanTimer.IsTimeExpired());
            RETURN_EC_IF_TRUE(ERROR_FAILED, m_CancelCurrentCommand);

            {
                list<string> responseStrings;
                if (SUCCEEDED(ReceiveData(responseStrings, 100)))
                {
                    list<string>::iterator it;

                    for (it = responseStrings.begin(); it != responseStrings.end(); it++)
                    {
                        vector<string> inquiryResults;
                        RETURN_IF_FAILED(SplitString(inquiryResults, *it, ' ', true));

                        if (inquiryResults[0] == "INQU_OK")
                        {
                            m_scanState = SS_CLEANUP;
                            break;
                        }
                        else if (inquiryResults.size() >= 5 && inquiryResults[0] == "INQUIRY")
                        {
                            shared_ptr<CBTEADetectedDevice> pDevice;

                            // The BT device name can have  a space in it, which throws the parsing off.
                            string btAddress = inquiryResults[1];

                            vector<string> deviceNameStrings;
                            RETURN_IF_FAILED(SplitString(deviceNameStrings, *it, '\"', true));

                            if (deviceNameStrings.size() == 3)
                            {
                                string deviceName = Utf8Decode(deviceNameStrings[1]);

                                vector<string> remainingStrings;
                                deviceNameStrings[2] = deviceNameStrings[2].substr(1);
                                RETURN_IF_FAILED(SplitString(remainingStrings, deviceNameStrings[2], ' ', true));

                                if (remainingStrings.size() == 2)
                                {
                                    string deviceCOD = remainingStrings[0];
                                    string rssi = remainingStrings[1];
                                    INT32U deviceCODValue = strtol(remainingStrings[0].c_str(), NULL, 16);

                                    if ((!scanAllDevices && to_lower(deviceCOD) == to_lower(m_currentCOD)) ||
                                        (scanAllDevices && (deviceCODValue & 0x200400) == 0x200400))
                                    {
                                        list<shared_ptr<CBTEADetectedDevice> >::iterator deviceListIt;
                                        for (deviceListIt = detectedDeviceList.begin(); deviceListIt != detectedDeviceList.end(); deviceListIt++)
                                        {
                                            if ((*deviceListIt)->m_btAddress == btAddress)
                                            {
                                                pDevice = *deviceListIt;
                                                break;
                                            }
                                        }

                                        if (pDevice == NULL)
                                        {
                                            pDevice = make_shared<CBTEADetectedDevice>();
                                            detectedDeviceList.push_back(pDevice);
                                        }

                                        pDevice->m_btAddress = btAddress;

                                        if (deviceName != "UNKNOWN" && pDevice->m_btDeviceName != deviceName)
                                        {
                                            pDevice->m_btDeviceName = deviceName;
                                        }
                                        pDevice->m_btCOD = deviceCOD;
                                        pDevice->m_btRSSI = rssi;
                                        pDevice->timeoutCounter = CBTEADetectedDevice::RELOAD_TIMER;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            PetBTAdapterWatchdog();
            break;

        case SS_CLEANUP:
            if (!detectedDeviceList.empty())
            {
                // filter out any detected devices that are our own BT Adapters
                shared_ptr<IBTAdapterStatusTable> pStatusTable;
                CIO::g_pNode->GetBTAdapterStatusTable(pStatusTable);
                if (pStatusTable != NULL)
                {
                    list<shared_ptr<CBTEADetectedDevice> >::iterator it;
                    for (it = detectedDeviceList.begin(); it != detectedDeviceList.end();)
                    {
                        if (pStatusTable->IsIABluetoothAdapter((*it)->m_btAddress))
                        {
                            it = detectedDeviceList.erase(it);
                        }
                        else
                        {
                            it++;
                        }
                    }
                }
            }
            m_scanState = SS_INIT;
            break;
    }

    scanCleanupSe.Dismiss();
    RETURN_EC_IF_FALSE(STATUS_OPERATION_INCOMPLETE, m_scanState == SS_INIT);
    return STATUS_SUCCESS;
}

/********************************************************************************************************
********************************************************************************************************/
ERROR_CODE_T BTASerialDevice::AbortBtDeviceScan(void)
{
    CSimpleLock myLock(&m_CS);
    m_scanAbort = true;
    m_CancelCurrentCommand = true;
    return STATUS_SUCCESS;
}

/********************************************************************************************************
********************************************************************************************************/
ERROR_CODE_T BTASerialDevice::OpenConnection(string btAddress, string &linkIdOut)
{
    CSimpleLock myLock(&m_CS);
    RETURN_EC_IF_FALSE(ERROR_NOT_INITIALIZED, m_IsCommEnabled);
    FlushRxBuffer();
    RETURN_IF_FAILED(WriteDataIgnoreResponse("OPEN %s A2DP 0", btAddress.c_str()));
    TRI_STATE_T success = TS_AUTO;

    list<string> expectedResponseStrings;
    expectedResponseStrings.push_back("OPEN_OK");
    expectedResponseStrings.push_back("OPEN_ERROR");
    expectedResponseStrings.push_back("PAIR_ERROR");

    CTimeDeltaSec scanTimer(10);
    while (!scanTimer.IsTimeExpired() && success == TS_AUTO)
    {
        PetWatchdog();
        RETURN_EC_IF_TRUE(ERROR_FAILED, m_CancelCurrentCommand);

        list<string> responseStrings;
        if (SUCCEEDED(ReceiveData(responseStrings, 100, expectedResponseStrings)))
        {
            list<string>::iterator it;
            for (it = responseStrings.begin(); it != responseStrings.end(); it++)
            {
                vector<string> openResponseStrings;
                RETURN_IF_FAILED(SplitString(openResponseStrings, *it, ' ', true));

                if (openResponseStrings[0] == "OPEN_OK")
                {
                    linkIdOut = openResponseStrings[1];
                    success = TS_TRUE;
                    break;
                }
                else if (openResponseStrings[0] == "OPEN_ERROR" || openResponseStrings[0] == "PAIR_ERROR")
                {
                    success = TS_FALSE;
                    break;
                }
            }
        }

        OSTimeDly(OS_TICKS_PER_SEC);
        PetBTAdapterWatchdog();
    }

    RETURN_EC_IF_FALSE(ERROR_FAILED, success == TS_TRUE);
    return STATUS_SUCCESS;
}

/********************************************************************************************************
********************************************************************************************************/
ERROR_CODE_T BTASerialDevice::PairDevice(string btAddress, string &linkIdOut)
{
    CSimpleLock myLock(&m_CS);
    RETURN_EC_IF_FALSE(ERROR_NOT_INITIALIZED, m_IsCommEnabled);
    FlushRxBuffer();
    RETURN_IF_FAILED(WriteDataIgnoreResponse("PAIR %s", btAddress.c_str()));
    TRI_STATE_T success = TS_AUTO;

    list<string> expectedResponseStrings;
    expectedResponseStrings.push_back("PAIR_OK");
    expectedResponseStrings.push_back("PAIR_ERROR");

    CTimeDeltaSec scanTimer(10);
    while (!scanTimer.IsTimeExpired() && success == TS_AUTO)
    {
        PetWatchdog();
        RETURN_EC_IF_TRUE(ERROR_FAILED, m_CancelCurrentCommand);

        list<string> responseStrings;
        if (SUCCEEDED(ReceiveData(responseStrings, 100, expectedResponseStrings)))
        {
            list<string>::iterator it;
            for (it = responseStrings.begin(); it != responseStrings.end(); it++)
            {
                vector<string> openResponseStrings;
                RETURN_IF_FAILED(SplitString(openResponseStrings, *it, ' ', true));

                if (openResponseStrings[0] == "PAIR_OK")
                {
                    success = TS_TRUE;
                    break;
                }
                else if (openResponseStrings[0] == "PAIR_ERROR")
                {
                    success = TS_FALSE;
                    break;
                }
            }
        }

        OSTimeDly(OS_TICKS_PER_SEC);
        PetBTAdapterWatchdog();
    }

    RETURN_EC_IF_FALSE(ERROR_FAILED, success == TS_TRUE);
    return STATUS_SUCCESS;
}

/********************************************************************************************************
********************************************************************************************************/
ERROR_CODE_T BTASerialDevice::CloseConnection(string linkId)
{
    CSimpleLock myLock(&m_CS);
    RETURN_EC_IF_FALSE(ERROR_NOT_INITIALIZED, m_IsCommEnabled);
    FlushRxBuffer();
    RETURN_IF_FAILED(WriteDataIgnoreResponse("CLOSE %s", linkId.c_str()));
    bool success = false;

    CTimeDeltaSec scanTimer(10);
    while (!scanTimer.IsTimeExpired() && !success)
    {
        PetWatchdog();
        RETURN_EC_IF_TRUE(ERROR_FAILED, m_CancelCurrentCommand);

        list<string> responseStrings;
        if (SUCCEEDED(ReceiveData(responseStrings, 100)))
        {
            list<string>::iterator it;
            for (it = responseStrings.begin(); it != responseStrings.end(); it++)
            {
                vector<string> closeResponseStrings;
                RETURN_IF_FAILED(SplitString(closeResponseStrings, *it, ' ', true));

                if (closeResponseStrings.size() >= 2 && closeResponseStrings[0] == "CLOSE_OK" && closeResponseStrings[1] == linkId)
                {
                    success = true;
                    break;
                }
            }
        }

        OSTimeDly(OS_TICKS_PER_SEC);
        PetBTAdapterWatchdog();
    }

    RETURN_EC_IF_FALSE(ERROR_FAILED, success);
    return STATUS_SUCCESS;
}

/********************************************************************************************************
********************************************************************************************************/
ERROR_CODE_T BTASerialDevice::SendPlayCommand(string linkId)
{
    CSimpleLock myLock(&m_CS);
    RETURN_EC_IF_FALSE(ERROR_NOT_INITIALIZED, m_IsCommEnabled);
    RETURN_IF_FAILED(WriteData("MUSIC %s PLAY", linkId.c_str()));
    return STATUS_SUCCESS;
}

/********************************************************************************************************
                                ERROR_CODE_T FlushRxBuffer( INT32U timeoutMS = 50 )
    This flushes the serial receive buffer by reading data out and returning when no more data is read.
    This should get the packet framing lined up.
********************************************************************************************************/
ERROR_CODE_T BTASerialDevice::FlushRxBuffer(INT32U timeoutMS)
{
    m_CancelCurrentCommand = false;
    INT32U maxRetryCounter = 10;
    static const INT32U MS_PER_TICK = 1000 / OS_TICKS_PER_SEC;
    INT32U ticks = timeoutMS / MS_PER_TICK;
    if (ticks % MS_PER_TICK)
    {
        ticks++;
    }

    list<string> responseStrings;
    while (maxRetryCounter-- && !m_CancelCurrentCommand)
    {
        OSTimeDly(ticks);
        if (FAILED(ReceiveData(responseStrings, 10)))
        {
            break;
        }
    }

    m_rxBytesProcessed = m_rxBytesReceived = 0;
    return STATUS_SUCCESS;
}

/********************************************************************************************************
********************************************************************************************************/
ERROR_CODE_T BTASerialDevice::CancelCurrentCommand(void)
{
    m_CancelCurrentCommand = true;
    return STATUS_SUCCESS;
}

/********************************************************************************************************
********************************************************************************************************/
ERROR_CODE_T BTASerialDevice::SimulateConnectivityLoss(bool connectionLost)
{
    m_DiscardReceivedData = true;
    return STATUS_SUCCESS;
}

ERROR_CODE_T BTASerialDevice::SetCfgValue(string cfgOption, string value)
{
    string setString = "Set " + cfgOption + "=" + value;
    return WriteData(setString.c_str());
}

/********************************************************************************************************
                ERROR_CODE_T SetCfgValue( CHAR8 *pCfgOption, const CHAR8 *pFormatStr, ... )
    Sets a configuration value. The message will be formatted as [pCfgOption]=[pFormattedString]
********************************************************************************************************/
ERROR_CODE_T BTASerialDevice::SetCfgValue(CHAR8 *pCfgOption, const CHAR8 *pFormatStr, ...)
{
    CSimpleLock myLock(&m_CS);
    RETURN_EC_IF_NULL(ERROR_INVALID_PARAMETER, pCfgOption);
    RETURN_EC_IF_NULL(ERROR_INVALID_PARAMETER, pFormatStr);
    CHAR8 *pBuffer = new CHAR8[strlen(pCfgOption) + strlen(pFormatStr) + 6];
    sprintf(pBuffer, "Set %s=%s", pCfgOption, pFormatStr);

    va_list args;
//    va_start(args, pFormatMsg);
    ERROR_CODE_T result = WriteData(pBuffer, args);
//    va_end(args);
    delete[] pBuffer;
    return result;
}

/********************************************************************************************************
                ERROR_CODE_T SetCfgValue( CHAR8 *pCfgOption, const string &formatStr, ... )
    Sets a configuration value. The message will be formatted as [pCfgOption]=[pFormattedString]
********************************************************************************************************/
ERROR_CODE_T BTASerialDevice::SetCfgValue(CHAR8 *pCfgOption, const string &formatStr, ...)
{
    CSimpleLock myLock(&m_CS);
    RETURN_EC_IF_NULL(ERROR_INVALID_PARAMETER, pCfgOption);

    CHAR8 *pBuffer = new CHAR8[strlen(pCfgOption) + formatStr.size() + 6];
    sprintf(pBuffer, "Set %s=%s", pCfgOption, formatStr.c_str());

    va_list args;
//    va_start(args, pFormatMsg);
    ERROR_CODE_T result = WriteData(pBuffer, args);
//    va_end(args);
    delete[] pBuffer;
    return result;
}

/********************************************************************************************************
        ERROR_CODE_T SetCfgValueIgnoreResponse( CHAR8 *pCfgOption, const CHAR8 *pFormatStr, ... )
    Sets a configuration value. The message will be formatted as [pCfgOption]=[pFormattedString]
********************************************************************************************************/
ERROR_CODE_T BTASerialDevice::SetCfgValueIgnoreResponse(CHAR8 *pCfgOption, const CHAR8 *pFormatStr, ...)
{
    CSimpleLock myLock(&m_CS);
    RETURN_EC_IF_NULL(ERROR_INVALID_PARAMETER, pCfgOption);
    RETURN_EC_IF_NULL(ERROR_INVALID_PARAMETER, pFormatStr);
    CHAR8 *pBuffer = new CHAR8[strlen(pCfgOption) + strlen(pFormatStr) + 6];
    sprintf(pBuffer, "Set %s=%s", pCfgOption, pFormatStr);

    va_list args;
//    va_start(args, pFormatMsg);
    ERROR_CODE_T result = WriteData(pBuffer, args, true);
    FlushRxBuffer(75);
//    va_end(args);
    delete[] pBuffer;
    return result;
}

/********************************************************************************************************
        ERROR_CODE_T SetCfgValueIgnoreResponse( CHAR8 *pCfgOption, const string &formatStr, ... )
    Sets a configuration value. The message will be formatted as [pCfgOption]=[pFormattedString]
********************************************************************************************************/
ERROR_CODE_T BTASerialDevice::SetCfgValueIgnoreResponse(CHAR8 *pCfgOption, const string &formatStr, ...)
{
    CSimpleLock myLock(&m_CS);
    RETURN_EC_IF_NULL(ERROR_INVALID_PARAMETER, pCfgOption);

    CHAR8 *pBuffer = new CHAR8[strlen(pCfgOption) + formatStr.size() + 6];
    sprintf(pBuffer, "Set %s=%s", pCfgOption, formatStr.c_str());

    va_list args;
//    va_start(args, pFormatMsg);
    ERROR_CODE_T result = WriteData(pBuffer, args, true);
    FlushRxBuffer(75);
//    va_end(args);
    delete[] pBuffer;
    return result;
}

/********************************************************************************************************
                ERROR_CODE_T GetCfgValue( string &outString, const CHAR8 *pCfgOption )
    Gets a configuration value from the BT device. The returned string will be set to the outString
    reference. This will be stripped of the "pCfgoption=" portion of the response.
********************************************************************************************************/
ERROR_CODE_T BTASerialDevice::GetCfgValue(string &outString, const CHAR8 *pCfgOption)
{
    CSimpleLock myLock(&m_CS);
    RETURN_EC_IF_NULL(ERROR_INVALID_PARAMETER, pCfgOption);
    vector<string> outStrings;
    RETURN_IF_FAILED(ReadData(outStrings, "Get %s", pCfgOption));
    RETURN_EC_IF_FALSE(ERROR_FAILED, outStrings.size() > 0);
    INT32S pos = outStrings[0].find(pCfgOption);
    RETURN_EC_IF_TRUE(ERROR_FAILED, pos == string::npos);
    outString = outStrings[0].substr(outStrings[0].find("=") + 1);
    return STATUS_SUCCESS;
}

/********************************************************************************************************
                ERROR_CODE_T GetCfgValue( string &outString, const string &cfgOption )
    Gets a configuration value from the BT device. The returned string will be set to the outString
    reference. This will be stripped of the "pCfgoption=" portion of the response.
********************************************************************************************************/
ERROR_CODE_T BTASerialDevice::GetCfgValue(string &outString, const string &cfgOption)
{
    CSimpleLock myLock(&m_CS);
    vector<string> outStrings;
    RETURN_IF_FAILED(ReadData(outStrings, "Get %s", cfgOption.c_str()));
    RETURN_EC_IF_FALSE(ERROR_FAILED, outStrings.size() > 0);
    INT32S pos = outStrings[0].find(cfgOption);
    RETURN_EC_IF_TRUE(ERROR_FAILED, pos == string::npos);
    outString = outStrings[0].substr(outStrings[0].find("=") + 1);
    return STATUS_SUCCESS;
}

/********************************************************************************************************
                  ERROR_CODE_T WriteData( const CHAR8 *pFormatMsg, va_list args )
    A private function that write the data to the uArt.
********************************************************************************************************/
ERROR_CODE_T BTASerialDevice::WriteData(const CHAR8 *pFormatMsg, va_list args)
{
    return WriteData(pFormatMsg, args, false);
}

ERROR_CODE_T BTASerialDevice::WriteData(const CHAR8 *pFormatMsg, va_list args, bool ignoreResponse)
{
    CSimpleLock myLock(&m_CS);
    m_CancelCurrentCommand = false;
    shared_ptr<IUart> pUart = m_pUart.lock();
    RETURN_EC_IF_NULL(ERROR_NOT_INITIALIZED, pUart.get());
    RETURN_EC_IF_FALSE(ERROR_NOT_INITIALIZED, m_IsCommEnabled);
    RETURN_EC_IF_NULL(ERROR_INVALID_PARAMETER, pFormatMsg);
    INT32U retryCounter = 3;
    INT32S strLength = 0;
    ERROR_CODE_T result = STATUS_SUCCESS;

    // Format the message string.
    strLength += vsnprintf(&m_TxBuffer[strLength], MAX_PKT_LEN - (strLength + 2), pFormatMsg, args);
    RETURN_EC_IF_TRUE(ERROR_INVALID_PARAMETER, strLength < 0 || strLength > MAX_PKT_LEN - 2);
    m_TxBuffer[strLength++] = '\r';
    m_TxBuffer[strLength] = '\0';

    // We need to have some retries because there is no synchronization values in the protocol
    // that will get us back in phase if we receive a partial packet.
    while (retryCounter)
    {
        if (!ignoreResponse)
        {
            FlushRxBuffer();
        }

        INT32U writeLength = 0;
        // Send out the message string.
        DebugPrintf(m_PacketVerbosity, DEBUG_NO_LOGGING, m_DebugID, "Sending Message: %s", (unsigned char *)m_TxBuffer);
        pUart->WritePort((unsigned char *)m_TxBuffer, strLength, &writeLength);

        if (strLength != writeLength)
        {
            DebugPrintf(m_PacketVerbosity, DEBUG_NO_LOGGING, m_DebugID, "Failed to send message: %s", (unsigned char *)m_TxBuffer);
            result = ERROR_FAILED;
            break;
        }

        if (ignoreResponse)
        {
            break;
        }

        list<string> responses;
        result = ReceiveData(responses, s_defaultResponseTimeout);
        if (SUCCEEDED(result))
            break;

        retryCounter--;
    }

    return result;
}

/********************************************************************************************************
********************************************************************************************************/
bool BTASerialDevice::CommandIsInList(const string &command, const list<string> &commandList)
{
    list<string>::const_iterator it;
    for (it = commandList.begin(); it != commandList.end(); it++)
    {
        string compareCommand = *it;

        if (command.compare(0, compareCommand.size(), compareCommand) == 0)
        {
            return true;
        }
    }

    return false;
}


/********************************************************************************************************
ERROR_CODE_T CBTEAComm::ReceiveData(list<string> &responses, INT32U timeoutMS = 200, const list<string> &expectedResponses = GetDefaultList())

    Waits for a response to a command. This attempts to discard notification messages and just return
    when the actual message response is loaded in m_RxBuffer. The returned response will be at
    m_RxBuffer[0]. the response ends when one of the following are found: "OK\r", "PENDING...\r", or
    "ERROR ...\r".
********************************************************************************************************/
ERROR_CODE_T BTASerialDevice::ReceiveData(list<string> &responses, INT32U timeoutMS, const list<string> &expectedResponses)
{
    responses.clear();
    shared_ptr<IUart> pUart = m_pUart.lock();
    RETURN_EC_IF_NULL(ERROR_NOT_INITIALIZED, pUart.get());

    CTimeDelta rxTimeout(timeoutMS);
    do
    {
        OSTimeDly(1);
        INT32U bytesReceived = 0;
        pUart->ReadPort((INT8U *)&m_RxBuffer[m_rxBytesReceived], MAX_PKT_LEN - m_rxBytesReceived, &bytesReceived);
        if (bytesReceived)
        {
            m_rxBytesReceived += bytesReceived;
            rxTimeout.ResetTime(s_defaultInterByteGapTimeout);
        }
    } while (!rxTimeout.IsTimeExpired() && m_rxBytesReceived < MAX_PKT_LEN && !m_CancelCurrentCommand);

    if (m_DiscardReceivedData)
    {
        m_rxBytesProcessed = m_rxBytesReceived = 0;
        return ERROR_OPERATION_TIMED_OUT;
    }

    INT32U strStartIdx = 0;
    bool acknowledgeReceived = false;
    list<string> unsolicitedMessages;

    while (m_rxBytesProcessed < m_rxBytesReceived && !m_CancelCurrentCommand)
    {
        if (m_RxBuffer[m_rxBytesProcessed] == '\r')
        {
            string command = string(&m_RxBuffer[strStartIdx], m_rxBytesProcessed - strStartIdx);

            if (command.compare(0, 7, "PENDING") == 0 || command.compare(0, 2, "OK") == 0 ||
                command.compare(0, 7, "INQU_OK") == 0 || command.compare(0, 10, "OPEN_ERROR") == 0 ||
                command.compare(0, 4, "NAME") == 0 || command.compare(0, 12, "PAIR_PENDING") == 0 ||
                CommandIsInList(command, expectedResponses))
            {
                if (strncmp(&m_RxBuffer[strStartIdx], "OPEN_OK", 7) == 0)
                {
                    m_receivedOpenNotification = true;
                }

                responses.push_back(command);
                DebugPrintf(m_PacketVerbosity, DEBUG_NO_LOGGING, m_DebugID, "Received Response: %s", command.c_str());
                acknowledgeReceived = true;
            }
            else if (command.compare(0, 5, "ERROR") == 0)
            {
                responses.push_back(command);
                INT32U errorCode = strtol(command.substr(6).c_str(), NULL, 16);

                BT_AUDIO_ERROR_CODE const *pErrorCode = NULL;
                for (INT32U i = 0; i < sizeof(s_ErrorCodeList) / sizeof(s_ErrorCodeList[0]); i++)
                {
                    if (errorCode == s_ErrorCodeList[i].errorCode)
                    {
                        pErrorCode = &s_ErrorCodeList[i];
                        break;
                    }
                }

                if (errorCode != 0x0012)
                {
                    DebugPrintf(m_PacketVerbosity, DEBUG_NO_LOGGING, m_DebugID, "Received Error: %s, %s",
                                command.c_str(), (pErrorCode == NULL ? "null" : pErrorCode->pMsgString));
                }

                if (errorCode == 0x19)
                {
                    if (!acknowledgeReceived)
                    {
                        responses.push_back(string(&m_RxBuffer[strStartIdx], m_rxBytesProcessed - strStartIdx));
                    }
                }
            }
            else
            {
                bool isUnsolictedMessage = false;

                for (int cmdIdx = 0; cmdIdx < sizeof(s_UnsolictedCommands) / sizeof(CHAR8 *); cmdIdx++)
                {
                    if (command.compare(0, strlen(s_UnsolictedCommands[cmdIdx]), s_UnsolictedCommands[cmdIdx]) == 0)
                    {
                        // This is some unsolicited message.
                        unsolicitedMessages.push_back(command);
                        DebugPrintf(m_PacketVerbosity, DEBUG_NO_LOGGING, m_DebugID, "Received Unsolicited Message: %s", command.c_str());
                        isUnsolictedMessage = true;
                        break;
                    }
                }

                if (!isUnsolictedMessage)
                {
                    responses.push_back(command);
                    DebugPrintf(m_PacketVerbosity, DEBUG_NO_LOGGING, m_DebugID, "Received Response: %s", command.c_str());
                    acknowledgeReceived = true;
                }
            }

            strStartIdx = (m_rxBytesProcessed + 1);
        }

        m_rxBytesProcessed++;
    }

    if (strStartIdx < m_rxBytesReceived)
    {
        memcpy(m_RxBuffer, &m_RxBuffer[strStartIdx], m_rxBytesReceived - strStartIdx);
        m_rxBytesReceived -= strStartIdx;
        m_rxBytesProcessed = m_rxBytesReceived;
    }
    else
    {
        m_rxBytesProcessed = m_rxBytesReceived = 0;
    }

    list<string>::iterator it;
    for (it = unsolicitedMessages.begin(); it != unsolicitedMessages.end(); it++)
    {
        ProcessUnsolicitedMessage(*it);
    }

    return acknowledgeReceived ? STATUS_SUCCESS : ERROR_OPERATION_TIMED_OUT;
}

/********************************************************************************************************
********************************************************************************************************/
ERROR_CODE_T BTASerialDevice::ProcessUnsolicitedMessage(string command)
{
    OnUnsolicitedMessageReceived.notifyObservers(command);
    return STATUS_SUCCESS;
}

/********************************************************************************************************
********************************************************************************************************/
string BTASerialDevice::Utf8Decode(string utf8EncodedString)
{
    string outputString = "";
    string utf8Char = "";

    for (INT32U inIdx = 0; inIdx < utf8EncodedString.size(); inIdx++)
    {
        INT8U tempChar = (INT8U)utf8EncodedString[inIdx];
        if ((tempChar & 0xC0) != 0x80 && !utf8Char.empty())
        {
            INT8U decodedChar = Utf8DecodeChar(utf8Char);
            if (decodedChar != '\0')
            {
                outputString.push_back(decodedChar);
            }
            utf8Char = "";
        }

        if ((tempChar & 0x80) == 0)
        {
            outputString.push_back(tempChar);
        }
        else
        {
            utf8Char.push_back(tempChar);
        }
    }

    if (!utf8Char.empty())
    {
        CHAR8 tempChar = Utf8DecodeChar(utf8Char);
        if (tempChar != '\0')
        {
            outputString.push_back(tempChar);
        }
    }

    return outputString;
}

/********************************************************************************************************
********************************************************************************************************/
CHAR8 BTASerialDevice::Utf8DecodeChar(string utf8EncodedChar)
{
    INT16U charEncode = 0;

    if ((utf8EncodedChar[0] & 0x80) == 0x00)
    {
        charEncode = utf8EncodedChar[0];
    }
    else if ((utf8EncodedChar[0] & 0xE0) == 0xC0)
    {
        charEncode = (utf8EncodedChar[0] & 0x1F);
    }
    else
    {
        charEncode = (utf8EncodedChar[0] & 0x0F);
    }

    for (INT32U inIdx = 1; inIdx < utf8EncodedChar.size(); inIdx++)
    {
        charEncode <<= 6;
        charEncode |= (utf8EncodedChar[inIdx] & 0x3F);
    }

    if (charEncode < 0x80)
    {
        return (CHAR8)charEncode;
    }

    if (charEncode == 0x2010 || charEncode == 0x2012 || charEncode == 0x2013 || charEncode == 0x2014 || charEncode == 0x2015)
    {
        return '-';
    }
    if (charEncode == 0x2018 || charEncode == 0x2019 || charEncode == 0x201B)
    {
        return '\'';
    }
    if (charEncode == 0x201A)
    {
        return ',';
    }
    if (charEncode == 0x201C || charEncode == 0x201D || charEncode == 0x201F)
    {
        return '\"';
    }
    if (charEncode == 0x2024)
    {
        return '.';
    }

    return '\0';
}