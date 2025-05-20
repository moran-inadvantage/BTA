#pragma once

#include "types.h"

// Enumeration for baud rates
enum BAUDRATE {
  BAUDRATE_9600,
  BAUDRATE_19200,
  BAUDRATE_38400,
  BAUDRATE_57600,
  BAUDRATE_115200,
  BAUDRATE_230400,
  BAUDRATE_460800,
  BAUDRATE_921600,
  BAUDRATE_UNKNOWN,
};

// Enumeration for byte sizes
enum BYTE_SIZE { BYTE_SZ_5, BYTE_SZ_6, BYTE_SZ_7, BYTE_SZ_8 };

// Enumeration for parity settings
enum PARITY { NO_PARITY, PARITY_EVEN, PARITY_ODD };

// Enumeration for stop bits
enum STOP_BITS { STOP_BITS_1, STOP_BITS_2 };

class IUart {
public:
  IUart(INT32U PortNumber) : m_Port(PortNumber){};
  virtual ~IUart(void){};
  virtual ERROR_CODE_T Open(BAUDRATE Baud, BYTE_SIZE ByteSize, PARITY Parity,
                            STOP_BITS StopBits) = 0;
  virtual ERROR_CODE_T Close(void) = 0;
  virtual INT32U RxBytesAvailable(void) = 0;
  virtual void WriteString(const CHAR8 *pString) = 0;
  virtual void WriteByte(INT8U Byte) = 0;
  virtual void WriteWord(INT16U Word) = 0;
  virtual void WriteDWord(INT32U DWord) = 0;
  virtual BOOLEAN ReadByte(INT8U *pByte) = 0;
  virtual BOOLEAN ReadWord(INT16U *pWord) = 0;
  virtual BOOLEAN ReadDWord(INT32U *pDWord) = 0;
  virtual void WritePort(const INT8U *pBuf, INT32U BytesToWrite,
                         INT32U *pBytesWritten) = 0;
  virtual void ReadPort(INT8U *pBuf, INT32U MaxBytes, INT32U *pBytesRead) = 0;

protected:
  INT32U m_Port;
};
