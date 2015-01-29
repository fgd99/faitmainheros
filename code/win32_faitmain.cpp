#include <Windows.h>
#include <stdint.h> // Types indépendent de la plateforme

// Pour bien comprendre la différence de fonctionnement des variables statiques en C en fonction du scope
#define internal static // fonctions non visible depuis l'extérieur de ce fichier
#define local_persist static     // variable visibles juste dans le scope où elle définie
#define global_variable static   // variable visible dans tous le fichiers (globale)

typedef unsigned char uint8;
typedef uint8_t uint8; // comme un unsigned char, un 8 bits
typedef int16_t uint16;
typedef int32_t uint32;
typedef int64_t uint64;

// variable globale pour le moment, on gèrera autrement plus tard
global_variable bool Running;
global_variable BITMAPINFO BitmapInfo;
global_variable void *BitmapMemory;
global_variable int BitmapWidth;
global_variable int BitmapHeight;
global_variable int BytesPerPixel = 4;

internal void
RenderWeirdGradient(int XOffset, int YOffset)
{
  int Width = BitmapWidth;
  int Height = BitmapHeight;

  int Pitch = Width * BytesPerPixel;  
  uint8 *Row = (uint8 *)BitmapMemory; // on va se déplacer dans la mémoire par pas de 8 bits
  for (int Y = 0; Y < BitmapHeight; ++Y)
  {
    uint32 *Pixel = (uint32 *)Row; // Pixel par pixel
    for (int X = 0; X < BitmapWidth; ++X)
    {
      /*
        Pixels en little endian architecture
                             0  1  2  3 ...
        Pixels en mémoire : 00 00 00 00 ...
        Couleur             BB GG RR xx
        en hexa: 0xxxRRGGBB
      */
      uint8 Blue = (X + XOffset);
      uint8 Green = (Y + YOffset);
      // *Pixel = 0xFF00FF00;
      *Pixel = ((Green << 8) | Blue); // ce qui équivaut en hexa à 0x00BBGG00
      ++Pixel; // Façon de faire si on prend pixel par pixel
    }
    Row += Pitch;
  }
}

/**
 * DIB: Device Independent Bitmap
 **/
internal void
Win32ResizeDIBSection(int Width, int Height)
{
  if (BitmapMemory)
  {
    VirtualFree(BitmapMemory, 0, MEM_RELEASE); // cf. VirtualProtect, utile pour debug
  }

  BitmapWidth = Width;
  BitmapHeight = Height;

  BitmapInfo.bmiHeader.biSize = sizeof(BitmapInfo.bmiHeader);
  BitmapInfo.bmiHeader.biWidth = BitmapWidth;
  BitmapInfo.bmiHeader.biHeight = -BitmapHeight; // Atention au sens des coordonnées
  BitmapInfo.bmiHeader.biPlanes = 1;
  BitmapInfo.bmiHeader.biBitCount = 32;
  BitmapInfo.bmiHeader.biCompression = BI_RGB;
  
  int BitmapMemorySize = (BitmapWidth * BitmapHeight) * BytesPerPixel;
  BitmapMemory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE); // cf. aussi HeapAlloc  
}

internal void
Win32UpdateWindow(HDC DeviceContext, RECT *ClientRect, int X, int Y, int Width, int Height)
{
  int WindowWidth = ClientRect->right - ClientRect->left;
  int WindowHeight = ClientRect->bottom - ClientRect->top;
  StretchDIBits( // copie d'un rectangle vers un autre (scaling si nécessaire, bit opérations...)
    DeviceContext,
    /*X, Y, Width, Height,
    X, Y, Width, Height,*/
    0, 0, BitmapWidth, BitmapHeight,
    0, 0, WindowWidth, WindowHeight,
    BitmapMemory,
    &BitmapInfo,
    DIB_RGB_COLORS,
    SRCCOPY // BitBlt: bit-block transfer of the color data => voir les autres modes dans la MSDN
  );
}

LRESULT CALLBACK
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
        RECT ClientRect;
        GetClientRect(Window, &ClientRect);
        int Width = ClientRect.right - ClientRect.left;
        int Height = ClientRect.bottom - ClientRect.top;
        Win32ResizeDIBSection(Width, Height);
        OutputDebugStringA("WM_SIZE\n");
      }
      break;
    case WM_DESTROY:
      {
        // PostQuitMessage(0); // Va permettre de sortir de la boucle infinie en dessous
        Running = false;
        OutputDebugStringA("WM_DESTROY\n");
      }
      break;
    case WM_CLOSE:
      {
        // DestroyWindow(Window);
        Running = false;
        OutputDebugStringA("WM_CLOSE\n");
      }
      break;
    case WM_ACTIVATEAPP:
      {
        OutputDebugStringA("WM_ACTIVATEAPP\n");
      }
      break;
    case WM_PAINT:
      {
        PAINTSTRUCT Paint;
        HDC DeviceContext = BeginPaint(Window, &Paint);
        int X = Paint.rcPaint.left;
        int Y = Paint.rcPaint.top;
        int Width = Paint.rcPaint.right - Paint.rcPaint.left;
        int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;

        RECT ClientRect;
        GetClientRect(Window, &ClientRect);

        Win32UpdateWindow(DeviceContext, &ClientRect, X, Y, Width, Height);
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

int CALLBACK
WinMain(
  HINSTANCE Instance,
  HINSTANCE PrevInstance,
  LPSTR CommandLine,
  int ShowCode)
{
  // Création de la fenêtre principale
  WNDCLASSA WindowClass = {}; // initialisation par défaut, ANSI version de WNDCLASSA
  // On ne configure que les membres que l'on veut
  WindowClass.style = CS_OWNDC|CS_HREDRAW|CS_VREDRAW;
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
      int XOffset = 0;
      int YOffset = 0;
      MSG Message;
      Running = true;
      while (Running) // boucle infinie pour traiter tous les messages
      {
        while(PeekMessageA(&Message, 0, 0, 0, PM_REMOVE)) // On utilise PeekMessage au lieu de GetMessage qui est bloquant
        {
          if (Message.message == WM_QUIT) Running = false;
          TranslateMessage(&Message); // On demande à Windows de traiter le message
          DispatchMessage(&Message); // Envoie le message au main WindowCallback, que l'on a défini et déclaré au dessus
        }
        
        // Grâce à PeekMessage on a tout le temps CPU que l'on veut et on peut dessiner ici
        RenderWeirdGradient(XOffset, YOffset);
        ++XOffset;

        // On doit alors écrire dans la fenêtre à chaque fois que l'on veut rendre
        // On en fera une fonction propre
        HDC DeviceContext = GetDC(Window);
        RECT ClientRect;
        GetClientRect(Window, &ClientRect);
        int WindowWidth = ClientRect.right - ClientRect.left;
        int WindowHeight = ClientRect.bottom - ClientRect.top;
        Win32UpdateWindow(DeviceContext, &ClientRect, 0, 0, WindowWidth, WindowHeight);
        ReleaseDC(Window, DeviceContext);
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