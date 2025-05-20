#pragma once

#include <gmock/gmock.h>

#include "iuart.h"
#include "types.h"

class BTASerialDevice {
public:
    MOCK_METHOD(ERROR_CODE_T, SetUArt, (std::weak_ptr<IUart> puArt));
    MOCK_METHOD(ERROR_CODE_T, GetBaudrate, (BAUDRATE &baudrate));
    MOCK_METHOD(ERROR_CODE_T, SetBaudrate, (BAUDRATE baudrate));
    MOCK_METHOD(ERROR_CODE_T, SetCommEnable, (bool enabled));
    MOCK_METHOD(BOOLEAN, IsCommEnabled, ());
    MOCK_METHOD(ERROR_CODE_T, SetPacketVerbosity, (INT8U verbosity));
    MOCK_METHOD(INT8U, GetPacketVerbosity, ());
    MOCK_METHOD(ERROR_CODE_T, SetDebugID, (CHAR8 (&debugID)[OS_TASK_NAME_SIZE]));
    MOCK_METHOD(ERROR_CODE_T, SetCardNumber, (INT8U cardNumber));
    MOCK_METHOD(ERROR_CODE_T, PetBTAdapterWatchdog, (bool flushRxBuffer));
    MOCK_METHOD(ERROR_CODE_T, ReadData, (std::vector<std::string> &outStrings, std::string message));
    MOCK_METHOD(ERROR_CODE_T, WriteData, (std::string message, bool ignoreResponse));
    MOCK_METHOD(ERROR_CODE_T, ReadVerifyWriteCfgData, (const std::string cfgOption, std::string expectedResult, bool* optionWasSet));
    MOCK_METHOD(ERROR_CODE_T, GetCfgValue, (std::string &outString, const std::string cfgOption));
    MOCK_METHOD(ERROR_CODE_T, SetCfgValue, (std::string cfgOption, std::string value, bool ignoreResponse));
    MOCK_METHOD(ERROR_CODE_T, FlushRxBuffer, (INT32U timeoutMS));
    MOCK_METHOD(ERROR_CODE_T, CancelCurrentCommand, ());
    MOCK_METHOD(ERROR_CODE_T, SimulateConnectivityLoss, (bool connectionLost));
    MOCK_METHOD(bool, GetCancelCurrentCommand, ());
    MOCK_METHOD(void, SetCancelCurrentCommand, (bool cancel));
};
