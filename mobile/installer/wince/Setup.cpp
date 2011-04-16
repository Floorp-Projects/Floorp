/* -*- Mode: C++; c-basic-offset: 2; tab-width: 8; indent-tabs-mode: nil; -*- */
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
 * The Original Code is Fennec Installer for WinCE.
 *
 * The Initial Developer of the Original Code is The Mozilla Foundation.
 *
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Pakhotin <alexp@mozilla.com> (original author)
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

/**
 *
 * Mozilla Fennec Setup DLL
 *
 */

#include <windows.h>
#include <ce_setup.h>

const WCHAR c_sAppRegKey[] = L"Software\\Mozilla\\" MOZ_APP_DISPLAYNAME;

// Forward declarations
BOOL GetInstallPath(WCHAR *sPath);
BOOL RunUninstall();

//////////////////////////////////////////////////////////////////////////
// Entry point
//
BOOL APIENTRY DllMain(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
  return TRUE;
}

codeINSTALL_INIT Install_Init(HWND hwndParent, BOOL fFirstCall, BOOL fPreviouslyInstalled, LPCTSTR pszInstallDir)
{
  return codeINSTALL_INIT_CONTINUE;
}

codeINSTALL_EXIT Install_Exit(HWND hwndParent, LPCTSTR pszInstallDir,
                              WORD cFailedDirs, WORD cFailedFiles, WORD cFailedRegKeys,
                              WORD cFailedRegVals, WORD cFailedShortcuts)
{
  return codeINSTALL_EXIT_DONE;
}

codeUNINSTALL_INIT Uninstall_Init(HWND hwndParent, LPCTSTR pszInstallDir)
{
  RunUninstall();

  // Continue regardless of the RunUninstall result
  // This uninstallation may run from the Uninstall.exe,
  // in this case RunUninstall will not succeed, which is correct
  return codeUNINSTALL_INIT_CONTINUE;
}

codeUNINSTALL_EXIT Uninstall_Exit(HWND hwndParent)
{
  return codeUNINSTALL_EXIT_DONE;
}

//////////////////////////////////////////////////////////////////////////
// Utility functions
//
BOOL GetInstallPath(WCHAR *sPath)
{
  HKEY hKey;

  LONG result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, c_sAppRegKey, 0, KEY_ALL_ACCESS, &hKey);
  if (result == ERROR_SUCCESS)
  {
    DWORD dwType = NULL;
    DWORD dwCount = MAX_PATH * sizeof(WCHAR);
    result = RegQueryValueEx(hKey, L"Path", NULL, &dwType, (LPBYTE)sPath, &dwCount);

    RegCloseKey(hKey);
  }

  return (result == ERROR_SUCCESS);
}

BOOL RunUninstall()
{
  BOOL bResult = FALSE;
  WCHAR sUninstallPath[MAX_PATH];
  if (GetInstallPath(sUninstallPath))
  {
    if (wcslen(sUninstallPath) > 0 && sUninstallPath[wcslen(sUninstallPath)-1] != '\\')
      wcscat(sUninstallPath, L"\\");

    wcscat(sUninstallPath, L"uninstall.exe");

    PROCESS_INFORMATION pi;
    bResult = CreateProcess(sUninstallPath, L"[setup]",
                            NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi);
    if (bResult)
    {
      // Wait for it to finish
      WaitForSingleObject(pi.hProcess, INFINITE);
    }
  }
  return bResult;
}
