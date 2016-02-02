/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Petar Kovacevic $
   $Notice: (C) Copyright 2014 by Molly Rocket, Inc. All Rights Reserved. $
   ======================================================================== */
/*
TODO : This is not a final platform layer

- Saved game locations
- Getting a handle to our own executable file
- Asset loading path
- Threading (launch a thread)
- Raw Input (support multiple keyboards)
- Sleep/time begin period
- ClipCursor() (multimonitor support)
 - Fullscreen support
 - WM_SETCURSOR ( Control cursor visibility)
 - QueryCancelAutoplay
 - WM_ACTIVEAPP (for when we are not the active applicarion)
 - Blti speed improvements(BlitBlt)
 - Hardware acceleration  (OpenGL or Direct3D or Both?)
 - GetKeyboardLayout ( for French keyboards, international WASD support)

*/

#include "handmade.h"

#include <windows.h>
#include <stdio.h>
#include <malloc.h>
#include <xinput.h>
#include <dsound.h>

#include "win32_handmade.h"

//TODO: This is a global now.
global_variable bool32 GlobalRunning;
global_variable bool32 GlobalPause;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;
global_variable int64 GlobalPerfCountFrequency;

//NOTE XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
    return (ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

//NOTE XInputSetState and the rest is macro.
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
    return (ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory)
{
    if(Memory)
    {
        VirtualFree(Memory, 0 , MEM_RELEASE);
    }
    
}

internal void
Win32GetEXEFileName(win32_state *State)
{
     //NOTE never use a MAX_PATH in code that is user-facing, because it
    //can be dangerous and lead to bad results.

      
    DWORD SizeOfFilename = GetModuleFileNameA(0, State->EXEFileName, sizeof(State->EXEFileName));
    State->OnePastLastEXEFileNameSlash = State->EXEFileName;
    for(char *Scan = State->EXEFileName;
        *Scan;
        ++Scan)
    {
        if(*Scan == '\\')
        {
            State->OnePastLastEXEFileNameSlash = Scan + 1;
        }
    }
}

internal int
StringLength(char *String)
{
    int Count = 0;
    while(*String++)
    {
        ++Count;
    }
    return(Count);//we do not count a null at the end of each word for exemple "handmade.dll\0" has null at end
}

internal void
CatStrings(size_t SourceACount, char *SourceA,
           size_t SourceBCount, char *SourceB,
           int DestCount , char *Dest)
{
    //TODO Dest bounds checking
    for(int Index = 0;
        Index < SourceACount;
        ++Index)
    {
        *Dest++ = *SourceA++;
    }
    for(int Index = 0;
        Index < SourceBCount;
        ++Index)
    {
        *Dest++ = *SourceB++;
    }
    *Dest++ = 0;    
}

internal void
Win32BuildEXEPathFileName(win32_state *State, char *FileName,
                          int DestCount ,char *Dest)
{
    CatStrings(State->OnePastLastEXEFileNameSlash - State->EXEFileName, State->EXEFileName,           
               StringLength(FileName), FileName,
               DestCount, Dest);
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile)
{
    debug_read_file_result Result = {};
    
    HANDLE FileHandle = CreateFileA(FileName,GENERIC_READ, FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if(FileHandle != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER FileSize;
        if(GetFileSizeEx(FileHandle, &FileSize))
            {                               
                uint32 FileSize32 = SafeTruncateUInt64((uint32)FileSize.QuadPart);
                Result.Contents = VirtualAlloc(0,FileSize32, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
                if(Result.Contents)
                {
                    DWORD BytesRead;
                    if(ReadFile(FileHandle, Result.Contents, FileSize32, &BytesRead, 0) &&
                       (FileSize32 == BytesRead))    
                    {
                        //NOTE:(Petar): File read successfylly
                        Result.ContentSize = FileSize32;
                    }
                    else
                    {
                        //TODO logging
                        DEBUGPlatformFreeFileMemory(Thread, Result.Contents);
                        Result.Contents = 0;
                    }
                }
                else
                {
                    //TODO logging
                }          
            }
        else
        {
            //TODO logging
        }
        
        CloseHandle(FileHandle);            
    }
    else
    {
        //TODO logging
    }
    return (Result);
}

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile)
{
    bool32 Result = false;
    
    HANDLE FileHandle = CreateFileA(FileName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if(FileHandle != INVALID_HANDLE_VALUE)
    {        
        DWORD BytesWritten;
        if(WriteFile(FileHandle, Memory, MemorySize, &BytesWritten, 0))  
        {
            //NOTE:(Petar): File read successfully
            Result = (BytesWritten == MemorySize);
        }
        else
        {
            //TODO logging
          
            Result = false;
        }
        CloseHandle(FileHandle);
    }
    else
    {
        //TODO logging
    }
    return(Result);
}


inline FILETIME
Win32GetLastWriteTime(char *Filename)
{
    FILETIME LastWriteTime = {};
#if 0
    WIN32_FIND_DATA FindData;
    HANDLE FindHandle = FindFirstFileA(Filename, &FindData);
    if(FindHandle != INVALID_HANDLE_VALUE)
    {
        LastWriteTime =  FindData.ftLastWriteTime;
        FindClose(FindHandle);        
    }
#else
    WIN32_FILE_ATTRIBUTE_DATA Data;
    if(GetFileAttributesEx(Filename, GetFileExInfoStandard, &Data))
    {
        LastWriteTime = Data.ftLastWriteTime;
    }
#endif    
    
    return(LastWriteTime);
}


internal win32_game_code
Win32LoadGameCode(char *SourceDLLName, char *TempDLLName)
{
    win32_game_code Result = {};

    //TODO need to get proper path here!
    //TODO automatic determination of when updates are necessary.
    
    Result.DLLLastWriteTime = Win32GetLastWriteTime(SourceDLLName);
    
    CopyFile(SourceDLLName, TempDLLName, FALSE);
    Result.GameCodeDLL = LoadLibraryA(TempDLLName);
    if(Result.GameCodeDLL)
    {
        Result.UpdateAndRender = (game_update_and_render *)
            GetProcAddress(Result.GameCodeDLL, "GameUpdateAndRender");
        Result.GetSoundSamples = (game_get_sound_samples *)
            GetProcAddress(Result.GameCodeDLL, "GameGetSoundSamples");

        Result.IsValid = (Result.UpdateAndRender &&
                          Result.GetSoundSamples);
    }
    
    if(!Result.IsValid)
    {
        Result.UpdateAndRender = 0;
        Result.GetSoundSamples = 0;
    }
    
    return(Result);
}

internal void
Win32UnloadGameCode(win32_game_code *GameCode)
{
    if(GameCode->GameCodeDLL)
    {
        FreeLibrary(GameCode->GameCodeDLL);
        GameCode->GameCodeDLL = 0;
    }
    GameCode->IsValid = false;
    GameCode->UpdateAndRender = 0;
    GameCode->GetSoundSamples = 0;
}

internal void
Win32LoadXInput(void)
{
    HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
    if(!XInputLibrary)
    {
        //TODO (pero): Diagnostic
        XInputLibrary = LoadLibraryA("xinput1_3.dll");
    }
    if(XInputLibrary)
    {
        XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
        if(!XInputGetState) { XInputGetState = XInputGetStateStub;}
        
        XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
        if(!XInputSetState) { XInputSetState = XInputSetStateStub;}

        //TODO(Pero): Diagnostic
        
    }
    else
    {
        //TODO(Pero): Diagnostic
    }
}

internal void Win32InitDSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize)
{
    //NOTE  Load The library
    HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");

    if(DSoundLibrary)
    {
        //NOTE Get a direct sound object!
        direct_sound_create *DirectSoundCreate = (direct_sound_create *)
            GetProcAddress(DSoundLibrary, "DirectSoundCreate");

        LPDIRECTSOUND DirectSound;
        if(DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
        {
            WAVEFORMATEX WaveFormat = {};
            WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
            WaveFormat.nChannels = 2;
            WaveFormat.nSamplesPerSec = SamplesPerSecond;
            WaveFormat.wBitsPerSample = 16;          
            WaveFormat.nBlockAlign = (WaveFormat.nChannels*WaveFormat.wBitsPerSample) / 8;
            WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec*WaveFormat.nBlockAlign;
            WaveFormat.cbSize = 0;
            
            // DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY);
            if(SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
            {
                DSBUFFERDESC BufferDescription = {};
                BufferDescription.dwSize =  sizeof(BufferDescription);
                BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;
                              
                LPDIRECTSOUNDBUFFER PrimaryBuffer;
                
                if(SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
                {
                    // if(SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat)))
                    HRESULT Error = PrimaryBuffer->SetFormat(&WaveFormat);                  
                    if(SUCCEEDED(Error))
                    {
                        //NOTE We have funally set the format
                        OutputDebugStringA("Primary buffer format was set .\n");
                    }
                    else
                    {
                        //TODO diagnostic
                    }
                }
                else
                {
                    //TODO diagnostic
                }               
            }
            else
            {
                //TODO Diagnostic
            }

            //TODO DSBCAPS_GETCURRENTPOSITION2
            DSBUFFERDESC BufferDescription = {};
            BufferDescription.dwSize =  sizeof(BufferDescription);
            BufferDescription.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
            BufferDescription.dwBufferBytes = BufferSize;
            BufferDescription.lpwfxFormat = &WaveFormat;
           
           
            // if(SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0)))
            HRESULT Error= DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0);
            if(SUCCEEDED(Error))
            {
                //NOTE start playing
                OutputDebugStringA("Secondary buffer created successfully.\n");                 
            }
            //NOTE "create"  a primary buffer

            //NOTE (petar): "create" a secondary buffer
            //  BufferDescription.dwBufferBytes = BufferSize;

            //NOTE (Petar): Start it Playing!
        }
        else
        {
            //TODO(Pero): Diagnostic
        }
    }
}

internal win32_window_dimension Win32GetWindowDimension(HWND Window)
{
    win32_window_dimension Result;
    
    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    Result.Width = ClientRect.right - ClientRect.left;
    Result.Height = ClientRect.bottom - ClientRect.top;

    return (Result);
}


internal void Win32ResizeDIBSection
(win32_offscreen_buffer *Buffer,
 int Width,
 int Height)
{
    //TODO Bulletproof this
    //maybe free first ten free if that fails.

    if(Buffer->Memory)
    {
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
    }
    
    Buffer->Width = Width;
    Buffer->Height = Height;

    int BytesPerPixel = 4;
    Buffer->BytesPerPixel = BytesPerPixel;       
    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Buffer->Width;
    Buffer->Info.bmiHeader.biHeight = -Buffer->Height; //negative means windows treats this as first byte on top left.
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = 32;
    Buffer->Info.bmiHeader.biCompression = BI_RGB;

    //NOTE Hiiiii
    
    // int BytesPerPixel = 4;
    int BitmapMemorySize = (Width*Height)*BytesPerPixel;
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    Buffer->Pitch = Width*BytesPerPixel; //check this code in episode prior to 11
    //RenderWeirdGradient(128,0);
    //TODO probably clear this to black

   
}
//Device Context is used to draw into windows
internal void
Win32DisplayBufferInWindow(win32_offscreen_buffer *Buffer,
                           HDC DeviceContext,
                           int WindowWidth, int WindowHeight
                           )
// int X, int Y, int Width, int Height)
{
    
    //int WindowWidth = ClientRect.right - ClientRect.left; // This is now moved up to the Win32DisplayBufferInWindow
    // int WindowHeight = ClientRect.bottom - ClientRect.top;
//NOTE for portotyping purposes
    //1-to-1 pixel
    StretchDIBits(DeviceContext,
                  // 0,0, WindowWidth, WindowHeight,
                  0,0,Buffer->Width, Buffer->Height,
                  0,0,Buffer->Width, Buffer->Height,
                  Buffer->Memory,
                  &Buffer->Info,
                  DIB_RGB_COLORS, SRCCOPY);
}

//Following will call back window and set properties.

internal LRESULT CALLBACK
Win32MainWindowCallback(HWND Window,
                        UINT Message,
                        WPARAM WParam,
                        LPARAM LParam)
{
    LRESULT Result = 0;
    
    switch(Message)
    {
        //case WM_SIZE:
        // {
        //  win32_window_dimension Dimension = Win32GetWindowDimension(Window);
        // Win32ResizeDIBSection(&GlobalBackBuffer, Dimension.Width, Dimension.Height);
        //  }break;
     
        case WM_CLOSE:
        {
            // TODO : Handle this with a message to the user
            GlobalRunning = false;
//           OutputDebugStringA("WM_CLOSE\n");
        } break;

        case WM_ACTIVATEAPP:
        {
#if 0
            if(WParam == TRUE)
            {
                SetLayeredWindowAttributes(Window, RGB(0, 0, 0), 255, LWA_ALPHA);
            }
            else
            {
                SetLayeredWindowAttributes(Window, RGB(0, 0, 0), 64, LWA_ALPHA);
            }
#endif
        } break;

        case WM_DESTROY:
        {
            //TODO handle this as ab error - recreate window?
            GlobalRunning = false;
        }break;

        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            Assert(!"NOOOOOOKeyboard input came in through non-dispatch message.");
        } break;
        
        case WM_PAINT:
        {
            PAINTSTRUCT Paint;
            HDC DeviceContext=BeginPaint(Window, &Paint);
           
            win32_window_dimension Dimension = Win32GetWindowDimension(Window);
            Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext,
                                       Dimension.Width, Dimension.Height); 
            EndPaint(Window,&Paint);
        } break;

        default:
        {
            // OutputDebugStringA("default\n");
            Result =DefWindowProcA(Window, Message, WParam, LParam);
            
        } break;
    }

    return(Result);
}


internal void
Win32ClearBuffer(win32_sound_output * SoundOutput)
{
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;    
    if(SUCCEEDED(GlobalSecondaryBuffer->Lock(0,SoundOutput->SecondaryBufferSize,
                                             &Region1, &Region1Size,
                                             &Region2, &Region2Size,
                                             0)))
    {
        //TODO assert that region1Size is valid
        uint8 *DestSample = (uint8 *)Region1;
        for(DWORD ByteIndex = 0;
            ByteIndex < Region1Size;
            ++ByteIndex)
        {
            *DestSample++ = 0;                                    
        }
        DestSample = (uint8 *)Region2;
        for(DWORD ByteIndex = 0;
            ByteIndex < Region2Size;
            ++ByteIndex)
        {
            *DestSample++ = 0;                                    
        }
        GlobalSecondaryBuffer->Unlock(Region1,Region1Size, Region2, Region2Size);  
    }
}

//NOTE(petar): Episode 012 50 min is debugging for this.
internal void
Win32FillSoundBuffer(win32_sound_output * SoundOutput, DWORD ByteToLock , DWORD BytesToWrite,
                     game_sound_output_buffer * SourceBuffer)
{
    //TODO(petar): More Strenous tests
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;
    if(SUCCEEDED(GlobalSecondaryBuffer->Lock(ByteToLock,BytesToWrite,
                                             &Region1,&Region1Size,
                                             &Region2,&Region2Size,
                                             0)))
    {
        //TODO Assert that region size is valid

        //TODO colapse these two loops   
        DWORD Region1SampleCount = Region1Size/SoundOutput->BytesPerSample;   
        int16 *DestSample = (int16 *)Region1;
        int16 *SourceSample = SourceBuffer->Samples;
        for(DWORD SampleIndex = 0;
            SampleIndex < Region1SampleCount;
            ++SampleIndex)
        {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            ++SoundOutput->RunningSampleIndex;                           
        }
        
        DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
        DestSample = (int16 *)Region2;
        for(DWORD SampleIndex = 0;
            SampleIndex < Region2SampleCount;
            ++SampleIndex)
        {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            ++SoundOutput->RunningSampleIndex;   
        }
        GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
}

internal void
Win32ProcessKeyboardMessage(game_button_state *NewState, bool32 IsDown)
{
  if(NewState->EndedDown != IsDown)
  {
    NewState->EndedDown = IsDown;
    ++NewState->HalfTransitionCount;   
  }
}

internal void
Win32ProcessXInputDigitalButton(DWORD XInputButtonState,
                                game_button_state *OldState, DWORD ButtonBit,
                                game_button_state *NewState)
{
    NewState->EndedDown = ((XInputButtonState & ButtonBit) == ButtonBit);
    NewState->HalfTransitionCount = (OldState->EndedDown != NewState->EndedDown) ? 1 : 0; 
}


internal real32
Win32ProcessXInputStickValue(SHORT Value, SHORT DeadZoneThreshold)
{
    real32 Result = 0;    
    if (Value < -DeadZoneThreshold)
    {
        Result = (real32)((Value + DeadZoneThreshold) /( 32768.0f - DeadZoneThreshold));
    }
      
    else if(Value > DeadZoneThreshold)
    {
        Result = (real32)((Value - DeadZoneThreshold) / (32767.0f - DeadZoneThreshold));
    }
    return (Result);
}


internal void
Win32GetInputFileLocation(win32_state *State, int SlotIndex, int DestCount, char *Dest)
{
    Assert(SlotIndex ==1 );
    Win32BuildEXEPathFileName(State, "loop_edit.hmi", DestCount, Dest);
}

internal void
Win32BeginRecordingInput(win32_state *State, int InputRecordingIndex)
{
    State->InputRecordingIndex = InputRecordingIndex;
    //TODO These 
    //TODO
    char FileName[WIN32_STATE_FILE_NAME_COUNT];
    Win32GetInputFileLocation(State, InputRecordingIndex, sizeof(FileName), FileName);
  
    State->RecordingHandle =
        CreateFileA(FileName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);

    DWORD BytesToWrite = (DWORD)State->TotalSize;
    Assert(State->TotalSize == BytesToWrite);
    DWORD BytesWritten;
    WriteFile(State->RecordingHandle, State->GameMemoryBlock, BytesToWrite, &BytesWritten, 0);    
}
    
internal void
Win32EndRecordingInput(win32_state *State)
{
    CloseHandle(State->RecordingHandle);
    State->InputRecordingIndex = 0;
}

internal void
Win32BeginInputPlayBack(win32_state *State, int InputPlayingIndex)
{
    State->InputPlayingIndex = InputPlayingIndex;
    
    char FileName[WIN32_STATE_FILE_NAME_COUNT];
    Win32GetInputFileLocation(State, InputPlayingIndex, sizeof(FileName), FileName);
    
    State->PlayBackHandle =
        CreateFileA(FileName,GENERIC_READ, FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);

    DWORD BytesToRead = (DWORD)State->TotalSize;
    Assert(State->TotalSize == BytesToRead);
    DWORD BytesRead;
    ReadFile(State->PlayBackHandle, State->GameMemoryBlock, BytesToRead, &BytesRead, 0);
}
    
internal void
Win32EndInputPlayBack(win32_state *State)
{
    CloseHandle(State->PlayBackHandle);
    State->InputPlayingIndex = 0;
}

internal void
Win32RecordInput(win32_state *State, game_input *NewInput)
{
    DWORD BytesWritten;
    WriteFile(State->RecordingHandle, NewInput, sizeof(*NewInput), &BytesWritten, 0);
}
                        
internal void
Win32PlayBackInput(win32_state *State, game_input *NewInput)
{
    DWORD BytesRead = 0;
    if(ReadFile(State->PlayBackHandle, NewInput, sizeof(*NewInput), &BytesRead, 0))
    {
        if(BytesRead == 0)
        {
            //NOTE we've hit the end of the stream
            int PlayingIndex = State->InputPlayingIndex;
            Win32EndInputPlayBack(State);
            Win32BeginInputPlayBack(State, PlayingIndex);
            ReadFile(State->PlayBackHandle, NewInput, sizeof(*NewInput), &BytesRead, 0);
        }
    }
}

internal void
Win32ProcessPendingMessages(win32_state *State, game_controller_input *KeyboardController)
{
    MSG Message;
    while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
    {        
        switch(Message.message)
        {
            case WM_QUIT:
            {
                GlobalRunning = false;                
            } break;
                           
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP:
            {                               
                uint32 VKCode =(uint32) Message.wParam;
                //NOTE Since we are comparing was down and ISDown we must use following two lines.
                bool32 WasDown = ((Message.lParam & (1 << 30)) != 0);
                bool32 IsDown = ((Message.lParam & (1 << 31)) == 0);                   
                if(WasDown != IsDown)
                {
                    if(VKCode == 'W')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveUp, IsDown);
                    }
                    else if(VKCode == 'T')
                    {
                        OutputDebugStringA("Escape: ");
                        if(IsDown)
                        {
                            OutputDebugStringA("IsDown");
                        }
                        if(WasDown)
                        {
                            OutputDebugStringA("WasDown");
                        }
                        OutputDebugStringA("\n");
                    
                    }
                    else if (VKCode =='A')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveLeft, IsDown);
                    }
                    else if(VKCode == 'S')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveDown, IsDown);
                    }
                    else if(VKCode == 'D')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveRight, IsDown);
                    }
                    else if(VKCode == 'Q')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->LeftShoulder, IsDown);
                    }
                    else if(VKCode == 'E')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->RightShoulder, IsDown);
                    }
                    else if(VKCode == VK_UP)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionUp, IsDown);
                    }
                    else if(VKCode == VK_LEFT)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionLeft,IsDown);
                    }
                    else if(VKCode == VK_DOWN)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionDown, IsDown);
                    }
                    else if(VKCode == VK_RIGHT)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionRight, IsDown);
                    }
                    else if(VKCode == VK_ESCAPE)
                    {
                        // GlobalRunning = false;
                        Win32ProcessKeyboardMessage(&KeyboardController->Start, IsDown);
                    }
                    else if(VKCode == VK_SPACE)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->Back, IsDown);
                    }
#if HANDMADE_INTERNAL
                    else if(VKCode == 'P')
                    {
                        if(IsDown)
                        {
                            GlobalPause = !GlobalPause;                       
                        }
                    }
                    else if (VKCode == 'L')
                    {
                        if(IsDown)
                        {
                            if(State->InputRecordingIndex == 0)
                            {
                                Win32BeginRecordingInput(State, 1);
                            }
                            else
                            {
                                Win32EndRecordingInput(State);
                                Win32BeginInputPlayBack(State, 1);                           
                            }
                        }
                    }                 
#endif
                }
            
                bool32 AltKeyWasDown = (Message.lParam & (1 << 29));
                if((VKCode == VK_F4) && AltKeyWasDown)
                {
                    GlobalRunning = false;
                }
            } break;
                            
            default:
            {
                TranslateMessage(&Message);
                DispatchMessageA(&Message);
            } break;
        }                      
    }
}

//#define used for different platforms 1996
// Function creates window.



inline LARGE_INTEGER
Win32GetWallClock(void)
{
    LARGE_INTEGER Result;    
    QueryPerformanceCounter(&Result);
    return(Result); 
}

inline real32
Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End)
{
    real32 Result = ((real32)(End.QuadPart - Start.QuadPart) /
                     (real32)GlobalPerfCountFrequency);
    return(Result);
}

internal void
Win32DebugDrawVertical(win32_offscreen_buffer *BackBuffer,
                       int X, int Top, int Bottom, uint32 Color)
{
    if(Top <= 0)
    {
        Top = 0;
    }

    if(Bottom > BackBuffer->Height)
    {
        Bottom = BackBuffer->Height;
    }
    
    if((X >= 0) && ( X < BackBuffer->Width))
    {
        uint8 *Pixel = ((uint8*)BackBuffer->Memory +
                        X*BackBuffer->BytesPerPixel +
                        Top*BackBuffer->Pitch);
        for(int Y = Top;
            Y < Bottom;
            ++Y)
        {
            *(uint32 *)Pixel = Color;
            Pixel += BackBuffer->Pitch;
        }
    }
}

inline void
win32DrawSoundBufferMarker(win32_offscreen_buffer *BackBuffer,
                           win32_sound_output *SoundOutput,
                           real32 C, int PadX, int Top, int Bottom,
                           DWORD Value, DWORD Color)
{
    // Assert(Value < SoundOutput->SecondaryBufferSize);
    real32 XReal32 = (C * (real32)Value);
    int X = PadX + (int)XReal32;        
    Win32DebugDrawVertical(BackBuffer, X, Top, Bottom, Color);
}

internal void
Win32DebugSyncDisplay(win32_offscreen_buffer *BackBuffer,
                      int MarkerCount, win32_debug_time_marker *Markers,
                      int CurrentMarkerIndex,
                      win32_sound_output *SoundOutput, real32 TargetSecondsPerFrame)
{

    //TODO draw where we are writting out sound
    int PadX = 16;
    int PadY = 16;

    int LineHeight = 64;
    
    real32 C = (real32)(BackBuffer->Width - 2*PadX) / (real32)SoundOutput->SecondaryBufferSize;
    for(int MarkerIndex = 0;
        MarkerIndex< MarkerCount;
        ++MarkerIndex)
    {
        win32_debug_time_marker *ThisMarker = &Markers[MarkerIndex];
        Assert(ThisMarker->OutputPlayCursor < SoundOutput->SecondaryBufferSize);
        Assert(ThisMarker->OutputWriteCursor < SoundOutput->SecondaryBufferSize);
        Assert(ThisMarker->OutputLocation < SoundOutput->SecondaryBufferSize);
        Assert(ThisMarker->OutputByteCount < SoundOutput->SecondaryBufferSize);       
        Assert(ThisMarker->FlipPlayCursor < SoundOutput->SecondaryBufferSize);
        Assert(ThisMarker->FlipWriteCursor < SoundOutput->SecondaryBufferSize);
        
        DWORD PlayColor = 0xFFFFFFFF;
        DWORD WriteColor = 0xFFFF0000;
        DWORD ExpectedFlipPlayColor = 0xFFFFFF00;
        DWORD PlayWindowColor = 0xFFFF00FF;

        int Top = PadY;
        int Bottom = PadY + LineHeight;
        if(MarkerIndex == CurrentMarkerIndex)
        {
            Top += LineHeight+PadY;
            Bottom += LineHeight+PadY;

            int FirstTop = Top;

            win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputPlayCursor, PlayColor);
            win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputWriteCursor, WriteColor);

            Top += LineHeight+PadY;
            Bottom += LineHeight+PadY;
            
            win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputLocation, PlayColor);
            win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputLocation + ThisMarker->OutputByteCount, WriteColor);
            
            Top += LineHeight+PadY;
            Bottom += LineHeight+PadY;

            win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, FirstTop, Bottom, ThisMarker->ExpectedFlipPlayCursor,ExpectedFlipPlayColor);
           
        }
             
        win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipPlayCursor, PlayColor);
        win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipPlayCursor + 480*SoundOutput->BytesPerSample, PlayWindowColor);
        win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipWriteCursor, WriteColor);
    }
}

int CALLBACK
WinMain(HINSTANCE Instance,
        HINSTANCE PrevInstance,
        LPSTR CommandLine,
        int ShowCode)
{
    //  char *SourceDLLName = "handmade.dll";
//    wsprintf("%shandmade.dll", E);
    win32_state State = {};
    
    LARGE_INTEGER PerfCountFrequencyResult;
    QueryPerformanceFrequency(&PerfCountFrequencyResult);
    GlobalPerfCountFrequency = PerfCountFrequencyResult.QuadPart;

    Win32GetEXEFileName(&State);
    
    char SourceGameCodeDLLFullPath[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildEXEPathFileName(&State, "handmade.dll",
                              sizeof(SourceGameCodeDLLFullPath), SourceGameCodeDLLFullPath);

    char TempGameCodeDLLFullPath[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildEXEPathFileName(&State, "handmade_temp.dll",
                              sizeof(TempGameCodeDLLFullPath), TempGameCodeDLLFullPath);
    

    //NOTE Sset windows schedullar granulrity to 1ms
    //so that our sleep() can be more granular
    UINT DesiredSchedulerMS =  1;
    bool32 SleepIsGranular = (timeBeginPeriod(DesiredSchedulerMS) == TIMERR_NOERROR);
    
    Win32LoadXInput();

// uint8 BigOldBlockOfMemory[2*1024*1024] = {}; // make stack break
    
    WNDCLASSA WindowClass = {};
     
    Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);
    
    //TODO(Pero) Check if HREDRAW|CS_VREDRAW is still needed
    //   WindowClass.style=CS_OWNDC|CS_HREDRAW|CS_VREDRAW;   do not need this one.
    
    WindowClass.style = CS_HREDRAW|CS_VREDRAW;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;  
    WindowClass.hInstance= Instance;
    //WindowClass.hIcon;
    WindowClass.lpszClassName= "HandmadeHeroWindowClass";

    //TODO How do we reliably query to do this on windows
  
//#define MonitorRefreshHz  60
//#define GameUpdateHz (MonitorRefreshHz / 2)
    //real32 TargetSecondsPerFrame = 1.0f / (real32)GameUpdateHz;


    //TODO(petar): 
    if(RegisterClassA(&WindowClass))
    {
        HWND Window =
            CreateWindowExA(0,
                            // WS_EX_TOPMOST|WS_EX_LAYERED,
                            WindowClass.lpszClassName,
                            "HandMade Hero",
                            WS_OVERLAPPEDWINDOW|WS_VISIBLE ,
                            CW_USEDEFAULT,
                            CW_USEDEFAULT,
                            CW_USEDEFAULT,
                            CW_USEDEFAULT,
                            0,
                            0,
                            Instance,
                            0);                                           
        if(Window)
        {
            // SetLayeredWindowAttributes(Window, RGB(0, 0, 0), 128, LWA_ALPHA);            
            win32_sound_output SoundOutput = {};
           
            int MonitorRefreshHz = 60;
            HDC RefreshDC = GetDC(Window);
            int Win32RefreshRate = GetDeviceCaps(RefreshDC, VREFRESH);
            ReleaseDC(Window, RefreshDC);
            if(Win32RefreshRate > 1)
            {
                MonitorRefreshHz = Win32RefreshRate;
            }
            real32 GameUpdateHz = (MonitorRefreshHz / 2.0f);
            real32 TargetSecondsPerFrame = 1.0f / (real32)GameUpdateHz; 
            //TODO (petar) make this like 60 secons
            SoundOutput.SamplesPerSecond =48000;
            SoundOutput.BytesPerSample = sizeof(int16)*2;
            SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond*SoundOutput.BytesPerSample;
            //TODO get rid of latency sample count
        
            //TODO actually compute this varuabce abd see
            //what resonable value is
            SoundOutput.SafetyBytes = (int)(((real32)SoundOutput.SamplesPerSecond*(real32)SoundOutput.BytesPerSample / GameUpdateHz)/3.0f);
            Win32InitDSound(Window, SoundOutput.SamplesPerSecond,SoundOutput.SecondaryBufferSize);
            Win32ClearBuffer(&SoundOutput);                      
            GlobalSecondaryBuffer->Play(0, 0,DSBPLAY_LOOPING);

            win32_state Win32State = {};
            GlobalRunning = true;
          
#if 0
            //play sound
            //NOTE this is to test PlayCursor/WriteCursor update frequency on handmade hero machine
            while(GlobalRunning)
            {    
                DWORD PlayCursor;
                DWORD WriteCursor;
                GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor);
                
                char TextBuffer[255];
                _snprintf_s(TextBuffer, sizeof(TextBuffer),
                            "PC:%u BTL:%u\n", PlayCursor, WriteCursor);
                OutputDebugStringA(TextBuffer);
            }

#endif
            //TODO(petar): Pool with bitmap VirtualAlloc

            int16 *Samples = (int16 *)VirtualAlloc(0, SoundOutput.SecondaryBufferSize,
                                                   MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

//following is setting our own memory 
#if HANDMADE_INTERNAL
            LPVOID BaseAddress = (LPVOID)Terabytes(2);
#else            
            LPVOID BaseAddress = 0;
#endif
            
            game_memory GameMemory = {};
            GameMemory.PermanentStorageSize = Megabytes(64);
            GameMemory.TransientStorageSize = Gigabytes(1);
            GameMemory.DEBUGPlatformReadEntireFile = DEBUGPlatformReadEntireFile;
            GameMemory.DEBUGPlatformWriteEntireFile = DEBUGPlatformWriteEntireFile;
            GameMemory.DEBUGPlatformFreeFileMemory = DEBUGPlatformFreeFileMemory; 

            //TODO something here
            //TODO use mem_large_pages and call adjust tocket preveliges when not on xp
            Win32State.TotalSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;
            Win32State.GameMemoryBlock = VirtualAlloc(BaseAddress, (size_t)Win32State.TotalSize,
                                                 MEM_RESERVE|MEM_COMMIT,
                                                 PAGE_READWRITE);
            GameMemory.PermanentStorage = Win32State.GameMemoryBlock;
            //make transient storage be in the same page where permanent storage is.
            GameMemory.TransientStorage = ((uint8 *)GameMemory.PermanentStorage +
                                           GameMemory.TransientStorageSize);

            //for(int ReplayIndex = 0;
            //  ReplayIndex < ArrayCount(Win32State.ReplayBuffers
           
            if(Samples && GameMemory.PermanentStorage && GameMemory.TransientStorage)
            {            
                game_input Input[2] = {};
                game_input *NewInput = &Input[0];
                game_input *OldInput = &Input[1];              

                LARGE_INTEGER LastCounter = Win32GetWallClock();
                LARGE_INTEGER FlipWallClock = Win32GetWallClock();  

                int DebugTimeMarkerIndex = 0;
                win32_debug_time_marker DebugTimeMarkers[30] = {0};

              
                DWORD AudioLatencyBytes = 0;
                real32 AudioLatencySeconds = 0;
                bool32 SoundIsValid = false;

              
                win32_game_code Game = Win32LoadGameCode(SourceGameCodeDLLFullPath,
                                                         TempGameCodeDLLFullPath);
                uint32 LoadCounter = 0;
                     
                uint64 LastCycleCount = __rdtsc(); //rdtsc is not good for shipping for actual game time         
                while(GlobalRunning)
                {                                      
                    FILETIME NewDLLWriteTime = Win32GetLastWriteTime(SourceGameCodeDLLFullPath);
                    if(CompareFileTime(&NewDLLWriteTime, &Game.DLLLastWriteTime) != 0)
                    {
                        Win32UnloadGameCode(&Game);
                        Game = Win32LoadGameCode(SourceGameCodeDLLFullPath,
                                                 TempGameCodeDLLFullPath);
                        LoadCounter = 0;
                    }
                    //TODO(petar) Zeroing macro
                    //TODO we cant zero everything  because tghe up/down state will
                    //be wrong!!!
                    game_controller_input *OldKeyboardController = GetController(OldInput, 0);
                    game_controller_input *NewKeyboardController = GetController(NewInput, 0); 
                    *NewKeyboardController = {};
                    NewKeyboardController->IsConnected = true;
                    for(int ButtonIndex = 0;
                        ButtonIndex < ArrayCount(NewKeyboardController->Buttons);
                        ++ButtonIndex)
                    {
                        NewKeyboardController->Buttons[ButtonIndex].EndedDown =
                            OldKeyboardController->Buttons[ButtonIndex].EndedDown;
                    }
                    OutputDebugStringA("Hiiii\n");
                    Win32ProcessPendingMessages(&Win32State, NewKeyboardController);
                    
                    if(!GlobalPause)
                    {
                        POINT MouseP;
                        GetCursorPos(&MouseP);
                        ScreenToClient(Window, &MouseP);
                        NewInput->MouseX = MouseP.x ;
                        NewInput->MouseY =  MouseP.y;
                        NewInput->MouseZ = 0;
//                        NewInput->MouseButtons[0] = ;
//                        NewInput->MouseButtons[1] = ;
                        Win32ProcessKeyboardMessage(&NewInput->MouseButtons[0],
                                                    GetKeyState(VK_LBUTTON) &(1 <<15));
                        Win32ProcessKeyboardMessage(&NewInput->MouseButtons[1],
                                                    GetKeyState(VK_MBUTTON) &(1 <<15));
                        Win32ProcessKeyboardMessage(&NewInput->MouseButtons[2],
                                                    GetKeyState(VK_RBUTTON) &(1 <<15));
                        Win32ProcessKeyboardMessage(&NewInput->MouseButtons[3],
                                                    GetKeyState(VK_XBUTTON1) &(1 <<15));
                        Win32ProcessKeyboardMessage(&NewInput->MouseButtons[4],
                                                    GetKeyState(VK_XBUTTON2) &(1 <<15));
                                                             
                        
                        //TODO need to not poll disconected controllers
                        //TODO : should we poll this more frequently?
                        //Following will take user input with gampad
                        DWORD MaxControllerCount =  XUSER_MAX_COUNT;
                        if(MaxControllerCount > ArrayCount(NewInput->Controllers) - 1)
                        {
                            MaxControllerCount = (ArrayCount(NewInput->Controllers) - 1);
                        }
                
                        for(DWORD ControllerIndex = 0;
                            ControllerIndex < MaxControllerCount;
                            ++ControllerIndex)
                        { 
                            DWORD OurControllerIndex = ControllerIndex + 1;
                            game_controller_input *OldController = GetController(OldInput, OurControllerIndex);
                            game_controller_input *NewController = GetController(NewInput, OurControllerIndex);
                    
                            XINPUT_STATE ControllerState;
                            if(XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
                            {
                                NewController->IsConnected = true;
                                NewController->IsAnalog = OldController->IsAnalog;
                                // NOTE This controller is pluged in.
                                // TODO See if ControllerState.dwPacketNumber increments too rapidly
                                XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

                           
                                NewController->StickAverageX = Win32ProcessXInputStickValue(
                                    Pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                                NewController->StickAverageY = Win32ProcessXInputStickValue(
                                    Pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                                if((NewController->StickAverageX != 0.0f) ||
                                   (NewController->StickAverageY != 0.0f))
                                {
                                    NewController->IsAnalog = true;
                                }

                                //TODO(Petar): DPad
                                if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)
                                {
                                    NewController->StickAverageY = 1.0f;
                                    NewController->IsAnalog = false;
                                }
                                if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
                                {
                                    NewController->StickAverageY = -1.0f;
                                    NewController->IsAnalog = false;
                                }
                                if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
                                {
                                    NewController->StickAverageX = -1.0f;
                                    NewController->IsAnalog = false;
                                }
                                if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
                                {
                                    NewController->StickAverageX = 1.0f;
                                    NewController->IsAnalog = false;
                                }
                            
                                real32 Threshold = 0.5f;                            
                                Win32ProcessXInputDigitalButton(
                                    (NewController->StickAverageX < -Threshold) ? 1 : 0,
                                    &OldController->MoveLeft, 1,
                                    &NewController->MoveLeft);                              
                                Win32ProcessXInputDigitalButton(
                                    (NewController->StickAverageX > -Threshold) ? 1 : 0,
                                    &OldController->MoveRight, 1,
                                    &NewController->MoveRight);                              
                                Win32ProcessXInputDigitalButton(
                                    (NewController->StickAverageY < -Threshold) ? 1 : 0,
                                    &OldController->MoveDown, 1,
                                    &NewController->MoveDown);                              
                                Win32ProcessXInputDigitalButton(
                                    (NewController->StickAverageY < Threshold) ? 1 : 0,
                                    &OldController->MoveUp, 1,
                                    &NewController->MoveUp);                           

                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                &OldController->ActionDown, XINPUT_GAMEPAD_A ,
                                                                &NewController->ActionDown);
                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                &OldController->ActionRight, XINPUT_GAMEPAD_B ,
                                                                &NewController->ActionRight);
                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                &OldController->ActionLeft, XINPUT_GAMEPAD_X ,
                                                                &NewController->ActionLeft);
                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                &OldController->ActionUp, XINPUT_GAMEPAD_Y,
                                                                &NewController->ActionUp);
                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                &OldController->LeftShoulder, XINPUT_GAMEPAD_LEFT_SHOULDER ,
                                                                &NewController->LeftShoulder);
                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                &OldController->RightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER ,
                                                                &NewController->RightShoulder);
                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                &OldController->Start, XINPUT_GAMEPAD_START ,
                                                                &NewController->Start);
                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                &OldController->Back, XINPUT_GAMEPAD_BACK ,
                                                                &NewController->Back);
                                    
                                //  bool32 Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
                                // bool32 Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
                       
                                //input from sticks on the controller.                       
                            }
                            else 
                            {
                                //NOTE: The controller is not available.
                                NewController->IsConnected = false;
                            }
                        }
                        /*
                        //following is making gamepad vibrate
                        XINPUT_VIBRATION Vibration;
                        Vibration.wLeftMotorSpeed = 60000;
                        Vibration.wRightMotorSpeed = 60000;
                        XInputSetState(0, &Vibration);
                        */                      

                        thread_context Thread = {};
                        game_offscreen_buffer Buffer = {};
                        Buffer.Memory = GlobalBackBuffer.Memory;
                        Buffer.Width = GlobalBackBuffer.Width;
                        Buffer.Height = GlobalBackBuffer.Height;
                        Buffer.Pitch = GlobalBackBuffer.Pitch;
                        Buffer.BytesPerPixel = GlobalBackBuffer.BytesPerPixel;

                        if(Win32State.InputRecordingIndex)
                        {
                            Win32RecordInput(&Win32State, NewInput);
                        }
                        
                        if(Win32State.InputPlayingIndex)
                        {
                            Win32PlayBackInput(&Win32State , NewInput);
                        }
                        if(Game.UpdateAndRender)
                        {
                            Game.UpdateAndRender(&Thread, &GameMemory, NewInput, &Buffer);                              
                        }         
                        //NOTE DurectSound Output test.
                        //Following is playing sound.

                        LARGE_INTEGER AudioWallClock = Win32GetWallClock();
                        real32 FromBeginToAudioSeconds =Win32GetSecondsElapsed(FlipWallClock, AudioWallClock);
                        
                        DWORD PlayCursor;
                        DWORD WriteCursor;
                        if(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK)
                        {      
                            /*NOTE (petar)

                              Here is how sound output compilation works

                              WE define a safety value that is the number of samples we think our game update
                              loop may vary by ( lets say up to 2 ms)

                              When we waje up to write audio we will look anbd see what the play cursor
                              position is and we will forecast aheadwhere we think the
                              play cursor will be on the next frame boundary.

                              We will then look to see if the cusror is before that.
                              if it is by our safe amount, the target fill position is that frame
                              boundary plus one frame. This gives us perfect audio sync in case of a card
                              has low enough latency.

                              If the write cursor is _after_ the next frame boundary, then we assume we can
                              never sync the audio perfectly, so we will write one frame's worth
                              of audio plus safety margin's woprth
                              of guard amount.  
                            */


                            if(!SoundIsValid)
                            {
                                SoundOutput.RunningSampleIndex = WriteCursor / SoundOutput.BytesPerSample;
                                SoundIsValid = true;
                            }
                        
                            DWORD  ByteToLock = ((SoundOutput.RunningSampleIndex*SoundOutput.BytesPerSample) %
                                                 SoundOutput.SecondaryBufferSize);

                            DWORD ExpectedSoundBytesPerFrame =
                                (int)((real32)(SoundOutput.SamplesPerSecond*SoundOutput.BytesPerSample) / GameUpdateHz);
                            real32 SecondsLeftUntilFlip = (TargetSecondsPerFrame - FromBeginToAudioSeconds);
                            DWORD ExpectedBytesUntilFlip = (DWORD)((SecondsLeftUntilFlip/TargetSecondsPerFrame)*(real32)ExpectedSoundBytesPerFrame);

                            DWORD ExpectedFrameBoundaryByte = PlayCursor +  ExpectedBytesUntilFlip;
                     
                            DWORD SafeWriteCursor = WriteCursor;
                            if(SafeWriteCursor < PlayCursor)
                            {
                                SafeWriteCursor += SoundOutput.SecondaryBufferSize;
                            }
                            Assert(SafeWriteCursor >=PlayCursor);
                            SafeWriteCursor += SoundOutput.SafetyBytes;
                       
                            bool32 AudioCardIsLowLatent = (SafeWriteCursor < ExpectedFrameBoundaryByte);
                       
                            DWORD TargetCursor = 0;  
                            if(AudioCardIsLowLatent)
                            {                           
                                TargetCursor = (ExpectedFrameBoundaryByte + ExpectedSoundBytesPerFrame);
                            }
                            else
                            {
                                TargetCursor =(WriteCursor + ExpectedSoundBytesPerFrame +
                                               SoundOutput.SafetyBytes);                           
                            }
                            TargetCursor = (TargetCursor %  SoundOutput.SecondaryBufferSize);
                        
                            DWORD BytesToWrite = 0;
                            if(ByteToLock > TargetCursor)
                            {
                                BytesToWrite = SoundOutput.SecondaryBufferSize - ByteToLock;
                                BytesToWrite += TargetCursor;                            
                            }
                            else
                            {
                                BytesToWrite = TargetCursor - ByteToLock;
                            }
                 
                            game_sound_output_buffer SoundBuffer = {};
                            SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
                            SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
                            SoundBuffer.Samples = Samples;
                            if(Game.GetSoundSamples)
                            {
                                Game.GetSoundSamples(&GameMemory,&SoundBuffer);
                            }
#if HANDMADE_INTERNAL
                            win32_debug_time_marker *Marker = &DebugTimeMarkers[DebugTimeMarkerIndex];
                            Marker->OutputPlayCursor = PlayCursor;
                            Marker->OutputWriteCursor = WriteCursor;
                            Marker->OutputLocation =  ByteToLock;
                            Marker->OutputByteCount = BytesToWrite;
                            Marker->ExpectedFlipPlayCursor = ExpectedFrameBoundaryByte;
                     
                            DWORD UnwrapperWriteCursor = WriteCursor;
                            if (UnwrapperWriteCursor < PlayCursor)
                            {
                                UnwrapperWriteCursor += SoundOutput.SecondaryBufferSize;
                            }                        
                            AudioLatencyBytes = UnwrapperWriteCursor - PlayCursor;
                            AudioLatencySeconds =
                                (((real32)AudioLatencyBytes / (real32)SoundOutput.BytesPerSample) /
                                 (real32)SoundOutput.SamplesPerSecond);

                            char TextBuffer[255];
                            _snprintf_s(TextBuffer, sizeof(TextBuffer),
                                        "BTL:%u TC:%u BTW:%u - PC:%u  WC:%u  DELTA:%u (%fs)\n",
                                        ByteToLock, TargetCursor, BytesToWrite,
                                        PlayCursor, WriteCursor, AudioLatencyBytes, AudioLatencySeconds);
                            OutputDebugStringA(TextBuffer);
#endif
                            Win32FillSoundBuffer(&SoundOutput, ByteToLock , BytesToWrite, &SoundBuffer);                        
                        }
                        else
                        {
                            SoundIsValid = false;
                        }
                                        
                        LARGE_INTEGER WorkCounter = Win32GetWallClock();
                        real32 WorkSecondsElapsed =  Win32GetSecondsElapsed(LastCounter, WorkCounter);

                        //TODO NOt TESTED YET
                        real32 SecondsElapsedForFrame = WorkSecondsElapsed;
                        //TODO (petar) pokusaj da napravis stabilnih 33 frames.
                        //This will check on how many frames we missed and put processor to sleep
                        if(SecondsElapsedForFrame < TargetSecondsPerFrame)
                        {
                            if(SleepIsGranular)
                            {
                                DWORD SleepMS = (DWORD)(1000.0f *(TargetSecondsPerFrame -
                                                                  SecondsElapsedForFrame));
                                if(SleepMS > 0)
                                {
                                    Sleep(SleepMS);
                                }                                
                            }

                            real32 TestSecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter,
                                                                                       Win32GetWallClock());
                            if(TestSecondsElapsedForFrame < TargetSecondsPerFrame)
                            {
                                //TODO LOG MISSED SLEEP HERE
                            }
                                                                                   
                            while(SecondsElapsedForFrame < TargetSecondsPerFrame)
                            {
                                SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter,
                                                                                Win32GetWallClock());
                            }
                        }
                        else
                        {
                            //TODO MiSSED A FRAME
                            //TODO LOGGING
                        }

                        LARGE_INTEGER EndCounter = Win32GetWallClock();
                        real64 MSPerFrame =1000.0f* Win32GetSecondsElapsed(LastCounter, EndCounter);
                        LastCounter= EndCounter;
                    
                    
                        win32_window_dimension Dimension = Win32GetWindowDimension(Window);
#if HANDMADE_INTERNAL
                    
                        Win32DebugSyncDisplay(&GlobalBackBuffer, ArrayCount(DebugTimeMarkers), DebugTimeMarkers,
                                              DebugTimeMarkerIndex -1, &SoundOutput, TargetSecondsPerFrame);
#endif
                        //NOTE since we specified CS_OWBDC we can just get one device context,
                        //and use it forever because we
                        //are not sharing it with anyone.
                        HDC DeviceContext = GetDC(Window);
                        Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext,
                                                   Dimension.Width, Dimension.Height);
                        ReleaseDC(Window, DeviceContext);

                        FlipWallClock = Win32GetWallClock();                        
#if HANDMADE_INTERNAL
                        {
                            //NOTE this is debug code 
                            DWORD PlayCursor;
                            DWORD WriteCursor;
                            if(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK)
                            {
                                Assert(DebugTimeMarkerIndex < ArrayCount(DebugTimeMarkers));
                                win32_debug_time_marker *Marker = &DebugTimeMarkers[DebugTimeMarkerIndex];
                                Marker->FlipPlayCursor = PlayCursor;
                                Marker->FlipWriteCursor = WriteCursor;
                            }                       
                        }
#endif          
                        game_input *Temp = NewInput;
                        NewInput = OldInput;
                        OldInput = Temp;
                        //TODO(petar): Should we clear these
                    
                  
                        uint64 EndCycleCount = __rdtsc();
                        uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
                        LastCycleCount = EndCycleCount;  
                        //  #if 0                    
                   
                        real64 FPS = 0.0f;                        real64 MDPF =(real64)((CyclesElapsed / (1000 *1000)));
                        real64 CPU = FPS*MDPF;

                        char FPSBuffer[255];
                        _snprintf_s(FPSBuffer, sizeof(FPSBuffer),"%.02fms/f, %.02ff/s, %.02fmc/f, CPU: %.02f\n", MSPerFrame,FPS,MDPF,CPU);
                        OutputDebugStringA(FPSBuffer);
#if HANDMADE_INTERNAL
                        ++DebugTimeMarkerIndex;
                        if(DebugTimeMarkerIndex == ArrayCount(DebugTimeMarkers))
                        {
                            DebugTimeMarkerIndex = 0;
                        }
#endif
                    }
                    
                }
            }
            else
            {
             
            }
        }
        else
        {
            //TODO : logging
        }
    }
    else
    {
        //TODO logging
    }
    
    return(0);
}

