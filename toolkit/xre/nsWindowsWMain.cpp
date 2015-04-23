/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is a .cpp file meant to be included in nsBrowserApp.cpp and other
// similar bootstrap code. It converts wide-character windows wmain into UTF-8
// narrow-character strings.

#ifndef XP_WIN
#error This file only makes sense on Windows.
#endif

#include "nsUTF8Utils.h"

#ifndef XRE_DONT_PROTECT_DLL_LOAD
#include "nsSetDllDirectory.h"
#endif

#if defined(__GNUC__)
#define XRE_DONT_SUPPORT_XPSP2
#endif

#ifndef XRE_DONT_SUPPORT_XPSP2
#include "WindowsCrtPatch.h"
#endif

#ifdef __MINGW32__

/* MingW currently does not implement a wide version of the
   startup routines.  Workaround is to implement something like
   it ourselves.  See bug 411826 */

#include <shellapi.h>

int wmain(int argc, WCHAR **argv);

int main(int argc, char **argv)
{
  LPWSTR commandLine = GetCommandLineW();
  int argcw = 0;
  LPWSTR *argvw = CommandLineToArgvW(commandLine, &argcw);
  if (!argvw)
    return 127;

  int result = wmain(argcw, argvw);
  LocalFree(argvw);
  return result;
}
#endif /* __MINGW32__ */

#define main NS_internal_main

#ifndef XRE_WANT_ENVIRON
int main(int argc, char **argv);
#else
int main(int argc, char **argv, char **envp);
#endif

static char*
AllocConvertUTF16toUTF8(char16ptr_t arg)
{
  // be generous... UTF16 units can expand up to 3 UTF8 units
  int len = wcslen(arg);
  char *s = new char[len * 3 + 1];
  if (!s)
    return nullptr;

  ConvertUTF16toUTF8 convert(s);
  convert.write(arg, len);
  convert.write_terminator();
  return s;
}

static void
FreeAllocStrings(int argc, char **argv)
{
  while (argc) {
    --argc;
    delete [] argv[argc];
  }

  delete [] argv;
}

int wmain(int argc, WCHAR **argv)
{
#if !defined(XRE_DONT_SUPPORT_XPSP2)
  WindowsCrtPatch::Init();
#endif

#ifndef XRE_DONT_PROTECT_DLL_LOAD
  mozilla::SanitizeEnvironmentVariables();
  SetDllDirectoryW(L"");
#endif

  char **argvConverted = new char*[argc + 1];
  if (!argvConverted)
    return 127;

  for (int i = 0; i < argc; ++i) {
    argvConverted[i] = AllocConvertUTF16toUTF8(argv[i]);
    if (!argvConverted[i]) {
      return 127;
    }
  }
  argvConverted[argc] = nullptr;

  // need to save argvConverted copy for later deletion.
  char **deleteUs = new char*[argc+1];
  if (!deleteUs) {
    FreeAllocStrings(argc, argvConverted);
    return 127;
  }
  for (int i = 0; i < argc; i++)
    deleteUs[i] = argvConverted[i];
#ifndef XRE_WANT_ENVIRON
  int result = main(argc, argvConverted);
#else
  // Force creation of the multibyte _environ variable.
  getenv("PATH");
  int result = main(argc, argvConverted, _environ);
#endif

  delete[] argvConverted;
  FreeAllocStrings(argc, deleteUs);

  return result;
}
