#include <windows.h>




int main(int argc, char *argv[])
{
  HWND h = FindWindowW(NULL, L"Minimo");
  if (h)
  {
    ShowWindow(h, SW_SHOWNORMAL);
    SetForegroundWindow(h);
    return 0;
  }

  char *cp;
  char exe[MAX_PATH];
  GetModuleFileName(GetModuleHandle(NULL), exe, sizeof(exe));
  cp = strrchr(exe,'\\');
  if (cp != NULL)
  {
    cp++; // pass the \ char.
    *cp = 0;
  }
  
  strcat(exe, "minimo.exe");

  PROCESS_INFORMATION pi;

  BOOL b = CreateProcess(exe,
                         NULL,
                         NULL,
                         NULL,
                         NULL,
                         0,
                         NULL,
                         NULL,
                         NULL,
                         &pi);
  return 0;
}

