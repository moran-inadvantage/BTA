#include <memory>

#include "types.h"
#include "BTADeviceDriver.h"
#include "BTASerialDevice.h"

BTADeviceDriver::BTADeviceDriver()
{
    m_PacketVerbosity = 0;
}

BTADeviceDriver::~BTADeviceDriver()
{

}

ERROR_CODE_T BTADeviceDriver::EnterCommandMode()
{
    RETURN_EC_IF_NULL(ERROR_FAILED, m_pBTASerialDevice);
    // Will not get a response if we're already in data mode
    m_pBTASerialDevice->WriteData("$$$$");

    return STATUS_SUCCESS;
}

ERROR_CODE_T BTADeviceDriver::SendReset()
{
    RETURN_EC_IF_NULL(ERROR_FAILED, m_pBTASerialDevice);
    // Will not get a valid response, just resets us
    m_pBTASerialDevice->WriteData("RESET");

    RETURN_IF_FAILED(m_pBTASerialDevice->FlushRxBuffer(100));

    return STATUS_SUCCESS;
}