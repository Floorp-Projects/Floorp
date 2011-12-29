/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla XULRunner bootstrap.
 *
 * The Initial Developer of the Original Code is
 * Benjamin Smedberg <benjamin@smedbergs.us>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Mozilla Foundation. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert Strong <robert.bugzilla@gmail.com>
 *   Brian R. Bondy <netzen@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
static int ArgStrLen(const PRUnichar *s)
{
  int backslashes = 0;
  int i = wcslen(s);
  BOOL hasDoubleQuote = wcschr(s, L'"') != NULL;
  // Only add doublequotes if the string contains a space or a tab
  BOOL addDoubleQuotes = wcspbrk(s, L" \t") != NULL;

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
static PRUnichar* ArgToString(PRUnichar *d, const PRUnichar *s)
{
  int backslashes = 0;
  BOOL hasDoubleQuote = wcschr(s, L'"') != NULL;
  // Only add doublequotes if the string contains a space or a tab
  BOOL addDoubleQuotes = wcspbrk(s, L" \t") != NULL;

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
PRUnichar*
MakeCommandLine(int argc, PRUnichar **argv)
{
  int i;
  int len = 0;

  // The + 1 of the last argument handles the allocation for null termination
  for (i = 0; i < argc; ++i)
    len += ArgStrLen(argv[i]) + 1;

  // Protect against callers that pass 0 arguments
  if (len == 0)
    len = 1;

  PRUnichar *s = (PRUnichar*) malloc(len * sizeof(PRUnichar));
  if (!s)
    return NULL;

  PRUnichar *c = s;
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
static PRUnichar*
AllocConvertUTF8toUTF16(const char *arg)
{
  // UTF16 can't be longer in units than UTF8
  int len = strlen(arg);
  PRUnichar *s = new PRUnichar[(len + 1) * sizeof(PRUnichar)];
  if (!s)
    return NULL;

  ConvertUTF8toUTF16 convert(s);
  convert.write(arg, len);
  convert.write_terminator();
  return s;
}

static void
FreeAllocStrings(int argc, PRUnichar **argv)
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
WinLaunchChild(const PRUnichar *exePath, 
               int argc, PRUnichar **argv, 
               HANDLE userToken = NULL);

BOOL
WinLaunchChild(const PRUnichar *exePath, 
               int argc, char **argv, 
               HANDLE userToken)
{
  PRUnichar** argvConverted = new PRUnichar*[argc];
  if (!argvConverted)
    return FALSE;

  for (int i = 0; i < argc; ++i) {
    argvConverted[i] = AllocConvertUTF8toUTF16(argv[i]);
    if (!argvConverted[i]) {
      FreeAllocStrings(i, argvConverted);
      return FALSE;
    }
  }

  BOOL ok = WinLaunchChild(exePath, argc, argvConverted, userToken);
  FreeAllocStrings(argc, argvConverted);
  return ok;
}

BOOL
WinLaunchChild(const PRUnichar *exePath, 
               int argc, 
               PRUnichar **argv, 
               HANDLE userToken)
{
  PRUnichar *cl;
  BOOL ok;

  cl = MakeCommandLine(argc, argv);
  if (!cl) {
    return FALSE;
  }

  STARTUPINFOW si = {0};
  si.cb = sizeof(STARTUPINFOW);
  si.lpDesktop = L"winsta0\\Default";
  PROCESS_INFORMATION pi = {0};

  DWORD creationFlags = 0;
#ifdef DEBUG
  creationFlags |= CREATE_NEW_CONSOLE;
#endif
  if (userToken == NULL) {
    ok = CreateProcessW(exePath,
                        cl,
                        NULL,  // no special security attributes
                        NULL,  // no special thread attributes
                        FALSE, // don't inherit filehandles
                        creationFlags,
                        NULL,  // inherit my environment
                        NULL,  // use my current directory
                        &si,
                        &pi);
  } else {
    // Create an environment block for the process we're about to start using
    // the user's token.
    LPVOID environmentBlock = NULL;
    if (!CreateEnvironmentBlock(&environmentBlock, userToken, TRUE)) {
      environmentBlock = NULL;
    }

    ok = CreateProcessAsUserW(userToken, 
                              exePath,
                              cl,
                              NULL,  // no special security attributes
                              NULL,  // no special thread attributes
                              FALSE, // don't inherit filehandles
                              CREATE_DEFAULT_ERROR_MODE |
#ifdef DEBUG
                              CREATE_NEW_CONSOLE |
#endif
                              CREATE_UNICODE_ENVIRONMENT,                              
                              environmentBlock,
                              NULL,  // use my current directory
                              &si,
                              &pi);

    if (environmentBlock) {
      DestroyEnvironmentBlock(environmentBlock);
    }
  }

  if (ok) {
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
  } else {
    LPVOID lpMsgBuf = NULL;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                  FORMAT_MESSAGE_FROM_SYSTEM |
                  FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL,
                  GetLastError(),
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPTSTR) &lpMsgBuf,
                  0,
                  NULL);
    wprintf(L"Error restarting: %s\n", lpMsgBuf ? lpMsgBuf : L"(null)");
    if (lpMsgBuf)
      LocalFree(lpMsgBuf);
  }

  free(cl);

  return ok;
}

