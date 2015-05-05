/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAppRunner.h"
#include "nsToolkitCompsCID.h"
#include "nsXREDirProvider.h"

#include "jsapi.h"
#include "xpcpublic.h"

#include "nsIJSRuntimeService.h"
#include "nsIAddonInterposition.h"
#include "nsIAppStartup.h"
#include "nsIDirectoryEnumerator.h"
#include "nsIFile.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsISimpleEnumerator.h"
#include "nsIToolkitChromeRegistry.h"
#include "nsIXULRuntime.h"

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
#include "mozilla/Preferences.h"
#include "mozilla/Telemetry.h"

#include <stdlib.h>

#ifdef XP_WIN
#include <windows.h>
#include <shlobj.h>
#endif
#ifdef XP_MACOSX
#include "nsILocalFileMac.h"
// for chflags()
#include <sys/stat.h>
#include <unistd.h>
#endif
#ifdef XP_UNIX
#include <ctype.h>
#endif

#if defined(XP_MACOSX)
#define APP_REGISTRY_NAME "Application Registry"
#elif defined(XP_WIN)
#define APP_REGISTRY_NAME "registry.dat"
#else
#define APP_REGISTRY_NAME "appreg"
#endif

#define PREF_OVERRIDE_DIRNAME "preferences"

static already_AddRefed<nsIFile>
CloneAndAppend(nsIFile* aFile, const char* name)
{
  nsCOMPtr<nsIFile> file;
  aFile->Clone(getter_AddRefs(file));
  file->AppendNative(nsDependentCString(name));
  return file.forget();
}

nsXREDirProvider* gDirServiceProvider = nullptr;

nsXREDirProvider::nsXREDirProvider() :
  mProfileNotified(false)
{
  gDirServiceProvider = this;
}

nsXREDirProvider::~nsXREDirProvider()
{
  gDirServiceProvider = nullptr;
}

nsXREDirProvider*
nsXREDirProvider::GetSingleton()
{
  return gDirServiceProvider;
}

nsresult
nsXREDirProvider::Initialize(nsIFile *aXULAppDir,
                             nsIFile *aGREDir,
                             nsIDirectoryServiceProvider* aAppProvider)
{
  NS_ENSURE_ARG(aXULAppDir);
  NS_ENSURE_ARG(aGREDir);

  mAppProvider = aAppProvider;
  mXULAppDir = aXULAppDir;
  mGREDir = aGREDir;
  mGREDir->Clone(getter_AddRefs(mGREBinDir));
#ifdef XP_MACOSX
  mGREBinDir->SetNativeLeafName(NS_LITERAL_CSTRING("MacOS"));
#endif

  if (!mProfileDir) {
    nsCOMPtr<nsIDirectoryServiceProvider> app(do_QueryInterface(mAppProvider));
    if (app) {
      bool per = false;
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

#ifdef XP_MACOSX
  bool same;
  if (NS_SUCCEEDED(aDir->Equals(aLocalDir, &same)) && !same) {
    // Ensure that the cache directory is not indexed by Spotlight
    // (bug 718910).  At least on OS X, the cache directory (under
    // ~/Library/Caches/) is always the "local" user profile
    // directory.  This is confusing, since *both* user profile
    // directories are "local" (they both exist under the user's
    // home directory).  But this usage dates back at least as far
    // as the patch for bug 291033, where "local" seems to mean
    // "suitable for temporary storage".  Don't hide the cache
    // directory if by some chance it and the "non-local" profile
    // directory are the same -- there are bad side effects from
    // hiding a profile directory under /Library/Application Support/
    // (see bug 801883).
    nsAutoCString cacheDir;
    if (NS_SUCCEEDED(aLocalDir->GetNativePath(cacheDir))) {
      if (chflags(cacheDir.get(), UF_HIDDEN)) {
        NS_WARNING("Failed to set Cache directory to HIDDEN.");
      }
    }
  }
#endif

  mProfileDir = aDir;
  mProfileLocalDir = aLocalDir;
  return NS_OK;
}

NS_IMPL_QUERY_INTERFACE(nsXREDirProvider,
                        nsIDirectoryServiceProvider,
                        nsIDirectoryServiceProvider2,
                        nsIProfileStartup)

NS_IMETHODIMP_(MozExternalRefCountType)
nsXREDirProvider::AddRef()
{
  return 1;
}

NS_IMETHODIMP_(MozExternalRefCountType)
nsXREDirProvider::Release()
{
  return 0;
}

nsresult
nsXREDirProvider::GetUserProfilesRootDir(nsIFile** aResult,
                                         const nsACString* aProfileName,
                                         const nsACString* aAppName,
                                         const nsACString* aVendorName)
{
  nsCOMPtr<nsIFile> file;
  nsresult rv = GetUserDataDirectory(getter_AddRefs(file),
                                     false,
                                     aProfileName, aAppName, aVendorName);

  if (NS_SUCCEEDED(rv)) {
#if !defined(XP_UNIX) || defined(XP_MACOSX)
    rv = file->AppendNative(NS_LITERAL_CSTRING("Profiles"));
#endif
    // We must create the profile directory here if it does not exist.
    nsresult tmp = EnsureDirectoryExists(file);
    if (NS_FAILED(tmp)) {
      rv = tmp;
    }
  }
  file.swap(*aResult);
  return rv;
}

nsresult
nsXREDirProvider::GetUserProfilesLocalDir(nsIFile** aResult,
                                          const nsACString* aProfileName,
                                          const nsACString* aAppName,
                                          const nsACString* aVendorName)
{
  nsCOMPtr<nsIFile> file;
  nsresult rv = GetUserDataDirectory(getter_AddRefs(file),
                                     true,
                                     aProfileName, aAppName, aVendorName);

  if (NS_SUCCEEDED(rv)) {
#if !defined(XP_UNIX) || defined(XP_MACOSX)
    rv = file->AppendNative(NS_LITERAL_CSTRING("Profiles"));
#endif
    // We must create the profile directory here if it does not exist.
    nsresult tmp = EnsureDirectoryExists(file);
    if (NS_FAILED(tmp)) {
      rv = tmp;
    }
  }
  file.swap(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsXREDirProvider::GetFile(const char* aProperty, bool* aPersistent,
                          nsIFile** aFile)
{
  nsresult rv;

  bool gettingProfile = false;

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
    gettingProfile = true;
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

  *aPersistent = true;

  if (!strcmp(aProperty, NS_GRE_DIR)) {
    return mGREDir->Clone(aFile);
  }
  else if (!strcmp(aProperty, NS_GRE_BIN_DIR)) {
    return mGREBinDir->Clone(aFile);
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
    rv = GetUserAppDataDirectory(getter_AddRefs(file));
  }
  else if (!strcmp(aProperty, XRE_UPDATE_ROOT_DIR)) {
    rv = GetUpdateRootDir(getter_AddRefs(file));
  }
  else if (!strcmp(aProperty, NS_APP_APPLICATION_REGISTRY_FILE)) {
    rv = GetUserAppDataDirectory(getter_AddRefs(file));
    if (NS_SUCCEEDED(rv))
      rv = file->AppendNative(NS_LITERAL_CSTRING(APP_REGISTRY_NAME));
  }
  else if (!strcmp(aProperty, NS_APP_USER_PROFILES_ROOT_DIR)) {
    rv = GetUserProfilesRootDir(getter_AddRefs(file), nullptr, nullptr, nullptr);
  }
  else if (!strcmp(aProperty, NS_APP_USER_PROFILES_LOCAL_ROOT_DIR)) {
    rv = GetUserProfilesLocalDir(getter_AddRefs(file), nullptr, nullptr, nullptr);
  }
  else if (!strcmp(aProperty, XRE_EXECUTABLE_FILE) && gArgv[0]) {
    nsCOMPtr<nsIFile> lf;
    rv = XRE_GetBinaryPath(gArgv[0], getter_AddRefs(lf));
    if (NS_SUCCEEDED(rv))
      file = lf;
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
#ifdef ENABLE_SYSTEM_EXTENSION_DIRS
    return GetSystemExtensionsDirectory(aFile);
#else
    return NS_ERROR_FAILURE;
#endif
  }
#endif
#if defined(XP_UNIX) && !defined(XP_MACOSX)
  else if (!strcmp(aProperty, XRE_SYS_SHARE_EXTENSION_PARENT_DIR)) {
#ifdef ENABLE_SYSTEM_EXTENSION_DIRS
#if defined(__OpenBSD__) || defined(__FreeBSD__)
    static const char *const sysLExtDir = "/usr/local/share/mozilla/extensions";
#else
    static const char *const sysLExtDir = "/usr/share/mozilla/extensions";
#endif
    return NS_NewNativeLocalFile(nsDependentCString(sysLExtDir),
                                 false, aFile);
#else
    return NS_ERROR_FAILURE;
#endif
  }
#endif
  else if (!strcmp(aProperty, XRE_USER_SYS_EXTENSION_DIR)) {
#ifdef ENABLE_SYSTEM_EXTENSION_DIRS
    return GetSysUserExtensionsDirectory(aFile);
#else
    return NS_ERROR_FAILURE;
#endif
  }
  else if (!strcmp(aProperty, XRE_APP_DISTRIBUTION_DIR)) {
    bool persistent = false;
    rv = GetFile(NS_GRE_DIR, &persistent, getter_AddRefs(file));
    if (NS_SUCCEEDED(rv))
      rv = file->AppendNative(NS_LITERAL_CSTRING("distribution"));
  }
  else if (NS_SUCCEEDED(GetProfileStartupDir(getter_AddRefs(file)))) {
    // We need to allow component, xpt, and chrome registration to
    // occur prior to the profile-after-change notification.
    if (!strcmp(aProperty, NS_APP_USER_CHROME_DIR)) {
      rv = file->AppendNative(NS_LITERAL_CSTRING("chrome"));
    }
  }

  if (NS_SUCCEEDED(rv) && file) {
    file.forget(aFile);
    return NS_OK;
  }

  bool ensureFilePermissions = false;

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
        file->Remove(false);
      }
      else {
        rv = file->AppendNative(NS_LITERAL_CSTRING("localstore.rdf"));
        EnsureProfileFileExists(file);
        ensureFilePermissions = true;
      }
    }
    else if (!strcmp(aProperty, NS_APP_USER_MIMETYPES_50_FILE)) {
      rv = file->AppendNative(NS_LITERAL_CSTRING("mimeTypes.rdf"));
      EnsureProfileFileExists(file);
      ensureFilePermissions = true;
    }
    else if (!strcmp(aProperty, NS_APP_DOWNLOADS_50_FILE)) {
      rv = file->AppendNative(NS_LITERAL_CSTRING("downloads.rdf"));
    }
    else if (!strcmp(aProperty, NS_APP_PREFS_OVERRIDE_DIR)) {
      rv = mProfileDir->Clone(getter_AddRefs(file));
      nsresult tmp = file->AppendNative(NS_LITERAL_CSTRING(PREF_OVERRIDE_DIRNAME));
      if (NS_FAILED(tmp)) {
        rv = tmp;
      }
      tmp = EnsureDirectoryExists(file);
      if (NS_FAILED(tmp)) {
        rv = tmp;
      }
    }
  }
  if (NS_FAILED(rv) || !file)
    return NS_ERROR_FAILURE;

  if (ensureFilePermissions) {
    bool fileToEnsureExists;
    bool isWritable;
    if (NS_SUCCEEDED(file->Exists(&fileToEnsureExists)) && fileToEnsureExists
        && NS_SUCCEEDED(file->IsWritable(&isWritable)) && !isWritable) {
      uint32_t permissions;
      if (NS_SUCCEEDED(file->GetPermissions(&permissions))) {
        rv = file->SetPermissions(permissions | 0600);
        NS_ASSERTION(NS_SUCCEEDED(rv), "failed to ensure file permissions");
      }
    }
  }

  file.forget(aFile);
  return NS_OK;
}

static void
LoadDirIntoArray(nsIFile* dir,
                 const char *const *aAppendList,
                 nsCOMArray<nsIFile>& aDirectories)
{
  if (!dir)
    return;

  nsCOMPtr<nsIFile> subdir;
  dir->Clone(getter_AddRefs(subdir));
  if (!subdir)
    return;

  for (const char *const *a = aAppendList; *a; ++a) {
    subdir->AppendNative(nsDependentCString(*a));
  }

  bool exists;
  if (NS_SUCCEEDED(subdir->Exists(&exists)) && exists) {
    aDirectories.AppendObject(subdir);
  }
}

static void
LoadDirsIntoArray(nsCOMArray<nsIFile>& aSourceDirs,
                  const char *const* aAppendList,
                  nsCOMArray<nsIFile>& aDirectories)
{
  nsCOMPtr<nsIFile> appended;
  bool exists;

  for (int32_t i = 0; i < aSourceDirs.Count(); ++i) {
    aSourceDirs[i]->Clone(getter_AddRefs(appended));
    if (!appended)
      continue;

    nsAutoCString leaf;
    appended->GetNativeLeafName(leaf);
    if (!Substring(leaf, leaf.Length() - 4).EqualsLiteral(".xpi")) {
      LoadDirIntoArray(appended,
                       aAppendList,
                       aDirectories);
    }
    else if (NS_SUCCEEDED(appended->Exists(&exists)) && exists)
      aDirectories.AppendObject(appended);
  }
}

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
      appEnum = nullptr;
    }
    else if (rv != NS_SUCCESS_AGGREGATE_RESULT) {
      appEnum.forget(aResult);
      return NS_OK;
    }
  }

  nsCOMPtr<nsISimpleEnumerator> xreEnum;
  rv = GetFilesInternal(aProperty, getter_AddRefs(xreEnum));
  if (NS_FAILED(rv)) {
    if (appEnum) {
      appEnum.forget(aResult);
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
RegisterExtensionInterpositions(nsINIParser &parser)
{
  if (!mozilla::Preferences::GetBool("extensions.interposition.enabled", false))
    return;

  nsCOMPtr<nsIAddonInterposition> interposition =
    do_GetService("@mozilla.org/addons/multiprocess-shims;1");

  nsresult rv;
  int32_t i = 0;
  do {
    nsAutoCString buf("Extension");
    buf.AppendInt(i++);

    nsAutoCString addonId;
    rv = parser.GetString("MultiprocessIncompatibleExtensions", buf.get(), addonId);
    if (NS_FAILED(rv))
      return;

    if (!xpc::SetAddonInterposition(addonId, interposition))
      continue;
  }
  while (true);
}

static void
LoadExtensionDirectories(nsINIParser &parser,
                         const char *aSection,
                         nsCOMArray<nsIFile> &aDirectories,
                         NSLocationType aType)
{
  nsresult rv;
  int32_t i = 0;
  do {
    nsAutoCString buf("Extension");
    buf.AppendInt(i++);

    nsAutoCString path;
    rv = parser.GetString(aSection, buf.get(), path);
    if (NS_FAILED(rv))
      return;

    nsCOMPtr<nsIFile> dir = do_CreateInstance("@mozilla.org/file/local;1", &rv);
    if (NS_FAILED(rv))
      continue;

    rv = dir->SetPersistentDescriptor(path);
    if (NS_FAILED(rv))
      continue;

    aDirectories.AppendObject(dir);
    if (Substring(path, path.Length() - 4).EqualsLiteral(".xpi")) {
      XRE_AddJarManifestLocation(aType, dir);
    }
    else {
      nsCOMPtr<nsIFile> manifest =
        CloneAndAppend(dir, "chrome.manifest");
      XRE_AddManifestLocation(aType, manifest);
    }
  }
  while (true);
}

void
nsXREDirProvider::LoadExtensionBundleDirectories()
{
  if (!mozilla::Preferences::GetBool("extensions.defaultProviders.enabled", true))
    return;

  if (mProfileDir && !gSafeMode) {
    nsCOMPtr<nsIFile> extensionsINI;
    mProfileDir->Clone(getter_AddRefs(extensionsINI));
    if (!extensionsINI)
      return;

    extensionsINI->AppendNative(NS_LITERAL_CSTRING("extensions.ini"));

    nsCOMPtr<nsIFile> extensionsINILF =
      do_QueryInterface(extensionsINI);
    if (!extensionsINILF)
      return;

    nsINIParser parser;
    nsresult rv = parser.Init(extensionsINILF);
    if (NS_FAILED(rv))
      return;

    RegisterExtensionInterpositions(parser);
    LoadExtensionDirectories(parser, "ExtensionDirs", mExtensionDirectories,
                             NS_EXTENSION_LOCATION);
    LoadExtensionDirectories(parser, "ThemeDirs", mThemeDirectories,
                             NS_SKIN_LOCATION);
  }
}

void
nsXREDirProvider::LoadAppBundleDirs()
{
  nsCOMPtr<nsIFile> dir;
  bool persistent = false;
  nsresult rv = GetFile(XRE_APP_DISTRIBUTION_DIR, &persistent, getter_AddRefs(dir));
  if (NS_FAILED(rv))
    return;

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

    nsCOMPtr<nsIFile> manifest =
      CloneAndAppend(subdir, "chrome.manifest");
    XRE_AddManifestLocation(NS_EXTENSION_LOCATION, manifest);
  }
}

static const char *const kAppendPrefDir[] = { "defaults", "preferences", nullptr };

#ifdef DEBUG_bsmedberg
static void
DumpFileArray(const char *key,
              nsCOMArray<nsIFile> dirs)
{
  fprintf(stderr, "nsXREDirProvider::GetFilesInternal(%s)\n", key);

  nsAutoCString path;
  for (int32_t i = 0; i < dirs.Count(); ++i) {
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
  *aResult = nullptr;

  if (!strcmp(aProperty, XRE_EXTENSIONS_DIR_LIST)) {
    nsCOMArray<nsIFile> directories;

    static const char *const kAppendNothing[] = { nullptr };

    LoadDirsIntoArray(mAppBundleDirectories,
                      kAppendNothing, directories);
    LoadDirsIntoArray(mExtensionDirectories,
                      kAppendNothing, directories);

    rv = NS_NewArrayEnumerator(aResult, directories);
  }
  else if (!strcmp(aProperty, NS_APP_PREFS_DEFAULTS_DIR_LIST)) {
    nsCOMArray<nsIFile> directories;

    LoadDirIntoArray(mXULAppDir, kAppendPrefDir, directories);
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

      bool exists;
      if (NS_SUCCEEDED(overrideFile->Exists(&exists)) && exists)
        directories.AppendObject(overrideFile);
    }

    rv = NS_NewArrayEnumerator(aResult, directories);
  }
  else if (!strcmp(aProperty, NS_APP_CHROME_DIR_LIST)) {
    // NS_APP_CHROME_DIR_LIST is only used to get default (native) icons
    // for OS window decoration.

    static const char *const kAppendChromeDir[] = { "chrome", nullptr };
    nsCOMArray<nsIFile> directories;
    LoadDirIntoArray(mXULAppDir,
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
    nsCOMArray<nsIFile> directories;

    if (mozilla::Preferences::GetBool("plugins.load_appdir_plugins", false)) {
      nsCOMPtr<nsIFile> appdir;
      rv = XRE_GetBinaryPath(gArgv[0], getter_AddRefs(appdir));
      if (NS_SUCCEEDED(rv)) {
        appdir->SetNativeLeafName(NS_LITERAL_CSTRING("plugins"));
        directories.AppendObject(appdir);
      }
    }

    static const char *const kAppendPlugins[] = { "plugins", nullptr };

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

    mProfileNotified = true;

    /*
       Setup prefs before profile-do-change to be able to use them to track
       crashes and because we want to begin crash tracking before other code run
       from this notification since they may cause crashes.
    */
    nsresult rv = mozilla::Preferences::ResetAndReadUserPrefs();
    if (NS_FAILED(rv)) NS_WARNING("Failed to setup pref service.");

    bool safeModeNecessary = false;
    nsCOMPtr<nsIAppStartup> appStartup (do_GetService(NS_APPSTARTUP_CONTRACTID));
    if (appStartup) {
      rv = appStartup->TrackStartupCrashBegin(&safeModeNecessary);
      if (NS_FAILED(rv) && rv != NS_ERROR_NOT_AVAILABLE)
        NS_WARNING("Error while beginning startup crash tracking");

      if (!gSafeMode && safeModeNecessary) {
        appStartup->RestartInSafeMode(nsIAppStartup::eForceQuit);
        return NS_OK;
      }
    }

    static const char16_t kStartup[] = {'s','t','a','r','t','u','p','\0'};
    obsSvc->NotifyObservers(nullptr, "profile-do-change", kStartup);
    // Init the Extension Manager
    nsCOMPtr<nsIObserver> em = do_GetService("@mozilla.org/addons/integration;1");
    if (em) {
      em->Observe(nullptr, "addons-startup", nullptr);
    } else {
      NS_WARNING("Failed to create Addons Manager.");
    }

    LoadExtensionBundleDirectories();

    obsSvc->NotifyObservers(nullptr, "load-extension-defaults", nullptr);
    obsSvc->NotifyObservers(nullptr, "profile-after-change", kStartup);

    // Any component that has registered for the profile-after-change category
    // should also be created at this time.
    (void)NS_CreateServicesFromCategory("profile-after-change", nullptr,
                                        "profile-after-change");

    if (gSafeMode && safeModeNecessary) {
      static const char16_t kCrashed[] = {'c','r','a','s','h','e','d','\0'};
      obsSvc->NotifyObservers(nullptr, "safemode-forced", kCrashed);
    }

    // 1 = Regular mode, 2 = Safe mode, 3 = Safe mode forced
    int mode = 1;
    if (gSafeMode) {
      if (safeModeNecessary)
        mode = 3;
      else
        mode = 2;
    }
    mozilla::Telemetry::Accumulate(mozilla::Telemetry::SAFE_MODE_USAGE, mode);

    obsSvc->NotifyObservers(nullptr, "profile-initial-state", nullptr);
  }
  return NS_OK;
}

void
nsXREDirProvider::DoShutdown()
{
  if (mProfileNotified) {
    nsCOMPtr<nsIObserverService> obsSvc =
      mozilla::services::GetObserverService();
    NS_ASSERTION(obsSvc, "No observer service?");
    if (obsSvc) {
      static const char16_t kShutdownPersist[] = MOZ_UTF16("shutdown-persist");
      obsSvc->NotifyObservers(nullptr, "profile-change-net-teardown", kShutdownPersist);
      obsSvc->NotifyObservers(nullptr, "profile-change-teardown", kShutdownPersist);

      // Phase 2c: Now that things are torn down, force JS GC so that things which depend on
      // resources which are about to go away in "profile-before-change" are destroyed first.

      nsCOMPtr<nsIJSRuntimeService> rtsvc
        (do_GetService("@mozilla.org/js/xpc/RuntimeService;1"));
      if (rtsvc)
      {
        JSRuntime *rt = nullptr;
        rtsvc->GetRuntime(&rt);
        if (rt)
          ::JS_GC(rt);
      }

      // Phase 3: Notify observers of a profile change
      obsSvc->NotifyObservers(nullptr, "profile-before-change", kShutdownPersist);
      obsSvc->NotifyObservers(nullptr, "profile-before-change2", kShutdownPersist);
    }
    mProfileNotified = false;
  }
}

#ifdef XP_WIN
static nsresult
GetShellFolderPath(int folder, nsAString& _retval)
{
  wchar_t* buf;
  uint32_t bufLength = _retval.GetMutableData(&buf, MAXPATHLEN + 3);
  NS_ENSURE_TRUE(bufLength >= (MAXPATHLEN + 3), NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = NS_OK;

  LPITEMIDLIST pItemIDList = nullptr;

  if (SUCCEEDED(SHGetSpecialFolderLocation(nullptr, folder, &pItemIDList)) &&
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

  return rv;
}

/**
 * Provides a fallback for getting the path to APPDATA or LOCALAPPDATA by
 * querying the registry when the call to SHGetSpecialFolderLocation or
 * SHGetPathFromIDListW is unable to provide these paths (Bug 513958).
 */
static nsresult
GetRegWindowsAppDataFolder(bool aLocal, nsAString& _retval)
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
  res = RegQueryValueExW(key, (aLocal ? L"Local AppData" : L"AppData"),
                         nullptr, &type, nullptr, &size);
  // The call to RegQueryValueExW must succeed, the type must be REG_SZ, the
  // buffer size must not equal 0, and the buffer size be a multiple of 2.
  if (res != ERROR_SUCCESS || type != REG_SZ || size == 0 || size % 2 != 0) {
    ::RegCloseKey(key);
    _retval.SetLength(0);
    return NS_ERROR_NOT_AVAILABLE;
  }

  // |size| may or may not include room for the terminating null character
  DWORD resultLen = size / 2;

  _retval.SetLength(resultLen);
  nsAString::iterator begin;
  _retval.BeginWriting(begin);
  if (begin.size_forward() != resultLen) {
    ::RegCloseKey(key);
    _retval.SetLength(0);
    return NS_ERROR_NOT_AVAILABLE;
  }

  res = RegQueryValueExW(key, (aLocal ? L"Local AppData" : L"AppData"),
                         nullptr, nullptr, (LPBYTE) begin.get(), &size);
  ::RegCloseKey(key);
  if (res != ERROR_SUCCESS) {
    _retval.SetLength(0);
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (!_retval.CharAt(resultLen - 1)) {
    // It was already null terminated.
    _retval.Truncate(resultLen - 1);
  }

  return NS_OK;
}

static bool
GetCachedHash(HKEY rootKey, const nsAString &regPath, const nsAString &path,
              nsAString &cachedHash)
{
  HKEY baseKey;
  if (RegOpenKeyExW(rootKey, reinterpret_cast<const wchar_t*>(regPath.BeginReading()), 0, KEY_READ, &baseKey) !=
      ERROR_SUCCESS) {
    return false;
  }

  wchar_t cachedHashRaw[512];
  DWORD bufferSize = sizeof(cachedHashRaw);
  LONG result = RegQueryValueExW(baseKey, reinterpret_cast<const wchar_t*>(path.BeginReading()), 0, nullptr,
                                 (LPBYTE)cachedHashRaw, &bufferSize);
  RegCloseKey(baseKey);
  if (result == ERROR_SUCCESS) {
    cachedHash.Assign(cachedHashRaw);
  }
  return ERROR_SUCCESS == result;
}

#endif

nsresult
nsXREDirProvider::GetUpdateRootDir(nsIFile* *aResult)
{
  nsCOMPtr<nsIFile> updRoot;
#if defined(MOZ_WIDGET_GONK)

  nsresult rv = NS_NewNativeLocalFile(nsDependentCString("/data/local"),
                                      true,
                                      getter_AddRefs(updRoot));
  NS_ENSURE_SUCCESS(rv, rv);

#else
  nsCOMPtr<nsIFile> appFile;
  bool per = false;
  nsresult rv = GetFile(XRE_EXECUTABLE_FILE, &per, getter_AddRefs(appFile));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = appFile->GetParent(getter_AddRefs(updRoot));
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef XP_MACOSX
  nsCOMPtr<nsIFile> appRootDirFile;
  nsCOMPtr<nsIFile> localDir;
  nsAutoString appDirPath;
  if (NS_FAILED(appFile->GetParent(getter_AddRefs(appRootDirFile))) ||
      NS_FAILED(appRootDirFile->GetPath(appDirPath)) ||
      NS_FAILED(GetUserDataDirectoryHome(getter_AddRefs(localDir), true))) {
    return NS_ERROR_FAILURE;
  }

  int32_t dotIndex = appDirPath.RFind(".app");
  if (dotIndex == kNotFound) {
    dotIndex = appDirPath.Length();
  }
  appDirPath = Substring(appDirPath, 1, dotIndex - 1);

  bool hasVendor = gAppData->vendor && strlen(gAppData->vendor) != 0;
  if (hasVendor || gAppData->name) {
    if (NS_FAILED(localDir->AppendNative(nsDependentCString(hasVendor ?
                                           gAppData->vendor :
                                           gAppData->name)))) {
      return NS_ERROR_FAILURE;
    }
  } else if (NS_FAILED(localDir->AppendNative(NS_LITERAL_CSTRING("Mozilla")))) {
    return NS_ERROR_FAILURE;
  }

  if (NS_FAILED(localDir->Append(NS_LITERAL_STRING("updates"))) ||
      NS_FAILED(localDir->AppendRelativePath(appDirPath))) {
    return NS_ERROR_FAILURE;
  }

  localDir.forget(aResult);
  return NS_OK;

#elif XP_WIN
  nsAutoString pathHash;
  bool pathHashResult = false;
  bool hasVendor = gAppData->vendor && strlen(gAppData->vendor) != 0;

  nsAutoString appDirPath;
  if (SUCCEEDED(updRoot->GetPath(appDirPath))) {

    // Figure out where we should check for a cached hash value. If the
    // application doesn't have the nsXREAppData vendor value defined check
    // under SOFTWARE\Mozilla.
    wchar_t regPath[1024] = { L'\0' };
    swprintf_s(regPath, mozilla::ArrayLength(regPath), L"SOFTWARE\\%S\\%S\\TaskBarIDs",
               (hasVendor ? gAppData->vendor : "Mozilla"), MOZ_APP_BASENAME);

    // If we pre-computed the hash, grab it from the registry.
    pathHashResult = GetCachedHash(HKEY_LOCAL_MACHINE,
                                   nsDependentString(regPath), appDirPath,
                                   pathHash);
    if (!pathHashResult) {
      pathHashResult = GetCachedHash(HKEY_CURRENT_USER,
                                     nsDependentString(regPath), appDirPath,
                                     pathHash);
    }
  }

  // Get the local app data directory and if a vendor name exists append it.
  // If only a product name exists, append it.  If neither exist fallback to
  // old handling.  We don't use the product name on purpose because we want a
  // shared update directory for different apps run from the same path.
  nsCOMPtr<nsIFile> localDir;
  if (pathHashResult && (hasVendor || gAppData->name) &&
      NS_SUCCEEDED(GetUserDataDirectoryHome(getter_AddRefs(localDir), true)) &&
      NS_SUCCEEDED(localDir->AppendNative(nsDependentCString(hasVendor ?
                                          gAppData->vendor : gAppData->name))) &&
      NS_SUCCEEDED(localDir->Append(NS_LITERAL_STRING("updates"))) &&
      NS_SUCCEEDED(localDir->Append(pathHash))) {
    localDir.forget(aResult);
    return NS_OK;
  }

  nsAutoString appPath;
  rv = updRoot->GetPath(appPath);
  NS_ENSURE_SUCCESS(rv, rv);

  // AppDir may be a short path. Convert to long path to make sure
  // the consistency of the update folder location
  nsString longPath;
  wchar_t* buf;

  uint32_t bufLength = longPath.GetMutableData(&buf, MAXPATHLEN);
  NS_ENSURE_TRUE(bufLength >= MAXPATHLEN, NS_ERROR_OUT_OF_MEMORY);

  DWORD len = GetLongPathNameW(appPath.get(), buf, bufLength);

  // Failing GetLongPathName() is not fatal.
  if (len <= 0 || len >= bufLength)
    longPath.Assign(appPath);
  else
    longPath.SetLength(len);

  // Use <UserLocalDataDir>\updates\<relative path to app dir from
  // Program Files> if app dir is under Program Files to avoid the
  // folder virtualization mess on Windows Vista
  nsAutoString programFiles;
  rv = GetShellFolderPath(CSIDL_PROGRAM_FILES, programFiles);
  NS_ENSURE_SUCCESS(rv, rv);

  programFiles.Append('\\');
  uint32_t programFilesLen = programFiles.Length();

  nsAutoString programName;
  if (_wcsnicmp(programFiles.get(), longPath.get(), programFilesLen) == 0) {
    programName = Substring(longPath, programFilesLen);
  } else {
    // We need the update root directory to live outside of the installation
    // directory, because otherwise the updater writing the log file can cause
    // the directory to be locked, which prevents it from being replaced after
    // background updates.
    programName.AssignASCII(MOZ_APP_NAME);
  }

  rv = GetUserLocalDataDirectory(getter_AddRefs(updRoot));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = updRoot->AppendRelativePath(programName);
  NS_ENSURE_SUCCESS(rv, rv);

#endif // XP_WIN
#endif
  updRoot.forget(aResult);
  return NS_OK;
}

nsresult
nsXREDirProvider::GetProfileStartupDir(nsIFile* *aResult)
{
  if (mProfileDir)
    return mProfileDir->Clone(aResult);

  if (mAppProvider) {
    nsCOMPtr<nsIFile> needsclone;
    bool dummy;
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
    bool dummy;
    nsresult rv = mAppProvider->GetFile(NS_APP_USER_PROFILE_50_DIR,
                                        &dummy,
                                        getter_AddRefs(needsclone));
    if (NS_SUCCEEDED(rv))
      return needsclone->Clone(aResult);
  }

  return NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, aResult);
}

nsresult
nsXREDirProvider::GetUserDataDirectoryHome(nsIFile** aFile, bool aLocal)
{
  // Copied from nsAppFileLocationProvider (more or less)
  nsresult rv;
  nsCOMPtr<nsIFile> localDir;

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

  rv = NS_NewNativeLocalFile(EmptyCString(), true, getter_AddRefs(localDir));
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
    if (NS_FAILED(rv))
      rv = GetRegWindowsAppDataFolder(aLocal, path);
  }
  if (!aLocal || NS_FAILED(rv)) {
    rv = GetShellFolderPath(CSIDL_APPDATA, path);
    if (NS_FAILED(rv)) {
      if (!aLocal)
        rv = GetRegWindowsAppDataFolder(aLocal, path);
    }
  }
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_NewLocalFile(path, true, getter_AddRefs(localDir));
#elif defined(MOZ_WIDGET_GONK)
  rv = NS_NewNativeLocalFile(NS_LITERAL_CSTRING("/data/b2g"), true,
                             getter_AddRefs(localDir));
#elif defined(XP_UNIX)
  const char* homeDir = getenv("HOME");
  if (!homeDir || !*homeDir)
    return NS_ERROR_FAILURE;

#ifdef ANDROID /* We want (ProfD == ProfLD) on Android. */
  aLocal = false;
#endif

  if (aLocal) {
    // If $XDG_CACHE_HOME is defined use it, otherwise use $HOME/.cache.
    const char* cacheHome = getenv("XDG_CACHE_HOME");
    if (cacheHome && *cacheHome) {
      rv = NS_NewNativeLocalFile(nsDependentCString(cacheHome), true,
                                 getter_AddRefs(localDir));
    } else {
      rv = NS_NewNativeLocalFile(nsDependentCString(homeDir), true,
                                 getter_AddRefs(localDir));
      if (NS_SUCCEEDED(rv))
        rv = localDir->AppendNative(NS_LITERAL_CSTRING(".cache"));
    }
  } else {
    rv = NS_NewNativeLocalFile(nsDependentCString(homeDir), true,
                               getter_AddRefs(localDir));
  }
#else
#error "Don't know how to get product dir on your platform"
#endif

  NS_IF_ADDREF(*aFile = localDir);
  return rv;
}

nsresult
nsXREDirProvider::GetSysUserExtensionsDirectory(nsIFile** aFile)
{
  nsCOMPtr<nsIFile> localDir;
  nsresult rv = GetUserDataDirectoryHome(getter_AddRefs(localDir), false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AppendSysUserExtensionPath(localDir);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = EnsureDirectoryExists(localDir);
  NS_ENSURE_SUCCESS(rv, rv);

  localDir.forget(aFile);
  return NS_OK;
}

#if defined(XP_UNIX) || defined(XP_MACOSX)
nsresult
nsXREDirProvider::GetSystemExtensionsDirectory(nsIFile** aFile)
{
  nsresult rv;
  nsCOMPtr<nsIFile> localDir;
#if defined(XP_MACOSX)
  FSRef fsRef;
  OSErr err = ::FSFindFolder(kOnSystemDisk, kApplicationSupportFolderType, kCreateFolder, &fsRef);
  NS_ENSURE_FALSE(err, NS_ERROR_FAILURE);

  rv = NS_NewNativeLocalFile(EmptyCString(), true, getter_AddRefs(localDir));
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
#elif defined(__OpenBSD__) || defined(__FreeBSD__)
    "/usr/local/lib/mozilla/extensions";
#else
    "/usr/lib/mozilla/extensions";
#endif

  rv = NS_NewNativeLocalFile(nsDependentCString(sysSExtDir), false,
                             getter_AddRefs(localDir));
  NS_ENSURE_SUCCESS(rv, rv);
#endif

  localDir.forget(aFile);
  return NS_OK;
}
#endif

nsresult
nsXREDirProvider::GetUserDataDirectory(nsIFile** aFile, bool aLocal,
                                       const nsACString* aProfileName,
                                       const nsACString* aAppName,
                                       const nsACString* aVendorName)
{
  nsCOMPtr<nsIFile> localDir;
  nsresult rv = GetUserDataDirectoryHome(getter_AddRefs(localDir), aLocal);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AppendProfilePath(localDir, aProfileName, aAppName, aVendorName, aLocal);
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG_jungshik
  nsAutoCString cwd;
  localDir->GetNativePath(cwd);
  printf("nsXREDirProvider::GetUserDataDirectory: %s\n", cwd.get());
#endif
  rv = EnsureDirectoryExists(localDir);
  NS_ENSURE_SUCCESS(rv, rv);

  localDir.forget(aFile);
  return NS_OK;
}

nsresult
nsXREDirProvider::EnsureDirectoryExists(nsIFile* aDirectory)
{
  bool exists;
  nsresult rv = aDirectory->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);
#ifdef DEBUG_jungshik
  if (!exists) {
    nsAutoCString cwd;
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
  bool exists;

  rv = aFile->Exists(&exists);
  if (NS_FAILED(rv) || exists) return;

  nsAutoCString leafName;
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

  defaultsDir.forget(aResult);
  return NS_OK;
}

nsresult
nsXREDirProvider::AppendSysUserExtensionPath(nsIFile* aFile)
{
  NS_ASSERTION(aFile, "Null pointer!");

  nsresult rv;

#if defined (XP_MACOSX) || defined(XP_WIN)

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
nsXREDirProvider::AppendProfilePath(nsIFile* aFile,
                                    const nsACString* aProfileName,
                                    const nsACString* aAppName,
                                    const nsACString* aVendorName,
                                    bool aLocal)
{
  NS_ASSERTION(aFile, "Null pointer!");
  
  if (!gAppData) {
    return NS_ERROR_FAILURE;
  }

  nsAutoCString profile;
  nsAutoCString appName;
  nsAutoCString vendor;
  if (aProfileName && !aProfileName->IsEmpty()) {
    profile = *aProfileName;
  } else if (aAppName) {
    appName = *aAppName;
    if (aVendorName) {
      vendor = *aVendorName;
    }
  } else if (gAppData->profile) {
    profile = gAppData->profile;
  } else {
    appName = gAppData->name;
    vendor = gAppData->vendor;
  }

  nsresult rv;

#if defined (XP_MACOSX)
  if (!profile.IsEmpty()) {
    rv = AppendProfileString(aFile, profile.get());
  }
  else {
    // Note that MacOS ignores the vendor when creating the profile hierarchy -
    // all application preferences directories live alongside one another in
    // ~/Library/Application Support/
    rv = aFile->AppendNative(appName);
  }
  NS_ENSURE_SUCCESS(rv, rv);

#elif defined(XP_WIN)
  if (!profile.IsEmpty()) {
    rv = AppendProfileString(aFile, profile.get());
  }
  else {
    if (!vendor.IsEmpty()) {
      rv = aFile->AppendNative(vendor);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    rv = aFile->AppendNative(appName);
  }
  NS_ENSURE_SUCCESS(rv, rv);

#elif defined(ANDROID)
  // The directory used for storing profiles
  // The parent of this directory is set in GetUserDataDirectoryHome
  // XXX: handle gAppData->profile properly
  // XXXsmaug ...and the rest of the profile creation!
  MOZ_ASSERT(!aAppName,
             "Profile creation for external applications is not implemented!");
  rv = aFile->AppendNative(nsDependentCString("mozilla"));
  NS_ENSURE_SUCCESS(rv, rv);
#elif defined(XP_UNIX)
  nsAutoCString folder;
  // Make it hidden (by starting with "."), except when local (the
  // profile is already under ~/.cache or XDG_CACHE_HOME).
  if (!aLocal)
    folder.Assign('.');

  if (!profile.IsEmpty()) {
    // Skip any leading path characters
    const char* profileStart = profile.get();
    while (*profileStart == '/' || *profileStart == '\\')
      profileStart++;

    // On the off chance that someone wanted their folder to be hidden don't
    // let it become ".."
    if (*profileStart == '.' && !aLocal)
      profileStart++;

    folder.Append(profileStart);
    ToLowerCase(folder);

    rv = AppendProfileString(aFile, folder.BeginReading());
  }
  else {
    if (!vendor.IsEmpty()) {
      folder.Append(vendor);
      ToLowerCase(folder);

      rv = aFile->AppendNative(folder);
      NS_ENSURE_SUCCESS(rv, rv);

      folder.Truncate();
    }

    folder.Append(appName);
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

  nsAutoCString pathDup(aPath);

  char* path = pathDup.BeginWriting();

  nsresult rv;
  char* subdir;
  while ((subdir = NS_strtok("/\\", &path))) {
    rv = aFile->AppendNative(nsDependentCString(subdir));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}
