/********************************************************************************************************
    File Name:  BTAudioCard.cpp

    Author:     Rob Dahlgren

    Notes:      CBTAudioCard class implementation.  See header file for detailed description.

-------------------------------------------------HISTORY-------------------------------------------------

    REV  Date       Initials  Notes
    -----------------------------------------------------------------------------------------------------
    New  10/29/2018  RLD      Initial Release
    -----------------------------------------------------------------------------------------------------

---------------------------------------------------------------------------------------------------------
    © Copyright 2018 Innovative Advantage Inc.  All Rights Reserved.
********************************************************************************************************/

#include "BTAudioCard.h"
#include "ConfigBlock.h"
#include "ConfigChannel.h"
#include "ConfigDriver.h"
#include "DevCS181012.h"
#include "DownloadControl.h"
#include "ErrorCodes.h"
#include "ExtIO.h"
#include "HealthStatusBroadcast.h"
#include "HttpClient.h"
#include "JsonDocument.h"
#include "MQTT.h"
#include "MQTTTopics.h"
#include "ScopeExit.h"
#include "bsp.h"
#include "stdio.h"
#include "stdlib.h"

#define HEARTBEAT_RESET_TIMEOUT 11
#define HEARTBEAT_DEFAULT 10
#define INIT_TIMEOUT_COUNT 1200
#define INVALID_LINK_ID 0xFF
#define STARTUP_RECONNECT_DELAY_SEC 60
#define STANDARD_MONITOR_TIMEOUT 10
#define NEW_LINK_MONITOR_TIMEOUT 3
#define VOLUME_OFFSET 100
#define DEFAULT_BAUDRATE BAUDRATE_9600
#define DESIRED_BAUDRATE BAUDRATE_9600
#define DESIRED_BAUDRATE_6_1_5 BAUDRATE_115200


/****************************************************************************************************
Map desciptor defines what entries of the IO map should be populated by the baseboard.
****************************************************************************************************/
const AVDS1_CARD_MAP_DESCRIPTOR_T CBTAudioCard::m_MapDescriptor =
    {
        FALSE,        // Set to TRUE to get a SPI Manager.
        FALSE,        // Set to TRUE to get a IIC Manager.
        FALSE,        // Set to TRUE to get MISC 1 Bit.
        FALSE,        // Set to TRUE to get MISC 2 Bit.
        FALSE,        // Set to TRUE to get a CPU direct pin.
        0,            // Set to the number of input routers needed.
        {0, 0, 0, 0}, // Set to assign card routes to array elements, i.e. route 0 may be A3/B3.
        0,            // Set to the number of output routers needed.
        {0, 0, 0, 0}, // Set to assign card routes to array elements, i.e. route 0 may be A3/B3.
        FALSE,        // Set to TRUE to get a base audio router (receive from fibers).
        FALSE,        // Set to TRUE to get a base audio router (transmit to fibers).
        FALSE,        // Set to TRUE to get the EEPROM.
        FALSE,        // Set to TRUE to get the BaseBoard IO_Map.
        TRUE          // Set to TRUE to get an IEthernetPortControl interface.
};

/********************************************************************************************************
CBTAudioCard constructor
********************************************************************************************************/
CBTAudioCard::CBTAudioCard(void)
    : m_IsInitialized(false),
      m_pCardMap(NULL),
      m_pInRouter(NULL),
      m_pOutRouter(NULL),
      m_Online(false),
      m_CardNumber(0),
      m_ReadyFlag(READY_FLAG_INVALID),
      m_ConfigState(BT_CONFIG_STATE_START),
      m_FWRevision(FW_REV_UNKNOWN),
      m_InterfaceMainRetryCount(0),
      m_CfgWritePending(false),
      m_ResetPending(false),
      m_QualTestMode(false),
      m_notificationUrl(""),
      m_isInputDevice(true),
      m_isReset(true),
      m_unpairAllDevicesFlag(false),
      m_playTestSongActive(false),
      m_ScanAllDevices(false),
      m_isAutoConnecting(false),
      m_configuredSSPMode(SSP_NO_DISPLAY_NO_KEYBOARD),
      m_PairedBTDevice(""),
      m_desiredBaudrate(DESIRED_BAUDRATE)
{
    m_pCardMsgQueue = make_shared<CCardMsgQueue>();
    m_pMsgQueue = m_pCardMsgQueue.get();
    m_TestModeTimer.ResetTime(1);

#ifdef NDEBUG
    m_BTComm.SetPacketVerbosity(DEBUG_NO_LOGGING);
#else
    m_BTComm.SetPacketVerbosity(DEBUG_TRACE_MESSAGE);
#endif
}

/********************************************************************************************************
                   const AVDS1_CARD_MAP_DESCRIPTOR_T *GetMapDescriptor( void )
This gets the IO map descriptor for this card.
********************************************************************************************************/
const AVDS1_CARD_MAP_DESCRIPTOR_T *CBTAudioCard::GetMapDescriptor(void)
{
    return &m_MapDescriptor;
}

/********************************************************************************************************
                    void SetIOMap( AVDS1_CARD_IO_MAP_T * pIOMap )
This sets the IO map for the card. This will be called periodically and some of the requested data may
not be filled in the first time it is called. The card should endeavor to initialize as much as possible.
The baseboard is responsible for loading the data in a way that doesn't cause us to read bogus data.
********************************************************************************************************/
void CBTAudioCard::SetIOMap(AVDS1_CARD_IO_MAP_T *pMap)
{
    if (m_pCardMap == NULL && pMap != NULL)
    {
        m_pCardMap = pMap;
        m_CardNumber = m_pCardMap->slotNumber;
        sprintf(m_TaskID, "BTAudioCard %d", m_pCardMap->slotNumber);

        m_BTComm.SetDebugID(m_TaskID);
        m_BTComm.SetCardNumber(m_CardNumber);
        vector<shared_ptr<IConfigDriver> > pDrivers;
        CIO::g_pParser->GetDrivers(CIO::m_NodeAddress, ETHERNET_PORT, pDrivers);
        for (INT32U driverIdx = 0; driverIdx < pDrivers.size(); driverIdx++)
        {
            shared_ptr<IConfigDriver> pDriver = pDrivers[driverIdx];
            if (pDriver->GetCardNumber() == m_pCardMap->slotNumber)
            {
                ETHERNET_PORT_ID ethPortId = pDriver->GetEthernetPort();

                if (ethPortId == NULL)
                {
                    m_ReadyFlag = READY_FLAG_EXTENDED;
                    m_ReadyFlagString = string("Bluetooth Module, Card ") + m_pCardMap->slotNumber + " Invalid Ethernet Port Configured";
                }
                else if (ethPortId->GetDeviceType() == ETHERNET_DEVICE_TYPE_AVDS && ethPortId->GetCardNumber() == 0)
                {
                    m_ReadyFlag = (READY_FLAG)(READY_FLAG_BT_MODULE_1 +
                                               (ethPortId->GetPortId() - ETHERNET_PORT_ID_EXTERNAL_PORT1));
                }
                else
                {
                    m_ReadyFlag = READY_FLAG_EXTENDED;
                    m_ReadyFlagString = string("Bluetooth Module ") + ethPortId->GetFullName() + " Not Loaded";
                }
                break;
            }
        }

        SetReadyFlagEx(m_ReadyFlag, m_ReadyFlagString.c_str(), false);
        m_criticalSection.SetCSName(m_TaskID);

        if (m_pHealthStatusBuilder == NULL)
        {
            if (FAILED(CHealthStatusMessageBuilder::Create("BT_Adapter", m_pHealthStatusBuilder)))
            {
                DebugPrintf(DEBUG_TRACE, DEBUG_NO_LOGGING, m_TaskID, "Failed to create Health Status Builder");
            }
        }

        ERROR_CODE_T result = CreateTask(CBTAudioCard::TaskInit, (void *)this, NORMAL_PRIORITY, m_TaskID, &m_ThreadHandle, TRUE);
        if (m_ThreadHandle == 0)
        {
            DebugPrintf(DEBUG_TRACE_MESSAGE, DEBUG_NO_LOGGING, m_TaskID, "Failed to create task. Result: 0x%08X", result);
        }
    }
}

/********************************************************************************************************
      ERROR_CODE_T GetBlock( INT16U channelNumber, BLOCK_USE_T use, PORT_T port,
                            INTU portNumber, BLOCK_TYPE_T blockType, IBlock **ppObject )
This loads a block for the requested port and initializes the hardware.
********************************************************************************************************/
ERROR_CODE_T CBTAudioCard::GetBlock(INT16U channelNumber, BLOCK_USE_T use, PORT_T port,
                                    INTU portNumber, BLOCK_TYPE_T blockType, IBlock **ppObject)
{
    CSimpleLock localLock(&m_criticalSection, 1);
    RETURN_EC_IF_FALSE(ERROR_FAILED, localLock.IsLocked());
    RETURN_EC_IF_FALSE(ERROR_NOT_INITIALIZED, m_IsInitialized);
    RETURN_EC_IF_NULL(ERROR_NOT_INITIALIZED, m_pCobraNetDevice);
    RETURN_EC_IF_FALSE(ERROR_NOT_SUPPORTED, port == GROUP_PORT);
    RETURN_EC_IF_FALSE(ERROR_NOT_SUPPORTED, use == STEREO_AUDIO);
    RETURN_EC_IF_FALSE(ERROR_NOT_SUPPORTED, portNumber == 0);

    if (blockType == INPUT_BLOCK)
    {
        if (m_pInBlock == NULL)
        {
            if (m_pInRouter == NULL)
            {
                RETURN_IF_FAILED(m_pCobraNetDevice->GetCobraNetInputRouter(use, portNumber, &m_pInRouter));
            }

            m_pInBlock = make_shared<CBlock>(m_pCardMsgQueue.get(), INPUT_BLOCK, channelNumber, use, m_pCardMap->slotNumber, port, portNumber, m_pInRouter);
            m_pInBlock->SetStatusBits(BLOCK_STATUS_INITIALIZED);
        }
        else
        {
            RETURN_EC_IF_FALSE(ERROR_NOT_SUPPORTED, m_pInBlock->GetChannelNumber() != channelNumber);
        }

        *ppObject = m_pInBlock.get();
    }
    else
    {
        if (m_pOutBlock == NULL)
        {
            if (m_pOutRouter == NULL)
            {
                RETURN_IF_FAILED(m_pCobraNetDevice->GetCobraNetOutputRouter(use, portNumber, &m_pOutRouter));
                m_pOutRouter->SetProperty(PROP_VOLUME_OFFSET, VOLUME_OFFSET);
            }

            m_pOutBlock = make_shared<CBlock>(m_pCardMsgQueue.get(), OUTPUT_BLOCK, channelNumber, use, m_pCardMap->slotNumber, port, portNumber, m_pOutRouter);
            m_pOutBlock->SetStatusBits(BLOCK_STATUS_INITIALIZED);
        }
        else
        {
            RETURN_EC_IF_FALSE(ERROR_NOT_SUPPORTED, m_pOutBlock->GetChannelNumber() != channelNumber);
        }

        *ppObject = m_pOutBlock.get();
    }
    return STATUS_SUCCESS;
}

/********************************************************************************************************
                                    void ToggleReset( void )
Since we don't have direct control of the Cobranet reset line, this function will halt communication
for a period of time that should cause the BT module to toggle the reset for us.
********************************************************************************************************/
ERROR_CODE_T CBTAudioCard::ToggleReset(void)
{
    RETURN_EC_IF_FALSE(ERROR_NOT_INITIALIZED, m_IsInitialized);
    RETURN_IF_FAILED(m_BTComm.SetCommEnable(false));
    GotoOfflineState();
    OSTimeDly(HEARTBEAT_RESET_TIMEOUT);
    RETURN_IF_FAILED(m_BTComm.SetCommEnable(true));
    m_isReset = false;
    return STATUS_SUCCESS;
}

/********************************************************************************************************
                                        void Reset( bool isReset )
Controls the reset of the Cobranet card in the BT Module by witholding communication. The module will
reset the Cobranet card if there is no communication for a period of time.
********************************************************************************************************/
ERROR_CODE_T CBTAudioCard::Reset(bool isReset)
{
    RETURN_EC_IF_FALSE(ERROR_NOT_INITIALIZED, m_IsInitialized);
    if (m_isReset == isReset)
    {
        return STATUS_SUCCESS;
    }

    m_isReset = isReset;

    if (isReset)
    {
        RETURN_IF_FAILED(m_BTComm.SetCommEnable(false));
        GotoOfflineState();

        // Disable the ethernet port. This is important if we are updating
        // Cobranet firmware because we it's the only way to cut our cobranet
        // off from the others.
        if (m_pCobraNetDevice == NULL || (m_pCobraNetDevice->IsDownloadActive() == TRUE))
            RETURN_IF_FAILED(m_pCardMap->pEthernetControl->DisablePort());
    }
    else
    {
        // Make sure we have waited long enough for the BT module to have reset us.
        ENET_LINK_INFO_T LinkInfo;
        RETURN_IF_FAILED(m_pCardMap->pEthernetControl->EnablePort());
        OSTimeDly(OS_TICKS_PER_SEC);

        RETURN_IF_FAILED(m_pCardMap->pEthernetControl->SetVlanID(VLAN_AUDIO_NET, VLAN_BIT_MASK(VLAN_AUDIO_NET) | VLAN_BIT_MASK(VLAN_AVDS_CPU), FALSE, 0));
        RETURN_IF_FAILED(m_pCardMap->pEthernetControl->ChangeExternalPortMask(false));
        RETURN_IF_FAILED(m_pCardMap->pEthernetControl->ControlMACEgressOnPort(COBRA_BEAT_MAC, TRUE, TRUE));
        RETURN_IF_FAILED(m_pCardMap->pEthernetControl->ControlMACEgressOnPort(COBRA_ANNOUNCE_MAC, TRUE, TRUE));
        RETURN_IF_FAILED(m_pCardMap->pEthernetControl->ControlMACEgressOnPort(BROADCAST_MAC, FALSE, TRUE));

        OSTimeDly(OS_TICKS_PER_SEC * .5);

        // Wait for the time delay to expire OR until we see the
        // link on the port go away (an indication that the module
        // has reset). We don't bother looking if there is a download
        // active because in that case we will have disabled the port
        // on our side. Hopefully this will allow us to get in synch with
        // the reset state of the module rather quickly.
        m_ResetTimer.ResetTime(HEARTBEAT_RESET_TIMEOUT);
        while (m_ResetTimer.IsTimeExpired() == false)
        {
            if (m_pCobraNetDevice == NULL || m_pCobraNetDevice->IsDownloadActive() == FALSE)
            {
                if (SUCCEEDED(m_pCardMap->pEthernetControl->GetLinkInfo(&LinkInfo)))
                {
                    if (LinkInfo.isLinkUp == FALSE)
                    {
                        break;
                    }
                }
            }

            OSTimeDly(OS_TICKS_PER_SEC * .1);
        }

        m_ResetTimer.ResetTime(HEARTBEAT_RESET_TIMEOUT);
        while (m_ResetTimer.IsTimeExpired() == false)
        {
            if (m_pCobraNetDevice == NULL || m_pCobraNetDevice->IsDownloadActive() == FALSE)
            {
                if (SUCCEEDED(m_pCardMap->pEthernetControl->GetLinkInfo(&LinkInfo)))
                {
                    if (LinkInfo.isLinkUp == FALSE)
                    {
                        break;
                    }
                }

                OSTimeDly(OS_TICKS_PER_SEC * .1);
            }
            else
            {
                break;
            }
        }

        RETURN_IF_FAILED(m_BTComm.SetCommEnable(true));
    }

    return STATUS_SUCCESS;
}

/********************************************************************************************************

********************************************************************************************************/
ERROR_CODE_T CBTAudioCard::ResetCobra(bool isReset)
{
    RETURN_IF_FAILED(Reset(isReset));
    return STATUS_SUCCESS;
}

/********************************************************************************************************

********************************************************************************************************/
ERROR_CODE_T CBTAudioCard::ResetMAC(bool isReset)
{
    RETURN_IF_FAILED(Reset(isReset));
    return STATUS_SUCCESS;
}

/********************************************************************************************************

********************************************************************************************************/
ERROR_CODE_T CBTAudioCard::SetDownloadState(bool downloading)
{
    // Nothing to do here at this point.
    // We want to make sure serial messages are going out as normal so we
    // don't watchdog. When we eventually have the ability to program the
    // BC127, we will want this information to help us synchronize with
    // Cobranet programming.
    return STATUS_SUCCESS;
}

/*@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
IDiagnosticControl ROUTINES
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@*/
const IDiagnosticControl::DIAGNOSTIC_DESCRIPTOR CBTAudioCard::m_DiagDescriptors[] =
    {
        {DIAG_GET_DESCRIPTOR, 0, "Get Descriptor Info", DIAG_PROP_GET, DIAG_DESCRIPTOR_DATA(""), DIAG_NULL_DATA(), DIAG_NULL_DATA(), DIAG_NULL_DATA()},
        {DIAG_DEVICE_CNTRL, 0, "Cobranet Control", DIAG_PROP_GET_SET, DIAG_NULL_DATA(), DIAG_NULL_DATA(), DIAG_NULL_DATA(), DIAG_NULL_DATA()},
        {PAUSE_CARD_TASK, 0, "Pause Card Task", DIAG_PROP_GET_SET, DIAG_BIT_DATA("IsPaused"), DIAG_NULL_DATA(), DIAG_NULL_DATA(), DIAG_NULL_DATA()},
        {PACKET_TRACE, 0, "Enable Packet Trace", DIAG_PROP_GET_SET, DIAG_BIT_PARM("Trace Enable"), DIAG_NULL_DATA(), DIAG_NULL_DATA(), DIAG_NULL_DATA()},
        {INHIBIT_SERIAL_MESSAGES, 0, "Inhibit Serial Messages", DIAG_PROP_GET_SET, DIAG_BIT_DATA("Inhibited"), DIAG_NULL_DATA(), DIAG_NULL_DATA(), DIAG_NULL_DATA()},
        {SEND_BT_CMD, 0, "Send Serial Command", DIAG_PROP_GET, DIAG_STRING_PARM("Command"), DIAG_STRING_DATA("Response"), DIAG_NULL_DATA(), DIAG_NULL_DATA()},
        {UNPAIR, 0, "Unpair All Devices", DIAG_PROP_SET, DIAG_NULL_DATA(), DIAG_NULL_DATA(), DIAG_NULL_DATA(), DIAG_NULL_DATA()},
        {SCAN_FOR_DEVICES, 0, "Scan for Devices", DIAG_PROP_SET, DIAG_NULL_DATA(), DIAG_NULL_DATA(), DIAG_NULL_DATA(), DIAG_NULL_DATA()},
        {OPEN_SCANNING, 0, "Scan for all 2404 devices", DIAG_PROP_GET_SET, DIAG_BIT_DATA("Enabled"), DIAG_NULL_DATA(), DIAG_NULL_DATA(), DIAG_NULL_DATA()},
        {PAIR_DEVICE_INDEX, 0, "Pair Device", DIAG_PROP_SET, DIAG_BYTE_DATA("Device Index"), DIAG_NULL_DATA(), DIAG_NULL_DATA(), DIAG_NULL_DATA()},
        {CTRL_DESIRED_BAUDRATE, 0, "Ctrl Baudrate", DIAG_PROP_GET_SET, DIAG_DWORD_DATA("(9600,19200,38400,57600,115200)"), DIAG_NULL_DATA(), DIAG_NULL_DATA(), DIAG_NULL_DATA()},
        {RESTORE_DEFAULT_BAUDRATE, 0, "Restore Default Baudrate", DIAG_PROP_SET, DIAG_NULL_DATA(), DIAG_NULL_DATA(), DIAG_NULL_DATA(), DIAG_NULL_DATA()},
        {SIMULATE_CONNECTIVITY_LOSS, 0, "Simulate Connectivity Loss", DIAG_PROP_SET, DIAG_BIT_DATA("enabled"), DIAG_NULL_DATA(), DIAG_NULL_DATA(), DIAG_NULL_DATA()},
        {START_AUTO_PAIR, 0, "Start Auto Pair (Tx Only)", DIAG_PROP_SET, DIAG_BYTE_DATA("timeout sec"), DIAG_NULL_DATA(), DIAG_NULL_DATA(), DIAG_NULL_DATA()},
};

/********************************************************************************************************
                ERROR_CODE_T DiagPropertyControl( DIAGNOSTIC_COMMAND *pCommand )
********************************************************************************************************/
ERROR_CODE_T CBTAudioCard::DiagPropertyControl(DIAGNOSTIC_COMMAND *pCommand)
{
    ScopeExit(
        dataLenScope, {
            pCommand->DataLength = 0;
        },
        DIAGNOSTIC_COMMAND *, pCommand);

    // Use the interface function to validate the command.
    RETURN_IF_FAILED(ValidateCommand(pCommand, m_DiagDescriptors, sizeof(m_DiagDescriptors) / sizeof(DIAGNOSTIC_DESCRIPTOR)));

    // Execute the command.
    switch (pCommand->Property)
    {
        // Handle Diagnostic Descriptor Requests.
        case DIAG_GET_DESCRIPTOR:
            // The function is provided by the interface to load the descriptors.
            RETURN_IF_FAILED(LoadDescriptors(pCommand, m_DiagDescriptors, sizeof(m_DiagDescriptors) / sizeof(DIAGNOSTIC_DESCRIPTOR)));
            break;

        case DIAG_DEVICE_CNTRL:
            RETURN_IF_FAILED(HandleDiagDeviceControl(pCommand));
            break;

        case PAUSE_CARD_TASK:
            RETURN_IF_FAILED(HandlePauseCardTask(pCommand));
            break;

        case PACKET_TRACE:
            RETURN_IF_FAILED(HandleControlPacketTrace(pCommand));
            break;

        case INHIBIT_SERIAL_MESSAGES:
            RETURN_IF_FAILED(HandleInhibitSerialMessages(pCommand));
            break;

        case SEND_BT_CMD:
            RETURN_IF_FAILED(HandleSendBTCommand(pCommand));
            break;

        case UNPAIR:
            RETURN_IF_FAILED(HandleUnpair(pCommand));
            break;

        case SCAN_FOR_DEVICES:
            RETURN_IF_FAILED(HandleScanForDevices(pCommand));
            break;

        case OPEN_SCANNING:
            RETURN_IF_FAILED(HandleOpenScanning(pCommand));
            break;

        case PAIR_DEVICE_INDEX:
            RETURN_IF_FAILED(HandleSetPairedDeviceIndex(pCommand));
            break;

        case CTRL_DESIRED_BAUDRATE:
            RETURN_IF_FAILED(HandleCtrlDesiredBaudrate(pCommand));
            break;

        case RESTORE_DEFAULT_BAUDRATE:
            RETURN_IF_FAILED(HandleRestoreDefaultBaudrate(pCommand));
            break;

        case SIMULATE_CONNECTIVITY_LOSS:
            RETURN_IF_FAILED(HandleSimulateConnectivityLoss(pCommand));
            break;

        case START_AUTO_PAIR:
            RETURN_IF_FAILED(HandleStartAutoPair(pCommand));
            break;

        default:
            return ERROR_INVALID_PARAMETER;
    }

    dataLenScope.Dismiss();
    return STATUS_SUCCESS;
}

void CBTAudioCard::UpdateHealthData(shared_ptr<IHealthStatusBuilder> &pHealthStatusBuilder)
{
    if (pHealthStatusBuilder == NULL)
    {
        DebugPrintf(DEBUG_TRACE, DEBUG_NO_LOGGING, m_TaskID, "Attempt to update health status when broadcast is inactive.");
        return;
    }

    pHealthStatusBuilder->SetExclusiveLock();

    // Reset variadics count and length
    pHealthStatusBuilder->VariadicsClear();

    // Fetch Device status
    pHealthStatusBuilder->UpdateStatus(m_ReadyFlag);

    // Fetch network info
    INT32U tmpNetmask = 0;
    if (m_pCardMap->pEthernetControl != NULL)
    {
        INT32U tmpIP = 0; // Not actually used.
        m_pCardMap->pEthernetControl->GetSwitchManager()->GetIpAddr(&tmpIP, &tmpNetmask);
    }
    pHealthStatusBuilder->AddressInfoSet(m_pCardMap->slotNumber, CIO::m_NodeAddress);
    pHealthStatusBuilder->NetworkInfoSet(CIO::m_NodeAddress, tmpNetmask);

    // Add device-specific info
    pHealthStatusBuilder->AddVariadic(KP_BOOL, HEALTH_MSG_SYSTEM, "Online", (void *)&m_Online);
    pHealthStatusBuilder->AddVariadic(KP_U8, HEALTH_MSG_SYSTEM, "Card Number", (void *)&m_CardNumber);

    pHealthStatusBuilder->AddVariadic(KP_BOOL, HEALTH_MSG_SYSTEM, "Is Initialized", (void *)&m_IsInitialized);
    pHealthStatusBuilder->AddVariadic(KP_STRING, HEALTH_MSG_SYSTEM, "Device Name", (void *)&m_DeviceName);
    pHealthStatusBuilder->AddVariadic(KP_HEX_U8, HEALTH_MSG_SYSTEM, "Firmware Revision", (void *)&m_FWRevision);
    pHealthStatusBuilder->AddVariadic(KP_STRING, HEALTH_MSG_SYSTEM, "Device Location", (void *)&m_DeviceLocationInfo);
    BOOLEAN connected = m_pCobraNetDevice->IsInitialized();
    pHealthStatusBuilder->AddVariadic(KP_BOOL, HEALTH_MSG_SYSTEM, "Cobranet Device Initialized", (void *)&(connected));
    pHealthStatusBuilder->AddVariadic(KP_STRING, HEALTH_MSG_SYSTEM, "TX/RX Direction", (void *)&(m_isInputDevice ? "RX" : "TX"));

    // Add default device info
    pHealthStatusBuilder->AddVariadic(KP_STRING, HEALTH_MSG_SYSTEM, "Default Device", (void *)&m_PairedBTDevice);
    BOOLEAN paired = IsDefaultDeviceValid();
    pHealthStatusBuilder->AddVariadic(KP_BOOL, HEALTH_MSG_SYSTEM, "Default Device Paired", (void *)&paired);

    // Add paired device info if any.
    //  This code currently causes an issue with the client, since we are using the same key
    //  multiple times. If the client is updated to handle this, we can uncomment this code.
    //  if(m_pairedDeviceInfo.size()>0)
    //  {
    //      list<shared_ptr<CBTEAPairedDevice> >::iterator it;
    //      INT8U deviceNum = 0;
    //      const CHAR8 *pDeviceNumberBase = "Paired Device ";
    //      CHAR8 pDeviceNumber[32];
    //      BOOLEAN addedSuccessfully = TRUE;
    //      for (it = m_pairedDeviceInfo.begin(); it != m_pairedDeviceInfo.end(); ++it)
    //      {
    //          shared_ptr<CBTEAPairedDevice> pPairedDevice = *it;
    //          sprintf(pDeviceNumber, "%s%d", pDeviceNumberBase, deviceNum++);
    //          addedSuccessfully &= (STATUS_SUCCESS ==
    //              pHealthStatusBuilder->AddVariadic(KP_STRING, HEALTH_MSG_SYSTEM, pDeviceNumber, pPairedDevice->m_btDeviceName.c_str()));
    //          addedSuccessfully &= (STATUS_SUCCESS ==
    //              pHealthStatusBuilder->AddVariadic(KP_STRING, HEALTH_MSG_SYSTEM, "Device Address", pPairedDevice->m_btAddress.c_str()));
    //          if(!addedSuccessfully)
    //          {
    //              DebugPrintf(DEBUG_TRACE_MESSAGE, DEBUG_NO_LOGGING, m_TaskID, "Failed to add paired device info to health status message.");
    //              break;
    //          }
    //      }
    //  }

    pHealthStatusBuilder->ReleaseExclusiveLock();
}

ERROR_CODE_T CBTAudioCard::HandleDiagDeviceControl(DIAGNOSTIC_COMMAND *pCommand)
{
    // This just passes the message through to the appropriate device.
    INT32U tempSubPropertyIndex = pCommand->SubPropertyIndex;
    RETURN_EC_IF_TRUE(ERROR_INVALID_PARAMETER, tempSubPropertyIndex > 0);

    // There could be some data here for us, but there shouldn't be since we didn't ask for any.
    pCommand->pData += pCommand->DataLength;
    RETURN_IF_FAILED(BufToDiagnosticCommand(pCommand, &pCommand->pData));
    RETURN_EC_IF_NULL(ERROR_NOT_INITIALIZED, m_pCobraNetDevice);
    RETURN_IF_FAILED(m_pCobraNetDevice->DiagPropertyControl(pCommand));

    return STATUS_SUCCESS;
}

ERROR_CODE_T CBTAudioCard::HandlePauseCardTask(DIAGNOSTIC_COMMAND *pCommand)
{
    RETURN_EC_IF_NULL(ERROR_NOT_INITIALIZED, m_pCardMsgQueue);
    if (pCommand->AccessType == DIAG_PROP_GET)
    {
        RETURN_EC_IF_FALSE(ERROR_BUFFER_TOO_SMALL, pCommand->MaxResponseLength >= 1);
        pCommand->pData[0] = m_pCardMsgQueue->IsCardTaskPaused();
        pCommand->DataLength = 1;
    }
    else
    {
        RETURN_EC_IF_FALSE(ERROR_BUFFER_TOO_SMALL, pCommand->DataLength >= 1);
        m_pCardMsgQueue->PauseCardTask(pCommand->pData[0]);
    }

    return STATUS_SUCCESS;
}

ERROR_CODE_T CBTAudioCard::HandleControlPacketTrace(DIAGNOSTIC_COMMAND *pCommand)
{
    if (pCommand->AccessType == DIAG_PROP_GET)
    {
        RETURN_EC_IF_FALSE(ERROR_BUFFER_TOO_SMALL, pCommand->MaxResponseLength >= 1);
        pCommand->pData[0] = (m_BTComm.GetPacketVerbosity() == DEBUG_TRACE_MESSAGE ? 1 : 0);
        pCommand->DataLength = 1;
    }
    else
    {
        RETURN_EC_IF_FALSE(ERROR_BUFFER_TOO_SMALL, pCommand->DataLength >= 1);
        m_BTComm.SetPacketVerbosity(pCommand->pData[0] == 1 ? DEBUG_TRACE_MESSAGE : DEBUG_NO_LOGGING);
    }

    return STATUS_SUCCESS;
}

ERROR_CODE_T CBTAudioCard::HandleInhibitSerialMessages(DIAGNOSTIC_COMMAND *pCommand)
{
    if (pCommand->AccessType == DIAG_PROP_GET)
    {
        RETURN_EC_IF_FALSE(ERROR_BUFFER_TOO_SMALL, pCommand->MaxResponseLength >= 1);
        pCommand->pData[0] = m_BTComm.IsCommEnabled();
        pCommand->DataLength = 1;
    }
    else
    {
        RETURN_EC_IF_FALSE(ERROR_BUFFER_TOO_SMALL, pCommand->DataLength >= 1);
        m_BTComm.SetCommEnable(!!pCommand->pData[0]);
    }

    return STATUS_SUCCESS;
}

ERROR_CODE_T CBTAudioCard::HandleSendBTCommand(DIAGNOSTIC_COMMAND *pCommand)
{
    RETURN_EC_IF_FALSE(ERROR_NOT_INITIALIZED, m_IsInitialized);
    RETURN_EC_IF_FALSE(ERROR_INVALID_PARAMETER, pCommand->AccessType == DIAG_PROP_GET);

    vector<string> strings;
    pCommand->pData[pCommand->DataLength - 1] = '\0'; // Insurance in case the string is not null terminated.
    RETURN_IF_FAILED(m_BTComm.ReadData(strings, (CHAR8 *)pCommand->pData));
    pCommand->DataLength += strings[0].copy((CHAR8 *)&pCommand->pData[pCommand->DataLength], pCommand->MaxResponseLength - pCommand->DataLength);
    pCommand->pData[pCommand->DataLength] = '\0';
    pCommand->DataLength++;

    return STATUS_SUCCESS;
}

ERROR_CODE_T CBTAudioCard::HandleUnpair(DIAGNOSTIC_COMMAND *pCommand)
{
    RETURN_EC_IF_FALSE(ERROR_NOT_INITIALIZED, m_IsInitialized);
    RETURN_IF_FAILED(m_BTComm.CancelCurrentCommand());
    m_unpairAllDevicesFlag = true;

    return STATUS_SUCCESS;
}

ERROR_CODE_T CBTAudioCard::HandleScanForDevices(DIAGNOSTIC_COMMAND *pCommand)
{
    RETURN_EC_IF_TRUE(ERROR_NOT_SUPPORTED, m_isInputDevice || !m_PairedBTDevice.empty());

    DebugPrintf(DEBUG_TRACE_MESSAGE, DEBUG_NO_LOGGING, m_TaskID, "Scanned %d Device(s):", m_detectedDeviceList.size());

    if (m_detectedDeviceList.size() != 0)
    {
        INT32U index = 0;
        list<shared_ptr<CBTEADetectedDevice> >::iterator it;
        for (it = m_detectedDeviceList.begin(); it != m_detectedDeviceList.end(); it++)
        {
            DebugPrintf(DEBUG_TRACE_MESSAGE, DEBUG_NO_LOGGING, m_TaskID, "%d: %s", index, (*it)->m_btDeviceName.c_str());
            index++;
        }
    }

    return STATUS_SUCCESS;
}

ERROR_CODE_T CBTAudioCard::HandleOpenScanning(DIAGNOSTIC_COMMAND *pCommand)
{
    RETURN_EC_IF_TRUE(ERROR_NOT_SUPPORTED, m_isInputDevice);

    if (pCommand->AccessType == DIAG_PROP_GET)
    {
        RETURN_EC_IF_TRUE(ERROR_BUFFER_TOO_SMALL, pCommand->MaxResponseLength == 0);
        pCommand->pData[0] = m_ScanAllDevices;
        pCommand->DataLength = 1;
    }
    else
    {
        RETURN_EC_IF_TRUE(ERROR_BUFFER_TOO_SMALL, pCommand->DataLength == 0);
        m_ScanAllDevices = !!(pCommand->pData[0]);
    }

    return STATUS_SUCCESS;
}

ERROR_CODE_T CBTAudioCard::HandleSetPairedDeviceIndex(DIAGNOSTIC_COMMAND *pCommand)
{
    RETURN_EC_IF_TRUE(ERROR_NOT_SUPPORTED, m_isInputDevice || !m_PairedBTDevice.empty());
    RETURN_EC_IF_FALSE(ERROR_INVALID_PARAMETER, pCommand->AccessType == DIAG_PROP_SET);
    RETURN_EC_IF_FALSE(ERROR_BUFFER_TOO_SMALL, pCommand->MaxResponseLength >= 1);

    INT8U connectDeviceIndex = pCommand->pData[0];
    RETURN_EC_IF_FALSE(ERROR_INVALID_PARAMETER, connectDeviceIndex < m_detectedDeviceList.size());
    list<shared_ptr<CBTEADetectedDevice> >::iterator iter;
    for (iter = m_detectedDeviceList.begin(); iter != m_detectedDeviceList.end(); iter++)
    {
        if (connectDeviceIndex == 0)
        {
            m_connectDeviceAddr = (*iter)->m_btAddress;
            break;
        }

        connectDeviceIndex--;
    }

    return STATUS_SUCCESS;
}

ERROR_CODE_T CBTAudioCard::HandleCtrlDesiredBaudrate(DIAGNOSTIC_COMMAND *pCommand)
{
    RETURN_EC_IF_FALSE(ERROR_NOT_INITIALIZED, m_IsInitialized);
    INT8U *pData = pCommand->pData;

    if (pCommand->AccessType == DIAG_PROP_GET)
    {
        RETURN_EC_IF_FALSE(ERROR_BUFFER_TOO_SMALL, pCommand->MaxResponseLength >= 4);
        Int32ToBuf(GetBaudrateValue(m_desiredBaudrate), &pData);
        pCommand->DataLength = 4;
    }
    else
    {
        RETURN_EC_IF_TRUE(ERROR_INVALID_PARAMETER, pCommand->DataLength < 4);
        INT32U baudrateVal = BufToInt32(&pData);
        BAUDRATE newBadurate = GetBaudrateFromValue(baudrateVal);
        RETURN_EC_IF_TRUE(ERROR_INVALID_PARAMETER, newBadurate == BAUDRATE_UNKNOWN);

        m_desiredBaudrate = newBadurate;
        RETURN_IF_FAILED(SetBaudrate(m_desiredBaudrate));
    }

    return STATUS_SUCCESS;
}

ERROR_CODE_T CBTAudioCard::HandleRestoreDefaultBaudrate(DIAGNOSTIC_COMMAND *pCommand)
{
    RETURN_EC_IF_FALSE(ERROR_NOT_INITIALIZED, m_IsInitialized);
    RETURN_EC_IF_FALSE(ERROR_INVALID_PARAMETER, pCommand->AccessType == DIAG_PROP_SET);

    m_desiredBaudrate = DEFAULT_BAUDRATE;
    RETURN_IF_FAILED(WriteConfigToFlash());
    return STATUS_SUCCESS;
}

ERROR_CODE_T CBTAudioCard::HandleSimulateConnectivityLoss(DIAGNOSTIC_COMMAND *pCommand)
{
    RETURN_EC_IF_FALSE(ERROR_NOT_INITIALIZED, m_IsInitialized);
    RETURN_EC_IF_FALSE(ERROR_INVALID_PARAMETER, pCommand->AccessType == DIAG_PROP_SET);
    RETURN_EC_IF_FALSE(ERROR_BUFFER_TOO_SMALL, pCommand->DataLength >= 1);

    m_BTComm.SimulateConnectivityLoss(!!(pCommand->pData[0]));
    return STATUS_SUCCESS;
}

ERROR_CODE_T CBTAudioCard::HandleStartAutoPair(DIAGNOSTIC_COMMAND *pCommand)
{
    RETURN_EC_IF_FALSE(ERROR_NOT_INITIALIZED, m_IsInitialized);
    RETURN_EC_IF_NULL(ERROR_INVALID_PARAMETER, pCommand);
    RETURN_EC_IF_FALSE(ERROR_INVALID_PARAMETER, pCommand->AccessType == DIAG_PROP_SET);
    RETURN_EC_IF_FALSE(ERROR_BUFFER_TOO_SMALL, pCommand->DataLength >= 1);
    RETURN_EC_IF_TRUE(ERROR_NOT_SUPPORTED, m_isInputDevice);

    m_unpairAllDevicesFlag = true;
    m_autoConnectTimer.ResetTime(pCommand->pData[0]);
    m_isAutoConnecting = true;
    return STATUS_SUCCESS;
}

/*@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
Helper routines
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@*/

/********************************************************************************************************
********************************************************************************************************/
ERROR_CODE_T CBTAudioCard::OnConfigTableChange(void *pInst, shared_ptr<IBTAdapterConfig> pConfig)
{
    CBTAudioCard *pThis = ((CBTAudioCard *)pInst);

    if (NULL != pThis)
    {
        // Fire the event so the main thread can handle the change.
        pThis->m_pMsgQueue->PostUserEvent(BT_CONFIG_CHANGED, NULL, 0, true);
    }

    return STATUS_SUCCESS;
}

/********************************************************************************************************
                                void TaskInit( void *pArg )
This is the entry point for the card task. It is a static function that receives a pointer to
the class instance as an argument.
********************************************************************************************************/
void CBTAudioCard::TaskInit(void *pArg)
{
    DebugPrintf(DEBUG_TRACE_MESSAGE, DEBUG_NO_LOGGING, "CBTAudioCard", "Task Starting");
    ((CBTAudioCard *)pArg)->TaskMain();
}

/********************************************************************************************************
                                void TaskMain( void )
This is the main task function for the card.
********************************************************************************************************/
void CBTAudioCard::TaskMain(void)
{
    struct CardMsg Msg;
    bool isSignalPresent = TRUE;
    INT32U Timeout = 0;

    Initialize();
    GotoOfflineState();

    while (1)
    {
        PetWatchdog();

        if (RequestTaskTermination(OS_PRIO_SELF) == ERROR_TASK_DEL_REQ)
        {
            DebugPrintf(DEBUG_TRACE_MESSAGE, DEBUG_NO_LOGGING, m_TaskID, "Task Ending.");
            return;
        }

        // This handles communication with the Bluetooth Adapter.
        {
            CSimpleLock tempLock(&m_criticalSection);
            if (FAILED(InterfaceMain()))
            {
                if (++m_InterfaceMainRetryCount > 4)
                    GotoOfflineState();
            }
        }

        if (m_Online == false || m_ConfigState != BT_CONFIG_DONE)
        {
            Timeout = 1;
        }
        else
        {
            Timeout = OS_TICKS_PER_SEC;
        }

        if (m_pCardMsgQueue->Pend(&Msg, Timeout) == STATUS_SUCCESS)
        {
            switch (Msg.Command)
            {
                case CARD_MSG_SET_OVERRIDE:
                {
                    IBlock *pBlock = (IBlock *)Msg.Data[0];
                    INT32U numRoutes = Msg.Data[3];
                    INTU PortNumber = pBlock->GetPortNumber();

                    if (numRoutes != 0)
                    {
                        ICobraNetRouter *pRouter = (ICobraNetRouter *)pBlock->GetRouter();

                        // Always clear the mute for this command.
                        pRouter->SetProperty(PROP_MUTE_ON, 0);
                    }

                    pBlock->SetRoutes(&Msg.Data[1], true);
                }
                break;

                case CARD_MSG_SET_ROUTE_LIST:
                {
                    IBlock *pBlock = (IBlock *)Msg.Data[0];
                    pBlock->SetRoutes(Msg.Data);
                }
                break;

                case CARD_MSG_SET_ROUTE:
                {
                    IBlock *pBlock = (IBlock *)Msg.Data[0];
                    ROUTE_T route = (ROUTE_T)Msg.Data[1];

                    pBlock->SetRoute(route);
                }
                break;

                case CARD_MSG_CLEAR_ROUTE:
                {
                    IBlock *pBlock = (IBlock *)Msg.Data[0];
                    ROUTE_T route = (ROUTE_T)Msg.Data[1];

                    pBlock->ClearRoute(route);
                }
                break;

                case CARD_MSG_SET_PROPERTY:
                {
                    IBlock *pBlock = (IBlock *)Msg.Data[0];
                    PROPERTIES_T property = (PROPERTIES_T)Msg.Data[1];
                    INT32U value = Msg.Data[2];

                    switch (property)
                    {
                        case PROP_VOLUME:
                        case PROP_MUTE_ON:
                        case PROP_TREBLE:
                        case PROP_BASS:
                        case PROP_AUDIO_COMPRESSION:
                        case PROP_EQ_LEVEL_STEREO_LEFT_FRONT:
                        case PROP_EQ_LEVEL_STEREO_RIGHT_FRONT:
                        case PROP_EQ_LEVEL_LEFT_SURROUND:
                        case PROP_EQ_LEVEL_RIGHT_SURROUND:
                        case PROP_EQ_LEVEL_CENTER:
                        case PROP_EQ_LEVEL_SUBWOOFER:
                        case PROP_EQ_LEVEL_LEFT_BACK:
                        case PROP_EQ_LEVEL_RIGHT_BACK:
                        {
                            ICobraNetRouter *pRouter = (ICobraNetRouter *)pBlock->GetRouter();
                            pRouter->SetProperty(property, value);
                        }
                        break;
                    }
                }
                break;

                case CARD_MSG_FORMAT_CHANGE:
                    // No action on format change.
                    break;

                case CARD_MSG_BT_ADAPTER_UNPAIR:
                    m_unpairAllDevicesFlag = true;
                    break;

                case CARD_MSG_BT_ADAPTER_PAIR:
                    m_connectDeviceAddr = (CHAR8 *)Msg.Data;
                    break;

                case CARD_MSG_BT_ADAPTER_AUTO_PAIR:
                    if (!m_isInputDevice)
                    {
                        m_isAutoConnecting = true;
                        UnpairAllDevices();
                        m_autoConnectTimer.ResetTime(Msg.Data[0]);
                    }
                    break;

                case BT_CONFIG_CHANGED:
                {
                    shared_ptr<IBTAdapterConfigTable> pConfigTable;
                    CIO::g_pNode->GetBTAdapterConfigTable(pConfigTable);
                    if (pConfigTable != NULL)
                    {
                        shared_ptr<IBTAdapterConfig> pConfig;
                        pConfigTable->GetAdapterConfig(CIO::m_NodeAddress, m_pCardMap->slotNumber, pConfig);
                        if (pConfig != NULL && pConfig->GetNodeAddress() == CIO::m_NodeAddress &&
                            pConfig->GetCardNumber() == m_CardNumber)
                        {
                            bool restartDevice = false;
                            if (pConfig->GetDeviceID().compare(m_DeviceName))
                            {
                                memset(m_DeviceName, 0, DEVICE_NAME_LENGTH);
                                strncpy(m_DeviceName, pConfig->GetDeviceID().c_str(), DEVICE_NAME_MAX);
                                restartDevice = true;
                            }

                            bool newIsInputDevice = pConfig->GetIsAdapterInput();
                            if (m_isInputDevice != newIsInputDevice)
                            {
                                bool isValidDirection = false;
                                DirectionIsValid(newIsInputDevice, isValidDirection);

                                if (isValidDirection)
                                {
                                    m_isInputDevice = newIsInputDevice;
                                    UnpairAllDevices();
                                    UpdateAllStatus();
                                    restartDevice = true;
                                }
                            }

                            if (restartDevice)
                            {
                                GotoOfflineState();
                            }
                        }
                    }
                }
                break;
            }
        }
        else
        {
            if (m_pInBlock != NULL)
            {
                if (m_pInRouter != NULL)
                {
                    if (m_Online == true)
                        m_pInRouter->GetSignalDetect(&isSignalPresent);
                    else
                        isSignalPresent = false;
                }

                CSimpleLock tempLock(&m_criticalSection);
                if (isSignalPresent)
                {
                    m_pInBlock->SetStatusBits(BLOCK_STATUS_SIGNAL_OKAY);
                    m_pInBlock->SetFormat(AUDIO_FORMAT_STEREO);
                }
                else
                {
                    m_pInBlock->ClearStatusBits(BLOCK_STATUS_SIGNAL_OKAY);
                    m_pInBlock->SetFormat(AUDIO_FORMAT_NONE);
                }

                m_pInBlock->UpdateStatus();
            }

            if (m_pOutBlock != NULL)
            {
                if (m_pOutRouter != NULL)
                {
                    if (m_Online == true)
                        m_pOutRouter->GetSignalDetect(&isSignalPresent);
                    else
                        isSignalPresent = false;
                }

                CSimpleLock tempLock(&m_criticalSection);
                if (isSignalPresent)
                {
                    m_pOutBlock->SetStatusBits(BLOCK_STATUS_SIGNAL_OKAY);
                    m_pOutBlock->SetFormat(AUDIO_FORMAT_STEREO);
                }
                else
                {
                    m_pOutBlock->ClearStatusBits(BLOCK_STATUS_SIGNAL_OKAY);
                    m_pOutBlock->SetFormat(AUDIO_FORMAT_NONE);
                }
                m_pOutBlock->ClearStatusBits(BLOCK_STATUS_SIGNAL_PENDING);
                m_pOutBlock->UpdateStatus();
            }
            CSimpleLock tempLock(&m_criticalSection);
            UpdateHealthData(m_pHealthStatusBuilder);
        }
    }
}

/********************************************************************************************************
                                void Initialize( void )
This is called to load all the drivers and setup all the hardware.
********************************************************************************************************/
typedef enum
{
    BTEA_INIT_START,
    BTEA_COBRANET_INIT,
    BTEA_NETWORK_INIT,
    BTEA_LOAD_CONFIG_PARAMS,
    BTEA_INIT_COMPLETE,
} BTEAInitState_T;

void CBTAudioCard::Initialize(void)
{
    INT32U initCount = 0;
    INT8U initState = BTEA_INIT_START;
    m_InitStalled = false;

    DebugPrintf(DEBUG_TRACE_MESSAGE, DEBUG_NO_LOGGING, m_TaskID, "Beginning Initialization.");

    // Spin in this while loop attempting to initialize.
    while (initState < BTEA_INIT_COMPLETE)
    {
        PetWatchdog();

        if (m_pCardMsgQueue->IsCardTaskPaused() == TRUE)
        {
            OSTimeDly(OS_TICKS_PER_SEC);
            continue;
        }

        InitLoopDly();
        initCount++;

        switch (initState)
        {
            case BTEA_INIT_START:
                initState = BTEA_COBRANET_INIT;
                break;

            case BTEA_COBRANET_INIT:
                InitDebugPrintIfStalled(initCount >= INIT_TIMEOUT_COUNT,
                                        m_TaskID,
                                        "Initialization stalled because the Cobranet could not be created.");

                if (m_pCobraNetDevice == NULL)
                {
                    InitDebugPrintf(DEBUG_TRACE_MESSAGE, DEBUG_NO_LOGGING, m_TaskID, "Initializing Cobranet.");

                    // Establish IP Address of device, this is the first and only IP device on this card (CARD_BASE_OCTET(m_Slot) + 0)
                    // The second octet changes per device on a given node...10.x.nn.nn
                    // 0..9, baseboard devices
                    // 10..19, slot 0 devices
                    // 20..29, slot 1 devices, etc...
                    INT32U IpAddress = NODE_TO_IP_ADDRESS(CIO::m_NodeAddress) + (VIRTUAL_CARD_BASE_OCTET(m_pCardMap->slotNumber) << 16);

                    m_pCobraNetIOMap = make_shared<CDevCS181012::IO_MAP_T>(shared_from_this()); // ICobranetResetControl *pResetControl

                    m_pCobraNetDevice = make_shared<CDevCS181012>(IpAddress, (CDevCS181012::IO_MAP_T *)m_pCobraNetIOMap.get(), APP01);
                    m_BTComm.SetUArt(static_pointer_cast<CDevCS181012>(m_pCobraNetDevice));
                    m_pUnsolicitedMessageEvent = m_BTComm.OnUnsolicitedMessageReceived.registerObserver(shared_from_this(),
                                                                                                        &CBTAudioCard::OnUnsolicitedMessageReceived);
                    initState = BTEA_NETWORK_INIT;
                }
                break;

            case BTEA_NETWORK_INIT:
                InitDebugPrintIfStalled(initCount >= INIT_TIMEOUT_COUNT,
                                        m_TaskID,
                                        "Initialization stalled because the Ethernet port control was not set.");

                if (m_pCardMap->pEthernetControl != NULL)
                {
                    // The Cobanet doesn't need to be fully initialized for us to get
                    // the serial bridge working. This is handy because it allows us
                    // to send heartbeat messages while downloading.
                    m_pCardMap->pEthernetControl->ChangeExternalPortMask(false);
                    m_pCobraNetDevice->SetEthernetPort(m_pCardMap->pEthernetControl);
                    initState = BTEA_LOAD_CONFIG_PARAMS;
                }
                break;

            case BTEA_LOAD_CONFIG_PARAMS:
                InitDebugPrintIfStalled(initCount >= INIT_TIMEOUT_COUNT,
                                        m_TaskID,
                                        "Initialization stalled waiting for configuration information.");

                if (CIO::g_pParser && CIO::g_pNode)
                {
                    // We want to wait until we have received a valid system state table to continue.
                    // This will allow us to pick up whether we should be running as input or output.
                    shared_ptr<ISystemState> pSystemState;

                    if (SUCCEEDED(CIO::g_pNode->GetSystemStateTable(pSystemState)) && pSystemState != NULL &&
                        pSystemState->GetSystemStateRevision() != INVALID_TABLE_REVISION)
                    {
                        shared_ptr<IBTAdapterConfigTable> pConfigTable;

                        if (SUCCEEDED(CIO::g_pNode->GetBTAdapterConfigTable(pConfigTable)) && pConfigTable != NULL &&
                            CIO::g_pParser->ConfigurationValid() == TRUE)
                        {
                            vector<shared_ptr<IConfigDriver> > pDrivers;
                            shared_ptr<IConfigDriver> pDriver;
                            CIO::g_pParser->GetDrivers(CIO::m_NodeAddress, pDrivers);

                            for (INT32U driverIdx = 0; driverIdx < pDrivers.size(); driverIdx++)
                            {
                                if (pDrivers[driverIdx]->GetCardNumber() == m_pCardMap->slotNumber)
                                {
                                    pDriver = pDrivers[driverIdx];
                                    break;
                                }
                            }

                            if (pDriver != NULL)
                            {
                                memset(m_DeviceName, 0, DEVICE_NAME_LENGTH);

                                string tableCfgDeviceName;
                                pConfigTable->GetAdapterDeviceName(CIO::m_NodeAddress, m_CardNumber, tableCfgDeviceName);
                                strncpy(m_DeviceName, tableCfgDeviceName.c_str(), DEVICE_NAME_MAX);
                                void *pNotificationUrl = NULL;
                                if (SUCCEEDED(pDriver->GetDriverParam(NOTIFICATION_URL, pNotificationUrl)) &&
                                    pNotificationUrl != NULL)
                                {
                                    m_notificationUrl = string((CHAR8 *)pNotificationUrl);
                                }

                                void *pQualTestMode = NULL;
                                if (SUCCEEDED(pDriver->GetDriverParam(QUAL_TEST_MODE, pQualTestMode)) &&
                                    pQualTestMode != NULL)
                                {
                                    m_QualTestMode = *static_cast<bool *>(pQualTestMode);
                                }

                                void *pSSPMode = NULL;
                                if (SUCCEEDED(pDriver->GetDriverParam(SSP_MODE, pSSPMode)) &&
                                    pSSPMode != NULL)
                                {
                                    m_configuredSSPMode = *static_cast<SSP_MODE_T *>(pSSPMode);
                                }

                                pConfigTable->RegisterForChangeNotification(CIO::m_NodeAddress, m_CardNumber, this, OnConfigTableChange);
                                pConfigTable->GetAdapterDirection(CIO::m_NodeAddress, m_CardNumber, m_isInputDevice);
                                InitializeCommandAndStatusInfo();
                            }
                            else
                            {
                                DebugPrintf(DEBUG_TRACE_MESSAGE, DEBUG_NO_LOGGING, m_TaskID, "Unable to locate driver in configuration file, using default parameters.");
                            }

                            initState = BTEA_INIT_COMPLETE;
                        }

                        CIO::g_pNode->GetBTAdapterStatusTable(m_pStateTable);
                    }
                }

                break;

            default:
                InitDebugPrintf(DEBUG_TRACE_MESSAGE, DEBUG_NO_LOGGING, m_TaskID, "Init Error, Unknown State %d.", initState);
                initState = BTEA_INIT_START;
                break;
        }
    }

    m_IsInitialized = true;
    DebugPrintf(DEBUG_TRACE_MESSAGE, DEBUG_NO_LOGGING, m_TaskID, "Initialization Complete.");
}

/********************************************************************************************************
********************************************************************************************************/
ERROR_CODE_T CBTAudioCard::DirectionIsValid(bool isInput, bool &isValidDirection)
{
    isValidDirection = false;

    vector<shared_ptr<IConfigDriver> > pDrivers;
    RETURN_IF_FAILED(CIO::g_pParser->GetDrivers(CIO::m_NodeAddress, pDrivers));

    vector<shared_ptr<IConfigDriver> >::iterator iter;
    for (iter = pDrivers.begin(); iter != pDrivers.end(); iter++)
    {
        shared_ptr<IConfigDriver> pDriver = *iter;
        if (pDriver->GetCardNumber() == m_pCardMap->slotNumber)
        {
            vector<shared_ptr<IConfigBlock> > AVBlocksOut;
            RETURN_IF_FAILED(CIO::g_pParser->GetAVBlocks(pDriver->GetNodeAddress(), pDriver->GetCardNumber(), 0, AVBlocksOut));

            vector<shared_ptr<IConfigBlock> >::iterator blockIter;
            for (blockIter = AVBlocksOut.begin(); blockIter != AVBlocksOut.end(); blockIter++)
            {
                shared_ptr<IConfigBlock> pBlock = *blockIter;
                if (pBlock->GetBlockType() == (isInput ? INPUT_BLOCK : OUTPUT_BLOCK))
                {
                    isValidDirection = true;
                }
            }
        }
    }

    return STATUS_SUCCESS;
}

/********************************************************************************************************
                                void InterfaceMain( void )
This is the main processing function that handles the interface to the BT Module.
********************************************************************************************************/
ERROR_CODE_T CBTAudioCard::InterfaceMain(void)
{
    if (!m_BTComm.IsCommEnabled() || m_pCobraNetDevice == NULL || !m_pCobraNetDevice->IsInitialized())
    {
        return STATUS_SUCCESS;
    }

    CSimpleLock myLock(&m_criticalSection);
    if (m_ConfigState != BT_CONFIG_DONE)
    {
        switch (m_ConfigState)
        {
            case BT_CONFIG_STATE_START:
            {
                INT8U startFwRevision = m_FWRevision;
                RETURN_IF_FAILED(EnterCommandMode());
                RETURN_IF_FAILED(SendReset());

                if (FAILED(GetVersion()))
                {
                    BAUDRATE baudrate;
                    m_BTComm.GetBaudrate(baudrate);

                    for (INT8U i = 0; i < sizeof(s_potentialBaudrates) / sizeof(BAUDRATE); i++)
                    {
                        if (baudrate == s_potentialBaudrates[i])
                        {
                            baudrate = s_potentialBaudrates[(i + 1) % (sizeof(s_potentialBaudrates) / sizeof(BAUDRATE))];
                            break;
                        }
                    }

                    m_BTComm.SetBaudrate(baudrate);
                    DebugPrintf(DEBUG_TRACE_MESSAGE, DEBUG_NO_LOGGING, m_TaskID,
                                "Failed to get version, retrying with baudrate %s.", GetBaudrateString(baudrate).c_str());
                }
                else
                {
                    if (startFwRevision != m_FWRevision && m_FWRevision == FW_REV_6_1_5)
                    {
                        m_desiredBaudrate = DESIRED_BAUDRATE_6_1_5;
                    }
                    m_ConfigState = BT_CONFIG_SET_BAUDRATE;
                }
            }
            break;

            case BT_CONFIG_SET_BAUDRATE:
                RETURN_IF_FAILED(SetBaudrate(m_desiredBaudrate));
                m_ConfigState = BT_CONFIG_GET_LOCAL_ADDR;
                break;

            case BT_CONFIG_GET_LOCAL_ADDR:
                RETURN_IF_FAILED(GetLocalAddress());
                m_ConfigState = BT_CONFIG_SET_AUDIO_MODE;
                break;

            case BT_CONFIG_SET_AUDIO_MODE:
                RETURN_IF_FAILED(SetAudioModeDigital());
                m_ConfigState = BT_CONFIG_SET_DIGITAL_AUDIO_PARAMS;
                break;

            case BT_CONFIG_SET_DIGITAL_AUDIO_PARAMS:
                RETURN_IF_FAILED(SetDigitalAudioParams());
                m_ConfigState = BT_CONFIG_SET_AUTOCONN;
                break;

            case BT_CONFIG_SET_AUTOCONN:
                RETURN_IF_FAILED(SetAutoConnEnabled());
                m_ConfigState = BT_CONFIG_SET_BT_VOLUME;
                break;

            case BT_CONFIG_SET_BT_VOLUME:
                RETURN_IF_FAILED(SetBluetoothVolume());
                m_ConfigState = BT_CONFIG_SET_COD;
                break;

            case BT_CONFIG_SET_COD:
                RETURN_IF_FAILED(SetCOD());
                m_ConfigState = BT_CONFIG_SET_CODEC;
                break;

            case BT_CONFIG_SET_CODEC:
                RETURN_IF_FAILED(SetCodec());
                m_ConfigState = BT_CONFIG_SET_DEVICE_ID;
                break;

            case BT_CONFIG_SET_DEVICE_ID:
                RETURN_IF_FAILED(SetDeviceID());
                m_ConfigState = BT_CONFIG_SET_LED_ENABLE;
                break;

            case BT_CONFIG_SET_LED_ENABLE:
                RETURN_IF_FAILED(SetLEDEnabled());
                m_ConfigState = BT_CONFIG_SET_GPIO_CONFIG;
                break;

            case BT_CONFIG_SET_GPIO_CONFIG:
                RETURN_IF_FAILED(SetGPIOConfig());
                m_ConfigState = BT_CONFIG_SET_PROFILES;
                break;

            case BT_CONFIG_SET_PROFILES:
                RETURN_IF_FAILED(SetBTProfiles());
                m_ConfigState = BT_CONFIG_SET_VREG_ROLE;
                break;

            case BT_CONFIG_SET_VREG_ROLE:
                RETURN_IF_FAILED(SetVregLongPressDisable());
                m_ConfigState = BT_CONFIG_SET_SSP_CAPS;
                break;

            case BT_CONFIG_SET_SSP_CAPS:
                RETURN_IF_FAILED(SetSSPModeConfig());
                m_ConfigState = BT_CONFIG_SET_BT_STATE;
                break;

            case BT_CONFIG_SET_BT_STATE:
                RETURN_IF_FAILED(SetBTStateConfig());
                m_ConfigState = BT_CONFIG_SET_LONG_NAME;
                break;

            case BT_CONFIG_SET_LONG_NAME:
                RETURN_IF_FAILED(SetLongName());
                m_ConfigState = BT_CONFIG_SET_SHORT_NAME;
                break;

            case BT_CONFIG_SET_SHORT_NAME:
                RETURN_IF_FAILED(SetShortName());
                m_ConfigState = BT_CONFIG_GET_PAIRED_DEVICE;
                break;

            case BT_CONFIG_GET_PAIRED_DEVICE:
                RETURN_IF_FAILED(GetRemoteAddr(m_PairedBTDevice));
                m_ConfigState = BT_CONFIG_WRITE_CFG;
                break;

            case BT_CONFIG_WRITE_CFG:
                if (m_CfgWritePending)
                {
                    RETURN_IF_FAILED(WriteConfigToFlash());
                }
                m_ConfigState = BT_CONFIG_RESTART;
                break;

            case BT_CONFIG_RESTART:
                if (m_ResetPending)
                {
                    m_ConfigState = BT_CONFIG_STATE_START;
                }
                else
                {
                    if (m_QualTestMode)
                    {
                        SetTxPower(8, 8);
                    }
                    else if (!m_isInputDevice)
                    {
                        SetTxPower(8, 8);
                    }

                    m_CurrentLinkID = "";
                    m_BTAddrWritePending = false;
                    m_StartupTimer.ResetTime(STARTUP_RECONNECT_DELAY_SEC);
                    m_waitingForPreviousDeviceConnection = true;
                    m_MonitorTimer.ResetTime(1);
                    m_ConfigState = BT_CONFIG_DONE;
                    m_Online = true;
                    NotifyAdapterOnline();
                    NotifyConnection();
                }
                break;

            default:
                return ERROR_FAILED;
        }
    }
    else
    {
        if (m_QualTestMode)
        {
            if (m_TestModeTimer.IsTimeExpired())
            {
                SendInquiry(10);
                m_TestModeTimer.ResetTime(14);
            }
        }

        if (m_QualTestMode || m_playTestSongActive)
        {
            RETURN_IF_FAILED(m_ToneControl.PlayNextMusicSequence(m_BTComm));
        }

        if (m_unpairAllDevicesFlag)
        {
            UnpairAllDevices();
        }

        if (!m_isInputDevice)
        {
            if (m_inquiryActive || (m_PairedBTDevice.empty() && m_connectDeviceAddr.empty()))
            {
                ERROR_CODE_T result = m_BTComm.ScanForBTDevices(m_detectedDeviceList,
                                                                (m_FWRevision <= FW_REV_7_0 ? true : m_ScanAllDevices), 5);
                RETURN_IF_FAILED(result);
                m_inquiryActive = (result != STATUS_SUCCESS);
                if (!m_inquiryActive)
                {
                    if (m_isAutoConnecting)
                    {
                        if (m_autoConnectTimer.IsTimeExpired())
                        {
                            m_isAutoConnecting = false;
                            NotifyConnection();
                        }
                        else if (!m_detectedDeviceList.empty())
                        {
                            m_connectDeviceAddr = m_detectedDeviceList.front()->m_btAddress;
                            m_isAutoConnecting = false;
                        }
                    }

                    NotifyDetectedDevices();
                }
            }
            else
            {
                if (!m_PairedBTDevice.empty() && !m_detectedDeviceList.empty())
                {
                    m_detectedDeviceList.clear();
                    NotifyDetectedDevices();
                }
                m_BTComm.PetBTAdapterWatchdog(true);
            }
        }
        else
        {
            m_BTComm.PetBTAdapterWatchdog(true);
        }

        if (m_BTComm.IsNewDeviceConnected())
        {
            m_MonitorTimer.ResetTime(NEW_LINK_MONITOR_TIMEOUT);
        }

        bool runMonitorStatus = false;
        if (!m_isInputDevice)
        {
            if (!m_inquiryActive)
            {
                if (!m_connectDeviceAddr.empty() && m_PairedBTDevice.empty())
                {
                    runMonitorStatus = true;
                }
                else if (m_MonitorTimer.IsTimeExpired() && (!m_connectDeviceAddr.empty() || !m_PairedBTDevice.empty()))
                {
                    runMonitorStatus = true;
                }
            }
        }
        else
        {
            if (m_MonitorTimer.IsTimeExpired())
            {
                runMonitorStatus = true;
            }
        }

        if (runMonitorStatus)
        {
            RETURN_IF_FAILED(MonitorStatus());
            RETURN_IF_FAILED(UpdatePairedDeviceList());
        }
    }

    return STATUS_SUCCESS;
}


/********************************************************************************************************
********************************************************************************************************/
ERROR_CODE_T CBTAudioCard::UpdatePairedDeviceList(void)
{
    vector<string> retStrings;
    RETURN_IF_FAILED(m_BTComm.ReadData(retStrings, "LIST"));

    ClearPairedDeviceFoundFlags();

    vector<string>::iterator it;
    for (it = retStrings.begin(); it != retStrings.end(); it++)
    {
        if (it->find("OK") != string::npos)
        {
            // This should indicate the end of the data.
            break;
        }
        else
        {
            vector<string> pairStrings;
            RETURN_IF_FAILED(SplitString(pairStrings, *it, ' ', true));
            RETURN_EC_IF_TRUE(ERROR_FAILED, pairStrings.size() < 2);

            shared_ptr<CBTEAPairedDevice> pDevice = FindPairedDevice(pairStrings[1]);
            if (pDevice != NULL)
            {
                pDevice->m_found = true;
            }
            else if (!m_isInputDevice)
            {
                string deviceName;

                if (SUCCEEDED(m_BTComm.GetConnectedDeviceName(pairStrings[1], deviceName)))
                {
                    m_pairedDeviceInfo.push_back(make_shared<CBTEAPairedDevice>(pairStrings[1], deviceName));
                }
            }
            else
            {
                m_pairedDeviceInfo.push_back(make_shared<CBTEAPairedDevice>(pairStrings[1], ""));
            }
        }
    }

    PerformBackgroundDeviceNameRetrieval();
    NotifyPairedDevices();
    return STATUS_SUCCESS;
}

/********************************************************************************************************
********************************************************************************************************/
void CBTAudioCard::ClearPairedDeviceFoundFlags(void)
{
    list<shared_ptr<CBTEAPairedDevice> >::iterator it;
    for (it = m_pairedDeviceInfo.begin(); it != m_pairedDeviceInfo.end(); it++)
    {
        (*it)->m_found = false;
    }
}

/********************************************************************************************************
********************************************************************************************************/
bool CBTAudioCard::PairedDeviceCompare(shared_ptr<CBTEAPairedDevice> pDevice1, shared_ptr<CBTEAPairedDevice> pDevice2)
{
    return pDevice1->m_found;
}

/********************************************************************************************************
********************************************************************************************************/
shared_ptr<CBTAudioCard::CBTEAPairedDevice> CBTAudioCard::FindPairedDevice(string btAddress)
{
    list<shared_ptr<CBTEAPairedDevice> >::iterator it;
    for (it = m_pairedDeviceInfo.begin(); it != m_pairedDeviceInfo.end(); it++)
    {
        if (!(*it)->m_btAddress.compare(btAddress))
        {
            return *it;
        }
    }

    return shared_ptr<CBTAudioCard::CBTEAPairedDevice>();


/********************************************************************************************************
                        ERROR_CODE_T UnpairAllDevices( void )
********************************************************************************************************/
ERROR_CODE_T CBTAudioCard::UnpairAllDevices(void)
{
    m_CurrentLinkBTAddr = "";
    m_CurrentLinkName = "";
    m_PairedBTDevice = "";
    m_connectDeviceAddr = "";
    m_unpairAllDevicesFlag = false;
    RETURN_IF_FAILED(m_BTComm.WriteData("UNPAIR"));
    RETURN_IF_FAILED(SetRemoteAddr(m_CurrentLinkBTAddr));
    RETURN_IF_FAILED(WriteConfigToFlash());
    m_pairedDeviceInfo.clear();
    m_detectedDeviceList.clear();
    NotifyDetectedDevices();
    NotifyPairedDevices();
    NotifyConnection();
    return STATUS_SUCCESS;
}

/********************************************************************************************************
                            ERROR_CODE_T SendInquiry( INT32U timeout )
********************************************************************************************************/
ERROR_CODE_T CBTAudioCard::SendInquiry(INT32U timeout)
{
    RETURN_EC_IF_TRUE(ERROR_INVALID_PARAMETER, timeout < 1 || 48 < timeout);
    RETURN_IF_FAILED(m_BTComm.WriteData("INQUIRY %d", timeout));
    return STATUS_SUCCESS;
}

/********************************************************************************************************
********************************************************************************************************/
ERROR_CODE_T CBTAudioCard::RequestConnectionToDefaultDevice(void)
{
    // If the device did not automatically try to connect, we will force the issue
    // by opening the connection ourselves
    shared_ptr<CBTEAPairedDevice> pDevice = FindPairedDevice(m_PairedBTDevice);
    if (pDevice != NULL)
    {
        string outLinkId = "";
        RETURN_IF_FAILED(m_BTComm.OpenConnection(pDevice->m_btAddress, outLinkId));
    }

    return STATUS_SUCCESS;
}

/********************************************************************************************************
********************************************************************************************************/
ERROR_CODE_T CBTAudioCard::PerformBackgroundDeviceNameRetrieval(void)
{
    string deviceName;
    bool resetRequestFlags = true;
    list<shared_ptr<CBTEAPairedDevice> >::iterator it;

    for (it = m_pairedDeviceInfo.begin(); it != m_pairedDeviceInfo.end(); it++)
    {
        shared_ptr<CBTEAPairedDevice> pairedDevice = *it;

        if (pairedDevice->m_btDeviceName.empty() && !pairedDevice->m_nameRequested)
        {
            resetRequestFlags = false;
            pairedDevice->m_nameRequested = true;

            if (SUCCEEDED(m_BTComm.GetConnectedDeviceName(pairedDevice->m_btAddress, deviceName)))
            {
                pairedDevice->m_btDeviceName = deviceName;
            }
            break;
        }
    }

    if (resetRequestFlags)
    {
        for (it = m_pairedDeviceInfo.begin(); it != m_pairedDeviceInfo.end(); it++)
        {
            shared_ptr<CBTEAPairedDevice> pairedDevice = *it;
            pairedDevice->m_nameRequested = false;
        }
    }

    return STATUS_SUCCESS;
}

/********************************************************************************************************
********************************************************************************************************/
ERROR_CODE_T CBTAudioCard::OnUnsolicitedMessageReceived(string command)
{
    RETURN_EC_IF_TRUE(STATUS_SUCCESS, m_PairedBTDevice.empty());
    vector<string> splitStrings;
    RETURN_IF_FAILED(SplitString(splitStrings, command, ' ', true));
    RETURN_EC_IF_FALSE(ERROR_FAILED, splitStrings.size() > 0);

    if (!m_isInputDevice)
    {
        if (splitStrings[0] == "LINK_LOSS")
        {
            if (splitStrings.size() > 2)
            {
                if (splitStrings[2] == "0")
                {
                    RETURN_IF_FAILED(m_BTComm.SendPlayCommand(splitStrings[1]));
                }
            }
        }
        else if (splitStrings[0] == "OPEN_OK")
        {
            if (splitStrings.size() > 1)
            {
                RETURN_IF_FAILED(m_BTComm.SendPlayCommand(splitStrings[1]));
            }
        }
    }

    return STATUS_SUCCESS;
}

#if false
/********************************************************************************************************
********************************************************************************************************/
ERROR_CODE_T CBTAudioCard::ProgramDevice(string fileName)
{
    RETURN_EC_IF_TRUE(ERROR_INVALID_PARAMETER, fileName.empty());
    shared_ptr<IUart> pUart = static_pointer_cast<IUart>(pThis->m_pCobraNetDevice);
    CBTAudioCard *pThis = this;

    shared_ptr<CDownloadLock> pDownloadLock;
    RETURN_IF_FAILED(CDownloadLock::GetDownloadLock(pDownloadLock, true, false));

    INT32U fileLength = 0;
    IN16U fileCrc = 0;
    RETURN_IF_FAILED(g_pFileManager->GetFileInfo(fileName.c_str(), NULL, &fileLength, &fileCrc));

    FILE_HANDLE fileHandle;
    RETURN_IF_FAILED(CIO::g_pFileManager->OpenFile(FILE_APP, fileName.c_str(), 0, &fileHandle, FILE_READONLY));

    ScopeExit(
        dnldCleanupSe, {
            CIO::g_pFileManager->CloseFile(fileHandle);
            pThis->m_BTComm.SetUArt(static_pointer_cast<CDevCS181012>(pThis->m_pCobraNetDevice));
            pThis->EnterCommandMode();
            pThis->Reset();
        },
        CBTAudioCard *, pThis, FILE_HANDLE, fileHandle);

    RETURN_IF_FAILED(EnterDFUMode());
    shared_ptr<IUart> pNullUart;
    m_BTComm.SetUArt(pNullUart);

    INT32U fileIndex = 0;
    while(fileIndex < fileLength)
    {
        pUart->Write
    }



    return STATUS_SUCCESS;
}
#endif

/********************************************************************************************************
                                    void GotoOfflineState( void )
Resets everything when we the BT Module goes offline.
********************************************************************************************************/
void CBTAudioCard::GotoOfflineState(void)
{
    CSimpleLock myLock(&m_criticalSection);

    m_Online = false;
    m_ConfigState = BT_CONFIG_STATE_START;
    m_CfgWritePending = false;
    m_ResetPending = false;
    m_inquiryActive = false;
    m_InterfaceMainRetryCount = 0;
    m_CurrentLinkBTAddr.clear();
    m_versionString.clear();
    m_buildString.clear();

    if (m_isInputDevice)
    {
        m_connectDeviceAddr.clear();
        m_detectedDeviceList.clear();
    }
    m_pairedDeviceInfo.clear();

    UpdateAllStatus();
}

/********************************************************************************************************
********************************************************************************************************/
ERROR_CODE_T CBTAudioCard::InitializeCommandAndStatusInfo(void)
{
    vector<shared_ptr<IConfigBlock> > pBlocks;

    RETURN_IF_FAILED(CIO::g_pNode->GetBTAdapterStatusTable(m_pStateTable));

    if (SUCCEEDED(CIO::g_pParser->GetAVBlocks(CIO::m_NodeAddress, m_CardNumber, 0, pBlocks)))
    {
        vector<shared_ptr<IConfigBlock> >::iterator iter;
        for (iter = pBlocks.begin(); iter != pBlocks.end(); iter++)
        {
            shared_ptr<IConfigBlock> pBlock = *iter;

            if (pBlock != NULL)
            {
                INT16U channelNum = pBlock->GetChannel()->GetChannelNumber() & ~OUTPUT_CHANNEL_FLAG;

                if (pBlock->GetBlockType() == INPUT_BLOCK)
                {
                    CHAR8 tempLocationString[DEVICE_NAME_LENGTH + 1];
                    sprintf(tempLocationString, "%d", channelNum);
                    m_DeviceLocationInfo = tempLocationString;

                    m_InputChannelCommandAdvertiseId = AvdsChannelBtAdapterTopics::BTA_CHANNEL_ADVERTISE_ID_COMMAND(channelNum, true);
                    m_InputChannelCommandDirection = AvdsChannelBtAdapterTopics::BTA_CHANNEL_DIRECTION_COMMAND(channelNum, true);
                    m_InputChannelStatusConnectedDevice = AvdsChannelBtAdapterTopics::BTA_CHANNEL_CONNECTED_DEVICE_STAT(channelNum, true);
                    m_InputChannelStatusPairedDevices = AvdsChannelBtAdapterTopics::BTA_CHANNEL_PAIRED_DEVICES_STAT(channelNum, true);
                    m_InputChannelStatusDirection = AvdsChannelBtAdapterTopics::BTA_CHANNEL_DIRECTION_STAT(channelNum, true);
                    m_InputChannelStatusOnline = AvdsChannelBtAdapterTopics::BTA_CHANNEL_ONLINE_STAT(channelNum, true);
                }
                else
                {
                    m_OutputChannelCommandAdvertiseId = AvdsChannelBtAdapterTopics::BTA_CHANNEL_ADVERTISE_ID_COMMAND(channelNum, false);
                    m_OutputChannelCommandDirection = AvdsChannelBtAdapterTopics::BTA_CHANNEL_DIRECTION_COMMAND(channelNum, false);
                    m_OutputChannelCommandPair = AvdsChannelBtAdapterTopics::BTA_CHANNEL_PAIR_COMMAND(channelNum, false);
                    m_OutputChannelCommandUnpair = AvdsChannelBtAdapterTopics::BTA_CHANNEL_UNPAIR_COMMAND(channelNum, false);
                    m_OutputChannelCommandAutoPair = AvdsChannelBtAdapterTopics::BTA_CHANNEL_AUTOPAIR_COMMAND(channelNum, false);
                    m_OutputChannelStatusConnectedDevice = AvdsChannelBtAdapterTopics::BTA_CHANNEL_CONNECTED_DEVICE_STAT(channelNum, false);
                    m_OutputChannelStatusDetectedDevices = AvdsChannelBtAdapterTopics::BTA_CHANNEL_DETECTED_DEVICES_STAT(channelNum, false);
                    m_OutputChannelStatusPairedDevices = AvdsChannelBtAdapterTopics::BTA_CHANNEL_PAIRED_DEVICES_STAT(channelNum, false);
                    m_OutputChannelStatusDirection = AvdsChannelBtAdapterTopics::BTA_CHANNEL_DIRECTION_STAT(channelNum, false);
                    m_OutputChannelStatusOnline = AvdsChannelBtAdapterTopics::BTA_CHANNEL_ONLINE_STAT(channelNum, false);
                }
            }
        }
    }

    RETURN_IF_FAILED(CMQTTConnection::ConnectToServer(m_mqttConnection));
    m_pMqttConnectionChangedEvent = m_mqttConnection->OnConnectionChange.registerObserver<CBTAudioCard>(shared_from_this(),
                                                                                                        &CBTAudioCard::mqttOnConnectionChanged);
    m_pSubscribeEvent = m_mqttConnection->OnSubscribedDataReceived.registerObserver<CBTAudioCard>(shared_from_this(),
                                                                                                  &CBTAudioCard::mqttOnSubscribedDataChanged);

    mqttOnConnectionChanged(m_mqttConnection->IsConnected());

    UpdateAllStatus();
    return STATUS_SUCCESS;
}

/********************************************************************************************************
********************************************************************************************************/
ERROR_CODE_T CBTAudioCard::mqttOnConnectionChanged(bool connectionState)
{
    if (connectionState)
    {
        RETURN_IF_FAILED(m_mqttConnection->Subscribe(AvdsCardBtAdapterTopics::BTA_CARD_ADVERTISE_ID_COMMAND(m_pCardMap->slotNumber)));
        RETURN_IF_FAILED(m_mqttConnection->Subscribe(AvdsCardBtAdapterTopics::BTA_CARD_DIRECTION_COMMAND(m_pCardMap->slotNumber)));
        RETURN_IF_FAILED(m_mqttConnection->Subscribe(AvdsCardBtAdapterTopics::BTA_CARD_PAIR_COMMAND(m_pCardMap->slotNumber)));
        RETURN_IF_FAILED(m_mqttConnection->Subscribe(AvdsCardBtAdapterTopics::BTA_CARD_UNPAIR_COMMAND(m_pCardMap->slotNumber)));
        RETURN_IF_FAILED(m_mqttConnection->Subscribe(AvdsCardBtAdapterTopics::BTA_CARD_AUTOPAIR_COMMAND(m_pCardMap->slotNumber)));

        if (!m_InputChannelCommandAdvertiseId.empty())
        {
            RETURN_IF_FAILED(m_mqttConnection->Subscribe(m_InputChannelCommandAdvertiseId));
        }

        if (!m_InputChannelCommandDirection.empty())
        {
            RETURN_IF_FAILED(m_mqttConnection->Subscribe(m_InputChannelCommandDirection));
        }

        if (!m_OutputChannelCommandAdvertiseId.empty())
        {
            RETURN_IF_FAILED(m_mqttConnection->Subscribe(m_OutputChannelCommandAdvertiseId));
        }

        if (!m_OutputChannelCommandDirection.empty())
        {
            RETURN_IF_FAILED(m_mqttConnection->Subscribe(m_OutputChannelCommandDirection));
        }

        if (!m_OutputChannelCommandPair.empty())
        {
            RETURN_IF_FAILED(m_mqttConnection->Subscribe(m_OutputChannelCommandPair));
        }

        if (!m_OutputChannelCommandUnpair.empty())
        {
            RETURN_IF_FAILED(m_mqttConnection->Subscribe(m_OutputChannelCommandUnpair));
        }

        if (!m_OutputChannelCommandAutoPair.empty())
        {
            RETURN_IF_FAILED(m_mqttConnection->Subscribe(m_OutputChannelCommandAutoPair));
        }

        UpdateAllStatus();
    }

    return STATUS_SUCCESS;
}

/********************************************************************************************************
********************************************************************************************************/
ERROR_CODE_T CBTAudioCard::mqttOnSubscribedDataChanged(shared_ptr<IMQTTEventNotification> pEvent)
{
    string topic = pEvent->GetTopic();
    shared_ptr<IJsonDocument> pJsonDoc;

    string data;
    RETURN_IF_FAILED(pEvent->GetValue(data));
    RETURN_EC_IF_TRUE(ERROR_INVALID_PARAMETER, data.size() == 0);
    RETURN_IF_FAILED(JsonDocument::Deserialize(data, pJsonDoc));

    if (topic == m_InputChannelCommandAdvertiseId || topic == m_OutputChannelCommandAdvertiseId ||
        topic == AvdsCardBtAdapterTopics::BTA_CARD_ADVERTISE_ID_COMMAND(m_pCardMap->slotNumber))
    {
        string newName;
        RETURN_IF_FAILED(pJsonDoc->GetElement("name", newName));

        if (!newName.empty())
        {
            if ((m_isInputDevice && topic == m_InputChannelCommandAdvertiseId) ||
                (!m_isInputDevice && topic == m_OutputChannelCommandAdvertiseId) ||
                topic == AvdsCardBtAdapterTopics::BTA_CARD_ADVERTISE_ID_COMMAND(m_pCardMap->slotNumber))
            {
                if (newName.compare(m_DeviceName))
                {
                    shared_ptr<INodeCommand> pNodeCommand;
                    RETURN_IF_FAILED(CIO::g_pNode->GetNodeCommand(pNodeCommand));
                    pNodeCommand->SetBTDeviceName(CIO::m_NodeAddress, m_pCardMap->slotNumber, newName);
                    memset(m_DeviceName, 0, DEVICE_NAME_LENGTH);
                    strncpy(m_DeviceName, newName.c_str(), DEVICE_NAME_MAX);
                    GotoOfflineState();
                }
            }
            RETURN_IF_FAILED(pJsonDoc->SetElement("name", ""));
            m_mqttConnection->Publish(topic, QoS_AtLeastOnce, true, pJsonDoc);
        }
    }
    else if (topic == m_InputChannelCommandDirection || topic == m_OutputChannelCommandDirection ||
             topic == AvdsCardBtAdapterTopics::BTA_CARD_DIRECTION_COMMAND(m_pCardMap->slotNumber))
    {
        string direction;
        RETURN_IF_FAILED(pJsonDoc->GetElement("direction", direction));

        if (!direction.empty())
        {
            if (direction == "input" || direction == "output")
            {
                if ((m_isInputDevice && topic == m_InputChannelCommandDirection) ||
                    (!m_isInputDevice && topic == m_OutputChannelCommandDirection) ||
                    topic == AvdsCardBtAdapterTopics::BTA_CARD_DIRECTION_COMMAND(m_pCardMap->slotNumber))
                {
                    bool newIsInputDevice = direction == "input";
                    if (m_isInputDevice != newIsInputDevice)
                    {
                        bool isValidDirection = false;
                        DirectionIsValid(newIsInputDevice, isValidDirection);

                        if (isValidDirection)
                        {
                            shared_ptr<INodeCommand> pNodeCommand;
                            RETURN_IF_FAILED(CIO::g_pNode->GetNodeCommand(pNodeCommand));
                            pNodeCommand->SetBTDeviceDirection(CIO::m_NodeAddress, m_pCardMap->slotNumber, newIsInputDevice);

                            m_isInputDevice = newIsInputDevice;
                            UnpairAllDevices();
                            UpdateAllStatus();
                            GotoOfflineState();
                        }
                    }
                }
            }
            RETURN_IF_FAILED(pJsonDoc->SetElement("direction", ""));
            m_mqttConnection->Publish(topic, QoS_AtLeastOnce, true, pJsonDoc);
        }
    }
    else if (topic == m_OutputChannelCommandPair || topic == AvdsCardBtAdapterTopics::BTA_CARD_PAIR_COMMAND(m_pCardMap->slotNumber))
    {
        string deviceAddress;
        RETURN_IF_FAILED(pJsonDoc->GetElement("device_address", deviceAddress));

        if (!deviceAddress.empty())
        {
            if (!m_isInputDevice)
            {
                RETURN_IF_FAILED(m_pMsgQueue->SetBTAdapterPair((CHAR8 *)deviceAddress.c_str()));
            }

            pJsonDoc->SetElement("device_address", "");
            m_mqttConnection->Publish(topic, QoS_AtLeastOnce, true, pJsonDoc);
        }
    }
    else if (topic == m_OutputChannelCommandUnpair || topic == AvdsCardBtAdapterTopics::BTA_CARD_UNPAIR_COMMAND(m_pCardMap->slotNumber))
    {
        bool unpair;
        RETURN_IF_FAILED(pJsonDoc->GetElement("unpair", unpair));

        if (unpair)
        {
            if (!m_isInputDevice)
            {
                RETURN_IF_FAILED(m_pMsgQueue->SetBTAdapterUnpair());
            }

            pJsonDoc->SetElement("unpair", false);
            m_mqttConnection->Publish(topic, QoS_AtLeastOnce, true, pJsonDoc);
        }
    }
    else if (topic == m_OutputChannelCommandAutoPair || topic == AvdsCardBtAdapterTopics::BTA_CARD_AUTOPAIR_COMMAND(m_pCardMap->slotNumber))
    {
        bool autopair;
        INT32U timeout;
        RETURN_IF_FAILED(pJsonDoc->GetElement("autopair", autopair));

        if (autopair)
        {
            RETURN_IF_FAILED(pJsonDoc->GetElement("timeout", timeout));
            RETURN_EC_IF_FALSE(ERROR_INVALID_PARAMETER, timeout <= 300);
            if (timeout)
            {
                RETURN_IF_FAILED(m_pMsgQueue->SetBTAdapterAutoPair(timeout));
            }
            else if (m_isAutoConnecting)
            {
                m_isAutoConnecting = false;
                NotifyConnection();
            }
        }
        else if (m_isAutoConnecting)
        {
            m_isAutoConnecting = false;
            NotifyConnection();
        }
    }

    return STATUS_SUCCESS;
}

/********************************************************************************************************
********************************************************************************************************/
ERROR_CODE_T CBTAudioCard::UpdateAllStatus(void)
{
    NotifyAdapterOnline();
    NotifyDetectedDevices();
    NotifyPairedDevices();
    NotifyConnection();
    NotifyDirection();
    return STATUS_SUCCESS;
}

/********************************************************************************************************
********************************************************************************************************/
ERROR_CODE_T CBTAudioCard::NotifyConnection(void)
{
    RETURN_IF_FAILED(NotifyConnection(!m_CurrentLinkBTAddr.empty()));
    return STATUS_SUCCESS;
}

/********************************************************************************************************
********************************************************************************************************/
ERROR_CODE_T CBTAudioCard::NotifyConnection(bool isConnected)
{
    RETURN_EC_IF_NULL(ERROR_NOT_INITIALIZED, m_pStateTable);
    m_pStateTable->SetConnectionState(m_CardNumber, isConnected, m_isAutoConnecting, m_CurrentLinkBTAddr, m_CurrentLinkName, m_PairedBTDevice);

    if (!m_notificationUrl.empty() && isConnected)
    {
        string url = m_notificationUrl + "/" + m_DeviceLocationInfo;
        string message = string() +
                         "{ \n" +
                         "\"id\":" + m_DeviceLocationInfo + ",\n" +
                         "\"connected\":" + (!m_CurrentLinkBTAddr.empty() ? "true" : "false") + ",\n" +
                         "\"deviceName\":\"" + m_CurrentLinkName + "\"\n" +
                         "}";

        RETURN_IF_FAILED(CHttpClient::PutMessage(url, message, MIME_TYPE_APPLICATION_JSON));
    }

    if (m_mqttConnection->IsConnected())
    {
        shared_ptr<IJsonDocument> document = JsonDocument::Create();
        RETURN_EC_IF_NULL(ERROR_EXCEEDS_MEMORY, document);
        RETURN_IF_FAILED(document->SetElement("status", (isConnected ? "connected" : (m_isAutoConnecting ? "connecting" : "disconnected"))));
        RETURN_IF_FAILED(document->SetElement("device_address", m_CurrentLinkBTAddr));
        RETURN_IF_FAILED(document->SetElement("device_name", m_CurrentLinkName));
        RETURN_IF_FAILED(m_mqttConnection->Publish(AvdsCardBtAdapterTopics::BTA_CARD_CONNECTED_DEVICE_STAT(m_pCardMap->slotNumber),
                                                   QoS_AtLeastOnce, true, document));

        shared_ptr<IJsonDocument> inactive_document = JsonDocument::Create();
        RETURN_EC_IF_NULL(ERROR_EXCEEDS_MEMORY, inactive_document);
        RETURN_IF_FAILED(inactive_document->SetElement("is_connected", false));

        if (!m_InputChannelStatusConnectedDevice.empty())
        {
            if (m_isInputDevice)
            {
                RETURN_IF_FAILED(m_mqttConnection->Publish(m_InputChannelStatusConnectedDevice,
                                                           QoS_AtLeastOnce, true, document));
            }
            else
            {
                RETURN_IF_FAILED(m_mqttConnection->Publish(m_InputChannelStatusConnectedDevice,
                                                           QoS_AtLeastOnce, true, inactive_document));
            }
        }

        if (!m_OutputChannelStatusConnectedDevice.empty())
        {
            if (!m_isInputDevice)
            {
                RETURN_IF_FAILED(m_mqttConnection->Publish(m_OutputChannelStatusConnectedDevice,
                                                           QoS_AtLeastOnce, true, document));
            }
            else
            {
                RETURN_IF_FAILED(m_mqttConnection->Publish(m_OutputChannelStatusConnectedDevice,
                                                           QoS_AtLeastOnce, true, inactive_document));
            }
        }
    }

    return STATUS_SUCCESS;
}

/********************************************************************************************************
********************************************************************************************************/
ERROR_CODE_T CBTAudioCard::NotifyAdapterOnline(void)
{
    SetReadyFlagEx(m_ReadyFlag, m_ReadyFlagString.c_str(), m_Online);
    m_pStateTable->SetAdapterOnline(m_CardNumber, m_Online);
    RETURN_EC_IF_NULL(ERROR_NOT_INITIALIZED, m_mqttConnection);

    if (m_mqttConnection->IsConnected())
    {
        shared_ptr<IJsonDocument> document = JsonDocument::Create();
        RETURN_EC_IF_NULL(ERROR_EXCEEDS_MEMORY, document);
        RETURN_IF_FAILED(document->SetElement("schema.semver", MQTT_SCHEMA_REVISION));
        RETURN_IF_FAILED(document->SetElement("schema.hash", MQTT_SCHEMA_HASH));
        RETURN_IF_FAILED(document->PushArrayElement("dependencies", AvdsDeviceTopics::NODE_ONLINE_TOPIC()));
        RETURN_IF_FAILED(document->SetElement("online", m_Online));
        RETURN_IF_FAILED(m_mqttConnection->Publish(AvdsDeviceTopics::CARD_ONLINE_TOPIC(m_pCardMap->slotNumber),
                                                   QoS_AtLeastOnce, true, document));

        document = JsonDocument::Create();
        RETURN_EC_IF_NULL(ERROR_EXCEEDS_MEMORY, document);
        RETURN_IF_FAILED(document->SetElement("online", m_Online));

        if (!m_OutputChannelStatusOnline.empty())
        {
            RETURN_IF_FAILED(m_mqttConnection->Publish(m_OutputChannelStatusOnline, QoS_AtLeastOnce, true, document));
        }

        if (!m_InputChannelStatusOnline.empty())
        {
            RETURN_IF_FAILED(m_mqttConnection->Publish(m_InputChannelStatusOnline, QoS_AtLeastOnce, true, document));
        }

        document = JsonDocument::Create();
        RETURN_EC_IF_NULL(ERROR_EXCEEDS_MEMORY, document);
        RETURN_IF_FAILED(document->SetElement("version", m_versionString));
        RETURN_IF_FAILED(document->SetElement("build", m_buildString));
        RETURN_IF_FAILED(m_mqttConnection->Publish(AvdsCardBtAdapterTopics::BTA_CARD_FIRMWARE_STAT(m_pCardMap->slotNumber),
                                                   QoS_AtLeastOnce, true, document));
    }

    return STATUS_SUCCESS;
}

/********************************************************************************************************
********************************************************************************************************/
ERROR_CODE_T CBTAudioCard::NotifyDirection(void)
{
    if (m_mqttConnection->IsConnected())
    {
        shared_ptr<IJsonDocument> document = JsonDocument::Create();
        RETURN_EC_IF_NULL(ERROR_EXCEEDS_MEMORY, document);
        RETURN_IF_FAILED(document->SetElement("direction", (m_isInputDevice ? "input" : "output")));
        RETURN_IF_FAILED(m_mqttConnection->Publish(AvdsCardBtAdapterTopics::BTA_CARD_DIRECTION_STAT(m_pCardMap->slotNumber),
                                                   QoS_AtLeastOnce, true, document));
        if (!m_InputChannelStatusDirection.empty())
        {
            RETURN_IF_FAILED(m_mqttConnection->Publish(m_InputChannelStatusDirection,
                                                       QoS_AtLeastOnce, true, document));
        }

        if (!m_OutputChannelStatusDirection.empty())
        {
            RETURN_IF_FAILED(m_mqttConnection->Publish(m_OutputChannelStatusDirection,
                                                       QoS_AtLeastOnce, true, document));
        }
    }

    return STATUS_SUCCESS;
}

/********************************************************************************************************
********************************************************************************************************/
ERROR_CODE_T CBTAudioCard::NotifyDetectedDevices(void)
{
    if (m_detectedDeviceList.empty())
    {
        m_pStateTable->ClearDetectedDeviceList(m_CardNumber);
    }
    else
    {
        list<shared_ptr<CBTEADetectedDevice> >::iterator it;
        for (it = m_detectedDeviceList.begin(); it != m_detectedDeviceList.end();)
        {
            shared_ptr<CBTEADetectedDevice> detectedDevice = *it;
            if (detectedDevice->timeoutCounter)
            {
                if (!detectedDevice->notificationSent)
                {
                    m_pStateTable->AddDetectedDevice(m_CardNumber, (*it)->m_btAddress, (*it)->m_btDeviceName);
                    detectedDevice->notificationSent = true;
                }
                it++;
            }
            else
            {
                m_pStateTable->RemoveDetectedDevice(m_CardNumber, (*it)->m_btAddress);
                it = m_detectedDeviceList.erase(it);
            }
        }
    }

    if (m_mqttConnection->IsConnected())
    {
        if (!m_OutputChannelStatusDetectedDevices.empty())
        {
            if (m_isInputDevice || m_detectedDeviceList.empty())
            {
                shared_ptr<IJsonDocument> document = JsonDocument::Create();
                RETURN_EC_IF_NULL(ERROR_EXCEEDS_MEMORY, document);
                shared_ptr<IJsonDocument> element = JsonDocument::Create();
                RETURN_EC_IF_NULL(ERROR_EXCEEDS_MEMORY, element);
                RETURN_IF_FAILED(document->PushArrayElement("detected_devices", element));
                RETURN_IF_FAILED(m_mqttConnection->Publish(m_OutputChannelStatusDetectedDevices,
                                                           QoS_AtLeastOnce, true, document));

                if (!m_isInputDevice)
                {
                    RETURN_IF_FAILED(m_mqttConnection->Publish(AvdsCardBtAdapterTopics::BTA_CARD_DETECTED_DEVICES_STAT(m_pCardMap->slotNumber),
                                                               QoS_AtLeastOnce, true, document));
                }
            }
            else
            {
                shared_ptr<IJsonDocument> document = JsonDocument::Create();
                RETURN_EC_IF_NULL(ERROR_EXCEEDS_MEMORY, document);

                list<shared_ptr<CBTEADetectedDevice> >::iterator it;
                for (it = m_detectedDeviceList.begin(); it != m_detectedDeviceList.end(); it++)
                {
                    shared_ptr<IJsonDocument> element = JsonDocument::Create();
                    RETURN_EC_IF_NULL(ERROR_EXCEEDS_MEMORY, element);
                    RETURN_IF_FAILED(element->SetElement("address", (*it)->m_btAddress));
                    RETURN_IF_FAILED(element->SetElement("name", (*it)->m_btDeviceName));
                    RETURN_IF_FAILED(document->PushArrayElement("detected_devices", element));
                }

                RETURN_IF_FAILED(m_mqttConnection->Publish(m_OutputChannelStatusDetectedDevices,
                                                           QoS_AtLeastOnce, true, document));

                RETURN_IF_FAILED(m_mqttConnection->Publish(AvdsCardBtAdapterTopics::BTA_CARD_DETECTED_DEVICES_STAT(m_pCardMap->slotNumber),
                                                           QoS_AtLeastOnce, true, document));
            }
        }
    }
    return STATUS_SUCCESS;
}

/********************************************************************************************************
********************************************************************************************************/
ERROR_CODE_T CBTAudioCard::NotifyPairedDevices(void)
{
    // The invalid document is used to clear the paired devices list in the case of no paired devices,
    // and to handle when the direction of the adapter has changed.
    shared_ptr<IJsonDocument> invalid_document = JsonDocument::Create();
    RETURN_EC_IF_NULL(ERROR_EXCEEDS_MEMORY, invalid_document);
    shared_ptr<IJsonDocument> invalid_element = JsonDocument::Create();
    RETURN_EC_IF_NULL(ERROR_EXCEEDS_MEMORY, invalid_element);
    RETURN_IF_FAILED(invalid_document->PushArrayElement("paired_devices", invalid_element));

    shared_ptr<IJsonDocument> document = JsonDocument::Create();
    RETURN_EC_IF_NULL(ERROR_EXCEEDS_MEMORY, document);

    if (m_isInputDevice)
    {
        RETURN_EC_IF_NULL(ERROR_NOT_INITIALIZED, m_pStateTable);
        list<shared_ptr<CBTEAPairedDevice> > newPairDeviceList;

        list<shared_ptr<CBTEAPairedDevice> >::iterator it;
        for (it = m_pairedDeviceInfo.begin(); it != m_pairedDeviceInfo.end(); it++)
        {
            if ((*it)->m_found)
            {
                newPairDeviceList.push_back(*it);
                m_pStateTable->AddDetectedDevice(m_CardNumber, (*it)->m_btAddress, (*it)->m_btDeviceName);
            }
            else
            {
                m_pStateTable->RemoveDetectedDevice(m_CardNumber, (*it)->m_btAddress);
            }
        }

        if (m_mqttConnection->IsConnected())
        {
            list<shared_ptr<CBTEAPairedDevice> >::iterator it;
            for (it = newPairDeviceList.begin(); it != newPairDeviceList.end(); it++)
            {
                shared_ptr<IJsonDocument> element = JsonDocument::Create();
                RETURN_EC_IF_NULL(ERROR_EXCEEDS_MEMORY, element);
                RETURN_IF_FAILED(element->SetElement("address", (*it)->m_btAddress));
                RETURN_IF_FAILED(element->SetElement("name", (*it)->m_btDeviceName));
                RETURN_IF_FAILED(document->PushArrayElement("paired_devices", element));
            }

            if (!m_OutputChannelStatusPairedDevices.empty())
            {
                RETURN_IF_FAILED(m_mqttConnection->Publish(m_OutputChannelStatusPairedDevices,
                                                           QoS_AtLeastOnce, true, invalid_document));
            }

            if (!m_InputChannelStatusPairedDevices.empty())
            {
                if (newPairDeviceList.empty())
                {
                    RETURN_IF_FAILED(m_mqttConnection->Publish(m_InputChannelStatusPairedDevices,
                                                               QoS_AtLeastOnce, true, invalid_document));
                }
                else
                {
                    RETURN_IF_FAILED(m_mqttConnection->Publish(m_InputChannelStatusPairedDevices,
                                                               QoS_AtLeastOnce, true, document));
                }
            }

            if (newPairDeviceList.empty())
            {

                RETURN_IF_FAILED(m_mqttConnection->Publish(AvdsCardBtAdapterTopics::BTA_CARD_PAIRED_DEVICES_STAT(m_pCardMap->slotNumber),
                                                           QoS_AtLeastOnce, true, document));
            }
            else
            {
                RETURN_IF_FAILED(m_mqttConnection->Publish(AvdsCardBtAdapterTopics::BTA_CARD_PAIRED_DEVICES_STAT(m_pCardMap->slotNumber),
                                                           QoS_AtLeastOnce, true, document));
            }
        }

        m_pairedDeviceInfo = newPairDeviceList;
    }
    else
    {
        if (m_mqttConnection->IsConnected())
        {
            shared_ptr<IJsonDocument> element = JsonDocument::Create();
            RETURN_EC_IF_NULL(ERROR_EXCEEDS_MEMORY, element);
            RETURN_IF_FAILED(element->SetElement("address", m_PairedBTDevice));
            if (m_PairedBTDeviceName.empty())
            {
                RETURN_IF_FAILED(element->SetElement("name", "UNKNOWN"));
            }
            else
            {
                RETURN_IF_FAILED(element->SetElement("name", m_PairedBTDeviceName));
            }
            RETURN_IF_FAILED(document->PushArrayElement("paired_devices", element));

            if (!m_InputChannelStatusPairedDevices.empty())
            {
                RETURN_IF_FAILED(m_mqttConnection->Publish(m_InputChannelStatusPairedDevices,
                                                           QoS_AtLeastOnce, true, invalid_document));
            }

            if (!m_OutputChannelStatusPairedDevices.empty())
            {
                if (m_PairedBTDevice.empty())
                {
                    RETURN_IF_FAILED(m_mqttConnection->Publish(m_OutputChannelStatusPairedDevices,
                                                               QoS_AtLeastOnce, true, invalid_document));
                }
                else
                {
                    RETURN_IF_FAILED(m_mqttConnection->Publish(m_OutputChannelStatusPairedDevices,
                                                               QoS_AtLeastOnce, true, document));
                }
            }

            if (m_PairedBTDevice.empty())
            {
                RETURN_IF_FAILED(m_mqttConnection->Publish(AvdsCardBtAdapterTopics::BTA_CARD_PAIRED_DEVICES_STAT(m_pCardMap->slotNumber),
                                                           QoS_AtLeastOnce, true, document));
            }
            else
            {
                RETURN_IF_FAILED(m_mqttConnection->Publish(AvdsCardBtAdapterTopics::BTA_CARD_PAIRED_DEVICES_STAT(m_pCardMap->slotNumber),
                                                           QoS_AtLeastOnce, true, document));
            }
        }
    }

    return STATUS_SUCCESS;
}
