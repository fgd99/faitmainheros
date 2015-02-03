#include <Windows.h>
#include <stdint.h> // Types indépendents de la plateforme
#include <Xinput.h> // Pour la gestion des entrées (manette...)

// Pour bien comprendre la différence de fonctionnement des variables statiques en C en fonction du scope
#define internal static // fonctions non visible depuis l'extérieur de ce fichier
#define local_persist static     // variable visibles juste dans le scope où elle définie
#define global_variable static   // variable visible dans tous le fichiers (globale)

// Constantes
#define XUSER_MAX_COUNT 4 // Normalement définie dans Xinput.h, absente de VS2010

// Quelques définitions de types d'entiers pour ne pas être dépendant de la plateforme
typedef unsigned char uint8;
typedef uint8_t uint8; // comme un unsigned char, un 8 bits
typedef int16_t uint16;
typedef int32_t uint32;
typedef int64_t uint64;

// Struct qui représente un backbuffer qui nous permet de dessiner
struct win32_offscreen_buffer {
  BITMAPINFO Info;
  void *Memory;
  int Width;
  int Height;
  int BytesPerPixel;
  int Pitch; // Pitch représente la taille d'une ligne en octets
};

// variables globales pour le moment, on gèrera autrement plus tard
global_variable bool GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackBuffer;

// Struct qui représente des dimensions
struct win32_window_dimension
{
  int Width;
  int Height;
};

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
  return(0);
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// De même pour XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub) {
  return(0);
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

// On va alors charger la dll et pointer vers ses fonctions
internal void
Win32LoadXInput(void) {
  HMODULE XInputLibrary = LoadLibraryA("xinput1_3.dll"); // on essaye une version un peu plus ancienne qui sera présente sur plus de machines
  if (XInputLibrary)
  {
    XInputGetState_ = (x_input_get_state*)GetProcAddress(XInputLibrary, "XInputGetState");
    XInputSetState_ = (x_input_set_state*)GetProcAddress(XInputLibrary, "XInputSetState");
  }
}

/* Fonction qui va dessiner dans le backbuffer un gradient de couleur étrange */
internal void
RenderWeirdGradient(win32_offscreen_buffer *Buffer, int XOffset, int YOffset)
{
  uint8 *Row = (uint8 *)Buffer->Memory; // on va se déplacer dans la mémoire par pas de 8 bits
  for (int Y = 0; Y < Buffer->Height; ++Y)
  {
    uint32 *Pixel = (uint32 *)Row; // Pixel par pixel, on commence par le premier de la ligne
    for (int X = 0; X < Buffer->Width; ++X)
    {
      /*
        Pixels en little endian architecture
                             0  1  2  3 ...
        Pixels en mémoire : 00 00 00 00 ...
        Couleur             BB GG RR XX
        en hexa: 0xXXRRGGBB
      */
      uint8 Blue = (X + XOffset);
      uint8 Green = (Y + YOffset);
      uint8 Red = (X + Y);
      // *Pixel = 0xFF00FF00;
      *Pixel++ = ((Red << 16) | (Green << 8) | Blue); // ce qui équivaut en hexa à 0x00BBGG00
    }
    Row += Buffer->Pitch; // Ligne suivante
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
    VirtualFree(Buffer->Memory, 0, MEM_RELEASE); // cf. VirtualProtect, utile pour debug
  }

  Buffer->Width = Width;
  Buffer->Height = Height;
  Buffer->BytesPerPixel = 4;

  Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
  Buffer->Info.bmiHeader.biWidth = Buffer->Width;
  Buffer->Info.bmiHeader.biHeight = -Buffer->Height; // Attention au sens des coordonnées, du bas vers le haut (d'où le moins)
  Buffer->Info.bmiHeader.biPlanes = 1;
  Buffer->Info.bmiHeader.biBitCount = 32;
  Buffer->Info.bmiHeader.biCompression = BI_RGB;
  
  int BitmapMemorySize = (Buffer->Width * Buffer->Height) * Buffer->BytesPerPixel;
  Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE); // cf. aussi HeapAlloc

  Buffer->Pitch = Width * Buffer->BytesPerPixel;
}

/**
 * Ici au début on passait ClientRect par référence avec un pointeur (*ClientRect)
 * cependant comme la structure est petite le passer par valeur est suffisant
 **/
internal void
Win32DisplayBufferInWindow(
  win32_offscreen_buffer *Buffer,
  HDC DeviceContext,
  int WindowWidth,
  int WindowHeight)
{
  StretchDIBits( // copie d'un rectangle vers un autre (scaling si nécessaire, bit opérations...)
    DeviceContext,
    0, 0, WindowWidth, WindowHeight,
    0, 0, Buffer->Width, Buffer->Height,
    Buffer->Memory,
    &Buffer->Info,
    DIB_RGB_COLORS,
    SRCCOPY // BitBlt: bit-block transfer of the color data => voir les autres modes dans la MSDN
  );
}

/**
 * Callback de la fenêtre principale qui va traiter les messages renvoyés par Windows
 **/
internal LRESULT CALLBACK
Win32MainWindowCallback(
  HWND Window,
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
        // PostQuitMessage(0); // Va permettre de sortir de la boucle infinie en dessous
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
        uint32 VKCode = WParam;
        // On vérifie les bits de LParam (cf. MSDN pour les valeurs)
        bool WasDown = ((LParam & (1 << 30)) != 0);
        bool IsDown = ((LParam & (1 << 31)) == 0);

        if (WasDown != IsDown) // Pour éviter les répétitions de touches lorsqu'elles sont enfoncées
        {
          if (VKCode == 'Z')
          {
            OutputDebugStringA("Z\n");
          }
          else if (VKCode == 'S')
          {
            OutputDebugStringA("S\n");
          }
          else if (VKCode == 'Q')
          {
            OutputDebugStringA("Q\n");
          }
          else if (VKCode == 'D')
          {
            OutputDebugStringA("D\n");
          }
          else if (VKCode == VK_UP)
          {
            OutputDebugStringA("UP\n");
          }
          else if (VKCode == VK_DOWN)
          {
            OutputDebugStringA("DOWN\n");
          }
          else if (VKCode == VK_LEFT)
          {
            OutputDebugStringA("LEFT\n");
          }
          else if (VKCode == VK_RIGHT)
          {
            OutputDebugStringA("RIGHT\n");
          }
          else if (VKCode == VK_ESCAPE)
          {
            if (IsDown) OutputDebugStringA("ESCAPE IS DOWN\n");
            if (WasDown) OutputDebugStringA("ESCAPE WAS DOWN\n");
          }
          else if (VKCode == VK_SPACE)
          {
            OutputDebugStringA("SPACE\n");
          }
        }
      }
      break;
    case WM_PAINT:
      {
        PAINTSTRUCT Paint;
        HDC DeviceContext = BeginPaint(Window, &Paint);
        win32_window_dimension Dimension = Win32GetWindowDimension(Window);
        Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext, Dimension.Width, Dimension.Height);
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

/**
 * Main du programme qui va initialiser la fenêtre et gérer la boucle principale : attente des messages,
 * gestion de la manette et du clavier, dessin...
 **/
int CALLBACK
WinMain(
  HINSTANCE Instance,
  HINSTANCE PrevInstance,
  LPSTR CommandLine,
  int ShowCode)
{
  // On essaye de charger les fonctions de la dll qui gère les manettes
  Win32LoadXInput();

  // Création de la fenêtre principale
  WNDCLASSA WindowClass = {}; // initialisation par défaut, ANSI version de WNDCLASSA

  Win32ResizeDIBSection(&GlobalBackBuffer, 800, 600);

  // On ne configure que les membres que l'on veut
  WindowClass.style = CS_HREDRAW|CS_VREDRAW|CS_OWNDC; // indique que l'on veut rafraichir la fenêtre entière lors d'un resize (horizontal et vertical)
  WindowClass.lpfnWndProc = Win32MainWindowCallback;
  WindowClass.hInstance = Instance;
  // WindowClass.hIcon;
  WindowClass.lpszClassName = "FaitmainHerosWindowClass"; // nom pour retrouver la fenêtre

  // Ouverture de la fenêtre
  if (RegisterClassA(&WindowClass))
  {
    HWND Window = CreateWindowExA( // ANSI version de CreateWindowEx
      0, // dwExStyle : options de la fenêtre
      WindowClass.lpszClassName,
      "FaitmainHeros",
      WS_OVERLAPPEDWINDOW|WS_VISIBLE, //dwStyle : overlapped window, visible par défaut
      CW_USEDEFAULT, // X
      CW_USEDEFAULT, // Y
      CW_USEDEFAULT, // nWidth
      CW_USEDEFAULT, // nHeight
      0, // hWndParent : 0 pour dire que c'est une fenêtre top
      0, // hMenu : 0 pour dire pas de menu
      Instance,
      0 // Pas de passage de paramètres à la fenêtre
    );
    if (Window)
    {
      // Comme on a spécifié CS_OWNDC on peut initialiser un seul HDC
      // et s'en servir indéfiniment car on ne le partage pas
      HDC DeviceContext = GetDC(Window);

      int XOffset = 0;
      int YOffset = 0;
      GlobalRunning = true;
      
      while (GlobalRunning) // boucle infinie pour traiter tous les messages
      {
        MSG Message;
        while(PeekMessageA(&Message, 0, 0, 0, PM_REMOVE)) // On utilise PeekMessage au lieu de GetMessage qui est bloquant
        {
          if (Message.message == WM_QUIT) GlobalRunning = false;
          TranslateMessage(&Message); // On demande à Windows de traiter le message
          DispatchMessage(&Message); // Envoie le message au main WindowCallback, que l'on a défini et déclaré au dessus
        }

        // Gestion des entrées, pour le moment on gère ça à chaque image, il faudra peut-être le faire plus fréquemment
        // surtout si le nombre d'images par seconde chute
        for (DWORD ControllerIndex = 0; ControllerIndex < XUSER_MAX_COUNT; ++ControllerIndex)
        {
          XINPUT_STATE ControllerState;
          if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
          {
            // Le controller est branché
            XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

            bool Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
            bool Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
            bool Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
            bool Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
            bool Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
            bool Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
            bool LeftShoulder = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
            bool RightShoulder = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
            bool AButton = (Pad->wButtons & XINPUT_GAMEPAD_A);
            bool BButton = (Pad->wButtons & XINPUT_GAMEPAD_B);
            bool XButton = (Pad->wButtons & XINPUT_GAMEPAD_X);
            bool YButton = (Pad->wButtons & XINPUT_GAMEPAD_Y);

            uint16 StickX = Pad->sThumbLX;
            uint16 StickY = Pad->sThumbLY;

            // Test d'utilisation de la manette
            if (Up) YOffset += 2;
            if (Down) YOffset -= 2;
            if (Right) XOffset -= 4;

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
          }
        }

        // Grâce à PeekMessage on a tout le temps CPU que l'on veut et on peut dessiner ici
        RenderWeirdGradient(&GlobalBackBuffer, XOffset, YOffset);
        ++XOffset;

        // On doit alors écrire dans la fenêtre à chaque fois que l'on veut rendre
        // On en fera une fonction propre
        win32_window_dimension Dimension = Win32GetWindowDimension(Window);
        Win32DisplayBufferInWindow(
          &GlobalBackBuffer,
          DeviceContext,
          Dimension.Width, Dimension.Height);
        ReleaseDC(Window, DeviceContext);

        // Pour animer différemment le gradient
        ++XOffset;
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