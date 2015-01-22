#include <Windows.h>

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
        OutputDebugStringA("WM_DESTROY\n");
      }
      break;
    case WM_CLOSE:
      {
        OutputDebugStringA("WM_CLOSE\n");
      }
      break;
    case WM_ACTIVATEAPP:
      {
        OutputDebugStringA("WM_ACTIVATEAPP\n");
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
  // Cr�ation de la fen�tre principale
  WNDCLASS WindowClass = {}; // initialisation par d�faut
  // On ne configure que les membres que l'on veut
  WindowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
  WindowClass.lpfnWndProc = MainWindowCallback;
  WindowClass.hInstance = Instance;
  // WindowClass.hIcon;
  WindowClass.lpszClassName = (LPCWSTR)"FaitmainHerosWindowClass"; // nom pour retrouver la fen�tre

  // Ouverture de la fen�tre
  RegisterClass(&WindowClass);

  return(0);
};