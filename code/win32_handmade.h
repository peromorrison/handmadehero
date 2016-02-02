#if !defined(WIN32_HANDMADE_H)
/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Petar Kovacevic $
   $Notice: (C) Copyright 2014 by Molly Rocket, Inc. All Rights Reserved. $
   ======================================================================== */
struct win32_offscreen_buffer
{
    BITMAPINFO Info;
    void *Memory;
    int Width;
    int Height;
    int Pitch;
    int BytesPerPixel;
};

struct win32_window_dimension
{
    int Width;
    int Height;
};
struct win32_sound_output
{
//NOTE Sound test 
    int SamplesPerSecond;
    uint32 RunningSampleIndex;
    int BytesPerSample;
    DWORD SecondaryBufferSize;
    DWORD SafetyBytes;
    real32 tSine;
   
    //TODO math gets simplier if we add gytes per second field.
};

struct win32_debug_time_marker
{
    DWORD OutputPlayCursor;
    DWORD OutputWriteCursor;
    DWORD OutputLocation;
    DWORD OutputByteCount;
    DWORD ExpectedFlipPlayCursor;
    
    DWORD FlipPlayCursor;
    DWORD FlipWriteCursor;
};
struct win32_game_code
{
    HMODULE GameCodeDLL;
    FILETIME DLLLastWriteTime;
    //NOTE Following can be 0
    game_update_and_render *UpdateAndRender;
    game_get_sound_samples *GetSoundSamples;

    bool32 IsValid;
};
#define WIN32_STATE_FILE_NAME_COUNT MAX_PATH
struct win32_replay_buffer
{
    char ReplayFileName[WIN32_STATE_FILE_NAME_COUNT];
    void *MemoryBlock;
};
struct win32_state
{
    uint64 TotalSize;
    void *GameMemoryBlock;
    win32_replay_buffer ReplayBuffer[4];
   
    
    HANDLE RecordingHandle;
    int InputRecordingIndex;
    
    HANDLE PlayBackHandle;
    int InputPlayingIndex;

    char EXEFileName[MAX_PATH];
    char *OnePastLastEXEFileNameSlash;
};


#define WIN32_HANDMADE_H
#endif
