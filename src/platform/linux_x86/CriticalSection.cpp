#include "CriticalSection.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>

CCriticalSection::CCriticalSection(CHAR8 *pCsName, void *pMutex)
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&m_mutex, &attr);
    pthread_mutexattr_destroy(&attr);

    m_ownerThread = 0;
    m_lockCount = 0;
    m_taskLogging = FALSE;
    if (pCsName)
        strncpy(m_csName, pCsName, sizeof(m_csName) - 1);
    else
        m_csName[0] = '\0';
}

CCriticalSection::~CCriticalSection()
{
    pthread_mutex_destroy(&m_mutex);
}

INT8U CCriticalSection::Lock(INT16U timeout)
{
    return Lock(timeout, FALSE, NULL);
}

INT8U CCriticalSection::Lock(INT16U timeout, BOOLEAN logTaskInfo, CHAR8 *pIDString)
{
    int result = pthread_mutex_lock(&m_mutex); // timeout not handled
    if (result == 0)
    {
        m_ownerThread = pthread_self();
        m_lockCount++;

        if (m_taskLogging && logTaskInfo && pIDString)
        {
            printf("Locked by [%s] on CS [%s]\n", pIDString, m_csName);
        }

        return 0;
    }
    return 1;
}

INT8U CCriticalSection::Unlock(void)
{
    return Unlock(FALSE, NULL);
}

INT8U CCriticalSection::Unlock(BOOLEAN logTaskInfo, CHAR8 *pIDString)
{
    if (!pthread_equal(m_ownerThread, pthread_self()))
    {
        return 1;
    }

    m_lockCount--;
    if (m_lockCount == 0)
    {
        m_ownerThread = 0;
        pthread_mutex_unlock(&m_mutex);
    }

    if (m_taskLogging && logTaskInfo && pIDString)
    {
        printf("Unlocked by [%s] on CS [%s]\n", pIDString, m_csName);
    }

    return 0;
}

void CCriticalSection::SetCSName(CHAR8 *pCsName)
{
    if (pCsName)
    {
        strncpy(m_csName, pCsName, sizeof(m_csName) - 1);
        m_csName[sizeof(m_csName) - 1] = '\0';
    }
}

void CCriticalSection::SetTaskLogging(BOOLEAN enabled)
{
    m_taskLogging = enabled;
}

INT32U CCriticalSection::GetLockCount(void)
{
    return m_lockCount;
}
