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
 * The Original Code is common code between maintenanceservice and updater
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include <windows.h>
#include <shlwapi.h>

BOOL PathAppendSafe(LPWSTR base, LPCWSTR extra);


/**
 * Obtains the path of a file in the same directory as the specified file.
 *
 * @param  destinationBuffer A buffer of size MAX_PATH + 1 to store the result.
 * @param  siblingFIlePath   The path of another file in the same directory
 * @param  newFileName       The filename of another file in the same directory
 * @return TRUE if successful
 */
BOOL
PathGetSiblingFilePath(LPWSTR destinationBuffer, 
                       LPCWSTR siblingFilePath, 
                       LPCWSTR newFileName)
{
  if (wcslen(siblingFilePath) >= MAX_PATH) {
    return FALSE;
  }

  wcscpy(destinationBuffer, siblingFilePath);
  if (!PathRemoveFileSpecW(destinationBuffer)) {
    return FALSE;
  }

  if (wcslen(destinationBuffer) + wcslen(newFileName) >= MAX_PATH) {
    return FALSE;
  }

  return PathAppendSafe(destinationBuffer, newFileName);
}

/**
 * Launch the post update application as the specified user (helper.exe).
 * It takes in the path of the callback application to calculate the path
 * of helper.exe.  For service updates this is called from both the system
 * account and the current user account.
 *
 * @param  appExe        The path to the callback application binary.
 * @param  updateInfoDir The directory where update info is stored.
 * @param  forceSync     If true even if the ini file specifies async, the
 *                       process will wait for termination of PostUpdate.
 * @param  userToken     The user token to run as, if NULL the current user
 *                       will be used.
 */
BOOL
LaunchWinPostProcess(const WCHAR *appExe,
                     const WCHAR *updateInfoDir,
                     bool forceSync,
                     HANDLE userToken)
{
  WCHAR workingDirectory[MAX_PATH + 1];
  wcscpy(workingDirectory, appExe);
  if (!PathRemoveFileSpecW(workingDirectory)) {
    return FALSE;
  }

  // Launch helper.exe to perform post processing (e.g. registry and log file
  // modifications) for the update.
  WCHAR inifile[MAX_PATH + 1];
  if (!PathGetSiblingFilePath(inifile, appExe, L"updater.ini")) {
    return FALSE;
  }

  WCHAR exefile[MAX_PATH + 1];
  WCHAR exearg[MAX_PATH + 1];
  WCHAR exeasync[10];
  bool async = true;
  if (!GetPrivateProfileStringW(L"PostUpdateWin", L"ExeRelPath", NULL, exefile,
                                MAX_PATH + 1, inifile)) {
    return FALSE;
  }

  if (!GetPrivateProfileStringW(L"PostUpdateWin", L"ExeArg", NULL, exearg,
                                MAX_PATH + 1, inifile))
    return FALSE;

  if (!GetPrivateProfileStringW(L"PostUpdateWin", L"ExeAsync", L"TRUE", 
                                exeasync,
                                sizeof(exeasync)/sizeof(exeasync[0]), inifile))
    return FALSE;

  WCHAR exefullpath[MAX_PATH + 1];
  if (!PathGetSiblingFilePath(exefullpath, appExe, exefile)) {
    return FALSE;
  }

  WCHAR dlogFile[MAX_PATH + 1];
  if (!PathGetSiblingFilePath(dlogFile, exefullpath, L"uninstall.update")) {
    return FALSE;
  }

  WCHAR slogFile[MAX_PATH + 1];
  wcscpy(slogFile, updateInfoDir);
  if (!PathAppendSafe(slogFile, L"update.log")) {
    return FALSE;
  }

  WCHAR dummyArg[14];
  wcscpy(dummyArg, L"argv0ignored ");

  size_t len = wcslen(exearg) + wcslen(dummyArg);
  WCHAR *cmdline = (WCHAR *) malloc((len + 1) * sizeof(WCHAR));
  if (!cmdline) {
    return FALSE;
  }

  wcscpy(cmdline, dummyArg);
  wcscat(cmdline, exearg);

  if (forceSync ||
      !_wcsnicmp(exeasync, L"false", 6) || 
      !_wcsnicmp(exeasync, L"0", 2)) {
    async = false;
  }
  
  // We want to launch the post update helper app to update the Windows
  // registry even if there is a failure with removing the uninstall.update
  // file or copying the update.log file.
  CopyFileW(slogFile, dlogFile, false);

  STARTUPINFOW si = {sizeof(si), 0};
  si.lpDesktop = L"";
  PROCESS_INFORMATION pi = {0};

  bool ok;
  if (userToken) {
    ok = CreateProcessAsUserW(userToken,
                              exefullpath,
                              cmdline,
                              NULL,  // no special security attributes
                              NULL,  // no special thread attributes
                              false, // don't inherit filehandles
                              0,     // No special process creation flags
                              NULL,  // inherit my environment
                              workingDirectory,
                              &si,
                              &pi);
  } else {
    ok = CreateProcessW(exefullpath,
                        cmdline,
                        NULL,  // no special security attributes
                        NULL,  // no special thread attributes
                        false, // don't inherit filehandles
                        0,     // No special process creation flags
                        NULL,  // inherit my environment
                        workingDirectory,
                        &si,
                        &pi);
  }
  free(cmdline);
  if (ok) {
    if (!async)
      WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
  }
  return ok;
}

/**
 * Starts the upgrade process for update of the service if it is
 * already installed.
 *
 * @param  argc The argc value normally sent to updater.exe
 * @param  argv The argv value normally sent to updater.exe
 * @return TRUE if successful
 */
BOOL
StartServiceUpdate(int argc, LPWSTR *argv)
{
  if (argc < 2) {
    return FALSE;
  }

  // Get a handle to the local computer SCM database
  SC_HANDLE manager = OpenSCManager(NULL, NULL, 
                                    SC_MANAGER_ALL_ACCESS);
  if (!manager) {
    return FALSE;
  }

  // Open the service
  SC_HANDLE svc = OpenServiceW(manager, L"MozillaMaintenance", 
                               SERVICE_ALL_ACCESS);
  if (!svc) {
    CloseServiceHandle(manager);
    return FALSE;
  }
  CloseServiceHandle(svc);
  CloseServiceHandle(manager);

  // If we reach here, then the service is installed, so
  // proceed with upgrading it.

  STARTUPINFOW si = {0};
  si.cb = sizeof(STARTUPINFOW);
  // No particular desktop because no UI
  si.lpDesktop = L"";
  PROCESS_INFORMATION pi = {0};

  WCHAR maintserviceInstallerPath[MAX_PATH + 1];
  wcscpy(maintserviceInstallerPath, argv[2]);
  PathAppendSafe(maintserviceInstallerPath, 
                 L"maintenanceservice_installer.exe");
  WCHAR cmdLine[64];
  wcscpy(cmdLine, L"dummyparam.exe /Upgrade");
  BOOL svcUpdateProcessStarted = CreateProcessW(maintserviceInstallerPath, 
                                                cmdLine, 
                                                NULL, NULL, FALSE, 
                                                CREATE_DEFAULT_ERROR_MODE | 
                                                CREATE_UNICODE_ENVIRONMENT, 
                                                NULL, argv[2], &si, &pi);
  if (svcUpdateProcessStarted) {
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
  }
  return svcUpdateProcessStarted;
}
