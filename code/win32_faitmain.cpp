#include <stdint.h> // Types indépendents de la plateforme
#include <math.h>   // Fonctions mathématiques

// Pour bien comprendre la différence de fonctionnement des variables statiques en C en fonction du scope
#define internal static // fonctions non visible depuis l'extérieur de ce fichier
#define local_persist static     // variable visibles juste dans le scope où elle définie
#define global_variable static   // variable visible dans tous le fichiers (globale)

// Constantes
#define PI32 3.14159265358979323846
#define XUSER_MAX_COUNT 4 // Normalement définie dans Xinput.h, absente de VS2010

// Quelques définitions de types d'entiers pour ne pas être dépendant de la plateforme
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

// Implémentation du coeur du jeu indépendemment de la plateforme
#include "faitmain.cpp"

// Includes spécifiques à la plateforme
#include <Windows.h>
#include <Xinput.h> // Pour la gestion des entrées (manette...)
#include <dsound.h> // Pour jouer du son avec DirectSound
#include <stdio.h>
#include "win32_faitmain.h"

// variables globales pour le moment, on gèrera autrement plus tard
global_variable bool32 GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;

// Permet de renvoyer les dimensions actuelles de la fenêtre
internal win32_window_dimension
Win32GetWindowDimension(HWND Window) {
  win32_window_dimension Result;
  RECT ClientRect;
  GetClientRect(Window, &ClientRect);
  Result.Width = ClientRect.right - ClientRect.left;
  Result.Height = ClientRect.bottom - ClientRect.top;
  return(Result);
}

// Ici on définit des pointeurs vers les fonctions de Xinput
// Cette technique traditionnelle permet d'utiliser des fonctions
// Sans linker directement la lib, et permet aussi de tester si la lib est présente
// On utilise alors des macros pour définir la signature des fonctions
// Ici on définit deux fonctions qui vont se subsituer aux vraies si la lib n'est pas trouvée
// D'abord pour XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub) {
  return(ERROR_DEVICE_NOT_AVAILABLE); // Au lieu de renvoyer 0 (ERROR_SUCCESS) on renvoit un code plus explicite
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// De même pour XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub) {
  return(ERROR_DEVICE_NOT_AVAILABLE);
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

// On va alors charger la dll et pointer vers ses fonctions
// Soit la dll est présente sur le système ou alors il faut utiliser la lib redistributable
internal void
Win32LoadXInput(void) {
  // on essaye de charger la version 1.4 de xinput
  HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
  // on essaye une version un peu plus ancienne qui sera présente sur plus de machines si on n'a pas la 1.4
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
 * Implémentation des fonctions spécifiques à la plateforme
 * déclarées dans faitmain.h
 **/
internal debug_read_file_result
DEBUGPlatformReadEntireFile(char *Filename)
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
              (FileSize32 == BytesRead)) // On vérifie aussi si on a bien tout lu
        {
          // Le fichier a bien été lu
          Result.ContentsSize = FileSize32;
        }
        else
        {
          // On libère la mémoire immédiatement car le fichier n'a pas pu être ouvert
          DEBUGPlatformFreeFileMemory(Result.Contents);
          Result.Contents = 0;
        }
      }
    }
    CloseHandle(FileHandle);
  }
  return(Result);
}

internal void
DEBUGPlatformFreeFileMemory(void *Memory)
{
  if(Memory)
  {
    VirtualFree(Memory, 0, MEM_RELEASE);
  }
}

internal bool32
DEBUGPlatformWriteEntireFile(char *Filename, uint32 MemorySize, void *Memory)
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
      // Le fichier a bien été écrit
      Result = (BytesWritten == MemorySize);
    }
    else
    {

    }
    CloseHandle(FileHandle);
  }
  return(Result);
}

// On effectue de même pour les fonctions de DirectSound avec des stubs
// de fonctions si la dll n'a pas pu être chargée
#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuiDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

/**
 * Initialisation de DirectSound
 * On utilise deux buffers, celui qui joue vraiment le son est le secondaire
 * le primaire est une sorte de handle vers la carte son,
 * il joue le son sans resampling du buffer secondaire.
 * Auparavent on utilisait juste le premier buffer pour envoyer le son
 * directement à la carte son.
 * On utilise que 2 channels car on fait un jeu 2D qui n'a pas besoin
 * de positionnement, mais on pourrait le faire pour avoir du surround.
 **/
internal void
Win32InitDSound(HWND Window, uint32 SamplesPerSecond, uint32 BufferSize)
{
  // Chargement de la librairie
  HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");
  if (DSoundLibrary) {
    // Obtention d'un objet DirectSound - mode coopératif
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

      // Définition du mode de coopération
      if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
      {
        // Création d'un buffer principal
        // Astuce pout initialiser tous ses membres à 0
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
      // Création d'un buffer secondaire qui va contenir les sons
      // Astuce pout initialiser tous ses membres à 0
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
 * Fonction qui permet de définir et de réinitialiser un backbuffer en fonction de ses dimensions
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
  // Attention au sens des coordonnées, du bas vers le haut (d'où le moins)
  Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
  Buffer->Info.bmiHeader.biPlanes = 1;
  Buffer->Info.bmiHeader.biBitCount = 32;
  Buffer->Info.bmiHeader.biCompression = BI_RGB;

  int BitmapMemorySize = (Buffer->Width * Buffer->Height) * Buffer->BytesPerPixel;
  // MEM_COMMIT réserve automatiquement la mémoire en théorie
  // mais il vaut mieux lui dire explicitement MEM_RESERVE
  Buffer->Memory = VirtualAlloc(0, BitmapMemorySize,
                                MEM_RESERVE | MEM_COMMIT,
                                PAGE_READWRITE); // cf. aussi HeapAlloc

  Buffer->Pitch = Width * Buffer->BytesPerPixel;
}

/**
 * Ici au début on passait ClientRect par référence avec un pointeur (*ClientRect)
 * cependant comme la structure est petite le passer par valeur est suffisant
 **/
internal void
Win32DisplayBufferInWindow(win32_offscreen_buffer *Buffer,
                           HDC DeviceContext,
                           int WindowWidth,
                           int WindowHeight)
{
  // copie d'un rectangle vers un autre (scaling si nécessaire, bit opérations...)
  StretchDIBits(DeviceContext,
                0, 0, WindowWidth, WindowHeight,
                0, 0, Buffer->Width, Buffer->Height,
                Buffer->Memory,
                &Buffer->Info,
                DIB_RGB_COLORS,
                SRCCOPY); // BitBlt: bit-block transfer of the color data => voir les autres modes dans la MSDN
}

/**
 * Callback de la fenêtre principale qui va traiter les messages renvoyés par Windows
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
        // Mais finalement on va gérer ça avec une variable globale pour le moment
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
        Assert(!"Une entrée au clavier est passée au travers !!!");
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
 * Gestion de l'état des bouttons d'une manette
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
 * Gestion de l'état des bouttons du clavier
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
    Result = (real32)Value / 32768.0f;
  else if (Value > DeadZoneThreshold)
    Result = (real32)Value / 32767.0f;
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
          // On vérifie les bits de LParam (cf. MSDN pour les valeurs)
          #define KeyMessageWasDownBit (1 << 30)
          #define KeyMessageIsDownBit (1 << 31)
          bool32 WasDown = ((Message.lParam & KeyMessageWasDownBit) != 0);
          bool32 IsDown = ((Message.lParam & KeyMessageIsDownBit) == 0);

          if (WasDown != IsDown) // Pour éviter les répétitions de touches lorsqu'elles sont enfoncées
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
          }
          // Comme on capture les touches il faut gérer nous même le Alt-F4 pour quitter
          // Normalement c'est DefWindowProc (en default) qui fait le boulot
          bool32 AltKeyWasDown = (Message.lParam & (1 << 29));
          if ((VKCode == VK_F4) && AltKeyWasDown)
          {
            GlobalRunning = false;
          }
        } break;
      default: // On dispatche les autres boutons au callback pour Windows
        {
          // On demande à Windows de traiter le message
          TranslateMessage(&Message);
          // Envoie le message au main WindowCallback, que l'on a défini et déclaré au dessus
          DispatchMessage(&Message);
        } break;
    }
  }
}

/**
 * Main du programme qui va initialiser la fenêtre et gérer la boucle principale :
 * attente des messages, gestion de la manette et du clavier, dessin...
 **/
int CALLBACK
WinMain(HINSTANCE Instance,
        HINSTANCE PrevInstance,
        LPSTR CommandLine,
        int ShowCode)
{
  // QueryPerformanceFrequency va permettre de connaître la fréquence associée à QueryPerformanceCounter
  // et nous permettre d'avoir le nombre d'images par seconde.
  LARGE_INTEGER PerfCountFrequencyResult;
  QueryPerformanceFrequency(&PerfCountFrequencyResult);
  int64 PerfCountFrequency = PerfCountFrequencyResult.QuadPart;

  uint64 LastCycleCount = __rdtsc();

  // On essaye de charger les fonctions de la dll qui gère les manettes
  Win32LoadXInput();

  // Création de la fenêtre principale
  // initialisation par défaut, ANSI version de WNDCLASSA
  WNDCLASSA WindowClass = {};

  Win32ResizeDIBSection(&GlobalBackBuffer, 800, 600);

  // On ne configure que les membres que l'on veut
  // indique que l'on veut rafraichir la fenêtre entière lors d'un resize (horizontal et vertical)
  WindowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
  WindowClass.lpfnWndProc = Win32MainWindowCallback;
  WindowClass.hInstance = Instance;
  // WindowClass.hIcon;
  WindowClass.lpszClassName = "FaitmainHerosWindowClass"; // nom pour retrouver la fenêtre

  // Ouverture de la fenêtre
  if (RegisterClassA(&WindowClass))
  {
    // ANSI version de CreateWindowEx
    HWND Window = CreateWindowExA(0, // dwExStyle : options de la fenêtre
                                  WindowClass.lpszClassName,
                                  "FaitmainHeros",
                                  WS_OVERLAPPEDWINDOW | WS_VISIBLE, //dwStyle : overlapped window, visible par défaut
                                  CW_USEDEFAULT, // X
                                  CW_USEDEFAULT, // Y
                                  CW_USEDEFAULT, // nWidth
                                  CW_USEDEFAULT, // nHeight
                                  0, // hWndParent : 0 pour dire que c'est une fenêtre top
                                  0, // hMenu : 0 pour dire pas de menu
                                  Instance,
                                  0); // Pas de passage de paramètres à la fenêtre
    if (Window)
    {
      // Comme on a spécifié CS_OWNDC on peut initialiser un seul HDC
      // et s'en servir indéfiniment car on ne le partage pas
      HDC DeviceContext = GetDC(Window);

      // Initialisation de DirectSound et test de son
      // Pour le moment on a un buffer d'une seconde, on verra si ça suffit plus tard
      win32_sound_output SoundOutput = {};
      SoundOutput.SamplesPerSecond = 48000;
      SoundOutput.ToneVolume = 6000;
      SoundOutput.RunningSampleIndex = 0;
      SoundOutput.BytesPerSample = sizeof(uint16) * 2;
      SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;
      SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSecond / 15; // On aimerait 60 comme le nb img/s

      Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
      // Premièr remplissage du buffer pour le son
      Win32ClearSoundBuffer(&SoundOutput);
      // On lance la lecture du buffer pour le son
      GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

      // On utilise QueryPerformanceCounter pour mesurer le nombre d'images par seconde
      LARGE_INTEGER LastCounter;
      QueryPerformanceCounter(&LastCounter);

      // On alloue en une seule fois un buffer qui va être utilisé pour passer le son
      int16 *Samples = (int16*)VirtualAlloc(0,
                                            SoundOutput.SecondaryBufferSize,
                                            MEM_RESERVE|MEM_COMMIT,
                                            PAGE_READWRITE);


#if FAITMAIN_INTERNAL
      LPVOID BaseAddress = 0;
#else
      LPVOID BaseAddress = (LPVOID)Terabytes(2);
#endif

      // On alloue la mémoire utilisée par le moteur du jeu en une fois
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
      // Si on ne peut pas allouer la mémoire réservée ce n'est pas la peine de lancer le jeu
      if (Samples && GameMemory.PermanentStorage && GameMemory.TransientStorage)
      {
        // Gestion des entrées
        game_input Input[2] = {};
        game_input *NewInput = &Input[0];
        game_input *OldInput = &Input[1];

        GlobalRunning = true;
        // boucle infinie pour traiter tous les messages et tout passer au moteur de jeu
        while (GlobalRunning)
        {
          game_controller_input *OldKeyboardController = GetController(OldInput, 0);
          game_controller_input *NewKeyboardController = GetController(NewInput, 0);
          game_controller_input ZeroController = {};
          *NewKeyboardController = ZeroController;
          NewKeyboardController->IsConnected = true;
          // On retient l'état précédent des boutons pour gérer les boutons appuyés longtemps
          for (int ButtonIndex = 0;
               ButtonIndex < ArrayCount(OldKeyboardController->Buttons);
               ++ButtonIndex)
          {
            NewKeyboardController->Buttons[ButtonIndex].EndedDown =
              OldKeyboardController->Buttons[ButtonIndex].EndedDown;
          }
          Win32ProcessPendingMessages(NewKeyboardController);

          // Gestion des entrées, pour le moment on gère ça à chaque image
          // il faudra peut-être le faire plus fréquemment
          // surtout si le nombre d'images par seconde chute
          int MaxControllerCount = XUSER_MAX_COUNT; // On va gérer le clavier comme étant le controller 0
          if (MaxControllerCount > (ArrayCount(NewInput->Controllers) - 1))
          {
            MaxControllerCount = ArrayCount(NewInput->Controllers) - 1;
          }
          for (int ControllerIndex = 0;
               ControllerIndex < MaxControllerCount;
               ++ControllerIndex)
          {
            int OurControllerIndex = ControllerIndex + 1; // On réserve l'emplacement 0 pour le clavier
            game_controller_input *OldController = GetController(OldInput, OurControllerIndex);
            game_controller_input *NewController = GetController(NewInput, OurControllerIndex);

            XINPUT_STATE ControllerState;
            if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
            {
              // Le controller est branché
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

              // Si un bouton est pressé au lieu du stick on dit qu'on n'est pas analogue, sinon si le stick est utilisé on prévient
              if(Up || Down || Left || Right) NewController->IsAnalog = false;
              if(NewController->StickAverageX != 0.0f || NewController->StickAverageY != 0.0f) NewController->IsAnalog = true;

              // Si on veut considérer le stick comme un bouton
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
              // Le controlleur n'est pas branché
              NewController->IsConnected = false;
            }
          }

          // Test de rendu DirectSound
          // Forme d'un sample :  int16 int16   int16 int16   int16 int16 ...
          //                     (LEFT  RIGHT) (LEFT  RIGHT) (LEFT  RIGHT)...
          DWORD PlayCursor;
          DWORD WriteCursor;
          DWORD BytesToWrite;
          DWORD ByteToLock;
          DWORD TargetCursor;
          bool32 SoundIsValid = false;
          if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)))
          {
            // On remplit le buffer secondaire en fonction d'où se trouve le curseur de lecture
            // ce qui mériterait une meilleure gestion de l'état de lecture (lower latency offset)
            ByteToLock = (SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample)
                          % SoundOutput.SecondaryBufferSize;
            TargetCursor = (PlayCursor + (SoundOutput.LatencySampleCount * SoundOutput.BytesPerSample))
                            % SoundOutput.SecondaryBufferSize;

            if (ByteToLock > TargetCursor)
            {
              BytesToWrite = SoundOutput.SecondaryBufferSize - ByteToLock;
              BytesToWrite += TargetCursor;
            }
            else
            {
              BytesToWrite = TargetCursor - ByteToLock;
            }
            SoundIsValid = true;
          }

          // Grâce à PeekMessage on a tout le temps CPU que l'on veut et on peut calculer et rendre le jeu ici

          // Passage du back buffer pour dessiner
          game_offscreen_buffer Buffer = {};
          Buffer.Memory = GlobalBackBuffer.Memory;
          Buffer.Width = GlobalBackBuffer.Width;
          Buffer.Height = GlobalBackBuffer.Height;
          Buffer.Pitch = GlobalBackBuffer.Pitch;

          // Passage du buffer pour jouer du son
          game_sound_output_buffer SoundBuffer = {};
          SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
          SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
          SoundBuffer.Samples = Samples;

          GameUpdateAndRender(&GameMemory,
                              NewInput,
                              &Buffer,
                              &SoundBuffer);

          if (SoundIsValid)
          {
            Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
          }

          // On doit alors écrire dans la fenêtre à chaque fois que l'on veut rendre
          // On en fera une fonction propre
          win32_window_dimension Dimension = Win32GetWindowDimension(Window);
          Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext,
                                     Dimension.Width, Dimension.Height);
          ReleaseDC(Window, DeviceContext);

          // Mesure du nombre d'images par seconde
          LARGE_INTEGER EndCounter;
          QueryPerformanceCounter(&EndCounter);
          uint64 EndCycleCount = __rdtsc();
          uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
          int64 CounterElapsed = EndCounter.QuadPart - LastCounter.QuadPart;

          real32 MSPerFrame = (1000.0f*(real32)CounterElapsed) / (real32)PerfCountFrequency;
          real32 FPS = (real32)PerfCountFrequency / (real32)CounterElapsed;
          real32 MCPF = (real32)CyclesElapsed / 1000000.0f;
  #if 0
          char Buffer[256];
          sprintf(Buffer, "%0.2f ms/f, %0.2f f/s, %0.2f Mc/f\n", MSPerFrame, FPS, MCPF);
          OutputDebugStringA(Buffer);
  #endif
          // Remplacement du compteur d'images
          LastCounter = EndCounter;
          LastCycleCount = EndCycleCount;

          // Gestion des entrées
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