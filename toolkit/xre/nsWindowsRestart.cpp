/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is not build directly. Instead, it is included in multiple
// shared objects.

#ifdef nsWindowsRestart_cpp
#error "nsWindowsRestart.cpp is not a header file, and must only be included once."
#else
#define nsWindowsRestart_cpp
#endif

#include "nsUTF8Utils.h"

#include <shellapi.h>

// Needed for CreateEnvironmentBlock
#include <userenv.h>
#pragma comment(lib, "userenv.lib")

/**
 * Get the length that the string will take and takes into account the
 * additional length if the string needs to be quoted and if characters need to
 * be escaped.
 */
static int ArgStrLen(const wchar_t *s)
{
  int backslashes = 0;
  int i = wcslen(s);
  BOOL hasDoubleQuote = wcschr(s, L'"') != nullptr;
  // Only add doublequotes if the string contains a space or a tab
  BOOL addDoubleQuotes = wcspbrk(s, L" \t") != nullptr;

  if (addDoubleQuotes) {
    i += 2; // initial and final duoblequote
  }

  if (hasDoubleQuote) {
    while (*s) {
      if (*s == '\\') {
        ++backslashes;
      } else {
        if (*s == '"') {
          // Escape the doublequote and all backslashes preceding the doublequote
          i += backslashes + 1;
        }

        backslashes = 0;
      }

      ++s;
    }
  }

  return i;
}

/**
 * Copy string "s" to string "d", quoting the argument as appropriate and
 * escaping doublequotes along with any backslashes that immediately precede
 * doublequotes.
 * The CRT parses this to retrieve the original argc/argv that we meant,
 * see STDARGV.C in the MSVC CRT sources.
 *
 * @return the end of the string
 */
static wchar_t* ArgToString(wchar_t *d, const wchar_t *s)
{
  int backslashes = 0;
  BOOL hasDoubleQuote = wcschr(s, L'"') != nullptr;
  // Only add doublequotes if the string contains a space or a tab
  BOOL addDoubleQuotes = wcspbrk(s, L" \t") != nullptr;

  if (addDoubleQuotes) {
    *d = '"'; // initial doublequote
    ++d;
  }

  if (hasDoubleQuote) {
    int i;
    while (*s) {
      if (*s == '\\') {
        ++backslashes;
      } else {
        if (*s == '"') {
          // Escape the doublequote and all backslashes preceding the doublequote
          for (i = 0; i <= backslashes; ++i) {
            *d = '\\';
            ++d;
          }
        }

        backslashes = 0;
      }

      *d = *s;
      ++d; ++s;
    }
  } else {
    wcscpy(d, s);
    d += wcslen(s);
  }

  if (addDoubleQuotes) {
    *d = '"'; // final doublequote
    ++d;
  }

  return d;
}

/**
 * Creates a command line from a list of arguments. The returned
 * string is allocated with "malloc" and should be "free"d.
 *
 * argv is UTF8
 */
wchar_t*
MakeCommandLine(int argc, wchar_t **argv)
{
  int i;
  int len = 0;

  // The + 1 of the last argument handles the allocation for null termination
  for (i = 0; i < argc; ++i)
    len += ArgStrLen(argv[i]) + 1;

  // Protect against callers that pass 0 arguments
  if (len == 0)
    len = 1;

  wchar_t *s = (wchar_t*) malloc(len * sizeof(wchar_t));
  if (!s)
    return nullptr;

  wchar_t *c = s;
  for (i = 0; i < argc; ++i) {
    c = ArgToString(c, argv[i]);
    if (i + 1 != argc) {
      *c = ' ';
      ++c;
    }
  }

  *c = '\0';

  return s;
}

/**
 * Convert UTF8 to UTF16 without using the normal XPCOM goop, which we
 * can't link to updater.exe.
 */
static char16_t*
AllocConvertUTF8toUTF16(const char *arg)
{
  // UTF16 can't be longer in units than UTF8
  int len = strlen(arg);
  char16_t *s = new char16_t[(len + 1) * sizeof(char16_t)];
  if (!s)
    return nullptr;

  ConvertUTF8toUTF16 convert(s);
  convert.write(arg, len);
  convert.write_terminator();
  return s;
}

static void
FreeAllocStrings(int argc, wchar_t **argv)
{
  while (argc) {
    --argc;
    delete [] argv[argc];
  }

  delete [] argv;
}



/**
 * Launch a child process with the specified arguments.
 * @note argv[0] is ignored
 * @note The form of this function that takes char **argv expects UTF-8
 */

BOOL
WinLaunchChild(const wchar_t *exePath,
               int argc, wchar_t **argv,
               HANDLE userToken = nullptr,
               HANDLE *hProcess = nullptr);

BOOL
WinLaunchChild(const wchar_t *exePath,
               int argc, char **argv,
               HANDLE userToken,
               HANDLE *hProcess)
{
  wchar_t** argvConverted = new wchar_t*[argc];
  if (!argvConverted)
    return FALSE;

  for (int i = 0; i < argc; ++i) {
      argvConverted[i] = reinterpret_cast<wchar_t*>(AllocConvertUTF8toUTF16(argv[i]));
    if (!argvConverted[i]) {
      FreeAllocStrings(i, argvConverted);
      return FALSE;
    }
  }

  BOOL ok = WinLaunchChild(exePath, argc, argvConverted, userToken, hProcess);
  FreeAllocStrings(argc, argvConverted);
  return ok;
}

BOOL
WinLaunchChild(const wchar_t *exePath,
               int argc, 
               wchar_t **argv,
               HANDLE userToken,
               HANDLE *hProcess)
{
  wchar_t *cl;
  BOOL ok;

  cl = MakeCommandLine(argc, argv);
  if (!cl) {
    return FALSE;
  }

  STARTUPINFOW si = {0};
  si.cb = sizeof(STARTUPINFOW);
  si.lpDesktop = L"winsta0\\Default";
  PROCESS_INFORMATION pi = {0};

  if (userToken == nullptr) {
    ok = CreateProcessW(exePath,
                        cl,
                        nullptr,  // no special security attributes
                        nullptr,  // no special thread attributes
                        FALSE, // don't inherit filehandles
                        0,     // creation flags
                        nullptr,  // inherit my environment
                        nullptr,  // use my current directory
                        &si,
                        &pi);
  } else {
    // Create an environment block for the process we're about to start using
    // the user's token.
    LPVOID environmentBlock = nullptr;
    if (!CreateEnvironmentBlock(&environmentBlock, userToken, TRUE)) {
      environmentBlock = nullptr;
    }

    ok = CreateProcessAsUserW(userToken, 
                              exePath,
                              cl,
                              nullptr,  // no special security attributes
                              nullptr,  // no special thread attributes
                              FALSE,    // don't inherit filehandles
                              0,        // creation flags
                              environmentBlock,
                              nullptr,  // use my current directory
                              &si,
                              &pi);

    if (environmentBlock) {
      DestroyEnvironmentBlock(environmentBlock);
    }
  }

  if (ok) {
    if (hProcess) {
      *hProcess = pi.hProcess; // the caller now owns the HANDLE
    } else {
      CloseHandle(pi.hProcess);
    }
    CloseHandle(pi.hThread);
  } else {
    LPVOID lpMsgBuf = nullptr;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                  FORMAT_MESSAGE_FROM_SYSTEM |
                  FORMAT_MESSAGE_IGNORE_INSERTS,
                  nullptr,
                  GetLastError(),
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPTSTR) &lpMsgBuf,
                  0,
                  nullptr);
    wprintf(L"Error restarting: %s\n", lpMsgBuf ? static_cast<const wchar_t*>(lpMsgBuf) : L"(null)");
    if (lpMsgBuf)
      LocalFree(lpMsgBuf);
  }

  free(cl);

  return ok;
}
