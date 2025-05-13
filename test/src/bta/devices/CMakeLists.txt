create_gtest(
    TESTNAME IDC777
    EXTRA_SOURCES
        bta/BTADeviceDriver.cpp
        bta/BTASerialDevice.cpp
    MOCK_HEADERS
        platform/linux_x86/TimeDelta.h
)
