#include <Windows.h>

int CALLBACK WinMain(
  HINSTANCE hInstance,
  HINSTANCE hPrevInstance,
  LPSTR lpCmdLine,
  int nCmdShow
) {
  MessageBox(
    0,
    "Ceci est FaitmainHeros.",
    "FaitmainHero",
    MB_OK | MB_ICONINFORMATION);
  return(0);
};