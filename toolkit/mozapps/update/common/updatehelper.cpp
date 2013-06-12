/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_METRO
// Needed for COM calls to launch Metro applications
#undef WINVER
#undef _WIN32_WINNT
#define WINVER 0x602
#define _WIN32_WINNT 0x602
#include <objbase.h>
#include <shobjidl.h>
#pragma comment(lib, "ole32.lib")
#endif

#include <windows.h>

// Needed for CreateToolhelp32Snapshot
#include <tlhelp32.h>
#ifndef ONLY_SERVICE_LAUNCHING

#include <stdio.h>
#include "shlobj.h"
#include "updatehelper.h"
#include "pathhash.h"

// Needed for PathAppendW
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

WCHAR* MakeCommandLine(int argc, WCHAR **argv);
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
 * @param  installDir the installation directory where
 *         maintenanceservice_installer.exe is located.
 * @return TRUE if successful
 */
BOOL
StartServiceUpdate(LPCWSTR installDir)
{
  // Get a handle to the local computer SCM database
  SC_HANDLE manager = OpenSCManager(NULL, NULL,
                                    SC_MANAGER_ALL_ACCESS);
  if (!manager) {
    return FALSE;
  }

  // Open the service
  SC_HANDLE svc = OpenServiceW(manager, SVC_NAME,
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
  wcscpy(maintserviceInstallerPath, installDir);
  PathAppendSafe(maintserviceInstallerPath,
                 L"maintenanceservice_installer.exe");
  WCHAR cmdLine[64];
  wcscpy(cmdLine, L"dummyparam.exe /Upgrade");
  BOOL svcUpdateProcessStarted = CreateProcessW(maintserviceInstallerPath,
                                                cmdLine,
                                                NULL, NULL, FALSE,
                                                0,
                                                NULL, installDir, &si, &pi);
  if (svcUpdateProcessStarted) {
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
  }
  return svcUpdateProcessStarted;
}

#endif

/**
 * Executes a maintenance service command
 *
 * @param  argc    The total number of arguments in argv
 * @param  argv    An array of null terminated strings to pass to the service,
 * @return ERROR_SUCCESS if the service command was started.
 *         Less than 16000, a windows system error code from StartServiceW
 *         More than 20000, 20000 + the last state of the service constant if
 *         the last state is something other than stopped.
 *         17001 if the SCM could not be opened
 *         17002 if the service could not be opened
*/
DWORD
StartServiceCommand(int argc, LPCWSTR* argv)
{
  DWORD lastState = WaitForServiceStop(SVC_NAME, 5);
  if (lastState != SERVICE_STOPPED) {
    return 20000 + lastState;
  }

  // Get a handle to the SCM database.
  SC_HANDLE serviceManager = OpenSCManager(NULL, NULL,
                                           SC_MANAGER_CONNECT |
                                           SC_MANAGER_ENUMERATE_SERVICE);
  if (!serviceManager)  {
    return 17001;
  }

  // Get a handle to the service.
  SC_HANDLE service = OpenServiceW(serviceManager,
                                   SVC_NAME,
                                   SERVICE_START);
  if (!service) {
    CloseServiceHandle(serviceManager);
    return 17002;
  }

  // Wait at most 5 seconds trying to start the service in case of errors
  // like ERROR_SERVICE_DATABASE_LOCKED or ERROR_SERVICE_REQUEST_TIMEOUT.
  const DWORD maxWaitMS = 5000;
  DWORD currentWaitMS = 0;
  DWORD lastError = ERROR_SUCCESS;
  while (currentWaitMS < maxWaitMS) {
    BOOL result = StartServiceW(service, argc, argv);
    if (result) {
      lastError = ERROR_SUCCESS;
      break;
    } else {
      lastError = GetLastError();
    }
    Sleep(100);
    currentWaitMS += 100;
  }
  CloseServiceHandle(service);
  CloseServiceHandle(serviceManager);
  return lastError;
}

#ifndef ONLY_SERVICE_LAUNCHING

/**
 * Launch a service initiated action for a software update with the
 * specified arguments.
 *
 * @param  exePath The path of the executable to run
 * @param  argc    The total number of arguments in argv
 * @param  argv    An array of null terminated strings to pass to the exePath,
 *                 argv[0] must be the path to the updater.exe
 * @return ERROR_SUCCESS if successful
 */
DWORD
LaunchServiceSoftwareUpdateCommand(int argc, LPCWSTR* argv)
{
  // The service command is the same as the updater.exe command line except
  // it has 2 extra args: 1) The Path to udpater.exe, and 2) the command
  // being executed which is "software-update"
  LPCWSTR *updaterServiceArgv = new LPCWSTR[argc + 2];
  updaterServiceArgv[0] = L"MozillaMaintenance";
  updaterServiceArgv[1] = L"software-update";

  for (int i = 0; i < argc; ++i) {
    updaterServiceArgv[i + 2] = argv[i];
  }

  // Execute the service command by starting the service with
  // the passed in arguments.
  DWORD ret = StartServiceCommand(argc + 2, updaterServiceArgv);
  delete[] updaterServiceArgv;
  return ret;
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

#endif

/**
 * Waits for a service to enter a stopped state.
 * This function does not stop the service, it just blocks until the service
 * is stopped.
 *
 * @param  serviceName     The service to wait for.
 * @param  maxWaitSeconds  The maximum number of seconds to wait
 * @return state of the service after a timeout or when stopped.
 *         A value of 255 is returned for an error. Typical values are:
 *         SERVICE_STOPPED 0x00000001
 *         SERVICE_START_PENDING 0x00000002
 *         SERVICE_STOP_PENDING 0x00000003
 *         SERVICE_RUNNING 0x00000004
 *         SERVICE_CONTINUE_PENDING 0x00000005
 *         SERVICE_PAUSE_PENDING 0x00000006
 *         SERVICE_PAUSED 0x00000007
 *         last status not set 0x000000CF
 *         Could no query status 0x000000DF
 *         Could not open service, access denied 0x000000EB
 *         Could not open service, invalid handle 0x000000EC
 *         Could not open service, invalid name 0x000000ED
 *         Could not open service, does not exist 0x000000EE
 *         Could not open service, other error 0x000000EF
 *         Could not open SCM, access denied 0x000000FD
 *         Could not open SCM, database does not exist 0x000000FE;
 *         Could not open SCM, other error 0x000000FF;
 * Note: The strange choice of error codes above SERVICE_PAUSED are chosen
 * in case Windows comes out with other service stats higher than 7, they
 * would likely call it 8 and above.  JS code that uses this in TestAUSHelper
 * only handles values up to 255 so that's why we don't use GetLastError
 * directly.
 */
DWORD
WaitForServiceStop(LPCWSTR serviceName, DWORD maxWaitSeconds)
{
  // 0x000000CF is defined above to be not set
  DWORD lastServiceState = 0x000000CF;

  // Get a handle to the SCM database.
  SC_HANDLE serviceManager = OpenSCManager(NULL, NULL,
                                           SC_MANAGER_CONNECT |
                                           SC_MANAGER_ENUMERATE_SERVICE);
  if (!serviceManager)  {
    DWORD lastError = GetLastError();
    switch(lastError) {
    case ERROR_ACCESS_DENIED:
      return 0x000000FD;
    case ERROR_DATABASE_DOES_NOT_EXIST:
      return 0x000000FE;
    default:
      return 0x000000FF;
    }
  }

  // Get a handle to the service.
  SC_HANDLE service = OpenServiceW(serviceManager,
                                   serviceName,
                                   SERVICE_QUERY_STATUS);
  if (!service) {
    DWORD lastError = GetLastError();
    CloseServiceHandle(serviceManager);
    switch(lastError) {
    case ERROR_ACCESS_DENIED:
      return 0x000000EB;
    case ERROR_INVALID_HANDLE:
      return 0x000000EC;
    case ERROR_INVALID_NAME:
      return 0x000000ED;
    case ERROR_SERVICE_DOES_NOT_EXIST:
      return 0x000000EE;
    default:
      return 0x000000EF;
    }
  }

  DWORD currentWaitMS = 0;
  SERVICE_STATUS_PROCESS ssp;
  ssp.dwCurrentState = lastServiceState;
  while (currentWaitMS < maxWaitSeconds * 1000) {
    DWORD bytesNeeded;
    if (!QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp,
                              sizeof(SERVICE_STATUS_PROCESS), &bytesNeeded)) {
      DWORD lastError = GetLastError();
      switch (lastError) {
      case ERROR_INVALID_HANDLE:
        ssp.dwCurrentState = 0x000000D9;
        break;
      case ERROR_ACCESS_DENIED:
        ssp.dwCurrentState = 0x000000DA;
        break;
      case ERROR_INSUFFICIENT_BUFFER:
        ssp.dwCurrentState = 0x000000DB;
        break;
      case ERROR_INVALID_PARAMETER:
        ssp.dwCurrentState = 0x000000DC;
        break;
      case ERROR_INVALID_LEVEL:
        ssp.dwCurrentState = 0x000000DD;
        break;
      case ERROR_SHUTDOWN_IN_PROGRESS:
        ssp.dwCurrentState = 0x000000DE;
        break;
      // These 3 errors can occur when the service is not yet stopped but
      // it is stopping.
      case ERROR_INVALID_SERVICE_CONTROL:
      case ERROR_SERVICE_CANNOT_ACCEPT_CTRL:
      case ERROR_SERVICE_NOT_ACTIVE:
        currentWaitMS += 50;
        Sleep(50);
        continue;
      default:
        ssp.dwCurrentState = 0x000000DF;
      }

      // We couldn't query the status so just break out
      break;
    }

    // The service is already in use.
    if (ssp.dwCurrentState == SERVICE_STOPPED) {
      break;
    }
    currentWaitMS += 50;
    Sleep(50);
  }

  lastServiceState = ssp.dwCurrentState;
  CloseServiceHandle(service);
  CloseServiceHandle(serviceManager);
  return lastServiceState;
}

#ifndef ONLY_SERVICE_LAUNCHING

/**
 * Determines if there is at least one process running for the specified
 * application. A match will be found across any session for any user.
 *
 * @param process The process to check for existance
 * @return ERROR_NOT_FOUND if the process was not found
 *         ERROR_SUCCESS if the process was found and there were no errors
 *         Other Win32 system error code for other errors
**/
DWORD
IsProcessRunning(LPCWSTR filename)
{
  // Take a snapshot of all processes in the system.
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (INVALID_HANDLE_VALUE == snapshot) {
    return GetLastError();
  }

  PROCESSENTRY32W processEntry;
  processEntry.dwSize = sizeof(PROCESSENTRY32W);
  if (!Process32FirstW(snapshot, &processEntry)) {
    DWORD lastError = GetLastError();
    CloseHandle(snapshot);
    return lastError;
  }

  do {
    if (wcsicmp(filename, processEntry.szExeFile) == 0) {
      CloseHandle(snapshot);
      return ERROR_SUCCESS;
    }
  } while (Process32NextW(snapshot, &processEntry));
  CloseHandle(snapshot);
  return ERROR_NOT_FOUND;
}

/**
 * Waits for the specified applicaiton to exit.
 *
 * @param filename   The application to wait for.
 * @param maxSeconds The maximum amount of seconds to wait for all
 *                   instances of the application to exit.
 * @return  ERROR_SUCCESS if no instances of the application exist
 *          WAIT_TIMEOUT if the process is still running after maxSeconds.
 *          Any other Win32 system error code.
*/
DWORD
WaitForProcessExit(LPCWSTR filename, DWORD maxSeconds)
{
  DWORD applicationRunningError = WAIT_TIMEOUT;
  for(DWORD i = 0; i < maxSeconds; i++) {
    DWORD applicationRunningError = IsProcessRunning(filename);
    if (ERROR_NOT_FOUND == applicationRunningError) {
      return ERROR_SUCCESS;
    }
    Sleep(1000);
  }

  if (ERROR_SUCCESS == applicationRunningError) {
    return WAIT_TIMEOUT;
  }

  return applicationRunningError;
}

/**
 *  Determines if the fallback key exists or not
 *
 *  @return TRUE if the fallback key exists and there was no error checking
*/
BOOL
DoesFallbackKeyExist()
{
  HKEY testOnlyFallbackKey;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                    TEST_ONLY_FALLBACK_KEY_PATH, 0,
                    KEY_READ | KEY_WOW64_64KEY,
                    &testOnlyFallbackKey) != ERROR_SUCCESS) {
    return FALSE;
  }

  RegCloseKey(testOnlyFallbackKey);
  return TRUE;
}

#endif

/**
 * Determines if the file system for the specified file handle is local
 * @param file path to check the filesystem type for, must be at most MAX_PATH
 * @param isLocal out parameter which will hold TRUE if the drive is local
 * @return TRUE if the call succeeded
*/
BOOL
IsLocalFile(LPCWSTR file, BOOL &isLocal)
{
  WCHAR rootPath[MAX_PATH + 1];
  if (wcslen(file) > MAX_PATH) {
    return FALSE;
  }

  wcscpy(rootPath, file);
  PathStripToRootW(rootPath);
  isLocal = GetDriveTypeW(rootPath) == DRIVE_FIXED;
  return TRUE;
}


/**
 * Determines the DWORD value of a registry key value
 *
 * @param key       The base key to where the value name exists
 * @param valueName The name of the value
 * @param retValue  Out parameter which will hold the value
 * @return TRUE on success
*/
static BOOL
GetDWORDValue(HKEY key, LPCWSTR valueName, DWORD &retValue)
{
  DWORD regDWORDValueSize = sizeof(DWORD);
  LONG retCode = RegQueryValueExW(key, valueName, 0, NULL,
                                  reinterpret_cast<LPBYTE>(&retValue),
                                  &regDWORDValueSize);
  return ERROR_SUCCESS == retCode;
}

/**
 * Determines if the the system's elevation type allows
 * unprmopted elevation.  This may not 100% reflect reality since
 * a reboot is necessary to change the UAC level.
 *
 * @param isUnpromptedElevation Out parameter which specifies if unprompted
 *                              elevation is allowed.
 * @return TRUE if the value was obtained successfully.
*/
BOOL
IsUnpromptedElevation(BOOL &isUnpromptedElevation)
{
  LPCWSTR UACBaseRegKey =
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System";
  HKEY baseKey;
  LONG retCode = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                               UACBaseRegKey, 0,
                               KEY_READ, &baseKey);
  if (retCode != ERROR_SUCCESS) {
    return FALSE;
  }

  DWORD enabled, consent, secureDesktop;
  BOOL success = GetDWORDValue(baseKey, L"EnableLUA", enabled);
  success = success &&
            GetDWORDValue(baseKey, L"ConsentPromptBehaviorAdmin", consent);
  success = success &&
            GetDWORDValue(baseKey, L"PromptOnSecureDesktop", secureDesktop);
  isUnpromptedElevation = enabled && !consent && !secureDesktop;

  RegCloseKey(baseKey);
  return success;
}

#ifdef MOZ_METRO
  /*
  * Retrieve the app model id of the firefox metro browser.
  *
  * @aPathBuffer Buffer to fill
  * @aCharLength Length of buffer to fill in characters
  */
  bool GetDefaultBrowserAppModelID(WCHAR* aIDBuffer, long aCharLength)
  {
    if (!aIDBuffer || aCharLength <= 0)
      return false;

    memset(aIDBuffer, 0, (sizeof(WCHAR)*aCharLength));
    static const WCHAR* kDefaultMetroBrowserIDPathKey = L"FirefoxURL";

    HKEY key;
    if (RegOpenKeyExW(HKEY_CLASSES_ROOT, kDefaultMetroBrowserIDPathKey,
                      0, KEY_READ, &key) != ERROR_SUCCESS) {
      return false;
    }
    DWORD len = aCharLength * sizeof(WCHAR);
    memset(aIDBuffer, 0, len);
    if (RegQueryValueExW(key, L"AppUserModelID", NULL, NULL,
                         (LPBYTE)aIDBuffer, &len) != ERROR_SUCCESS || !len) {
      RegCloseKey(key);
      return false;
    }
    RegCloseKey(key);
    return true;
  }

  HRESULT
  LaunchDefaultMetroBrowser()
  {
    CoInitialize(NULL);
    HRESULT hr = E_FAIL;
    // The interface that allows us to activate the browser
    IApplicationActivationManager *activateMgr;
    if (FAILED(hr = CoCreateInstance(CLSID_ApplicationActivationManager, NULL,
                                     CLSCTX_LOCAL_SERVER,
                                     IID_IApplicationActivationManager,
                                     (void**)&activateMgr))) {
      CoUninitialize();
      return hr;
    }

    // Activation is based on the browser's registered app model id
    WCHAR appModelID[256];
    if (!GetDefaultBrowserAppModelID(appModelID, (sizeof(appModelID)/sizeof(WCHAR)))) {
      activateMgr->Release();
      CoUninitialize();
      return hr;
    }

    // Hand off focus rights to the out-of-process activation server. Without
    // this the metro interface won't launch.
    CoAllowSetForegroundWindow(activateMgr, NULL);

    // Launch default browser in Metro
    DWORD processID;
    hr = activateMgr->ActivateApplication(appModelID, L"", AO_NOERRORUI,
                                          &processID);
    activateMgr->Release();
    CoUninitialize();
    return hr;
  }
#endif
