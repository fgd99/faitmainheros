#include <stdint.h> // Types ind�pendents de la plateforme
#include <math.h>   // Fonctions math�matiques

// Pour bien comprendre la diff�rence de fonctionnement des variables statiques en C en fonction du scope
#define internal static // fonctions non visible depuis l'ext�rieur de ce fichier
#define local_persist static     // variable visibles juste dans le scope o� elle d�finie
#define global_variable static   // variable visible dans tous le fichiers (globale)

// Constantes
#define PI32 3.14159265358979323846
#define XUSER_MAX_COUNT 4 // Normalement d�finie dans Xinput.h, absente de VS2010

// Quelques d�finitions de types d'entiers pour ne pas �tre d�pendant de la plateforme
typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32_t bool32;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef float real32;
typedef double real64;

// Impl�mentation du coeur du jeu ind�pendemment de la plateforme
#include "faitmain.h"

// Includes sp�cifiques � la plateforme
#include <Windows.h>
#include <Xinput.h> // Pour la gestion des entr�es (manette...)
#include <dsound.h> // Pour jouer du son avec DirectSound
#include <stdio.h>

#include "win32_faitmain.h"

// variables globales pour le moment, on g�rera autrement plus tard
global_variable bool32 GlobalRunning = true;
global_variable bool32 GlobalPause = false;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;
global_variable int64 GlobalPerfCountFrequency;

// Permet de renvoyer les dimensions actuelles de la fen�tre
internal win32_window_dimension
Win32GetWindowDimension(HWND Window) {
  win32_window_dimension Result;
  RECT ClientRect;
  GetClientRect(Window, &ClientRect);
  Result.Width = ClientRect.right - ClientRect.left;
  Result.Height = ClientRect.bottom - ClientRect.top;
  return(Result);
}

// Ici on d�finit des pointeurs vers les fonctions du moteur de jeu
// charg�es depuis une DLL
struct win32_game_code
{
  HMODULE GameCodeDLL;
  game_update_and_render *UpdateAndRender;
  game_get_sound_samples *GetSoundSamples;
  bool32 IsValid;
};

internal win32_game_code
Win32LoadGameCode(void)
{
  win32_game_code Result = {};
  Result.GameCodeDLL = LoadLibraryA("faitmain.dll");
  if(Result.GameCodeDLL)
  {
    Result.UpdateAndRender = (game_update_and_render*)
      GetProcAddress(Result.GameCodeDLL, "GameUpdateAndRender");
    Result.GetSoundSamples = (game_get_sound_samples*)
      GetProcAddress(Result.GameCodeDLL, "GameGetSoundSamples");
    Result.IsValid = (Result.UpdateAndRender && Result.GetSoundSamples);
  }
  if(!Result.IsValid)
  {
    Result.UpdateAndRender = GameUpdateAndRenderStub;
    Result.GetSoundSamples = GameGetSoundSamplesStub;
  }
  return(Result);
}

// Ici on d�finit des pointeurs vers les fonctions de Xinput
// Cette technique traditionnelle permet d'utiliser des fonctions
// Sans linker directement la lib, et permet aussi de tester si la lib est pr�sente
// On utilise alors des macros pour d�finir la signature des fonctions
// Ici on d�finit deux fonctions qui vont se subsituer aux vraies si la lib n'est pas trouv�e
// D'abord pour XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub) {
  return(ERROR_DEVICE_NOT_AVAILABLE); // Au lieu de renvoyer 0 (ERROR_SUCCESS) on renvoit un code plus explicite
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// De m�me pour XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub) {
  return(ERROR_DEVICE_NOT_AVAILABLE);
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

// On va alors charger la dll et pointer vers ses fonctions
// Soit la dll est pr�sente sur le syst�me ou alors il faut utiliser la lib redistributable
internal void
Win32LoadXInput(void) {
  // on essaye de charger la version 1.4 de xinput
  HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
  // on essaye une version un peu plus ancienne qui sera pr�sente sur plus de machines si on n'a pas la 1.4
  if (!XInputLibrary) XInputLibrary = LoadLibraryA("xinput1_3.dll");
  // Autre version alternative
  if (!XInputLibrary) XInputLibrary = LoadLibraryA("xinput9_1_0.dll");
  if (XInputLibrary)
  {
    XInputGetState_ = (x_input_get_state*)GetProcAddress(XInputLibrary, "XInputGetState");
    XInputSetState_ = (x_input_set_state*)GetProcAddress(XInputLibrary, "XInputSetState");
  }
  else
  {
    OutputDebugStringA("Cannot load xinput1_4.dll or xinput1_3.dll\n");
  }
}

/**
 * Impl�mentation des fonctions sp�cifiques � la plateforme
 * d�clar�es dans faitmain.h
 **/
DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory)
{
  if(Memory)
  {
    VirtualFree(Memory, 0, MEM_RELEASE);
  }
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile)
{
  debug_read_file_result Result = {};
  HANDLE FileHandle = CreateFileA(
    Filename,
    GENERIC_READ,
    FILE_SHARE_READ,
    0,
    OPEN_EXISTING,
    0,
    0);
  if (FileHandle != INVALID_HANDLE_VALUE)
  {
    LARGE_INTEGER FileSize;
    if(GetFileSizeEx(FileHandle, &FileSize))
    {
      uint32 FileSize32 = SafeTruncateUint64(FileSize.QuadPart);
      Result.Contents = VirtualAlloc(0, FileSize32, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
      if (Result.Contents)
      {
        DWORD BytesRead;
        if (ReadFile(FileHandle, Result.Contents, FileSize32, &BytesRead, 0) &&
              (FileSize32 == BytesRead)) // On v�rifie aussi si on a bien tout lu
        {
          // Le fichier a bien �t� lu
          Result.ContentsSize = FileSize32;
        }
        else
        {
          // On lib�re la m�moire imm�diatement car le fichier n'a pas pu �tre ouvert
          DEBUGPlatformFreeFileMemory(Result.Contents);
          Result.Contents = 0;
        }
      }
    }
    CloseHandle(FileHandle);
  }
  return(Result);
}

DEBUG_PLATEFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile)
{
  bool32 Result = false;
  HANDLE FileHandle = CreateFileA(
    Filename,
    GENERIC_WRITE,
    0,
    0,
    CREATE_ALWAYS,
    0,
    0);
  if (FileHandle != INVALID_HANDLE_VALUE)
  {
    DWORD BytesWritten;
    if (WriteFile(FileHandle, Memory, MemorySize, &BytesWritten, 0))
    {
      // Le fichier a bien �t� �crit
      Result = (BytesWritten == MemorySize);
    }
    else
    {

    }
    CloseHandle(FileHandle);
  }
  return(Result);
}

// On effectue de m�me pour les fonctions de DirectSound avec des stubs
// de fonctions si la dll n'a pas pu �tre charg�e
#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuiDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

/**
 * Initialisation de DirectSound
 * On utilise deux buffers, celui qui joue vraiment le son est le secondaire
 * le primaire est une sorte de handle vers la carte son,
 * il joue le son sans resampling du buffer secondaire.
 * Auparavent on utilisait juste le premier buffer pour envoyer le son
 * directement � la carte son.
 * On utilise que 2 channels car on fait un jeu 2D qui n'a pas besoin
 * de positionnement, mais on pourrait le faire pour avoir du surround.
 **/
internal void
Win32InitDSound(HWND Window, uint32 SamplesPerSecond, uint32 BufferSize)
{
  // Chargement de la librairie
  HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");
  if (DSoundLibrary) {
    // Obtention d'un objet DirectSound - mode coop�ratif
    direct_sound_create *DirectSoundCreate = (direct_sound_create*)GetProcAddress(
      DSoundLibrary,
      "DirectSoundCreate");
    LPDIRECTSOUND DirectSound;
    if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
    {
      // Description du format sonore
      WAVEFORMATEX WaveFormat = {};
      WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
      WaveFormat.nChannels = 2;
      WaveFormat.nSamplesPerSec = SamplesPerSecond;
      WaveFormat.wBitsPerSample = 16;
      WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
      WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
      WaveFormat.cbSize = 0;

      // D�finition du mode de coop�ration
      if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
      {
        // Cr�ation d'un buffer principal
        // Astuce pout initialiser tous ses membres � 0
        DSBUFFERDESC BufferDescription = {sizeof(BufferDescription)};
        BufferDescription.dwSize = sizeof(BufferDescription);
        BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

        LPDIRECTSOUNDBUFFER PrimaryBuffer;
        if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
        {
          HRESULT Error = PrimaryBuffer->SetFormat(&WaveFormat);
          if (SUCCEEDED(Error))
          {
            OutputDebugStringA("Primary Buffer format was set.\n");
          }
          else
          {
            OutputDebugStringA("Primary Buffer format was NOT set.\n");
          }
        }
      }
      else
      {
        OutputDebugStringA("Cannot set DirectSound Cooperative Level\n");
      }
      // Cr�ation d'un buffer secondaire qui va contenir les sons
      // Astuce pout initialiser tous ses membres � 0
      DSBUFFERDESC BufferDescription = {sizeof(BufferDescription)};
      BufferDescription.dwSize = sizeof(BufferDescription);
      BufferDescription.dwFlags = 0;
      BufferDescription.dwBufferBytes = BufferSize;
      BufferDescription.lpwfxFormat = &WaveFormat;

      HRESULT Error = DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0);
      if (SUCCEEDED(Error))
      {
        OutputDebugStringA("Secondary Buffer created successfully.\n");
      }
    }
    else
    {
      OutputDebugStringA("Cannot call DirectSoundCreate\n");
    }
  }
  else
  {
    OutputDebugStringA("Cannot load dsound.dll\n");
  }
}

/**
 * Fonction qui permet de d�finir et de r�initialiser un backbuffer en fonction de ses dimensions
 * DIB: Device Independent Bitmap
 **/
internal void
Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{
  if (Buffer->Memory)
  {
    // cf. VirtualProtect, utile pour debug
    VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
  }

  Buffer->Width = Width;
  Buffer->Height = Height;
  Buffer->BytesPerPixel = 4;

  Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
  Buffer->Info.bmiHeader.biWidth = Buffer->Width;
  // Attention au sens des coordonn�es, du bas vers le haut (d'o� le moins)
  Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
  Buffer->Info.bmiHeader.biPlanes = 1;
  Buffer->Info.bmiHeader.biBitCount = 32;
  Buffer->Info.bmiHeader.biCompression = BI_RGB;

  int BitmapMemorySize = (Buffer->Width * Buffer->Height) * Buffer->BytesPerPixel;
  // MEM_COMMIT r�serve automatiquement la m�moire en th�orie
  // mais il vaut mieux lui dire explicitement MEM_RESERVE
  Buffer->Memory = VirtualAlloc(0, BitmapMemorySize,
                                MEM_RESERVE | MEM_COMMIT,
                                PAGE_READWRITE); // cf. aussi HeapAlloc

  Buffer->Pitch = Width * Buffer->BytesPerPixel;
}

/**
 * Ici au d�but on passait ClientRect par r�f�rence avec un pointeur (*ClientRect)
 * cependant comme la structure est petite le passer par valeur est suffisant
 **/
internal void
Win32DisplayBufferInWindow(win32_offscreen_buffer *Buffer,
                           HDC DeviceContext,
                           int WindowWidth,
                           int WindowHeight)
{
  // copie d'un rectangle vers un autre (scaling si n�cessaire, bit op�rations...)
  StretchDIBits(DeviceContext,
                0, 0, WindowWidth, WindowHeight,
                0, 0, Buffer->Width, Buffer->Height,
                Buffer->Memory,
                &Buffer->Info,
                DIB_RGB_COLORS,
                SRCCOPY); // BitBlt: bit-block transfer of the color data => voir les autres modes dans la MSDN
}

/**
 * Callback de la fen�tre principale qui va traiter les messages renvoy�s par Windows
 **/
internal LRESULT CALLBACK
Win32MainWindowCallback(HWND Window,
                        UINT Message,
                        WPARAM WParam,
                        LPARAM LParam)
{
  LRESULT Result = 0;
  switch(Message)
  {
    case WM_SIZE:
      {
        OutputDebugStringA("WM_SIZE\n");
      }
      break;
    case WM_DESTROY:
      {
        // PostQuitMessage(0);
        // Va permettre de sortir de la boucle infinie en dessous
        // Mais finalement on va g�rer �a avec une variable globale pour le moment
        GlobalRunning = false;
        OutputDebugStringA("WM_DESTROY\n");
      }
      break;
    case WM_CLOSE:
      {
        // DestroyWindow(Window);
        GlobalRunning = false;
        OutputDebugStringA("WM_CLOSE\n");
      }
      break;
    case WM_ACTIVATEAPP:
      {
        OutputDebugStringA("WM_ACTIVATEAPP\n");
      }
      break;
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_KEYUP:
      {
        Assert(!"Une entr�e au clavier est pass�e au travers !!!");
      }
      break;
    case WM_PAINT:
      {
        PAINTSTRUCT Paint;
        HDC DeviceContext = BeginPaint(Window, &Paint);
        win32_window_dimension Dimension = Win32GetWindowDimension(Window);
        Win32DisplayBufferInWindow(&GlobalBackBuffer,
                                   DeviceContext,
                                   Dimension.Width,
                                   Dimension.Height);
        EndPaint(Window, &Paint);
      }
      break;
    default:
      {
        // OutputDebugStringA("default\n");
        Result = DefWindowProc(Window, Message, WParam, LParam);
      }
      break;
  }
  return(Result);
}

internal void
Win32ClearSoundBuffer(win32_sound_output *SoundOutput)
{
  VOID *Region1;
  DWORD Region1Size;
  VOID *Region2;
  DWORD Region2Size;

  if (SUCCEEDED(GlobalSecondaryBuffer->Lock(0, SoundOutput->SecondaryBufferSize,
                                            &Region1, &Region1Size,
                                            &Region2, &Region2Size,
                                            0)))
  {
    uint8 *DestSample = (uint8*)Region1;
    for (DWORD ByteIndex = 0; ByteIndex < Region1Size; ++ByteIndex)
    {
      *DestSample++ = 0;
    }

    DestSample = (uint8*)Region2;
    for (DWORD ByteIndex = 0; ByteIndex < Region2Size; ++ByteIndex)
    {
      *DestSample++ = 0;
    }

    // Unlocking the buffer
    GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
  }
}

internal void
Win32FillSoundBuffer(win32_sound_output *SoundOutput,
                     DWORD ByteToLock, DWORD BytesToWrite,
                     game_sound_output_buffer *SourceBuffer)
{
  VOID *Region1;
  DWORD Region1Size;
  VOID *Region2;
  DWORD Region2Size;

  if (SUCCEEDED(GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite,
                                            &Region1, &Region1Size,
                                            &Region2, &Region2Size,
                                            0)))
  {

    // il faut bien avoir Region1Size et Region2Size valides
    DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
    int16 *DestSample = (int16*)Region1;
    int16 *SourceSample = SourceBuffer->Samples;
    for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex)
    {
      *DestSample++ = *SourceSample++;
      *DestSample++ = *SourceSample++;
      ++SoundOutput->RunningSampleIndex;
    }
    DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
    DestSample = (int16*)Region2;
    for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex)
    {
      *DestSample++ = *SourceSample++;
      *DestSample++ = *SourceSample++;
      ++SoundOutput->RunningSampleIndex;
    }
    // Unlocking the buffer
    GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
  }
}

/**
 * Gestion de l'�tat des bouttons d'une manette
 **/
internal void
Win32ProcessXInputDigitalButton(DWORD XInputButtonState,
                                game_button_state *OldState,
                                DWORD ButtonBit,
                                game_button_state *NewState)
{
  NewState->EndedDown = ((XInputButtonState & ButtonBit) == ButtonBit);
  NewState->HalfTransitionCount = (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
}

/**
 * Gestion de l'�tat des bouttons du clavier
 **/
internal void
Win32ProcessKeyboardMessage(game_button_state *NewState,
                            bool32 IsDown)
{
  Assert(NewState->EndedDown != IsDown);
  NewState->EndedDown = IsDown;
  ++NewState->HalfTransitionCount;
}

/**
 * Gestion de la position du stick de la manette avec prise en compte de la deadzone
 **/
internal real32
Win32ProcessXInputStickValue(SHORT Value, SHORT DeadZoneThreshold)
{
  real32 Result = 0;
  if (Value < -DeadZoneThreshold)
    Result = (real32)(Value - DeadZoneThreshold) / (32768.0f - DeadZoneThreshold);
  else if (Value > DeadZoneThreshold)
    Result = (real32)(Value - DeadZoneThreshold) / (32767.0f - DeadZoneThreshold);
  return(Result);
}              

/**
 * Traitement des messages Windows, clavier inclus
 **/
internal void
Win32ProcessPendingMessages(game_controller_input *KeyboardController)
{
  MSG Message;
  // On utilise PeekMessage au lieu de GetMessage qui est bloquant
  while(PeekMessageA(&Message, 0, 0, 0, PM_REMOVE))
  {
    // Test de passage des actions claviers au moteur de jeu
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
          uint32 VKCode = (uint32)Message.wParam;
          // On v�rifie les bits de LParam (cf. MSDN pour les valeurs)
          #define KeyMessageWasDownBit (1 << 30)
          #define KeyMessageIsDownBit (1 << 31)
          bool32 WasDown = ((Message.lParam & KeyMessageWasDownBit) != 0);
          bool32 IsDown = ((Message.lParam & KeyMessageIsDownBit) == 0);

          if (WasDown != IsDown) // Pour �viter les r�p�titions de touches lorsqu'elles sont enfonc�es
          {
            if (VKCode == 'Z')
            {
              Win32ProcessKeyboardMessage(&KeyboardController->MoveUp, IsDown);
            }
            else if (VKCode == 'S')
            {
              Win32ProcessKeyboardMessage(&KeyboardController->MoveDown, IsDown);
            }
            else if (VKCode == 'Q')
            {
              Win32ProcessKeyboardMessage(&KeyboardController->MoveLeft, IsDown);
            }
            else if (VKCode == 'D')
            {
              Win32ProcessKeyboardMessage(&KeyboardController->MoveRight, IsDown);
            }
            else if (VKCode == 'A')
            {
              Win32ProcessKeyboardMessage(&KeyboardController->LeftShoulder, IsDown);
            }
            else if (VKCode == 'E')
            {
              Win32ProcessKeyboardMessage(&KeyboardController->RightShoulder, IsDown);
            }
            else if (VKCode == VK_UP)
            {
              Win32ProcessKeyboardMessage(&KeyboardController->ActionUp, IsDown);
            }
            else if (VKCode == VK_DOWN)
            {
              Win32ProcessKeyboardMessage(&KeyboardController->ActionDown, IsDown);
            }
            else if (VKCode == VK_LEFT)
            {
              Win32ProcessKeyboardMessage(&KeyboardController->ActionLeft, IsDown);
            }
            else if (VKCode == VK_RIGHT)
            {
              Win32ProcessKeyboardMessage(&KeyboardController->ActionRight, IsDown);
            }
            else if (VKCode == VK_ESCAPE)
            {
              Win32ProcessKeyboardMessage(&KeyboardController->Start, IsDown);
            }
            else if (VKCode == VK_SPACE)
            {
              Win32ProcessKeyboardMessage(&KeyboardController->Back, IsDown);
            }
#if FAITMAIN_INTERNAL
            else if (VKCode == 'P')
            {
              if(IsDown) GlobalPause = !GlobalPause;
            }
#endif
          }
          // Comme on capture les touches il faut g�rer nous m�me le Alt-F4 pour quitter
          // Normalement c'est DefWindowProc (en default) qui fait le boulot
          bool32 AltKeyWasDown = (Message.lParam & (1 << 29));
          if ((VKCode == VK_F4) && AltKeyWasDown)
          {
            GlobalRunning = false;
          }
        } break;
      default: // On dispatche les autres boutons au callback pour Windows
        {
          // On demande � Windows de traiter le message
          TranslateMessage(&Message);
          // Envoie le message au main WindowCallback, que l'on a d�fini et d�clar� au dessus
          DispatchMessage(&Message);
        } break;
    }
  }
}

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
  real32 Result = ((real32)(End.QuadPart - Start.QuadPart) / (real32)GlobalPerfCountFrequency);
  return(Result);
}

internal void
Win32DebugDrawVertical(win32_offscreen_buffer *BackBuffer,
                       int X, int Top, int Bottom, uint32 Color)
{
  // On ne dessine que si on est bien dans le cadre du buffer
  if(Top <= 0) Top = 0;
  if(Bottom > BackBuffer->Height) Bottom = BackBuffer->Height;
  if((X >=0) && (X < BackBuffer->Width))
  {
    uint8 *Pixel = ((uint8*)BackBuffer->Memory
                    + X * BackBuffer->BytesPerPixel
                    + Top * BackBuffer->Pitch);
    for(int Y = Top; Y < Bottom; ++Y)
    {
        *(uint32*)Pixel = Color;
        Pixel += BackBuffer->Pitch;
    }
  }
}

inline void
Win32DrawSoundBuffer(win32_offscreen_buffer *BackBuffer,
                     win32_sound_output *SoundOutput,
                     real32 C, int PadX, int Top, int Bottom,
                     DWORD Value, uint32 Color)
{
  real32 XReal32 = C * (real32)Value;
  int X = PadX + (int)XReal32;
  Win32DebugDrawVertical(BackBuffer, X, Top, Bottom, Color);
}

internal void
Win32DebugSyncDisplay(win32_offscreen_buffer *BackBuffer,
                      int MarkerCount, win32_debug_time_marker *Markers,
                      int CurrentMarkerIndex,
                      win32_sound_output *SoundOutput,
                      real32 TargetSecondsPerFrame)
{
  int PadX = 16;
  int PadY = 16;

  int LineHeight = 64;

  real32 C = (real32)(BackBuffer->Width - 2*PadX) / (real32)SoundOutput->SecondaryBufferSize;
  for(int MarkerIndex = 0; MarkerIndex < MarkerCount; ++MarkerIndex)
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
    DWORD ExpectedFlipColor = 0xFFFFFF00;
    DWORD PlayWindowColor = 0xFFFF00FF;

    int Top = PadY;
    int Bottom = PadY + LineHeight;
    if(MarkerIndex == CurrentMarkerIndex)
    {
      Top += PadY + LineHeight;
      Bottom += PadY + LineHeight;

      int FirstTop = Top;

      Win32DrawSoundBuffer(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputPlayCursor, PlayColor);
      Win32DrawSoundBuffer(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputWriteCursor, WriteColor);

      Top += PadY + LineHeight;
      Bottom += PadY + LineHeight;

      Win32DrawSoundBuffer(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputLocation, PlayColor);
      Win32DrawSoundBuffer(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputLocation + ThisMarker->OutputByteCount, WriteColor);

      Top += PadY + LineHeight;
      Bottom += PadY + LineHeight;

      Win32DrawSoundBuffer(BackBuffer, SoundOutput, C, PadX, FirstTop, Bottom, ThisMarker->ExpectedFlipPlayCursor, ExpectedFlipColor);
    }
    Assert(ThisMarker->FlipPlayCursor < SoundOutput->SecondaryBufferSize);
    Win32DrawSoundBuffer(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipPlayCursor, PlayColor);
    Win32DrawSoundBuffer(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipPlayCursor + 480*SoundOutput->BytesPerSample, PlayWindowColor);
    Win32DrawSoundBuffer(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipWriteCursor, WriteColor);
  }
}

/**
 * Main du programme qui va initialiser la fen�tre et g�rer la boucle principale :
 * attente des messages, gestion de la manette et du clavier, dessin...
 **/
int CALLBACK
WinMain(HINSTANCE Instance,
        HINSTANCE PrevInstance,
        LPSTR CommandLine,
        int ShowCode)
{
  win32_game_code Game = Win32LoadGameCode();

  // QueryPerformanceFrequency va permettre de conna�tre la fr�quence associ�e � QueryPerformanceCounter
  // et nous permettre d'avoir le nombre d'images par seconde.
  LARGE_INTEGER PerfCountFrequencyResult;
  QueryPerformanceFrequency(&PerfCountFrequencyResult);
  GlobalPerfCountFrequency = PerfCountFrequencyResult.QuadPart;

  // On d�finit la granularit� du scheduler de Windows � 1ms pour permettre le calcul du timing
  // Pour que la fonction Sleep() soit plus performante (plus granulaire)
  UINT DesiredSchedulerMS = 1;
  bool32 SleepIsGranular = (timeBeginPeriod(DesiredSchedulerMS) == TIMERR_NOERROR);

  // On essaye de charger les fonctions de la dll qui g�re les manettes
  Win32LoadXInput();

  // Cr�ation de la fen�tre principale
  // initialisation par d�faut, ANSI version de WNDCLASSA
  WNDCLASSA WindowClass = {};

  Win32ResizeDIBSection(&GlobalBackBuffer, 800, 600);

  // On ne configure que les membres que l'on veut
  // indique que l'on veut rafraichir la fen�tre enti�re lors d'un resize (horizontal et vertical)
  WindowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
  WindowClass.lpfnWndProc = Win32MainWindowCallback;
  WindowClass.hInstance = Instance;
  // WindowClass.hIcon;
  WindowClass.lpszClassName = "FaitmainHerosWindowClass"; // nom pour retrouver la fen�tre

  // Calcul de la dur�e de calcul d'une image en fonction du taux de rafra�chissement de l'�cran
  // TODO: Demander � Windows la vraie valeur
#define MonitorRefreshHz 60
#define GameUpdateHz (MonitorRefreshHz / 2)
  real32 TargetSecondsPerFrame = 1.0f / (real32)GameUpdateHz;

  // Ouverture de la fen�tre
  if (RegisterClassA(&WindowClass))
  {
    // ANSI version de CreateWindowEx
    HWND Window = CreateWindowExA(0, // dwExStyle : options de la fen�tre
                                  WindowClass.lpszClassName,
                                  "FaitmainHeros",
                                  WS_OVERLAPPEDWINDOW | WS_VISIBLE, //dwStyle : overlapped window, visible par d�faut
                                  CW_USEDEFAULT, // X
                                  CW_USEDEFAULT, // Y
                                  CW_USEDEFAULT, // nWidth
                                  CW_USEDEFAULT, // nHeight
                                  0, // hWndParent : 0 pour dire que c'est une fen�tre top
                                  0, // hMenu : 0 pour dire pas de menu
                                  Instance,
                                  0); // Pas de passage de param�tres � la fen�tre
    if (Window)
    {
      // Comme on a sp�cifi� CS_OWNDC on peut initialiser un seul HDC
      // et s'en servir ind�finiment car on ne le partage pas
      HDC DeviceContext = GetDC(Window);

      // Initialisation de DirectSound et test de son
      // Pour le moment on a un buffer d'une seconde, on verra si �a suffit plus tard
      win32_sound_output SoundOutput = {};
      SoundOutput.SamplesPerSecond = 48000;
      SoundOutput.ToneVolume = 6000;
      SoundOutput.RunningSampleIndex = 0;
      SoundOutput.BytesPerSample = sizeof(uint16) * 2;
      SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;
      SoundOutput.LatencySampleCount = 3 * (SoundOutput.SamplesPerSecond / GameUpdateHz); // On aimerait 60 comme le nb img/s, 2* pour prendre de l'avance
      SoundOutput.SafetyBytes = (SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample / GameUpdateHz) / 3;

      Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
      // Premi�r remplissage du buffer pour le son
      Win32ClearSoundBuffer(&SoundOutput);
      // On lance la lecture du buffer pour le son
      GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

      // On utilise QueryPerformanceCounter pour mesurer le nombre d'images par seconde
      LARGE_INTEGER LastCounter;
      QueryPerformanceCounter(&LastCounter);

      // On alloue en une seule fois un buffer qui va �tre utilis� pour passer le son
      int16 *Samples = (int16*)VirtualAlloc(0,
                                            SoundOutput.SecondaryBufferSize,
                                            MEM_RESERVE|MEM_COMMIT,
                                            PAGE_READWRITE);


#if FAITMAIN_INTERNAL
      LPVOID BaseAddress = 0;
#else
      LPVOID BaseAddress = (LPVOID)Terabytes(2);
#endif

      // On alloue la m�moire utilis�e par le moteur du jeu en une fois
      game_memory GameMemory = {};
      GameMemory.PermanentStorageSize = Megabytes(64);
      GameMemory.TransientStorageSize = Gigabytes(1);
      uint64 TotalSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;
      GameMemory.PermanentStorage = VirtualAlloc(BaseAddress,
                                                 (SIZE_T)GameMemory.PermanentStorageSize,
                                                 MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
      GameMemory.TransientStorage = ((uint8 *)GameMemory.PermanentStorage + 
                                      GameMemory.PermanentStorageSize);
      /*
      GameMemory.TransientStorage = VirtualAlloc(0,
                                                 GameMemory.TransientStorageSize,
                                                 MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
      */
      // Si on ne peut pas allouer la m�moire r�serv�e ce n'est pas la peine de lancer le jeu
      if (Samples && GameMemory.PermanentStorage && GameMemory.TransientStorage)
      {
        // Gestion des entr�es
        game_input Input[2] = {};
        game_input *NewInput = &Input[0];
        game_input *OldInput = &Input[1];

        // Gestion du timing
        LARGE_INTEGER LastCounter = Win32GetWallClock();
        LARGE_INTEGER FlipWallClock = Win32GetWallClock();

        // Pour le debug de la syncro audio
        int DebugTimeMarkerIndex = 0;
        win32_debug_time_marker DebugTimeMarkers[GameUpdateHz / 2] = {0};

        DWORD AudioLatencyBytes = 0;
        real32 AudioLatencySeconds = 0;
        bool32 SoundIsValid = false;

        // rdtsc ne sert que pour le profiling, ne peut pas servir au timing
        uint64 LastCycleCount = __rdtsc();

        // boucle infinie pour traiter tous les messages et tout passer au moteur de jeu
        while (GlobalRunning)
        {
          game_controller_input *OldKeyboardController = GetController(OldInput, 0);
          game_controller_input *NewKeyboardController = GetController(NewInput, 0);
          game_controller_input ZeroController = {};
          *NewKeyboardController = ZeroController;
          NewKeyboardController->IsConnected = true;
          // On retient l'�tat pr�c�dent des boutons pour g�rer les boutons appuy�s longtemps
          for (int ButtonIndex = 0;
               ButtonIndex < ArrayCount(OldKeyboardController->Buttons);
               ++ButtonIndex)
          {
            NewKeyboardController->Buttons[ButtonIndex].EndedDown =
              OldKeyboardController->Buttons[ButtonIndex].EndedDown;
          }
          Win32ProcessPendingMessages(NewKeyboardController);

          // Gestion des entr�es, pour le moment on g�re �a � chaque image
          // il faudra peut-�tre le faire plus fr�quemment
          // surtout si le nombre d'images par seconde chute
          int MaxControllerCount = XUSER_MAX_COUNT; // On va g�rer le clavier comme �tant le controller 0
          if (MaxControllerCount > (ArrayCount(NewInput->Controllers) - 1))
          {
            MaxControllerCount = ArrayCount(NewInput->Controllers) - 1;
          }
          for (int ControllerIndex = 0;
               ControllerIndex < MaxControllerCount;
               ++ControllerIndex)
          {
            int OurControllerIndex = ControllerIndex + 1; // On r�serve l'emplacement 0 pour le clavier
            game_controller_input *OldController = GetController(OldInput, OurControllerIndex);
            game_controller_input *NewController = GetController(NewInput, OurControllerIndex);

            XINPUT_STATE ControllerState;
            if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
            {
              // Le controller est branch�
              NewController->IsConnected = true;
              XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

              // Stick
              NewController->IsAnalog = true;
              NewController->StickAverageX = Win32ProcessXInputStickValue(
                Pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
              NewController->StickAverageY = Win32ProcessXInputStickValue(
                Pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

              // DPAD, que l'on peut traiter comme le stick
              bool32 Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
              bool32 Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
              bool32 Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
              bool32 Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);

              if (Up) NewController->StickAverageY = 1.0f;
              if (Down) NewController->StickAverageY = -1.0f;
              if (Left) NewController->StickAverageX = -1.0f;
              if (Right) NewController->StickAverageX = 1.0f;

              // Si un bouton est press� au lieu du stick on dit qu'on n'est pas analogue, sinon si le stick est utilis� on pr�vient
              if(Up || Down || Left || Right) NewController->IsAnalog = false;
              if(NewController->StickAverageX != 0.0f || NewController->StickAverageY != 0.0f) NewController->IsAnalog = true;

              // Si on veut consid�rer le stick comme un bouton
              real32 Threshold = 0.5f;
              WORD VirtualLeft = NewController->StickAverageX < -Threshold ? 1 : 0;
              WORD VirtualRight = NewController->StickAverageX > Threshold ? 1 : 0;
              WORD VirtualUp = NewController->StickAverageY < -Threshold ? 1 : 0;
              WORD VirtualDown = NewController->StickAverageY > Threshold ? 1 : 0;
              Win32ProcessXInputDigitalButton(
                VirtualLeft, &OldController->MoveLeft, 1,
                &NewController->MoveLeft);
              Win32ProcessXInputDigitalButton(
                VirtualRight, &OldController->MoveRight, 1,
                &NewController->MoveRight);
              Win32ProcessXInputDigitalButton(
                VirtualUp, &OldController->MoveUp, 1,
                &NewController->MoveUp);
              Win32ProcessXInputDigitalButton(
                VirtualDown, &OldController->MoveDown, 1,
                &NewController->MoveDown);

              // Boutons
              Win32ProcessXInputDigitalButton(
                Pad->wButtons, &OldController->A,
                XINPUT_GAMEPAD_A, &NewController->A);
              Win32ProcessXInputDigitalButton(
                Pad->wButtons, &OldController->B,
                XINPUT_GAMEPAD_B, &NewController->B);
              Win32ProcessXInputDigitalButton(
                Pad->wButtons, &OldController->X,
                XINPUT_GAMEPAD_X, &NewController->X);
              Win32ProcessXInputDigitalButton(
                Pad->wButtons, &OldController->Y,
                XINPUT_GAMEPAD_Y, &NewController->Y);
              Win32ProcessXInputDigitalButton(
                Pad->wButtons, &OldController->LeftShoulder,
                XINPUT_GAMEPAD_LEFT_SHOULDER, &NewController->LeftShoulder);
              Win32ProcessXInputDigitalButton(
                Pad->wButtons, &OldController->RightShoulder,
                XINPUT_GAMEPAD_RIGHT_SHOULDER, &NewController->RightShoulder);
              Win32ProcessXInputDigitalButton(
                Pad->wButtons, &OldController->Back,
                XINPUT_GAMEPAD_BACK, &NewController->Back);
              Win32ProcessXInputDigitalButton(
                Pad->wButtons, &OldController->Start,
                XINPUT_GAMEPAD_START, &NewController->Start);

              // Vibration de la manette
              XINPUT_VIBRATION Vibration;
              if (Left)
              {
                Vibration.wLeftMotorSpeed = 60000;
                Vibration.wRightMotorSpeed = 60000;
              }
              else
              {
                Vibration.wLeftMotorSpeed = 0;
                Vibration.wRightMotorSpeed = 0;
              }
              XInputSetState(0, &Vibration);
            }
            else
            {
              // Le controlleur n'est pas branch�
              NewController->IsConnected = false;
            }
          }
          
          // Gestion de la pause
          if(!GlobalPause)
          {
            // Passage du back buffer pour dessiner
            game_offscreen_buffer Buffer = {};
            Buffer.Memory = GlobalBackBuffer.Memory;
            Buffer.Width = GlobalBackBuffer.Width;
            Buffer.Height = GlobalBackBuffer.Height;
            Buffer.Pitch = GlobalBackBuffer.Pitch;

            // On demande au moteur de jeu de g�n�rer les graphismes et le son
            Game.UpdateAndRender(&GameMemory, NewInput, &Buffer);

            LARGE_INTEGER AudioWallClock = Win32GetWallClock();
            real32 FromBeginToAudioSeconds = Win32GetSecondsElapsed(FlipWallClock, AudioWallClock);

            DWORD PlayCursor;
            DWORD WriteCursor;
            if(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK)
            {
              /* NOTE : Comment fonctionne la syncro audio ?

                 On d�finit une marge de s�curit� de samples correspondant � la
                 variation de framerate.
             
                 Lorque l'on doit �crire le buffer audio, on regarde o� se situe le
                 curseur de lecture, et on estime o� il se trouvera lorsque le calcul
                 de la prochaine image d�butera.

                 On regarde alors si le curseur d'�criture se trouvera avant cette
                 position avec la marge de s�curit�. Si c'est le cas alors alors la
                 cible sera cette position plus une image. C'est le cas d'une syncro
                 audio parfaite avec une carte son de faible latence.

                 Si le curseur d'�criture se trouvera apr�s le d�but de calcul de la
                 prochaine image alors on assume de ne pas pouvoir avoir une syncro
                 parfaite, et on �crira une 'image' d'audio plus tard, plus quelques
                 samples en plus par s�curit� (marge de s�curit� d'environ 1ms ou plus),
                 quelque soit les variations de framerate.
              */
              if(!SoundIsValid)
              {
                SoundOutput.RunningSampleIndex = WriteCursor / SoundOutput.BytesPerSample;
                SoundIsValid = true;
              }
              // On remplit le buffer secondaire en fonction d'o� se trouve le curseur de lecture
              // ce qui m�riterait une meilleure gestion de l'�tat de lecture (lower latency offset)
              // Forme d'un sample :  int16 int16   int16 int16   int16 int16 ...
              //                     (LEFT  RIGHT) (LEFT  RIGHT) (LEFT  RIGHT)...
              DWORD ByteToLock = (SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample)
                                  % SoundOutput.SecondaryBufferSize;
            
              DWORD ExpectedSoundBytesPerFrame = (SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample) /
                                                  GameUpdateHz;
              real32 SecondsLeftUntilFlip = (TargetSecondsPerFrame - FromBeginToAudioSeconds);
              DWORD ExpectedBytesUntilFlip = (DWORD)((SecondsLeftUntilFlip/TargetSecondsPerFrame)*(real32)ExpectedSoundBytesPerFrame);
              DWORD ExpectedFrameBoundaryByte = PlayCursor + ExpectedSoundBytesPerFrame;

              DWORD SafeWriteCursor = WriteCursor;
              if(SafeWriteCursor < PlayCursor)
              {
                SafeWriteCursor += SoundOutput.SecondaryBufferSize;
              }
              Assert(SafeWriteCursor >= PlayCursor);
              SafeWriteCursor += SoundOutput.SafetyBytes;
              bool32 AudioCardIsLowLatency = (SafeWriteCursor < ExpectedFrameBoundaryByte);

              DWORD TargetCursor = 0;
              if(AudioCardIsLowLatency)
              {
                TargetCursor = ExpectedFrameBoundaryByte + ExpectedSoundBytesPerFrame;
              }
              else
              {
                TargetCursor = WriteCursor + ExpectedSoundBytesPerFrame + SoundOutput.SafetyBytes;
              }
              TargetCursor = TargetCursor % SoundOutput.SecondaryBufferSize;

              DWORD BytesToWrite = 0;
              if (ByteToLock > TargetCursor)
              {
                BytesToWrite = SoundOutput.SecondaryBufferSize - ByteToLock;
                BytesToWrite += TargetCursor;
              }
              else
              {
                BytesToWrite = TargetCursor - ByteToLock;
              }

              // R�cup�ration du buffer audio depuis le moteur de jeu
              game_sound_output_buffer SoundBuffer = {};
              SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
              SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
              SoundBuffer.Samples = Samples;
              Game.GetSoundSamples(&GameMemory, &SoundBuffer);

  #if FAITMAIN_INTERNAL
              win32_debug_time_marker *Marker = &DebugTimeMarkers[DebugTimeMarkerIndex];
              Marker->OutputPlayCursor = PlayCursor;
              Marker->OutputWriteCursor = WriteCursor;
              Marker->OutputLocation = ByteToLock;
              Marker->OutputByteCount = BytesToWrite;
              Marker->ExpectedFlipPlayCursor = ExpectedFrameBoundaryByte;

              // c'est un buffer circulaire, c'est plus compliqu� pour mesurer l'�cart
              DWORD UnwrappedWriteCursor = WriteCursor;
              if(UnwrappedWriteCursor < PlayCursor)
              {
                UnwrappedWriteCursor += SoundOutput.SecondaryBufferSize;
              }
              AudioLatencyBytes = UnwrappedWriteCursor - PlayCursor;
              AudioLatencySeconds = (((real32)AudioLatencyBytes /
                                    (real32)SoundOutput.BytesPerSample) /
                                    (real32)SoundOutput.SamplesPerSecond);

              // Une sortie debug pour v�rifier le son
              char TextBuffer[256];
              _snprintf_s(
                TextBuffer,
                sizeof(TextBuffer),
                "BTL:%u TC:%u BTW:%u - PC:%u WC:%u DELTA:%u (%fs)\n",
                ByteToLock, TargetCursor,
                BytesToWrite, PlayCursor, WriteCursor,
                AudioLatencyBytes, AudioLatencySeconds);
              OutputDebugStringA(TextBuffer);
  #endif
              Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
            }
            else
            {
              SoundIsValid = false;
            }

            // Timing entre les images pour assurer un FPS constant
            LARGE_INTEGER WorkCounter = Win32GetWallClock();
            real32 WorkSecondsElapsed = Win32GetSecondsElapsed(LastCounter, WorkCounter);
          
            real32 SecondsElapsedForFrame = WorkSecondsElapsed;
            if (SecondsElapsedForFrame < TargetSecondsPerFrame)
            {
              if (SleepIsGranular)
              {
                DWORD SleepMS = (DWORD)(1000.0f * (TargetSecondsPerFrame - SecondsElapsedForFrame));
                if (SleepMS > 0) Sleep(SleepMS);
              }
              // On v�rifie que l'on n'a pas dormi trop longtemps...
              real32 TestSecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter,
                                                                         Win32GetWallClock());
              if(TestSecondsElapsedForFrame > TargetSecondsPerFrame)
              {
                // On loguera le probl�me de sommeil ;)
              }
              while(SecondsElapsedForFrame < TargetSecondsPerFrame)
              {
                SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter,
                                                                Win32GetWallClock());
              }
            }
            else
            {
              // Probl�me de timing, qu'il faudra logguer
            }

            // Remplacement du compteur d'images pour le timing
            LARGE_INTEGER EndCounter = Win32GetWallClock();
            real32 MSPerFrame = 1000.0f * Win32GetSecondsElapsed(LastCounter, EndCounter);
            LastCounter = EndCounter;

            // On doit alors �crire dans la fen�tre � chaque fois que l'on veut rendre
            // On en fera une fonction propre
            win32_window_dimension Dimension = Win32GetWindowDimension(Window);
  #if FAITMAIN_INTERNAL
            Win32DebugSyncDisplay(
              &GlobalBackBuffer,
              ArrayCount(DebugTimeMarkers),
              DebugTimeMarkers,
              DebugTimeMarkerIndex - 1, // Faux � l'index 0 mais ce n'est pas grave pour le moment
              &SoundOutput,
              TargetSecondsPerFrame);
  #endif
            Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext,
                                       Dimension.Width, Dimension.Height);
          
            // PROBLEME avec ce RealeaseDC, � v�rifier
            // ReleaseDC(Window, DeviceContext);

            LARGE_INTEGER FlipWallClock = Win32GetWallClock();

            // On regarde la syncro audio en mode debug
  #if FAITMAIN_INTERNAL
            {
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
            ++DebugTimeMarkerIndex;
            if(DebugTimeMarkerIndex >= ArrayCount(DebugTimeMarkers))
            {
              DebugTimeMarkerIndex = 0;
            }
  #endif

            // Mesure du nombre d'images par seconde
            uint64 EndCycleCount = __rdtsc();
            uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
            LastCycleCount = EndCycleCount;

    #if 1
            real32 FPS = 0.0f; //(real32)PerfCountFrequency / (real32)CounterElapsed;
            real32 MCPF = (real32)CyclesElapsed / 1000000.0f;
  
            char FPSBuffer[256];
            _snprintf_s(
              FPSBuffer,
              sizeof(FPSBuffer),
              "%0.2f ms/f, %0.2f f/s, %0.2f Mc/f\n",
              MSPerFrame,
              FPS,
              MCPF);
            OutputDebugStringA(FPSBuffer);
    #endif
          } // Fin GlobalPause
          
          // Gestion des entr�es
          game_input *Temp = NewInput;
          NewInput = OldInput;
          OldInput = Temp;
        }
      }
      else
      {
        OutputDebugStringA("Error: Memory not allocated\n");
      }
    }
    else
    {
      OutputDebugStringA("Error: CreateWindowEx\n");
    }
  }
  else
  {
    OutputDebugStringA("Error: RegisterClass\n");
  }

  return(0);
};