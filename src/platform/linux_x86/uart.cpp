#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <errno.h>
#include <sys/ioctl.h>
#include <sstream>
#include <string>

#include "uart.h"

CuArt::CuArt(INT32U PortNumber) :
    IUart(PortNumber),
    m_TxPutIndex(0),
    m_TxGetIndex(0),
    m_RxPutIndex(0),
    m_RxGetIndex(0),
    m_TransmitterActive(0),
    m_Fd(-1)  // File descriptor for the UART port
{
}

CuArt::~CuArt()
{
    Close();
}

ERROR_CODE_T CuArt::Open(BAUDRATE Baud, BYTE_SIZE ByteSize, PARITY Parity, STOP_BITS StopBits)
{
    std::ostringstream devPath;
    devPath << "/dev/ttyUSB" << m_Port;

    m_Fd = open(devPath.str().c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    if (m_Fd < 0) {
        printf("Failed to open UART port %s: %s\n", devPath.str().c_str(), strerror(errno));
        return ERROR_FAILED;
    }

    fcntl(m_Fd, F_SETFL, 0); // Set to blocking

    struct termios options;
    tcgetattr(m_Fd, &options);

    // Set baud rate
    speed_t baud;
    switch (Baud) {
        case BAUDRATE_9600: baud = B9600; break;
        case BAUDRATE_19200: baud = B19200; break;
        case BAUDRATE_115200: baud = B115200; break;
        default: baud = B9600; break;
    }
    cfsetispeed(&options, baud);
    cfsetospeed(&options, baud);

    // Configure byte size
    options.c_cflag &= ~CSIZE;
    switch (ByteSize) {
        case BYTE_SZ_5: options.c_cflag |= CS5; break;
        case BYTE_SZ_6: options.c_cflag |= CS6; break;
        case BYTE_SZ_7: options.c_cflag |= CS7; break;
        case BYTE_SZ_8:
        default: options.c_cflag |= CS8; break;
    }

    // Configure parity
    if (Parity == NO_PARITY)
        options.c_cflag &= ~PARENB;
    else {
        options.c_cflag |= PARENB;
        if (Parity == PARITY_ODD)
            options.c_cflag |= PARODD;
        else
            options.c_cflag &= ~PARODD;
    }

    // Stop bits
    if (StopBits == STOP_BITS_2)
        options.c_cflag |= CSTOPB;
    else
        options.c_cflag &= ~CSTOPB;

    // Raw input/output mode
    options.c_lflag = 0;
    options.c_oflag = 0;
    options.c_iflag = 0;

    // Minimum number of characters for non-canonical read
    options.c_cc[VMIN] = 0;
    // Timeout in deciseconds (e.g., 10 = 1 second)
    options.c_cc[VTIME] = 1;

    // Apply settings
    tcsetattr(m_Fd, TCSANOW, &options);
    return STATUS_SUCCESS;
}

ERROR_CODE_T CuArt::Close()
{
    if (m_Fd >= 0) {
        close(m_Fd);
        m_Fd = -1;
    }
    return STATUS_SUCCESS;
}

INT32U CuArt::RxBytesAvailable()
{
    int bytes;
    if (ioctl(m_Fd, FIONREAD, &bytes) == -1)
        return 0;
    return static_cast<INT32U>(bytes);
}

void CuArt::WriteString(const CHAR8 *pString)
{
    INT32U written;
    WritePort(reinterpret_cast<const INT8U*>(pString), strlen(pString), &written);
}

void CuArt::WriteByte(INT8U Byte)
{
    INT32U written;
    WritePort(&Byte, 1, &written);
}

void CuArt::WriteWord(INT16U Word)
{
    INT8U buf[2] = { static_cast<INT8U>(Word & 0xFF), static_cast<INT8U>((Word >> 8) & 0xFF) };
    INT32U written;
    WritePort(buf, 2, &written);
}

void CuArt::WriteDWord(INT32U DWord)
{
    INT8U buf[4] = {
        static_cast<INT8U>(DWord & 0xFF),
        static_cast<INT8U>((DWord >> 8) & 0xFF),
        static_cast<INT8U>((DWord >> 16) & 0xFF),
        static_cast<INT8U>((DWord >> 24) & 0xFF)
    };
    INT32U written;
    WritePort(buf, 4, &written);
}

BOOLEAN CuArt::ReadByte(INT8U *pByte)
{
    INT32U read;
    ReadPort(pByte, 1, &read);
    return read == 1;
}

BOOLEAN CuArt::ReadWord(INT16U *pWord)
{
    INT8U buf[2];
    INT32U read;
    ReadPort(buf, 2, &read);
    if (read == 2) {
        *pWord = buf[0] | (buf[1] << 8);
        return true;
    }
    return false;
}

BOOLEAN CuArt::ReadDWord(INT32U *pDWord)
{
    INT8U buf[4];
    INT32U read;
    ReadPort(buf, 4, &read);
    if (read == 4) {
        *pDWord = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
        return true;
    }
    return false;
}

void CuArt::WritePort(const INT8U *pBuf, INT32U BytesToWrite, INT32U *pBytesWritten)
{
    ssize_t result = write(m_Fd, pBuf, BytesToWrite);
    if (pBytesWritten != NULL)
    {
        *pBytesWritten = (result < 0) ? 0 : static_cast<INT32U>(result);
    }
}

void CuArt::ReadPort(INT8U *pBuf, INT32U MaxBytes, INT32U *pBytesRead)
{
    ssize_t result = read(m_Fd, pBuf, MaxBytes);
    if (pBytesRead != NULL)
    {
        *pBytesRead = (result < 0) ? 0 : static_cast<INT32U>(result);
    }
}
