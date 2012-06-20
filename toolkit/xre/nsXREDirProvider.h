/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsXREDirProvider_h__
#define _nsXREDirProvider_h__

#include "nsIDirectoryService.h"
#include "nsIProfileMigrator.h"
#include "nsIFile.h"

#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "mozilla/Attributes.h"

class nsXREDirProvider MOZ_FINAL : public nsIDirectoryServiceProvider2,
                                   public nsIProfileStartup
{
public:
  // we use a custom isupports implementation (no refcount)
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);

  NS_DECL_NSIDIRECTORYSERVICEPROVIDER
  NS_DECL_NSIDIRECTORYSERVICEPROVIDER2
  NS_DECL_NSIPROFILESTARTUP

  nsXREDirProvider();

  // if aXULAppDir is null, use gArgv[0]
  nsresult Initialize(nsIFile *aXULAppDir,
                      nsIFile *aGREDir,
                      nsIDirectoryServiceProvider* aAppProvider = nsnull);
  ~nsXREDirProvider();

  static nsXREDirProvider* GetSingleton();

  nsresult GetUserProfilesRootDir(nsIFile** aResult,
                                  const nsACString* aProfileName,
                                  const nsACString* aAppName,
                                  const nsACString* aVendorName);
  nsresult GetUserProfilesLocalDir(nsIFile** aResult,
                                   const nsACString* aProfileName,
                                   const nsACString* aAppName,
                                   const nsACString* aVendorName);

  // We only set the profile dir, we don't ensure that it exists;
  // that is the responsibility of the toolkit profile service.
  // We also don't fire profile-changed notifications... that is
  // the responsibility of the apprunner.
  nsresult SetProfile(nsIFile* aProfileDir, nsIFile* aProfileLocalDir);

  void DoShutdown();

  nsresult GetProfileDefaultsDir(nsIFile* *aResult);

  static nsresult GetUserAppDataDirectory(nsIFile* *aFile) {
    return GetUserDataDirectory(aFile, false, nsnull, nsnull, nsnull);
  }
  static nsresult GetUserLocalDataDirectory(nsIFile* *aFile) {
    return GetUserDataDirectory(aFile, true, nsnull, nsnull, nsnull);
  }

  // By default GetUserDataDirectory gets profile path from gAppData,
  // but that can be overridden by using aProfileName/aAppName/aVendorName.
  static nsresult GetUserDataDirectory(nsIFile** aFile, bool aLocal,
                                       const nsACString* aProfileName,
                                       const nsACString* aAppName,
                                       const nsACString* aVendorName);

  /* make sure you clone it, if you need to do stuff to it */
  nsIFile* GetGREDir() { return mGREDir; }
  nsIFile* GetAppDir() {
    if (mXULAppDir)
      return mXULAppDir;
    return mGREDir;
  }

  /**
   * Get the directory under which update directory is created.
   * This method may be called before XPCOM is started. aResult
   * is a clone, it may be modified.
   */
  nsresult GetUpdateRootDir(nsIFile* *aResult);

  /**
   * Get the profile startup directory as determined by this class or by
   * mAppProvider. This method may be called before XPCOM is started. aResult
   * is a clone, it may be modified.
   */
  nsresult GetProfileStartupDir(nsIFile* *aResult);

  /**
   * Get the profile directory as determined by this class or by an
   * embedder-provided XPCOM directory provider. Only call this method
   * when XPCOM is initialized! aResult is a clone, it may be modified.
   */
  nsresult GetProfileDir(nsIFile* *aResult);

protected:
  nsresult GetFilesInternal(const char* aProperty, nsISimpleEnumerator** aResult);
  static nsresult GetUserDataDirectoryHome(nsIFile* *aFile, bool aLocal);
  static nsresult GetSysUserExtensionsDirectory(nsIFile* *aFile);
#if defined(XP_UNIX) || defined(XP_MACOSX)
  static nsresult GetSystemExtensionsDirectory(nsIFile** aFile);
#endif
  static nsresult EnsureDirectoryExists(nsIFile* aDirectory);
  void EnsureProfileFileExists(nsIFile* aFile);

  // Determine the profile path within the UAppData directory. This is different
  // on every major platform.
  static nsresult AppendProfilePath(nsIFile* aFile,
                                    const nsACString* aProfileName,
                                    const nsACString* aAppName,
                                    const nsACString* aVendorName);

  static nsresult AppendSysUserExtensionPath(nsIFile* aFile);

  // Internal helper that splits a path into components using the '/' and '\\'
  // delimiters.
  static inline nsresult AppendProfileString(nsIFile* aFile, const char* aPath);

  // Calculate and register extension and theme bundle directories.
  void LoadExtensionBundleDirectories();

  // Calculate and register app-bundled extension directories.
  void LoadAppBundleDirs();

  void Append(nsIFile* aDirectory);

  nsCOMPtr<nsIDirectoryServiceProvider> mAppProvider;
  nsCOMPtr<nsIFile>      mGREDir;
  nsCOMPtr<nsIFile>      mXULAppDir;
  nsCOMPtr<nsIFile>      mProfileDir;
  nsCOMPtr<nsIFile>      mProfileLocalDir;
  bool                   mProfileNotified;
  nsCOMArray<nsIFile>    mAppBundleDirectories;
  nsCOMArray<nsIFile>    mExtensionDirectories;
  nsCOMArray<nsIFile>    mThemeDirectories;
};

#endif
