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
#include "nsXREDirProvider.h"
#include "SpecialSystemDirectory.h"
#include "nsDirectoryServiceDefs.h"
#include "nsThreadUtils.h"
#include "nsIXULAppInfo.h"
#include "mozilla/Preferences.h"
#include "nsPrintfCString.h"
#include "mozilla/DebugOnly.h"

#ifdef XP_MACOSX
#include "nsILocalFileMac.h"
#include "nsCommandLineServiceMac.h"
#include "MacLaunchHelper.h"
#endif

#if defined(XP_WIN)
# include <direct.h>
# include <process.h>
# include <windows.h>
# include <shlwapi.h>
# include "nsWindowsHelpers.h"
# define getcwd(path, size) _getcwd(path, size)
# define getpid() GetCurrentProcessId()
#elif defined(XP_UNIX)
# include <unistd.h>
#endif

using namespace mozilla;

//
// We use execv to spawn the updater process on all UNIX systems except Mac OSX
// since it is known to cause problems on the Mac.  Windows has execv, but it
// is a faked implementation that doesn't really replace the current process.
// Instead it spawns a new process, so we gain nothing from using execv on
// Windows.
//
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
//
// A similar #define lives in updater.cpp and should be kept in sync with this.
//
#if defined(XP_UNIX) && !defined(XP_MACOSX)
#define USE_EXECV
#endif

static PRLogModuleInfo *
GetUpdateLog()
{
  static PRLogModuleInfo *sUpdateLog;
  if (!sUpdateLog)
    sUpdateLog = PR_NewLogModule("updatedriver");
  return sUpdateLog;
}
#define LOG(args) MOZ_LOG(GetUpdateLog(), mozilla::LogLevel::Debug, args)

#ifdef XP_WIN
#define UPDATER_BIN "updater.exe"
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

#if defined(MOZ_WIDGET_GONK)
#include <linux/ioprio.h>

static const int kB2GServiceArgc = 2;
static const char *kB2GServiceArgv[] = { "/system/bin/start", "b2g" };

static const char kAppUpdaterPrio[]        = "app.update.updater.prio";
static const char kAppUpdaterOomScoreAdj[] = "app.update.updater.oom_score_adj";
static const char kAppUpdaterIOPrioClass[] = "app.update.updater.ioprio.class";
static const char kAppUpdaterIOPrioLevel[] = "app.update.updater.ioprio.level";

static const int  kAppUpdaterPrioDefault        = 19;     // -20..19 where 19 = lowest priority
static const int  kAppUpdaterOomScoreAdjDefault = -1000;  // -1000 = Never kill
static const int  kAppUpdaterIOPrioClassDefault = IOPRIO_CLASS_IDLE;
static const int  kAppUpdaterIOPrioLevelDefault = 0;      // Doesn't matter for CLASS IDLE
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
GetFile(nsIFile *dir, const nsCSubstring &name, nsCOMPtr<nsIFile> &result)
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
  // The buffer needs to be large enough to hold the known status codes
  PR_STATIC_ASSERT(Size > 16);

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
  eAppliedUpdate,
  eAppliedService
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
      const char kApplied[] = "applied";
      const char kAppliedService[] = "applied-service";
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
  // Copy the updater application from the GRE and the updater ini from the app
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
#if defined(MOZ_VERIFY_MAR_SIGNATURE) && !defined(XP_WIN) && \
    !defined(XP_MACOSX) && !defined(MOZ_WIDGET_GONK)
#include "prprf.h"
#define PATH_SEPARATOR ":"
#define LD_LIBRARY_PATH_ENVVAR_NAME "LD_LIBRARY_PATH"
static void
AppendToLibPath(const char *pathToAppend)
{
  char *pathValue = getenv(LD_LIBRARY_PATH_ENVVAR_NAME);
  if (nullptr == pathValue || '\0' == *pathValue) {
    char *s = PR_smprintf("%s=%s", LD_LIBRARY_PATH_ENVVAR_NAME, pathToAppend);
    PR_SetEnv(s);
  } else if (!strstr(pathValue, pathToAppend)) {
    char *s = PR_smprintf("%s=%s" PATH_SEPARATOR "%s",
                    LD_LIBRARY_PATH_ENVVAR_NAME, pathToAppend, pathValue);
    PR_SetEnv(s);
  }

  // The memory used by PR_SetEnv is not copied to the environment on all
  // platform, it can be used by reference directly. So we purposely do not
  // call PR_smprintf_free on s.  Subsequent calls to PR_SetEnv will free
  // the old memory first.
}
#endif

/**
 * Switch an existing application directory to an updated version that has been
 * staged.
 *
 * @param greDir the GRE dir
 * @param updateDir the update dir where the mar file is located
 * @param appDir the app dir
 * @param appArgc the number of args to the application
 * @param appArgv the args to the application, used for restarting if needed
 */
static void
SwitchToUpdatedApp(nsIFile *greDir, nsIFile *updateDir,
                   nsIFile *appDir, int appArgc, char **appArgv)
{
  nsresult rv;

  // Steps:
  //  - copy updater into updates/0/MozUpdater/bgupdate/ dir on all platforms
  //    except Windows
  //  - run updater with the correct arguments
#ifndef XP_WIN
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
  mozUpdaterDir->CreateUnique(nsIFile::DIRECTORY_TYPE, 0755);

  nsCOMPtr<nsIFile> updater;
  if (!CopyUpdaterIntoUpdateDir(greDir, appDir, mozUpdaterDir, updater)) {
    LOG(("failed copying updater\n"));
    return;
  }
#endif

  // We need to use the value returned from XRE_GetBinaryPath when attempting
  // to restart the running application.
  nsCOMPtr<nsIFile> appFile;

#if defined(XP_MACOSX)
  // On OS X we need to pass the location of the xulrunner-stub executable
  // rather than xulrunner-bin. See bug 349737.
  GetXULRunnerStubPath(appArgv[0], getter_AddRefs(appFile));
#else
  XRE_GetBinaryPath(appArgv[0], getter_AddRefs(appFile));
#endif

  if (!appFile)
    return;

#ifdef XP_WIN
  nsAutoString appFilePathW;
  rv = appFile->GetPath(appFilePathW);
  if (NS_FAILED(rv)) {
    return;
  }
  NS_ConvertUTF16toUTF8 appFilePath(appFilePathW);

  nsCOMPtr<nsIFile> updater;
  rv = greDir->Clone(getter_AddRefs(updater));
  if (NS_FAILED(rv)) {
    return;
  }

  rv = updater->AppendNative(NS_LITERAL_CSTRING(UPDATER_BIN));
  if (NS_FAILED(rv)) {
    return;
  }

  nsAutoString updaterPathW;
  rv = updater->GetPath(updaterPathW);
  if (NS_FAILED(rv)) {
    return;
  }
  NS_ConvertUTF16toUTF8 updaterPath(updaterPathW);
#else

  nsAutoCString appFilePath;
#if defined(MOZ_WIDGET_GONK)
  appFilePath.Assign(kB2GServiceArgv[0]);
  appArgc = kB2GServiceArgc;
  appArgv = const_cast<char**>(kB2GServiceArgv);
#else
  rv = appFile->GetNativePath(appFilePath);
  if (NS_FAILED(rv))
    return;
#endif

  nsAutoCString updaterPath;
  rv = updater->GetNativePath(updaterPath);
  if (NS_FAILED(rv))
    return;
#endif

  nsAutoCString installDirPath;
  rv = GetInstallDirPath(appDir, installDirPath);
  if (NS_FAILED(rv)) {
    return;
  }

  // Get the directory where the update will be staged.
  nsAutoCString applyToDir;
  nsCOMPtr<nsIFile> updatedDir;
#ifdef XP_MACOSX
  if (!GetFile(updateDir, NS_LITERAL_CSTRING("Updated.app"), updatedDir)) {
#else
  if (!GetFile(appDir, NS_LITERAL_CSTRING("updated"), updatedDir)) {
#endif
    return;
  }
#ifdef XP_WIN
  nsAutoString applyToDirW;
  rv = updatedDir->GetPath(applyToDirW);
  if (NS_FAILED(rv)) {
    return;
  }
  applyToDir = NS_ConvertUTF16toUTF8(applyToDirW);
#else
  rv = updatedDir->GetNativePath(applyToDir);
#endif
  if (NS_FAILED(rv)) {
    return;
  }

  // Make sure that the updated directory exists
  bool updatedDirExists = false;
  updatedDir->Exists(&updatedDirExists);
  if (!updatedDirExists) {
    return;
  }

#if defined(XP_WIN)
  nsAutoString updateDirPathW;
  rv = updateDir->GetPath(updateDirPathW);
  NS_ConvertUTF16toUTF8 updateDirPath(updateDirPathW);
#else
  nsAutoCString updateDirPath;
  rv = updateDir->GetNativePath(updateDirPath);
#endif
  if (NS_FAILED(rv))
    return;

  // Get the current working directory.
  char workingDirPath[MAXPATHLEN];
  rv = GetCurrentWorkingDir(workingDirPath, sizeof(workingDirPath));
  if (NS_FAILED(rv))
    return;

  // Construct the PID argument for this process.  If we are using execv, then
  // we pass "0" which is then ignored by the updater.
#if defined(USE_EXECV)
  nsAutoCString pid("0");
#else
  nsAutoCString pid;
  pid.AppendInt((int32_t) getpid());
#endif

  // Append a special token to the PID in order to let the updater know that it
  // just needs to replace the update directory.
  pid.AppendLiteral("/replace");

  int immersiveArgc = 0;
  int argc = appArgc + 6 + immersiveArgc;
  char **argv = new char*[argc + 1];
  if (!argv)
    return;
  argv[0] = (char*) updaterPath.get();
  argv[1] = (char*) updateDirPath.get();
  argv[2] = (char*) installDirPath.get();
  argv[3] = (char*) applyToDir.get();
  argv[4] = (char*) pid.get();
  if (appArgc) {
    argv[5] = workingDirPath;
    argv[6] = (char*) appFilePath.get();
    for (int i = 1; i < appArgc; ++i)
      argv[6 + i] = appArgv[i];
#ifdef XP_WIN
    if (immersiveArgc) {
      argv[argc - 1] = "-ServerName:DefaultBrowserServer";
    }
#endif
    argv[argc] = nullptr;
  } else {
    argc = 5;
    argv[5] = nullptr;
  }

  if (gSafeMode) {
    PR_SetEnv("MOZ_SAFE_MODE_RESTART=1");
  }
#if defined(MOZ_VERIFY_MAR_SIGNATURE) && !defined(XP_WIN) && \
    !defined(XP_MACOSX) && !defined(MOZ_WIDGET_GONK)
  AppendToLibPath(installDirPath.get());
#endif

  LOG(("spawning updater process for replacing [%s]\n", updaterPath.get()));

#if defined(USE_EXECV)
# if defined(MOZ_WIDGET_GONK)
  // In Gonk, we preload libmozglue, which the updater process doesn't need.
  // Since the updater will move and delete libmozglue.so, this can actually
  // stop the /system mount from correctly being remounted as read-only.
  unsetenv("LD_PRELOAD");
# endif
  execv(updaterPath.get(), argv);
#elif defined(XP_WIN)
  // Switch the application using updater.exe
  if (!WinLaunchChild(updaterPathW.get(), argc, argv)) {
    return;
  }
  _exit(0);
#elif defined(XP_MACOSX)
  CommandLineServiceMac::SetupMacCommandLine(argc, argv, true);
  // LaunchChildMac uses posix_spawnp and prefers the current
  // architecture when launching. It doesn't require a
  // null-terminated string but it doesn't matter if we pass one.
  LaunchChildMac(argc, argv);
  exit(0);
#else
  PR_CreateProcessDetached(updaterPath.get(), argv, nullptr, nullptr);
  exit(0);
#endif
}

#if defined(MOZ_WIDGET_GONK)
static nsresult
GetOSApplyToDir(nsACString& applyToDir)
{
  nsCOMPtr<nsIProperties> ds =
    do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID);
  NS_ASSERTION(ds, "Can't get directory service");

  nsCOMPtr<nsIFile> osApplyToDir;
  nsresult rv = ds->Get(XRE_OS_UPDATE_APPLY_TO_DIR, NS_GET_IID(nsIFile),
                                   getter_AddRefs(osApplyToDir));
  if (NS_FAILED(rv)) {
    LOG(("Can't get the OS applyTo dir"));
    return rv;
  }

  return osApplyToDir->GetNativePath(applyToDir);
}

static void
SetOSApplyToDir(nsIUpdate* update, const nsACString& osApplyToDir)
{
  nsresult rv;
  nsCOMPtr<nsIWritablePropertyBag> updateProperties =
    do_QueryInterface(update, &rv);

  if (NS_FAILED(rv)) {
    return;
  }

  RefPtr<nsVariant> variant = new nsVariant();
  rv = variant->SetAsACString(osApplyToDir);
  if (NS_FAILED(rv)) {
    return;
  }

  updateProperties->SetProperty(NS_LITERAL_STRING("osApplyToDir"), variant);
}
#endif

/**
 * Apply an update. This applies to both normal and staged updates.
 *
 * @param greDir the GRE dir
 * @param updateDir the update root dir
 * @param statusFile the update.status file
 * @param appDir the app dir
 * @param appArgc the number of args to the application
 * @param appArgv the args to the application, used for restarting if needed
 * @param restart if true, apply the update in the foreground and restart the
 *                application when done.  otherwise, stage the update and don't
 *                restart the application.
 * @param outpid out parameter holding the handle to the updater application for
 *               staging updates.
 */
static void
ApplyUpdate(nsIFile *greDir, nsIFile *updateDir, nsIFile *statusFile,
            nsIFile *appDir, int appArgc, char **appArgv,
            bool restart, bool isOSUpdate, nsIFile *osApplyToDir,
            ProcessType *outpid)
{
  nsresult rv;

  // Steps:
  //  - mark update as 'applying'
  //  - copy updater into update dir on all platforms except Windows
  //  - run updater w/ appDir as the current working dir
#ifndef XP_WIN
  nsCOMPtr<nsIFile> updater;
  if (!CopyUpdaterIntoUpdateDir(greDir, appDir, updateDir, updater)) {
    LOG(("failed copying updater\n"));
    return;
  }
#endif

  // We need to use the value returned from XRE_GetBinaryPath when attempting
  // to restart the running application.
  nsCOMPtr<nsIFile> appFile;

#if defined(XP_MACOSX)
  // On OS X we need to pass the location of the xulrunner-stub executable
  // rather than xulrunner-bin. See bug 349737.
  GetXULRunnerStubPath(appArgv[0], getter_AddRefs(appFile));
#else
  XRE_GetBinaryPath(appArgv[0], getter_AddRefs(appFile));
#endif

  if (!appFile)
    return;

#ifdef XP_WIN
  nsAutoString appFilePathW;
  rv = appFile->GetPath(appFilePathW);
  if (NS_FAILED(rv)) {
    return;
  }
  NS_ConvertUTF16toUTF8 appFilePath(appFilePathW);

  nsCOMPtr<nsIFile> updater;
  rv = greDir->Clone(getter_AddRefs(updater));
  if (NS_FAILED(rv)) {
    return;
  }

  rv = updater->AppendNative(NS_LITERAL_CSTRING(UPDATER_BIN));
  if (NS_FAILED(rv)) {
    return;
  }

  nsAutoString updaterPathW;
  rv = updater->GetPath(updaterPathW);
  if (NS_FAILED(rv)) {
    return;
  }
  NS_ConvertUTF16toUTF8 updaterPath(updaterPathW);
#else
  nsAutoCString appFilePath;
  rv = appFile->GetNativePath(appFilePath);
  if (NS_FAILED(rv))
    return;
  
  nsAutoCString updaterPath;
  rv = updater->GetNativePath(updaterPath);
  if (NS_FAILED(rv))
    return;

#endif

  nsAutoCString installDirPath;
  rv = GetInstallDirPath(appDir, installDirPath);
  if (NS_FAILED(rv))
    return;

  // Get the directory where the update was staged for replace and GONK OS
  // Updates or where it will be applied.
#ifndef MOZ_WIDGET_GONK
  // OS Updates are only supported on GONK so force it to false on everything
  // but GONK to simplify the following logic.
  isOSUpdate = false;
#endif
  nsAutoCString applyToDir;
  nsCOMPtr<nsIFile> updatedDir;
  if (restart && !isOSUpdate) {
    applyToDir.Assign(installDirPath);
  } else {
#ifdef XP_MACOSX
    if (!GetFile(updateDir, NS_LITERAL_CSTRING("Updated.app"), updatedDir)) {
#else
    if (!GetFile(appDir, NS_LITERAL_CSTRING("updated"), updatedDir)) {
#endif
      return;
    }
#ifdef XP_WIN
    nsAutoString applyToDirW;
    rv = updatedDir->GetPath(applyToDirW);
    if (NS_FAILED(rv)) {
      return;
    }
    applyToDir = NS_ConvertUTF16toUTF8(applyToDirW);
#elif MOZ_WIDGET_GONK
    if (isOSUpdate) {
      if (!osApplyToDir) {
        return;
      }
      rv = osApplyToDir->GetNativePath(applyToDir);
    } else {
      rv = updatedDir->GetNativePath(applyToDir);
    }
#else
    rv = updatedDir->GetNativePath(applyToDir);
#endif
  }
  if (NS_FAILED(rv))
    return;

#if defined(XP_WIN)
  nsAutoString updateDirPathW;
  rv = updateDir->GetPath(updateDirPathW);
  NS_ConvertUTF16toUTF8 updateDirPath(updateDirPathW);
#else
  nsAutoCString updateDirPath;
  rv = updateDir->GetNativePath(updateDirPath);
#endif
  if (NS_FAILED(rv)) {
    return;
  }

  // Get the current working directory.
  char workingDirPath[MAXPATHLEN];
  rv = GetCurrentWorkingDir(workingDirPath, sizeof(workingDirPath));
  if (NS_FAILED(rv))
    return;

  // We used to write out "Applying" to the update.status file here.
  // Instead we do this from within the updater application now.
  // This is so that we don't overwrite the status of pending-service
  // in the Windows case.  This change was made for all platforms so
  // that it stays consistent across all OS.

  // Construct the PID argument for this process.  If we are using execv, then
  // we pass "0" which is then ignored by the updater.
  nsAutoCString pid;
  if (!restart) {
    // Signal the updater application that it should stage the update.
    pid.AssignASCII("-1");
  } else {
#if defined(USE_EXECV)
    pid.AssignASCII("0");
#else
    pid.AppendInt((int32_t) getpid());
#endif
  }

  int immersiveArgc = 0;
  int argc = appArgc + 6 + immersiveArgc;
  char **argv = new char*[argc + 1 ];
  if (!argv)
    return;
  argv[0] = (char*) updaterPath.get();
  argv[1] = (char*) updateDirPath.get();
  argv[2] = (char*) installDirPath.get();
  argv[3] = (char*) applyToDir.get();
  argv[4] = (char*) pid.get();
  if (restart && appArgc) {
    argv[5] = workingDirPath;
    argv[6] = (char*) appFilePath.get();
    for (int i = 1; i < appArgc; ++i)
      argv[6 + i] = appArgv[i];
#ifdef XP_WIN
    if (immersiveArgc) {
      argv[argc - 1] = "-ServerName:DefaultBrowserServer";
    }
#endif
    argv[argc] = nullptr;
  } else {
    argc = 5;
    argv[5] = nullptr;
  }

  if (gSafeMode) {
    PR_SetEnv("MOZ_SAFE_MODE_RESTART=1");
  }
#if defined(MOZ_VERIFY_MAR_SIGNATURE) && !defined(XP_WIN) && \
    !defined(XP_MACOSX) && !defined(MOZ_WIDGET_GONK)
  AppendToLibPath(installDirPath.get());
#endif

  if (isOSUpdate) {
    PR_SetEnv("MOZ_OS_UPDATE=1");
  }
#if defined(MOZ_WIDGET_GONK)
  // We want the updater to be CPU friendly and not subject to being killed by
  // the low memory killer, so we pass in some preferences to allow it to
  // adjust its priority.

  int32_t prioVal = Preferences::GetInt(kAppUpdaterPrio,
                                        kAppUpdaterPrioDefault);
  int32_t oomScoreAdj = Preferences::GetInt(kAppUpdaterOomScoreAdj,
                                            kAppUpdaterOomScoreAdjDefault);
  int32_t ioprioClass = Preferences::GetInt(kAppUpdaterIOPrioClass,
                                            kAppUpdaterIOPrioClassDefault);
  int32_t ioprioLevel = Preferences::GetInt(kAppUpdaterIOPrioLevel,
                                            kAppUpdaterIOPrioLevelDefault);
  nsPrintfCString prioEnv("MOZ_UPDATER_PRIO=%d/%d/%d/%d",
                          prioVal, oomScoreAdj, ioprioClass, ioprioLevel);
  // Note: we allocate a new string on heap and pass that to PR_SetEnv, since
  // the string can be used after this function returns.  This means that we
  // will intentionally leak this buffer.
  PR_SetEnv(ToNewCString(prioEnv));
#endif

  LOG(("spawning updater process [%s]\n", updaterPath.get()));

#if defined(USE_EXECV)
  // Don't use execv when staging updates.
  if (restart) {
    execv(updaterPath.get(), argv);
  } else {
    *outpid = PR_CreateProcess(updaterPath.get(), argv, nullptr, nullptr);
  }
#elif defined(XP_WIN)
  // Launch the update using updater.exe
  if (!WinLaunchChild(updaterPathW.get(), argc, argv, nullptr, outpid)) {
    return;
  }

  if (restart) {
    // We are going to process an update so we should exit now
    _exit(0);
  }
#elif defined(XP_MACOSX)
  CommandLineServiceMac::SetupMacCommandLine(argc, argv, true);
  // LaunchChildMac uses posix_spawnp and prefers the current
  // architecture when launching. It doesn't require a
  // null-terminated string but it doesn't matter if we pass one.
  LaunchChildMac(argc, argv, 0, outpid);
  if (restart) {
    exit(0);
  }
#else
  *outpid = PR_CreateProcess(updaterPath.get(), argv, nullptr, nullptr);
  if (restart) {
    exit(0);
  }
#endif
}

/**
 * Wait for a process until it terminates.  This call is blocking.
 */
static void
WaitForProcess(ProcessType pt)
{
#if defined(XP_WIN)
  WaitForSingleObject(pt, INFINITE);
  CloseHandle(pt);
#elif defined(XP_MACOSX)
  waitpid(pt, 0, 0);
#else
  int32_t exitCode;
  PR_WaitProcess(pt, &exitCode);
  if (exitCode != 0) {
    LOG(("Error while running the updater process, check update.log"));
  }
#endif
}

nsresult
ProcessUpdates(nsIFile *greDir, nsIFile *appDir, nsIFile *updRootDir,
               int argc, char **argv, const char *appVersion,
               bool restart, bool isOSUpdate, nsIFile *osApplyToDir,
               ProcessType *pid)
{
  nsresult rv;

  nsCOMPtr<nsIFile> updatesDir;
  rv = updRootDir->Clone(getter_AddRefs(updatesDir));
  if (NS_FAILED(rv))
    return rv;

  rv = updatesDir->AppendNative(NS_LITERAL_CSTRING("updates"));
  if (NS_FAILED(rv))
    return rv;

  rv = updatesDir->AppendNative(NS_LITERAL_CSTRING("0"));
  if (NS_FAILED(rv))
    return rv;
 
  ProcessType dummyPID; // this will only be used for MOZ_UPDATE_STAGING
  const char *processingUpdates = PR_GetEnv("MOZ_TEST_PROCESS_UPDATES");
  if (processingUpdates && *processingUpdates) {
    // Enable the tests to request an update to be staged.
    const char *stagingUpdate = PR_GetEnv("MOZ_UPDATE_STAGING");
    if (stagingUpdate && *stagingUpdate) {
      restart = false;
      pid = &dummyPID;
    }
  }

  nsCOMPtr<nsIFile> statusFile;
  UpdateStatus status = GetUpdateStatus(updatesDir, statusFile);
  switch (status) {
  case ePendingUpdate:
  case ePendingService: {
    nsCOMPtr<nsIFile> versionFile;
    // Remove the update if the update application version file doesn't exist
    // or if the update's application version is less than the current
    // application version.
    if (!GetVersionFile(updatesDir, versionFile) ||
        IsOlderVersion(versionFile, appVersion)) {
      updatesDir->Remove(true);
    } else {
      ApplyUpdate(greDir, updatesDir, statusFile,
                  appDir, argc, argv, restart, isOSUpdate, osApplyToDir, pid);
    }
    break;
  }
  case eAppliedUpdate:
  case eAppliedService:
    // An update was staged and needs to be switched so the updated application
    // is used.
    SwitchToUpdatedApp(greDir, updatesDir, appDir, argc, argv);
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
  nsCOMPtr<nsIFile> greDir, appDir, updRoot;
  nsAutoCString appVersion;
  int argc;
  char **argv;

  nsAutoCString binPath;
  nsXREDirProvider* dirProvider = nsXREDirProvider::GetSingleton();
  if (dirProvider) { // Normal code path
    // Check for and process any available updates
    bool persistent;
    nsresult rv = NS_ERROR_FAILURE; // Take the NS_FAILED path when non-GONK
#ifdef MOZ_WIDGET_GONK
    // Check in the sdcard for updates first, since that's our preferred
    // download location.
    rv = dirProvider->GetFile(XRE_UPDATE_ARCHIVE_DIR, &persistent,
                              getter_AddRefs(updRoot));
#endif
    if (NS_FAILED(rv)) {
      rv = dirProvider->GetFile(XRE_UPDATE_ROOT_DIR, &persistent,
                                getter_AddRefs(updRoot));
    }
    // XRE_UPDATE_ROOT_DIR may fail. Fallback to appDir if failed
    if (NS_FAILED(rv))
      updRoot = dirProvider->GetAppDir();

    greDir = dirProvider->GetGREDir();
    nsCOMPtr<nsIFile> exeFile;
    rv = dirProvider->GetFile(XRE_EXECUTABLE_FILE, &persistent,
                              getter_AddRefs(exeFile));
    if (NS_SUCCEEDED(rv))
      rv = exeFile->GetParent(getter_AddRefs(appDir));

    if (NS_FAILED(rv))
      appDir = dirProvider->GetAppDir();

    appVersion = gAppData->version;
    argc = gRestartArgc;
    argv = gRestartArgv;
  } else {
    // In the xpcshell environment, the usual XRE_main is not run, so things
    // like dirProvider and gAppData do not exist.  This code path accesses
    // XPCOM (which is not available in the previous code path) in order to get
    // the same information.
    nsCOMPtr<nsIProperties> ds =
      do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID);
    if (!ds) {
      NS_ABORT(); // There's nothing which we can do if this fails!
    }

    nsresult rv = ds->Get(NS_GRE_DIR, NS_GET_IID(nsIFile),
                          getter_AddRefs(greDir));
    NS_ASSERTION(NS_SUCCEEDED(rv), "Can't get the GRE dir");

    nsCOMPtr<nsIFile> exeFile;
    rv = ds->Get(XRE_EXECUTABLE_FILE, NS_GET_IID(nsIFile),
                 getter_AddRefs(exeFile));
    if (NS_SUCCEEDED(rv))
      rv = exeFile->GetParent(getter_AddRefs(appDir));

    NS_ASSERTION(NS_SUCCEEDED(rv), "Can't get the XREExeF parent dir");

    rv = ds->Get(XRE_UPDATE_ROOT_DIR, NS_GET_IID(nsIFile),
                 getter_AddRefs(updRoot));
    NS_ASSERTION(NS_SUCCEEDED(rv), "Can't get the UpdRootD dir");

    nsCOMPtr<nsIXULAppInfo> appInfo =
      do_GetService("@mozilla.org/xre/app-info;1");
    if (appInfo) {
      rv = appInfo->GetVersion(appVersion);
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      appVersion = MOZ_APP_VERSION;
    }

    // We need argv[0] to point to the current executable's name.  The rest of
    // the entries in this array will be ignored if argc<2.  Therefore, for
    // xpcshell, we only fill out that item, and leave the rest empty.
    argc = 1;
    nsCOMPtr<nsIFile> binary;
    rv = ds->Get(XRE_EXECUTABLE_FILE, NS_GET_IID(nsIFile),
                 getter_AddRefs(binary));
    NS_ASSERTION(NS_SUCCEEDED(rv), "Can't get the binary path");
    binary->GetNativePath(binPath);
  }

  // Copy the parameters to the StagedUpdateInfo structure shared with the
  // watcher thread.
  mInfo.mGREDir = greDir;
  mInfo.mAppDir = appDir;
  mInfo.mUpdateRoot = updRoot;
  mInfo.mArgc = argc;
  mInfo.mArgv = new char*[argc];
  if (dirProvider) {
    for (int i = 0; i < argc; ++i) {
      const size_t length = strlen(argv[i]);
      mInfo.mArgv[i] = new char[length + 1];
      strcpy(mInfo.mArgv[i], argv[i]);
    }
  } else {
    MOZ_ASSERT(argc == 1); // see above
    const size_t length = binPath.Length();
    mInfo.mArgv[0] = new char[length + 1];
    strcpy(mInfo.mArgv[0], binPath.get());
  }
  mInfo.mAppVersion = appVersion;

#if defined(MOZ_WIDGET_GONK)
  NS_ENSURE_ARG_POINTER(aUpdate);

  bool isOSUpdate;
  if (NS_SUCCEEDED(aUpdate->GetIsOSUpdate(&isOSUpdate)) &&
      isOSUpdate) {
    nsAutoCString osApplyToDir;

    // This needs to be done on the main thread, so we pass it along in
    // BackgroundThreadInfo
    nsresult rv = GetOSApplyToDir(osApplyToDir);
    if (NS_FAILED(rv)) {
      LOG(("Can't get the OS apply to dir"));
      return rv;
    }

    SetOSApplyToDir(aUpdate, osApplyToDir);

    mInfo.mIsOSUpdate = true;
    rv = NS_NewNativeLocalFile(osApplyToDir, false,
                               getter_AddRefs(mInfo.mOSApplyToDir));
    if (NS_FAILED(rv)) {
      LOG(("Can't create nsIFile for OS apply to dir"));
      return rv;
    }
  }
#endif

  MOZ_ASSERT(NS_IsMainThread(), "not main thread");
  nsCOMPtr<nsIRunnable> r = NewRunnableMethod(this, &nsUpdateProcessor::StartStagedUpdate);
  return NS_NewThread(getter_AddRefs(mProcessWatcher), r);
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
                               mInfo.mIsOSUpdate,
                               mInfo.mOSApplyToDir,
                               &mUpdaterPID);
  NS_ENSURE_SUCCESS_VOID(rv);

  if (mUpdaterPID) {
    // Track the state of the updater process while it is staging an update.
    rv = NS_DispatchToCurrentThread(NewRunnableMethod(this, &nsUpdateProcessor::WaitForProcess));
    NS_ENSURE_SUCCESS_VOID(rv);
  } else {
    // Failed to launch the updater process for some reason.
    // We need to shutdown the current thread as there isn't anything more for
    // us to do...
    rv = NS_DispatchToMainThread(NewRunnableMethod(this, &nsUpdateProcessor::ShutdownWatcherThread));
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
  ::WaitForProcess(mUpdaterPID);
  NS_DispatchToMainThread(NewRunnableMethod(this, &nsUpdateProcessor::UpdateDone));
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

