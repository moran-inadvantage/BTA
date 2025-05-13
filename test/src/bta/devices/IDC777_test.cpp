#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "IDC777.h"
#include "BTASerialDevice.h"
#include "BTADeviceDriver.h"

using namespace std;
using ::testing::Return;
using ::testing::_;
using ::testing::DoAll;
using ::testing::SetArgReferee;

#include "../../../mocks/platform/linux_x86/TimeDelta.h"

class MockBTADeviceDriver : public BTADeviceDriver {
public:
    MOCK_METHOD(ERROR_CODE_T, SetBtaSerialDevice, (std::shared_ptr<BTASerialDevice> pBTASerialDevice), (override));
    MOCK_METHOD(ERROR_CODE_T, GetDeviceVersion, (std::shared_ptr<BTAVersionInfo_t>& version), (override));
    MOCK_METHOD(ERROR_CODE_T, EnterCommandMode, (), (override));
    MOCK_METHOD(ERROR_CODE_T, SendReset, (), (override));
};

class MockBTASerialDevice : public BTASerialDevice {
public:
    MOCK_METHOD(ERROR_CODE_T, SetUArt, (std::weak_ptr<IUart> puArt), (override));
    MOCK_METHOD(ERROR_CODE_T, GetBaudrate, (BAUDRATE &baudrate), (override));
    MOCK_METHOD(ERROR_CODE_T, SetBaudrate, (BAUDRATE baudrate), (override));
    MOCK_METHOD(ERROR_CODE_T, SetCommEnable, (bool enabled), (override));
    MOCK_METHOD(ERROR_CODE_T, ReadDataSimple, (std::vector<std::string> &outStrings, const std::string &command), (override));
};

// Helper function to create a shared mock
shared_ptr<MockBTASerialDevice> CreateMockSerialDevice() {
    return make_shared<MockBTASerialDevice>();
}

// Test SetBtaSerialDevice: success case
TEST(IDC777Test, SetBtaSerialDeviceSuccess) {
    IDC777 device;
    auto mockSerial = CreateMockSerialDevice();

    EXPECT_CALL(*mockSerial, SetBaudrate(BAUDRATE_115200))
        .WillOnce(Return(STATUS_SUCCESS));

    auto result = device.SetBtaSerialDevice(mockSerial);
    EXPECT_EQ(result, STATUS_SUCCESS);
}

// Test SetBtaSerialDevice: null pointer
TEST(IDC777Test, SetBtaSerialDeviceNull) {
    IDC777 device;
    auto result = device.SetBtaSerialDevice(NULL);
    EXPECT_EQ(result, ERROR_FAILED);
}

// Test GetDeviceVersion: success case
TEST(IDC777Test, GetDeviceVersionSuccess) {
    IDC777 device;
    auto mockSerial = CreateMockSerialDevice();

    EXPECT_CALL(*mockSerial, SetBaudrate(BAUDRATE_115200))
        .WillOnce(Return(STATUS_SUCCESS));
    device.SetBtaSerialDevice(mockSerial);

    vector<string> mockOutput = {
        "IOT747 Copyright 2022",
        "AudioAgent IDC777 V3.1.21",
        "Build: 0400427b",
        "Bluetooth address 245DFC020196",
        "OK"
    };

    EXPECT_CALL(*mockSerial, ReadDataSimple(_, "VERSION"))
        .WillOnce(DoAll(SetArgReferee<0>(mockOutput), Return(STATUS_SUCCESS)));

    shared_ptr<BTAVersionInfo_t> version;
    auto result = device.GetDeviceVersion(version);

    EXPECT_EQ(result, STATUS_SUCCESS);
    ASSERT_NE(version, nullptr);
    EXPECT_EQ(version->hardware, BTA_HW_IDC777);
}

// Test GetDeviceVersion: no serial device set
TEST(IDC777Test, GetDeviceVersionNoSerialDevice) {
    IDC777 device;
    shared_ptr<BTAVersionInfo_t> version;
    auto result = device.GetDeviceVersion(version);
    EXPECT_EQ(result, ERROR_FAILED);
}

// Test GetDeviceVersion: invalid output
TEST(IDC777Test, GetDeviceVersionInvalidOutput) {
    IDC777 device;
    auto mockSerial = CreateMockSerialDevice();

    EXPECT_CALL(*mockSerial, SetBaudrate(BAUDRATE_115200))
        .WillOnce(Return(STATUS_SUCCESS));
    device.SetBtaSerialDevice(mockSerial);

    vector<string> invalidOutput = {
        "WrongHeader",
        "AudioAgent Unknown",
        "Build: x",
        "SomeAddress",
        "OK"
    };

    EXPECT_CALL(*mockSerial, ReadDataSimple(_, "VERSION"))
        .WillOnce(DoAll(SetArgReferee<0>(invalidOutput), Return(STATUS_SUCCESS)));

    shared_ptr<BTAVersionInfo_t> version;
    auto result = device.GetDeviceVersion(version);

    EXPECT_EQ(result, ERROR_FAILED);
}

