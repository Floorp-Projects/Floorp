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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>
 *  Benjamin Smedberg <bsmedberg@covad.net>
 *  Ben Goodger <ben@mozilla.org>
 *  Jens Bannmann <jens.b@web.de>
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

#include "nsAppRunner.h"
#include "nsXREDirProvider.h"

#include "jsapi.h"

#include "nsIJSContextStack.h"
#include "nsILocalFile.h"
#include "nsIObserverService.h"
#include "nsIProfileChangeStatus.h"
#include "nsISimpleEnumerator.h"
#include "nsIToolkitChromeRegistry.h"

#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsXULAppAPI.h"

#include "nsINIParser.h"
#include "nsDependentString.h"
#include "nsCOMArray.h"
#include "nsArrayEnumerator.h"
#include "nsEnumeratorUtils.h"

#include <stdlib.h>

#ifdef XP_WIN
#include <windows.h>
#include <shlobj.h>
// This is not defined by VC6. 
#ifndef CSIDL_LOCAL_APPDATA
#define CSIDL_LOCAL_APPDATA             0x001C
#endif
#endif
#ifdef XP_MACOSX
#include "nsILocalFileMac.h"
#endif
#ifdef XP_BEOS
#include <be/kernel/image.h>
#include <FindDirectory.h>
#endif
#ifdef XP_UNIX
#include <ctype.h>
#endif
#ifdef XP_OS2
#define INCL_DOS
#include <os2.h>
#endif

#if defined(XP_MACOSX)
#define APP_REGISTRY_NAME "Application Registry"
#elif defined(XP_WIN) || defined(XP_OS2)
#define APP_REGISTRY_NAME "registry.dat"
#else
#define APP_REGISTRY_NAME "appreg"
#endif

nsXREDirProvider* gDirServiceProvider = nsnull;

nsXREDirProvider::nsXREDirProvider() :
  mProfileNotified(PR_FALSE)
{
  gDirServiceProvider = this;
}

nsXREDirProvider::~nsXREDirProvider()
{
  gDirServiceProvider = nsnull;
}

nsresult
nsXREDirProvider::Initialize(nsIFile *aXULAppDir,
                             nsILocalFile *aGREDir,
                             nsIDirectoryServiceProvider* aAppProvider)
{
  NS_ENSURE_ARG(aGREDir);

  mAppProvider = aAppProvider;
  mXULAppDir = aXULAppDir;
  mGREDir = aGREDir;

  return NS_OK;
}

nsresult
nsXREDirProvider::SetProfile(nsIFile* aDir, nsIFile* aLocalDir)
{
  NS_ASSERTION(aDir && aLocalDir, "We don't support no-profile apps yet!");

  nsresult rv;
  
  rv = EnsureDirectoryExists(aDir);
  if (NS_FAILED(rv))
    return rv;

  rv = EnsureDirectoryExists(aLocalDir);
  if (NS_FAILED(rv))
    return rv;

  mProfileDir = aDir;
  mProfileLocalDir = aLocalDir;
  return NS_OK;
}

NS_IMPL_QUERY_INTERFACE3(nsXREDirProvider,
                         nsIDirectoryServiceProvider,
                         nsIDirectoryServiceProvider2,
                         nsIProfileStartup)

NS_IMETHODIMP_(nsrefcnt)
nsXREDirProvider::AddRef()
{
  return 1;
}

NS_IMETHODIMP_(nsrefcnt)
nsXREDirProvider::Release()
{
  return 0;
}

NS_IMETHODIMP
nsXREDirProvider::GetFile(const char* aProperty, PRBool* aPersistent,
			  nsIFile** aFile)
{
  nsresult rv;

  PRBool gettingProfile = PR_FALSE;

  if (!strcmp(aProperty, NS_APP_USER_PROFILE_LOCAL_50_DIR)) {
    // If XRE_NotifyProfile hasn't been called, don't fall through to
    // mAppProvider on the profile keys.
    if (!mProfileNotified)
      return NS_ERROR_FAILURE;

    if (mProfileLocalDir)
      return mProfileLocalDir->Clone(aFile);

    if (mAppProvider)
      return mAppProvider->GetFile(aProperty, aPersistent, aFile);

    // This falls through to the case below
    gettingProfile = PR_TRUE;
  }
  if (!strcmp(aProperty, NS_APP_USER_PROFILE_50_DIR) || gettingProfile) {
    if (!mProfileNotified)
      return NS_ERROR_FAILURE;

    if (mProfileDir)
      return mProfileDir->Clone(aFile);

    if (mAppProvider)
      return mAppProvider->GetFile(aProperty, aPersistent, aFile);

    // If we don't succeed here, bail early so that we aren't reentrant
    // through the "GetProfileDir" call below.
    return NS_ERROR_FAILURE;
  }

  if (mAppProvider) {
    rv = mAppProvider->GetFile(aProperty, aPersistent, aFile);
    if (NS_SUCCEEDED(rv) && *aFile)
      return rv;
  }

  *aPersistent = PR_TRUE;

  if (!strcmp(aProperty, NS_GRE_DIR)) {
    return mGREDir->Clone(aFile);
  }
  else if (!strcmp(aProperty, NS_OS_CURRENT_PROCESS_DIR) ||
      !strcmp(aProperty, NS_APP_INSTALL_CLEANUP_DIR)) {
    return GetAppDir()->Clone(aFile);
  }

  rv = NS_ERROR_FAILURE;
  nsCOMPtr<nsIFile> file;

  if (!strcmp(aProperty, NS_APP_PROFILE_DEFAULTS_50_DIR) ||
           !strcmp(aProperty, NS_APP_PROFILE_DEFAULTS_NLOC_50_DIR)) {
    return GetProfileDefaultsDir(aFile);
  }
  else if (!strcmp(aProperty, NS_APP_PREF_DEFAULTS_50_DIR))
  {
    // return the GRE default prefs directory here, and the app default prefs
    // directory (if applicable) in NS_APP_PREFS_DEFAULTS_DIR_LIST.
    rv = mGREDir->Clone(getter_AddRefs(file));
    if (NS_SUCCEEDED(rv)) {
      rv = file->AppendNative(NS_LITERAL_CSTRING("defaults"));
      if (NS_SUCCEEDED(rv))
        rv = file->AppendNative(NS_LITERAL_CSTRING("pref"));
    }
  }
  else if (!strcmp(aProperty, NS_APP_APPLICATION_REGISTRY_DIR) ||
           !strcmp(aProperty, XRE_USER_APP_DATA_DIR)) {
    rv = GetUserAppDataDirectory((nsILocalFile**)(nsIFile**) getter_AddRefs(file));
  }
#ifdef XP_WIN
  else if (!strcmp(aProperty, XRE_UPDATE_ROOT_DIR)) {
    rv = GetUpdateRootDir(getter_AddRefs(file));
  }
#endif
  else if (!strcmp(aProperty, NS_APP_APPLICATION_REGISTRY_FILE)) {
    rv = GetUserAppDataDirectory((nsILocalFile**)(nsIFile**) getter_AddRefs(file));
    if (NS_SUCCEEDED(rv))
      rv = file->AppendNative(NS_LITERAL_CSTRING(APP_REGISTRY_NAME));
  }
  else if (!strcmp(aProperty, NS_APP_USER_PROFILES_ROOT_DIR)) {
    rv = GetUserAppDataDirectory((nsILocalFile**)(nsIFile**) getter_AddRefs(file));

    if (NS_SUCCEEDED(rv)) {
#if !defined(XP_UNIX) || defined(XP_MACOSX)
      rv = file->AppendNative(NS_LITERAL_CSTRING("Profiles"));
#endif

      // We must create the profile directory here if it does not exist.
      rv |= EnsureDirectoryExists(file);
    }
  }
  else if (!strcmp(aProperty, NS_APP_USER_PROFILES_LOCAL_ROOT_DIR)) {
    rv = GetUserLocalDataDirectory((nsILocalFile**)(nsIFile**) getter_AddRefs(file));

    if (NS_SUCCEEDED(rv)) {
#if !defined(XP_UNIX) || defined(XP_MACOSX)
      rv = file->AppendNative(NS_LITERAL_CSTRING("Profiles"));
#endif

      // We must create the profile directory here if it does not exist.
      rv |= EnsureDirectoryExists(file);
    }
  }
  else if (!strcmp(aProperty, XRE_EXECUTABLE_FILE) && gArgv[0]) {
    nsCOMPtr<nsILocalFile> lf;
    rv = XRE_GetBinaryPath(gArgv[0], getter_AddRefs(lf));
    if (NS_SUCCEEDED(rv))
      file = lf;
  }
  else if (!strcmp(aProperty, "resource:app")) {
    rv = GetAppDir()->Clone(getter_AddRefs(file));
  }

  else if (!strcmp(aProperty, NS_APP_PROFILE_DIR_STARTUP) && mProfileDir) {
    return mProfileDir->Clone(aFile);
  }
  else if (!strcmp(aProperty, NS_APP_PROFILE_LOCAL_DIR_STARTUP)) {
    if (mProfileLocalDir)
      return mProfileLocalDir->Clone(aFile);

    if (mProfileDir)
      return mProfileDir->Clone(aFile);

    if (mAppProvider)
      return mAppProvider->GetFile(NS_APP_PROFILE_DIR_STARTUP, aPersistent,
                                   aFile);
  }
  else if (NS_SUCCEEDED(GetProfileStartupDir(getter_AddRefs(file)))) {
    // We need to allow component, xpt, and chrome registration to
    // occur prior to the profile-after-change notification.
    if (!strcmp(aProperty, NS_XPCOM_COMPONENT_REGISTRY_FILE)) {
      rv = file->AppendNative(NS_LITERAL_CSTRING("compreg.dat"));
    }
    else if (!strcmp(aProperty, NS_XPCOM_XPTI_REGISTRY_FILE)) {
      rv = file->AppendNative(NS_LITERAL_CSTRING("xpti.dat"));
    }
    else if (!strcmp(aProperty, NS_APP_USER_CHROME_DIR)) {
      rv = file->AppendNative(NS_LITERAL_CSTRING("chrome"));
    }
  }

  if (NS_SUCCEEDED(rv) && file) {
    NS_ADDREF(*aFile = file);
    return NS_OK;
  }

  PRBool ensureFilePermissions = PR_FALSE;

  if (NS_SUCCEEDED(GetProfileDir(getter_AddRefs(file)))) {
    if (!strcmp(aProperty, NS_APP_PREFS_50_DIR)) {
      rv = NS_OK;
    }
    else if (!strcmp(aProperty, NS_APP_PREFS_50_FILE)) {
      rv = file->AppendNative(NS_LITERAL_CSTRING("prefs.js"));
    }
    else if (!strcmp(aProperty, NS_LOCALSTORE_UNSAFE_FILE)) {
      rv = file->AppendNative(NS_LITERAL_CSTRING("localstore.rdf"));
    }
    else if (!strcmp(aProperty, NS_APP_LOCALSTORE_50_FILE)) {
      if (gSafeMode) {
        rv = file->AppendNative(NS_LITERAL_CSTRING("localstore-safe.rdf"));
        file->Remove(PR_FALSE);
      }
      else {
        rv = file->AppendNative(NS_LITERAL_CSTRING("localstore.rdf"));
        EnsureProfileFileExists(file);
        ensureFilePermissions = PR_TRUE;
      }
    }
    else if (!strcmp(aProperty, NS_APP_HISTORY_50_FILE)) {
      rv = file->AppendNative(NS_LITERAL_CSTRING("history.dat"));
    }
    else if (!strcmp(aProperty, NS_APP_USER_MIMETYPES_50_FILE)) {
      rv = file->AppendNative(NS_LITERAL_CSTRING("mimeTypes.rdf"));
      EnsureProfileFileExists(file);
      ensureFilePermissions = PR_TRUE;
    }
    else if (!strcmp(aProperty, NS_APP_STORAGE_50_FILE)) {
      rv = file->AppendNative(NS_LITERAL_CSTRING("storage.sdb"));
    }
    else if (!strcmp(aProperty, NS_APP_DOWNLOADS_50_FILE)) {
      rv = file->AppendNative(NS_LITERAL_CSTRING("downloads.rdf"));
    }
    // XXXbsmedberg move these defines into application-specific providers.
    else if (!strcmp(aProperty, NS_APP_MAIL_50_DIR)) {
      rv = file->AppendNative(NS_LITERAL_CSTRING("Mail"));
    }
    else if (!strcmp(aProperty, NS_APP_IMAP_MAIL_50_DIR)) {
      rv = file->AppendNative(NS_LITERAL_CSTRING("ImapMail"));
    }
    else if (!strcmp(aProperty, NS_APP_NEWS_50_DIR)) {
      rv = file->AppendNative(NS_LITERAL_CSTRING("News"));
    }
    else if (!strcmp(aProperty, NS_APP_MESSENGER_FOLDER_CACHE_50_DIR)) {
      rv = file->AppendNative(NS_LITERAL_CSTRING("panacea.dat"));
    }
  }
  if (NS_FAILED(rv) || !file)
    return NS_ERROR_FAILURE;

  if (ensureFilePermissions) {
    PRBool fileToEnsureExists;
    PRBool isWritable;
    if (NS_SUCCEEDED(file->Exists(&fileToEnsureExists)) && fileToEnsureExists
        && NS_SUCCEEDED(file->IsWritable(&isWritable)) && !isWritable) {
      PRUint32 permissions;
      if (NS_SUCCEEDED(file->GetPermissions(&permissions))) {
        rv = file->SetPermissions(permissions | 0600);
        NS_ASSERTION(NS_SUCCEEDED(rv), "failed to ensure file permissions");
      }
    }
  }

  NS_ADDREF(*aFile = file);
  return NS_OK;
}

static void
LoadDirsIntoArray(nsIFile* aComponentsList, const char* aSection,
                  const char *const* aAppendList,
                  nsCOMArray<nsIFile>& aDirectories)
{
  nsINIParser parser;
  nsCOMPtr<nsILocalFile> lf(do_QueryInterface(aComponentsList));
  nsresult rv =  parser.Init(lf);
  if (NS_FAILED(rv))
    return;

  NS_NAMED_LITERAL_CSTRING(platform, "platform");
  NS_NAMED_LITERAL_CSTRING(osTarget, OS_TARGET);
#ifdef TARGET_OS_ABI
  NS_NAMED_LITERAL_CSTRING(targetOSABI, TARGET_OS_ABI);
#endif

  PRInt32 i = 0;
  do {
    nsCAutoString buf("Extension");
    buf.AppendInt(i++);

    nsCAutoString path;
    rv = parser.GetString(aSection, buf.get(), path);
    if (NS_FAILED(rv))
      break;

    nsCOMPtr<nsILocalFile> dir = do_CreateInstance("@mozilla.org/file/local;1", &rv);
    if (NS_FAILED(rv))
      continue;

    nsCOMPtr<nsIFile> platformDir;
#ifdef TARGET_OS_ABI
    nsCOMPtr<nsIFile> platformABIDir;
#endif
    rv = dir->SetPersistentDescriptor(path);
    if (NS_FAILED(rv))
      continue;

    rv = dir->Clone(getter_AddRefs(platformDir));
    if (NS_FAILED(rv))
      continue;

    platformDir->AppendNative(platform);
    platformDir->AppendNative(osTarget);

#ifdef TARGET_OS_ABI
    rv = dir->Clone(getter_AddRefs(platformABIDir));
    if (NS_FAILED(rv))
      continue;

    platformABIDir->AppendNative(platform);
    platformABIDir->AppendNative(targetOSABI);
#endif

    const char* const* a = aAppendList;
    while (*a) {
      nsDependentCString directory(*a);
      dir->AppendNative(directory);
      platformDir->AppendNative(directory);
#ifdef TARGET_OS_ABI
      platformABIDir->AppendNative(directory);
#endif
      ++a;
    }

    PRBool exists;
    rv = dir->Exists(&exists);
    if (NS_SUCCEEDED(rv) && exists)
      aDirectories.AppendObject(dir);

    rv = platformDir->Exists(&exists);
    if (NS_SUCCEEDED(rv) && exists)
      aDirectories.AppendObject(platformDir);

#ifdef TARGET_OS_ABI
    rv = platformABIDir->Exists(&exists);
    if (NS_SUCCEEDED(rv) && exists)
      aDirectories.AppendObject(platformABIDir);
#endif
  }
  while (PR_TRUE);
}

static const char *const kAppendChromeManifests[] =
  { "chrome.manifest", nsnull };

NS_IMETHODIMP
nsXREDirProvider::GetFiles(const char* aProperty, nsISimpleEnumerator** aResult)
{
  nsresult rv;

  nsCOMPtr<nsISimpleEnumerator> appEnum;
  nsCOMPtr<nsIDirectoryServiceProvider2>
    appP2(do_QueryInterface(mAppProvider));
  if (appP2) {
    rv = appP2->GetFiles(aProperty, getter_AddRefs(appEnum));
    if (NS_FAILED(rv)) {
      appEnum = nsnull;
    }
    else if (rv != NS_SUCCESS_AGGREGATE_RESULT) {
      NS_ADDREF(*aResult = appEnum);
      return NS_OK;
    }
  }

  nsCOMPtr<nsISimpleEnumerator> xreEnum;
  rv = GetFilesInternal(aProperty, getter_AddRefs(xreEnum));
  if (NS_FAILED(rv)) {
    if (appEnum) {
      NS_ADDREF(*aResult = appEnum);
      return NS_SUCCESS_AGGREGATE_RESULT;
    }

    return rv;
  }

  rv = NS_NewUnionEnumerator(aResult, appEnum, xreEnum);
  if (NS_FAILED(rv))
    return rv;

  return NS_SUCCESS_AGGREGATE_RESULT;
}

nsresult
nsXREDirProvider::GetFilesInternal(const char* aProperty,
                                   nsISimpleEnumerator** aResult)
{
  nsresult rv = NS_OK;
  *aResult = nsnull;

  nsCOMPtr<nsIFile> profileFile;
  if (mProfileDir) {
    mProfileDir->Clone(getter_AddRefs(profileFile));
    profileFile->AppendNative(NS_LITERAL_CSTRING("extensions.ini"));
  }

  if (!strcmp(aProperty, XRE_EXTENSIONS_DIR_LIST)) {
    nsCOMArray<nsIFile> directories;
    
    if (mProfileDir && !gSafeMode) {
      static const char *const kAppendNothing[] = { nsnull };

      LoadDirsIntoArray(profileFile, "ExtensionDirs",
                        kAppendNothing, directories);
    }

    rv = NS_NewArrayEnumerator(aResult, directories);
  }
  else if (!strcmp(aProperty, NS_XPCOM_COMPONENT_DIR_LIST)) {
    nsCOMArray<nsIFile> directories;

    if (mXULAppDir) {
      nsCOMPtr<nsIFile> file;
      mXULAppDir->Clone(getter_AddRefs(file));
      file->AppendNative(NS_LITERAL_CSTRING("components"));
      PRBool exists;
      if (NS_SUCCEEDED(file->Exists(&exists)) && exists)
        directories.AppendObject(file);
    }

    if (mProfileDir && !gSafeMode) {
      static const char *const kAppendCompDir[] = { "components", nsnull };

      LoadDirsIntoArray(profileFile, "ExtensionDirs",
                        kAppendCompDir, directories);
    }

    rv = NS_NewArrayEnumerator(aResult, directories);
  }
  else if (!strcmp(aProperty, NS_APP_PREFS_DEFAULTS_DIR_LIST)) {
    nsCOMArray<nsIFile> directories;

    if (mXULAppDir) {
      nsCOMPtr<nsIFile> file;
      mXULAppDir->Clone(getter_AddRefs(file));
      file->AppendNative(NS_LITERAL_CSTRING("defaults"));
      file->AppendNative(NS_LITERAL_CSTRING("preferences"));
      PRBool exists;
      if (NS_SUCCEEDED(file->Exists(&exists)) && exists)
        directories.AppendObject(file);
    }

    if (mProfileDir && !gSafeMode) {
      static const char *const kAppendPrefDir[] = { "defaults", "preferences", nsnull };

      LoadDirsIntoArray(profileFile, "ExtensionDirs",
                        kAppendPrefDir, directories);
    }

    rv = NS_NewArrayEnumerator(aResult, directories);
  }
  else if (!strcmp(aProperty, NS_CHROME_MANIFESTS_FILE_LIST)) {
    nsCOMArray<nsIFile> manifests;

    nsCOMPtr<nsIFile> manifest;
    mGREDir->Clone(getter_AddRefs(manifest));
    manifest->AppendNative(NS_LITERAL_CSTRING("chrome"));
    manifests.AppendObject(manifest);

    if (mXULAppDir) {
      nsCOMPtr<nsIFile> file;
      mXULAppDir->Clone(getter_AddRefs(file));
      file->AppendNative(NS_LITERAL_CSTRING("chrome"));
      PRBool exists;
      if (NS_SUCCEEDED(file->Exists(&exists)) && exists)
        manifests.AppendObject(file);

      mXULAppDir->Clone(getter_AddRefs(file));
      file->AppendNative(NS_LITERAL_CSTRING("chrome.manifest"));
      if (NS_SUCCEEDED(file->Exists(&exists)) && exists)
        manifests.AppendObject(file);
    }

    if (mProfileDir && !gSafeMode) {
      LoadDirsIntoArray(profileFile, "ExtensionDirs",
                        kAppendChromeManifests, manifests);
    }

    rv = NS_NewArrayEnumerator(aResult, manifests);
  }  
  else if (!strcmp(aProperty, NS_SKIN_MANIFESTS_FILE_LIST)) {
    nsCOMArray<nsIFile> manifests;
    if (mProfileDir && !gSafeMode) {
      LoadDirsIntoArray(profileFile, "ThemeDirs",
                        kAppendChromeManifests, manifests);
    }

    rv = NS_NewArrayEnumerator(aResult, manifests);
  }
  else if (!strcmp(aProperty, NS_APP_CHROME_DIR_LIST)) {
    // NS_APP_CHROME_DIR_LIST is only used to get default (native) icons
    // for OS window decoration.

    nsCOMArray<nsIFile> directories;

    if (mXULAppDir) {
      nsCOMPtr<nsIFile> file;
      mXULAppDir->Clone(getter_AddRefs(file));
      file->AppendNative(NS_LITERAL_CSTRING("chrome"));
      PRBool exists;
      if (NS_SUCCEEDED(file->Exists(&exists)) && exists)
        directories.AppendObject(file);
    }

    if (mProfileDir && !gSafeMode) {
      static const char *const kAppendChromeDir[] = { "chrome", nsnull };

      LoadDirsIntoArray(profileFile, "ExtensionDirs",
                        kAppendChromeDir, directories);
    }

    rv = NS_NewArrayEnumerator(aResult, directories);
  }
  else if (!strcmp(aProperty, NS_APP_PLUGINS_DIR_LIST)) {
    nsCOMArray<nsIFile> directories;

    // The root dirserviceprovider does quite a bit for us: we're mainly
    // interested in xulapp and extension-provided plugins.
    if (mXULAppDir) {
      nsCOMPtr<nsIFile> file;
      mXULAppDir->Clone(getter_AddRefs(file));
      file->AppendNative(NS_LITERAL_CSTRING("plugins"));
      PRBool exists;
      if (NS_SUCCEEDED(file->Exists(&exists)) && exists)
        directories.AppendObject(file);
    }

    if (mProfileDir && !gSafeMode) {
      static const char *const kAppendPlugins[] = { "plugins", nsnull };

      LoadDirsIntoArray(profileFile, "ExtensionDirs",
                        kAppendPlugins, directories);
    }

    rv = NS_NewArrayEnumerator(aResult, directories);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = NS_SUCCESS_AGGREGATE_RESULT;
  }
  else
    rv = NS_ERROR_FAILURE;

  return rv;
}

NS_IMETHODIMP
nsXREDirProvider::GetDirectory(nsIFile* *aResult)
{
  NS_ENSURE_TRUE(mProfileDir, NS_ERROR_NOT_INITIALIZED);

  return mProfileDir->Clone(aResult);
}

NS_IMETHODIMP
nsXREDirProvider::DoStartup()
{
  if (!mProfileNotified) {
    nsCOMPtr<nsIObserverService> obsSvc
      (do_GetService("@mozilla.org/observer-service;1"));
    if (!obsSvc) return NS_ERROR_FAILURE;

    mProfileNotified = PR_TRUE;

    static const PRUnichar kStartup[] = {'s','t','a','r','t','u','p','\0'};
    obsSvc->NotifyObservers(nsnull, "profile-do-change", kStartup);
    obsSvc->NotifyObservers(nsnull, "profile-after-change", kStartup);
  }
  return NS_OK;
}

class ProfileChangeStatusImpl : public nsIProfileChangeStatus
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROFILECHANGESTATUS
  ProfileChangeStatusImpl() { }
private:
  ~ProfileChangeStatusImpl() { }
};

NS_IMPL_ISUPPORTS1(ProfileChangeStatusImpl, nsIProfileChangeStatus)

NS_IMETHODIMP
ProfileChangeStatusImpl::VetoChange()
{
  NS_ERROR("Can't veto change!");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
ProfileChangeStatusImpl::ChangeFailed()
{
  NS_ERROR("Profile change cancellation.");
  return NS_ERROR_FAILURE;
}

void
nsXREDirProvider::DoShutdown()
{
  if (mProfileNotified) {
    nsCOMPtr<nsIObserverService> obssvc
      (do_GetService("@mozilla.org/observer-service;1"));
    NS_ASSERTION(obssvc, "No observer service?");
    if (obssvc) {
      nsCOMPtr<nsIProfileChangeStatus> cs = new ProfileChangeStatusImpl();
      static const PRUnichar kShutdownPersist[] =
        {'s','h','u','t','d','o','w','n','-','p','e','r','s','i','s','t','\0'};
      obssvc->NotifyObservers(cs, "profile-change-net-teardown", kShutdownPersist);
      obssvc->NotifyObservers(cs, "profile-change-teardown", kShutdownPersist);

      // Phase 2c: Now that things are torn down, force JS GC so that things which depend on
      // resources which are about to go away in "profile-before-change" are destroyed first.
      nsCOMPtr<nsIThreadJSContextStack> stack
        (do_GetService("@mozilla.org/js/xpc/ContextStack;1"));
      if (stack)
      {
        JSContext *cx = nsnull;
        stack->GetSafeJSContext(&cx);
        if (cx)
          ::JS_GC(cx);
      }

      // Phase 3: Notify observers of a profile change
      obssvc->NotifyObservers(cs, "profile-before-change", kShutdownPersist);
    }
    mProfileNotified = PR_FALSE;
  }
}

static void 
GetProfileFolderName(char* aProfileFolderName, const char* aSource)
{
  const char* reading = aSource;

  while (*reading) {
    *aProfileFolderName = tolower(*reading);
    ++aProfileFolderName; ++reading;
  }
  *aProfileFolderName = '\0';
}

#ifdef XP_WIN
static nsresult
GetShellFolderPath(int folder, char result[MAXPATHLEN])
{
  LPITEMIDLIST pItemIDList = NULL;

  nsresult rv;
  if (SUCCEEDED(SHGetSpecialFolderLocation(NULL, folder, &pItemIDList)) &&
      SUCCEEDED(SHGetPathFromIDList(pItemIDList, result))) {
    rv = NS_OK;
  } else {
    rv = NS_ERROR_NOT_AVAILABLE;
  }

  CoTaskMemFree(pItemIDList);

  return rv;
}

nsresult
nsXREDirProvider::GetUpdateRootDir(nsIFile* *aResult)
{
  nsCOMPtr<nsIFile> appDir = GetAppDir();
  nsCAutoString appPath;
  nsresult rv = appDir->GetNativePath(appPath);
  NS_ENSURE_SUCCESS(rv, rv);

  // AppDir may be a short path. Convert to long path to make sure
  // the consistency of the update folder location
  nsCString longPath;
  char *buf;
  longPath.GetMutableData(&buf, MAXPATHLEN);
  DWORD len = GetLongPathName(appPath.get(), buf, MAXPATHLEN);
  // Failing GetLongPathName() is not fatal.
  if (len <= 0 || len >= MAXPATHLEN)
    longPath.Assign(appPath);
  else
    longPath.SetLength(len);

  // Use <UserLocalDataDir>\updates\<relative path to app dir from
  // Program Files> if app dir is under Program Files to avoid the
  // folder virtualization mess on Windows Vista
  char programFiles[MAXPATHLEN];
  rv = GetShellFolderPath(CSIDL_PROGRAM_FILES, programFiles);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 programFilesLen = strlen(programFiles);
  programFiles[programFilesLen++] = '\\';
  programFiles[programFilesLen] = '\0';

  if (longPath.Length() < programFilesLen)
    return NS_ERROR_FAILURE;

  if (_strnicmp(programFiles, longPath.get(), programFilesLen) != 0)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsILocalFile> updRoot;
  rv = GetUserLocalDataDirectory(getter_AddRefs(updRoot));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = updRoot->AppendRelativeNativePath(Substring(longPath, programFilesLen));
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ADDREF(*aResult = updRoot);
  return NS_OK;
}
#endif

nsresult
nsXREDirProvider::GetProfileStartupDir(nsIFile* *aResult)
{
  if (mProfileDir)
    return mProfileDir->Clone(aResult);

  if (mAppProvider) {
    nsCOMPtr<nsIFile> needsclone;
    PRBool dummy;
    nsresult rv = mAppProvider->GetFile(NS_APP_PROFILE_DIR_STARTUP,
                                        &dummy,
                                        getter_AddRefs(needsclone));
    if (NS_SUCCEEDED(rv))
      return needsclone->Clone(aResult);
  }

  return NS_ERROR_FAILURE;
}

nsresult
nsXREDirProvider::GetProfileDir(nsIFile* *aResult)
{
  if (mProfileDir) {
    if (!mProfileNotified)
      return NS_ERROR_FAILURE;

    return mProfileDir->Clone(aResult);
  }

  if (mAppProvider) {
    nsCOMPtr<nsIFile> needsclone;
    PRBool dummy;
    nsresult rv = mAppProvider->GetFile(NS_APP_USER_PROFILE_50_DIR,
                                        &dummy,
                                        getter_AddRefs(needsclone));
    if (NS_SUCCEEDED(rv))
      return needsclone->Clone(aResult);
  }

  return NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, aResult);
}

nsresult
nsXREDirProvider::GetUserDataDirectory(nsILocalFile** aFile, PRBool aLocal)
{
  if (!gAppData)
    return NS_ERROR_FAILURE;

  // Copied from nsAppFileLocationProvider (more or less)
  nsresult rv;
  nsCOMPtr<nsILocalFile> localDir;

#if defined(XP_MACOSX)
  FSRef fsRef;
  OSType folderType;
  if (aLocal) {
    folderType = kCachedDataFolderType;
  } else {
#ifdef MOZ_THUNDERBIRD 
    folderType = kDomainLibraryFolderType;
#else
    folderType = kApplicationSupportFolderType;
#endif 
  }
  OSErr err = ::FSFindFolder(kUserDomain, folderType, kCreateFolder, &fsRef);
  NS_ENSURE_FALSE(err, NS_ERROR_FAILURE);

  rv = NS_NewNativeLocalFile(EmptyCString(), PR_TRUE, getter_AddRefs(localDir));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsILocalFileMac> dirFileMac = do_QueryInterface(localDir);
  NS_ENSURE_TRUE(dirFileMac, NS_ERROR_UNEXPECTED);

  rv = dirFileMac->InitWithFSRef(&fsRef);
  NS_ENSURE_SUCCESS(rv, rv);

  // Note that MacOS ignores the vendor when creating the profile hierarchy - all
  // application preferences directories live alongside one another in 
  // ~/Library/Application Support/
  rv = dirFileMac->AppendNative(nsDependentCString(gAppData->name));
  NS_ENSURE_SUCCESS(rv, rv);
#elif defined(XP_WIN)
  char path[MAXPATHLEN];

  // CSIDL_LOCAL_APPDATA is only defined on newer versions of Windows.  If the
  // OS does not understand it, then we'll fallback to the regular APPDATA
  // location.  If neither is defined, then we fallback to the Windows folder.

  if (aLocal)
    rv = GetShellFolderPath(CSIDL_LOCAL_APPDATA, path);
  if (!aLocal || NS_FAILED(rv))
    rv = GetShellFolderPath(CSIDL_APPDATA, path);

  if (NS_FAILED(rv) && !GetWindowsDirectory(path, sizeof(path))) {
    NS_WARNING("Aaah, no windows directory!");
    return NS_ERROR_FAILURE;
  }

  rv = NS_NewNativeLocalFile(nsDependentCString(path),
                             PR_TRUE, getter_AddRefs(localDir));
  NS_ENSURE_SUCCESS(rv, rv);

  if (gAppData->vendor) {
    rv = localDir->AppendNative(nsDependentCString(gAppData->vendor));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  rv = localDir->AppendNative(nsDependentCString(gAppData->name));
  NS_ENSURE_SUCCESS(rv, rv);

#elif defined(XP_OS2)
#if 0 /* For OS/2 we want to always use MOZILLA_HOME */
  // we want an environment variable of the form
  // FIREFOX_HOME, etc
  nsDependentCString envVar(nsDependentCString(gAppData->name));
  envVar.Append("_HOME");
  char *pHome = getenv(envVar.get());
#endif
  char *pHome = getenv("MOZILLA_HOME");
  if (pHome && *pHome) {
    rv = NS_NewNativeLocalFile(nsDependentCString(pHome), PR_TRUE,
                               getter_AddRefs(localDir));
  } else {
    PPIB ppib;
    PTIB ptib;
    char appDir[CCHMAXPATH];

    DosGetInfoBlocks(&ptib, &ppib);
    DosQueryModuleName(ppib->pib_hmte, CCHMAXPATH, appDir);
    *strrchr(appDir, '\\') = '\0';
    rv = NS_NewNativeLocalFile(nsDependentCString(appDir), PR_TRUE, getter_AddRefs(localDir));
  }
  NS_ENSURE_SUCCESS(rv, rv);

  if (gAppData->vendor) {
    rv = localDir->AppendNative(nsDependentCString(gAppData->vendor));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  rv = localDir->AppendNative(nsDependentCString(gAppData->name));
  NS_ENSURE_SUCCESS(rv, rv);

#elif defined(XP_BEOS)
  char appDir[MAXPATHLEN];
  if (find_directory(B_USER_SETTINGS_DIRECTORY, NULL, true, appDir, MAXPATHLEN))
    return NS_ERROR_FAILURE;

  int len = strlen(appDir);
  appDir[len]   = '/';
  appDir[len+1] = '\0';

  rv = NS_NewNativeLocalFile(nsDependentCString(appDir), PR_TRUE,
                             getter_AddRefs(localDir));
  NS_ENSURE_SUCCESS(rv, rv);

  if (gAppData->vendor) {
    rv = localDir->AppendNative(nsDependentCString(gAppData->vendor));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  rv = localDir->AppendNative(nsDependentCString(gAppData->name));
  NS_ENSURE_SUCCESS(rv, rv);

#elif defined(XP_UNIX)
  const char* homeDir = getenv("HOME");
  if (!homeDir || !*homeDir)
    return NS_ERROR_FAILURE;

  rv = NS_NewNativeLocalFile(nsDependentCString(homeDir), PR_TRUE,
                             getter_AddRefs(localDir));
  NS_ENSURE_SUCCESS(rv, rv);
 
  char* appNameFolder = nsnull;
  char profileFolderName[MAXPATHLEN] = ".";
 
  // Offset 1 for the outermost folder to make it hidden (i.e. using the ".")
  char* writing = profileFolderName + 1;
  if (gAppData->vendor) {
    GetProfileFolderName(writing, gAppData->vendor);
    
    rv = localDir->AppendNative(nsDependentCString(profileFolderName));
    NS_ENSURE_SUCCESS(rv, rv);
 
    char temp[MAXPATHLEN];
    GetProfileFolderName(temp, gAppData->name);
    appNameFolder = temp;
  }
  else {
    GetProfileFolderName(writing, gAppData->name);
    appNameFolder = profileFolderName;
  }
  rv = localDir->AppendNative(nsDependentCString(appNameFolder));
  NS_ENSURE_SUCCESS(rv, rv);
#else
#error dont_know_how_to_get_product_dir_on_your_platform
#endif

#ifdef DEBUG_jungshik
  nsCAutoString cwd;
  localDir->GetNativePath(cwd);
  printf("nsXREDirProvider::GetUserDataDirectory: %s\n", cwd.get());
#endif
  rv = EnsureDirectoryExists(localDir);
  NS_ENSURE_SUCCESS(rv, rv);

  *aFile = localDir;
  NS_ADDREF(*aFile);
  return NS_OK;
}

nsresult
nsXREDirProvider::EnsureDirectoryExists(nsIFile* aDirectory)
{
  PRBool exists;
  nsresult rv = aDirectory->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);
#ifdef DEBUG_jungshik
  if (!exists) {
    nsCAutoString cwd;
    aDirectory->GetNativePath(cwd);
    printf("nsXREDirProvider::EnsureDirectoryExists: %s does not\n", cwd.get());
  }
#endif
  if (!exists)
    rv = aDirectory->Create(nsIFile::DIRECTORY_TYPE, 0700);
#ifdef DEBUG_jungshik
  if (NS_FAILED(rv))
    NS_WARNING("nsXREDirProvider::EnsureDirectoryExists: create failed");
#endif

  return rv;
}

void
nsXREDirProvider::EnsureProfileFileExists(nsIFile *aFile)
{
  nsresult rv;
  PRBool exists;
    
  rv = aFile->Exists(&exists);
  if (NS_FAILED(rv) || exists) return;
  
  nsCAutoString leafName;
  rv = aFile->GetNativeLeafName(leafName);
  if (NS_FAILED(rv)) return;

  nsCOMPtr<nsIFile> defaultsFile;
  rv = GetProfileDefaultsDir(getter_AddRefs(defaultsFile));
  if (NS_FAILED(rv)) return;

  rv = defaultsFile->AppendNative(leafName);
  if (NS_FAILED(rv)) return;
  
  defaultsFile->CopyToNative(mProfileDir, EmptyCString());
}

nsresult
nsXREDirProvider::GetProfileDefaultsDir(nsIFile* *aResult)
{
  NS_ASSERTION(mGREDir, "nsXREDirProvider not initialized.");
  NS_PRECONDITION(aResult, "Null out-param");

  nsresult rv;
  nsCOMPtr<nsIFile> defaultsDir;

  rv = GetAppDir()->Clone(getter_AddRefs(defaultsDir));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = defaultsDir->AppendNative(NS_LITERAL_CSTRING("defaults"));
  rv |= defaultsDir->AppendNative(NS_LITERAL_CSTRING("profile"));
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ADDREF(*aResult = defaultsDir);
  return NS_OK;
}

