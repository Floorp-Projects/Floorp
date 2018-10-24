/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Windows only app to show a modal debug dialog - launched by nsDebug.cpp */
#include <windows.h>
#include <stdlib.h>
#ifdef _MSC_VER
#include <strsafe.h>
#endif
#ifdef __MINGW32__
/* MingW currently does not implement a wide version of the
   startup routines.  Workaround is to implement something like
   it ourselves.  See bug 472063 */
#include <stdio.h>
#include <shellapi.h>
int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int);

#undef __argc
#undef __wargv

static int __argc;
static wchar_t** __wargv;

int WINAPI
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
        LPSTR lpszCommandLine, int nCmdShow)
{
  LPWSTR commandLine = GetCommandLineW();

  /* parse for __argc and __wargv for compatibility, since mingw
   * doesn't claim to support it :(
   */
  __wargv = CommandLineToArgvW(commandLine, &__argc);
  if (!__wargv)
    return 127;

  /* need to strip off any leading whitespace plus the first argument
   * (the executable itself) to match what should be passed to wWinMain
   */
  while ((*commandLine <= L' ') && *commandLine) {
    ++commandLine;
  }
  if (*commandLine == L'"') {
    ++commandLine;
    while ((*commandLine != L'"') && *commandLine) {
      ++commandLine;
    }
    if (*commandLine) {
      ++commandLine;
    }
  } else {
    while (*commandLine > L' ') {
      ++commandLine;
    }
  }
  while ((*commandLine <= L' ') && *commandLine) {
    ++commandLine;
  }

  int result = wWinMain(hInstance, hPrevInstance, commandLine, nCmdShow);
  LocalFree(__wargv);
  return result;
}
#endif /* __MINGW32__ */


int WINAPI
wWinMain(HINSTANCE  hInstance, HINSTANCE  hPrevInstance,
         LPWSTR  lpszCmdLine, int  nCmdShow)
{
  /* support for auto answering based on words in the assertion.
   * the assertion message is sent as a series of arguements (words) to the commandline.
   * set a "word" to 0xffffffff to let the word not affect this code.
   * set a "word" to 0xfffffffe to show the dialog.
   * set a "word" to 0x5 to ignore (program should continue).
   * set a "word" to 0x4 to retry (should fall into debugger).
   * set a "word" to 0x3 to abort (die).
   */
  DWORD regType;
  DWORD regValue = -1;
  DWORD regLength = sizeof regValue;
  HKEY hkeyCU, hkeyLM;
  RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\mozilla.org\\windbgdlg", 0, KEY_READ, &hkeyCU);
  RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\mozilla.org\\windbgdlg", 0, KEY_READ, &hkeyLM);
  int argc =0;
  for (int i = __argc - 1; regValue == (DWORD)-1 && i; --i) {
    bool ok = false;
    if (hkeyCU)
      ok = RegQueryValueExW(hkeyCU, __wargv[i], 0, &regType, (LPBYTE)&regValue, &regLength) == ERROR_SUCCESS;
    if (!ok && hkeyLM)
      ok = RegQueryValueExW(hkeyLM, __wargv[i], 0, &regType, (LPBYTE)&regValue, &regLength) == ERROR_SUCCESS;
    if (!ok)
      regValue = -1;
  }
  if (hkeyCU)
    RegCloseKey(hkeyCU);
  if (hkeyLM)
    RegCloseKey(hkeyLM);
  if (regValue != (DWORD)-1 && regValue != (DWORD)-2)
    return regValue;
  static const int size = 4096;
  static WCHAR msg[size];

#ifdef _MSC_VER
  StringCchPrintfW(msg,
#else
  snwprintf(msg,
#endif
            size,
            L"%s\n\nClick Abort to exit the Application.\n"
            L"Click Retry to Debug the Application.\n"
            L"Click Ignore to continue running the Application.",
            lpszCmdLine);
  msg[size - 1] = L'\0';
  return MessageBoxW(nullptr, msg, L"NSGlue_Assertion",
                     MB_ICONSTOP | MB_SYSTEMMODAL |
                     MB_ABORTRETRYIGNORE | MB_DEFBUTTON3);
}
