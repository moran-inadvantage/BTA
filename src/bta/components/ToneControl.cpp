/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
                                                        Class CToneControl
    Handles the tone commands to play generated tunes
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#include "ToneControl.h"

const CToneControl::TONE_SEGMENT_T CToneControl::s_DalmatianSong[] =
    {
        //                 |   C6   |   D6#   |   F6   |   E6   |   C6
        {"TONE TE 180 TI 6 N C6 L 8 N DS6 L 4 N F6 L 8 N E6 L 4 N C6 L 8", OS_TICKS_PER_SEC * 2},
        //                 |   C6   |   E6   |   D6    |   B5   |  C6
        {"TONE TE 180 TI 6 N C6 L 8 N E6 L 4 N D6 L 8 N B5 L 4 N C6 L 4", (INT32U)(OS_TICKS_PER_SEC * 2.5)},
        //                 |  C6    ||       |   C6
        {"TONE TE 180 TI 6 N C6 L 8 N R0 L 8 N C6 L 8", OS_TICKS_PER_SEC * 2}};

const CToneControl::TONE_SEGMENT_T CToneControl::s_TorgoThemeSong[] =
    {
        //                 |  F6    |   E6   |   B5   |    C6
        {"TONE TE 180 TI 6 N F6 L 4 N E6 L 4 N B5 L 4 N C6 L 4", OS_TICKS_PER_SEC}};


// It seems like the commands should work with both platforms - more testing needed

// IDC777 Datasheet
const CToneControl::TONE_SEGMENT_T CToneControl::s_SampleToneFromDatasheet[] =
    {
        {"TONE TE 400 V 64 TI 0 N C5 L 8 N R0 L 32 N E5 L 8 N R0 L 32 N G5 L 8 N R0 L 32 N B5 L 4 N R0 L 1 N C6 L 2 TN C6 L 8", OS_TICKS_PER_SEC}};

// IDC777
const CToneControl::TONE_SEGMENT_T CToneControl::s_BatmanTheme[] =
    {
        //                dun    nu     nu     nu     nu     nu      BAT     MAN!
        {"TONE TE 320 TI 1 V 128 D 10 N G4 L 8 N A4 L 8 N G4 L 8 N A4 L 8 N G4 L 8 N A4 L 8 N D5 L 4 N C5 L 2", OS_TICKS_PER_SEC * 2}};

const CToneControl::SONG_T CToneControl::s_SongList[] =
    {
        {1, s_BatmanTheme, "Batman"},
        {1, s_SampleToneFromDatasheet, "Datasheet"},
        {3, s_DalmatianSong, "Dalmatian"},
        {1, s_TorgoThemeSong, "Torgo"}};

/********************************************************************************************************
                                    Constructor
********************************************************************************************************/
CToneControl::CToneControl(void)
    : m_CurToneIdx(0), m_CurSongIdx(0)
{
    m_TestToneTimer.ResetTime(1);
}

/********************************************************************************************************
                        ERROR_CODE_T PlayNextMusicSequence( BTEAComm &btcomm )
********************************************************************************************************/
ERROR_CODE_T CToneControl::PlayNextMusicSequence(weak_ptr<CBTASerialDevice> serialDevice)
{
    if (m_TestToneTimer.IsTimeExpired())
    {
        RETURN_IF_FAILED(SendNextSequenceCommand(serialDevice));
        m_TestToneTimer.ResetTime(s_SongList[m_CurSongIdx].pSegments[m_CurToneIdx].DelayTime);
    }

    return STATUS_SUCCESS;
}

/********************************************************************************************************
                        ERROR_CODE_T SendNextSequenceCommand( CBTEAComm &btcomm )
********************************************************************************************************/
ERROR_CODE_T CToneControl::SendNextSequenceCommand(weak_ptr<CBTASerialDevice> serialDevice)
{

    RETURN_IF_FAILED(serialDevice.lock()->WriteData(s_SongList[m_CurSongIdx].pSegments[m_CurToneIdx].Command));
    SelectNextSequence();

    return STATUS_SUCCESS;
}

/********************************************************************************************************
                          void SelectNextSequence( void )
********************************************************************************************************/
void CToneControl::SelectNextSequence(void)
{
    m_CurToneIdx++;
    if (m_CurToneIdx >= s_SongList[m_CurSongIdx].size)
        m_CurToneIdx = 0;
}

/********************************************************************************************************
                          void SelectNextSong( void )
********************************************************************************************************/
void CToneControl::SelectNextSong(void)
{
    m_CurSongIdx++;
    if (m_CurSongIdx >= sizeof(s_SongList) / sizeof(SONG_T))
        m_CurSongIdx = 0;
    m_CurToneIdx = 0;
}

/********************************************************************************************************
                          void SetSongIndex( INT8U songIndex )
********************************************************************************************************/
ERROR_CODE_T CToneControl::SetSongIndex(INT8U songIndex)
{
    RETURN_EC_IF_FALSE(ERROR_INVALID_PARAMETER, songIndex < sizeof(s_SongList) / sizeof(SONG_T));
    m_CurSongIdx = songIndex;
    m_CurToneIdx = 0;

    return STATUS_SUCCESS;
}

/********************************************************************************************************
                          void SetSongIndex( INT8U songIndex )
********************************************************************************************************/
ERROR_CODE_T CToneControl::GetSongIndex(INT8U &songIndexOut)
{
    songIndexOut = m_CurSongIdx;
    return STATUS_SUCCESS;
}
