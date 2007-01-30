/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef nsAppRunner_h__
#define nsAppRunner_h__

#ifdef XP_WIN
#include <windows.h>
#endif

#ifndef MAXPATHLEN
#ifdef _MAX_PATH
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
class nsILocalFile;
class nsIProfileLock;
class nsIProfileUnlocker;

extern nsXREDirProvider* gDirServiceProvider;

// NOTE: gAppData will be null in embedded contexts. The "size" parameter
// will be the size of the original structure passed to XRE_main, but the
// structure will have all of the members available.
extern const nsXREAppData* gAppData;
extern PRBool gSafeMode;

extern int    gArgc;
extern char **gArgv;
extern PRBool gLogConsoleErrors;

/**
 * Create the nativeappsupport implementation.
 *
 * @note XPCOMInit has not happened yet.
 */
nsresult NS_CreateNativeAppSupport(nsINativeAppSupport* *aResult);

NS_HIDDEN_(nsresult)
NS_NewToolkitProfileService(nsIToolkitProfileService* *aResult);

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
NS_LockProfilePath(nsILocalFile* aPath, nsILocalFile* aTempPath,
                   nsIProfileUnlocker* *aUnlocker, nsIProfileLock* *aResult);

NS_HIDDEN_(void)
WriteConsoleLog();

#ifdef XP_WIN
BOOL
WinLaunchChild(const char *exePath, int argc, char **argv, int needElevation);
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

#endif // nsAppRunner_h__
