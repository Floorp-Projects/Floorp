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

class nsACString;
struct nsStaticModuleInfo;

class nsINativeAppSupport;
class nsICmdLineService;
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

/**
 * Create the nativeappsupport implementation.
 *
 * @note XPCOMInit has not happened yet.
 */
nsresult NS_CreateNativeAppSupport(nsINativeAppSupport* *aResult);

NS_HIDDEN_(nsresult)
NS_NewToolkitProfileService(nsIToolkitProfileService* *aResult);

NS_HIDDEN_(nsresult)
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
NS_HIDDEN_(nsresult)
NS_LockProfilePath(nsIFile* aPath, nsIFile* aTempPath,
                   nsIProfileUnlocker* *aUnlocker, nsIProfileLock* *aResult);

NS_HIDDEN_(void)
WriteConsoleLog();

#ifdef XP_WIN
BOOL
WinLaunchChild(const PRUnichar *exePath, int argc, 
               char **argv, HANDLE userToken = NULL,
               HANDLE *hProcess = nsnull);
BOOL
WriteStatusPending(LPCWSTR updateDirPath);
BOOL
WriteStatusApplied(LPCWSTR updateDirPath);
#endif

#define NS_NATIVEAPPSUPPORT_CONTRACTID "@mozilla.org/toolkit/native-app-support;1"

// Like nsXREAppData, but releases all strong refs/allocated memory
// in the destructor.
class ScopedAppData : public nsXREAppData
{
public:
  ScopedAppData() { Zero(); this->size = sizeof(*this); }

  ScopedAppData(const nsXREAppData* aAppData);

  void Zero() { memset(this, 0, sizeof(*this)); }

  ~ScopedAppData();
};

/**
 * Given "str" is holding a string allocated with NS_Alloc, or null:
 * replace the value in "str" with a new value.
 *
 * @param newvalue Null is permitted. The string is cloned with
 *                 NS_strdup
 */
void SetAllocatedString(const char *&str, const char *newvalue);

/**
 * Given "str" is holding a string allocated with NS_Alloc, or null:
 * replace the value in "str" with a new value.
 *
 * @param newvalue If "newvalue" is the empty string, "str" will be set
 *                 to null.
 */
void SetAllocatedString(const char *&str, const nsACString &newvalue);

template<class T>
void SetStrongPtr(T *&ptr, T* newvalue)
{
  NS_IF_RELEASE(ptr);
  ptr = newvalue;
  NS_IF_ADDREF(ptr);
}

namespace mozilla {
namespace startup {
extern GeckoProcessType sChildProcessType;
}
}

/**
 * Set up platform specific error handling such as suppressing DLL load dialog
 * and the JIT debugger on Windows, and install unix signal handlers.
 */
void SetupErrorHandling(const char* progname);

#endif // nsAppRunner_h__
