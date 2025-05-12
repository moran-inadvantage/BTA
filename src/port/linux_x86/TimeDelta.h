#pragma once

#include "types.h"

class CTimeDeltaSec
{
public:
    CTimeDeltaSec( INT16U deltaTimeSec = 0 );
    void ResetTime( INT16U deltaTimeSec );
    INT16U GetElapsedTime();
    BOOLEAN IsTimeExpired();

private:
    // State data
    INT32U m_startTick;
    INT32U m_deltaTicks;
    BOOLEAN m_timeoutExpired;
};


class CTimeDelta
{
public:

    CTimeDelta( INT16U deltaTimeMsecs = 0 );
    void ResetTime( INT16U deltaTimeMsecs );
    void ResetTimeOSTicks( INT16U deltaTimeOSTicks );
    INT16U GetElapsedTime();
    INT16U GetElapsedTimeOSTicks();
    void WaitTimeElapsed();
    BOOLEAN IsTimeExpired();

private:

    // State data
    INT32U m_startTick;
    INT32U m_deltaTicks;
    BOOLEAN m_timeoutExpired;
    static INT32U m_ticksPerMsec;
    static INT32U m_ticksPerOSTicks;
};

class CTimeDeltaUs
{
public:
    CTimeDeltaUs( INT16U deltaTimeUs = 0 );
    static void GetTickCountUs( INT32U *pSeconds, INT32U *pUSeconds );
    static void GetTickCountUsAdjusted( INT32U *pSeconds, INT32U *pUSeconds );
    void ResetTime( INT16U deltaTimeUs );
    void ResetTimeOSTicks( INT16U deltaTimeOSTicks );
    INT16U GetElapsedTime();
    INT16U GetElapsedTimeOSTicks();
    BOOLEAN IsTimeExpired();

private:
    // State data
    INT32U m_startTick;
    INT32U m_deltaTicks;
    BOOLEAN m_timeoutExpired;
    static INT32U m_ticksPerUs;
    static INT32U m_ticksPerOSTicks;
    static INT32U m_Frequency;
};

// Sleep in ticks
void OSTimeDly(INT32U);