#pragma once

#include "types.h"

#include <pthread.h>
#include <string>
#include <atomic>
#include <thread>
#include <chrono>
#include <cstring>
#include <iostream>

class ICriticalSection
{
public:
    ICriticalSection(CHAR8* pCsName = nullptr, pthread_mutex_t* pMutex = nullptr)
        : m_pMutex(pMutex ? pMutex : new pthread_mutex_t),
          m_ownsMutex(!pMutex),
          m_CurrentProcessLockCount(0),
          m_TaskLogging(false)
    {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(m_pMutex, &attr);
        pthread_mutexattr_destroy(&attr);

        if (pCsName)
            m_CsName = pCsName;
    }

    ~ICriticalSection()
    {
        if (m_ownsMutex)
        {
            pthread_mutex_destroy(m_pMutex);
            delete m_pMutex;
        }
    }

    virtual INT8U Lock(INT16U timeout)
    {
        return Lock(timeout, false, nullptr);
    }

    virtual INT8U Lock(INT16U timeout, BOOLEAN logTaskInfo, CHAR8* pIDString)
    {
        if (timeout == 0)
        {
            if (pthread_mutex_trylock(m_pMutex) != 0)
                return OS_ERR_TIMEOUT;
        }
        else
        {
            auto start = std::chrono::steady_clock::now();
            while (pthread_mutex_trylock(m_pMutex) != 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                if (std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - start)
                        .count() > timeout)
                {
                    return OS_ERR_TIMEOUT;
                }
            }
        }

        m_CurrentThread = pthread_self();
        ++m_CurrentProcessLockCount;

        if (m_TaskLogging || logTaskInfo)
        {
            std::cout << "[Lock] " << (pIDString ? pIDString : "Unknown") << " acquired " << m_CsName << "\n";
        }

        return OS_ERR_NONE;
    }

    virtual INT8U Unlock(void)
    {
        return Unlock(false, nullptr);
    }

    virtual INT8U Unlock(BOOLEAN logTaskInfo, CHAR8* pIDString)
    {
        if (pthread_equal(m_CurrentThread, pthread_self()) == 0)
            return OS_ERR_FAILED;

        --m_CurrentProcessLockCount;
        pthread_mutex_unlock(m_pMutex);

        if (m_TaskLogging || logTaskInfo)
        {
            std::cout << "[Unlock] " << (pIDString ? pIDString : "Unknown") << " released " << m_CsName << "\n";
        }

        return OS_ERR_NONE;
    }

    virtual void SetCSName(CHAR8* pCsName)
    {
        m_CsName = pCsName ? pCsName : "";
    }

    virtual void SetTaskLogging(BOOLEAN enabled)
    {
        m_TaskLogging = enabled;
    }

    virtual INT32U GetLockCount(void)
    {
        return m_CurrentProcessLockCount;
    }

private:
    pthread_mutex_t* m_pMutex;
    pthread_t m_CurrentThread;
    INT32U m_CurrentProcessLockCount;
    bool m_ownsMutex;
    BOOLEAN m_TaskLogging;
    std::string m_CsName;
};


class CSimpleLock
{
public:
    CSimpleLock( ICriticalSection *pCriticalSection )
    {
        // JEB: we need this initialized even if to NULL.
        m_pCriticalSection = pCriticalSection;
        m_LogTaskInfo = FALSE;
        m_pIDString = NULL;
        m_IsLocked = FALSE;

        if( pCriticalSection != NULL )
        {
            m_pCriticalSection->Lock(0);
            m_IsLocked = TRUE;
        }
    }

    CSimpleLock( ICriticalSection *pCriticalSection, INT16U timeout )
    {
        // JEB: we need this initialized even if to NULL.
        m_pCriticalSection = pCriticalSection;
        m_LogTaskInfo = FALSE;
        m_pIDString = NULL;
        m_IsLocked = FALSE;

        if( pCriticalSection != NULL )
        {
            INT8U err = m_pCriticalSection->Lock(timeout);
            if( err == OS_NO_ERR )
                m_IsLocked = TRUE;
        }
    }

    CSimpleLock( ICriticalSection *pCriticalSection, INT16U timeout, BOOLEAN LogTaskInfo, CHAR8 *pIDString  )
    {
        m_pCriticalSection = pCriticalSection;
        m_LogTaskInfo = LogTaskInfo;
        m_pIDString = pIDString;
        m_IsLocked = FALSE;

        if( pCriticalSection != NULL )
        {
            INT8U err = m_pCriticalSection->Lock(timeout, LogTaskInfo, pIDString);
            if( err == OS_NO_ERR )
                m_IsLocked = TRUE;
        }
    }

    CSimpleLock( ICriticalSection *pCriticalSection, BOOLEAN LogTaskInfo, CHAR8 *pIDString )
    {
        m_pCriticalSection = pCriticalSection;
        m_LogTaskInfo = LogTaskInfo;
        m_pIDString = pIDString;
        m_IsLocked = FALSE;

        if( pCriticalSection != NULL )
        {
            m_pCriticalSection->Lock(0, LogTaskInfo, pIDString);
            m_IsLocked = TRUE;
        }
    }

    ~CSimpleLock(void)
    {
        if( m_pCriticalSection != NULL )
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