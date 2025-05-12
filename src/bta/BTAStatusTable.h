#pragma once

#include <string>

#include "types.h"

using namespace std;

class IBTAdapterStatusTable {
public:
    virtual BOOLEAN TableChanged( bool clearChangeFlag=true ) = 0;
    virtual INT16U GetNumTableEntries( void ) = 0;
    virtual INT16U GetNumChangedEntries( void ) = 0;
    virtual ERROR_CODE_T SetAdapterOnline(INT8U cardNumber, bool isOnline) = 0;
    virtual ERROR_CODE_T GetAdapterOnline(INT8U cardNumber, bool &isOnlineOut) = 0;
    virtual ERROR_CODE_T SetConnectionState(INT8U cardNumber, bool isConnected, string btAddress, string btDeviceName) = 0;
    virtual ERROR_CODE_T GetConnectionState(INT8U cardNumber, bool &isConnectedOut, string &btAddressOut, string &btDeviceNameOut) = 0;
    virtual ERROR_CODE_T AddDetectedDevice(INT8U cardNumber, string btAddress, string btDeviceName) = 0;
    virtual ERROR_CODE_T RemoveDetectedDevice(INT8U cardNumber, string btAddress) = 0;
    virtual ERROR_CODE_T ClearDetectedDeviceList(INT8U cardNumber) = 0;
    virtual ERROR_CODE_T GetNextDetectedDevice(INT8U cardNumber, string previousBtAddress, string &btAddressOut, string &btDeviceNameOut) = 0;
    virtual bool IsIABluetoothAdapter(string btAddress) = 0;
    virtual bool IsPairedToIABluetoothAdapter(string btAddress) = 0;
};