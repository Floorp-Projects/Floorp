/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAppRunner.h"
#include "nsXREDirProvider.h"
#ifndef ANDROID
#  include "commonupdatedir.h"
#endif

#include "jsapi.h"
#include "xpcpublic.h"

#include "nsIAppStartup.h"
#include "nsIDirectoryEnumerator.h"
#include "nsIFile.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsISimpleEnumerator.h"
#include "nsIToolkitChromeRegistry.h"
#include "nsIToolkitProfileService.h"
#include "nsIXULRuntime.h"
#include "commonupdatedir.h"

#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsXULAppAPI.h"
#include "nsCategoryManagerUtils.h"

#include "nsDependentString.h"
#include "nsCOMArray.h"
#include "nsArrayEnumerator.h"
#include "nsEnumeratorUtils.h"
#include "nsReadableUtils.h"

#include "SpecialSystemDirectory.h"

#include "mozilla/dom/ScriptSettings.h"

#include "mozilla/AutoRestore.h"
#include "mozilla/Components.h"
#include "mozilla/Services.h"
#include "mozilla/Omnijar.h"
#include "mozilla/Preferences.h"
#include "mozilla/Telemetry.h"

#include <stdlib.h>

#ifdef XP_WIN
#  include <windows.h>
#  include <shlobj.h>
#endif
#ifdef XP_MACOSX
#  include "nsILocalFileMac.h"
// for chflags()
#  include <sys/stat.h>
#  include <unistd.h>
#endif
#ifdef XP_UNIX
#  include <ctype.h>
#endif
#ifdef XP_IOS
#  include "UIKitDirProvider.h"
#endif

#if defined(MOZ_SANDBOX)
#  include "mozilla/SandboxSettings.h"
#  include "nsIUUIDGenerator.h"
#  include "mozilla/Unused.h"
#  if defined(XP_WIN)
#    include "sandboxBroker.h"
#    include "WinUtils.h"
#  endif
#endif

#if defined(XP_MACOSX)
#  define APP_REGISTRY_NAME "Application Registry"
#elif defined(XP_WIN)
#  define APP_REGISTRY_NAME "registry.dat"
#else
#  define APP_REGISTRY_NAME "appreg"
#endif

#define PREF_OVERRIDE_DIRNAME "preferences"

#if defined(MOZ_SANDBOX)
static already_AddRefed<nsIFile> GetProcessSandboxTempDir(
    GeckoProcessType type);
static nsresult DeleteDirIfExists(nsIFile* dir);
static bool IsContentSandboxDisabled();
static const char* GetProcessTempBaseDirKey();
static already_AddRefed<nsIFile> CreateProcessSandboxTempDir(
    GeckoProcessType procType);
#endif

nsXREDirProvider* gDirServiceProvider = nullptr;
nsIFile* gDataDirHomeLocal = nullptr;
nsIFile* gDataDirHome = nullptr;

// These are required to allow nsXREDirProvider to be usable in xpcshell tests.
// where gAppData is null.
#if defined(XP_MACOSX) || defined(XP_WIN)
static const char* GetAppName() {
  if (gAppData) {
    return gAppData->name;
  }
  return nullptr;
}
#endif

static const char* GetAppVendor() {
  if (gAppData) {
    return gAppData->vendor;
  }
  return nullptr;
}

nsXREDirProvider::nsXREDirProvider() : mProfileNotified(false) {
  gDirServiceProvider = this;
}

nsXREDirProvider::~nsXREDirProvider() {
  gDirServiceProvider = nullptr;
  gDataDirHomeLocal = nullptr;
  gDataDirHome = nullptr;
}

already_AddRefed<nsXREDirProvider> nsXREDirProvider::GetSingleton() {
  if (!gDirServiceProvider) {
    new nsXREDirProvider();  // This sets gDirServiceProvider
  }
  return do_AddRef(gDirServiceProvider);
}

nsresult nsXREDirProvider::Initialize(
    nsIFile* aXULAppDir, nsIFile* aGREDir,
    nsIDirectoryServiceProvider* aAppProvider) {
  NS_ENSURE_ARG(aXULAppDir);
  NS_ENSURE_ARG(aGREDir);

  mAppProvider = aAppProvider;
  mXULAppDir = aXULAppDir;
  mGREDir = aGREDir;
#if defined(XP_WIN) && defined(MOZ_SANDBOX)
  // The GRE directory can be used in sandbox rules, so we need to make sure
  // it doesn't contain any junction points or symlinks or the sandbox will
  // reject those rules.
  if (!mozilla::widget::WinUtils::ResolveJunctionPointsAndSymLinks(mGREDir)) {
    NS_WARNING("Failed to resolve GRE Dir.");
  }
  // If the mXULAppDir is different it lives below the mGREDir. To avoid
  // confusion resolve that as well even though we don't need it for sandbox
  // rules. Some tests rely on this for example.
  if (!mozilla::widget::WinUtils::ResolveJunctionPointsAndSymLinks(
          mXULAppDir)) {
    NS_WARNING("Failed to resolve XUL App Dir.");
  }
#endif
  mGREDir->Clone(getter_AddRefs(mGREBinDir));
#ifdef XP_MACOSX
  mGREBinDir->SetNativeLeafName(NS_LITERAL_CSTRING("MacOS"));
#endif

  if (!mProfileDir) {
    nsCOMPtr<nsIDirectoryServiceProvider> app(mAppProvider);
    if (app) {
      bool per = false;
      app->GetFile(NS_APP_USER_PROFILE_50_DIR, &per,
                   getter_AddRefs(mProfileDir));
      NS_ASSERTION(per, "NS_APP_USER_PROFILE_50_DIR must be persistent!");
      NS_ASSERTION(
          mProfileDir,
          "NS_APP_USER_PROFILE_50_DIR not defined! This shouldn't happen!");
    }
  }

  return NS_OK;
}

nsresult nsXREDirProvider::SetProfile(nsIFile* aDir, nsIFile* aLocalDir) {
  NS_ASSERTION(aDir && aLocalDir, "We don't support no-profile apps yet!");

  nsresult rv;

  rv = EnsureDirectoryExists(aDir);
  if (NS_FAILED(rv)) return rv;

  rv = EnsureDirectoryExists(aLocalDir);
  if (NS_FAILED(rv)) return rv;

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
#if defined(XP_WIN) && defined(MOZ_SANDBOX)
  // The profile directory can be used in sandbox rules, so we need to make sure
  // it doesn't contain any junction points or symlinks or the sandbox will
  // reject those rules.
  if (!mozilla::widget::WinUtils::ResolveJunctionPointsAndSymLinks(
          mProfileDir)) {
    NS_WARNING("Failed to resolve Profile Dir.");
  }
#endif
  return NS_OK;
}

NS_IMPL_QUERY_INTERFACE(nsXREDirProvider, nsIDirectoryServiceProvider,
                        nsIDirectoryServiceProvider2, nsIXREDirProvider,
                        nsIProfileStartup)

NS_IMETHODIMP_(MozExternalRefCountType)
nsXREDirProvider::AddRef() { return 1; }

NS_IMETHODIMP_(MozExternalRefCountType)
nsXREDirProvider::Release() { return 0; }

nsresult nsXREDirProvider::GetUserProfilesRootDir(nsIFile** aResult) {
  nsCOMPtr<nsIFile> file;
  nsresult rv = GetUserDataDirectory(getter_AddRefs(file), false);

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

nsresult nsXREDirProvider::GetUserProfilesLocalDir(nsIFile** aResult) {
  nsCOMPtr<nsIFile> file;
  nsresult rv = GetUserDataDirectory(getter_AddRefs(file), true);

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

#if defined(XP_UNIX) || defined(XP_MACOSX)
/**
 * Get the directory that is the parent of the system-wide directories
 * for extensions and native manifests.
 *
 * On OSX this is /Library/Application Support/Mozilla
 * On Linux this is /usr/{lib,lib64}/mozilla
 *   (for 32- and 64-bit systems respsectively)
 */
static nsresult GetSystemParentDirectory(nsIFile** aFile) {
  nsresult rv;
  nsCOMPtr<nsIFile> localDir;
#  if defined(XP_MACOSX)
  rv = GetOSXFolderType(kOnSystemDisk, kApplicationSupportFolderType,
                        getter_AddRefs(localDir));
  if (NS_SUCCEEDED(rv)) {
    rv = localDir->AppendNative(NS_LITERAL_CSTRING("Mozilla"));
  }
#  else
  NS_NAMED_LITERAL_CSTRING(dirname,
#    ifdef HAVE_USR_LIB64_DIR
                           "/usr/lib64/mozilla"
#    elif defined(__OpenBSD__) || defined(__FreeBSD__)
                           "/usr/local/lib/mozilla"
#    else
                           "/usr/lib/mozilla"
#    endif
  );
  rv = NS_NewNativeLocalFile(dirname, false, getter_AddRefs(localDir));
#  endif

  if (NS_SUCCEEDED(rv)) {
    localDir.forget(aFile);
  }
  return rv;
}
#endif

NS_IMETHODIMP
nsXREDirProvider::GetFile(const char* aProperty, bool* aPersistent,
                          nsIFile** aFile) {
  nsresult rv;

  bool gettingProfile = false;

  if (!strcmp(aProperty, NS_APP_USER_PROFILE_LOCAL_50_DIR)) {
    // If XRE_NotifyProfile hasn't been called, don't fall through to
    // mAppProvider on the profile keys.
    if (!mProfileNotified) return NS_ERROR_FAILURE;

    if (mProfileLocalDir) return mProfileLocalDir->Clone(aFile);

    if (mAppProvider)
      return mAppProvider->GetFile(aProperty, aPersistent, aFile);

    // This falls through to the case below
    gettingProfile = true;
  }
  if (!strcmp(aProperty, NS_APP_USER_PROFILE_50_DIR) || gettingProfile) {
    if (!mProfileNotified) return NS_ERROR_FAILURE;

    if (mProfileDir) return mProfileDir->Clone(aFile);

    if (mAppProvider)
      return mAppProvider->GetFile(aProperty, aPersistent, aFile);

    // If we don't succeed here, bail early so that we aren't reentrant
    // through the "GetProfileDir" call below.
    return NS_ERROR_FAILURE;
  }

  if (mAppProvider) {
    rv = mAppProvider->GetFile(aProperty, aPersistent, aFile);
    if (NS_SUCCEEDED(rv) && *aFile) return rv;
  }

  *aPersistent = true;

  if (!strcmp(aProperty, NS_GRE_DIR)) {
    return mGREDir->Clone(aFile);
  } else if (!strcmp(aProperty, NS_GRE_BIN_DIR)) {
    return mGREBinDir->Clone(aFile);
  } else if (!strcmp(aProperty, NS_OS_CURRENT_PROCESS_DIR) ||
             !strcmp(aProperty, NS_APP_INSTALL_CLEANUP_DIR)) {
    return GetAppDir()->Clone(aFile);
  }

  rv = NS_ERROR_FAILURE;
  nsCOMPtr<nsIFile> file;

  if (!strcmp(aProperty, NS_APP_PREF_DEFAULTS_50_DIR)) {
    // return the GRE default prefs directory here, and the app default prefs
    // directory (if applicable) in NS_APP_PREFS_DEFAULTS_DIR_LIST.
    rv = mGREDir->Clone(getter_AddRefs(file));
    if (NS_SUCCEEDED(rv)) {
      rv = file->AppendNative(NS_LITERAL_CSTRING("defaults"));
      if (NS_SUCCEEDED(rv)) rv = file->AppendNative(NS_LITERAL_CSTRING("pref"));
    }
  } else if (!strcmp(aProperty, NS_APP_APPLICATION_REGISTRY_DIR) ||
             !strcmp(aProperty, XRE_USER_APP_DATA_DIR)) {
    rv = GetUserAppDataDirectory(getter_AddRefs(file));
  }
#if defined(XP_UNIX) || defined(XP_MACOSX)
  else if (!strcmp(aProperty, XRE_SYS_NATIVE_MANIFESTS)) {
    nsCOMPtr<nsIFile> localDir;

    rv = ::GetSystemParentDirectory(getter_AddRefs(localDir));
    if (NS_SUCCEEDED(rv)) {
      localDir.swap(file);
    }
  } else if (!strcmp(aProperty, XRE_USER_NATIVE_MANIFESTS)) {
    nsCOMPtr<nsIFile> localDir;
    rv = GetUserDataDirectoryHome(getter_AddRefs(localDir), false);
    if (NS_SUCCEEDED(rv)) {
#  if defined(XP_MACOSX)
      rv = localDir->AppendNative(NS_LITERAL_CSTRING("Mozilla"));
#  else
      rv = localDir->AppendNative(NS_LITERAL_CSTRING(".mozilla"));
#  endif
    }
    if (NS_SUCCEEDED(rv)) {
      localDir.swap(file);
    }
  }
#endif
  else if (!strcmp(aProperty, XRE_UPDATE_ROOT_DIR)) {
    rv = GetUpdateRootDir(getter_AddRefs(file));
  } else if (!strcmp(aProperty, XRE_OLD_UPDATE_ROOT_DIR)) {
    rv = GetUpdateRootDir(getter_AddRefs(file), true);
  } else if (!strcmp(aProperty, NS_APP_APPLICATION_REGISTRY_FILE)) {
    rv = GetUserAppDataDirectory(getter_AddRefs(file));
    if (NS_SUCCEEDED(rv))
      rv = file->AppendNative(NS_LITERAL_CSTRING(APP_REGISTRY_NAME));
  } else if (!strcmp(aProperty, NS_APP_USER_PROFILES_ROOT_DIR)) {
    rv = GetUserProfilesRootDir(getter_AddRefs(file));
  } else if (!strcmp(aProperty, NS_APP_USER_PROFILES_LOCAL_ROOT_DIR)) {
    rv = GetUserProfilesLocalDir(getter_AddRefs(file));
  } else if (!strcmp(aProperty, XRE_EXECUTABLE_FILE)) {
    nsCOMPtr<nsIFile> lf;
    rv = XRE_GetBinaryPath(getter_AddRefs(lf));
    if (NS_SUCCEEDED(rv)) file = lf;
  }

  else if (!strcmp(aProperty, NS_APP_PROFILE_DIR_STARTUP) && mProfileDir) {
    return mProfileDir->Clone(aFile);
  } else if (!strcmp(aProperty, NS_APP_PROFILE_LOCAL_DIR_STARTUP)) {
    if (mProfileLocalDir) return mProfileLocalDir->Clone(aFile);

    if (mProfileDir) return mProfileDir->Clone(aFile);

    if (mAppProvider)
      return mAppProvider->GetFile(NS_APP_PROFILE_DIR_STARTUP, aPersistent,
                                   aFile);
  }
#if defined(XP_UNIX) || defined(XP_MACOSX)
  else if (!strcmp(aProperty, XRE_SYS_LOCAL_EXTENSION_PARENT_DIR)) {
#  ifdef ENABLE_SYSTEM_EXTENSION_DIRS
    return GetSystemExtensionsDirectory(aFile);
#  else
    return NS_ERROR_FAILURE;
#  endif
  }
#endif
#if defined(XP_UNIX) && !defined(XP_MACOSX)
  else if (!strcmp(aProperty, XRE_SYS_SHARE_EXTENSION_PARENT_DIR)) {
#  ifdef ENABLE_SYSTEM_EXTENSION_DIRS
#    if defined(__OpenBSD__) || defined(__FreeBSD__)
    static const char* const sysLExtDir = "/usr/local/share/mozilla/extensions";
#    else
    static const char* const sysLExtDir = "/usr/share/mozilla/extensions";
#    endif
    return NS_NewNativeLocalFile(nsDependentCString(sysLExtDir), false, aFile);
#  else
    return NS_ERROR_FAILURE;
#  endif
  }
#endif
  else if (!strcmp(aProperty, XRE_USER_SYS_EXTENSION_DIR)) {
#ifdef ENABLE_SYSTEM_EXTENSION_DIRS
    return GetSysUserExtensionsDirectory(aFile);
#else
    return NS_ERROR_FAILURE;
#endif
  } else if (!strcmp(aProperty, XRE_USER_SYS_EXTENSION_DEV_DIR)) {
    return GetSysUserExtensionsDevDirectory(aFile);
  } else if (!strcmp(aProperty, XRE_APP_DISTRIBUTION_DIR)) {
    bool persistent = false;
    rv = GetFile(NS_GRE_DIR, &persistent, getter_AddRefs(file));
    if (NS_SUCCEEDED(rv))
      rv = file->AppendNative(NS_LITERAL_CSTRING("distribution"));
  } else if (!strcmp(aProperty, XRE_APP_FEATURES_DIR)) {
    rv = GetAppDir()->Clone(getter_AddRefs(file));
    if (NS_SUCCEEDED(rv))
      rv = file->AppendNative(NS_LITERAL_CSTRING("features"));
  } else if (!strcmp(aProperty, XRE_ADDON_APP_DIR)) {
    nsCOMPtr<nsIDirectoryServiceProvider> dirsvc(
        do_GetService("@mozilla.org/file/directory_service;1", &rv));
    if (NS_FAILED(rv)) return rv;
    bool unused;
    rv = dirsvc->GetFile("XCurProcD", &unused, getter_AddRefs(file));
  }
#if defined(MOZ_SANDBOX)
  else if (!strcmp(aProperty, NS_APP_CONTENT_PROCESS_TEMP_DIR)) {
    if (!mContentTempDir && NS_FAILED((rv = LoadContentProcessTempDir()))) {
      return rv;
    }
    rv = mContentTempDir->Clone(getter_AddRefs(file));
  }
#endif  // defined(MOZ_SANDBOX)
#if defined(MOZ_SANDBOX)
  else if (0 == strcmp(aProperty, NS_APP_PLUGIN_PROCESS_TEMP_DIR)) {
    if (!mPluginTempDir && NS_FAILED((rv = LoadPluginProcessTempDir()))) {
      return rv;
    }
    rv = mPluginTempDir->Clone(getter_AddRefs(file));
  }
#endif  // defined(MOZ_SANDBOX)
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
    } else if (!strcmp(aProperty, NS_APP_PREFS_50_FILE)) {
      rv = file->AppendNative(NS_LITERAL_CSTRING("prefs.js"));
    } else if (!strcmp(aProperty, NS_APP_PREFS_OVERRIDE_DIR)) {
      rv = mProfileDir->Clone(getter_AddRefs(file));
      nsresult tmp =
          file->AppendNative(NS_LITERAL_CSTRING(PREF_OVERRIDE_DIRNAME));
      if (NS_FAILED(tmp)) {
        rv = tmp;
      }
      tmp = EnsureDirectoryExists(file);
      if (NS_FAILED(tmp)) {
        rv = tmp;
      }
    }
  }
  if (NS_FAILED(rv) || !file) return NS_ERROR_FAILURE;

  if (ensureFilePermissions) {
    bool fileToEnsureExists;
    bool isWritable;
    if (NS_SUCCEEDED(file->Exists(&fileToEnsureExists)) && fileToEnsureExists &&
        NS_SUCCEEDED(file->IsWritable(&isWritable)) && !isWritable) {
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

static void LoadDirIntoArray(nsIFile* dir, const char* const* aAppendList,
                             nsCOMArray<nsIFile>& aDirectories) {
  if (!dir) return;

  nsCOMPtr<nsIFile> subdir;
  dir->Clone(getter_AddRefs(subdir));
  if (!subdir) return;

  for (const char* const* a = aAppendList; *a; ++a) {
    subdir->AppendNative(nsDependentCString(*a));
  }

  bool exists;
  if (NS_SUCCEEDED(subdir->Exists(&exists)) && exists) {
    aDirectories.AppendObject(subdir);
  }
}

static void LoadDirsIntoArray(const nsCOMArray<nsIFile>& aSourceDirs,
                              const char* const* aAppendList,
                              nsCOMArray<nsIFile>& aDirectories) {
  nsCOMPtr<nsIFile> appended;
  bool exists;

  for (int32_t i = 0; i < aSourceDirs.Count(); ++i) {
    aSourceDirs[i]->Clone(getter_AddRefs(appended));
    if (!appended) continue;

    nsAutoCString leaf;
    appended->GetNativeLeafName(leaf);
    if (!Substring(leaf, leaf.Length() - 4).EqualsLiteral(".xpi")) {
      LoadDirIntoArray(appended, aAppendList, aDirectories);
    } else if (NS_SUCCEEDED(appended->Exists(&exists)) && exists)
      aDirectories.AppendObject(appended);
  }
}

NS_IMETHODIMP
nsXREDirProvider::GetFiles(const char* aProperty,
                           nsISimpleEnumerator** aResult) {
  nsresult rv;

  nsCOMPtr<nsISimpleEnumerator> appEnum;
  nsCOMPtr<nsIDirectoryServiceProvider2> appP2(do_QueryInterface(mAppProvider));
  if (appP2) {
    rv = appP2->GetFiles(aProperty, getter_AddRefs(appEnum));
    if (NS_FAILED(rv)) {
      appEnum = nullptr;
    } else if (rv != NS_SUCCESS_AGGREGATE_RESULT) {
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
  if (NS_FAILED(rv)) return rv;

  return NS_SUCCESS_AGGREGATE_RESULT;
}

#if defined(MOZ_SANDBOX)

static const char* GetProcessTempBaseDirKey() {
#  if defined(XP_WIN)
  return NS_WIN_LOW_INTEGRITY_TEMP_BASE;
#  else
  return NS_OS_TEMP_DIR;
#  endif
}

#  if defined(MOZ_SANDBOX)
//
// Sets mContentTempDir so that it refers to the appropriate temp dir.
// If the sandbox is enabled, NS_APP_CONTENT_PROCESS_TEMP_DIR, otherwise
// NS_OS_TEMP_DIR is used.
//
nsresult nsXREDirProvider::LoadContentProcessTempDir() {
  // The parent is responsible for creating the sandbox temp dir.
  if (XRE_IsParentProcess()) {
    mContentProcessSandboxTempDir =
        CreateProcessSandboxTempDir(GeckoProcessType_Content);
    mContentTempDir = mContentProcessSandboxTempDir;
  } else {
    mContentTempDir = !IsContentSandboxDisabled()
                          ? GetProcessSandboxTempDir(GeckoProcessType_Content)
                          : nullptr;
  }

  if (!mContentTempDir) {
    nsresult rv =
        NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(mContentTempDir));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

#    if defined(XP_WIN)
  // The content temp dir can be used in sandbox rules, so we need to make sure
  // it doesn't contain any junction points or symlinks or the sandbox will
  // reject those rules.
  if (!mozilla::widget::WinUtils::ResolveJunctionPointsAndSymLinks(
          mContentTempDir)) {
    NS_WARNING("Failed to resolve Content Temp Dir.");
  }
#    endif

  return NS_OK;
}
#  endif

//
// Sets mPluginTempDir so that it refers to the appropriate temp dir.
// If NS_APP_PLUGIN_PROCESS_TEMP_DIR fails for any reason, NS_OS_TEMP_DIR
// is used.
//
nsresult nsXREDirProvider::LoadPluginProcessTempDir() {
  // The parent is responsible for creating the sandbox temp dir.
  if (XRE_IsParentProcess()) {
    mPluginProcessSandboxTempDir =
        CreateProcessSandboxTempDir(GeckoProcessType_Plugin);
    mPluginTempDir = mPluginProcessSandboxTempDir;
  } else {
    MOZ_ASSERT(XRE_IsPluginProcess());
    mPluginTempDir = GetProcessSandboxTempDir(GeckoProcessType_Plugin);
  }

  if (!mPluginTempDir) {
    nsresult rv =
        NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(mPluginTempDir));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

#  if defined(XP_WIN)
  // The temp dir is used in sandbox rules, so we need to make sure
  // it doesn't contain any junction points or symlinks or the sandbox will
  // reject those rules.
  if (!mozilla::widget::WinUtils::ResolveJunctionPointsAndSymLinks(
          mPluginTempDir)) {
    NS_WARNING("Failed to resolve plugin temp dir.");
  }
#  endif

  return NS_OK;
}

static bool IsContentSandboxDisabled() {
  return !mozilla::BrowserTabsRemoteAutostart() ||
         (!mozilla::IsContentSandboxEnabled());
}

//
// If a process sandbox temp dir is to be used, returns an nsIFile
// for the directory. Returns null if an error occurs.
//
static already_AddRefed<nsIFile> GetProcessSandboxTempDir(
    GeckoProcessType type) {
  nsCOMPtr<nsIFile> localFile;

  nsresult rv = NS_GetSpecialDirectory(GetProcessTempBaseDirKey(),
                                       getter_AddRefs(localFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  MOZ_ASSERT((type == GeckoProcessType_Content) ||
             (type == GeckoProcessType_Plugin));

  const char* prefKey = (type == GeckoProcessType_Content)
                            ? "security.sandbox.content.tempDirSuffix"
                            : "security.sandbox.plugin.tempDirSuffix";

  nsAutoString tempDirSuffix;
  rv = mozilla::Preferences::GetString(prefKey, tempDirSuffix);
  if (NS_WARN_IF(NS_FAILED(rv)) || tempDirSuffix.IsEmpty()) {
    return nullptr;
  }

  rv = localFile->Append(NS_LITERAL_STRING("Temp-") + tempDirSuffix);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  return localFile.forget();
}

//
// Create a temporary directory for use from sandboxed processes.
// Only called in the parent. The path is derived from a UUID stored in a
// pref which is available to content and plugin processes. Returns null
// if the content sandbox is disabled or if an error occurs.
//
static already_AddRefed<nsIFile> CreateProcessSandboxTempDir(
    GeckoProcessType procType) {
#  if defined(MOZ_SANDBOX)
  if ((procType == GeckoProcessType_Content) && IsContentSandboxDisabled()) {
    return nullptr;
  }
#  endif

  MOZ_ASSERT((procType == GeckoProcessType_Content) ||
             (procType == GeckoProcessType_Plugin));

  // Get (and create if blank) temp directory suffix pref.
  const char* pref = (procType == GeckoProcessType_Content)
                         ? "security.sandbox.content.tempDirSuffix"
                         : "security.sandbox.plugin.tempDirSuffix";

  nsresult rv;
  nsAutoString tempDirSuffix;
  mozilla::Preferences::GetString(pref, tempDirSuffix);
  if (tempDirSuffix.IsEmpty()) {
    nsCOMPtr<nsIUUIDGenerator> uuidgen =
        do_GetService("@mozilla.org/uuid-generator;1", &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }

    nsID uuid;
    rv = uuidgen->GenerateUUIDInPlace(&uuid);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }

    char uuidChars[NSID_LENGTH];
    uuid.ToProvidedString(uuidChars);
    tempDirSuffix.AssignASCII(uuidChars, NSID_LENGTH);
#  ifdef XP_UNIX
    // Braces in a path are somewhat annoying to deal with
    // and pretty alien on Unix
    tempDirSuffix.StripChars(u"{}");
#  endif

    // Save the pref
    rv = mozilla::Preferences::SetString(pref, tempDirSuffix);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      // If we fail to save the pref we don't want to create the temp dir,
      // because we won't be able to clean it up later.
      return nullptr;
    }

    nsCOMPtr<nsIPrefService> prefsvc = mozilla::Preferences::GetService();
    if (!prefsvc || NS_FAILED((rv = prefsvc->SavePrefFile(nullptr)))) {
      // Again, if we fail to save the pref file we might not be able to clean
      // up the temp directory, so don't create one.  Note that in the case
      // the preference values allows an off main thread save, the successful
      // return from the call doesn't mean we actually saved the file.  See
      // bug 1364496 for details.
      NS_WARNING("Failed to save pref file, cannot create temp dir.");
      return nullptr;
    }
  }

  nsCOMPtr<nsIFile> sandboxTempDir = GetProcessSandboxTempDir(procType);
  if (!sandboxTempDir) {
    NS_WARNING("Failed to determine sandbox temp dir path.");
    return nullptr;
  }

  // Remove the directory. It may exist due to a previous crash.
  if (NS_FAILED(DeleteDirIfExists(sandboxTempDir))) {
    NS_WARNING("Failed to reset sandbox temp dir.");
    return nullptr;
  }

  // Create the directory
  rv = sandboxTempDir->Create(nsIFile::DIRECTORY_TYPE, 0700);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to create sandbox temp dir.");
    return nullptr;
  }

  return sandboxTempDir.forget();
}

static nsresult DeleteDirIfExists(nsIFile* dir) {
  if (dir) {
    // Don't return an error if the directory doesn't exist.
    // Windows Remove() returns NS_ERROR_FILE_NOT_FOUND while
    // OS X returns NS_ERROR_FILE_TARGET_DOES_NOT_EXIST.
    nsresult rv = dir->Remove(/* aRecursive */ true);
    if (NS_FAILED(rv) && rv != NS_ERROR_FILE_NOT_FOUND &&
        rv != NS_ERROR_FILE_TARGET_DOES_NOT_EXIST) {
      return rv;
    }
  }
  return NS_OK;
}

#endif  // defined(MOZ_SANDBOX)

static const char* const kAppendPrefDir[] = {"defaults", "preferences",
                                             nullptr};

#ifdef DEBUG_bsmedberg
static void DumpFileArray(const char* key, nsCOMArray<nsIFile> dirs) {
  fprintf(stderr, "nsXREDirProvider::GetFilesInternal(%s)\n", key);

  nsAutoCString path;
  for (int32_t i = 0; i < dirs.Count(); ++i) {
    dirs[i]->GetNativePath(path);
    fprintf(stderr, "  %s\n", path.get());
  }
}
#endif  // DEBUG_bsmedberg

nsresult nsXREDirProvider::GetFilesInternal(const char* aProperty,
                                            nsISimpleEnumerator** aResult) {
  nsresult rv = NS_OK;
  *aResult = nullptr;

  if (!strcmp(aProperty, XRE_EXTENSIONS_DIR_LIST)) {
    nsCOMArray<nsIFile> directories;

    static const char* const kAppendNothing[] = {nullptr};

    LoadDirsIntoArray(mAppBundleDirectories, kAppendNothing, directories);

    rv = NS_NewArrayEnumerator(aResult, directories, NS_GET_IID(nsIFile));
  } else if (!strcmp(aProperty, NS_APP_PREFS_DEFAULTS_DIR_LIST)) {
    nsCOMArray<nsIFile> directories;

    LoadDirIntoArray(mXULAppDir, kAppendPrefDir, directories);
    LoadDirsIntoArray(mAppBundleDirectories, kAppendPrefDir, directories);

    rv = NS_NewArrayEnumerator(aResult, directories, NS_GET_IID(nsIFile));
  } else if (!strcmp(aProperty, NS_APP_CHROME_DIR_LIST)) {
    // NS_APP_CHROME_DIR_LIST is only used to get default (native) icons
    // for OS window decoration.

    static const char* const kAppendChromeDir[] = {"chrome", nullptr};
    nsCOMArray<nsIFile> directories;
    LoadDirIntoArray(mXULAppDir, kAppendChromeDir, directories);
    LoadDirsIntoArray(mAppBundleDirectories, kAppendChromeDir, directories);

    rv = NS_NewArrayEnumerator(aResult, directories, NS_GET_IID(nsIFile));
  } else if (!strcmp(aProperty, NS_APP_PLUGINS_DIR_LIST)) {
    nsCOMArray<nsIFile> directories;

    if (mozilla::Preferences::GetBool("plugins.load_appdir_plugins", false)) {
      nsCOMPtr<nsIFile> appdir;
      rv = XRE_GetBinaryPath(getter_AddRefs(appdir));
      if (NS_SUCCEEDED(rv)) {
        appdir->SetNativeLeafName(NS_LITERAL_CSTRING("plugins"));
        directories.AppendObject(appdir);
      }
    }

    static const char* const kAppendPlugins[] = {"plugins", nullptr};

    // The root dirserviceprovider does quite a bit for us: we're mainly
    // interested in xulapp and extension-provided plugins.
    LoadDirsIntoArray(mAppBundleDirectories, kAppendPlugins, directories);

    if (mProfileDir) {
      nsCOMArray<nsIFile> profileDir;
      profileDir.AppendObject(mProfileDir);
      LoadDirsIntoArray(profileDir, kAppendPlugins, directories);
    }

    rv = NS_NewArrayEnumerator(aResult, directories, NS_GET_IID(nsIFile));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = NS_SUCCESS_AGGREGATE_RESULT;
  } else
    rv = NS_ERROR_FAILURE;

  return rv;
}

NS_IMETHODIMP
nsXREDirProvider::GetDirectory(nsIFile** aResult) {
  NS_ENSURE_TRUE(mProfileDir, NS_ERROR_NOT_INITIALIZED);

  return mProfileDir->Clone(aResult);
}

void nsXREDirProvider::InitializeUserPrefs() {
  if (!mPrefsInitialized) {
    // Temporarily set mProfileNotified to true so that the preference service
    // can access the profile directory during initialization. Afterwards, clear
    // it so that no other code can inadvertently access it until we get to
    // profile-do-change.
    mozilla::AutoRestore<bool> ar(mProfileNotified);
    mProfileNotified = true;

    mozilla::Preferences::InitializeUserPrefs();
    mPrefsInitialized = true;
  }
}

NS_IMETHODIMP
nsXREDirProvider::DoStartup() {
  nsresult rv;

  if (!mProfileNotified) {
    nsCOMPtr<nsIObserverService> obsSvc =
        mozilla::services::GetObserverService();
    if (!obsSvc) return NS_ERROR_FAILURE;

    mProfileNotified = true;

    /*
       Make sure we've setup prefs before profile-do-change to be able to use
       them to track crashes and because we want to begin crash tracking before
       other code run from this notification since they may cause crashes.
    */
    MOZ_ASSERT(mPrefsInitialized);

    bool safeModeNecessary = false;
    nsCOMPtr<nsIAppStartup> appStartup(
        mozilla::components::AppStartup::Service());
    if (appStartup) {
      rv = appStartup->TrackStartupCrashBegin(&safeModeNecessary);
      if (NS_FAILED(rv) && rv != NS_ERROR_NOT_AVAILABLE)
        NS_WARNING("Error while beginning startup crash tracking");

      if (!gSafeMode && safeModeNecessary) {
        appStartup->RestartInSafeMode(nsIAppStartup::eForceQuit);
        return NS_OK;
      }
    }

    static const char16_t kStartup[] = {'s', 't', 'a', 'r',
                                        't', 'u', 'p', '\0'};
    obsSvc->NotifyObservers(nullptr, "profile-do-change", kStartup);

    // Initialize the Enterprise Policies service in the parent process
    // In the content process it's loaded on demand when needed
    if (XRE_IsParentProcess()) {
      nsCOMPtr<nsIObserver> policies(
          do_GetService("@mozilla.org/enterprisepolicies;1"));
      if (policies) {
        policies->Observe(nullptr, "policies-startup", nullptr);
      }
    }

#if defined(MOZ_SANDBOX) && defined(XP_WIN)
    // Call SandboxBroker to initialize things that depend on Gecko machinery
    // like the directory provider. We insert this initialization code here
    // (rather than in XRE_mainRun) because we need NS_APP_USER_PROFILE_50_DIR
    // to be known and so that any child content processes spawned by extensions
    // from the notifications below will have all the requisite directories
    // white-listed for read/write access. An example of this is the
    // tor-launcher launching the network configuration window. See bug 1485836.
    mozilla::SandboxBroker::GeckoDependentInitialize();
#endif

    // Init the Extension Manager
    nsCOMPtr<nsIObserver> em =
        do_GetService("@mozilla.org/addons/integration;1");
    if (em) {
      em->Observe(nullptr, "addons-startup", nullptr);
    } else {
      NS_WARNING("Failed to create Addons Manager.");
    }

    obsSvc->NotifyObservers(nullptr, "profile-after-change", kStartup);

    // Any component that has registered for the profile-after-change category
    // should also be created at this time.
    (void)NS_CreateServicesFromCategory("profile-after-change", nullptr,
                                        "profile-after-change");

    if (gSafeMode && safeModeNecessary) {
      static const char16_t kCrashed[] = {'c', 'r', 'a', 's',
                                          'h', 'e', 'd', '\0'};
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

    // Telemetry about number of profiles.
    nsCOMPtr<nsIToolkitProfileService> profileService =
        do_GetService("@mozilla.org/toolkit/profile-service;1");
    if (profileService) {
      uint32_t count = 0;
      rv = profileService->GetProfileCount(&count);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      mozilla::Telemetry::Accumulate(mozilla::Telemetry::NUMBER_OF_PROFILES,
                                     count);
    }

    obsSvc->NotifyObservers(nullptr, "profile-initial-state", nullptr);

#if defined(MOZ_SANDBOX)
    // Makes sure the content temp dir has been loaded if it hasn't been
    // already. In the parent this ensures it has been created before we attempt
    // to start any content processes.
    if (!mContentTempDir) {
      mozilla::Unused << NS_WARN_IF(NS_FAILED(LoadContentProcessTempDir()));
    }
#endif
#if defined(MOZ_SANDBOX)
    if (!mPluginTempDir) {
      mozilla::Unused << NS_WARN_IF(NS_FAILED(LoadPluginProcessTempDir()));
    }
#endif
  }
  return NS_OK;
}

void nsXREDirProvider::DoShutdown() {
  AUTO_PROFILER_LABEL("nsXREDirProvider::DoShutdown", OTHER);

  if (mProfileNotified) {
    nsCOMPtr<nsIObserverService> obsSvc =
        mozilla::services::GetObserverService();
    NS_ASSERTION(obsSvc, "No observer service?");
    if (obsSvc) {
      static const char16_t kShutdownPersist[] = u"shutdown-persist";
      obsSvc->NotifyObservers(nullptr, "profile-change-net-teardown",
                              kShutdownPersist);
      obsSvc->NotifyObservers(nullptr, "profile-change-teardown",
                              kShutdownPersist);

#ifdef DEBUG
      // Not having this causes large intermittent leaks. See bug 1340425.
      if (JSContext* cx = mozilla::dom::danger::GetJSContext()) {
        JS_GC(cx);
      }
#endif

      obsSvc->NotifyObservers(nullptr, "profile-before-change",
                              kShutdownPersist);
      obsSvc->NotifyObservers(nullptr, "profile-before-change-qm",
                              kShutdownPersist);
      obsSvc->NotifyObservers(nullptr, "profile-before-change-telemetry",
                              kShutdownPersist);
    }
    mProfileNotified = false;
  }

  if (XRE_IsParentProcess()) {
#if defined(MOZ_SANDBOX)
    mozilla::Unused << DeleteDirIfExists(mContentProcessSandboxTempDir);
    mozilla::Unused << DeleteDirIfExists(mPluginProcessSandboxTempDir);
#endif
  }
}

#ifdef XP_WIN
static nsresult GetShellFolderPath(KNOWNFOLDERID folder, nsAString& _retval) {
  DWORD flags = KF_FLAG_SIMPLE_IDLIST | KF_FLAG_DONT_VERIFY | KF_FLAG_NO_ALIAS;
  PWSTR path = nullptr;

  if (!SUCCEEDED(SHGetKnownFolderPath(folder, flags, NULL, &path))) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  _retval = nsDependentString(path);
  CoTaskMemFree(path);
  return NS_OK;
}

/**
 * Provides a fallback for getting the path to APPDATA or LOCALAPPDATA by
 * querying the registry when the call to SHGetSpecialFolderLocation or
 * SHGetPathFromIDListW is unable to provide these paths (Bug 513958).
 */
static nsresult GetRegWindowsAppDataFolder(bool aLocal, nsAString& _retval) {
  HKEY key;
  LPCWSTR keyName =
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders";
  DWORD res = ::RegOpenKeyExW(HKEY_CURRENT_USER, keyName, 0, KEY_READ, &key);
  if (res != ERROR_SUCCESS) {
    _retval.SetLength(0);
    return NS_ERROR_NOT_AVAILABLE;
  }

  DWORD type, size;
  res = RegQueryValueExW(key, (aLocal ? L"Local AppData" : L"AppData"), nullptr,
                         &type, nullptr, &size);
  // The call to RegQueryValueExW must succeed, the type must be REG_SZ, the
  // buffer size must not equal 0, and the buffer size be a multiple of 2.
  if (res != ERROR_SUCCESS || type != REG_SZ || size == 0 || size % 2 != 0) {
    ::RegCloseKey(key);
    _retval.SetLength(0);
    return NS_ERROR_NOT_AVAILABLE;
  }

  // |size| may or may not include room for the terminating null character
  DWORD resultLen = size / 2;

  if (!_retval.SetLength(resultLen, mozilla::fallible)) {
    ::RegCloseKey(key);
    _retval.SetLength(0);
    return NS_ERROR_NOT_AVAILABLE;
  }

  auto begin = _retval.BeginWriting();

  res = RegQueryValueExW(key, (aLocal ? L"Local AppData" : L"AppData"), nullptr,
                         nullptr, (LPBYTE)begin, &size);
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
#endif

nsresult nsXREDirProvider::GetInstallHash(nsAString& aPathHash) {
  nsCOMPtr<nsIFile> installDir;
  nsCOMPtr<nsIFile> appFile;
  bool per = false;
  nsresult rv = GetFile(XRE_EXECUTABLE_FILE, &per, getter_AddRefs(appFile));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = appFile->GetParent(getter_AddRefs(installDir));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString installPath;
  rv = installDir->GetPath(installPath);
  NS_ENSURE_SUCCESS(rv, rv);

  const char* vendor = GetAppVendor();
  if (vendor && vendor[0] == '\0') {
    vendor = nullptr;
  }

  mozilla::UniquePtr<NS_tchar[]> hash;
  rv = ::GetInstallHash(PromiseFlatString(installPath).get(), vendor, hash);
  NS_ENSURE_SUCCESS(rv, rv);

  // The hash string is a NS_tchar*, which is wchar* in Windows and char*
  // elsewhere.
#ifdef XP_WIN
  aPathHash.Assign(hash.get());
#else
  aPathHash.AssignASCII(hash.get());
#endif
  return NS_OK;
}

nsresult nsXREDirProvider::GetUpdateRootDir(nsIFile** aResult,
                                            bool aGetOldLocation) {
#ifndef XP_WIN
  // There is no old update location on platforms other than Windows. Windows is
  // the only platform for which we migrated the update directory.
  if (aGetOldLocation) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
#endif
  nsCOMPtr<nsIFile> updRoot;
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

  bool hasVendor = GetAppVendor() && strlen(GetAppVendor()) != 0;
  if (hasVendor || GetAppName()) {
    if (NS_FAILED(localDir->AppendNative(
            nsDependentCString(hasVendor ? GetAppVendor() : GetAppName())))) {
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
  nsAutoString installPath;
  rv = updRoot->GetPath(installPath);
  NS_ENSURE_SUCCESS(rv, rv);

  mozilla::UniquePtr<wchar_t[]> updatePath;
  HRESULT hrv;
  if (aGetOldLocation) {
    const char* vendor = GetAppVendor();
    if (vendor && vendor[0] == '\0') {
      vendor = nullptr;
    }
    const char* appName = GetAppName();
    if (appName && appName[0] == '\0') {
      appName = nullptr;
    }
    hrv = GetUserUpdateDirectory(PromiseFlatString(installPath).get(), vendor,
                                 appName, updatePath);
  } else {
    hrv = GetCommonUpdateDirectory(PromiseFlatString(installPath).get(),
                                   SetPermissionsOf::BaseDirIfNotExists,
                                   updatePath);
  }
  if (FAILED(hrv)) {
    return NS_ERROR_FAILURE;
  }
  nsAutoString updatePathStr;
  updatePathStr.Assign(updatePath.get());
  updRoot->InitWithPath(updatePathStr);
#endif  // XP_WIN
  updRoot.forget(aResult);
  return NS_OK;
}

nsresult nsXREDirProvider::GetProfileStartupDir(nsIFile** aResult) {
  if (mProfileDir) return mProfileDir->Clone(aResult);

  if (mAppProvider) {
    nsCOMPtr<nsIFile> needsclone;
    bool dummy;
    nsresult rv = mAppProvider->GetFile(NS_APP_PROFILE_DIR_STARTUP, &dummy,
                                        getter_AddRefs(needsclone));
    if (NS_SUCCEEDED(rv)) return needsclone->Clone(aResult);
  }

  return NS_ERROR_FAILURE;
}

nsresult nsXREDirProvider::GetProfileDir(nsIFile** aResult) {
  if (mProfileDir) {
    if (!mProfileNotified) return NS_ERROR_FAILURE;

    return mProfileDir->Clone(aResult);
  }

  if (mAppProvider) {
    nsCOMPtr<nsIFile> needsclone;
    bool dummy;
    nsresult rv = mAppProvider->GetFile(NS_APP_USER_PROFILE_50_DIR, &dummy,
                                        getter_AddRefs(needsclone));
    if (NS_SUCCEEDED(rv)) return needsclone->Clone(aResult);
  }

  return NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, aResult);
}

NS_IMETHODIMP
nsXREDirProvider::SetUserDataDirectory(nsIFile* aFile, bool aLocal) {
  if (aLocal) {
    NS_IF_RELEASE(gDataDirHomeLocal);
    NS_IF_ADDREF(gDataDirHomeLocal = aFile);
  } else {
    NS_IF_RELEASE(gDataDirHome);
    NS_IF_ADDREF(gDataDirHome = aFile);
  }

  return NS_OK;
}

nsresult nsXREDirProvider::GetUserDataDirectoryHome(nsIFile** aFile,
                                                    bool aLocal) {
  // Copied from nsAppFileLocationProvider (more or less)
  nsresult rv;
  nsCOMPtr<nsIFile> localDir;

  if (aLocal && gDataDirHomeLocal) {
    return gDataDirHomeLocal->Clone(aFile);
  }
  if (!aLocal && gDataDirHome) {
    return gDataDirHome->Clone(aFile);
  }

#if defined(XP_MACOSX)
  FSRef fsRef;
  OSType folderType;
  if (aLocal) {
    folderType = kCachedDataFolderType;
  } else {
#  ifdef MOZ_THUNDERBIRD
    folderType = kDomainLibraryFolderType;
#  else
    folderType = kApplicationSupportFolderType;
#  endif
  }
  OSErr err = ::FSFindFolder(kUserDomain, folderType, kCreateFolder, &fsRef);
  NS_ENSURE_FALSE(err, NS_ERROR_FAILURE);

  rv = NS_NewNativeLocalFile(EmptyCString(), true, getter_AddRefs(localDir));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsILocalFileMac> dirFileMac = do_QueryInterface(localDir);
  NS_ENSURE_TRUE(dirFileMac, NS_ERROR_UNEXPECTED);

  rv = dirFileMac->InitWithFSRef(&fsRef);
  NS_ENSURE_SUCCESS(rv, rv);

  localDir = dirFileMac;
#elif defined(XP_IOS)
  nsAutoCString userDir;
  if (GetUIKitDirectory(aLocal, userDir)) {
    rv = NS_NewNativeLocalFile(userDir, true, getter_AddRefs(localDir));
  } else {
    rv = NS_ERROR_FAILURE;
  }
  NS_ENSURE_SUCCESS(rv, rv);
#elif defined(XP_WIN)
  nsString path;
  if (aLocal) {
    rv = GetShellFolderPath(FOLDERID_LocalAppData, path);
    if (NS_FAILED(rv)) rv = GetRegWindowsAppDataFolder(aLocal, path);
  }
  if (!aLocal || NS_FAILED(rv)) {
    rv = GetShellFolderPath(FOLDERID_RoamingAppData, path);
    if (NS_FAILED(rv)) {
      if (!aLocal) rv = GetRegWindowsAppDataFolder(aLocal, path);
    }
  }
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_NewLocalFile(path, true, getter_AddRefs(localDir));
#elif defined(XP_UNIX)
  const char* homeDir = getenv("HOME");
  if (!homeDir || !*homeDir) return NS_ERROR_FAILURE;

#  ifdef ANDROID /* We want (ProfD == ProfLD) on Android. */
  aLocal = false;
#  endif

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
#  error "Don't know how to get product dir on your platform"
#endif

  NS_IF_ADDREF(*aFile = localDir);
  return rv;
}

nsresult nsXREDirProvider::GetSysUserExtensionsDirectory(nsIFile** aFile) {
  nsCOMPtr<nsIFile> localDir;
  nsresult rv = GetUserDataDirectoryHome(getter_AddRefs(localDir), false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AppendSysUserExtensionPath(localDir);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = EnsureDirectoryExists(localDir);
  NS_ENSURE_SUCCESS(rv, rv);

#if defined(XP_WIN) && defined(MOZ_SANDBOX)
  // This is used in sandbox rules, so we need to make sure it doesn't contain
  // any junction points or symlinks or the sandbox will reject those rules.
  if (!mozilla::widget::WinUtils::ResolveJunctionPointsAndSymLinks(localDir)) {
    NS_WARNING("Failed to resolve sys user extensions directory.");
  }
#endif

  localDir.forget(aFile);
  return NS_OK;
}

nsresult nsXREDirProvider::GetSysUserExtensionsDevDirectory(nsIFile** aFile) {
  nsCOMPtr<nsIFile> localDir;
  nsresult rv = GetUserDataDirectoryHome(getter_AddRefs(localDir), false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AppendSysUserExtensionsDevPath(localDir);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = EnsureDirectoryExists(localDir);
  NS_ENSURE_SUCCESS(rv, rv);

#if defined(XP_WIN) && defined(MOZ_SANDBOX)
  // This is used in sandbox rules, so we need to make sure it doesn't contain
  // any junction points or symlinks or the sandbox will reject those rules.
  if (!mozilla::widget::WinUtils::ResolveJunctionPointsAndSymLinks(localDir)) {
    NS_WARNING("Failed to resolve sys user extensions dev directory.");
  }
#endif

  localDir.forget(aFile);
  return NS_OK;
}

#if defined(XP_UNIX) || defined(XP_MACOSX)
nsresult nsXREDirProvider::GetSystemExtensionsDirectory(nsIFile** aFile) {
  nsresult rv;
  nsCOMPtr<nsIFile> localDir;

  rv = GetSystemParentDirectory(getter_AddRefs(localDir));
  if (NS_SUCCEEDED(rv)) {
    NS_NAMED_LITERAL_CSTRING(sExtensions,
#  if defined(XP_MACOSX)
                             "Extensions"
#  else
                             "extensions"
#  endif
    );

    rv = localDir->AppendNative(sExtensions);
    if (NS_SUCCEEDED(rv)) {
      localDir.forget(aFile);
    }
  }
  return rv;
}
#endif

nsresult nsXREDirProvider::GetUserDataDirectory(nsIFile** aFile, bool aLocal) {
  nsCOMPtr<nsIFile> localDir;
  nsresult rv = GetUserDataDirectoryHome(getter_AddRefs(localDir), aLocal);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AppendProfilePath(localDir, aLocal);
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

nsresult nsXREDirProvider::EnsureDirectoryExists(nsIFile* aDirectory) {
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
  if (!exists) rv = aDirectory->Create(nsIFile::DIRECTORY_TYPE, 0700);
#ifdef DEBUG_jungshik
  if (NS_FAILED(rv))
    NS_WARNING("nsXREDirProvider::EnsureDirectoryExists: create failed");
#endif

  return rv;
}

nsresult nsXREDirProvider::AppendSysUserExtensionPath(nsIFile* aFile) {
  NS_ASSERTION(aFile, "Null pointer!");

  nsresult rv;

#if defined(XP_MACOSX) || defined(XP_WIN)

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
#  error "Don't know how to get XRE user extension path on your platform"
#endif
  return NS_OK;
}

nsresult nsXREDirProvider::AppendSysUserExtensionsDevPath(nsIFile* aFile) {
  MOZ_ASSERT(aFile);

  nsresult rv;

#if defined(XP_MACOSX) || defined(XP_WIN)

  static const char* const sXR = "Mozilla";
  rv = aFile->AppendNative(nsDependentCString(sXR));
  NS_ENSURE_SUCCESS(rv, rv);

  static const char* const sExtensions = "SystemExtensionsDev";
  rv = aFile->AppendNative(nsDependentCString(sExtensions));
  NS_ENSURE_SUCCESS(rv, rv);

#elif defined(XP_UNIX)

  static const char* const sXR = ".mozilla";
  rv = aFile->AppendNative(nsDependentCString(sXR));
  NS_ENSURE_SUCCESS(rv, rv);

  static const char* const sExtensions = "systemextensionsdev";
  rv = aFile->AppendNative(nsDependentCString(sExtensions));
  NS_ENSURE_SUCCESS(rv, rv);

#else
#  error "Don't know how to get XRE system extension dev path on your platform"
#endif
  return NS_OK;
}

nsresult nsXREDirProvider::AppendProfilePath(nsIFile* aFile, bool aLocal) {
  NS_ASSERTION(aFile, "Null pointer!");

  // If there is no XREAppData then there is no information to use to build
  // the profile path so just do nothing. This should only happen in xpcshell
  // tests.
  if (!gAppData) {
    return NS_OK;
  }

  nsAutoCString profile;
  nsAutoCString appName;
  nsAutoCString vendor;
  if (gAppData->profile) {
    profile = gAppData->profile;
  } else {
    appName = gAppData->name;
    vendor = gAppData->vendor;
  }

  nsresult rv = NS_OK;

#if defined(XP_MACOSX)
  if (!profile.IsEmpty()) {
    rv = AppendProfileString(aFile, profile.get());
  } else {
    // Note that MacOS ignores the vendor when creating the profile hierarchy -
    // all application preferences directories live alongside one another in
    // ~/Library/Application Support/
    rv = aFile->AppendNative(appName);
  }
  NS_ENSURE_SUCCESS(rv, rv);

#elif defined(XP_WIN)
  if (!profile.IsEmpty()) {
    rv = AppendProfileString(aFile, profile.get());
  } else {
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
  rv = aFile->AppendNative(nsDependentCString("mozilla"));
  NS_ENSURE_SUCCESS(rv, rv);
#elif defined(XP_UNIX)
  nsAutoCString folder;
  // Make it hidden (by starting with "."), except when local (the
  // profile is already under ~/.cache or XDG_CACHE_HOME).
  if (!aLocal) folder.Assign('.');

  if (!profile.IsEmpty()) {
    // Skip any leading path characters
    const char* profileStart = profile.get();
    while (*profileStart == '/' || *profileStart == '\\') profileStart++;

    // On the off chance that someone wanted their folder to be hidden don't
    // let it become ".."
    if (*profileStart == '.' && !aLocal) profileStart++;

    folder.Append(profileStart);
    ToLowerCase(folder);

    rv = AppendProfileString(aFile, folder.BeginReading());
  } else {
    if (!vendor.IsEmpty()) {
      folder.Append(vendor);
      ToLowerCase(folder);

      rv = aFile->AppendNative(folder);
      NS_ENSURE_SUCCESS(rv, rv);

      folder.Truncate();
    }

    // This can be the case in tests.
    if (!appName.IsEmpty()) {
      folder.Append(appName);
      ToLowerCase(folder);

      rv = aFile->AppendNative(folder);
    }
  }
  NS_ENSURE_SUCCESS(rv, rv);

#else
#  error "Don't know how to get profile path on your platform"
#endif
  return NS_OK;
}

nsresult nsXREDirProvider::AppendProfileString(nsIFile* aFile,
                                               const char* aPath) {
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
