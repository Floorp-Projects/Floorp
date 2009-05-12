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
 * The Original Code is Netscape Portable Runtime (NSPR) Tests.
 *
 * The Initial Developer of the Original Code is John Wolfe
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
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

#ifndef nst_wince_h___
#define nst_wince_h___

#ifndef __WINDOWS__
#include <windows.h>
#endif

/***********
 * NOTE: This file is loaded once in each test - and defines two
 * functions for each test's eventual executable: DllMain() and
 * nspr_test_runme() -- both exported functions.
 *
 * DLLs require a DllMain() entry-point.
 *
 * nspr_test_runme() is required so that the WinCE/WinMobile
 * controlling test program can run each DLL's tests in a standard way.
 ***********/

#ifdef WINCE

#ifdef perror
#undef perror
#endif
#define perror(x)


/***********
 * Do nothing DllMain -- needed to create a functional DLL
 ***********/
BOOL WINAPI DllMain( HANDLE hDllHandle,
                     DWORD  nReason,
                     LPVOID lpvReserved )
{
  BOOLEAN bSuccess = TRUE;

  switch ( nReason )
  {
    case DLL_PROCESS_ATTACH:
      /* Do nothing yet  */
      break;

    case DLL_THREAD_ATTACH:
      /* Do nothing yet  */
      break;

    case DLL_THREAD_DETACH:
      /* Do nothing yet  */
      break;

    case DLL_PROCESS_DETACH:
      /* Do nothing yet  */
      break;

    default:
      /* SHOULD NOT GET HERE - Do nothing  */
      break;
  }

  return bSuccess;
}
/*  end DllMain   */

/* NOW, take care of all the other main() functions out there */
#define main __declspec(dllexport) WINAPI nspr_test_runme

#undef getcwd
#define getcwd(x, nc) \
{ \
  int i; \
  unsigned short dir[MAX_PATH]; \
  GetModuleFileName(GetModuleHandle (NULL), dir, MAX_PATH); \
  for (i = _tcslen(dir); i && dir[i] != TEXT('\\'); i--) {} \
  dir[i + 1] = L'\0'; \
  WideCharToMultiByte(CP_ACP, 0, dir, -1, x, nc, NULL, NULL); \
}

#endif  /* WINCE */

#endif /* nst_wince_h___ */
