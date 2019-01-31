
/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsToolkitProfileService_h
#define nsToolkitProfileService_h

#include "nsIToolkitProfileService.h"
#include "nsIToolkitProfile.h"
#include "nsIFactory.h"
#include "nsSimpleEnumerator.h"
#include "nsProfileLock.h"

class nsToolkitProfile final : public nsIToolkitProfile {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITOOLKITPROFILE

  friend class nsToolkitProfileService;
  RefPtr<nsToolkitProfile> mNext;
  nsToolkitProfile* mPrev;

 private:
  ~nsToolkitProfile() = default;

  nsToolkitProfile(const nsACString& aName, nsIFile* aRootDir,
                   nsIFile* aLocalDir, nsToolkitProfile* aPrev);

  nsresult RemoveInternal(bool aRemoveFiles, bool aInBackground);

  friend class nsToolkitProfileLock;

  nsCString mName;
  nsCOMPtr<nsIFile> mRootDir;
  nsCOMPtr<nsIFile> mLocalDir;
  nsIProfileLock* mLock;
};

class nsToolkitProfileLock final : public nsIProfileLock {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROFILELOCK

  nsresult Init(nsToolkitProfile* aProfile, nsIProfileUnlocker** aUnlocker);
  nsresult Init(nsIFile* aDirectory, nsIFile* aLocalDirectory,
                nsIProfileUnlocker** aUnlocker);

  nsToolkitProfileLock() = default;

 private:
  ~nsToolkitProfileLock();

  RefPtr<nsToolkitProfile> mProfile;
  nsCOMPtr<nsIFile> mDirectory;
  nsCOMPtr<nsIFile> mLocalDirectory;

  nsProfileLock mLock;
};

class nsToolkitProfileFactory final : public nsIFactory {
  ~nsToolkitProfileFactory() = default;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIFACTORY
};

class nsToolkitProfileService final : public nsIToolkitProfileService {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITOOLKITPROFILESERVICE

  nsresult SelectStartupProfile(int* aArgc, char* aArgv[], bool aIsResetting,
                                nsIFile** aRootDir, nsIFile** aLocalDir,
                                nsIToolkitProfile** aProfile, bool* aDidCreate);

 private:
  friend class nsToolkitProfile;
  friend class nsToolkitProfileFactory;
  friend nsresult NS_NewToolkitProfileService(nsIToolkitProfileService**);

  nsToolkitProfileService();
  ~nsToolkitProfileService();

  nsresult Init();

  nsresult CreateTimesInternal(nsIFile* profileDir);
  void GetProfileByDir(nsIFile* aRootDir, nsIFile* aLocalDir,
                       nsIToolkitProfile** aResult);

  bool mStartupProfileSelected;
  RefPtr<nsToolkitProfile> mFirst;
  nsCOMPtr<nsIToolkitProfile> mChosen;
  nsCOMPtr<nsIToolkitProfile> mDefault;
  nsCOMPtr<nsIFile> mAppData;
  nsCOMPtr<nsIFile> mTempData;
  nsCOMPtr<nsIFile> mListFile;
  bool mStartWithLast;
  bool mIsFirstRun;

  static nsToolkitProfileService* gService;

  class ProfileEnumerator final : public nsSimpleEnumerator {
   public:
    NS_DECL_NSISIMPLEENUMERATOR

    const nsID& DefaultInterface() override {
      return NS_GET_IID(nsIToolkitProfile);
    }

    explicit ProfileEnumerator(nsToolkitProfile* first) { mCurrent = first; }

   private:
    RefPtr<nsToolkitProfile> mCurrent;
  };
};

#endif
