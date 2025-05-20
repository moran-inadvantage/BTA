#pragma once

/****************************************************************************************************
    File Name:  ToneControl.h

    Notes:      This class defines and controls playback of tone signals.
                It manages a list of songs and sequences, and sends commands
                to a Bluetooth serial device to play them.
****************************************************************************************************/

//--------------------------------------------------------------------------------------------------
// Includes
//--------------------------------------------------------------------------------------------------
#include "BTASerialDevice.h"
#include "TimeDelta.h"
#include "types.h"
#include <string>

//--------------------------------------------------------------------------------------------------
// Tone Control Class
//--------------------------------------------------------------------------------------------------
class CToneControl
{
  public:
    CToneControl(void);

    // Plays the next tone segment in the current song using the BT serial device
    ERROR_CODE_T PlayNextMusicSequence(weak_ptr<CBTASerialDevice> serialDevice);

    // Advances to the next song in the list
    void SelectNextSong(void);

    // Sets or retrieves the index of the currently selected song
    ERROR_CODE_T SetSongIndex(INT8U songIndex);
    ERROR_CODE_T GetSongIndex(INT8U &songIndexOut);

  private:
    // Sends the current sequence command to the BT device
    ERROR_CODE_T SendNextSequenceCommand(weak_ptr<CBTASerialDevice> serialDevice);

    // Advances to the next tone segment in the current song
    void SelectNextSequence(void);

    //--------------------------------------------------------------------------------------------------
    // Song and Tone Segment Structures
    //--------------------------------------------------------------------------------------------------
    typedef struct
    {
        // Command string to send
        string Command;
        // Delay after sending the command (ms)
        INT32U DelayTime;
    } TONE_SEGMENT_T;

    typedef struct
    {
        // Number of segments in the song
        INT32U size;
        // Pointer to array of segments
        const TONE_SEGMENT_T *pSegments;
        // Human-readable song name
        string name;
    } SONG_T;

    //--------------------------------------------------------------------------------------------------
    // Static Songs and Sequences
    //--------------------------------------------------------------------------------------------------
    static const TONE_SEGMENT_T s_SampleToneFromDatasheet[];
    static const TONE_SEGMENT_T s_DalmatianSong[];
    static const TONE_SEGMENT_T s_TorgoThemeSong[];
    static const TONE_SEGMENT_T s_BatmanTheme[];

    static const SONG_T s_SongList[];

    //--------------------------------------------------------------------------------------------------
    // Member Variables
    //--------------------------------------------------------------------------------------------------
    INT32U m_CurToneIdx;        // Current tone index within the song
    INT32U m_CurSongIdx;        // Index of the current song
    CTimeDelta m_TestToneTimer; // Timer to manage delays between tones
};
