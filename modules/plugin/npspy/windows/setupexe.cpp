#include <windows.h>
#include "setup.h"

int PASCAL WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
  SPY_Setup();
  return 0;
}
