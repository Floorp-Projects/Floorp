/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsXREDirProvider_h__
#define _nsXREDirProvider_h__

#include "nsIDirectoryService.h"
#include "nsIProfileMigrator.h"
#include "nsIFile.h"
#include "nsIXREDirProvider.h"

#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "mozilla/Attributes.h"

// {5573967d-f6cf-4c63-8e0e-9ac06e04d62b}
#define NS_XREDIRPROVIDER_CID                        \
  {                                                  \
    0x5573967d, 0xf6cf, 0x4c63, {                    \
      0x8e, 0x0e, 0x9a, 0xc0, 0x6e, 0x04, 0xd6, 0x2b \
    }                                                \
  }
#define NS_XREDIRPROVIDER_CONTRACTID "@mozilla.org/xre/directory-provider;1"

class nsXREDirProvider final : public nsIDirectoryServiceProvider2,
                               public nsIXREDirProvider,
                               public nsIProfileStartup {
 public:
  // we use a custom isupports implementation (no refcount)
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) override;
  NS_IMETHOD_(MozExternalRefCountType) AddRef(void) override;
  NS_IMETHOD_(MozExternalRefCountType) Release(void) override;

  NS_DECL_NSIDIRECTORYSERVICEPROVIDER
  NS_DECL_NSIDIRECTORYSERVICEPROVIDER2
  NS_DECL_NSIXREDIRPROVIDER
  NS_DECL_NSIPROFILESTARTUP

  nsXREDirProvider();

  // if aXULAppDir is null, use gArgv[0]
  nsresult Initialize(nsIFile* aXULAppDir, nsIFile* aGREDir,
                      nsIDirectoryServiceProvider* aAppProvider = nullptr);
  ~nsXREDirProvider();

  static already_AddRefed<nsXREDirProvider> GetSingleton();

  nsresult GetUserProfilesRootDir(nsIFile** aResult);
  nsresult GetUserProfilesLocalDir(nsIFile** aResult);

  nsresult GetLegacyInstallHash(nsAString& aPathHash);

  // We only set the profile dir, we don't ensure that it exists;
  // that is the responsibility of the toolkit profile service.
  // We also don't fire profile-changed notifications... that is
  // the responsibility of the apprunner.
  nsresult SetProfile(nsIFile* aProfileDir, nsIFile* aProfileLocalDir);

  void InitializeUserPrefs();
  void FinishInitializingUserPrefs();

  void DoShutdown();

  static nsresult GetUserAppDataDirectory(nsIFile** aFile) {
    return GetUserDataDirectory(aFile, false);
  }
  static nsresult GetUserLocalDataDirectory(nsIFile** aFile) {
    return GetUserDataDirectory(aFile, true);
  }

  // GetUserDataDirectory gets the profile path from gAppData.
  static nsresult GetUserDataDirectory(nsIFile** aFile, bool aLocal);

  /* make sure you clone it, if you need to do stuff to it */
  nsIFile* GetGREDir() { return mGREDir; }
  nsIFile* GetGREBinDir() { return mGREBinDir; }
  nsIFile* GetAppDir() {
    if (mXULAppDir) return mXULAppDir;
    return mGREDir;
  }

  /**
   * Get the directory under which update directory is created.
   * This method may be called before XPCOM is started. aResult
   * is a clone, it may be modified.
   *
   * If aGetOldLocation is true, this function will return the location of
   * the update directory before it was moved from the user profile directory
   * to a per-installation directory. This functionality is only meant to be
   * used for migration of the update directory to the new location. It is only
   * valid to request the old update location on Windows, since that is the only
   * platform on which the update directory was migrated.
   */
  nsresult GetUpdateRootDir(nsIFile** aResult, bool aGetOldLocation = false);

  /**
   * Get the profile startup directory as determined by this class or by
   * mAppProvider. This method may be called before XPCOM is started. aResult
   * is a clone, it may be modified.
   */
  nsresult GetProfileStartupDir(nsIFile** aResult);

  /**
   * Get the profile directory as determined by this class or by an
   * embedder-provided XPCOM directory provider. Only call this method
   * when XPCOM is initialized! aResult is a clone, it may be modified.
   */
  nsresult GetProfileDir(nsIFile** aResult);

 protected:
  nsresult GetFilesInternal(const char* aProperty,
                            nsISimpleEnumerator** aResult);
  static nsresult GetUserDataDirectoryHome(nsIFile** aFile, bool aLocal);
  static nsresult GetSysUserExtensionsDirectory(nsIFile** aFile);
  static nsresult GetSysUserExtensionsDevDirectory(nsIFile** aFile);
#if defined(XP_UNIX) || defined(XP_MACOSX)
  static nsresult GetSystemExtensionsDirectory(nsIFile** aFile);
#endif
  static nsresult EnsureDirectoryExists(nsIFile* aDirectory);

  // Determine the profile path within the UAppData directory. This is different
  // on every major platform.
  static nsresult AppendProfilePath(nsIFile* aFile, bool aLocal);

  static nsresult AppendSysUserExtensionPath(nsIFile* aFile);
  static nsresult AppendSysUserExtensionsDevPath(nsIFile* aFile);

  // Internal helper that splits a path into components using the '/' and '\\'
  // delimiters.
  static inline nsresult AppendProfileString(nsIFile* aFile, const char* aPath);

#if defined(MOZ_SANDBOX)
  // Load the temp directory for sandboxed content processes
  nsresult LoadContentProcessTempDir();
  nsresult LoadPluginProcessTempDir();
#endif

  void Append(nsIFile* aDirectory);

  nsCOMPtr<nsIDirectoryServiceProvider> mAppProvider;
  // On OSX, mGREDir points to .app/Contents/Resources
  nsCOMPtr<nsIFile> mGREDir;
  // On OSX, mGREBinDir points to .app/Contents/MacOS
  nsCOMPtr<nsIFile> mGREBinDir;
  // On OSX, mXULAppDir points to .app/Contents/Resources/browser
  nsCOMPtr<nsIFile> mXULAppDir;
  nsCOMPtr<nsIFile> mProfileDir;
  nsCOMPtr<nsIFile> mProfileLocalDir;
  bool mProfileNotified;
  bool mPrefsInitialized = false;
#if defined(MOZ_SANDBOX)
  nsCOMPtr<nsIFile> mContentTempDir;
  nsCOMPtr<nsIFile> mContentProcessSandboxTempDir;
  nsCOMPtr<nsIFile> mPluginTempDir;
  nsCOMPtr<nsIFile> mPluginProcessSandboxTempDir;
#endif

 private:
  static nsresult SetUserDataProfileDirectory(nsCOMPtr<nsIFile>& aFile,
                                              bool aLocal);
};

#endif
