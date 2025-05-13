#include <gmock/gmock.h>

#include "types.h"

class CTimeDeltaSec {
public:
    CTimeDeltaSec(INT16U deltaTimeSec = 0) {}

    MOCK_METHOD(void, ResetTime, (INT16U deltaTimeSec));
    MOCK_METHOD(INT16U, GetElapsedTime, ());
    MOCK_METHOD(BOOLEAN, IsTimeExpired, ());
};

class CTimeDelta {
public:
    CTimeDelta(INT16U deltaTimeMsecs = 0) {}

    MOCK_METHOD(void, ResetTime, (INT16U deltaTimeMsecs));
    MOCK_METHOD(void, ResetTimeOSTicks, (INT16U deltaTimeOSTicks));
    MOCK_METHOD(INT16U, GetElapsedTime, ());
    MOCK_METHOD(INT16U, GetElapsedTimeOSTicks, ());
    MOCK_METHOD(void, WaitTimeElapsed, ());
    MOCK_METHOD(BOOLEAN, IsTimeExpired, ());
};

class CTimeDeltaUs {
public:
    CTimeDeltaUs(INT16U deltaTimeUs = 0)  {}

    MOCK_METHOD(void, ResetTime, (INT16U deltaTimeUs));
    MOCK_METHOD(void, ResetTimeOSTicks, (INT16U deltaTimeOSTicks));
    MOCK_METHOD(INT16U, GetElapsedTime, ());
    MOCK_METHOD(INT16U, GetElapsedTimeOSTicks, ());
    MOCK_METHOD(BOOLEAN, IsTimeExpired, ());
};
