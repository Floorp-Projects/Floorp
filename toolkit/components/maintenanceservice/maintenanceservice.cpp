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
 * The Original Code is Mozilla Maintenance service.
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
#include <stdio.h>
#include <wchar.h>
#include <shlobj.h>

#include "serviceinstall.h"
#include "maintenanceservice.h"
#include "servicebase.h"
#include "workmonitor.h"
#include "uachelper.h"
#include "updatehelper.h"

SERVICE_STATUS gSvcStatus = { 0 }; 
SERVICE_STATUS_HANDLE gSvcStatusHandle = NULL; 
HANDLE ghSvcStopEvent = NULL;
BOOL gServiceStopping = FALSE;

// logs are pretty small ~10 lines, so 5 seems reasonable.
#define LOGS_TO_KEEP 5

BOOL GetLogDirectoryPath(WCHAR *path);

int 
wmain(int argc, WCHAR **argv)
{
  // If command-line parameter is "install", install the service
  // or upgrade if already installed
  // If command line parameter is "forceinstall", install the service
  // even if it is older than what is already installed.
  // If command-line parameter is "upgrade", upgrade the service
  // but do not install it if it is not already installed.
  // If command line parameter is "uninstall", uninstall the service.
  // Otherwise, the service is probably being started by the SCM.
  bool forceInstall = !lstrcmpi(argv[1], L"forceinstall");
  if (!lstrcmpi(argv[1], L"install") || forceInstall) {
    WCHAR updatePath[MAX_PATH + 1];
    if (GetLogDirectoryPath(updatePath)) {
      LogInit(updatePath, L"maintenanceservice-install.log");
    }

    LOG(("Installing service"));
    SvcInstallAction action = InstallSvc;
    if (forceInstall) {
      action = ForceInstallSvc;
      LOG((" with force specified"));
    }
    LOG(("...\n"));

    if (!SvcInstall(action)) {
      LOG(("Could not install service (%d)\n", GetLastError()));
      LogFinish();
      return 1;
    }

    LOG(("The service was installed successfully\n"));
    LogFinish();
    return 0;
  } 

  if (!lstrcmpi(argv[1], L"upgrade")) {
    WCHAR updatePath[MAX_PATH + 1];
    if (GetLogDirectoryPath(updatePath)) {
      LogInit(updatePath, L"maintenanceservice-install.log");
    }
    LOG(("Upgrading service if installed...\n"));
    if (!SvcInstall(UpgradeSvc)) {
      LOG(("Could not upgrade service (%d)\n", GetLastError()));
      LogFinish();
      return 1;
    }

    LOG(("The service was upgraded successfully\n"));
    LogFinish();
    return 0;
  }

  if (!lstrcmpi(argv[1], L"uninstall")) {
    WCHAR updatePath[MAX_PATH + 1];
    if (GetLogDirectoryPath(updatePath)) {
      LogInit(updatePath, L"maintenanceservice-uninstall.log");
    }
    LOG(("Uninstalling service...\n"));
    if (!SvcUninstall()) {
      LOG(("Could not uninstall service (%d)\n", GetLastError()));
      LogFinish();
      return 1;
    }
    LOG(("The service was uninstalled successfully\n"));
    LogFinish();
    return 0;
  }

  SERVICE_TABLE_ENTRYW DispatchTable[] = { 
    { SVC_NAME, (LPSERVICE_MAIN_FUNCTION) SvcMain }, 
    { NULL, NULL } 
  }; 

  // This call returns when the service has stopped. 
  // The process should simply terminate when the call returns.
  if (!StartServiceCtrlDispatcher(DispatchTable)) {
    LOG(("StartServiceCtrlDispatcher failed (%d)\n", GetLastError()));
  }

  return 0;
}

/**
 * Wrapper callback for the monitoring thread.
 *
 * @param param Unused thread callback parameter
 */
DWORD
WINAPI StartMaintenanceServiceThread(LPVOID param) 
{
  ThreadData *threadData = reinterpret_cast<ThreadData*>(param);
  ExecuteServiceCommand(threadData->argc, threadData->argv);
  delete threadData;
  return 0;
}

/**
 * Obtains the base path where logs should be stored
 *
 * @param  path The out buffer for the backup log path of size MAX_PATH + 1
 * @return TRUE if successful.
 */
BOOL
GetLogDirectoryPath(WCHAR *path) 
{
  HRESULT hr = SHGetFolderPathW(NULL, CSIDL_COMMON_APPDATA, NULL, 
    SHGFP_TYPE_CURRENT, path);
  if (FAILED(hr)) {
    return FALSE;
  }

  if (!PathAppendSafe(path, L"Mozilla")) {
    return FALSE;
  }
  // The directory should already be created from the installer, but
  // just to be safe in case someone deletes.
  CreateDirectoryW(path, NULL);

  if (!PathAppendSafe(path, L"logs")) {
    return FALSE;
  }
  CreateDirectoryW(path, NULL);
  return TRUE;
}

/**
 * Calculated a backup path based on the log number.
 *
 * @param  path      The out buffer to store the log path of size MAX_PATH + 1
 * @param  basePath  The base directory where the calculated path should go
 * @param  logNumber The log number, 0 == updater.log
 * @return TRUE if successful.
 */
BOOL
GetBackupLogPath(LPWSTR path, LPCWSTR basePath, int logNumber)
{
  WCHAR logName[64];
  wcscpy(path, basePath);
  if (logNumber <= 0) {
    swprintf(logName, L"maintenanceservice.log");
  } else {
    swprintf(logName, L"maintenanceservice-%d.log", logNumber);
  }
  return PathAppendSafe(path, logName);
}

/**
 * Moves the old log files out of the way before a new one is written.
 * If you for example keep 3 logs, then this function will do:
 *   updater2.log -> updater3.log
 *   updater1.log -> updater2.log
 *   updater.log -> updater1.log
 * Which clears room for a new updater.log in the basePath directory
 * 
 * @param basePath      The base directory path where log files are stored
 * @param numLogsToKeep The number of logs to keep
 */
void
BackupOldLogs(LPCWSTR basePath, int numLogsToKeep) 
{
  WCHAR oldPath[MAX_PATH + 1];
  WCHAR newPath[MAX_PATH + 1];
  for (int i = numLogsToKeep; i >= 1; i--) {
    if (!GetBackupLogPath(oldPath, basePath, i -1)) {
      continue;
    }

    if (!GetBackupLogPath(newPath, basePath, i)) {
      continue;
    }

    if (!MoveFileEx(oldPath, newPath, MOVEFILE_REPLACE_EXISTING)) {
      continue;
    }
  }
}

/**
 * Main entry point when running as a service.
 */
void WINAPI 
SvcMain(DWORD dwArgc, LPWSTR *lpszArgv)
{
  // Setup logging, and backup the old logs
  WCHAR updatePath[MAX_PATH + 1];
  if (GetLogDirectoryPath(updatePath)) {
    BackupOldLogs(updatePath, LOGS_TO_KEEP);
    LogInit(updatePath, L"maintenanceservice.log");
  }

  // Disable every privilege we don't need. Processes started using
  // CreateProcess will use the same token as this process.
  UACHelper::DisablePrivileges(NULL);

  // Register the handler function for the service
  gSvcStatusHandle = RegisterServiceCtrlHandlerW(SVC_NAME, SvcCtrlHandler);
  if (!gSvcStatusHandle) {
    LOG(("RegisterServiceCtrlHandler failed (%d)\n", GetLastError()));
    return; 
  } 

  // These SERVICE_STATUS members remain as set here
  gSvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
  gSvcStatus.dwServiceSpecificExitCode = 0;

  // Report initial status to the SCM
  ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

  // Perform service-specific initialization and work.
  SvcInit(dwArgc, lpszArgv);
}

/**
 * Service initialization.
 */
void
SvcInit(DWORD argc, LPWSTR *argv)
{
  // Create an event. The control handler function, SvcCtrlHandler,
  // signals this event when it receives the stop control code.
  ghSvcStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
  if (NULL == ghSvcStopEvent) {
    ReportSvcStatus(SERVICE_STOPPED, 1, 0);
    return;
  }

  ThreadData *threadData = new ThreadData();
  threadData->argc = argc;
  threadData->argv = argv;

  DWORD threadID;
  HANDLE thread = CreateThread(NULL, 0, StartMaintenanceServiceThread, 
                               threadData, 0, &threadID);

  // Report running status when initialization is complete.
  ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);

  // Perform work until service stops.
  for(;;) {
    // Check whether to stop the service.
    WaitForSingleObject(ghSvcStopEvent, INFINITE);
    ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
    return;
  }
}

/**
 * Sets the current service status and reports it to the SCM.
 *  
 * @param currentState  The current state (see SERVICE_STATUS)
 * @param exitCode      The system error code
 * @param waitHint      Estimated time for pending operation in milliseconds
 */
void
ReportSvcStatus(DWORD currentState, 
                DWORD exitCode, 
                DWORD waitHint)
{
  static DWORD dwCheckPoint = 1;

  // Fill in the SERVICE_STATUS structure.
  gSvcStatus.dwCurrentState = currentState;
  gSvcStatus.dwWin32ExitCode = exitCode;
  gSvcStatus.dwWaitHint = waitHint;

  if (SERVICE_START_PENDING == currentState) {
    gSvcStatus.dwControlsAccepted = 0;
  } else {
    gSvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
  }

  if ((SERVICE_RUNNING == currentState) ||
      (SERVICE_STOPPED == currentState)) {
    gSvcStatus.dwCheckPoint = 0;
  } else {
    gSvcStatus.dwCheckPoint = dwCheckPoint++;
  }

  // Report the status of the service to the SCM.
  SetServiceStatus(gSvcStatusHandle, &gSvcStatus);
}

/**
 * Called by SCM whenever a control code is sent to the service
 * using the ControlService function.
 */
void WINAPI
SvcCtrlHandler(DWORD dwCtrl)
{
  // Handle the requested control code. 
  switch(dwCtrl) {
  case SERVICE_CONTROL_STOP: 
    ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);
    // Signal the service to stop.
    SetEvent(ghSvcStopEvent);
    ReportSvcStatus(gSvcStatus.dwCurrentState, NO_ERROR, 0);
    LogFinish();
    break;
  case SERVICE_CONTROL_INTERROGATE: 
    break; 
  default: 
    break;
  }
}
