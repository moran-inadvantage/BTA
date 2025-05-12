#include <memory>

#include "types.h"
#include "BTADeviceDriver.h"
#include "BTASerialDevice.h"

BTADeviceDriver::BTADeviceDriver()
{
}

BTADeviceDriver::~BTADeviceDriver()
{

}

ERROR_CODE_T BTADeviceDriver::EnterCommandMode()
{
    RETURN_EC_IF_NULL(ERROR_FAILED, m_pBTASerialDevice);
    RETURN_IF_FAILED(m_pBTASerialDevice->WriteData("$$$$"));
    RETURN_IF_FAILED(m_pBTASerialDevice->FlushRxBuffer(100));
}

ERROR_CODE_T BTADeviceDriver::SendReset()
{
    RETURN_EC_IF_NULL(ERROR_FAILED, m_pBTASerialDevice);
    RETURN_IF_FAILED(m_pBTASerialDevice->WriteData("RESET"));
    RETURN_IF_FAILED(m_pBTASerialDevice->FlushRxBuffer(100));
}