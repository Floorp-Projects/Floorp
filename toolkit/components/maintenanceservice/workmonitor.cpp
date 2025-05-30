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

#include "nsWindowsHelpers.h"

#include "mozilla/CmdLineAndEnvUtils.h"
#include "mozilla/NotNull.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"

using namespace mozilla;

#include "workmonitor.h"
#include "serviceinstall.h"
#include "servicebase.h"
#include "registrycertificates.h"
#include "uachelper.h"
#include "updatehelper.h"
#include "pathhash.h"
#include "updatererrors.h"
#include "commonupdatedir.h"

// Wait 15 minutes for an update operation to run at most.
// Updates usually take less than a minute so this seems like a
// significantly large and safe amount of time to wait.
static const int TIME_TO_WAIT_ON_UPDATER = 15 * 60 * 1000;
BOOL PathGetSiblingFilePath(LPWSTR destinationBuffer, LPCWSTR siblingFilePath,
                            LPCWSTR newFileName);
BOOL DoesFallbackKeyExist();

/**
 * The updater is always the same version as the application, so there is no
 * need for it to keep track of argument versioning. But the Maintenance
 * Service may be called upon to update old versions of the application that are
 * also installed. So it has to be able to handle any past argument format
 * version.
 */
enum UpdaterArgVersion {
  // The version 1 format looks like
  // updater patch-dir apply-to-dir wait-pid [callback-working-dir callback-path
  // args...]
  Version1,
  // The version 2 format looks like
  // updater patch-dir install-dir apply-to-dir [wait-pid [callback-working-dir
  // callback-path args...]]
  Version2,
  // The version 3 format looks like
  // updater 3 patch-dir install-dir apply-to-dir which-invocation [wait-pid
  // [callback-working-dir callback-path args...]]
  Version3,
};

/**
 * Represents the arguments passed to the MMS symbolically rather than
 * numerically so that we don't have to do a bunch of version checking and
 * index juggling every time we want a value.
 *
 * Should only be instantiated via `parseUpdaterArgs`.
 *
 * Raw character pointers will be references to within `argv` and are guaranteed
 * not to be `null`.
 *
 * It's very intentional that the only non-optional raw argument pointers are
 * the updater and the patch directory. It is important that `parseUpdaterArgs`
 * be as permissive as possible by always making a best effort attempt to return
 * at least the patch directory so that we can write a failure status there,
 * even if none of the other arguments are valid.
 */
struct UpdaterArgs {
  UpdaterArgVersion version;
  UniquePtr<wchar_t[]> fullCommandLine;
  NotNull<wchar_t*> updaterBin;
  NotNull<wchar_t*> patchDirPath;
  Maybe<NotNull<wchar_t*>> installDirPath;
  Maybe<NotNull<wchar_t*>> applyToDirPath;
  Maybe<NotNull<wchar_t*>> whichInvocation;
  Maybe<NotNull<wchar_t*>> waitPid;
  Maybe<NotNull<wchar_t*>> callbackWorkingDir;
  Maybe<NotNull<wchar_t*>> callbackBinPath;
  // The callback arguments are currently not included here (other than in
  // `fullCommandLine`) simply because we do not need them in the Maintenance
  // Service (other than to pass unmodified to the updater).
};

Maybe<NotNull<wchar_t*>> optionalArg(int argc, wchar_t** argv, int index) {
  if (argc > index) {
    return Some(WrapNotNull(argv[index]));
  }
  return Nothing();
}

/**
 * Determines whether the param only contains digits.
 *
 * @param str     The string to check
 * @param boolean True if the param only contains digits
 */
static bool isDigits(wchar_t* str) {
  while (*str) {
    if (!iswdigit(*str++)) {
      return FALSE;
    }
  }
  return TRUE;
}

void logParam(const char* name, Maybe<NotNull<wchar_t*>>& maybeValue) {
  if (maybeValue) {
    LOG(("Loaded param %s as \"%S\"", name, maybeValue.value().get()));
  } else {
    LOG(("Loaded param %s as Nothing", name));
  }
}

/**
 * See `UpdaterArgs`.
 * Returns `Nothing` if the arguments can't be parsed at all.
 */
Maybe<UpdaterArgs> parseUpdaterArgs(int argc, wchar_t** argv) {
  if (argc < 1) {
    LOG_WARN(("Argument parsing failed: No arguments!"));
    return Nothing();
  }
  Maybe<NotNull<wchar_t*>> updaterBin = Some(WrapNotNull(argv[0]));

  UniquePtr<wchar_t[]> fullCommandLine = mozilla::MakeCommandLine(argc, argv);
  LOG(("Command Line: %S", fullCommandLine.get()));

  UpdaterArgVersion version;
  Maybe<NotNull<wchar_t*>> patchDirPath = Nothing();
  Maybe<NotNull<wchar_t*>> installDirPath = Nothing();
  Maybe<NotNull<wchar_t*>> applyToDirPath = Nothing();
  Maybe<NotNull<wchar_t*>> whichInvocation = Nothing();
  Maybe<NotNull<wchar_t*>> waitPid = Nothing();
  Maybe<NotNull<wchar_t*>> callbackWorkingDir = Nothing();
  Maybe<NotNull<wchar_t*>> callbackBinPath = Nothing();
  if (argc > 1 && wcscmp(argv[1], L"3") == 0) {
    LOG(("Identified argument format version 3"));
    version = UpdaterArgVersion::Version3;

    // The version 3 format looks like
    // index   0    1     2         3            4              5
    //      updater 3 patch-dir install-dir apply-to-dir which-invocation
    // index    6            7                    8         9+
    //      [wait-pid [callback-working-dir callback-path args...]]
    if (argc < 3) {
      LOG_WARN(("No arguments for version 3"));
      return Nothing();
    }
    patchDirPath = Some(WrapNotNull(argv[2]));
    installDirPath = optionalArg(argc, argv, 3);
    applyToDirPath = optionalArg(argc, argv, 4);
    whichInvocation = optionalArg(argc, argv, 5);
    waitPid = optionalArg(argc, argv, 6);
    callbackWorkingDir = optionalArg(argc, argv, 7);
    callbackBinPath = optionalArg(argc, argv, 8);
  } else if ((argc == 4 && wcscmp(argv[3], L"-1") == 0) ||
             (argc >= 4 &&
              (wcsstr(argv[3], L"/replace") != nullptr || isDigits(argv[3])))) {
    LOG(("Identified argument format version 1"));
    version = UpdaterArgVersion::Version1;

    // The version 1 format looks like
    // index   0         1          2          3              4
    //      updater patch-dir apply-to-dir wait-pid [callback-working-dir
    // index      5         6+
    //      callback-path args...]
    patchDirPath = Some(WrapNotNull(argv[1]));
    applyToDirPath = Some(WrapNotNull(argv[2]));
    waitPid = Some(WrapNotNull(argv[3]));
    callbackWorkingDir = optionalArg(argc, argv, 4);
    callbackBinPath = optionalArg(argc, argv, 5);
  } else {
    LOG(("Identified argument format version 2"));
    version = UpdaterArgVersion::Version2;

    // The version 2 format looks like
    // index   0         1         2            3          4
    //      updater patch-dir install-dir apply-to-dir [wait-pid
    // index       5                    6         7+
    //      [callback-working-dir callback-path args...]]
    if (argc < 2) {
      LOG_WARN(("No arguments for version 2"));
      return Nothing();
    }
    patchDirPath = Some(WrapNotNull(argv[1]));
    installDirPath = optionalArg(argc, argv, 2);
    applyToDirPath = optionalArg(argc, argv, 3);
    waitPid = optionalArg(argc, argv, 4);
    callbackWorkingDir = optionalArg(argc, argv, 5);
    callbackBinPath = optionalArg(argc, argv, 6);
  }

  logParam("updaterBin", updaterBin);
  logParam("patchDirPath", patchDirPath);
  logParam("installDirPath", installDirPath);
  logParam("applyToDirPath", applyToDirPath);
  logParam("whichInvocation", whichInvocation);
  logParam("waitPid", waitPid);
  logParam("callbackWorkingDir", callbackWorkingDir);
  logParam("callbackBinPath", callbackBinPath);

  return Some(UpdaterArgs{
      .version = version,
      .fullCommandLine = std::move(fullCommandLine),
      .updaterBin = updaterBin.value(),
      .patchDirPath = patchDirPath.value(),
      .installDirPath = installDirPath,
      .applyToDirPath = applyToDirPath,
      .whichInvocation = whichInvocation,
      .waitPid = waitPid,
      .callbackWorkingDir = callbackWorkingDir,
      .callbackBinPath = callbackBinPath,
  });
}

/*
 * Reads the secure update status file and sets isApplying to true if the status
 * is set to applying.
 *
 * @param  patchDirPath
 *         The update patch directory path
 * @param  isApplying Out parameter for specifying if the status
 *         is set to applying or not.
 * @return TRUE if the information was filled.
 */
static BOOL IsStatusApplying(LPCWSTR patchDirPath, BOOL& isApplying) {
  isApplying = FALSE;
  WCHAR statusFilePath[MAX_PATH + 1] = {L'\0'};
  if (!GetSecureOutputFilePath(patchDirPath, L".status", statusFilePath)) {
    LOG_WARN(("Could not get path for the secure update status file"));
    return FALSE;
  }

  nsAutoHandle statusFile(
      CreateFileW(statusFilePath, GENERIC_READ,
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
 * @param  args    The updater arguments.
 * @return boolean True if we're staging an update
 */
static bool IsUpdateBeingStaged(const UpdaterArgs& args) {
  // PID will be set to -1 if we're supposed to stage an update.
  return args.waitPid && wcscmp(args.waitPid.value(), L"-1") == 0;
}

/**
 * Determines whether the update request we are servicing is a replace request.
 *
 * @param  args    The updater arguments.
 * @return boolean True if this is a replace request
 */
static bool IsUpdateAReplaceRequest(const UpdaterArgs& args) {
  return args.waitPid && wcsstr(args.waitPid.value(), L"/replace");
}

/**
 * Gets the installation directory from the arguments passed to updater.exe.
 *
 * @param args       The updater arguments.
 * @param aResultDir Buffer to hold the installation directory.
 */
static BOOL GetInstallationDir(const UpdaterArgs& args,
                               WCHAR aResultDir[MAX_PATH + 1]) {
  if (args.installDirPath) {
    wcsncpy(aResultDir, args.installDirPath.value(), MAX_PATH);
  } else if (args.applyToDirPath) {
    if (args.version != UpdaterArgVersion::Version1) {
      // In version 1, we infer the install directory from the "apply to"
      // directory (i.e. using it directly or converting "dir\Firefox\updated"
      // to "dir\Firefox"). But this is only an appropriate conversion to make
      // in version 1, when (a) the arguments were guaranteed to have
      // a format that would work like this, and (b) it was valid to not specify
      // the install directory as an argument.
      return FALSE;
    }
    wcsncpy(aResultDir, args.applyToDirPath.value(), MAX_PATH);
  } else {
    return FALSE;
  }

  WCHAR* backSlash = wcsrchr(aResultDir, L'\\');
  // Make sure that the path does not include trailing backslashes
  if (backSlash && (backSlash[1] == L'\0')) {
    *backSlash = L'\0';
  }

  // Handle the version 1 "dir\Firefox\updated" to "dir\Firefox" conversion.
  if (!args.installDirPath &&
      (IsUpdateBeingStaged(args) || IsUpdateAReplaceRequest(args))) {
    return PathRemoveFileSpecW(aResultDir);
  }
  return TRUE;
}

/**
 * Runs an update process as the service using the SYSTEM account.
 *
 * @param  args           The updater arguments.
 * @param  processStarted Set to TRUE if the process was started.
 * @return TRUE if the update process was run had a return code of 0.
 */
BOOL StartUpdateProcess(const UpdaterArgs& args, LPCWSTR installDir,
                        BOOL& processStarted) {
  processStarted = FALSE;

  LOG(("Starting update process as the service in session 0."));
  STARTUPINFOW si;
  PROCESS_INFORMATION pi;

  ZeroMemory(&si, sizeof(si));
  ZeroMemory(&pi, sizeof(pi));
  si.cb = sizeof(si);
  si.lpDesktop = const_cast<LPWSTR>(L"");  // -Wwritable-strings
  si.dwFlags = STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_HIDE;

  // Add an env var for MOZ_USING_SERVICE so the updater.exe can
  // do anything special that it needs to do for service updates.
  // Search in updater.cpp for more info on MOZ_USING_SERVICE.
  putenv(const_cast<char*>("MOZ_USING_SERVICE=1"));

  LOG(("Starting service with cmdline: %ls", args.fullCommandLine.get()));
  processStarted = CreateProcessW(
      args.updaterBin, args.fullCommandLine.get(), nullptr, nullptr, FALSE,
      CREATE_DEFAULT_ERROR_MODE, nullptr, nullptr, &si, &pi);

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
        LOG(("Process finished with return code %lu.", returnCode));
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
    if (IsStatusApplying(args.patchDirPath, isApplying) && isApplying) {
      if (updateWasSuccessful) {
        LOG(
            ("update.status is still applying even though update was "
             "successful."));
        if (!WriteStatusFailure(args.patchDirPath,
                                SERVICE_STILL_APPLYING_ON_SUCCESS)) {
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
        if (!WriteStatusFailure(args.patchDirPath, failcode)) {
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
         "updaterPath: %ls; cmdLine: %ls.  (%lu)",
         args.updaterBin.get(), args.fullCommandLine.get(), lastError));
  }

  // Empty value on putenv is how you remove an env variable in Windows
  putenv(const_cast<char*>("MOZ_USING_SERVICE="));

  return updateWasSuccessful;
}

/**
 * Validates a file as an official updater.
 *
 * @param updater     Path to the updater to validate
 * @param installDir  Path to the application installation
 *                    being updated
 * @param updateDir   Patch directory, where the update status file is.
 *
 * @return true if updater is the path to a valid updater
 */
static bool UpdaterIsValid(LPWSTR updater, LPWSTR installDir,
                           LPWSTR updateDir) {
  // Make sure the path to the updater to use for the update is local.
  // We do this check to make sure that file locking is available for
  // race condition security checks.
  BOOL isLocal = FALSE;
  if (!IsLocalFile(updater, isLocal) || !isLocal) {
    LOG_WARN(("Filesystem in path %ls is not supported (%lu)", updater,
              GetLastError()));
    if (!WriteStatusFailure(updateDir, SERVICE_UPDATER_NOT_FIXED_DRIVE)) {
      LOG_WARN(("Could not write update.status service update failure.  (%lu)",
                GetLastError()));
    }
    return false;
  }

  nsAutoHandle noWriteLock(CreateFileW(updater, GENERIC_READ, FILE_SHARE_READ,
                                       nullptr, OPEN_EXISTING, 0, nullptr));
  if (INVALID_HANDLE_VALUE == noWriteLock) {
    LOG_WARN(("Could not set no write sharing access on file.  (%lu)",
              GetLastError()));
    if (!WriteStatusFailure(updateDir, SERVICE_COULD_NOT_LOCK_UPDATER)) {
      LOG_WARN(("Could not write update.status service update failure.  (%lu)",
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
    if (!WriteStatusFailure(updateDir, SERVICE_UPDATER_COMPARE_ERROR)) {
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
    LOG_WARN(("updater.exe module could not be loaded. (%lu)", GetLastError()));
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
    if (!WriteStatusFailure(updateDir, SERVICE_UPDATER_IDENTITY_ERROR)) {
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
 * @param  args           The updater arguments.
 *
 * @return TRUE if the update was successful.
 */
BOOL ProcessSoftwareUpdateCommand(const UpdaterArgs& args) {
  BOOL result = TRUE;
  if (!args.installDirPath && !args.applyToDirPath) {
    LOG_WARN(
        ("Not enough command line parameters specified. "
         "Updating update.status."));

    if (!WriteStatusFailure(args.patchDirPath,
                            SERVICE_NOT_ENOUGH_COMMAND_LINE_ARGS)) {
      LOG_WARN(("Could not write update.status service update failure.  (%lu)",
                GetLastError()));
    }
    return FALSE;
  }

  WCHAR installDir[MAX_PATH + 1] = {L'\0'};
  if (!GetInstallationDir(args, installDir)) {
    LOG_WARN(("Could not get the installation directory"));
    if (!WriteStatusFailure(args.patchDirPath, SERVICE_INSTALLDIR_ERROR)) {
      LOG_WARN(
          ("Could not write update.status for GetInstallationDir failure."));
    }
    return FALSE;
  }

  if (UpdaterIsValid(args.updaterBin, installDir, args.patchDirPath)) {
    BOOL updateProcessWasStarted = FALSE;
    if (StartUpdateProcess(args, installDir, updateProcessWasStarted)) {
      LOG(("updater.exe was launched and run successfully!"));
      LogFlush();

      // Don't attempt to update the service when the update is being staged.
      if (!IsUpdateBeingStaged(args)) {
        // We might not execute code after StartServiceUpdate because
        // the service installer will stop the service if it is running.
        StartServiceUpdate(installDir);
      }
    } else {
      result = FALSE;
      LOG_WARN(("Error running update process. Updating update.status  (%lu)",
                GetLastError()));
      LogFlush();

      // If the update process was started, then updater.exe is responsible for
      // setting the failure code.  If it could not be started then we do the
      // work.  We set an error instead of directly setting status pending
      // so that the app.update.service.errors pref can be updated when
      // the callback app restarts.
      if (!updateProcessWasStarted) {
        if (!WriteStatusFailure(args.patchDirPath,
                                SERVICE_UPDATER_COULD_NOT_BE_STARTED)) {
          LOG_WARN(
              ("Could not write update.status service update failure.  (%lu)",
               GetLastError()));
        }
      }
    }
  } else {
    result = FALSE;
    LOG_WARN(
        ("Could not start process due to certificate check error on "
         "updater.exe. Updating update.status.  (%lu)",
         GetLastError()));

    // When there is a certificate check error on the updater.exe application,
    // we want to write out the error.
    if (!WriteStatusFailure(args.patchDirPath, SERVICE_UPDATER_SIGN_ERROR)) {
      LOG_WARN(("Could not write pending state to update.status.  (%lu)",
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
         "use a secure updater path.  (%lu)",
         GetLastError()));
    return FALSE;
  }

  if (!PathRemoveFileSpecW(serviceUpdaterPath)) {
    LOG_WARN(
        ("Couldn't remove file spec when attempting to use a secure "
         "updater path.  (%lu)",
         GetLastError()));
    return FALSE;
  }

  if (!PathAppendSafe(serviceUpdaterPath, L"update")) {
    LOG_WARN(
        ("Couldn't append file spec when attempting to use a secure "
         "updater path.  (%lu)",
         GetLastError()));
    return FALSE;
  }

  CreateDirectoryW(serviceUpdaterPath, nullptr);

  if (!PathAppendSafe(serviceUpdaterPath, L"updater.exe")) {
    LOG_WARN(
        ("Couldn't append file spec when attempting to use a secure "
         "updater path.  (%lu)",
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
 *             "MozillaMaintenance", and argv[2] is the service command.
 *
 * @return FALSE if there was an error executing the service command.
 */
BOOL ExecuteServiceCommand(int argc, LPWSTR* argv) {
  const int serviceArgCount = 3;
  if (argc < serviceArgCount) {
    LOG_WARN(
        ("Not enough command line arguments to execute a service command"));
    return FALSE;
  }

  const wchar_t* serviceName = argv[1];
  const wchar_t* serviceCommand = argv[2];

  // The tests work by making sure the log has changed, so we put a
  // unique ID in the log.
  WCHAR uuidString[MAX_PATH + 1] = {L'\0'};
  if (GetUUIDString(uuidString)) {
    LOG(("Executing service command %ls, ID: %ls", serviceCommand, uuidString));
  } else {
    // The ID is only used by tests, so failure to allocate it isn't fatal.
    LOG(("Executing service command %ls", serviceCommand));
  }

  BOOL result = FALSE;
  if (!lstrcmpi(serviceCommand, L"software-update")) {
    Maybe<UpdaterArgs> maybeArgs =
        parseUpdaterArgs(argc - serviceArgCount, argv + serviceArgCount);
    if (!maybeArgs) {
      // Not really much we can do here. `parseUpdaterArgs` is extremely
      // permissive. If it failed, we don't even have a patch directory to write
      // an error to.
      LOG_WARN(("Unable to parse updater arguments!"));
      return FALSE;
    }
    UpdaterArgs args = maybeArgs.extract();

    // This check is also performed in updater.cpp and is performed here
    // as well since the maintenance service can be called directly.
    if (!IsValidFullPath(args.patchDirPath)) {
      // Since the status file is written to the patch directory and the patch
      // directory is invalid don't write the status file.
      LOG_WARN(("The patch directory path is not valid for this application."));
      return FALSE;
    }

    // The patch directory path must end with updates\0 to use the maintenance
    // service.
    size_t fullPathLen = NS_tstrlen(args.patchDirPath);
    size_t relPathLen = NS_tstrlen(PATCH_DIR_PATH);
    if (relPathLen > fullPathLen) {
      LOG_WARN(
          ("The patch directory path length is not valid for this "
           "application."));
      return FALSE;
    }

    if (_wcsnicmp(args.patchDirPath + fullPathLen - relPathLen, PATCH_DIR_PATH,
                  relPathLen) != 0) {
      LOG_WARN(
          ("The patch directory path subdirectory is not valid for this "
           "application."));
      return FALSE;
    }

    // Remove the secure output files so it is easier to determine when new
    // files are created in the unelevated updater.
    RemoveSecureOutputFiles(args.patchDirPath);

    // Create a new secure ID for this update.
    if (!WriteSecureIDFile(args.patchDirPath)) {
      LOG_WARN(("Unable to write to secure ID file."));
      return FALSE;
    }

    if (args.version == UpdaterArgVersion::Version1) {
      // This check is also performed in updater.cpp and is performed here
      // as well since the maintenance service can be called directly.
      if (!args.applyToDirPath || !IsValidFullPath(args.applyToDirPath.value())
      // This build flag is used as a handy proxy to tell when we're a build
      // made for local testing, because there isn't much other reason to set
      // it.
#ifndef DISABLE_UPDATER_AUTHENTICODE_CHECK
          || !IsProgramFilesPath(args.applyToDirPath.value())
#endif
      ) {
        LOG_WARN(
            ("The apply-to directory path is not valid for this application."));
        if (!WriteStatusFailure(args.patchDirPath,
                                SERVICE_INVALID_INSTALL_DIR_PATH_ERROR)) {
          LOG_WARN(("Could not write update.status for previous failure."));
        }
        return FALSE;
      }
    } else {
      // This check is also performed in updater.cpp and is performed here
      // as well since the maintenance service can be called directly.
      if (!args.installDirPath || !IsValidFullPath(args.installDirPath.value())
      // This build flag is used as a handy proxy to tell when we're a build
      // made for local testing, because there isn't much other reason to set
      // it.
#ifndef DISABLE_UPDATER_AUTHENTICODE_CHECK
          || !IsProgramFilesPath(args.installDirPath.value())
#endif
      ) {
        LOG_WARN(
            ("The install directory path is not valid for this application."));
        if (!WriteStatusFailure(args.patchDirPath,
                                SERVICE_INVALID_INSTALL_DIR_PATH_ERROR)) {
          LOG_WARN(("Could not write update.status for previous failure."));
        }
        return FALSE;
      }

      // This check is also performed in updater.cpp and is performed here
      // as well since the maintenance service can be called directly.
      if (!args.applyToDirPath ||
          !IsValidFullPath(args.applyToDirPath.value())) {
        LOG_WARN(
            ("The working directory path is not valid for this application."));
        if (!WriteStatusFailure(args.patchDirPath,
                                SERVICE_INVALID_WORKING_DIR_PATH_ERROR)) {
          LOG_WARN(("Could not write update.status for previous failure."));
        }
        return FALSE;
      }

      // These checks are also performed in updater.cpp and is performed here
      // as well since the maintenance service can be called directly.
      if (_wcsnicmp(args.applyToDirPath.value(), args.installDirPath.value(),
                    MAX_PATH) != 0) {
        if (!IsUpdateBeingStaged(args) && !IsUpdateAReplaceRequest(args)) {
          LOG_WARN(
              ("Installation directory and working directory must be the "
               "same for non-staged updates. Exiting."));
          if (!WriteStatusFailure(args.patchDirPath,
                                  SERVICE_INVALID_APPLYTO_DIR_ERROR)) {
            LOG_WARN(("Could not write update.status for previous failure."));
          }
          return FALSE;
        }

        NS_tchar workingDirParent[MAX_PATH];
        NS_tsnprintf(workingDirParent,
                     sizeof(workingDirParent) / sizeof(workingDirParent[0]),
                     NS_T("%s"), args.applyToDirPath.value().get());
        if (!PathRemoveFileSpecW(workingDirParent)) {
          LOG_WARN(
              ("Couldn't remove file spec when attempting to verify the "
               "working directory path.  (%lu)",
               GetLastError()));
          if (!WriteStatusFailure(args.patchDirPath, REMOVE_FILE_SPEC_ERROR)) {
            LOG_WARN(("Could not write update.status for previous failure."));
          }
          return FALSE;
        }

        if (_wcsnicmp(workingDirParent, args.installDirPath.value(),
                      MAX_PATH) != 0) {
          LOG_WARN(
              ("The apply-to directory must be the same as or "
               "the direct child of the installation directory! Exiting."));
          if (!WriteStatusFailure(args.patchDirPath,
                                  SERVICE_INVALID_APPLYTO_DIR_STAGED_ERROR)) {
            LOG_WARN(("Could not write update.status for previous failure."));
          }
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
    if (!GetInstallationDir(args, installDir)) {
      LOG_WARN(("Could not get the installation directory"));
      if (!WriteStatusFailure(args.patchDirPath, SERVICE_INSTALLDIR_ERROR)) {
        LOG_WARN(("Could not write update.status for previous failure."));
      }
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
          if (!WriteStatusFailure(args.patchDirPath,
                                  SERVICE_INSTALL_DIR_REG_ERROR)) {
            LOG_WARN(("Could not write update.status for previous failure."));
          }
          return FALSE;
        }
        RegCloseKey(baseKey);
      } else {
        if (!WriteStatusFailure(args.patchDirPath,
                                SERVICE_CALC_REG_PATH_ERROR)) {
          LOG_WARN(("Could not write update.status for previous failure."));
        }
        return FALSE;
      }
    }

    WCHAR installDirUpdater[MAX_PATH + 1] = {L'\0'};
    wcsncpy(installDirUpdater, installDir, MAX_PATH);
    result = PathAppendSafe(installDirUpdater, L"updater.exe");
    if (!result) {
      LOG_WARN(("Install directory updater could not be determined."));
    }

    if (result) {
      result = UpdaterIsValid(installDirUpdater, installDir, args.patchDirPath);
    }

    WCHAR secureUpdaterPath[MAX_PATH + 1] = {L'\0'};
    if (result) {
      result = GetSecureUpdaterPath(secureUpdaterPath);  // Does its own logging
    }
    if (result) {
      LOG(
          ("Passed in path: '%ls' (ignored); "
           "Install dir has: '%ls'; "
           "Using this path for updating: '%ls'.",
           args.updaterBin.get(), installDirUpdater, secureUpdaterPath));
      DeleteSecureUpdater(secureUpdaterPath);
      result = CopyFileW(installDirUpdater, secureUpdaterPath, FALSE);
    }

    if (!result) {
      LOG_WARN(
          ("Could not copy path to secure location.  (%lu)", GetLastError()));
      if (!WriteStatusFailure(args.patchDirPath,
                              SERVICE_COULD_NOT_COPY_UPDATER)) {
        LOG_WARN(
            ("Could not write update.status could not copy updater error"));
      }
    } else {
      // We obtained the path and copied it successfully, update the path to
      // use for the service update.
      args.updaterBin = WrapNotNull(secureUpdaterPath);

      WCHAR installDirUpdaterINIPath[MAX_PATH + 1] = {L'\0'};
      WCHAR secureUpdaterINIPath[MAX_PATH + 1] = {L'\0'};
      if (PathGetSiblingFilePath(secureUpdaterINIPath, secureUpdaterPath,
                                 L"updater.ini") &&
          PathGetSiblingFilePath(installDirUpdaterINIPath, installDirUpdater,
                                 L"updater.ini")) {
        // This is non fatal if it fails there is no real harm
        if (!CopyFileW(installDirUpdaterINIPath, secureUpdaterINIPath, FALSE)) {
          LOG_WARN(("Could not copy updater.ini from: '%ls' to '%ls'.  (%lu)",
                    installDirUpdaterINIPath, secureUpdaterINIPath,
                    GetLastError()));
        }
      }

      result = ProcessSoftwareUpdateCommand(args);
      DeleteSecureUpdater(secureUpdaterPath);
    }

    // We might not reach here if the service install succeeded
    // because the service self updates itself and the service
    // installer will stop the service.
  } else {
    LOG_WARN(("Service command not recognized: %ls.", serviceCommand));
    // result is already set to FALSE
  }

  LOG(("%ls service command %ls complete with result: %ls.", serviceName,
       serviceCommand, result ? L"Success" : L"Failure"));
  return result;
}
