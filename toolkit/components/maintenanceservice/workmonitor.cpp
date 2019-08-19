/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <shlobj.h>
#include <shlwapi.h>
#include <wtsapi32.h>
#include <userenv.h>
#include <shellapi.h>

#ifndef __MINGW32__
#  pragma comment(lib, "wtsapi32.lib")
#  pragma comment(lib, "userenv.lib")
#  pragma comment(lib, "shlwapi.lib")
#  pragma comment(lib, "ole32.lib")
#  pragma comment(lib, "rpcrt4.lib")
#endif

#include "mozilla/CmdLineAndEnvUtils.h"
#include "nsWindowsHelpers.h"
#include "mozilla/UniquePtr.h"

using mozilla::MakeUnique;
using mozilla::UniquePtr;

#include "workmonitor.h"
#include "usertoken.h"
#include "serviceinstall.h"
#include "servicebase.h"
#include "registrycertificates.h"
#include "uachelper.h"
#include "updatehelper.h"
#include "pathhash.h"
#include "updatererrors.h"
#include "commonupdatedir.h"

#define PATCH_DIR_PATH L"\\updates\\0"

// Wait 15 minutes for an update operation to run at most.
// Updates usually take less than a minute so this seems like a
// significantly large and safe amount of time to wait.
static const int TIME_TO_WAIT_ON_UPDATER = 15 * 60 * 1000;
BOOL PathGetSiblingFilePath(LPWSTR destinationBuffer, LPCWSTR siblingFilePath,
                            LPCWSTR newFileName);
BOOL DoesFallbackKeyExist();

/*
 * Read the update.status file and sets isApplying to true if
 * the status is set to applying.
 *
 * @param  updateDirPath The directory where update.status is stored
 * @param  isApplying Out parameter for specifying if the status
 *         is set to applying or not.
 * @return TRUE if the information was filled.
 */
static BOOL IsStatusApplying(LPCWSTR updateDirPath, BOOL& isApplying) {
  isApplying = FALSE;
  WCHAR updateStatusFilePath[MAX_PATH + 1] = {L'\0'};
  wcsncpy(updateStatusFilePath, updateDirPath, MAX_PATH);
  if (!PathAppendSafe(updateStatusFilePath, L"update.status")) {
    LOG_WARN(("Could not append path for update.status file"));
    return FALSE;
  }

  nsAutoHandle statusFile(
      CreateFileW(updateStatusFilePath, GENERIC_READ,
                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                  nullptr, OPEN_EXISTING, 0, nullptr));

  if (INVALID_HANDLE_VALUE == statusFile) {
    LOG_WARN(("Could not open update.status file"));
    return FALSE;
  }

  char buf[32] = {0};
  DWORD read;
  if (!ReadFile(statusFile, buf, sizeof(buf), &read, nullptr)) {
    LOG_WARN(("Could not read from update.status file"));
    return FALSE;
  }

  const char kApplying[] = "applying";
  isApplying = strncmp(buf, kApplying, sizeof(kApplying) - 1) == 0;
  return TRUE;
}

/**
 * Determines whether we're staging an update.
 *
 * @param  argc    The argc value normally sent to updater.exe
 * @param  argv    The argv value normally sent to updater.exe
 * @return boolean True if we're staging an update
 */
static bool IsUpdateBeingStaged(int argc, LPWSTR* argv) {
  // PID will be set to -1 if we're supposed to stage an update.
  return (argc == 4 && !wcscmp(argv[3], L"-1")) ||
         (argc == 5 && !wcscmp(argv[4], L"-1"));
}

/**
 * Determines whether the param only contains digits.
 *
 * @param str     The string to check
 * @param boolean True if the param only contains digits
 */
static bool IsDigits(WCHAR* str) {
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
static bool IsOldCommandline(int argc, LPWSTR* argv) {
  return (argc == 4 && !wcscmp(argv[3], L"-1")) ||
         (argc >= 4 && (wcsstr(argv[3], L"/replace") || IsDigits(argv[3])));
}

/**
 * Gets the installation directory from the arguments passed to updater.exe.
 *
 * @param argcTmp    The argc value normally sent to updater.exe
 * @param argvTmp    The argv value normally sent to updater.exe
 * @param aResultDir Buffer to hold the installation directory.
 */
static BOOL GetInstallationDir(int argcTmp, LPWSTR* argvTmp,
                               WCHAR aResultDir[MAX_PATH + 1]) {
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
 * @param  userToken      User impersonation token to pass to the updater.
 * @return TRUE if the update process was run had a return code of 0.
 */
BOOL StartUpdateProcess(int argc, LPWSTR* argv, LPCWSTR installDir,
                        BOOL& processStarted, nsAutoHandle& userToken) {
  processStarted = FALSE;

  LOG(("Starting update process as the service in session 0."));
  STARTUPINFOEXW sie;
  ZeroMemory(&sie, sizeof(sie));
  sie.StartupInfo.cb = sizeof(sie);

  STARTUPINFOW& si = sie.StartupInfo;
  si.lpDesktop = const_cast<LPWSTR>(L"winsta0\\Default");  // -Wwritable-strings
  PROCESS_INFORMATION pi = {0};

  // The updater command line is of the form:
  // updater.exe update-dir apply [wait-pid [callback-dir callback-path args]]
  auto cmdLine = mozilla::MakeCommandLine(argc, argv);

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
    si.lpDesktop = const_cast<LPWSTR>(L"");  // -Wwritable-strings
    si.dwFlags |= STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
  }

  // Add an env var for MOZ_USING_SERVICE so the updater.exe can
  // do anything special that it needs to do for service updates.
  // Search in updater.cpp for more info on MOZ_USING_SERVICE.
  putenv(const_cast<char*>("MOZ_USING_SERVICE=1"));

#ifndef DISABLE_USER_IMPERSONATION
  // Add an env var with a pointer to the impersonation token.
  {
    static const char USER_TOKEN_FMT[] = USER_TOKEN_VAR_NAME "=%p";
    int fmtChars = _scprintf(USER_TOKEN_FMT, userToken.get());
    UniquePtr<char[]> userTokenEnv = MakeUnique<char[]>(fmtChars + 1);
    sprintf_s(userTokenEnv.get(), fmtChars + 1, USER_TOKEN_FMT,
              userToken.get());
    putenv(userTokenEnv.get());
  }
#endif

  // Prepare the attribute list used to inherit the impersonation token handle.
  {
#ifndef DISABLE_USER_IMPERSONATION
    SIZE_T attributeListSize = 0;
    const DWORD attributeListCount = 1;
    UniquePtr<char[]> attributeListBuf;

    if (InitializeProcThreadAttributeList(nullptr, attributeListCount, 0,
                                          &attributeListSize) ||
        (GetLastError() != ERROR_INSUFFICIENT_BUFFER) ||
        !(attributeListBuf = MakeUnique<char[]>(attributeListSize))) {
      LOG_WARN(("Failed to allocate attribute list for child process. (%d)",
                GetLastError()));
      return FALSE;
    }

    UniquePtr<_PROC_THREAD_ATTRIBUTE_LIST, ProcThreadAttributeListDeleter>
        attributeList(reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(
            attributeListBuf.get()));
    HANDLE handlesToInherit[] = {userToken};

    if (!InitializeProcThreadAttributeList(
            attributeList.get(), attributeListCount, 0, &attributeListSize) ||
        !UpdateProcThreadAttribute(
            attributeList.get(), 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
            handlesToInherit, sizeof(handlesToInherit), nullptr, nullptr)) {
      LOG_WARN(("Failed to create attribute list for child process. (%d)",
                GetLastError()));
      return FALSE;
    }

    sie.lpAttributeList = attributeList.get();
#endif

    LOG(("Starting service with cmdline: %ls", cmdLine.get()));
    processStarted =
        CreateProcessW(argv[0], cmdLine.get(), nullptr, nullptr, TRUE,
                       CREATE_DEFAULT_ERROR_MODE | EXTENDED_STARTUPINFO_PRESENT,
                       nullptr, nullptr, &si, &pi);
  }

  BOOL updateWasSuccessful = FALSE;
  if (processStarted) {
    BOOL processTerminated = FALSE;
    BOOL noProcessExitCode = FALSE;
    // Wait for the updater process to finish
    LOG(("Process was started... waiting on result."));
    DWORD waitRes = WaitForSingleObject(pi.hProcess, TIME_TO_WAIT_ON_UPDATER);
    if (WAIT_TIMEOUT == waitRes) {
      // We waited a long period of time for updater.exe and it never finished
      // so kill it.
      TerminateProcess(pi.hProcess, 1);
      processTerminated = TRUE;
    } else {
      // Check the return code of updater.exe to make sure we get 0
      DWORD returnCode;
      if (GetExitCodeProcess(pi.hProcess, &returnCode)) {
        LOG(("Process finished with return code %d.", returnCode));
        // updater returns 0 if successful.
        updateWasSuccessful = (returnCode == 0);
      } else {
        LOG_WARN(("Process finished but could not obtain return code."));
        noProcessExitCode = TRUE;
      }
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    // Check just in case updater.exe didn't change the status from
    // applying.  If this is the case we report an error.
    BOOL isApplying = FALSE;
    if (IsStatusApplying(argv[1], isApplying) && isApplying) {
      if (updateWasSuccessful) {
        LOG(
            ("update.status is still applying even though update was "
             "successful."));
        if (!WriteStatusFailure(argv[1], SERVICE_STILL_APPLYING_ON_SUCCESS,
                                userToken)) {
          LOG_WARN(
              ("Could not write update.status still applying on "
               "success error."));
        }
        // Since we still had applying we know updater.exe didn't do its
        // job correctly.
        updateWasSuccessful = FALSE;
      } else {
        LOG_WARN(
            ("update.status is still applying and update was not successful."));
        int failcode = SERVICE_STILL_APPLYING_ON_FAILURE;
        if (noProcessExitCode) {
          failcode = SERVICE_STILL_APPLYING_NO_EXIT_CODE;
        } else if (processTerminated) {
          failcode = SERVICE_STILL_APPLYING_TERMINATED;
        }
        if (!WriteStatusFailure(argv[1], failcode, userToken)) {
          LOG_WARN(
              ("Could not write update.status still applying on "
               "failure error."));
        }
      }
    }
  } else {
    DWORD lastError = GetLastError();
    LOG_WARN(
        ("Could not create process as current user, "
         "updaterPath: %ls; cmdLine: %ls.  (%d)",
         argv[0], cmdLine.get(), lastError));
  }

  // Empty value on putenv is how you remove an env variable in Windows
  putenv(const_cast<char*>("MOZ_USING_SERVICE="));
  putenv(const_cast<char*>(USER_TOKEN_VAR_NAME "="));

  return updateWasSuccessful;
}

/**
 * Validates a file as an official updater.
 *
 * @param updater     Path to the updater to validate
 * @param installDir  Path to the application installation
 *                    being updated
 * @param updateDir   Update applyTo direcotry,
 *                    where logs will be written
 * @param userToken   Impersonation token for writing update.status.
 *                    May be nullptr if we haven't validated the
 *                    install dir updater yet, in which case no
 *                    failure status will be written here.
 *
 * @return true if updater is the path to a valid updater
 */
static bool UpdaterIsValid(LPWSTR updater, LPWSTR installDir, LPWSTR updateDir,
                           nsAutoHandle& userToken) {
  // Make sure the path to the updater to use for the update is local.
  // We do this check to make sure that file locking is available for
  // race condition security checks.
  BOOL isLocal = FALSE;
  if (!IsLocalFile(updater, isLocal) || !isLocal) {
    LOG_WARN(("Filesystem in path %ls is not supported (%d)", updater,
              GetLastError()));
    if (
#ifndef DISABLE_USER_IMPERSONATION
        !userToken ||
#endif
        !WriteStatusFailure(updateDir, SERVICE_UPDATER_NOT_FIXED_DRIVE,
                            userToken)) {
      LOG_WARN(("Could not write update.status service update failure.  (%d)",
                GetLastError()));
    }
    return false;
  }

  nsAutoHandle noWriteLock(CreateFileW(updater, GENERIC_READ, FILE_SHARE_READ,
                                       nullptr, OPEN_EXISTING, 0, nullptr));
  if (INVALID_HANDLE_VALUE == noWriteLock) {
    LOG_WARN(("Could not set no write sharing access on file.  (%d)",
              GetLastError()));
    if (
#ifndef DISABLE_USER_IMPERSONATION
        !userToken ||
#endif
        !WriteStatusFailure(updateDir, SERVICE_COULD_NOT_LOCK_UPDATER,
                            userToken)) {
      LOG_WARN(("Could not write update.status service update failure.  (%d)",
                GetLastError()));
    }
    return false;
  }

  // Verify that the updater.exe that we are executing is the same
  // as the one in the installation directory which we are updating.
  // The installation dir that we are installing to is installDir.
  WCHAR installDirUpdater[MAX_PATH + 1] = {L'\0'};
  wcsncpy(installDirUpdater, installDir, MAX_PATH);
  if (!PathAppendSafe(installDirUpdater, L"updater.exe")) {
    LOG_WARN(("Install directory updater could not be determined."));
    return false;
  }

  BOOL updaterIsCorrect;
  if (!VerifySameFiles(updater, installDirUpdater, updaterIsCorrect)) {
    LOG_WARN(
        ("Error checking if the updaters are the same.\n"
         "Path 1: %ls\nPath 2: %ls",
         updater, installDirUpdater));
    return false;
  }

  if (!updaterIsCorrect) {
    LOG_WARN(
        ("The updaters do not match, updater will not run.\n"
         "Path 1: %ls\nPath 2: %ls",
         updater, installDirUpdater));
    if (
#ifndef DISABLE_USER_IMPERSONATION
        !userToken ||
#endif
        !WriteStatusFailure(updateDir, SERVICE_UPDATER_COMPARE_ERROR,
                            userToken)) {
      LOG_WARN(("Could not write update.status updater compare failure."));
    }
    return false;
  }

  LOG(
      ("updater.exe was compared successfully to the installation directory"
       " updater.exe."));

  // Check to make sure the updater.exe module has the unique updater identity.
  // This is a security measure to make sure that the signed executable that
  // we will run is actually an updater.
  bool result = true;
  HMODULE updaterModule =
      LoadLibraryEx(updater, nullptr, LOAD_LIBRARY_AS_DATAFILE);
  if (!updaterModule) {
    LOG_WARN(("updater.exe module could not be loaded. (%d)", GetLastError()));
    result = false;
  } else {
    char updaterIdentity[64];
    if (!LoadStringA(updaterModule, IDS_UPDATER_IDENTITY, updaterIdentity,
                     sizeof(updaterIdentity))) {
      LOG_WARN(
          ("The updater.exe application does not contain the Mozilla"
           " updater identity."));
      result = false;
    }

    if (strcmp(updaterIdentity, UPDATER_IDENTITY_STRING)) {
      LOG_WARN(("The updater.exe identity string is not valid."));
      result = false;
    }
    FreeLibrary(updaterModule);
  }

  if (result) {
    LOG(
        ("The updater.exe application contains the Mozilla"
         " updater identity."));
  } else {
    if (
#ifndef DISABLE_USER_IMPERSONATION
        !userToken ||
#endif
        !WriteStatusFailure(updateDir, SERVICE_UPDATER_IDENTITY_ERROR,
                            userToken)) {
      LOG_WARN(("Could not write update.status no updater identity."));
    }
    return false;
  }

#ifndef DISABLE_UPDATER_AUTHENTICODE_CHECK
  return DoesBinaryMatchAllowedCertificates(installDir, updater);
#else
  return true;
#endif
}

/**
 * Processes a software update command
 *
 * @param  argc           The number of arguments in argv
 * @param  argv           The arguments normally passed to updater.exe
 *                        argv[0] must be the path to updater.exe
 * @param  userToken      Impersonation token to use when writing
 *                        update.status, passed along to the updater
 *
 * @return TRUE if the update was successful.
 */
BOOL ProcessSoftwareUpdateCommand(DWORD argc, LPWSTR* argv,
                                  nsAutoHandle& userToken) {
  BOOL result = TRUE;
  if (argc < 3) {
    LOG_WARN(
        ("Not enough command line parameters specified. "
         "Updating update.status."));

    // We can only update update.status if argv[1] exists.  argv[1] is
    // the directory where the update.status file exists.
    if (argc < 2 ||
        !WriteStatusFailure(argv[1], SERVICE_NOT_ENOUGH_COMMAND_LINE_ARGS,
                            userToken)) {
      LOG_WARN(("Could not write update.status service update failure.  (%d)",
                GetLastError()));
    }
    return FALSE;
  }

  WCHAR installDir[MAX_PATH + 1] = {L'\0'};
  if (!GetInstallationDir(argc, argv, installDir)) {
    LOG_WARN(("Could not get the installation directory"));
    if (!WriteStatusFailure(argv[1], SERVICE_INSTALLDIR_ERROR, userToken)) {
      LOG_WARN(
          ("Could not write update.status for GetInstallationDir failure."));
    }
    return FALSE;
  }

  if (UpdaterIsValid(argv[0], installDir, argv[1], userToken)) {
    BOOL updateProcessWasStarted = FALSE;
    if (StartUpdateProcess(argc, argv, installDir, updateProcessWasStarted,
                           userToken)) {
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
        if (!WriteStatusFailure(argv[1], SERVICE_UPDATER_COULD_NOT_BE_STARTED,
                                userToken)) {
          LOG_WARN(
              ("Could not write update.status service update failure.  (%d)",
               GetLastError()));
        }
      }
    }
  } else {
    result = FALSE;
    LOG_WARN(
        ("Could not start process due to certificate check error on "
         "updater.exe. Updating update.status.  (%d)",
         GetLastError()));

    // When there is a certificate check error on the updater.exe application,
    // we want to write out the error.
    if (!WriteStatusFailure(argv[1], SERVICE_UPDATER_SIGN_ERROR, userToken)) {
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
BOOL GetSecureUpdaterPath(WCHAR serviceUpdaterPath[MAX_PATH + 1]) {
  if (!GetModuleFileNameW(nullptr, serviceUpdaterPath, MAX_PATH)) {
    LOG_WARN(
        ("Could not obtain module filename when attempting to "
         "use a secure updater path.  (%d)",
         GetLastError()));
    return FALSE;
  }

  if (!PathRemoveFileSpecW(serviceUpdaterPath)) {
    LOG_WARN(
        ("Couldn't remove file spec when attempting to use a secure "
         "updater path.  (%d)",
         GetLastError()));
    return FALSE;
  }

  if (!PathAppendSafe(serviceUpdaterPath, L"update")) {
    LOG_WARN(
        ("Couldn't append file spec when attempting to use a secure "
         "updater path.  (%d)",
         GetLastError()));
    return FALSE;
  }

  CreateDirectoryW(serviceUpdaterPath, nullptr);

  if (!PathAppendSafe(serviceUpdaterPath, L"updater.exe")) {
    LOG_WARN(
        ("Couldn't append file spec when attempting to use a secure "
         "updater path.  (%d)",
         GetLastError()));
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
BOOL DeleteSecureUpdater(WCHAR serviceUpdaterPath[MAX_PATH + 1]) {
  BOOL result = FALSE;
  if (serviceUpdaterPath[0]) {
    result = DeleteFileW(serviceUpdaterPath);
    if (!result && GetLastError() != ERROR_PATH_NOT_FOUND &&
        GetLastError() != ERROR_FILE_NOT_FOUND) {
      LOG_WARN(("Could not delete service updater path: '%ls'.",
                serviceUpdaterPath));
    }

    WCHAR updaterINIPath[MAX_PATH + 1] = {L'\0'};
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
 * @param argv The service command line arguments, argv[0] is automatically
 *             included by Windows, argv[1] is unused but hardcoded to
 *             "MozillaMaintenance". argv[2] is the service command.
 *
 * @return FALSE if there was an error executing the service command.
 */
BOOL ExecuteServiceCommand(int argc, LPWSTR* argv) {
  if (argc < 3) {
    LOG_WARN(
        ("Not enough command line arguments to execute a service command"));
    return FALSE;
  }

  // The tests work by making sure the log has changed, so we put a
  // unique ID in the log.
  GUID guid;
  HRESULT hr = CoCreateGuid(&guid);
  if (SUCCEEDED(hr)) {
    RPC_WSTR guidString = RPC_WSTR(L"");
    if (UuidToString(&guid, &guidString) == RPC_S_OK) {
      LOG(("Executing service command %ls, ID: %ls", argv[2],
           reinterpret_cast<LPCWSTR>(guidString)));
      RpcStringFree(&guidString);
    } else {
      // The ID is only used by tests, so failure to allocate it isn't fatal.
      LOG(("Executing service command %ls", argv[2]));
    }
  }

  BOOL result = FALSE;
  if (!lstrcmpi(argv[2], L"software-update")) {
    // Null userToken will be treated as "no impersonation needed" in most
    // cases, except UpdaterIsValid below.
    nsAutoHandle userToken;

    // This check is also performed in updater.cpp and is performed here
    // as well since the maintenance service can be called directly.
    if (argc < 4 || !IsValidFullPath(argv[4])) {
      // Since the status file is written to the patch directory and the patch
      // directory is invalid don't write the status file.
      LOG_WARN(("The patch directory path is not valid for this application."));
      return FALSE;
    }

    // The patch directory path must end with updates\0 to use the maintenance
    // service.
    size_t fullPathLen = NS_tstrlen(argv[4]);
    size_t relPathLen = NS_tstrlen(PATCH_DIR_PATH);
    if (relPathLen > fullPathLen) {
      LOG_WARN(
          ("The patch directory path length is not valid for this "
           "application."));
      return FALSE;
    }

    if (_wcsnicmp(argv[4] + fullPathLen - relPathLen, PATCH_DIR_PATH,
                  relPathLen) != 0) {
      LOG_WARN(
          ("The patch directory path subdirectory is not valid for this "
           "application."));
      return FALSE;
    }

    // This check is also performed in updater.cpp and is performed here
    // as well since the maintenance service can be called directly.
    if (argc < 5 || !IsValidFullPath(argv[5])) {
      LOG_WARN(
          ("The install directory path is not valid for this application."));
#ifdef DISABLE_USER_IMPERSONATION
      if (!WriteStatusFailure(argv[4], SERVICE_INVALID_INSTALL_DIR_PATH_ERROR,
                              userToken)) {
        LOG_WARN(("Could not write update.status for previous failure."));
      }
#else
      // No user token so we can't call
      // WriteStatusFailure(argv[4], SERVICE_INVALID_INSTALL_DIR_PATH_ERROR)
#endif
      return FALSE;
    }

    if (!IsOldCommandline(argc - 3, argv + 3)) {
      // This check is also performed in updater.cpp and is performed here
      // as well since the maintenance service can be called directly.
      if (argc < 6 || !IsValidFullPath(argv[6])) {
        LOG_WARN(
            ("The working directory path is not valid for this application."));
#ifdef DISABLE_USER_IMPERSONATION
        if (!WriteStatusFailure(argv[4], SERVICE_INVALID_WORKING_DIR_PATH_ERROR,
                                userToken)) {
          LOG_WARN(("Could not write update.status for previous failure."));
        }
#else
        // No user token so we can't call
        // WriteStatusFailure(argv[4], SERVICE_INVALID_WORKING_DIR_PATH_ERROR)
#endif
        return FALSE;
      }

      // These checks are also performed in updater.cpp and is performed here
      // as well since the maintenance service can be called directly.
      if (_wcsnicmp(argv[6], argv[5], MAX_PATH) != 0) {
        if (argc < 7 ||
            (wcscmp(argv[7], L"-1") != 0 && !wcsstr(argv[7], L"/replace"))) {
          LOG_WARN(
              ("Installation directory and working directory must be the "
               "same for non-staged updates. Exiting."));
#ifdef DISABLE_USER_IMPERSONATION
          if (!WriteStatusFailure(argv[4], SERVICE_INVALID_APPLYTO_DIR_ERROR,
                                  userToken)) {
            LOG_WARN(("Could not write update.status for previous failure."));
          }
#else
          // No user token so we can't call
          // WriteStatusFailure(argv[4], SERVICE_INVALID_APPLYTO_DIR_ERROR)
#endif
          return FALSE;
        }

        NS_tchar workingDirParent[MAX_PATH];
        NS_tsnprintf(workingDirParent,
                     sizeof(workingDirParent) / sizeof(workingDirParent[0]),
                     NS_T("%s"), argv[6]);
        if (!PathRemoveFileSpecW(workingDirParent)) {
          LOG_WARN(
              ("Couldn't remove file spec when attempting to verify the "
               "working directory path.  (%d)",
               GetLastError()));
#ifdef DISABLE_USER_IMPERSONATION
          if (!WriteStatusFailure(argv[4], REMOVE_FILE_SPEC_ERROR, userToken)) {
            LOG_WARN(("Could not write update.status for previous failure."));
          }
#else
          // No user token so we can't call
          // WriteStatusFailure(argv[4], REMOVE_FILE_SPEC_ERROR)
#endif
          return FALSE;
        }

        if (_wcsnicmp(workingDirParent, argv[5], MAX_PATH) != 0) {
          LOG_WARN(
              ("The apply-to directory must be the same as or "
               "a child of the installation directory! Exiting."));
#ifdef DISABLE_USER_IMPERSONATION
          if (!WriteStatusFailure(argv[4],
                                  SERVICE_INVALID_APPLYTO_DIR_STAGED_ERROR,
                                  userToken)) {
            LOG_WARN(("Could not write update.status for previous failure."));
          }
#else
          // No user token so we can't call
          // WriteStatusFailure(argv[4],
          // SERVICE_INVALID_APPLYTO_DIR_STAGED_ERROR)
#endif
          return FALSE;
        }
      }
    }

    // Use the passed in command line arguments for the update, except for the
    // path to updater.exe. We always look for updater.exe in the installation
    // directory, then we copy that updater.exe to a the directory of the
    // MozillaMaintenance service so that a low integrity process cannot
    // replace the updater.exe at any point and use that for the update.
    // It also makes DLL injection attacks harder.
    WCHAR installDir[MAX_PATH + 1] = {L'\0'};
    if (!GetInstallationDir(argc - 3, argv + 3, installDir)) {
      LOG_WARN(("Could not get the installation directory"));
#ifdef DISABLE_USER_IMPERSONATION
      if (!WriteStatusFailure(argv[4], SERVICE_INSTALLDIR_ERROR, userToken)) {
        LOG_WARN(("Could not write update.status for previous failure."));
      }
#else
      // No user token so we can't call
      // WriteStatusFailure(argv[4], SERVICE_INSTALLDIR_ERROR)
#endif
      return FALSE;
    }

    if (!DoesFallbackKeyExist()) {
      WCHAR maintenanceServiceKey[MAX_PATH + 1];
      if (CalculateRegistryPathFromFilePath(installDir,
                                            maintenanceServiceKey)) {
        LOG(("Checking for Maintenance Service registry. key: '%ls'",
             maintenanceServiceKey));
        HKEY baseKey = nullptr;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, maintenanceServiceKey, 0,
                          KEY_READ | KEY_WOW64_64KEY,
                          &baseKey) != ERROR_SUCCESS) {
          LOG_WARN(("The maintenance service registry key does not exist."));
#ifdef DISABLE_USER_IMPERSONATION
          if (!WriteStatusFailure(argv[4], SERVICE_INSTALL_DIR_REG_ERROR,
                                  userToken)) {
            LOG_WARN(("Could not write update.status for previous failure."));
          }
#else
          // No user token so we can't call
          // WriteStatusFailure(argv[4], SERVICE_INSTALL_DIR_REG_ERROR)
#endif
          return FALSE;
        }
        RegCloseKey(baseKey);
      } else {
#ifdef DISABLE_USER_IMPERSONATION
        if (!WriteStatusFailure(argv[4], SERVICE_CALC_REG_PATH_ERROR,
                                userToken)) {
          LOG_WARN(("Could not write update.status for previous failure."));
        }
#else
        // No user token so we can't call
        // WriteStatusFailure(argv[4], SERVICE_CALC_REG_PATH_ERROR)
#endif
        return FALSE;
      }
    }

    WCHAR installDirUpdater[MAX_PATH + 1] = {L'\0'};
    wcsncpy(installDirUpdater, installDir, MAX_PATH);
    if (!PathAppendSafe(installDirUpdater, L"updater.exe")) {
      LOG_WARN(("Install directory updater could not be determined."));
      result = FALSE;
    }

    // Intentionally passing null userToken so that UpdaterIsValid will not
    // write status.
    result = UpdaterIsValid(installDirUpdater, installDir, argv[4], userToken);

#ifndef DISABLE_USER_IMPERSONATION
    if (result) {
      userToken.own(GetUserProcessToken(installDirUpdater, argc - 3,
                                        const_cast<LPCWSTR*>(argv + 3)));
      result = !!userToken;
      if (!userToken) {
        LOG_WARN(("Could not get user process impersonation token"));
      }
    }
#endif

    WCHAR secureUpdaterPath[MAX_PATH + 1] = {L'\0'};
    if (result) {
      result = GetSecureUpdaterPath(secureUpdaterPath);  // Does its own logging
    }
    if (result) {
      LOG(
          ("Passed in path: '%ls' (ignored); "
           "Install dir has: '%ls'; "
           "Using this path for updating: '%ls'.",
           argv[3], installDirUpdater, secureUpdaterPath));
      DeleteSecureUpdater(secureUpdaterPath);
      result = CopyFileW(installDirUpdater, secureUpdaterPath, FALSE);
    }

    if (!result) {
      LOG_WARN(
          ("Could not copy path to secure location.  (%d)", GetLastError()));
      if (
#ifndef DISABLE_USER_IMPERSONATION
          !userToken ||
#endif
          !WriteStatusFailure(argv[4], SERVICE_COULD_NOT_COPY_UPDATER,
                              userToken)) {
        LOG_WARN(
            ("Could not write update.status could not copy updater error"));
      }
    } else {
      // We obtained the path and copied it successfully, update the path to
      // use for the service update.
      argv[3] = secureUpdaterPath;

      WCHAR installDirUpdaterINIPath[MAX_PATH + 1] = {L'\0'};
      WCHAR secureUpdaterINIPath[MAX_PATH + 1] = {L'\0'};
      if (PathGetSiblingFilePath(secureUpdaterINIPath, secureUpdaterPath,
                                 L"updater.ini") &&
          PathGetSiblingFilePath(installDirUpdaterINIPath, installDirUpdater,
                                 L"updater.ini")) {
        // This is non fatal if it fails there is no real harm
        if (!CopyFileW(installDirUpdaterINIPath, secureUpdaterINIPath, FALSE)) {
          LOG_WARN(("Could not copy updater.ini from: '%ls' to '%ls'.  (%d)",
                    installDirUpdaterINIPath, secureUpdaterINIPath,
                    GetLastError()));
        }
      }

      result = ProcessSoftwareUpdateCommand(argc - 3, argv + 3, userToken);
      DeleteSecureUpdater(secureUpdaterPath);
    }

    // We might not reach here if the service install succeeded
    // because the service self updates itself and the service
    // installer will stop the service.
    LOG(("Service command %ls complete.", argv[2]));
  } else if (!lstrcmpi(argv[2], L"fix-update-directory-perms")) {
    bool gotInstallDir = true;
    mozilla::UniquePtr<wchar_t[]> updateDir;
    if (argc <= 3) {
      LOG_WARN(("Didn't get an install dir for fix-update-directory-perms"));
      gotInstallDir = false;
    }
    HRESULT permResult =
        GetCommonUpdateDirectory(gotInstallDir ? argv[3] : nullptr,
                                 SetPermissionsOf::AllFilesAndDirs, updateDir);
    if (FAILED(permResult)) {
      LOG_WARN(
          ("Unable to set the permissions on the update directory "
           "('%S'): %d",
           updateDir.get(), permResult));
      result = FALSE;
    } else {
      result = TRUE;
    }
  } else {
    LOG_WARN(("Service command not recognized: %ls.", argv[2]));
    // result is already set to FALSE
  }

  LOG(("service command %ls complete with result: %ls.", argv[1],
       (result ? L"Success" : L"Failure")));
  return result;
}
