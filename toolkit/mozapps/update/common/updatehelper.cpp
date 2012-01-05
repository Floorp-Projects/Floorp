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
#include <stdio.h>
#include "shlobj.h"

// Needed for PathAppendW
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib") 

WCHAR*
MakeCommandLine(int argc, WCHAR **argv);
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
 * @param  installationDir The path to the callback application binary.
 * @param  updateInfoDir   The directory where update info is stored.
 * @param  forceSync       If true even if the ini file specifies async, the
 *                         process will wait for termination of PostUpdate.
 * @param  userToken       The user token to run as, if NULL the current user
 *                         will be used.
 * @return TRUE if there was no error starting the process.
 */
BOOL
LaunchWinPostProcess(const WCHAR *installationDir,
                     const WCHAR *updateInfoDir,
                     bool forceSync,
                     HANDLE userToken)
{
  WCHAR workingDirectory[MAX_PATH + 1];
  wcscpy(workingDirectory, installationDir);

  // Launch helper.exe to perform post processing (e.g. registry and log file
  // modifications) for the update.
  WCHAR inifile[MAX_PATH + 1];
  wcscpy(inifile, installationDir);
  if (!PathAppendSafe(inifile, L"updater.ini")) {
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
                                MAX_PATH + 1, inifile)) {
    return FALSE;
  }

  if (!GetPrivateProfileStringW(L"PostUpdateWin", L"ExeAsync", L"TRUE", 
                                exeasync,
                                sizeof(exeasync)/sizeof(exeasync[0]), 
                                inifile)) {
    return FALSE;
  }

  WCHAR exefullpath[MAX_PATH + 1];
  wcscpy(exefullpath, installationDir);
  if (!PathAppendSafe(exefullpath, exefile)) {
    return false;
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


/**
 * Executes a maintenance service command
 * 
 * @param  argc    The total number of arguments in argv
 * @param  argv    An array of null terminated strings to pass to the service, 
 * @return TRUE if the service command was started.
*/
BOOL 
StartServiceCommand(int argc, LPCWSTR* argv) 
{
  // Get a handle to the SCM database.
  SC_HANDLE serviceManager = OpenSCManager(NULL, NULL, 
                                           SC_MANAGER_CONNECT | 
                                           SC_MANAGER_ENUMERATE_SERVICE);
  if (!serviceManager)  {
    return FALSE;
  }

  // Get a handle to the service.
  SC_HANDLE service = OpenServiceW(serviceManager, 
                                   L"MozillaMaintenance", 
                                   SERVICE_QUERY_STATUS | SERVICE_START);
  if (!service) {
    CloseServiceHandle(serviceManager);
    return FALSE;
  }

  // Make sure the service is not stopped.
  SERVICE_STATUS_PROCESS ssp;
  DWORD bytesNeeded;
  if (!QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp,
                            sizeof(SERVICE_STATUS_PROCESS), &bytesNeeded)) {
    CloseServiceHandle(service);
    CloseServiceHandle(serviceManager);
    return FALSE;
  }

  // The service is already in use.
  if (ssp.dwCurrentState != SERVICE_STOPPED) {
    CloseServiceHandle(service);
    CloseServiceHandle(serviceManager);
    return FALSE;
  }

  return StartServiceW(service, argc, argv);
}

/**
 * Launch a service initiated action for a software update with the 
 * specified arguments.
 *
 * @param  exePath The path of the executable to run
 * @param  argc    The total number of arguments in argv
 * @param  argv    An array of null terminated strings to pass to the exePath, 
 *                 argv[0] must be the path to the updater.exe
 * @return TRUE if successful
 */
BOOL
LaunchServiceSoftwareUpdateCommand(DWORD argc, LPCWSTR* argv)
{
  // The service command is the same as the updater.exe command line except 
  // it has 2 extra args: 1) The Path to udpater.exe, and 2) the command 
  // being executed which is "software-update"
  LPCWSTR *updaterServiceArgv = new LPCWSTR[argc + 2];
  updaterServiceArgv[0] = L"maintenanceservice.exe";
  updaterServiceArgv[1] = L"software-update";

  for (int i = 0; i < argc; ++i) {
    updaterServiceArgv[i + 2] = argv[i];
  }

  // Execute the service command by starting the service with
  // the passed in arguments.
  BOOL result = StartServiceCommand(argc + 2, updaterServiceArgv);
  delete[] updaterServiceArgv;
  return result;
}

/**
 * Joins a base directory path with a filename.
 *
 * @param  base  The base directory path of size MAX_PATH + 1
 * @param  extra The filename to append
 * @return TRUE if the file name was successful appended to base
 */
BOOL
PathAppendSafe(LPWSTR base, LPCWSTR extra)
{
  if (wcslen(base) + wcslen(extra) >= MAX_PATH) {
    return FALSE;
  }

  return PathAppendW(base, extra);
}

/**
 * Sets update.status to pending so that the next startup will not use
 * the service and instead will attempt an update the with a UAC prompt.
 *
 * @param  updateDirPath The path of the update directory
 * @return TRUE if successful
 */
BOOL
WriteStatusPending(LPCWSTR updateDirPath)
{
  WCHAR updateStatusFilePath[MAX_PATH + 1];
  wcscpy(updateStatusFilePath, updateDirPath);
  if (!PathAppendSafe(updateStatusFilePath, L"update.status")) {
    return FALSE;
  }

  const char pending[] = "pending";
  HANDLE statusFile = CreateFileW(updateStatusFilePath, GENERIC_WRITE, 0, 
                                  NULL, CREATE_ALWAYS, 0, NULL);
  if (statusFile == INVALID_HANDLE_VALUE) {
    return FALSE;
  }

  DWORD wrote;
  BOOL ok = WriteFile(statusFile, pending, 
                      sizeof(pending) - 1, &wrote, NULL); 
  CloseHandle(statusFile);
  return ok && (wrote == sizeof(pending) - 1);
}

/**
 * Sets update.status to a specific failure code
 *
 * @param  updateDirPath The path of the update directory
 * @return TRUE if successful
 */
BOOL
WriteStatusFailure(LPCWSTR updateDirPath, int errorCode) 
{
  WCHAR updateStatusFilePath[MAX_PATH + 1];
  wcscpy(updateStatusFilePath, updateDirPath);
  if (!PathAppendSafe(updateStatusFilePath, L"update.status")) {
    return FALSE;
  }

  HANDLE statusFile = CreateFileW(updateStatusFilePath, GENERIC_WRITE, 0, 
                                  NULL, CREATE_ALWAYS, 0, NULL);
  if (statusFile == INVALID_HANDLE_VALUE) {
    return FALSE;
  }
  char failure[32];
  sprintf(failure, "failed: %d", errorCode);

  DWORD toWrite = strlen(failure);
  DWORD wrote;
  BOOL ok = WriteFile(statusFile, failure, 
                      toWrite, &wrote, NULL); 
  CloseHandle(statusFile);
  return ok && wrote == toWrite;
}
