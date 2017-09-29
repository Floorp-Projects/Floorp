/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>
#include <stdio.h>
#include "nsUpdateDriver.h"
#include "nsXULAppAPI.h"
#include "nsAppRunner.h"
#include "nsIWritablePropertyBag.h"
#include "nsIFile.h"
#include "nsVariant.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "prproces.h"
#include "mozilla/Logging.h"
#include "prenv.h"
#include "nsVersionComparator.h"
#include "nsDirectoryServiceDefs.h"
#include "nsThreadUtils.h"
#include "nsIXULAppInfo.h"
#include "mozilla/Preferences.h"
#include "nsPrintfCString.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Printf.h"

#ifdef XP_MACOSX
#include "nsILocalFileMac.h"
#include "nsCommandLineServiceMac.h"
#include "MacLaunchHelper.h"
#include "updaterfileutils_osx.h"
#include "mozilla/Monitor.h"
#endif

#if defined(XP_WIN)
# include <direct.h>
# include <process.h>
# include <windows.h>
# include <shlwapi.h>
# define getcwd(path, size) _getcwd(path, size)
# define getpid() GetCurrentProcessId()
#elif defined(XP_UNIX)
# include <unistd.h>
# include <sys/wait.h>
#endif

using namespace mozilla;

static LazyLogModule sUpdateLog("updatedriver");
#define LOG(args) MOZ_LOG(sUpdateLog, mozilla::LogLevel::Debug, args)

#ifdef XP_WIN
#define UPDATER_BIN "updater.exe"
#elif XP_MACOSX
#define UPDATER_BIN "org.mozilla.updater"
#else
#define UPDATER_BIN "updater"
#endif
#define UPDATER_INI "updater.ini"
#ifdef XP_MACOSX
#define UPDATER_APP "updater.app"
#endif
#if defined(XP_UNIX) && !defined(XP_MACOSX)
#define UPDATER_PNG "updater.png"
#endif

#ifdef XP_MACOSX
static void
UpdateDriverSetupMacCommandLine(int& argc, char**& argv, bool restart)
{
  if (NS_IsMainThread()) {
    CommandLineServiceMac::SetupMacCommandLine(argc, argv, restart);
    return;
  }
  // Bug 1335916: SetupMacCommandLine calls a CoreFoundation function that
  // asserts that it was called from the main thread, so if we are not the main
  // thread, we have to dispatch that call to there. But we also have to get the
  // result from it, so we can't just dispatch and return, we have to wait
  // until the dispatched operation actually completes. So we also set up a
  // monitor to signal us when that happens, and block until then.
  Monitor monitor("nsUpdateDriver SetupMacCommandLine");

  nsresult rv = NS_DispatchToMainThread(
    NS_NewRunnableFunction("UpdateDriverSetupMacCommandLine",
                           [&argc, &argv, restart, &monitor]() -> void
    {
      CommandLineServiceMac::SetupMacCommandLine(argc, argv, restart);
      MonitorAutoLock(monitor).Notify();
    }));

  if (NS_FAILED(rv)) {
    LOG(("Update driver error dispatching SetupMacCommandLine to main thread: %d\n", rv));
    return;
  }

  // The length of this wait is arbitrary, but should be long enough that having
  // it expire means something is seriously wrong.
  rv = MonitorAutoLock(monitor).Wait(PR_SecondsToInterval(60));
  if (NS_FAILED(rv)) {
    LOG(("Update driver timed out waiting for SetupMacCommandLine: %d\n", rv));
  }
}
#endif

static nsresult
GetCurrentWorkingDir(char *buf, size_t size)
{
  // Cannot use NS_GetSpecialDirectory because XPCOM is not yet initialized.
  // This code is duplicated from xpcom/io/SpecialSystemDirectory.cpp:

#if defined(XP_WIN)
  wchar_t wpath[MAX_PATH];
  if (!_wgetcwd(wpath, size))
    return NS_ERROR_FAILURE;
  NS_ConvertUTF16toUTF8 path(wpath);
  strncpy(buf, path.get(), size);
#else
  if(!getcwd(buf, size))
    return NS_ERROR_FAILURE;
#endif
  return NS_OK;
}

/**
 * Get the path to the installation directory. For Mac OS X this will be the
 * bundle directory.
 *
 * @param appDir         the application directory file object
 * @param installDirPath the path to the installation directory
 */
static nsresult
GetInstallDirPath(nsIFile *appDir, nsACString& installDirPath)
{
  nsresult rv;
#ifdef XP_MACOSX
  nsCOMPtr<nsIFile> parentDir1, parentDir2;
  rv = appDir->GetParent(getter_AddRefs(parentDir1));
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = parentDir1->GetParent(getter_AddRefs(parentDir2));
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = parentDir2->GetNativePath(installDirPath);
#elif XP_WIN
  nsAutoString installDirPathW;
  rv = appDir->GetPath(installDirPathW);
  if (NS_FAILED(rv)) {
    return rv;
  }
  installDirPath = NS_ConvertUTF16toUTF8(installDirPathW);
#else
  rv = appDir->GetNativePath(installDirPath);
#endif
  if (NS_FAILED(rv)) {
    return rv;
  }
  return NS_OK;
}

#if defined(XP_MACOSX)
// This is a copy of OS X's XRE_GetBinaryPath from nsAppRunner.cpp with the
// gBinaryPath check removed so that the updater can reload the stub executable
// instead of xulrunner-bin. See bug 349737.
static nsresult
GetXULRunnerStubPath(const char* argv0, nsIFile* *aResult)
{
  // Works even if we're not bundled.
  CFBundleRef appBundle = ::CFBundleGetMainBundle();
  if (!appBundle)
    return NS_ERROR_FAILURE;

  CFURLRef bundleURL = ::CFBundleCopyExecutableURL(appBundle);
  if (!bundleURL)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsILocalFileMac> lfm;
  nsresult rv = NS_NewLocalFileWithCFURL(bundleURL, true, getter_AddRefs(lfm));

  ::CFRelease(bundleURL);

  if (NS_FAILED(rv))
    return rv;

  lfm.forget(aResult);
  return NS_OK;
}
#endif /* XP_MACOSX */

static bool
GetFile(nsIFile* dir, const nsACString& name, nsCOMPtr<nsIFile>& result)
{
  nsresult rv;

  nsCOMPtr<nsIFile> file;
  rv = dir->Clone(getter_AddRefs(file));
  if (NS_FAILED(rv))
    return false;

  rv = file->AppendNative(name);
  if (NS_FAILED(rv))
    return false;

  result = do_QueryInterface(file, &rv);
  return NS_SUCCEEDED(rv);
}

static bool
GetStatusFile(nsIFile *dir, nsCOMPtr<nsIFile> &result)
{
  return GetFile(dir, NS_LITERAL_CSTRING("update.status"), result);
}

/**
 * Get the contents of the update.status file.
 *
 * @param statusFile the status file object.
 * @param buf        the buffer holding the file contents
 *
 * @return true if successful, false otherwise.
 */
template <size_t Size>
static bool
GetStatusFileContents(nsIFile *statusFile, char (&buf)[Size])
{
  static_assert(Size > 16, "Buffer needs to be large enough to hold the known status codes");

  PRFileDesc *fd = nullptr;
  nsresult rv = statusFile->OpenNSPRFileDesc(PR_RDONLY, 0660, &fd);
  if (NS_FAILED(rv))
    return false;

  const int32_t n = PR_Read(fd, buf, Size);
  PR_Close(fd);

  return (n >= 0);
}

typedef enum {
  eNoUpdateAction,
  ePendingUpdate,
  ePendingService,
  ePendingElevate,
  eAppliedUpdate,
  eAppliedService,
} UpdateStatus;

/**
 * Returns a value indicating what needs to be done in order to handle an update.
 *
 * @param dir the directory in which we should look for an update.status file.
 * @param statusFile the update.status file found in the directory.
 *
 * @return the update action to be performed.
 */
static UpdateStatus
GetUpdateStatus(nsIFile* dir, nsCOMPtr<nsIFile> &statusFile)
{
  if (GetStatusFile(dir, statusFile)) {
    char buf[32];
    if (GetStatusFileContents(statusFile, buf)) {
      const char kPending[] = "pending";
      const char kPendingService[] = "pending-service";
      const char kPendingElevate[] = "pending-elevate";
      const char kApplied[] = "applied";
      const char kAppliedService[] = "applied-service";
      if (!strncmp(buf, kPendingElevate, sizeof(kPendingElevate) - 1)) {
        return ePendingElevate;
      }
      if (!strncmp(buf, kPendingService, sizeof(kPendingService) - 1)) {
        return ePendingService;
      }
      if (!strncmp(buf, kPending, sizeof(kPending) - 1)) {
        return ePendingUpdate;
      }
      if (!strncmp(buf, kAppliedService, sizeof(kAppliedService) - 1)) {
        return eAppliedService;
      }
      if (!strncmp(buf, kApplied, sizeof(kApplied) - 1)) {
        return eAppliedUpdate;
      }
    }
  }
  return eNoUpdateAction;
}

static bool
GetVersionFile(nsIFile *dir, nsCOMPtr<nsIFile> &result)
{
  return GetFile(dir, NS_LITERAL_CSTRING("update.version"), result);
}

// Compares the current application version with the update's application
// version.
static bool
IsOlderVersion(nsIFile *versionFile, const char *appVersion)
{
  PRFileDesc *fd = nullptr;
  nsresult rv = versionFile->OpenNSPRFileDesc(PR_RDONLY, 0660, &fd);
  if (NS_FAILED(rv))
    return true;

  char buf[32];
  const int32_t n = PR_Read(fd, buf, sizeof(buf));
  PR_Close(fd);

  if (n < 0)
    return false;

  // Trim off the trailing newline
  if (buf[n - 1] == '\n')
    buf[n - 1] = '\0';

  // If the update xml doesn't provide the application version the file will
  // contain the string "null" and it is assumed that the update is not older.
  const char kNull[] = "null";
  if (strncmp(buf, kNull, sizeof(kNull) - 1) == 0)
    return false;

  if (mozilla::Version(appVersion) > buf)
    return true;

  return false;
}

static bool
CopyFileIntoUpdateDir(nsIFile *parentDir, const nsACString& leaf, nsIFile *updateDir)
{
  nsCOMPtr<nsIFile> file;

  // Make sure there is not an existing file in the target location.
  nsresult rv = updateDir->Clone(getter_AddRefs(file));
  if (NS_FAILED(rv))
    return false;
  rv = file->AppendNative(leaf);
  if (NS_FAILED(rv))
    return false;
  file->Remove(true);

  // Now, copy into the target location.
  rv = parentDir->Clone(getter_AddRefs(file));
  if (NS_FAILED(rv))
    return false;
  rv = file->AppendNative(leaf);
  if (NS_FAILED(rv))
    return false;
  rv = file->CopyToNative(updateDir, EmptyCString());
  if (NS_FAILED(rv))
    return false;

  return true;
}

static bool
CopyUpdaterIntoUpdateDir(nsIFile *greDir, nsIFile *appDir, nsIFile *updateDir,
                         nsCOMPtr<nsIFile> &updater)
{
  // Copy the updater application from the GRE and the updater ini from the app.
#if defined(XP_MACOSX)
  if (!CopyFileIntoUpdateDir(appDir, NS_LITERAL_CSTRING(UPDATER_APP), updateDir))
    return false;
  CopyFileIntoUpdateDir(greDir, NS_LITERAL_CSTRING(UPDATER_INI), updateDir);
#else
  if (!CopyFileIntoUpdateDir(greDir, NS_LITERAL_CSTRING(UPDATER_BIN), updateDir))
    return false;
  CopyFileIntoUpdateDir(appDir, NS_LITERAL_CSTRING(UPDATER_INI), updateDir);
#endif
#if defined(XP_UNIX) && !defined(XP_MACOSX) && !defined(ANDROID)
  nsCOMPtr<nsIFile> iconDir;
  appDir->Clone(getter_AddRefs(iconDir));
  iconDir->AppendNative(NS_LITERAL_CSTRING("icons"));
  if (!CopyFileIntoUpdateDir(iconDir, NS_LITERAL_CSTRING(UPDATER_PNG), updateDir))
    return false;
#endif
  // Finally, return the location of the updater binary.
  nsresult rv = updateDir->Clone(getter_AddRefs(updater));
  if (NS_FAILED(rv))
    return false;
#if defined(XP_MACOSX)
  rv  = updater->AppendNative(NS_LITERAL_CSTRING(UPDATER_APP));
  nsresult tmp = updater->AppendNative(NS_LITERAL_CSTRING("Contents"));
  if (NS_FAILED(tmp)) {
    rv = tmp;
  }
  tmp = updater->AppendNative(NS_LITERAL_CSTRING("MacOS"));
  if (NS_FAILED(tmp) || NS_FAILED(rv))
    return false;
#endif
  rv = updater->AppendNative(NS_LITERAL_CSTRING(UPDATER_BIN));
  return NS_SUCCEEDED(rv);
}

/**
 * Appends the specified path to the library path.
 * This is used so that updater can find libmozsqlite3.so and other shared libs.
 *
 * @param pathToAppend A new library path to prepend to LD_LIBRARY_PATH
 */
#if defined(MOZ_VERIFY_MAR_SIGNATURE) && !defined(XP_WIN) && !defined(XP_MACOSX)
#include "prprf.h"
#define PATH_SEPARATOR ":"
#define LD_LIBRARY_PATH_ENVVAR_NAME "LD_LIBRARY_PATH"
static void
AppendToLibPath(const char *pathToAppend)
{
  char *pathValue = getenv(LD_LIBRARY_PATH_ENVVAR_NAME);
  if (nullptr == pathValue || '\0' == *pathValue) {
    // Leak the string because that is required by PR_SetEnv.
    char *s = Smprintf("%s=%s", LD_LIBRARY_PATH_ENVVAR_NAME, pathToAppend).release();
    PR_SetEnv(s);
  } else if (!strstr(pathValue, pathToAppend)) {
    // Leak the string because that is required by PR_SetEnv.
    char *s = Smprintf("%s=%s" PATH_SEPARATOR "%s",
                       LD_LIBRARY_PATH_ENVVAR_NAME, pathToAppend, pathValue).release();
    PR_SetEnv(s);
  }
}
#endif

/**
 * Applies, switches, or stages an update.
 *
 * @param greDir       the GRE directory
 * @param updateDir    the update root directory
 * @param appDir       the application directory
 * @param appArgc      the number of args passed to the application
 * @param appArgv      the args passed to the application
 *                     (used for restarting the application when necessary)
 * @param restart      true when a restart is necessary.
 * @param isStaged     true when the update has already been staged
 * @param outpid (out) parameter holding the handle to the updater application
 *                     when staging updates
 */
static void
ApplyUpdate(nsIFile *greDir, nsIFile *updateDir, nsIFile *appDir, int appArgc,
            char **appArgv, bool restart, bool isStaged, ProcessType *outpid)
{
  // The following determines the update operation to perform.
  // 1. When restart is false the update will be staged.
  // 2. When restart is true and isStaged is false the update will apply the mar
  //    file to the installation directory.
  // 3. When restart is true and isStaged is true the update will switch the
  //    staged update with the installation directory.

  nsresult rv;

  nsCOMPtr<nsIFile> updater;
  nsAutoCString updaterPath;
  nsAutoCString updateDirPath;
#if defined(XP_WIN)
  // Get an nsIFile reference for the updater in the installation dir.
  if (!GetFile(greDir, NS_LITERAL_CSTRING(UPDATER_BIN), updater)) {
    return;
  }

  // Get the path to the updater.
  nsAutoString updaterPathW;
  rv = updater->GetPath(updaterPathW);
  if (NS_FAILED(rv)) {
    return;
  }
  updaterPath = NS_ConvertUTF16toUTF8(updaterPathW);

  // Get the path to the update dir.
  nsAutoString updateDirPathW;
  rv = updateDir->GetPath(updateDirPathW);
  if (NS_FAILED(rv)) {
    return;
  }
  updateDirPath = NS_ConvertUTF16toUTF8(updateDirPathW);
#else
  if (isStaged) {
    nsCOMPtr<nsIFile> mozUpdaterDir;
    rv = updateDir->Clone(getter_AddRefs(mozUpdaterDir));
    if (NS_FAILED(rv)) {
      LOG(("failed cloning update dir\n"));
      return;
    }

    // Create a new directory named MozUpdater in the updates/0 directory to copy
    // the updater files to that will be used to replace the installation with the
    // staged application that has been updated. Note that we don't check for
    // directory creation errors since the call to CopyUpdaterIntoUpdateDir will
    // fail if the creation of the directory fails. A unique directory is created
    // in MozUpdater in case a previous attempt locked the directory or files.
    mozUpdaterDir->Append(NS_LITERAL_STRING("MozUpdater"));
    mozUpdaterDir->Append(NS_LITERAL_STRING("bgupdate"));
    rv = mozUpdaterDir->CreateUnique(nsIFile::DIRECTORY_TYPE, 0755);
    if (NS_FAILED(rv)) {
      LOG(("failed creating unique dir\n"));
      return;
    }

    // Copy the updater and files needed to update into the MozUpdater/bgupdate
    // directory in the update dir and get an nsIFile reference to the copied
    // updater.
    if (!CopyUpdaterIntoUpdateDir(greDir, appDir, mozUpdaterDir, updater)) {
      LOG(("failed copying updater\n"));
      return;
    }
  } else {
    // Copy the updater and files needed to update into the update directory and
    // get an nsIFile reference to the copied updater.
    if (!CopyUpdaterIntoUpdateDir(greDir, appDir, updateDir, updater)) {
      LOG(("failed copying updater\n"));
      return;
    }
  }

  // Get the path to the updater that will be used.
  rv = updater->GetNativePath(updaterPath);
  if (NS_FAILED(rv)) {
    return;
  }

  // Get the path to the update dir.
  rv = updateDir->GetNativePath(updateDirPath);
  if (NS_FAILED(rv)) {
     return;
  }
#endif

  // appFilePath and workingDirPath are only used when the application will be
  // restarted.
  nsAutoCString appFilePath;
  char workingDirPath[MAXPATHLEN];
  if (restart) {
    // Get the path to the current working directory.
    rv = GetCurrentWorkingDir(workingDirPath, sizeof(workingDirPath));
    if (NS_FAILED(rv)) {
      return;
    }

    // Get the application file path used by the updater to restart the
    // application after the update has finished.
    nsCOMPtr<nsIFile> appFile;
#if defined(XP_MACOSX)
    // On OS X we need to pass the location of the xulrunner-stub executable
    // rather than xulrunner-bin. See bug 349737.
    GetXULRunnerStubPath(appArgv[0], getter_AddRefs(appFile));
#else
    XRE_GetBinaryPath(appArgv[0], getter_AddRefs(appFile));
#endif
    if (!appFile) {
      return;
    }

#if defined(XP_WIN)
    nsAutoString appFilePathW;
    rv = appFile->GetPath(appFilePathW);
    if (NS_FAILED(rv)) {
      return;
    }
    appFilePath = NS_ConvertUTF16toUTF8(appFilePathW);
#else
    rv = appFile->GetNativePath(appFilePath);
    if (NS_FAILED(rv)) {
      return;
    }
#endif
  }

  // Get the installation directory path.
  nsAutoCString installDirPath;
  rv = GetInstallDirPath(appDir, installDirPath);
  if (NS_FAILED(rv)) {
    return;
  }

  nsAutoCString applyToDirPath;
  nsCOMPtr<nsIFile> updatedDir;
  if (restart && !isStaged) {
    // The install directory is the same as the apply to directory.
    applyToDirPath.Assign(installDirPath);
  } else {
    // Get the directory where the update is staged or will be staged.
#if defined(XP_MACOSX)
    if (!GetFile(updateDir, NS_LITERAL_CSTRING("Updated.app"), updatedDir)) {
#else
    if (!GetFile(appDir, NS_LITERAL_CSTRING("updated"), updatedDir)) {
#endif
      return;
    }
#if defined(XP_WIN)
    nsAutoString applyToDirPathW;
    rv = updatedDir->GetPath(applyToDirPathW);
    if (NS_FAILED(rv)) {
      return;
    }
    applyToDirPath = NS_ConvertUTF16toUTF8(applyToDirPathW);
#else
    rv = updatedDir->GetNativePath(applyToDirPath);
#endif
  }
  if (NS_FAILED(rv)) {
     return;
  }

  if (restart && isStaged) {
    // When the update should already be staged make sure that the updated
    // directory exists.
    bool updatedDirExists = false;
    if (NS_FAILED(updatedDir->Exists(&updatedDirExists)) || !updatedDirExists) {
      return;
    }
  }

  // On platforms where we are not calling execv, we may need to make the
  // updater executable wait for the calling process to exit.  Otherwise, the
  // updater may have trouble modifying our executable image (because it might
  // still be in use).  This is accomplished by passing our PID to the updater so
  // that it can wait for us to exit.  This is not perfect as there is a race
  // condition that could bite us.  It's possible that the calling process could
  // exit before the updater waits on the specified PID, and in the meantime a
  // new process with the same PID could be created.  This situation is unlikely,
  // however, given the way most operating systems recycle PIDs.  We'll take our
  // chances ;-)
  // Construct the PID argument for this process to pass to the updater.
  nsAutoCString pid;
  if (restart) {
#if defined(XP_UNIX) & !defined(XP_MACOSX)
    // When execv is used for an update that requires a restart 0 is passed
    // which is ignored by the updater.
    pid.AssignASCII("0");
#else
    pid.AppendInt((int32_t) getpid());
#endif
    if (isStaged) {
      // Append a special token to the PID in order to inform the updater that
      // it should replace install with the updated directory.
      pid.AppendLiteral("/replace");
    }
  } else {
    // Signal the updater application that it should stage the update.
    pid.AssignASCII("-1");
  }

  int argc = 5;
  if (restart) {
    argc = appArgc + 6;
  }
  char **argv = new char*[argc + 1];
  if (!argv) {
    return;
  }
  argv[0] = (char*) updaterPath.get();
  argv[1] = (char*) updateDirPath.get();
  argv[2] = (char*) installDirPath.get();
  argv[3] = (char*) applyToDirPath.get();
  argv[4] = (char*) pid.get();
  if (restart && appArgc) {
    argv[5] = workingDirPath;
    argv[6] = (char*) appFilePath.get();
    for (int i = 1; i < appArgc; ++i) {
      argv[6 + i] = appArgv[i];
    }
    argv[argc] = nullptr;
  } else {
    argv[5] = nullptr;
  }

  if (restart && gSafeMode) {
    PR_SetEnv("MOZ_SAFE_MODE_RESTART=1");
  }

#if defined(MOZ_VERIFY_MAR_SIGNATURE) && !defined(XP_WIN) && !defined(XP_MACOSX)
  AppendToLibPath(installDirPath.get());
#endif

  LOG(("spawning updater process [%s]\n", updaterPath.get()));

#if defined(XP_UNIX) && !defined(XP_MACOSX)
  // We use execv to spawn the updater process on all UNIX systems except Mac OSX
  // since it is known to cause problems on the Mac.  Windows has execv, but it
  // is a faked implementation that doesn't really replace the current process.
  // Instead it spawns a new process, so we gain nothing from using execv on
  // Windows.
  if (restart) {
    exit(execv(updaterPath.get(), argv));
  }
  *outpid = fork();
  if (*outpid == -1) {
    return;
  } else if (*outpid == 0) {
    exit(execv(updaterPath.get(), argv));
  }
#elif defined(XP_WIN)
  if (isStaged) {
    // Launch the updater to replace the installation with the staged updated.
    if (!WinLaunchChild(updaterPathW.get(), argc, argv)) {
      return;
    }
  } else {
    // Launch the updater to either stage or apply an update.
    if (!WinLaunchChild(updaterPathW.get(), argc, argv, nullptr, outpid)) {
      return;
    }
  }
#elif defined(XP_MACOSX)
  UpdateDriverSetupMacCommandLine(argc, argv, restart);
  // We need to detect whether elevation is required for this update. This can
  // occur when an admin user installs the application, but another admin
  // user attempts to update (see bug 394984).
  if (restart && !IsRecursivelyWritable(installDirPath.get())) {
    if (!LaunchElevatedUpdate(argc, argv, outpid)) {
      LOG(("Failed to launch elevated update!"));
      exit(1);
    }
    exit(0);
  }

  if (isStaged) {
    // Launch the updater to replace the installation with the staged updated.
    LaunchChildMac(argc, argv);
  } else {
    // Launch the updater to either stage or apply an update.
    LaunchChildMac(argc, argv, outpid);
  }
  if (restart) {
    exit(0);
  }
#else
  if (isStaged) {
    // Launch the updater to replace the installation with the staged updated.
    PR_CreateProcessDetached(updaterPath.get(), argv, nullptr, nullptr);
  } else {
    // Launch the updater to either stage or apply an update.
    *outpid = PR_CreateProcess(updaterPath.get(), argv, nullptr, nullptr);
  }
#endif
#if !defined(USE_EXECV)
  if (restart) {
    exit(0);
  }
#endif
}

/**
 * Wait briefly to see if a process terminates, then return true if it has.
 */
static bool
ProcessHasTerminated(ProcessType pt)
{
#if defined(XP_WIN)
  if (WaitForSingleObject(pt, 1000)) {
    return false;
  }
  CloseHandle(pt);
  return true;
#elif defined(XP_MACOSX)
  // We're waiting for the process to terminate in LaunchChildMac.
  return true;
#elif defined(XP_UNIX)
  int exitStatus;
  pid_t exited = waitpid(pt, &exitStatus, WNOHANG);
  if (exited == 0) {
    // Process is still running.
    sleep(1);
    return false;
  }
  if (exited == -1) {
    LOG(("Error while checking if the updater process is finished"));
    // This shouldn't happen, but if it does, the updater process is lost to us,
    // so the best we can do is pretend that it's exited.
    return true;
  }
  // If we get here, the process has exited; make sure it exited normally.
  if (WIFEXITED(exitStatus) && (WEXITSTATUS(exitStatus) != 0)) {
    LOG(("Error while running the updater process, check update.log"));
  }
  return true;
#else
  // No way to have a non-blocking implementation on these platforms,
  // because we're using NSPR and it only provides a blocking wait.
  int32_t exitCode;
  PR_WaitProcess(pt, &exitCode);
  if (exitCode != 0) {
    LOG(("Error while running the updater process, check update.log"));
  }
  return true;
#endif
}

nsresult
ProcessUpdates(nsIFile *greDir, nsIFile *appDir, nsIFile *updRootDir,
               int argc, char **argv, const char *appVersion,
               bool restart, ProcessType *pid)
{
  nsresult rv;

  nsCOMPtr<nsIFile> updatesDir;
  rv = updRootDir->Clone(getter_AddRefs(updatesDir));
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = updatesDir->AppendNative(NS_LITERAL_CSTRING("updates"));
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = updatesDir->AppendNative(NS_LITERAL_CSTRING("0"));
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Return early since there isn't a valid update when the update application
  // version file doesn't exist or if the update's application version is less
  // than the current application version. The cleanup of the update will happen
  // during post update processing in nsUpdateService.js.
  nsCOMPtr<nsIFile> versionFile;
  if (!GetVersionFile(updatesDir, versionFile) ||
      IsOlderVersion(versionFile, appVersion)) {
    return NS_OK;
  }

  nsCOMPtr<nsIFile> statusFile;
  UpdateStatus status = GetUpdateStatus(updatesDir, statusFile);
  switch (status) {
  case ePendingElevate: {
    if (NS_IsMainThread()) {
      // Only do this if we're called from the main thread.
      nsCOMPtr<nsIUpdatePrompt> up =
        do_GetService("@mozilla.org/updates/update-prompt;1");
      if (up) {
        up->ShowUpdateElevationRequired();
      }
      break;
    }
    // Intentional fallthrough to ePendingUpdate and ePendingService.
    MOZ_FALLTHROUGH;
  }
  case ePendingUpdate:
  case ePendingService: {
    ApplyUpdate(greDir, updatesDir, appDir, argc, argv, restart, false, pid);
    break;
  }
  case eAppliedUpdate:
  case eAppliedService:
    // An update was staged and needs to be switched so the updated application
    // is used.
    ApplyUpdate(greDir, updatesDir, appDir, argc, argv, restart, true, pid);
    break;
  case eNoUpdateAction:
    // We don't need to do any special processing here, we'll just continue to
    // startup the application.
    break;
  }

  return NS_OK;
}



NS_IMPL_ISUPPORTS(nsUpdateProcessor, nsIUpdateProcessor)

nsUpdateProcessor::nsUpdateProcessor()
  : mUpdaterPID(0)
{
}

nsUpdateProcessor::~nsUpdateProcessor()
{
}

NS_IMETHODIMP
nsUpdateProcessor::ProcessUpdate(nsIUpdate* aUpdate)
{
  nsresult rv;

  nsCOMPtr<nsIProperties> ds =
    do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFile> exeFile;
  rv = ds->Get(XRE_EXECUTABLE_FILE, NS_GET_IID(nsIFile),
               getter_AddRefs(exeFile));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFile> appDir;
  rv = exeFile->GetParent(getter_AddRefs(appDir));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFile> greDir;
  rv = ds->Get(NS_GRE_DIR, NS_GET_IID(nsIFile), getter_AddRefs(greDir));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFile> updRoot;
  rv = ds->Get(XRE_UPDATE_ROOT_DIR, NS_GET_IID(nsIFile),
               getter_AddRefs(updRoot));
  NS_ASSERTION(NS_SUCCEEDED(rv), "Can't get the UpdRootD dir");

  // XRE_UPDATE_ROOT_DIR should not fail but if it does fallback to the
  // application directory just to be safe.
  if (NS_FAILED(rv)) {
    rv = appDir->Clone(getter_AddRefs(updRoot));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIXULAppInfo> appInfo =
    do_GetService("@mozilla.org/xre/app-info;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString appVersion;
  rv = appInfo->GetVersion(appVersion);
  NS_ENSURE_SUCCESS(rv, rv);

  // Copy the parameters to the StagedUpdateInfo structure shared with the
  // watcher thread.
  mInfo.mGREDir = greDir;
  mInfo.mAppDir = appDir;
  mInfo.mUpdateRoot = updRoot;
  mInfo.mArgc = 0;
  mInfo.mArgv = nullptr;
  mInfo.mAppVersion = appVersion;

  MOZ_ASSERT(NS_IsMainThread(), "not main thread");
  nsCOMPtr<nsIRunnable> r =
    NewRunnableMethod("nsUpdateProcessor::StartStagedUpdate",
                      this,
                      &nsUpdateProcessor::StartStagedUpdate);
  return NS_NewNamedThread("Update Watcher", getter_AddRefs(mProcessWatcher),
                           r);
}



void
nsUpdateProcessor::StartStagedUpdate()
{
  MOZ_ASSERT(!NS_IsMainThread(), "main thread");

  nsresult rv = ProcessUpdates(mInfo.mGREDir,
                               mInfo.mAppDir,
                               mInfo.mUpdateRoot,
                               mInfo.mArgc,
                               mInfo.mArgv,
                               mInfo.mAppVersion.get(),
                               false,
                               &mUpdaterPID);
  NS_ENSURE_SUCCESS_VOID(rv);

  if (mUpdaterPID) {
    // Track the state of the updater process while it is staging an update.
    rv = NS_DispatchToCurrentThread(
      NewRunnableMethod("nsUpdateProcessor::WaitForProcess",
                        this,
                        &nsUpdateProcessor::WaitForProcess));
    NS_ENSURE_SUCCESS_VOID(rv);
  } else {
    // Failed to launch the updater process for some reason.
    // We need to shutdown the current thread as there isn't anything more for
    // us to do...
    rv = NS_DispatchToMainThread(
      NewRunnableMethod("nsUpdateProcessor::ShutdownWatcherThread",
                        this,
                        &nsUpdateProcessor::ShutdownWatcherThread));
    NS_ENSURE_SUCCESS_VOID(rv);
  }
}

void
nsUpdateProcessor::ShutdownWatcherThread()
{
  MOZ_ASSERT(NS_IsMainThread(), "not main thread");
  mProcessWatcher->Shutdown();
  mProcessWatcher = nullptr;
}

void
nsUpdateProcessor::WaitForProcess()
{
  MOZ_ASSERT(!NS_IsMainThread(), "main thread");
  if (ProcessHasTerminated(mUpdaterPID)) {
    NS_DispatchToMainThread(NewRunnableMethod(
      "nsUpdateProcessor::UpdateDone", this, &nsUpdateProcessor::UpdateDone));
  } else {
    NS_DispatchToCurrentThread(
      NewRunnableMethod("nsUpdateProcessor::WaitForProcess",
                        this,
                        &nsUpdateProcessor::WaitForProcess));
  }
}

void
nsUpdateProcessor::UpdateDone()
{
  MOZ_ASSERT(NS_IsMainThread(), "not main thread");

  nsCOMPtr<nsIUpdateManager> um =
    do_GetService("@mozilla.org/updates/update-manager;1");
  if (um) {
    um->RefreshUpdateStatus();
  }

  ShutdownWatcherThread();
}
