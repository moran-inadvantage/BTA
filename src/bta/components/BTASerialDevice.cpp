#include <cstdarg>
#include <string.h>

#include "BTASerialDevice.h"
#include "ExtIO.h"
#include "IO.h"

/********************************************************************************************************
                                    Constructor
********************************************************************************************************/
BTASerialDevice::BTASerialDevice()
    : m_IsCommEnabled(false), m_CancelCurrentCommand(false), m_DiscardReceivedData(false),
      m_PacketVerbosity(DEBUG_NO_LOGGING), m_rxBytesProcessed(0),
      m_rxBytesReceived(0), m_CardNumber(0), m_curBaudrate(BAUDRATE_9600)
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
    m_DiscardReceivedData = false;
    return STATUS_SUCCESS;
}

/********************************************************************************************************
                                bool IsCommEnabled( void )
    Returns the state of the Comm Enabled flag.
********************************************************************************************************/
BOOLEAN BTASerialDevice::IsCommEnabled(void)
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
          ERROR_CODE_T ReadData( vector<string> &outStrings, const CHAR8 *pFormatMsg, ... )
    Sends the command in pFormatMsg and returns the response in the outStrings vector.
********************************************************************************************************/
ERROR_CODE_T BTASerialDevice::ReadData(vector<string> &outStrings, string message)
{
    CSimpleLock myLock(&m_CS);

    RETURN_IF_FAILED(WriteData(message, true));

    list<string> responseStrings;
    RETURN_IF_FAILED(ReceiveData(responseStrings, 100));
    outStrings = vector<string>(responseStrings.begin(), responseStrings.end());

    return STATUS_SUCCESS;
}

ERROR_CODE_T BTASerialDevice::ReadVerifyWriteCfgData(const string cfgOption, string expectedResult, bool *optionWasSet)
{
    string retString;
    *optionWasSet = false;
    RETURN_IF_FAILED(GetCfgValue(retString, cfgOption));

    if (retString.find(expectedResult) == string::npos)
    {
        SetCfgValue(const_cast<CHAR8 *>(cfgOption.c_str()), expectedResult.c_str());
    }

    // Verify we set it correctly
    RETURN_IF_FAILED(GetCfgValue(retString, cfgOption));

    RETURN_EC_IF_TRUE(ERROR_FAILED, retString.find(expectedResult) == string::npos);

    *optionWasSet = true;

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

ERROR_CODE_T BTASerialDevice::SetCfgValue(string cfgOption, string value, bool ignoreResponse)
{
    string setString = "Set " + cfgOption + "=" + value;
    return WriteData(setString, ignoreResponse);
}

/********************************************************************************************************
                ERROR_CODE_T GetCfgValue( string &outString, const string &cfgOption )
    Gets a configuration value from the BT device. The returned string will be set to the outString
    reference. This will be stripped of the "pCfgoption=" portion of the response.
********************************************************************************************************/
ERROR_CODE_T BTASerialDevice::GetCfgValue(string &outString, const string cfgOption)
{
    CSimpleLock myLock(&m_CS);
    vector<string> outStrings;
    RETURN_IF_FAILED(ReadData(outStrings, "Get " + cfgOption));
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

ERROR_CODE_T BTASerialDevice::WriteData(string data, bool ignoreResponse)
{
    CSimpleLock myLock(&m_CS);
    m_CancelCurrentCommand = false;
    shared_ptr<IUart> pUart = m_pUart.lock();
    RETURN_EC_IF_NULL(ERROR_NOT_INITIALIZED, pUart.get());
    RETURN_EC_IF_FALSE(ERROR_NOT_INITIALIZED, m_IsCommEnabled);
    INT32U retryCounter = 3;
    INT32S strLength = 0;
    ERROR_CODE_T result = STATUS_SUCCESS;

    // Format the message string.
    data += "\r";
    strLength = data.length();
    memcpy(m_TxBuffer, data.c_str(), strLength);

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
        DebugPrintf(DEBUG_TRACE, DEBUG_TRACE, m_DebugID, "Sending Message: %s", (unsigned char *)m_TxBuffer);
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
BOOLEAN BTASerialDevice::CommandIsInList(const string &command, const list<string> &commandList)
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
                    OnOpenNotificationReceived.notifyObservers(true);
                }

                responses.push_back(command);
                DebugPrintf(m_PacketVerbosity, DEBUG_NO_LOGGING, m_DebugID, "Received Response: %s\n", command.c_str());
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
                    DebugPrintf(m_PacketVerbosity, DEBUG_NO_LOGGING, m_DebugID, "Received Response: %s\n", command.c_str());
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
