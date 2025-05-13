#pragma once

#include "types.h"

class CTimeDeltaSec
{
public:
    CTimeDeltaSec( INT16U deltaTimeSec = 0 );
    virtual void ResetTime( INT16U deltaTimeSec );
    virtual INT16U GetElapsedTime();
    virtual BOOLEAN IsTimeExpired();

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
    virtual void ResetTime( INT16U deltaTimeMsecs );
    virtual void ResetTimeOSTicks( INT16U deltaTimeOSTicks );
    virtual INT16U GetElapsedTime();
    virtual INT16U GetElapsedTimeOSTicks();
    virtual void WaitTimeElapsed();
    virtual BOOLEAN IsTimeExpired();

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
    virtual void ResetTime( INT16U deltaTimeUs );
    virtual void ResetTimeOSTicks( INT16U deltaTimeOSTicks );
    virtual INT16U GetElapsedTime();
    virtual INT16U GetElapsedTimeOSTicks();
    virtual BOOLEAN IsTimeExpired();

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