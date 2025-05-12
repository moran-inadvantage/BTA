#include <time.h>
#include <sys/time.h>
#include "types.h"

#include "TimeDelta.h"
#include <unistd.h>

// Tick = 10ms → 1 tick = 10 milliseconds
#define TICK_MS 10

// Utility functions
static INT32U GetSystemTick()
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return (tv.tv_sec * 1000 + tv.tv_usec / 1000) / TICK_MS;
}

static INT32U GetSystemTickUs()
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return (tv.tv_sec * 1000000 + tv.tv_usec);
}

//
// CTimeDeltaSec
//

CTimeDeltaSec::CTimeDeltaSec(INT16U deltaTimeSec)
{
    m_startTick = 0;
    m_deltaTicks = 0;
    m_timeoutExpired = false;

    if (deltaTimeSec > 0)
    {
        ResetTime(deltaTimeSec);
    }
}

void CTimeDeltaSec::ResetTime(INT16U deltaTimeSec)
{
    m_startTick = GetSystemTick();
    m_deltaTicks = deltaTimeSec * 100; // 100 ticks/sec since 1 tick = 10ms
    m_timeoutExpired = false;
}

INT16U CTimeDeltaSec::GetElapsedTime()
{
    INT32U currentTick = GetSystemTick();
    INT32U elapsedTime = currentTick - m_startTick;

    if (elapsedTime >= m_deltaTicks)
    {
        m_timeoutExpired = true;
        return (INT16U)(elapsedTime / 100); // back to seconds
    }
    else
    {
        return 0;
    }
}

BOOLEAN CTimeDeltaSec::IsTimeExpired()
{
    return m_timeoutExpired;
}

//
// CTimeDelta
//

INT32U CTimeDelta::m_ticksPerMsec = 1 / TICK_MS;
INT32U CTimeDelta::m_ticksPerOSTicks = 1;

CTimeDelta::CTimeDelta(INT16U deltaTimeMsecs)
{
    m_startTick = 0;
    m_deltaTicks = 0;
    m_timeoutExpired = false;

    if (deltaTimeMsecs > 0)
    {
        ResetTime(deltaTimeMsecs);
    }
}

void CTimeDelta::ResetTime(INT16U deltaTimeMsecs)
{
    m_startTick = GetSystemTick();
    m_deltaTicks = deltaTimeMsecs / TICK_MS;
    m_timeoutExpired = false;
}

void CTimeDelta::ResetTimeOSTicks(INT16U deltaTimeOSTicks)
{
    m_startTick = GetSystemTick();
    m_deltaTicks = deltaTimeOSTicks;
    m_timeoutExpired = false;
}

INT16U CTimeDelta::GetElapsedTime()
{
    INT32U currentTick = GetSystemTick();
    INT32U elapsedTicks = currentTick - m_startTick;

    if (elapsedTicks >= m_deltaTicks)
    {
        m_timeoutExpired = true;
        return elapsedTicks * TICK_MS;
    }
    else
    {
        return 0;
    }
}

INT16U CTimeDelta::GetElapsedTimeOSTicks()
{
    return (INT16U)(GetSystemTick() - m_startTick);
}

void CTimeDelta::WaitTimeElapsed()
{
    while (!IsTimeExpired())
    {
        // optionally sleep here for efficiency
    }
}

BOOLEAN CTimeDelta::IsTimeExpired()
{
    if (m_timeoutExpired)
        return true;

    if ((GetSystemTick() - m_startTick) >= m_deltaTicks)
    {
        m_timeoutExpired = true;
    }

    return m_timeoutExpired;
}

//
// CTimeDeltaUs
//

INT32U CTimeDeltaUs::m_ticksPerUs = 1;
INT32U CTimeDeltaUs::m_ticksPerOSTicks = 1;
INT32U CTimeDeltaUs::m_Frequency = 1000000;

CTimeDeltaUs::CTimeDeltaUs(INT16U deltaTimeUs)
{
    m_startTick = 0;
    m_deltaTicks = 0;
    m_timeoutExpired = false;

    if (deltaTimeUs > 0)
    {
        ResetTime(deltaTimeUs);
    }
}

void CTimeDeltaUs::GetTickCountUs(INT32U* pSeconds, INT32U* pUSeconds)
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    if (pSeconds) *pSeconds = tv.tv_sec;
    if (pUSeconds) *pUSeconds = tv.tv_usec;
}

void CTimeDeltaUs::GetTickCountUsAdjusted(INT32U* pSeconds, INT32U* pUSeconds)
{
    GetTickCountUs(pSeconds, pUSeconds); // No adjustment in this example
}

void CTimeDeltaUs::ResetTime(INT16U deltaTimeUs)
{
    m_startTick = GetSystemTickUs();
    m_deltaTicks = deltaTimeUs;
    m_timeoutExpired = false;
}

void CTimeDeltaUs::ResetTimeOSTicks(INT16U deltaTimeOSTicks)
{
    m_startTick = GetSystemTickUs();
    m_deltaTicks = deltaTimeOSTicks;
    m_timeoutExpired = false;
}

INT16U CTimeDeltaUs::GetElapsedTime()
{
    INT32U currentTick = GetSystemTickUs();
    INT32U elapsed = currentTick - m_startTick;

    if (elapsed >= m_deltaTicks)
    {
        m_timeoutExpired = true;
        return elapsed;
    }
    else
    {
        return 0;
    }
}

INT16U CTimeDeltaUs::GetElapsedTimeOSTicks()
{
    return (INT16U)(GetSystemTickUs() - m_startTick);
}

BOOLEAN CTimeDeltaUs::IsTimeExpired()
{
    if (m_timeoutExpired)
        return true;

    if ((GetSystemTickUs() - m_startTick) >= m_deltaTicks)
    {
        m_timeoutExpired = true;
    }

    return m_timeoutExpired;
}

void OSTimeDly(uint32_t ticks)
{
    // Sleep for the specified number of ticks (10ms each)
    // This is a placeholder implementation; replace with actual sleep function
    // For example, use usleep or nanosleep in a real implementation
    usleep((ticks) * 10000);
}