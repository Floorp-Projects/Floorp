/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <shlobj.h>
#include <shlwapi.h>
#include <wtsapi32.h>
#include <userenv.h>
#include <shellapi.h>

#pragma comment(lib, "wtsapi32.lib")
#pragma comment(lib, "userenv.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "rpcrt4.lib")

#include "nsWindowsHelpers.h"

#include "workmonitor.h"
#include "serviceinstall.h"
#include "servicebase.h"
#include "registrycertificates.h"
#include "uachelper.h"
#include "updatehelper.h"
#include "errors.h"

// Wait 15 minutes for an update operation to run at most.
// Updates usually take less than a minute so this seems like a
// significantly large and safe amount of time to wait.
static const int TIME_TO_WAIT_ON_UPDATER = 15 * 60 * 1000;
char16_t* MakeCommandLine(int argc, char16_t **argv);
BOOL WriteStatusFailure(LPCWSTR updateDirPath, int errorCode);
BOOL PathGetSiblingFilePath(LPWSTR destinationBuffer,  LPCWSTR siblingFilePath, 
                            LPCWSTR newFileName);

/*
 * Read the update.status file and sets isApplying to true if
 * the status is set to applying.
 *
 * @param  updateDirPath The directory where update.status is stored
 * @param  isApplying Out parameter for specifying if the status
 *         is set to applying or not.
 * @return TRUE if the information was filled.
*/
static BOOL
IsStatusApplying(LPCWSTR updateDirPath, BOOL &isApplying)
{
  isApplying = FALSE;
  WCHAR updateStatusFilePath[MAX_PATH + 1] = {L'\0'};
  wcsncpy(updateStatusFilePath, updateDirPath, MAX_PATH);
  if (!PathAppendSafe(updateStatusFilePath, L"update.status")) {
    LOG_WARN(("Could not append path for update.status file"));
    return FALSE;
  }

  nsAutoHandle statusFile(CreateFileW(updateStatusFilePath, GENERIC_READ,
                                      FILE_SHARE_READ |
                                      FILE_SHARE_WRITE |
                                      FILE_SHARE_DELETE,
                                      nullptr, OPEN_EXISTING, 0, nullptr));

  if (INVALID_HANDLE_VALUE == statusFile) {
    LOG_WARN(("Could not open update.status file"));
    return FALSE;
  }

  char buf[32] = { 0 };
  DWORD read;
  if (!ReadFile(statusFile, buf, sizeof(buf), &read, nullptr)) {
    LOG_WARN(("Could not read from update.status file"));
    return FALSE;
  }

  LOG(("updater.exe returned status: %s", buf));

  const char kApplying[] = "applying";
  isApplying = strncmp(buf, kApplying,
                       sizeof(kApplying) - 1) == 0;
  return TRUE;
}

/**
 * Determines whether we're staging an update.
 *
 * @param  argc    The argc value normally sent to updater.exe
 * @param  argv    The argv value normally sent to updater.exe
 * @return boolean True if we're staging an update
 */
static bool
IsUpdateBeingStaged(int argc, LPWSTR *argv)
{
  // PID will be set to -1 if we're supposed to stage an update.
  return argc == 4 && !wcscmp(argv[3], L"-1") ||
         argc == 5 && !wcscmp(argv[4], L"-1");
}

/**
 * Determines whether the param only contains digits.
 *
 * @param str     The string to check
 * @param boolean True if the param only contains digits
 */
static bool
IsDigits(WCHAR *str)
{
  while (*str) {
    if (!iswdigit(*str++)) {
      return FALSE;
    }
  }
  return TRUE;
}

/**
 * Determines whether the command line contains just the directory to apply the
 * update to (old command line) or if it contains the installation directory and
 * the directory to apply the update to.
 *
 * @param argc    The argc value normally sent to updater.exe
 * @param argv    The argv value normally sent to updater.exe
 * @param boolean True if the command line contains just the directory to apply
 *                the update to
 */
static bool
IsOldCommandline(int argc, LPWSTR *argv)
{
  return argc == 4 && !wcscmp(argv[3], L"-1") ||
         argc >= 4 && (wcsstr(argv[3], L"/replace") || IsDigits(argv[3]));
}

/**
 * Gets the installation directory from the arguments passed to updater.exe.
 *
 * @param argcTmp    The argc value normally sent to updater.exe
 * @param argvTmp    The argv value normally sent to updater.exe
 * @param aResultDir Buffer to hold the installation directory.
 */
static BOOL
GetInstallationDir(int argcTmp, LPWSTR *argvTmp, WCHAR aResultDir[MAX_PATH + 1])
{
  int index = 3;
  if (IsOldCommandline(argcTmp, argvTmp)) {
    index = 2;
  }

  if (argcTmp < index) {
    return FALSE;
  }

  wcsncpy(aResultDir, argvTmp[2], MAX_PATH);
  WCHAR* backSlash = wcsrchr(aResultDir, L'\\');
  // Make sure that the path does not include trailing backslashes
  if (backSlash && (backSlash[1] == L'\0')) {
    *backSlash = L'\0';
  }

  // The new command line's argv[2] is always the installation directory.
  if (index == 2) {
    bool backgroundUpdate = IsUpdateBeingStaged(argcTmp, argvTmp);
    bool replaceRequest = (argcTmp >= 4 && wcsstr(argvTmp[3], L"/replace"));
    if (backgroundUpdate || replaceRequest) {
      return PathRemoveFileSpecW(aResultDir);
    }
  }
  return TRUE;
}

/**
 * Runs an update process as the service using the SYSTEM account.
 *
 * @param  argc           The number of arguments in argv
 * @param  argv           The arguments normally passed to updater.exe
 *                        argv[0] must be the path to updater.exe
 * @param  processStarted Set to TRUE if the process was started.
 * @return TRUE if the update process was run had a return code of 0.
 */
BOOL
StartUpdateProcess(int argc,
                   LPWSTR *argv,
                   LPCWSTR installDir,
                   BOOL &processStarted)
{
  LOG(("Starting update process as the service in session 0."));
  STARTUPINFO si = {0};
  si.cb = sizeof(STARTUPINFO);
  si.lpDesktop = L"winsta0\\Default";
  PROCESS_INFORMATION pi = {0};

  // The updater command line is of the form:
  // updater.exe update-dir apply [wait-pid [callback-dir callback-path args]]
  LPWSTR cmdLine = MakeCommandLine(argc, argv);

  int index = 3;
  if (IsOldCommandline(argc, argv)) {
    index = 2;
  }

  // If we're about to start the update process from session 0,
  // then we should not show a GUI.  This only really needs to be done
  // on Vista and higher, but it's better to keep everything consistent
  // across all OS if it's of no harm.
  if (argc >= index) {
    // Setting the desktop to blank will ensure no GUI is displayed
    si.lpDesktop = L"";
    si.dwFlags |= STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
  }

  // We move the updater.ini file out of the way because we will handle
  // executing PostUpdate through the service.  We handle PostUpdate from
  // the service because there are some per user things that happen that
  // can't run in session 0 which we run updater.exe in.
  // Once we are done running updater.exe we rename updater.ini back so
  // that if there were any errors the next updater.exe will run correctly.
  WCHAR updaterINI[MAX_PATH + 1];
  WCHAR updaterINITemp[MAX_PATH + 1];
  BOOL selfHandlePostUpdate = FALSE;
  // We use the updater.ini from the same directory as the updater.exe
  // because of background updates.
  if (PathGetSiblingFilePath(updaterINI, argv[0], L"updater.ini") &&
      PathGetSiblingFilePath(updaterINITemp, argv[0], L"updater.tmp")) {
    selfHandlePostUpdate = MoveFileExW(updaterINI, updaterINITemp,
                                       MOVEFILE_REPLACE_EXISTING);
  }

  // Add an env var for MOZ_USING_SERVICE so the updater.exe can
  // do anything special that it needs to do for service updates.
  // Search in updater.cpp for more info on MOZ_USING_SERVICE.
  putenv(const_cast<char*>("MOZ_USING_SERVICE=1"));
  LOG(("Starting service with cmdline: %ls", cmdLine));
  processStarted = CreateProcessW(argv[0], cmdLine,
                                  nullptr, nullptr, FALSE,
                                  CREATE_DEFAULT_ERROR_MODE,
                                  nullptr,
                                  nullptr, &si, &pi);
  // Empty value on putenv is how you remove an env variable in Windows
  putenv(const_cast<char*>("MOZ_USING_SERVICE="));

  BOOL updateWasSuccessful = FALSE;
  if (processStarted) {
    // Wait for the updater process to finish
    LOG(("Process was started... waiting on result."));
    DWORD waitRes = WaitForSingleObject(pi.hProcess, TIME_TO_WAIT_ON_UPDATER);
    if (WAIT_TIMEOUT == waitRes) {
      // We waited a long period of time for updater.exe and it never finished
      // so kill it.
      TerminateProcess(pi.hProcess, 1);
    } else {
      // Check the return code of updater.exe to make sure we get 0
      DWORD returnCode;
      if (GetExitCodeProcess(pi.hProcess, &returnCode)) {
        LOG(("Process finished with return code %d.", returnCode));
        // updater returns 0 if successful.
        updateWasSuccessful = (returnCode == 0);
      } else {
        LOG_WARN(("Process finished but could not obtain return code."));
      }
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    // Check just in case updater.exe didn't change the status from
    // applying.  If this is the case we report an error.
    BOOL isApplying = FALSE;
    if (IsStatusApplying(argv[1], isApplying) && isApplying) {
      if (updateWasSuccessful) {
        LOG(("update.status is still applying even know update "
             " was successful."));
        if (!WriteStatusFailure(argv[1],
                                SERVICE_STILL_APPLYING_ON_SUCCESS)) {
          LOG_WARN(("Could not write update.status still applying on"
                    " success error."));
        }
        // Since we still had applying we know updater.exe didn't do its
        // job correctly.
        updateWasSuccessful = FALSE;
      } else {
        LOG_WARN(("update.status is still applying and update was not successful."));
        if (!WriteStatusFailure(argv[1],
                                SERVICE_STILL_APPLYING_ON_FAILURE)) {
          LOG_WARN(("Could not write update.status still applying on"
                    " success error."));
        }
      }
    }
  } else {
    DWORD lastError = GetLastError();
    LOG_WARN(("Could not create process as current user, "
              "updaterPath: %ls; cmdLine: %ls.  (%d)",
              argv[0], cmdLine, lastError));
  }

  // Now that we're done with the update, restore back the updater.ini file
  // We use it ourselves, and also we want it back in case we had any type
  // of error so that the normal update process can use it.
  if (selfHandlePostUpdate) {
    MoveFileExW(updaterINITemp, updaterINI, MOVEFILE_REPLACE_EXISTING);

    // Only run the PostUpdate if the update was successful
    if (updateWasSuccessful && argc > index) {
      LPCWSTR updateInfoDir = argv[1];
      bool stagingUpdate = IsUpdateBeingStaged(argc, argv);

      // Launch the PostProcess with admin access in session 0.  This is
      // actually launching the post update process but it takes in the
      // callback app path to figure out where to apply to.
      // The PostUpdate process with user only access will be done inside
      // the unelevated updater.exe after the update process is complete
      // from the service.  We don't know here which session to start
      // the user PostUpdate process from.
      // Note that we don't need to do this if we're just staging the
      // update in the background, as the PostUpdate step runs when
      // performing the replacing in that case.
      if (!stagingUpdate) {
        LOG(("Launching post update process as the service in session 0."));
        if (!LaunchWinPostProcess(installDir, updateInfoDir, true, nullptr)) {
          LOG_WARN(("The post update process could not be launched."
                    " installDir: %ls, updateInfoDir: %ls",
                    installDir, updateInfoDir));
        }
      }
    }
  }

  free(cmdLine);
  return updateWasSuccessful;
}

/**
 * Processes a software update command
 *
 * @param  argc           The number of arguments in argv
 * @param  argv           The arguments normally passed to updater.exe
 *                        argv[0] must be the path to updater.exe
 * @return TRUE if the update was successful.
 */
BOOL
ProcessSoftwareUpdateCommand(DWORD argc, LPWSTR *argv)
{
  BOOL result = TRUE;
  if (argc < 3) {
    LOG_WARN(("Not enough command line parameters specified. "
              "Updating update.status."));

    // We can only update update.status if argv[1] exists.  argv[1] is
    // the directory where the update.status file exists.
    if (argc < 2 ||
        !WriteStatusFailure(argv[1],
                            SERVICE_NOT_ENOUGH_COMMAND_LINE_ARGS)) {
      LOG_WARN(("Could not write update.status service update failure.  (%d)",
                GetLastError()));
    }
    return FALSE;
  }

  WCHAR installDir[MAX_PATH + 1] = {L'\0'};
  if (!GetInstallationDir(argc, argv, installDir)) {
    LOG_WARN(("Could not get the installation directory"));
    if (!WriteStatusFailure(argv[1],
                            SERVICE_INSTALLDIR_ERROR)) {
      LOG_WARN(("Could not write update.status for GetInstallationDir failure."));
    }
    return FALSE;
  }

  // Make sure the path to the updater to use for the update is local.
  // We do this check to make sure that file locking is available for
  // race condition security checks.
  BOOL isLocal = FALSE;
  if (!IsLocalFile(argv[0], isLocal) || !isLocal) {
    LOG_WARN(("Filesystem in path %ls is not supported (%d)",
              argv[0], GetLastError()));
    if (!WriteStatusFailure(argv[1],
                            SERVICE_UPDATER_NOT_FIXED_DRIVE)) {
      LOG_WARN(("Could not write update.status service update failure.  (%d)",
                GetLastError()));
    }
    return FALSE;
  }

  nsAutoHandle noWriteLock(CreateFileW(argv[0], GENERIC_READ, FILE_SHARE_READ,
                                       nullptr, OPEN_EXISTING, 0, nullptr));
  if (INVALID_HANDLE_VALUE == noWriteLock) {
      LOG_WARN(("Could not set no write sharing access on file.  (%d)",
                GetLastError()));
    if (!WriteStatusFailure(argv[1],
                            SERVICE_COULD_NOT_LOCK_UPDATER)) {
      LOG_WARN(("Could not write update.status service update failure.  (%d)",
                GetLastError()));
    }
    return FALSE;
  }

  // Verify that the updater.exe that we are executing is the same
  // as the one in the installation directory which we are updating.
  // The installation dir that we are installing to is installDir.
  WCHAR installDirUpdater[MAX_PATH + 1] = { L'\0' };
  wcsncpy(installDirUpdater, installDir, MAX_PATH);
  if (!PathAppendSafe(installDirUpdater, L"updater.exe")) {
    LOG_WARN(("Install directory updater could not be determined."));
    result = FALSE;
  }

  BOOL updaterIsCorrect;
  if (result && !VerifySameFiles(argv[0], installDirUpdater,
                                 updaterIsCorrect)) {
    LOG_WARN(("Error checking if the updaters are the same.\n"
              "Path 1: %ls\nPath 2: %ls", argv[0], installDirUpdater));
    result = FALSE;
  }

  if (result && !updaterIsCorrect) {
    LOG_WARN(("The updaters do not match, updater will not run.\n"
              "Path 1: %ls\nPath 2: %ls", argv[0], installDirUpdater));
    result = FALSE;
  }

  if (result) {
    LOG(("updater.exe was compared successfully to the installation directory"
         " updater.exe."));
  } else {
    if (!WriteStatusFailure(argv[1],
                            SERVICE_UPDATER_COMPARE_ERROR)) {
      LOG_WARN(("Could not write update.status updater compare failure."));
    }
    return FALSE;
  }

  // Check to make sure the updater.exe module has the unique updater identity.
  // This is a security measure to make sure that the signed executable that
  // we will run is actually an updater.
  HMODULE updaterModule = LoadLibraryEx(argv[0], nullptr,
                                        LOAD_LIBRARY_AS_DATAFILE);
  if (!updaterModule) {
    LOG_WARN(("updater.exe module could not be loaded. (%d)", GetLastError()));
    result = FALSE;
  } else {
    char updaterIdentity[64];
    if (!LoadStringA(updaterModule, IDS_UPDATER_IDENTITY,
                     updaterIdentity, sizeof(updaterIdentity))) {
      LOG_WARN(("The updater.exe application does not contain the Mozilla"
                " updater identity."));
      result = FALSE;
    }

    if (strcmp(updaterIdentity, UPDATER_IDENTITY_STRING)) {
      LOG_WARN(("The updater.exe identity string is not valid."));
      result = FALSE;
    }
    FreeLibrary(updaterModule);
  }

  if (result) {
    LOG(("The updater.exe application contains the Mozilla"
          " updater identity."));
  } else {
    if (!WriteStatusFailure(argv[1],
                            SERVICE_UPDATER_IDENTITY_ERROR)) {
      LOG_WARN(("Could not write update.status no updater identity."));
    }
    return TRUE;
  }

  // Check for updater.exe sign problems
  BOOL updaterSignProblem = FALSE;
#ifndef DISABLE_UPDATER_AUTHENTICODE_CHECK
  updaterSignProblem = !DoesBinaryMatchAllowedCertificates(installDir,
                                                           argv[0]);
#endif

  // Only proceed with the update if we have no signing problems
  if (!updaterSignProblem) {
    BOOL updateProcessWasStarted = FALSE;
    if (StartUpdateProcess(argc, argv, installDir,
                           updateProcessWasStarted)) {
      LOG(("updater.exe was launched and run successfully!"));
      LogFlush();

      // Don't attempt to update the service when the update is being staged.
      if (!IsUpdateBeingStaged(argc, argv)) {
        // We might not execute code after StartServiceUpdate because
        // the service installer will stop the service if it is running.
        StartServiceUpdate(installDir);
      }
    } else {
      result = FALSE;
      LOG_WARN(("Error running update process. Updating update.status  (%d)",
                GetLastError()));
      LogFlush();

      // If the update process was started, then updater.exe is responsible for
      // setting the failure code.  If it could not be started then we do the
      // work.  We set an error instead of directly setting status pending
      // so that the app.update.service.errors pref can be updated when
      // the callback app restarts.
      if (!updateProcessWasStarted) {
        if (!WriteStatusFailure(argv[1],
                                SERVICE_UPDATER_COULD_NOT_BE_STARTED)) {
          LOG_WARN(("Could not write update.status service update failure.  (%d)",
                    GetLastError()));
        }
      }
    }
  } else {
    result = FALSE;
    LOG_WARN(("Could not start process due to certificate check error on "
              "updater.exe. Updating update.status.  (%d)", GetLastError()));

    // When there is a certificate check error on the updater.exe application,
    // we want to write out the error.
    if (!WriteStatusFailure(argv[1],
                            SERVICE_UPDATER_SIGN_ERROR)) {
      LOG_WARN(("Could not write pending state to update.status.  (%d)",
                GetLastError()));
    }
  }

  return result;
}

/**
 * Obtains the updater path alongside a subdir of the service binary.
 * The purpose of this function is to return a path that is likely high
 * integrity and therefore more safe to execute code from.
 *
 * @param serviceUpdaterPath Out parameter for the path where the updater
 *                           should be copied to.
 * @return TRUE if a file path was obtained.
 */
BOOL
GetSecureUpdaterPath(WCHAR serviceUpdaterPath[MAX_PATH + 1])
{
  if (!GetModuleFileNameW(nullptr, serviceUpdaterPath, MAX_PATH)) {
    LOG_WARN(("Could not obtain module filename when attempting to "
              "use a secure updater path.  (%d)", GetLastError()));
    return FALSE;
  }

  if (!PathRemoveFileSpecW(serviceUpdaterPath)) {
    LOG_WARN(("Couldn't remove file spec when attempting to use a secure "
              "updater path.  (%d)", GetLastError()));
    return FALSE;
  }

  if (!PathAppendSafe(serviceUpdaterPath, L"update")) {
    LOG_WARN(("Couldn't append file spec when attempting to use a secure "
              "updater path.  (%d)", GetLastError()));
    return FALSE;
  }

  CreateDirectoryW(serviceUpdaterPath, nullptr);

  if (!PathAppendSafe(serviceUpdaterPath, L"updater.exe")) {
    LOG_WARN(("Couldn't append file spec when attempting to use a secure "
              "updater path.  (%d)", GetLastError()));
    return FALSE;
  }

  return TRUE;
}

/**
 * Deletes the passed in updater path and the associated updater.ini file.
 *
 * @param serviceUpdaterPath The path to delete.
 * @return TRUE if a file was deleted.
 */
BOOL
DeleteSecureUpdater(WCHAR serviceUpdaterPath[MAX_PATH + 1])
{
  BOOL result = FALSE;
  if (serviceUpdaterPath[0]) {
    result = DeleteFileW(serviceUpdaterPath);
    if (!result && GetLastError() != ERROR_PATH_NOT_FOUND &&
        GetLastError() != ERROR_FILE_NOT_FOUND) {
      LOG_WARN(("Could not delete service updater path: '%ls'.",
                serviceUpdaterPath));
    }

    WCHAR updaterINIPath[MAX_PATH + 1] = { L'\0' };
    if (PathGetSiblingFilePath(updaterINIPath, serviceUpdaterPath,
                               L"updater.ini")) {
      result = DeleteFileW(updaterINIPath);
      if (!result && GetLastError() != ERROR_PATH_NOT_FOUND &&
          GetLastError() != ERROR_FILE_NOT_FOUND) {
        LOG_WARN(("Could not delete service updater INI path: '%ls'.",
                  updaterINIPath));
      }
    }
  }
  return result;
}

/**
 * Executes a service command.
 *
 * @param argc The number of arguments in argv
 * @param argv The service command line arguments, argv[0] and argv[1]
 *             and automatically included by Windows.  argv[2] is the
 *             service command.
 *
 * @return FALSE if there was an error executing the service command.
 */
BOOL
ExecuteServiceCommand(int argc, LPWSTR *argv)
{
  if (argc < 3) {
    LOG_WARN(("Not enough command line arguments to execute a service command"));
    return FALSE;
  }

  // The tests work by making sure the log has changed, so we put a
  // unique ID in the log.
  RPC_WSTR guidString = RPC_WSTR(L"");
  GUID guid;
  HRESULT hr = CoCreateGuid(&guid);
  if (SUCCEEDED(hr)) {
    UuidToString(&guid, &guidString);
  }
  LOG(("Executing service command %ls, ID: %ls",
       argv[2], reinterpret_cast<LPCWSTR>(guidString)));
  RpcStringFree(&guidString);

  BOOL result = FALSE;
  if (!lstrcmpi(argv[2], L"software-update")) {

    // Use the passed in command line arguments for the update, except for the
    // path to updater.exe.  We copy updater.exe to a the directory of the
    // MozillaMaintenance service so that a low integrity process cannot
    // replace the updater.exe at any point and use that for the update.
    // It also makes DLL injection attacks harder.
    LPWSTR oldUpdaterPath = argv[3];
    WCHAR secureUpdaterPath[MAX_PATH + 1] = { L'\0' };
    result = GetSecureUpdaterPath(secureUpdaterPath); // Does its own logging
    if (result) {
      LOG(("Passed in path: '%ls'; Using this path for updating: '%ls'.",
           oldUpdaterPath, secureUpdaterPath));
      DeleteSecureUpdater(secureUpdaterPath);
      result = CopyFileW(oldUpdaterPath, secureUpdaterPath, FALSE);
    }

    if (!result) {
      LOG_WARN(("Could not copy path to secure location.  (%d)",
                GetLastError()));
      if (argc > 4 && !WriteStatusFailure(argv[4],
                                          SERVICE_COULD_NOT_COPY_UPDATER)) {
        LOG_WARN(("Could not write update.status could not copy updater error"));
      }
    } else {

      // We obtained the path and copied it successfully, update the path to
      // use for the service update.
      argv[3] = secureUpdaterPath;

      WCHAR oldUpdaterINIPath[MAX_PATH + 1] = { L'\0' };
      WCHAR secureUpdaterINIPath[MAX_PATH + 1] = { L'\0' };
      if (PathGetSiblingFilePath(secureUpdaterINIPath, secureUpdaterPath,
                                 L"updater.ini") &&
          PathGetSiblingFilePath(oldUpdaterINIPath, oldUpdaterPath,
                                 L"updater.ini")) {
        // This is non fatal if it fails there is no real harm
        if (!CopyFileW(oldUpdaterINIPath, secureUpdaterINIPath, FALSE)) {
          LOG_WARN(("Could not copy updater.ini from: '%ls' to '%ls'.  (%d)",
                    oldUpdaterINIPath, secureUpdaterINIPath, GetLastError()));
        }
      }

      result = ProcessSoftwareUpdateCommand(argc - 3, argv + 3);
      DeleteSecureUpdater(secureUpdaterPath);
    }

    // We might not reach here if the service install succeeded
    // because the service self updates itself and the service
    // installer will stop the service.
    LOG(("Service command %ls complete.", argv[2]));
  } else {
    LOG_WARN(("Service command not recognized: %ls.", argv[2]));
    // result is already set to FALSE
  }

  LOG(("service command %ls complete with result: %ls.",
       argv[1], (result ? L"Success" : L"Failure")));
  return TRUE;
}
