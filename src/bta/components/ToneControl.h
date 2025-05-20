#pragma once
/****************************************************************************************************
class for defining tone signals.
****************************************************************************************************/

#include <string>

#include "BTASerialDevice.h"

#include "TimeDelta.h"
#include "types.h"

class CToneControl
{
  public:
    CToneControl(void);

    ERROR_CODE_T PlayNextMusicSequence(weak_ptr<BTASerialDevice> serialDevice);
    void SelectNextSong(void);
    ERROR_CODE_T SetSongIndex(INT8U songIndex);
    ERROR_CODE_T GetSongIndex(INT8U &songIndexOut);

  private:
    ERROR_CODE_T SendNextSequenceCommand(weak_ptr<BTASerialDevice> serialDevice);
    void SelectNextSequence(void);

    typedef struct
    {
        string Command;
        INT32U DelayTime;
    } TONE_SEGMENT_T;

    typedef struct
    {
        INT32U size;
        const TONE_SEGMENT_T *pSegments;
        string name;
    } SONG_T;

    static const TONE_SEGMENT_T s_SampleToneFromDatasheet[];
    static const TONE_SEGMENT_T s_DalmatianSong[];
    static const TONE_SEGMENT_T s_TorgoThemeSong[];
    static const TONE_SEGMENT_T s_BatmanTheme[];

    static const SONG_T s_SongList[];

    INT32U m_CurToneIdx;
    INT32U m_CurSongIdx;
    CTimeDelta m_TestToneTimer;
};
