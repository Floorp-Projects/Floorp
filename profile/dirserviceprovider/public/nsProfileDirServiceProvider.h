/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Interfaces Needed
#include "nsIDirectoryService.h"
#include "nsIFile.h"

#include "nsCOMPtr.h"
#include "nsDirectoryServiceUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"

#ifdef MOZILLA_INTERNAL_API
#include "nsString.h"
#else
#include "nsEmbedString.h"
#endif

// Forward Declarations
class nsProfileLock;

// --------------------------------------------------------------------------------------
// nsProfileDirServiceProvider - The nsIDirectoryServiceProvider implementation used for
// profile-relative file locations.
// --------------------------------------------------------------------------------------

class nsProfileDirServiceProvider: public nsIDirectoryServiceProvider
{  
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDIRECTORYSERVICEPROVIDER

  friend nsresult NS_NewProfileDirServiceProvider(bool, nsProfileDirServiceProvider**);

public:

   /**
    * SetProfileDir
    *
    * @param aProfileDir   The directory containing the profile files.
    *                      It does not need to exist before calling this
    *                      method. If it does not, it will be created and
    *                      defaults will be copied to it. 
    * @param aLocalProfileDir
    *                      Directory for local profile data, e.g. Cache.
    *                      If null, aProfileDir will be used for this purpose.
    */

   virtual nsresult        SetProfileDir(nsIFile* aProfileDir,
                                         nsIFile* aLocalProfileDir = nullptr);

  /**
   * Register
   *
   * Convenience method to register the provider with directory service.
   * The service holds strong references to registered providers so consumers
   * don't need to hold a reference to this object after calling Register().
   */

  virtual nsresult         Register();

  /**
   * Shutdown
   *
   * This method must be called before shutting down XPCOM if this object
   * was created with aNotifyObservers == true. If this object was
   * created with aNotifyObservers == false, this method is a no-op.
   */

  virtual nsresult         Shutdown();

protected:
                           nsProfileDirServiceProvider(bool aNotifyObservers = true);
   virtual                 ~nsProfileDirServiceProvider();

  nsresult                 Initialize();
  nsresult                 InitProfileDir(nsIFile* profileDir);
  nsresult                 InitNonSharedProfileDir();
  nsresult                 EnsureProfileFileExists(nsIFile *aFile, nsIFile *destDir);
  nsresult                 UndefineFileLocations();

protected:

  nsCOMPtr<nsIFile>        mProfileDir;
  nsCOMPtr<nsIFile>        mLocalProfileDir;
  nsProfileLock*           mProfileDirLock;
  bool                     mNotifyObservers;

  bool                     mSharingEnabled;
#ifndef MOZILLA_INTERNAL_API
  nsEmbedString            mNonSharedDirName;
#else
  nsString                 mNonSharedDirName;
#endif
  nsCOMPtr<nsIFile>        mNonSharedProfileDir;
};


// --------------------------------------------------------------------------------------

/**
 * Global method to create an instance of nsProfileDirServiceProvider
 *
 * @param aNotifyObservers    If true, will send out profile startup
 *                            notifications when the profile directory is set.
 *                            See nsIProfileChangeStatus.
 */
 
nsresult NS_NewProfileDirServiceProvider(bool aNotifyObservers,
                                         nsProfileDirServiceProvider** aProvider);

