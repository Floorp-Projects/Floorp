#include <windows.h>
#include <stdlib.h>
#include <tchar.h>
#include <stdio.h>

int WINAPI
WinMain(HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPTSTR lpCmdLine,
    int nCmdShow)
{
  PROCESS_INFORMATION pi;
  STARTUPINFOW si = { 0 };
  wchar_t modfilename[MAX_PATH];
  wchar_t execname[MAX_PATH];
  wchar_t args[MAX_PATH];
  wcscpy(args, L"-nosplash -faststart-hidden");

  HMODULE mod = GetModuleHandle(NULL);
  GetModuleFileNameW(mod, modfilename, sizeof(modfilename)/sizeof(modfilename[0]));
  wchar_t *chomp = wcsstr(modfilename, L"faststart.exe");
  if (!chomp) {
      MessageBoxW(NULL, L"Couldn't figure out how to run the faststart service!", L"Faststart Fail", MB_OK);
	  return 0;
  }

  size_t len = chomp - modfilename;
  wcsncpy(execname, modfilename, len);
  execname[len] = 0;
  wcscat(execname, L".exe");

  BOOL ok = CreateProcessW(execname, args, NULL, NULL, FALSE, 0,
                           NULL, NULL, &si, &pi);
  if (!ok) {
      MessageBoxW(NULL, L"Couldn't figure out how to run the faststart service!", L"Faststart Fail", MB_OK);
	  return 0;
  }
  return 0;
}
