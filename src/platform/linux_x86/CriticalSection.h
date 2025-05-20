#pragma once

#include "types.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <pthread.h>
#include <string>
#include <thread>
class ICriticalSection
{
  public:
    virtual ~ICriticalSection()
    {
    }
    virtual INT8U Lock(INT16U timeout) = 0;
    virtual INT8U Lock(INT16U timeout, BOOLEAN logTaskInfo, CHAR8 *pIDString) = 0;
    virtual INT8U Unlock(void) = 0;
    virtual INT8U Unlock(BOOLEAN logTaskInfo, CHAR8 *pIDString) = 0;
    virtual void SetCSName(CHAR8 *pCsName) = 0;
    virtual void SetTaskLogging(BOOLEAN enabled) = 0;
    virtual INT32U GetLockCount(void) = 0;
};

class CCriticalSection : public ICriticalSection
{
  public:
    CCriticalSection(CHAR8 *pCsName = NULL, void *pMutex = NULL);
    virtual ~CCriticalSection();

    virtual INT8U Lock(INT16U timeout);
    virtual INT8U Lock(INT16U timeout, BOOLEAN logTaskInfo, CHAR8 *pIDString);
    virtual INT8U Unlock(void);
    virtual INT8U Unlock(BOOLEAN logTaskInfo, CHAR8 *pIDString);
    virtual void SetCSName(CHAR8 *pCsName);
    virtual void SetTaskLogging(BOOLEAN enabled);
    virtual INT32U GetLockCount(void);

  private:
    pthread_mutex_t m_mutex;
    pthread_t m_ownerThread;
    INT32U m_lockCount;
    BOOLEAN m_taskLogging;
    CHAR8 m_csName[64];
};

class CSimpleLock
{
  public:
    CSimpleLock(ICriticalSection *pCriticalSection)
    {
        // JEB: we need this initialized even if to NULL.
        m_pCriticalSection = pCriticalSection;
        m_LogTaskInfo = FALSE;
        m_pIDString = NULL;
        m_IsLocked = FALSE;

        if (pCriticalSection != NULL)
        {
            m_pCriticalSection->Lock(0);
            m_IsLocked = TRUE;
        }
    }

    CSimpleLock(ICriticalSection *pCriticalSection, INT16U timeout)
    {
        // JEB: we need this initialized even if to NULL.
        m_pCriticalSection = pCriticalSection;
        m_LogTaskInfo = FALSE;
        m_pIDString = NULL;
        m_IsLocked = FALSE;

        if (pCriticalSection != NULL)
        {
            INT8U err = m_pCriticalSection->Lock(timeout);
            if (err == OS_NO_ERR)
                m_IsLocked = TRUE;
        }
    }

    CSimpleLock(ICriticalSection *pCriticalSection, INT16U timeout,
                BOOLEAN LogTaskInfo, CHAR8 *pIDString)
    {
        m_pCriticalSection = pCriticalSection;
        m_LogTaskInfo = LogTaskInfo;
        m_pIDString = pIDString;
        m_IsLocked = FALSE;

        if (pCriticalSection != NULL)
        {
            INT8U err = m_pCriticalSection->Lock(timeout, LogTaskInfo, pIDString);
            if (err == OS_NO_ERR)
                m_IsLocked = TRUE;
        }
    }

    CSimpleLock(ICriticalSection *pCriticalSection, BOOLEAN LogTaskInfo,
                CHAR8 *pIDString)
    {
        m_pCriticalSection = pCriticalSection;
        m_LogTaskInfo = LogTaskInfo;
        m_pIDString = pIDString;
        m_IsLocked = FALSE;

        if (pCriticalSection != NULL)
        {
            m_pCriticalSection->Lock(0, LogTaskInfo, pIDString);
            m_IsLocked = TRUE;
        }
    }

    ~CSimpleLock(void)
    {
        if (m_pCriticalSection != NULL)
        {
            m_pCriticalSection->Unlock(m_LogTaskInfo, m_pIDString);
        }
    }

    BOOLEAN IsLocked(void)
    {
        return m_IsLocked;
    }

  private:
    ICriticalSection *m_pCriticalSection;
    BOOLEAN m_LogTaskInfo;
    CHAR8 *m_pIDString;
    BOOLEAN m_IsLocked;
};