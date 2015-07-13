/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAppRunner_h__
#define nsAppRunner_h__

#ifdef XP_WIN
#include <windows.h>
#else
#include <limits.h>
#endif

#ifndef MAXPATHLEN
#ifdef PATH_MAX
#define MAXPATHLEN PATH_MAX
#elif defined(_MAX_PATH)
#define MAXPATHLEN _MAX_PATH
#elif defined(CCHMAXPATH)
#define MAXPATHLEN CCHMAXPATH
#else
#define MAXPATHLEN 1024
#endif
#endif

#include "nscore.h"
#include "nsXULAppAPI.h"

// This directory service key is a lot like NS_APP_LOCALSTORE_50_FILE,
// but it is always the "main" localstore file, even when we're in safe mode
// and we load localstore from somewhere else.
#define NS_LOCALSTORE_UNSAFE_FILE "LStoreS"

class nsINativeAppSupport;
class nsXREDirProvider;
class nsIToolkitProfileService;
class nsIFile;
class nsIProfileLock;
class nsIProfileUnlocker;
class nsIFactory;

extern nsXREDirProvider* gDirServiceProvider;

// NOTE: gAppData will be null in embedded contexts. The "size" parameter
// will be the size of the original structure passed to XRE_main, but the
// structure will have all of the members available.
extern const nsXREAppData* gAppData;
extern bool gSafeMode;

extern int    gArgc;
extern char **gArgv;
extern int    gRestartArgc;
extern char **gRestartArgv;
extern bool gLogConsoleErrors;

extern bool gIsGtest;

/**
 * Create the nativeappsupport implementation.
 *
 * @note XPCOMInit has not happened yet.
 */
nsresult NS_CreateNativeAppSupport(nsINativeAppSupport* *aResult);

nsresult
NS_NewToolkitProfileService(nsIToolkitProfileService* *aResult);

nsresult
NS_NewToolkitProfileFactory(nsIFactory* *aResult);

/**
 * Try to acquire exclusive access to the specified profile directory.
 *
 * @param aPath
 *        The profile directory to lock.
 * @param aTempPath
 *        The corresponding profile temporary directory.
 * @param aUnlocker
 *        A callback interface used to attempt to unlock a profile that
 *        appears to be locked.
 * @param aResult
 *        The resulting profile lock object (or null if the profile could
 *        not be locked).
 *
 * @return NS_ERROR_FILE_ACCESS_DENIED to indicate that the profile
 *         directory cannot be unlocked.
 */
nsresult
NS_LockProfilePath(nsIFile* aPath, nsIFile* aTempPath,
                   nsIProfileUnlocker* *aUnlocker, nsIProfileLock* *aResult);

void
WriteConsoleLog();

#ifdef XP_WIN
void
UseParentConsole();

BOOL
WinLaunchChild(const wchar_t *exePath, int argc,
               char **argv, HANDLE userToken = nullptr,
               HANDLE *hProcess = nullptr);
BOOL
WriteStatusPending(LPCWSTR updateDirPath);
BOOL
WriteStatusApplied(LPCWSTR updateDirPath);
#endif

#define NS_NATIVEAPPSUPPORT_CONTRACTID "@mozilla.org/toolkit/native-app-support;1"

namespace mozilla {
namespace startup {
extern GeckoProcessType sChildProcessType;
} // namespace startup
} // namespace mozilla

/**
 * Set up platform specific error handling such as suppressing DLL load dialog
 * and the JIT debugger on Windows, and install unix signal handlers.
 */
void SetupErrorHandling(const char* progname);

#endif // nsAppRunner_h__
