#include "BTAPairingManager.h"

#include "BTADeviceDriver.h"
#include "BTAStatusTable.h"
#include "ExtIO.h"
#include "IO.h"
#include "ScopeExit.h"

const CHAR8 *CBTAPairingManager::s_InquireCODList[] =
    {
        "240404", // Audio Rendering, Wearable Headset Device
        //"240414", // Audio Rendering, Loudspeaker
        "240418", // Audio Rendering, Headphones
        "2C0404", // Audio Rendering/Capturing, Wearable Headset Device
};

BOOLEAN CBTAPairingManager::IsNewDeviceConnected(void)
{
    CSimpleLock myLock(&m_CS);
    bool temp = m_receivedOpenNotification;
    m_receivedOpenNotification = false;
    return temp;
}

void CBTAPairingManager::ResetStateMachine(void)
{
    CSimpleLock myLock(&m_CS);
    m_scanState = SS_INIT;
    m_scanAbort = false;
    m_nextInquireIndex = 0;
    m_receivedOpenNotification = false;
}

ERROR_CODE_T CBTAPairingManager::OnOpenNotificationReceived(BOOLEAN state)
{
    CSimpleLock myLock(&m_CS);
    DebugPrintf(DEBUG_TRACE, DEBUG_TRACE, m_LogId.c_str(), "Open notification received: %s\n", state ? "true" : "false");
    m_receivedOpenNotification = state;
    return STATUS_SUCCESS;
}

ERROR_CODE_T CBTAPairingManager::GetConnectedDeviceName(string btAddress, string &nameString)
{
    CSimpleLock myLock(&m_CS);
    nameString = "";

    if (m_deviceNameMap.find(btAddress) != m_deviceNameMap.end())
    {
        nameString = m_deviceNameMap[btAddress];
    }

    if (nameString.empty())
    {
        m_pBTASerialDevice->FlushRxBuffer();
        RETURN_IF_FAILED(m_pBTASerialDevice->WriteData("NAME " + btAddress, true));

        CTimeDeltaSec messageTimer(10);
        while (!messageTimer.IsTimeExpired() && nameString.empty())
        {
            PetWatchdog();
            RETURN_EC_IF_TRUE(ERROR_FAILED, m_pBTASerialDevice->GetCancelCurrentCommand());

            list<string> responseStrings;
            if (SUCCEEDED(m_pBTASerialDevice->ReceiveData(responseStrings, 100)))
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
                            nameString = m_pBTASerialDevice->Utf8Decode(deviceNameStrings[1]);
                            m_deviceNameMap.insert(pair<string, string>(btAddress, nameString));
                            break;
                        }
                    }
                }
            }

            OSTimeDly(OS_TICKS_PER_SEC);
            m_pBTASerialDevice->PetBTAdapterWatchdog();
        }

        m_pBTASerialDevice->PetBTAdapterWatchdog(true);
    }

    RETURN_EC_IF_TRUE(ERROR_FAILED, nameString.empty());
    return STATUS_SUCCESS;
}

shared_ptr<CBTEAPairedDevice> CBTAPairingManager::FindPairedDevice(string btAddress)
{
    list<shared_ptr<CBTEAPairedDevice> >::iterator it;

    for (it = m_pairedDeviceInfo.begin(); it != m_pairedDeviceInfo.end(); it++)
    {
        if (!(*it)->m_btAddress.compare(btAddress))
        {
            return *it;
        }
    }

    return shared_ptr<CBTEAPairedDevice>();
}

ERROR_CODE_T CBTAPairingManager::RequestConnectionToDefaultDevice(void)
{
    // If the device did not automatically try to connect, we will force the issue
    // by opening the connection ourselves
    shared_ptr<CBTEAPairedDevice> pDevice = FindPairedDevice(m_PairedBtDeviceAddress);
    if (pDevice != NULL)
    {
        string outLinkId = "";
        RETURN_IF_FAILED(OpenConnection(pDevice->m_btAddress, outLinkId));
    }

    return STATUS_SUCCESS;
}

static string timeoutToString(INT8U timeoutSec)
{
    return "" + (INT32U)(timeoutSec / 1.28f);
}

ERROR_CODE_T CBTAPairingManager::ScanForBtDevices(list<shared_ptr<CBTEADetectedDevice> > &detectedDeviceList, bool scanAllDevices, INT8U timeoutSec)
{
    CSimpleLock myLock(&m_CS);
    CBTAPairingManager *pThis = this;

    ScopeExit(
        scanCleanupSe, {
            pThis->m_scanState = SS_INIT;
            pThis->m_scanAbort = false;
        },
        CBTAPairingManager *, pThis);

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
            m_pBTASerialDevice->PetBTAdapterWatchdog(true);

            if (!scanAllDevices)
            {
                RETURN_IF_FAILED(
                    m_pBTASerialDevice->WriteData(
                        "INQUIRY " + timeoutToString(timeoutSec) + "2" + s_InquireCODList[m_nextInquireIndex], true));
                m_currentCOD = s_InquireCODList[m_nextInquireIndex];
                m_nextInquireIndex++;
                if (m_nextInquireIndex >= sizeof(s_InquireCODList) / sizeof(CHAR8 *))
                {
                    m_nextInquireIndex = 0;
                }
            }
            else
            {
                RETURN_IF_FAILED(m_pBTASerialDevice->WriteData("INQUIRY " + timeoutToString(timeoutSec), true));
            }
            m_scanTimer.ResetTime(timeoutSec + 1);
            m_scanState = SS_WAIT_FOR_SCAN_RESPONSE;
            break;

        case SS_WAIT_FOR_SCAN_RESPONSE:
            RETURN_EC_IF_TRUE(ERROR_OPERATION_TIMED_OUT, m_scanTimer.IsTimeExpired());
            RETURN_EC_IF_TRUE(ERROR_FAILED, m_pBTASerialDevice->GetCancelCurrentCommand());

            {
                list<string> responseStrings;
                if (SUCCEEDED(m_pBTASerialDevice->ReceiveData(responseStrings, 100)))
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
                                string deviceName = m_pBTASerialDevice->Utf8Decode(deviceNameStrings[1]);

                                vector<string> remainingStrings;
                                deviceNameStrings[2] = deviceNameStrings[2].substr(1);
                                RETURN_IF_FAILED(SplitString(remainingStrings, deviceNameStrings[2], ' ', true));

                                if (remainingStrings.size() == 2)
                                {
                                    string deviceCOD = remainingStrings[0];
                                    string rssi = remainingStrings[1];
                                    INT32U deviceCODValue = strtol(remainingStrings[0].c_str(), NULL, 16);

                                    if ((!scanAllDevices && to_lower(deviceCOD) == to_lower(m_currentCOD)) ||
                                        (scanAllDevices && IsValidAudioRenderingDevice(deviceCODValue)))
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

            m_pBTASerialDevice->PetBTAdapterWatchdog();
            break;

        case SS_CLEANUP:
            if (!detectedDeviceList.empty())
            {
                // filter out any detected devices that are our own BT Adapters
                shared_ptr<IBTAdapterConfigTable> pConfigTable;
                CIO::g_pNode->GetBTAdapterConfigTable(pConfigTable);
                if (pConfigTable != NULL)
                {
                    list<shared_ptr<CBTEADetectedDevice> >::iterator it;
                    for (it = detectedDeviceList.begin(); it != detectedDeviceList.end();)
                    {
                        if (pConfigTable->IsIABluetoothAdapter((*it)->m_btAddress) ||
                            pConfigTable->IsPairedToIABluetoothAdapter((*it)->m_btAddress))
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
    // Expected output
    return (m_scanState == SS_INIT) ? STATUS_OPERATION_INCOMPLETE : STATUS_SUCCESS;
}

ERROR_CODE_T CBTAPairingManager::AbortBtDeviceScan(void)
{
    CSimpleLock myLock(&m_CS);
    m_scanAbort = true;
    m_pBTASerialDevice->SetCancelCurrentCommand(true);

    return STATUS_SUCCESS;
}

ERROR_CODE_T CBTAPairingManager::UnpairAllDevices()
{
    RETURN_IF_FAILED(m_pBTASerialDevice->WriteData("UNPAIR"));

    return STATUS_SUCCESS;
}

ERROR_CODE_T CBTAPairingManager::OpenConnection(string btAddress, string &linkIdOut)
{
    CSimpleLock myLock(&m_CS);

    m_pBTASerialDevice->FlushRxBuffer();
    RETURN_IF_FAILED(m_pBTASerialDevice->WriteData("OPEN " + btAddress + " A2DP 0", true));
    TRI_STATE_T success = TS_AUTO;

    list<string> expectedResponseStrings;
    expectedResponseStrings.push_back("OPEN_OK");
    expectedResponseStrings.push_back("OPEN_ERROR");
    expectedResponseStrings.push_back("PAIR_ERROR");

    CTimeDeltaSec scanTimer(10);
    while (!scanTimer.IsTimeExpired() && success == TS_AUTO)
    {
        PetWatchdog();
        RETURN_EC_IF_TRUE(ERROR_FAILED, m_pBTASerialDevice->GetCancelCurrentCommand());

        list<string> responseStrings;
        ERROR_CODE_T er = m_pBTASerialDevice->ReceiveData(responseStrings, 100, expectedResponseStrings);
        if (SUCCEEDED(er))
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
        m_pBTASerialDevice->PetBTAdapterWatchdog();
    }

    RETURN_EC_IF_FALSE(ERROR_FAILED, success == TS_TRUE);
    return STATUS_SUCCESS;
}

ERROR_CODE_T CBTAPairingManager::PairDevice(string btAddress, string &linkIdOut)
{
    CSimpleLock myLock(&m_CS);

    m_pBTASerialDevice->FlushRxBuffer();
    RETURN_IF_FAILED(m_pBTASerialDevice->WriteData("PAIR " + btAddress, true));
    TRI_STATE_T success = TS_AUTO;

    list<string> expectedResponseStrings;
    expectedResponseStrings.push_back("PAIR_OK");
    expectedResponseStrings.push_back("PAIR_ERROR");

    CTimeDeltaSec scanTimer(10);
    while (!scanTimer.IsTimeExpired() && success == TS_AUTO)
    {
        PetWatchdog();
        RETURN_EC_IF_TRUE(ERROR_FAILED, m_pBTASerialDevice->GetCancelCurrentCommand());

        list<string> responseStrings;
        if (SUCCEEDED(m_pBTASerialDevice->ReceiveData(responseStrings, 100, expectedResponseStrings)))
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
        m_pBTASerialDevice->PetBTAdapterWatchdog();
    }

    RETURN_EC_IF_FALSE(ERROR_FAILED, success == TS_TRUE);
    return STATUS_SUCCESS;
}

ERROR_CODE_T CBTAPairingManager::PairToConnectedDevice()
{
    bool connectedToDevice = false;
    list<shared_ptr<CBTEADetectedDevice> >::iterator it;
    for (it = m_detectedDeviceList.begin(); it != m_detectedDeviceList.end(); it++)
    {
        if (m_PairedBtDeviceAddress == (*it)->m_btAddress)
        {
            DebugPrintf(DEBUG_TRACE_MESSAGE, DEBUG_NO_LOGGING, m_LogId.c_str(), "Connecting to %s...",
                        (*it)->m_btDeviceName.c_str());

            string linkId = "";

            if (SUCCEEDED(OpenConnection((*it)->m_btAddress, linkId)))
            {
                SetCurrentConnectedDevice((*it)->m_btAddress, linkId);
                DebugPrintf(DEBUG_TRACE_MESSAGE, DEBUG_NO_LOGGING, m_LogId.c_str(), "Connected to %s...", (*it)->m_btDeviceName.c_str());
                connectedToDevice = true;
            }
            break;
        }
    }

    return connectedToDevice ? STATUS_SUCCESS : ERROR_FAILED;
}

ERROR_CODE_T CBTAPairingManager::CloseConnection(string linkId)
{
    CSimpleLock myLock(&m_CS);

    m_pBTASerialDevice->FlushRxBuffer();
    RETURN_IF_FAILED(m_pBTASerialDevice->WriteData("CLOSE " + linkId, true));
    bool success = false;

    CTimeDeltaSec scanTimer(10);
    while (!scanTimer.IsTimeExpired() && !success)
    {
        PetWatchdog();
        RETURN_EC_IF_TRUE(ERROR_FAILED, m_pBTASerialDevice->GetCancelCurrentCommand());

        list<string> responseStrings;
        if (SUCCEEDED(m_pBTASerialDevice->ReceiveData(responseStrings, 100)))
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
        m_pBTASerialDevice->PetBTAdapterWatchdog();
    }

    RETURN_EC_IF_FALSE(ERROR_FAILED, success);
    return STATUS_SUCCESS;
}

void CBTAPairingManager::ClearPairedDeviceFoundFlags(void)
{
    list<shared_ptr<CBTEAPairedDevice> >::iterator it;
    for (it = m_pairedDeviceInfo.begin(); it != m_pairedDeviceInfo.end(); it++)
    {
        (*it)->m_found = false;
    }
}

/********************************************************************************************************
********************************************************************************************************/
ERROR_CODE_T CBTAPairingManager::UpdatePairedDeviceList(void)
{
    vector<string> retStrings;
    RETURN_IF_FAILED(m_pBTASerialDevice->ReadData(retStrings, "LIST"));

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
            else if (m_DeviceMode != BTA_DEVICE_MODE_INPUT)
            {
                string deviceName;

                if (SUCCEEDED(GetConnectedDeviceName(pairStrings[1], deviceName)))
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

    PairedDeviceListUpdated.notifyObservers();

    return STATUS_SUCCESS;
}

ERROR_CODE_T CBTAPairingManager::PerformBackgroundDeviceNameRetrieval(void)
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

            if (SUCCEEDED(GetConnectedDeviceName(pairedDevice->m_btAddress, deviceName)))
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

// These enumeration values are defined in the Bluetooth specification.
typedef enum
{
    MAJOR_SERVICE_LIMITED_DISCOVERABLE_MODE = 0x00000001,
    MAJOR_SERVICE_RESERVED_1 = 0x00000002,
    MAJOR_SERVICE_RESERVED_2 = 0x00000004,
    MAJOR_SERVICE_POSITIONING = 0x00000008,
    MAJOR_SERVICE_NETWORKING = 0x00000010,
    MAJOR_SERVICE_RENDERING = 0x00000020,
    MAJOR_SERVICE_CAPTURING = 0x00000040,
    MAJOR_SERVICE_OBJECT_TRANSFER = 0x00000080,
    MAJOR_SERVICE_AUDIO = 0x00000100,
    MAJOR_SERVICE_TELEPHONY = 0x00000200,
    MAJOR_SERVICE_INFORMATION = 0x00000400,
} MAJOR_SERVICE_CLASS;

typedef enum
{
    MAJOR_DEVICE_MISCELLANEOUS = 0x00000000,
    MAJOR_DEVICE_COMPUTER = 0x00000001,
    MAJOR_DEVICE_PHONE = 0x00000002,
    MAJOR_DEVICE_LAN = 0x00000003,
    MAJOR_DEVICE_AUDIO_VIDEO = 0x00000004,
    MAJOR_DEVICE_PERIPHERAL = 0x00000005,
    MAJOR_DEVICE_IMAGING = 0x00000006,
    MAJOR_DEVICE_WEARABLE = 0x00000007,
    MAJOR_DEVICE_TOY = 0x00000008,
    MAJOR_DEVICE_HEALTH = 0x00000009,
    MAJOR_DEVICE_UNCATEGORIZED = 0x0000001F,
} MAJOR_DEVICE_CLASS;

typedef enum
{
    AV_MINOR_DEVICE_UNCATEGORIZED = 0x00,
    AV_MINOR_DEVICE_WEARABLE_HEADSET = 0x01,
    AV_MINOR_DEVICE_WEARABLE_HANDS_FREE = 0x02,
    AV_MINOR_DEVICE_RESERVED_1 = 0x03,
    AV_MINOR_DEVICE_MICROPHONE = 0x04,
    AV_MINOR_DEVICE_LOUDSPEAKER = 0x05,
    AV_MINOR_DEVICE_HEADPHONES = 0x06,
    AV_MINOR_DEVICE_PORTABLE_AUDIO = 0x07,
    AV_MINOR_DEVICE_CAR_AUDIO = 0x08,
    AV_MINOR_DEVICE_SET_TOP_BOX = 0x09,
    AV_MINOR_DEVICE_HIFI_AUDIO = 0x0A,
    AV_MINOR_DEVICE_VCR = 0x0B,
    AV_MINOR_DEVICE_VIDEO_CAMERA = 0x0C,
    AV_MINOR_DEVICE_CAMCORDER = 0x0D,
    AV_MINOR_DEVICE_VIDEO_MONITOR = 0x0E,
    AV_MINOR_DEVICE_VIDEO_DISPLAY_AND_LOUDSPEAKER = 0x0F,
    AV_MINOR_DEVICE_VIDEO_CONFERENCING = 0x10,
    AV_MINOR_DEVICE_RESERVED_2 = 0x11,
    AV_MINOR_DEVICE_GAMING_TOY = 0x12,
} AV_MINOR_DEVICE_CLASS;

bool CBTAPairingManager::IsValidAudioRenderingDevice(INT32U deviceCODValue)
{
    // Extract the Major Service Class from the deviceCODValue.
    // The Major Service Class is located in bits 13-23 of the CoD value.
    // Mask: 0x200000 (bits 13-23), Shift: >> 13 to align with LSB.
    INT32U majorServiceClass = ((deviceCODValue & 0x200000) >> 13);

    // Extract the Major Device Class from the deviceCODValue.
    // The Major Device Class is located in bits 8-12 of the CoD value.
    // Mask: 0x001F00 (bits 8-12), Shift: >> 8 to align with LSB.
    INT32U majorDeviceClass = ((deviceCODValue & 0x001F00) >> 8);

    // Extract the Minor Device Class from the deviceCODValue.
    // The Minor Device Class is located in bits 2-7 of the CoD value.
    // Mask: 0x0000FC (bits 2-7), Shift: >> 2 to align with LSB.
    INT32U minorDeviceClass = ((deviceCODValue & 0x0000FC) >> 2);

    // Validate the extracted classes against known audio rendering device classes.
    RETURN_BOOL_IF_FALSE(false, majorServiceClass == MAJOR_SERVICE_AUDIO);
    RETURN_BOOL_IF_FALSE(false, majorDeviceClass == MAJOR_DEVICE_AUDIO_VIDEO);
    RETURN_BOOL_IF_FALSE(false, minorDeviceClass == AV_MINOR_DEVICE_WEARABLE_HEADSET ||
                                    minorDeviceClass == AV_MINOR_DEVICE_LOUDSPEAKER ||
                                    minorDeviceClass == AV_MINOR_DEVICE_HEADPHONES ||
                                    minorDeviceClass == AV_MINOR_DEVICE_PORTABLE_AUDIO);

    return true;
}
