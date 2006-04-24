#include <windows.h>

void SentURLLoadRequest(const char* url)
{
  COPYDATASTRUCT cds = { 0, ::strlen( url ) + 1, (void*)url };
  HWND a = FindWindowW(L"MINIMO_LISTENER", NULL);
  SendMessage(a, WM_COPYDATA, NULL, (LPARAM)&cds); 
}


int main(int argc, char *argv[])
{
  SetCursor(LoadCursor(NULL,IDC_WAIT));

  HWND h = FindWindowW(NULL, L"Minimo");
  if (!h)
  {
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
    CreateProcess(exe, NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL, &pi);
  }
  else
  {
    SetForegroundWindow(h);
    ShowWindow(h, SW_SHOWNORMAL);
  }

  if (argc == 3)
  {
    if (!strcmp("-url", argv[1]))
      SentURLLoadRequest(argv[2]);
  }

  SetCursor(NULL);
  return 0;
}

 
