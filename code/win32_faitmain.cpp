#include <Windows.h>

// Pour bien comprendre la différence de fonctionnement des variables statiques en C en fonction du scope
#define internal static // fonctions non visible depuis l'extérieur de ce fichier
#define local_persist static     // variable visibles juste dans le scope où elle définie
#define global_variable static   // variable visible dans tous le fichiers (globale)

// variable globale pour le moment, on gèrera autrement plus tard
global_variable bool Running;

/*
  DIB: Device Independent Bitmap
*/
internal void ResizeDIBSection() {
}

LRESULT CALLBACK MainWindowCallback(
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
        local_persist DWORD Operation = WHITENESS; // Une variable statique est pratique pour le debug, mais ce n'est pas thread safe et c'est une variable globale...
        PatBlt(DeviceContext, X, Y, Width, Height, Operation);
        if (Operation == WHITENESS)
          Operation = BLACKNESS;
        else
          Operation = WHITENESS;
        SetPixel(DeviceContext, 100, 100, RGB(255, 0, 255));
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

int CALLBACK WinMain(
  HINSTANCE Instance,
  HINSTANCE PrevInstance,
  LPSTR CommandLine,
  int ShowCode)
{
  // Création de la fenêtre principale
  WNDCLASSA WindowClass = {}; // initialisation par défaut, ANSI version de WNDCLASSA
  // On ne configure que les membres que l'on veut
  WindowClass.style = CS_OWNDC|CS_HREDRAW|CS_VREDRAW;
  WindowClass.lpfnWndProc = MainWindowCallback;
  WindowClass.hInstance = Instance;
  // WindowClass.hIcon;
  WindowClass.lpszClassName = "FaitmainHerosWindowClass"; // nom pour retrouver la fenêtre

  // Ouverture de la fenêtre
  if (RegisterClassA(&WindowClass))
  {
    HWND WindowHandle = CreateWindowExA( // ANSI version de CreateWindowEx
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
    if (WindowHandle) {
      MSG Message;
      Running = true;
      while (Running) { // boucle infinie pour traiter tous les messages
        BOOL MessageResult = GetMessage(&Message, 0, 0, 0); // On demande à Windows de nous donner le prochain message de la queue de message
        if (MessageResult > 0) {
          TranslateMessage(&Message); // On demande à Windows de traiter le message
          DispatchMessage(&Message); // Envoie le message au main WindowCallback, que l'on a défini et déclaré au dessus
        } else {
          break; // On arrête la boucle infinie en cas de problème, ou bien si PostQuitMessage(0) est appelé par exemple au dessus
          // On ne libère pas manuellement les ressources, comme la fenêtre par exemple,
          // car en quittant Windows va faire le ménage, et ce sera plus rapide visuellement pour
          // l'utilisateur (la fenêtre va se fermer immédiatement, sans temps mort).
        }
      }
    } else {
      OutputDebugStringA("Error: CreateWindowEx\n");
    }
  } else {
    OutputDebugStringA("Error: RegisterClass\n");
  }

  return(0);
};