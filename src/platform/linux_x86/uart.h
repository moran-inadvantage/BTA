#include "interfaces/iuart.h"


class CuArt : public IUart {
public:
    explicit CuArt(INT32U portNumber);
    ~CuArt();

    ERROR_CODE_T Open(BAUDRATE baud, BYTE_SIZE byteSize, PARITY parity, STOP_BITS stopBits) override;
    ERROR_CODE_T Close() override;

    INT32U RxBytesAvailable() override;

    void WriteString(const CHAR8* pString) override;
    void WriteByte(INT8U byte) override;
    void WriteWord(INT16U word) override;
    void WriteDWord(INT32U dword) override;

    BOOLEAN ReadByte(INT8U* pByte) override;
    BOOLEAN ReadWord(INT16U* pWord) override;
    BOOLEAN ReadDWord(INT32U* pDWord) override;

    void WritePort(const INT8U* pBuf, INT32U bytesToWrite, INT32U* pBytesWritten) override;
    void ReadPort(INT8U* pBuf, INT32U maxBytes, INT32U* pBytesRead) override;

private:
    int m_Fd;
    INT32U m_TxPutIndex;
    INT32U m_TxGetIndex;
    INT32U m_RxPutIndex;
    INT32U m_RxGetIndex;
    INT32U m_TransmitterActive;
};
