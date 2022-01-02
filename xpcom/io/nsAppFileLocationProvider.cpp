/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAppFileLocationProvider.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "nsEnumeratorUtils.h"
#include "nsAtom.h"
#include "nsIDirectoryService.h"
#include "nsIFile.h"
#include "nsString.h"
#include "nsSimpleEnumerator.h"
#include "prenv.h"
#include "nsCRT.h"
#if defined(MOZ_WIDGET_COCOA)
#  include <Carbon/Carbon.h>
#  include "nsILocalFileMac.h"
#elif defined(XP_WIN)
#  include <windows.h>
#  include <shlobj.h>
#elif defined(XP_UNIX)
#  include <unistd.h>
#  include <stdlib.h>
#  include <sys/param.h>
#endif

// WARNING: These hard coded names need to go away. They need to
// come from localizable resources

#if defined(MOZ_WIDGET_COCOA)
#  define APP_REGISTRY_NAME "Application Registry"_ns
#  define ESSENTIAL_FILES "Essential Files"_ns
#elif defined(XP_WIN)
#  define APP_REGISTRY_NAME "registry.dat"_ns
#else
#  define APP_REGISTRY_NAME "appreg"_ns
#endif

// define default product directory
#define DEFAULT_PRODUCT_DIR nsLiteralCString(MOZ_USER_DIR)

// Locally defined keys used by nsAppDirectoryEnumerator
#define NS_USER_PLUGINS_DIR "UserPlugins"

#ifdef MOZ_WIDGET_COCOA
#  define NS_MACOSX_USER_PLUGIN_DIR "OSXUserPlugins"
#  define NS_MACOSX_LOCAL_PLUGIN_DIR "OSXLocalPlugins"
#elif XP_UNIX
#  define NS_SYSTEM_PLUGINS_DIR "SysPlugins"
#endif

#define DEFAULTS_DIR_NAME "defaults"_ns
#define DEFAULTS_PREF_DIR_NAME "pref"_ns
#define RES_DIR_NAME "res"_ns
#define CHROME_DIR_NAME "chrome"_ns
#define PLUGINS_DIR_NAME "plugins"_ns

//*****************************************************************************
// nsAppFileLocationProvider::Constructor/Destructor
//*****************************************************************************

nsAppFileLocationProvider::nsAppFileLocationProvider() = default;

//*****************************************************************************
// nsAppFileLocationProvider::nsISupports
//*****************************************************************************

NS_IMPL_ISUPPORTS(nsAppFileLocationProvider, nsIDirectoryServiceProvider,
                  nsIDirectoryServiceProvider2)

//*****************************************************************************
// nsAppFileLocationProvider::nsIDirectoryServiceProvider
//*****************************************************************************

NS_IMETHODIMP
nsAppFileLocationProvider::GetFile(const char* aProp, bool* aPersistent,
                                   nsIFile** aResult) {
  if (NS_WARN_IF(!aProp)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIFile> localFile;
  nsresult rv = NS_ERROR_FAILURE;

  *aResult = nullptr;
  *aPersistent = true;

#ifdef MOZ_WIDGET_COCOA
  FSRef fileRef;
  nsCOMPtr<nsILocalFileMac> macFile;
#endif

  if (nsCRT::strcmp(aProp, NS_APP_APPLICATION_REGISTRY_DIR) == 0) {
    rv = GetProductDirectory(getter_AddRefs(localFile));
  } else if (nsCRT::strcmp(aProp, NS_APP_APPLICATION_REGISTRY_FILE) == 0) {
    rv = GetProductDirectory(getter_AddRefs(localFile));
    if (NS_SUCCEEDED(rv)) {
      rv = localFile->AppendNative(APP_REGISTRY_NAME);
    }
  } else if (nsCRT::strcmp(aProp, NS_APP_DEFAULTS_50_DIR) == 0) {
    rv = CloneMozBinDirectory(getter_AddRefs(localFile));
    if (NS_SUCCEEDED(rv)) {
      rv = localFile->AppendRelativeNativePath(DEFAULTS_DIR_NAME);
    }
  } else if (nsCRT::strcmp(aProp, NS_APP_PREF_DEFAULTS_50_DIR) == 0) {
    rv = CloneMozBinDirectory(getter_AddRefs(localFile));
    if (NS_SUCCEEDED(rv)) {
      rv = localFile->AppendRelativeNativePath(DEFAULTS_DIR_NAME);
      if (NS_SUCCEEDED(rv)) {
        rv = localFile->AppendRelativeNativePath(DEFAULTS_PREF_DIR_NAME);
      }
    }
  } else if (nsCRT::strcmp(aProp, NS_APP_USER_PROFILES_ROOT_DIR) == 0) {
    rv = GetDefaultUserProfileRoot(getter_AddRefs(localFile));
  } else if (nsCRT::strcmp(aProp, NS_APP_USER_PROFILES_LOCAL_ROOT_DIR) == 0) {
    rv = GetDefaultUserProfileRoot(getter_AddRefs(localFile), true);
  } else if (nsCRT::strcmp(aProp, NS_APP_RES_DIR) == 0) {
    rv = CloneMozBinDirectory(getter_AddRefs(localFile));
    if (NS_SUCCEEDED(rv)) {
      rv = localFile->AppendRelativeNativePath(RES_DIR_NAME);
    }
  } else if (nsCRT::strcmp(aProp, NS_APP_CHROME_DIR) == 0) {
    rv = CloneMozBinDirectory(getter_AddRefs(localFile));
    if (NS_SUCCEEDED(rv)) {
      rv = localFile->AppendRelativeNativePath(CHROME_DIR_NAME);
    }
  }
#ifdef MOZ_WIDGET_COCOA
  else if (nsCRT::strcmp(aProp, NS_MACOSX_USER_PLUGIN_DIR) == 0) {
    if (::FSFindFolder(kUserDomain, kInternetPlugInFolderType, false,
                       &fileRef) == noErr) {
      rv = NS_NewLocalFileWithFSRef(&fileRef, true, getter_AddRefs(macFile));
      if (NS_SUCCEEDED(rv)) {
        localFile = macFile;
      }
    }
  } else if (nsCRT::strcmp(aProp, NS_MACOSX_LOCAL_PLUGIN_DIR) == 0) {
    if (::FSFindFolder(kLocalDomain, kInternetPlugInFolderType, false,
                       &fileRef) == noErr) {
      rv = NS_NewLocalFileWithFSRef(&fileRef, true, getter_AddRefs(macFile));
      if (NS_SUCCEEDED(rv)) {
        localFile = macFile;
      }
    }
  }
#else
  else if (nsCRT::strcmp(aProp, NS_USER_PLUGINS_DIR) == 0) {
#  ifdef ENABLE_SYSTEM_EXTENSION_DIRS
    rv = GetProductDirectory(getter_AddRefs(localFile));
    if (NS_SUCCEEDED(rv)) {
      rv = localFile->AppendRelativeNativePath(PLUGINS_DIR_NAME);
    }
#  else
    rv = NS_ERROR_FAILURE;
#  endif
  }
#  ifdef XP_UNIX
  else if (nsCRT::strcmp(aProp, NS_SYSTEM_PLUGINS_DIR) == 0) {
#    ifdef ENABLE_SYSTEM_EXTENSION_DIRS
    static const char* const sysLPlgDir =
#      if defined(HAVE_USR_LIB64_DIR) && defined(__LP64__)
        "/usr/lib64/mozilla/plugins";
#      elif defined(__OpenBSD__) || defined(__FreeBSD__)
        "/usr/local/lib/mozilla/plugins";
#      else
        "/usr/lib/mozilla/plugins";
#      endif
    rv = NS_NewNativeLocalFile(nsDependentCString(sysLPlgDir), false,
                               getter_AddRefs(localFile));
#    else
    rv = NS_ERROR_FAILURE;
#    endif
  }
#  endif
#endif
  else if (nsCRT::strcmp(aProp, NS_APP_INSTALL_CLEANUP_DIR) == 0) {
    // This is cloned so that embeddors will have a hook to override
    // with their own cleanup dir.  See bugzilla bug #105087
    rv = CloneMozBinDirectory(getter_AddRefs(localFile));
  }

  if (localFile && NS_SUCCEEDED(rv)) {
    localFile.forget(aResult);
    return NS_OK;
  }

  return rv;
}

nsresult nsAppFileLocationProvider::CloneMozBinDirectory(nsIFile** aLocalFile) {
  if (NS_WARN_IF(!aLocalFile)) {
    return NS_ERROR_INVALID_ARG;
  }
  nsresult rv;

  if (!mMozBinDirectory) {
    // Get the mozilla bin directory
    // 1. Check the directory service first for NS_XPCOM_CURRENT_PROCESS_DIR
    //    This will be set if a directory was passed to NS_InitXPCOM
    // 2. If that doesn't work, set it to be the current process directory
    nsCOMPtr<nsIProperties> directoryService(
        do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv));
    if (NS_FAILED(rv)) {
      return rv;
    }

    rv =
        directoryService->Get(NS_XPCOM_CURRENT_PROCESS_DIR, NS_GET_IID(nsIFile),
                              getter_AddRefs(mMozBinDirectory));
    if (NS_FAILED(rv)) {
      rv = directoryService->Get(NS_OS_CURRENT_PROCESS_DIR, NS_GET_IID(nsIFile),
                                 getter_AddRefs(mMozBinDirectory));
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }

  nsCOMPtr<nsIFile> aFile;
  rv = mMozBinDirectory->Clone(getter_AddRefs(aFile));
  if (NS_FAILED(rv)) {
    return rv;
  }

  NS_IF_ADDREF(*aLocalFile = aFile);
  return NS_OK;
}

//----------------------------------------------------------------------------------------
// GetProductDirectory - Gets the directory which contains the application data
// folder
//
// UNIX   : ~/.mozilla/
// WIN    : <Application Data folder on user's machine>\Mozilla
// Mac    : :Documents:Mozilla:
//----------------------------------------------------------------------------------------
nsresult nsAppFileLocationProvider::GetProductDirectory(nsIFile** aLocalFile,
                                                        bool aLocal) {
  if (NS_WARN_IF(!aLocalFile)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv;
  bool exists;
  nsCOMPtr<nsIFile> localDir;

#if defined(MOZ_WIDGET_COCOA)
  FSRef fsRef;
  OSType folderType =
      aLocal ? (OSType)kCachedDataFolderType : (OSType)kDomainLibraryFolderType;
  OSErr err = ::FSFindFolder(kUserDomain, folderType, kCreateFolder, &fsRef);
  if (err) {
    return NS_ERROR_FAILURE;
  }
  NS_NewLocalFile(u""_ns, true, getter_AddRefs(localDir));
  if (!localDir) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsILocalFileMac> localDirMac(do_QueryInterface(localDir));
  rv = localDirMac->InitWithFSRef(&fsRef);
  if (NS_FAILED(rv)) {
    return rv;
  }
#elif defined(XP_WIN)
  nsCOMPtr<nsIProperties> directoryService =
      do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }
  const char* prop = aLocal ? NS_WIN_LOCAL_APPDATA_DIR : NS_WIN_APPDATA_DIR;
  rv = directoryService->Get(prop, NS_GET_IID(nsIFile),
                             getter_AddRefs(localDir));
  if (NS_FAILED(rv)) {
    return rv;
  }
#elif defined(XP_UNIX)
  rv = NS_NewNativeLocalFile(nsDependentCString(PR_GetEnv("HOME")), true,
                             getter_AddRefs(localDir));
  if (NS_FAILED(rv)) {
    return rv;
  }
#else
#  error dont_know_how_to_get_product_dir_on_your_platform
#endif

  rv = localDir->AppendRelativeNativePath(DEFAULT_PRODUCT_DIR);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = localDir->Exists(&exists);

  if (NS_SUCCEEDED(rv) && !exists) {
    rv = localDir->Create(nsIFile::DIRECTORY_TYPE, 0700);
  }

  if (NS_FAILED(rv)) {
    return rv;
  }

  localDir.forget(aLocalFile);

  return rv;
}

//----------------------------------------------------------------------------------------
// GetDefaultUserProfileRoot - Gets the directory which contains each user
// profile dir
//
// UNIX   : ~/.mozilla/
// WIN    : <Application Data folder on user's machine>\Mozilla\Profiles
// Mac    : :Documents:Mozilla:Profiles:
//----------------------------------------------------------------------------------------
nsresult nsAppFileLocationProvider::GetDefaultUserProfileRoot(
    nsIFile** aLocalFile, bool aLocal) {
  if (NS_WARN_IF(!aLocalFile)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv;
  nsCOMPtr<nsIFile> localDir;

  rv = GetProductDirectory(getter_AddRefs(localDir), aLocal);
  if (NS_FAILED(rv)) {
    return rv;
  }

#if defined(MOZ_WIDGET_COCOA) || defined(XP_WIN)
  // These 3 platforms share this part of the path - do them as one
  rv = localDir->AppendRelativeNativePath("Profiles"_ns);
  if (NS_FAILED(rv)) {
    return rv;
  }

  bool exists;
  rv = localDir->Exists(&exists);
  if (NS_SUCCEEDED(rv) && !exists) {
    rv = localDir->Create(nsIFile::DIRECTORY_TYPE, 0775);
  }
  if (NS_FAILED(rv)) {
    return rv;
  }
#endif

  localDir.forget(aLocalFile);

  return rv;
}

//*****************************************************************************
// nsAppFileLocationProvider::nsIDirectoryServiceProvider2
//*****************************************************************************

class nsAppDirectoryEnumerator : public nsSimpleEnumerator {
 public:
  /**
   * aKeyList is a null-terminated list of properties which are provided by
   * aProvider They do not need to be publicly defined keys.
   */
  nsAppDirectoryEnumerator(nsIDirectoryServiceProvider* aProvider,
                           const char* aKeyList[])
      : mProvider(aProvider), mCurrentKey(aKeyList) {}

  const nsID& DefaultInterface() override { return NS_GET_IID(nsIFile); }

  NS_IMETHOD HasMoreElements(bool* aResult) override {
    while (!mNext && *mCurrentKey) {
      bool dontCare;
      nsCOMPtr<nsIFile> testFile;
      (void)mProvider->GetFile(*mCurrentKey++, &dontCare,
                               getter_AddRefs(testFile));
      mNext = testFile;
    }
    *aResult = mNext != nullptr;
    return NS_OK;
  }

  NS_IMETHOD GetNext(nsISupports** aResult) override {
    if (NS_WARN_IF(!aResult)) {
      return NS_ERROR_INVALID_ARG;
    }
    *aResult = nullptr;

    bool hasMore;
    HasMoreElements(&hasMore);
    if (!hasMore) {
      return NS_ERROR_FAILURE;
    }

    *aResult = mNext;
    NS_IF_ADDREF(*aResult);
    mNext = nullptr;

    return *aResult ? NS_OK : NS_ERROR_FAILURE;
  }

 protected:
  nsCOMPtr<nsIDirectoryServiceProvider> mProvider;
  const char** mCurrentKey;
  nsCOMPtr<nsIFile> mNext;
};

NS_IMETHODIMP
nsAppFileLocationProvider::GetFiles(const char* aProp,
                                    nsISimpleEnumerator** aResult) {
  if (NS_WARN_IF(!aResult)) {
    return NS_ERROR_INVALID_ARG;
  }
  *aResult = nullptr;
  nsresult rv = NS_ERROR_FAILURE;

  if (!nsCRT::strcmp(aProp, NS_APP_PLUGINS_DIR_LIST)) {
#ifdef MOZ_WIDGET_COCOA
    static const char* keys[] = {NS_MACOSX_USER_PLUGIN_DIR,
                                 NS_MACOSX_LOCAL_PLUGIN_DIR, nullptr};
#else
#  ifdef XP_UNIX
    static const char* keys[] = {NS_USER_PLUGINS_DIR, NS_SYSTEM_PLUGINS_DIR,
                                 nullptr};
#  else
    static const char* keys[] = {NS_USER_PLUGINS_DIR, nullptr};
#  endif
#endif
    *aResult = new nsAppDirectoryEnumerator(this, keys);
    NS_ADDREF(*aResult);
    rv = NS_OK;
  }
  return rv;
}
