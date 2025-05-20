create_gtest(
    TESTNAME IDC777
    TESTPATH src/bta/devices
    EXTRA_SOURCES
        bta/IBTADeviceDriver.cpp
    INCLUDE_FOLDERS
        src/bta/devices
        src/platform/linux_x86
        src/platform/linux_x86/interfaces
        test/mocks/bta
)
