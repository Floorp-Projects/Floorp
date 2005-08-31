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

/**
 * Get the length that the string will take when it is quoted.
 */
static int QuotedStrLen(const char *s)
{
  int i = 2; // initial and final quote
  while (*s) {
    if (*s == '"') {
      ++i;
    }

    ++i; ++s;
  }
  return i;
}

/**
 * Copy string "s" to string "d", quoting and escaping all double quotes.
 * The CRT parses this to retrieve the original argc/argv that we meant,
 * see STDARGV.C in the MSVC6 CRT sources.
 *
 * @return the end of the string
 */
static char* QuoteString(char *d, const char *s)
{
  *d = '"';
  ++d;

  while (*s) {
    *d = *s;
    if (*s == '"') {
      ++d;
      *d = '"';
    }

    ++d; ++s;
  }

  *d = '"';
  ++d;

  return d;
}

/**
 * Create a quoted command from a list of arguments. The returned string
 * is allocated with "malloc" and should be "free"d.
 */
static char*
MakeCommandLine(int argc, char **argv)
{
  int i;
  int len = 1; // null-termination

  for (i = 0; i < argc; ++i)
    len += QuotedStrLen(argv[i]) + 1;

  char *s = (char*) malloc(len);
  if (!s)
    return NULL;

  char *c = s;
  for (i = 0; i < argc; ++i) {
    c = QuoteString(c, argv[i]);
    *c = ' ';
    ++c;
  }

  *c = '\0';

  return s;
}

/**
 * Launch a child process with the specified arguments.
 * @note argv[0] is ignored
 */
BOOL
WinLaunchChild(const char *exePath, int argc, char **argv)
{
  char *cl = MakeCommandLine(argc, argv);
  if (!cl)
    return FALSE;

  STARTUPINFO si = {sizeof(si), 0};
  PROCESS_INFORMATION pi = {0};

  BOOL ok = CreateProcess(exePath,
                          cl,
                          NULL,  // no special security attributes
                          NULL,  // no special thread attributes
                          FALSE, // don't inherit filehandles
                          0,     // No special process creation flags
                          NULL,  // inherit my environment
                          NULL,  // use my current directory
                          &si,
                          &pi);

  free(cl);

  if (ok) {
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
  }

  return ok;
}
