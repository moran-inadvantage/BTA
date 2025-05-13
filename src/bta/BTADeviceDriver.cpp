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

ERROR_CODE_T BTADeviceDriver::GetBaudrate(BAUDRATE &baudrate)
{
    RETURN_EC_IF_NULL(ERROR_FAILED, m_pBTASerialDevice);
    RETURN_IF_FAILED(m_pBTASerialDevice->GetBaudrate(baudrate));

    return STATUS_SUCCESS;
}

bool BTADeviceDriver::IsCommEnabled(void)
{
    return true;
}

ERROR_CODE_T BTADeviceDriver::WatchdogPet()
{
    RETURN_EC_IF_NULL(ERROR_FAILED, m_pBTASerialDevice);

    RETURN_IF_FAILED(m_pBTASerialDevice->PetBTAdapterWatchdog());

    return STATUS_SUCCESS;
}

/********************************************************************************************************
                             ERROR_CODE_T GetLocalAddress( void )
Retrieves the public bluetooth address from the module. This is used as part of the device name.
********************************************************************************************************/
ERROR_CODE_T BTADeviceDriver::GetLocalAddress(void)
{
    string retString;

    RETURN_EC_IF_NULL(ERROR_FAILED, m_pBTASerialDevice);

    RETURN_IF_FAILED(m_pBTASerialDevice->GetCfgValue(retString, "LOCAL_ADDR"));

    // get first 12 bytes of ret string and put it input public address
    if (retString.length() < 12)
    {
        return ERROR_FAILED;
    }

    m_PublicAddress = retString.substr(0, 12);

    return STATUS_SUCCESS;
}