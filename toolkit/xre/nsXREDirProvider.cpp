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
#include "nsIDirectoryEnumerator.h"
#include "nsILocalFile.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIPrefService.h"
#include "nsIProfileChangeStatus.h"
#include "nsISimpleEnumerator.h"
#include "nsIToolkitChromeRegistry.h"

#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsXULAppAPI.h"
#include "nsCategoryManagerUtils.h"

#include "nsINIParser.h"
#include "nsDependentString.h"
#include "nsCOMArray.h"
#include "nsArrayEnumerator.h"
#include "nsEnumeratorUtils.h"
#include "nsReadableUtils.h"
#include "mozilla/Services.h"
#include "mozilla/Omnijar.h"

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

#define PREF_OVERRIDE_DIRNAME "preferences"

static already_AddRefed<nsILocalFile>
CloneAndAppend(nsIFile* aFile, const char* name)
{
  nsCOMPtr<nsIFile> file;
  aFile->Clone(getter_AddRefs(file));
  nsCOMPtr<nsILocalFile> lfile = do_QueryInterface(file);
  lfile->AppendNative(nsDependentCString(name));
  return lfile.forget();
}

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
  NS_ENSURE_ARG(aXULAppDir);
  NS_ENSURE_ARG(aGREDir);

  mAppProvider = aAppProvider;
  mXULAppDir = aXULAppDir;
  mGREDir = aGREDir;

  if (!mProfileDir) {
    nsCOMPtr<nsIDirectoryServiceProvider> app(do_QueryInterface(mAppProvider));
    if (app) {
      PRBool per = PR_FALSE;
      app->GetFile(NS_APP_USER_PROFILE_50_DIR, &per, getter_AddRefs(mProfileDir));
      NS_ASSERTION(per, "NS_APP_USER_PROFILE_50_DIR must be persistent!"); 
      NS_ASSERTION(mProfileDir, "NS_APP_USER_PROFILE_50_DIR not defined! This shouldn't happen!"); 
    }
  }

  LoadAppBundleDirs();
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
  else if (!strcmp(aProperty, XRE_UPDATE_ROOT_DIR)) {
#if defined(XP_WIN) && !defined(WINCE)
    rv = GetUpdateRootDir(getter_AddRefs(file));
#else
    // Only supported on Windows other than WINCE, so just immediately fail.
    return NS_ERROR_FAILURE;
#endif
  }
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
#if defined(XP_UNIX) || defined(XP_MACOSX)
  else if (!strcmp(aProperty, XRE_SYS_LOCAL_EXTENSION_PARENT_DIR)) {
    return GetSystemExtensionsDirectory((nsILocalFile**)(nsIFile**) aFile);
  }
#endif
#if defined(XP_UNIX) && !defined(XP_MACOSX)
  else if (!strcmp(aProperty, XRE_SYS_SHARE_EXTENSION_PARENT_DIR)) {
    static const char *const sysLExtDir = "/usr/share/mozilla/extensions";
    return NS_NewNativeLocalFile(nsDependentCString(sysLExtDir),
                                 PR_FALSE, (nsILocalFile**)(nsIFile**) aFile);
  }
#endif
  else if (!strcmp(aProperty, XRE_USER_SYS_EXTENSION_DIR)) {
    return GetSysUserExtensionsDirectory((nsILocalFile**)(nsIFile**) aFile);
  }
  else if (NS_SUCCEEDED(GetProfileStartupDir(getter_AddRefs(file)))) {
    // We need to allow component, xpt, and chrome registration to
    // occur prior to the profile-after-change notification.
    if (!strcmp(aProperty, NS_APP_USER_CHROME_DIR)) {
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
    else if (!strcmp(aProperty, NS_APP_PREFS_OVERRIDE_DIR)) {
      rv = mProfileDir->Clone(getter_AddRefs(file));
      rv |= file->AppendNative(NS_LITERAL_CSTRING(PREF_OVERRIDE_DIRNAME));
      rv |= EnsureDirectoryExists(file);
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
LoadAppDirIntoArray(nsIFile* aXULAppDir,
                    const char *const *aAppendList,
                    nsCOMArray<nsIFile>& aDirectories)
{
  if (!aXULAppDir)
    return;

  nsCOMPtr<nsIFile> subdir;
  aXULAppDir->Clone(getter_AddRefs(subdir));
  if (!subdir)
    return;

  for (; *aAppendList; ++aAppendList)
    subdir->AppendNative(nsDependentCString(*aAppendList));

  PRBool exists;
  if (NS_SUCCEEDED(subdir->Exists(&exists)) && exists)
    aDirectories.AppendObject(subdir);
}

static void
LoadDirsIntoArray(nsCOMArray<nsIFile>& aSourceDirs,
                  const char *const* aAppendList,
                  nsCOMArray<nsIFile>& aDirectories)
{
  nsCOMPtr<nsIFile> appended;
  PRBool exists;

  for (PRInt32 i = 0; i < aSourceDirs.Count(); ++i) {
    aSourceDirs[i]->Clone(getter_AddRefs(appended));
    if (!appended)
      continue;

    for (const char *const *a = aAppendList; *a; ++a)
      appended->AppendNative(nsDependentCString(*a));

    if (NS_SUCCEEDED(appended->Exists(&exists)) && exists)
      aDirectories.AppendObject(appended);
  }
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

static void
LoadExtensionDirectories(nsINIParser &parser,
                         const char *aSection,
                         nsCOMArray<nsIFile> &aDirectories,
                         NSLocationType aType)
{
  nsresult rv;
  PRInt32 i = 0;
  nsCOMPtr<nsIPrefServiceInternal> prefs =
    do_GetService("@mozilla.org/preferences-service;1");
  do {
    nsCAutoString buf("Extension");
    buf.AppendInt(i++);

    nsCAutoString path;
    rv = parser.GetString(aSection, buf.get(), path);
    if (NS_FAILED(rv))
      return;

    nsCOMPtr<nsILocalFile> dir = do_CreateInstance("@mozilla.org/file/local;1", &rv);
    if (NS_FAILED(rv))
      continue;

    rv = dir->SetPersistentDescriptor(path);
    if (NS_FAILED(rv))
      continue;

    if (Substring(path, path.Length() - 4).Equals(NS_LITERAL_CSTRING(".xpi"))) {
      XRE_AddJarManifestLocation(aType, dir);
      if (!prefs)
        continue;
      prefs->ReadExtensionPrefs(dir);
    }
    else {
      aDirectories.AppendObject(dir);

      nsCOMPtr<nsILocalFile> manifest =
        CloneAndAppend(dir, "chrome.manifest");
      XRE_AddManifestLocation(aType, manifest);
    }
  }
  while (PR_TRUE);
}

void
nsXREDirProvider::LoadExtensionBundleDirectories()
{
  if (mProfileDir && !gSafeMode) {
    nsCOMPtr<nsIFile> extensionsINI;
    mProfileDir->Clone(getter_AddRefs(extensionsINI));
    if (!extensionsINI)
      return;

    extensionsINI->AppendNative(NS_LITERAL_CSTRING("extensions.ini"));

    nsCOMPtr<nsILocalFile> extensionsINILF =
      do_QueryInterface(extensionsINI);
    if (!extensionsINILF)
      return;

    nsINIParser parser;
    nsresult rv = parser.Init(extensionsINILF);
    if (NS_FAILED(rv))
      return;

    LoadExtensionDirectories(parser, "ExtensionDirs", mExtensionDirectories,
                             NS_COMPONENT_LOCATION);
    LoadExtensionDirectories(parser, "ThemeDirs", mThemeDirectories,
                             NS_SKIN_LOCATION);
  }
}

void
nsXREDirProvider::LoadAppBundleDirs()
{
  nsCOMPtr<nsIFile> dir;
  nsresult rv = mXULAppDir->Clone(getter_AddRefs(dir));
  if (NS_FAILED(rv))
    return;

  dir->AppendNative(NS_LITERAL_CSTRING("distribution"));
  dir->AppendNative(NS_LITERAL_CSTRING("bundles"));

  nsCOMPtr<nsISimpleEnumerator> e;
  rv = dir->GetDirectoryEntries(getter_AddRefs(e));
  if (NS_FAILED(rv))
    return;

  nsCOMPtr<nsIDirectoryEnumerator> files = do_QueryInterface(e);
  if (!files)
    return;

  nsCOMPtr<nsIFile> subdir;
  while (NS_SUCCEEDED(files->GetNextFile(getter_AddRefs(subdir))) && subdir) {
    mAppBundleDirectories.AppendObject(subdir);

    nsCOMPtr<nsILocalFile> manifest =
      CloneAndAppend(subdir, "chrome.manifest");
    XRE_AddManifestLocation(NS_COMPONENT_LOCATION, manifest);
  }
}

static const char *const kAppendPrefDir[] = { "defaults", "preferences", nsnull };

#ifdef DEBUG_bsmedberg
static void
DumpFileArray(const char *key,
              nsCOMArray<nsIFile> dirs)
{
  fprintf(stderr, "nsXREDirProvider::GetFilesInternal(%s)\n", key);

  nsCAutoString path;
  for (PRInt32 i = 0; i < dirs.Count(); ++i) {
    dirs[i]->GetNativePath(path);
    fprintf(stderr, "  %s\n", path.get());
  }
}
#endif // DEBUG_bsmedberg

nsresult
nsXREDirProvider::GetFilesInternal(const char* aProperty,
                                   nsISimpleEnumerator** aResult)
{
  nsresult rv = NS_OK;
  *aResult = nsnull;

  if (!strcmp(aProperty, XRE_EXTENSIONS_DIR_LIST)) {
    nsCOMArray<nsIFile> directories;

    static const char *const kAppendNothing[] = { nsnull };

    LoadDirsIntoArray(mAppBundleDirectories,
                      kAppendNothing, directories);
    LoadDirsIntoArray(mExtensionDirectories,
                      kAppendNothing, directories);

    rv = NS_NewArrayEnumerator(aResult, directories);
  }
  else if (!strcmp(aProperty, NS_APP_PREFS_DEFAULTS_DIR_LIST)) {
    nsCOMArray<nsIFile> directories;

    LoadAppDirIntoArray(mXULAppDir, kAppendPrefDir, directories);
    LoadDirsIntoArray(mAppBundleDirectories,
                      kAppendPrefDir, directories);

    rv = NS_NewArrayEnumerator(aResult, directories);
  }
  else if (!strcmp(aProperty, NS_EXT_PREFS_DEFAULTS_DIR_LIST)) {
    nsCOMArray<nsIFile> directories;

    LoadDirsIntoArray(mExtensionDirectories,
                      kAppendPrefDir, directories);

    if (mProfileDir) {
      nsCOMPtr<nsIFile> overrideFile;
      mProfileDir->Clone(getter_AddRefs(overrideFile));
      overrideFile->AppendNative(NS_LITERAL_CSTRING(PREF_OVERRIDE_DIRNAME));

      PRBool exists;
      if (NS_SUCCEEDED(overrideFile->Exists(&exists)) && exists)
        directories.AppendObject(overrideFile);
    }

    rv = NS_NewArrayEnumerator(aResult, directories);
  }
  else if (!strcmp(aProperty, NS_APP_CHROME_DIR_LIST)) {
    // NS_APP_CHROME_DIR_LIST is only used to get default (native) icons
    // for OS window decoration.

    static const char *const kAppendChromeDir[] = { "chrome", nsnull };
    nsCOMArray<nsIFile> directories;
    LoadAppDirIntoArray(mXULAppDir,
                        kAppendChromeDir,
                        directories);
    LoadDirsIntoArray(mAppBundleDirectories,
                      kAppendChromeDir,
                      directories);
    LoadDirsIntoArray(mExtensionDirectories,
                      kAppendChromeDir,
                      directories);

    rv = NS_NewArrayEnumerator(aResult, directories);
  }
  else if (!strcmp(aProperty, NS_APP_PLUGINS_DIR_LIST)) {
    static const char *const kAppendPlugins[] = { "plugins", nsnull };
    nsCOMArray<nsIFile> directories;

    // The root dirserviceprovider does quite a bit for us: we're mainly
    // interested in xulapp and extension-provided plugins.
    LoadDirsIntoArray(mAppBundleDirectories,
                      kAppendPlugins,
                      directories);
    LoadDirsIntoArray(mExtensionDirectories,
                      kAppendPlugins,
                      directories);

    if (mProfileDir) {
      nsCOMArray<nsIFile> profileDir;
      profileDir.AppendObject(mProfileDir);
      LoadDirsIntoArray(profileDir,
                        kAppendPlugins,
                        directories);
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
    nsCOMPtr<nsIObserverService> obsSvc =
      mozilla::services::GetObserverService();
    if (!obsSvc) return NS_ERROR_FAILURE;

    mProfileNotified = PR_TRUE;

    static const PRUnichar kStartup[] = {'s','t','a','r','t','u','p','\0'};
    obsSvc->NotifyObservers(nsnull, "profile-do-change", kStartup);
    // Init the Extension Manager
    nsCOMPtr<nsIObserver> em = do_GetService("@mozilla.org/addons/integration;1");
    if (em) {
      em->Observe(nsnull, "addons-startup", nsnull);
    } else {
      NS_WARNING("Failed to create Addons Manager.");
    }

    LoadExtensionBundleDirectories();

    obsSvc->NotifyObservers(nsnull, "load-extension-defaults", nsnull);
    obsSvc->NotifyObservers(nsnull, "profile-after-change", kStartup);

    // Any component that has registered for the profile-after-change category
    // should also be created at this time.
    (void)NS_CreateServicesFromCategory("profile-after-change", nsnull,
                                        "profile-after-change");

    obsSvc->NotifyObservers(nsnull, "profile-initial-state", nsnull);
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
    nsCOMPtr<nsIObserverService> obsSvc =
      mozilla::services::GetObserverService();
    NS_ASSERTION(obsSvc, "No observer service?");
    if (obsSvc) {
      nsCOMPtr<nsIProfileChangeStatus> cs = new ProfileChangeStatusImpl();
      static const PRUnichar kShutdownPersist[] =
        {'s','h','u','t','d','o','w','n','-','p','e','r','s','i','s','t','\0'};
      obsSvc->NotifyObservers(cs, "profile-change-net-teardown", kShutdownPersist);
      obsSvc->NotifyObservers(cs, "profile-change-teardown", kShutdownPersist);

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
      obsSvc->NotifyObservers(cs, "profile-before-change", kShutdownPersist);
    }
    mProfileNotified = PR_FALSE;
  }
}

#ifdef XP_WIN
static nsresult
GetShellFolderPath(int folder, nsAString& _retval)
{
  PRUnichar* buf;
  PRUint32 bufLength = _retval.GetMutableData(&buf, MAXPATHLEN + 3);
  NS_ENSURE_TRUE(bufLength >= (MAXPATHLEN + 3), NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = NS_OK;

#if defined(WINCE) && !defined(WINCE_WINDOWS_MOBILE)
  if (folder == CSIDL_APPDATA || folder == CSIDL_LOCAL_APPDATA)
    folder = CSIDL_PROFILE;

  BOOL ok = SHGetSpecialFolderPath(NULL, buf, folder, true);
  if (!ok) {
    _retval.SetLength(0);
    return NS_ERROR_FAILURE;
  }

  buf[bufLength - 1] = L'\0';
  _retval.SetLength(wcslen(buf));

  // sometimes CSIDL_PROFILE shows up without a root slash
  if (folder == CSIDL_PROFILE && buf[0] != '\\') {
    _retval.Insert('\\', 0);
  }
#else
  LPITEMIDLIST pItemIDList = NULL;

  if (SUCCEEDED(SHGetSpecialFolderLocation(NULL, folder, &pItemIDList)) &&
      SHGetPathFromIDListW(pItemIDList, buf)) {
    // We're going to use wcslen (wcsnlen not available in msvc7.1) so make
    // sure to null terminate.
    buf[bufLength - 1] = L'\0';
    _retval.SetLength(wcslen(buf));
  } else {
    _retval.SetLength(0);
    rv = NS_ERROR_NOT_AVAILABLE;
  }

  CoTaskMemFree(pItemIDList);
#endif

  return rv;
}

#ifndef WINCE
/**
 * Provides a fallback for getting the path to APPDATA or LOCALAPPDATA by
 * querying the registry when the call to SHGetSpecialFolderLocation or
 * SHGetPathFromIDListW is unable to provide these paths (Bug 513958).
 */
static nsresult
GetRegWindowsAppDataFolder(PRBool aLocal, nsAString& _retval)
{
  HKEY key;
  NS_NAMED_LITERAL_STRING(keyName,
  "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders");
  DWORD res = ::RegOpenKeyExW(HKEY_CURRENT_USER, keyName.get(), 0, KEY_READ,
                              &key);
  if (res != ERROR_SUCCESS) {
    _retval.SetLength(0);
    return NS_ERROR_NOT_AVAILABLE;
  }

  DWORD type, size;
  res = RegQueryValueExW(key, (aLocal ? L"Local AppData" : L"AppData"), NULL,
                         &type, NULL, &size);
  // The call to RegQueryValueExW must succeed, the type must be REG_SZ, the
  // buffer size must not equal 0, and the buffer size be a multiple of 2.
  if (res != ERROR_SUCCESS || type != REG_SZ || size == 0 || size % 2 != 0) {
    ::RegCloseKey(key);
    _retval.SetLength(0);
    return NS_ERROR_NOT_AVAILABLE;
  }

  // |size| includes room for the terminating null character
  DWORD resultLen = size / 2 - 1;

  _retval.SetLength(resultLen);
  nsAString::iterator begin;
  _retval.BeginWriting(begin);
  if (begin.size_forward() != resultLen) {
    ::RegCloseKey(key);
    _retval.SetLength(0);
    return NS_ERROR_NOT_AVAILABLE;
  }

  res = RegQueryValueExW(key, (aLocal ? L"Local AppData" : L"AppData"), NULL,
                         NULL, (LPBYTE) begin.get(), &size);
  ::RegCloseKey(key);
  if (res != ERROR_SUCCESS) {
    _retval.SetLength(0);
    return NS_ERROR_NOT_AVAILABLE;
  }

  return NS_OK;
}
#endif

nsresult
nsXREDirProvider::GetUpdateRootDir(nsIFile* *aResult)
{
  nsCOMPtr<nsIFile> appDir = GetAppDir();

  nsAutoString appPath;
  nsresult rv = appDir->GetPath(appPath);
  NS_ENSURE_SUCCESS(rv, rv);

  // AppDir may be a short path. Convert to long path to make sure
  // the consistency of the update folder location
  nsString longPath;
  PRUnichar* buf;

  PRUint32 bufLength = longPath.GetMutableData(&buf, MAXPATHLEN);
  NS_ENSURE_TRUE(bufLength >= MAXPATHLEN, NS_ERROR_OUT_OF_MEMORY);

#ifdef WINCE
  longPath.Assign(appPath);
#else
  DWORD len = GetLongPathNameW(appPath.get(), buf, bufLength);

  // Failing GetLongPathName() is not fatal.
  if (len <= 0 || len >= bufLength)
    longPath.Assign(appPath);
  else
    longPath.SetLength(len);
#endif
  // Use <UserLocalDataDir>\updates\<relative path to app dir from
  // Program Files> if app dir is under Program Files to avoid the
  // folder virtualization mess on Windows Vista
  nsAutoString programFiles;
  rv = GetShellFolderPath(CSIDL_PROGRAM_FILES, programFiles);
  NS_ENSURE_SUCCESS(rv, rv);

  programFiles.AppendLiteral("\\");
  PRUint32 programFilesLen = programFiles.Length();

  if (longPath.Length() < programFilesLen)
    return NS_ERROR_FAILURE;

  if (_wcsnicmp(programFiles.get(), longPath.get(), programFilesLen) != 0)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsILocalFile> updRoot;
  rv = GetUserLocalDataDirectory(getter_AddRefs(updRoot));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = updRoot->AppendRelativePath(Substring(longPath, programFilesLen));
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
nsXREDirProvider::GetUserDataDirectoryHome(nsILocalFile** aFile, PRBool aLocal)
{
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

  localDir = do_QueryInterface(dirFileMac, &rv);
#elif defined(XP_WIN)
  nsString path;
  if (aLocal) {
    rv = GetShellFolderPath(CSIDL_LOCAL_APPDATA, path);
#ifndef WINCE
    if (NS_FAILED(rv))
      rv = GetRegWindowsAppDataFolder(aLocal, path);
#endif
  }
  if (!aLocal || NS_FAILED(rv)) {
    rv = GetShellFolderPath(CSIDL_APPDATA, path);
#ifndef WINCE
    if (NS_FAILED(rv)) {
      if (!aLocal)
        rv = GetRegWindowsAppDataFolder(aLocal, path);
    }
#endif

  }
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_NewLocalFile(path, PR_TRUE, getter_AddRefs(localDir));
#elif defined(XP_OS2)
#if 0 /* For OS/2 we want to always use MOZILLA_HOME */
  // we want an environment variable of the form
  // FIREFOX_HOME, etc
  if (!gAppData)
    return NS_ERROR_FAILURE;
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
#elif defined(XP_BEOS)
  char appDir[MAXPATHLEN];
  if (find_directory(B_USER_SETTINGS_DIRECTORY, NULL, true, appDir, MAXPATHLEN))
    return NS_ERROR_FAILURE;

  int len = strlen(appDir);
  appDir[len]   = '/';
  appDir[len+1] = '\0';

  rv = NS_NewNativeLocalFile(nsDependentCString(appDir), PR_TRUE,
                             getter_AddRefs(localDir));
#elif defined(ANDROID)
  // used for setting the patch to our profile
  // XXX: investigate putting the profile somewhere else
  const char* homeDir = "/data/data/org.mozilla." MOZ_APP_NAME;

  rv = NS_NewNativeLocalFile(nsDependentCString(homeDir), PR_TRUE,
                             getter_AddRefs(localDir));
#elif defined(XP_UNIX)
  const char* homeDir = getenv("HOME");
  if (!homeDir || !*homeDir)
    return NS_ERROR_FAILURE;

  rv = NS_NewNativeLocalFile(nsDependentCString(homeDir), PR_TRUE,
                             getter_AddRefs(localDir));
#else
#error "Don't know how to get product dir on your platform"
#endif

  NS_IF_ADDREF(*aFile = localDir);
  return rv;
}

nsresult
nsXREDirProvider::GetSysUserExtensionsDirectory(nsILocalFile** aFile)
{
  nsCOMPtr<nsILocalFile> localDir;
  nsresult rv = GetUserDataDirectoryHome(getter_AddRefs(localDir), PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AppendSysUserExtensionPath(localDir);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = EnsureDirectoryExists(localDir);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ADDREF(*aFile = localDir);
  return NS_OK;
}

#if defined(XP_UNIX) || defined(XP_MACOSX)
nsresult
nsXREDirProvider::GetSystemExtensionsDirectory(nsILocalFile** aFile)
{
  nsresult rv;
  nsCOMPtr<nsILocalFile> localDir;
#if defined(XP_MACOSX)
  FSRef fsRef;
  OSErr err = ::FSFindFolder(kOnSystemDisk, kApplicationSupportFolderType, kCreateFolder, &fsRef);
  NS_ENSURE_FALSE(err, NS_ERROR_FAILURE);

  rv = NS_NewNativeLocalFile(EmptyCString(), PR_TRUE, getter_AddRefs(localDir));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsILocalFileMac> dirFileMac = do_QueryInterface(localDir);
  NS_ENSURE_TRUE(dirFileMac, NS_ERROR_UNEXPECTED);

  rv = dirFileMac->InitWithFSRef(&fsRef);
  NS_ENSURE_SUCCESS(rv, rv);

  localDir = do_QueryInterface(dirFileMac, &rv);

  static const char* const sXR = "Mozilla";
  rv = localDir->AppendNative(nsDependentCString(sXR));
  NS_ENSURE_SUCCESS(rv, rv);

  static const char* const sExtensions = "Extensions";
  rv = localDir->AppendNative(nsDependentCString(sExtensions));
  NS_ENSURE_SUCCESS(rv, rv);
#elif defined(XP_UNIX)
  static const char *const sysSExtDir = 
#ifdef HAVE_USR_LIB64_DIR
    "/usr/lib64/mozilla/extensions";
#else
    "/usr/lib/mozilla/extensions";
#endif

  rv = NS_NewNativeLocalFile(nsDependentCString(sysSExtDir), PR_FALSE,
                             getter_AddRefs(localDir));
  NS_ENSURE_SUCCESS(rv, rv);
#endif

  NS_ADDREF(*aFile = localDir);
  return NS_OK;
}
#endif

nsresult
nsXREDirProvider::GetUserDataDirectory(nsILocalFile** aFile, PRBool aLocal)
{
  nsCOMPtr<nsILocalFile> localDir;
  nsresult rv = GetUserDataDirectoryHome(getter_AddRefs(localDir), aLocal);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AppendProfilePath(localDir);
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG_jungshik
  nsCAutoString cwd;
  localDir->GetNativePath(cwd);
  printf("nsXREDirProvider::GetUserDataDirectory: %s\n", cwd.get());
#endif
  rv = EnsureDirectoryExists(localDir);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ADDREF(*aFile = localDir);
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
  NS_ENSURE_SUCCESS(rv, rv);

  rv = defaultsDir->AppendNative(NS_LITERAL_CSTRING("profile"));
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ADDREF(*aResult = defaultsDir);
  return NS_OK;
}

nsresult
nsXREDirProvider::AppendSysUserExtensionPath(nsIFile* aFile)
{
  NS_ASSERTION(aFile, "Null pointer!");

  nsresult rv;

#if defined (XP_MACOSX) || defined(XP_WIN) || defined(XP_OS2) || defined(XP_BEOS)

  static const char* const sXR = "Mozilla";
  rv = aFile->AppendNative(nsDependentCString(sXR));
  NS_ENSURE_SUCCESS(rv, rv);

  static const char* const sExtensions = "Extensions";
  rv = aFile->AppendNative(nsDependentCString(sExtensions));
  NS_ENSURE_SUCCESS(rv, rv);

#elif defined(XP_UNIX)

  static const char* const sXR = ".mozilla";
  rv = aFile->AppendNative(nsDependentCString(sXR));
  NS_ENSURE_SUCCESS(rv, rv);

  static const char* const sExtensions = "extensions";
  rv = aFile->AppendNative(nsDependentCString(sExtensions));
  NS_ENSURE_SUCCESS(rv, rv);

#else
#error "Don't know how to get XRE user extension path on your platform"
#endif
  return NS_OK;
}


nsresult
nsXREDirProvider::AppendProfilePath(nsIFile* aFile)
{
  NS_ASSERTION(aFile, "Null pointer!");

  nsresult rv;

  if (!gAppData)
    return NS_ERROR_FAILURE;

#if defined (XP_MACOSX)
  if (gAppData->profile) {
    rv = AppendProfileString(aFile, gAppData->profile);
  }
  else {
    // Note that MacOS ignores the vendor when creating the profile hierarchy -
    // all application preferences directories live alongside one another in
    // ~/Library/Application Support/
    rv = aFile->AppendNative(nsDependentCString(gAppData->name));
  }
  NS_ENSURE_SUCCESS(rv, rv);

#elif defined(XP_WIN) || defined(XP_OS2) || defined(XP_BEOS)
  if (gAppData->profile) {
    rv = AppendProfileString(aFile, gAppData->profile);
  }
  else {
    if (gAppData->vendor) {
      rv = aFile->AppendNative(nsDependentCString(gAppData->vendor));
      NS_ENSURE_SUCCESS(rv, rv);
    }
    rv = aFile->AppendNative(nsDependentCString(gAppData->name));
  }
  NS_ENSURE_SUCCESS(rv, rv);

#elif defined(ANDROID)
  // The directory used for storing profiles
  // The parent of this directory is set in GetUserDataDirectoryHome
  // XXX: handle gAppData->profile properly
  rv = aFile->AppendNative(nsDependentCString("mozilla"));
  NS_ENSURE_SUCCESS(rv, rv);
#elif defined(XP_UNIX)
  // Make it hidden (i.e. using the ".")
  nsCAutoString folder(".");

  if (gAppData->profile) {
    // Skip any leading path characters
    const char* profileStart = gAppData->profile;
    while (*profileStart == '/' || *profileStart == '\\')
      profileStart++;

    // On the off chance that someone wanted their folder to be hidden don't
    // let it become ".."
    if (*profileStart == '.')
      profileStart++;

    folder.Append(profileStart);
    ToLowerCase(folder);

    rv = AppendProfileString(aFile, folder.BeginReading());
  }
  else {
    if (gAppData->vendor) {
      folder.Append(gAppData->vendor);
      ToLowerCase(folder);

      rv = aFile->AppendNative(folder);
      NS_ENSURE_SUCCESS(rv, rv);

      folder.Truncate();
    }

    folder.Append(gAppData->name);
    ToLowerCase(folder);

    rv = aFile->AppendNative(folder);
  }
  NS_ENSURE_SUCCESS(rv, rv);

#else
#error "Don't know how to get profile path on your platform"
#endif
  return NS_OK;
}

nsresult
nsXREDirProvider::AppendProfileString(nsIFile* aFile, const char* aPath)
{
  NS_ASSERTION(aFile, "Null file!");
  NS_ASSERTION(aPath, "Null path!");

  nsCAutoString pathDup(aPath);

  char* path = pathDup.BeginWriting();

  nsresult rv;
  char* subdir;
  while ((subdir = NS_strtok("/\\", &path))) {
    rv = aFile->AppendNative(nsDependentCString(subdir));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}
