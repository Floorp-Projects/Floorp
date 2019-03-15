/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/UniquePtr.h"

#include <stdio.h>
#include <stdlib.h>
#include <prprf.h>
#include <prtime.h>

#ifdef XP_WIN
#  include <windows.h>
#  include <shlobj.h>
#endif
#ifdef XP_UNIX
#  include <unistd.h>
#endif

#include "nsToolkitProfileService.h"
#include "CmdLineAndEnvUtils.h"
#include "nsIFile.h"

#ifdef XP_MACOSX
#  include <CoreFoundation/CoreFoundation.h>
#  include "nsILocalFileMac.h"
#endif

#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "nsNetCID.h"
#include "nsXULAppAPI.h"
#include "nsThreadUtils.h"

#include "nsIRunnable.h"
#include "nsXREDirProvider.h"
#include "nsAppRunner.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsNativeCharsetUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/Sprintf.h"
#include "nsPrintfCString.h"
#include "mozilla/UniquePtr.h"
#include "nsIToolkitShellService.h"
#include "mozilla/Telemetry.h"

using namespace mozilla;

#define DEV_EDITION_NAME "dev-edition-default"
#define DEFAULT_NAME "default"
#define COMPAT_FILE NS_LITERAL_STRING("compatibility.ini")

nsToolkitProfile::nsToolkitProfile(const nsACString& aName, nsIFile* aRootDir,
                                   nsIFile* aLocalDir, nsToolkitProfile* aPrev)
    : mPrev(aPrev),
      mName(aName),
      mRootDir(aRootDir),
      mLocalDir(aLocalDir),
      mLock(nullptr) {
  NS_ASSERTION(aRootDir, "No file!");

  if (aPrev) {
    aPrev->mNext = this;
  } else {
    nsToolkitProfileService::gService->mFirst = this;
  }
}

NS_IMPL_ISUPPORTS(nsToolkitProfile, nsIToolkitProfile)

NS_IMETHODIMP
nsToolkitProfile::GetRootDir(nsIFile** aResult) {
  NS_ADDREF(*aResult = mRootDir);
  return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfile::GetLocalDir(nsIFile** aResult) {
  NS_ADDREF(*aResult = mLocalDir);
  return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfile::GetName(nsACString& aResult) {
  aResult = mName;
  return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfile::SetName(const nsACString& aName) {
  NS_ASSERTION(nsToolkitProfileService::gService, "Where did my service go?");

  if (mName.Equals(aName)) {
    return NS_OK;
  }

  // Changing the name from the dev-edition default profile name makes this
  // profile no longer the dev-edition default.
  if (mName.EqualsLiteral(DEV_EDITION_NAME) &&
      nsToolkitProfileService::gService->mDevEditionDefault == this) {
    nsToolkitProfileService::gService->mDevEditionDefault = nullptr;
  }

  mName = aName;

  // Setting the name to the dev-edition default profile name will cause this
  // profile to become the dev-edition default.
  if (aName.EqualsLiteral(DEV_EDITION_NAME) &&
      !nsToolkitProfileService::gService->mDevEditionDefault) {
    nsToolkitProfileService::gService->mDevEditionDefault = this;
  }

  return NS_OK;
}

nsresult nsToolkitProfile::RemoveInternal(bool aRemoveFiles,
                                          bool aInBackground) {
  NS_ASSERTION(nsToolkitProfileService::gService, "Whoa, my service is gone.");

  if (mLock) return NS_ERROR_FILE_IS_LOCKED;

  if (!mPrev && !mNext && nsToolkitProfileService::gService->mFirst != this)
    return NS_ERROR_NOT_INITIALIZED;

  if (aRemoveFiles) {
    // Check if another instance is using this profile.
    nsCOMPtr<nsIProfileLock> lock;
    nsresult rv = Lock(nullptr, getter_AddRefs(lock));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIFile> rootDir(mRootDir);
    nsCOMPtr<nsIFile> localDir(mLocalDir);

    nsCOMPtr<nsIRunnable> runnable = NS_NewRunnableFunction(
        "nsToolkitProfile::RemoveInternal", [rootDir, localDir, lock]() {
          bool equals;
          nsresult rv = rootDir->Equals(localDir, &equals);
          // The root dir might contain the temp dir, so remove
          // the temp dir first.
          if (NS_SUCCEEDED(rv) && !equals) {
            localDir->Remove(true);
          }

          // Ideally we'd unlock after deleting but since the lock is a file
          // in the profile we must unlock before removing.
          lock->Unlock();

          rootDir->Remove(true);
        });

    if (aInBackground) {
      nsCOMPtr<nsIEventTarget> target =
          do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
      target->Dispatch(runnable, NS_DISPATCH_NORMAL);
    } else {
      runnable->Run();
    }
  }

  if (mPrev)
    mPrev->mNext = mNext;
  else
    nsToolkitProfileService::gService->mFirst = mNext;

  if (mNext) mNext->mPrev = mPrev;

  mPrev = nullptr;
  mNext = nullptr;

  if (nsToolkitProfileService::gService->mNormalDefault == this) {
    nsToolkitProfileService::gService->mNormalDefault = nullptr;
  }
  if (nsToolkitProfileService::gService->mDevEditionDefault == this) {
    nsToolkitProfileService::gService->mDevEditionDefault = nullptr;
  }
  if (nsToolkitProfileService::gService->mDedicatedProfile == this) {
    nsToolkitProfileService::gService->SetDefaultProfile(nullptr);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfile::Remove(bool removeFiles) {
  return RemoveInternal(removeFiles, false /* in background */);
}

NS_IMETHODIMP
nsToolkitProfile::RemoveInBackground(bool removeFiles) {
  return RemoveInternal(removeFiles, true /* in background */);
}

NS_IMETHODIMP
nsToolkitProfile::Lock(nsIProfileUnlocker** aUnlocker,
                       nsIProfileLock** aResult) {
  if (mLock) {
    NS_ADDREF(*aResult = mLock);
    return NS_OK;
  }

  RefPtr<nsToolkitProfileLock> lock = new nsToolkitProfileLock();
  if (!lock) return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = lock->Init(this, aUnlocker);
  if (NS_FAILED(rv)) return rv;

  NS_ADDREF(*aResult = lock);
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsToolkitProfileLock, nsIProfileLock)

nsresult nsToolkitProfileLock::Init(nsToolkitProfile* aProfile,
                                    nsIProfileUnlocker** aUnlocker) {
  nsresult rv;
  rv = Init(aProfile->mRootDir, aProfile->mLocalDir, aUnlocker);
  if (NS_SUCCEEDED(rv)) mProfile = aProfile;

  return rv;
}

nsresult nsToolkitProfileLock::Init(nsIFile* aDirectory,
                                    nsIFile* aLocalDirectory,
                                    nsIProfileUnlocker** aUnlocker) {
  nsresult rv;

  rv = mLock.Lock(aDirectory, aUnlocker);

  if (NS_SUCCEEDED(rv)) {
    mDirectory = aDirectory;
    mLocalDirectory = aLocalDirectory;
  }

  return rv;
}

NS_IMETHODIMP
nsToolkitProfileLock::GetDirectory(nsIFile** aResult) {
  if (!mDirectory) {
    NS_ERROR("Not initialized, or unlocked!");
    return NS_ERROR_NOT_INITIALIZED;
  }

  NS_ADDREF(*aResult = mDirectory);
  return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfileLock::GetLocalDirectory(nsIFile** aResult) {
  if (!mLocalDirectory) {
    NS_ERROR("Not initialized, or unlocked!");
    return NS_ERROR_NOT_INITIALIZED;
  }

  NS_ADDREF(*aResult = mLocalDirectory);
  return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfileLock::Unlock() {
  if (!mDirectory) {
    NS_ERROR("Unlocking a never-locked nsToolkitProfileLock!");
    return NS_ERROR_UNEXPECTED;
  }

  mLock.Unlock();

  if (mProfile) {
    mProfile->mLock = nullptr;
    mProfile = nullptr;
  }
  mDirectory = nullptr;
  mLocalDirectory = nullptr;

  return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfileLock::GetReplacedLockTime(PRTime* aResult) {
  mLock.GetReplacedLockTime(aResult);
  return NS_OK;
}

nsToolkitProfileLock::~nsToolkitProfileLock() {
  if (mDirectory) {
    Unlock();
  }
}

nsToolkitProfileService* nsToolkitProfileService::gService = nullptr;

NS_IMPL_ISUPPORTS(nsToolkitProfileService, nsIToolkitProfileService)

nsToolkitProfileService::nsToolkitProfileService()
    : mStartupProfileSelected(false),
      mStartWithLast(true),
      mIsFirstRun(true),
      mUseDevEditionProfile(false),
#ifdef MOZ_DEDICATED_PROFILES
      mUseDedicatedProfile(!IsSnapEnvironment()),
#else
      mUseDedicatedProfile(false),
#endif
      mCreatedAlternateProfile(false),
      mStartupReason(NS_LITERAL_STRING("unknown")),
      mMaybeLockProfile(false) {
#ifdef MOZ_DEV_EDITION
  mUseDevEditionProfile = true;
#endif
  gService = this;
}

nsToolkitProfileService::~nsToolkitProfileService() { gService = nullptr; }

void nsToolkitProfileService::CompleteStartup() {
  if (!mStartupProfileSelected) {
    return;
  }

  ScalarSet(mozilla::Telemetry::ScalarID::STARTUP_PROFILE_SELECTION_REASON,
            mStartupReason);

  if (mMaybeLockProfile) {
    nsCOMPtr<nsIToolkitShellService> shell =
        do_GetService(NS_TOOLKITSHELLSERVICE_CONTRACTID);
    if (!shell) {
      return;
    }

    bool isDefaultApp;
    nsresult rv = shell->IsDefaultApplication(&isDefaultApp);
    NS_ENSURE_SUCCESS_VOID(rv);

    if (isDefaultApp) {
      mInstallData.SetString(mInstallHash.get(), "Locked", "1");
      Flush();
    }
  }
}

// Tests whether the passed profile was last used by this install.
bool nsToolkitProfileService::IsProfileForCurrentInstall(
    nsIToolkitProfile* aProfile) {
  nsCOMPtr<nsIFile> profileDir;
  nsresult rv = aProfile->GetRootDir(getter_AddRefs(profileDir));
  NS_ENSURE_SUCCESS(rv, false);

  nsCOMPtr<nsIFile> compatFile;
  rv = profileDir->Clone(getter_AddRefs(compatFile));
  NS_ENSURE_SUCCESS(rv, false);

  rv = compatFile->Append(COMPAT_FILE);
  NS_ENSURE_SUCCESS(rv, false);

  nsINIParser compatData;
  rv = compatData.Init(compatFile);
  NS_ENSURE_SUCCESS(rv, false);

  /**
   * In xpcshell gDirServiceProvider doesn't have all the correct directories
   * set so using NS_GetSpecialDirectory works better there. But in a normal
   * app launch the component registry isn't initialized so
   * NS_GetSpecialDirectory doesn't work. So we have to use two different
   * paths to support testing.
   */
  nsCOMPtr<nsIFile> currentGreDir;
  rv = NS_GetSpecialDirectory(NS_GRE_DIR, getter_AddRefs(currentGreDir));
  if (rv == NS_ERROR_NOT_INITIALIZED) {
    currentGreDir = gDirServiceProvider->GetGREDir();
    MOZ_ASSERT(currentGreDir, "No GRE dir found.");
  } else if (NS_FAILED(rv)) {
    return false;
  }

  nsCString greDirPath;
  rv = compatData.GetString("Compatibility", "LastPlatformDir", greDirPath);
  // If this string is missing then this profile is from an ancient version.
  // We'll opt to use it in this case.
  if (NS_FAILED(rv)) {
    return true;
  }

  nsCOMPtr<nsIFile> greDir;
  rv = NS_NewNativeLocalFile(EmptyCString(), false, getter_AddRefs(greDir));
  NS_ENSURE_SUCCESS(rv, false);

  rv = greDir->SetPersistentDescriptor(greDirPath);
  NS_ENSURE_SUCCESS(rv, false);

  bool equal;
  rv = greDir->Equals(currentGreDir, &equal);
  NS_ENSURE_SUCCESS(rv, false);

  return equal;
}

/**
 * Used the first time an install with dedicated profile support runs. Decides
 * whether to mark the passed profile as the default for this install.
 *
 * The goal is to reduce disruption but ideally end up with the OS default
 * install using the old default profile.
 *
 * If the decision is to use the profile then it will be unassigned as the
 * dedicated default for other installs.
 *
 * We won't attempt to use the profile if it was last used by a different
 * install.
 *
 * If the profile is currently in use by an install that was either the OS
 * default install or the profile has been explicitely chosen by some other
 * means then we won't use it.
 *
 * Returns true if we chose to make the profile the new dedicated default.
 */
bool nsToolkitProfileService::MaybeMakeDefaultDedicatedProfile(
    nsIToolkitProfile* aProfile) {
  nsresult rv;

  // If the profile was last used by a different install then we won't use it.
  if (!IsProfileForCurrentInstall(aProfile)) {
    return false;
  }

  nsCString descriptor;
  rv = GetProfileDescriptor(aProfile, descriptor, nullptr);
  NS_ENSURE_SUCCESS(rv, false);

  // Get a list of all the installs.
  nsTArray<nsCString> installs = GetKnownInstalls();

  // Cache the installs that use the profile.
  nsTArray<nsCString> inUseInstalls;

  // See if the profile is already in use by an install that hasn't locked it.
  for (uint32_t i = 0; i < installs.Length(); i++) {
    const nsCString& install = installs[i];

    nsCString path;
    rv = mInstallData.GetString(install.get(), "Default", path);
    if (NS_FAILED(rv)) {
      continue;
    }

    // Is this install using the profile we care about?
    if (!descriptor.Equals(path)) {
      continue;
    }

    // Is this profile locked to this other install?
    nsCString isLocked;
    rv = mInstallData.GetString(install.get(), "Locked", isLocked);
    if (NS_SUCCEEDED(rv) && isLocked.Equals("1")) {
      return false;
    }

    inUseInstalls.AppendElement(install);
  }

  // At this point we've decided to take the profile. Strip it from other
  // installs.
  for (uint32_t i = 0; i < inUseInstalls.Length(); i++) {
    // Removing the default setting entirely will make the install go through
    // the first run process again at startup and create itself a new profile.
    mInstallData.DeleteString(inUseInstalls[i].get(), "Default");
  }

  // Set this as the default profile for this install.
  SetDefaultProfile(aProfile);

  // SetDefaultProfile will have locked this profile to this install so no
  // other installs will steal it, but this was auto-selected so we want to
  // unlock it so that other installs can potentially take it.
  mInstallData.DeleteString(mInstallHash.get(), "Locked");

  // Persist the changes.
  Flush();

  // Once XPCOM is available check if this is the default application and if so
  // lock the profile again.
  mMaybeLockProfile = true;

  return true;
}

nsresult nsToolkitProfileService::Init() {
  NS_ASSERTION(gDirServiceProvider, "No dirserviceprovider!");
  nsresult rv;

  rv = nsXREDirProvider::GetUserAppDataDirectory(getter_AddRefs(mAppData));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = nsXREDirProvider::GetUserLocalDataDirectory(getter_AddRefs(mTempData));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString installProfilePath;

  if (mUseDedicatedProfile) {
    // Load the dedicated profiles database.
    rv = mAppData->Clone(getter_AddRefs(mInstallFile));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mInstallFile->AppendNative(NS_LITERAL_CSTRING("installs.ini"));
    NS_ENSURE_SUCCESS(rv, rv);

    nsString installHash;
    rv = gDirServiceProvider->GetInstallHash(installHash);
    NS_ENSURE_SUCCESS(rv, rv);
    CopyUTF16toUTF8(installHash, mInstallHash);

    rv = mInstallData.Init(mInstallFile);
    if (NS_SUCCEEDED(rv)) {
      // Try to find the descriptor for the default profile for this install.
      rv = mInstallData.GetString(mInstallHash.get(), "Default",
                                  installProfilePath);
      // Not having a value means this install doesn't appear in installs.ini so
      // this is the first run for this install.
      mIsFirstRun = NS_FAILED(rv);
    }
  }

  rv = mAppData->Clone(getter_AddRefs(mListFile));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mListFile->AppendNative(NS_LITERAL_CSTRING("profiles.ini"));
  NS_ENSURE_SUCCESS(rv, rv);

  nsINIParser parser;

  bool exists;
  rv = mListFile->IsFile(&exists);
  if (NS_SUCCEEDED(rv) && exists) {
    rv = parser.Init(mListFile);
    // Init does not fail on parsing errors, only on OOM/really unexpected
    // conditions.
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  nsAutoCString buffer;
  rv = parser.GetString("General", "StartWithLastProfile", buffer);
  if (NS_SUCCEEDED(rv) && buffer.EqualsLiteral("0")) mStartWithLast = false;

  nsToolkitProfile* currentProfile = nullptr;

#ifdef MOZ_DEV_EDITION
  nsCOMPtr<nsIFile> ignoreDevEditionProfile;
  rv = mAppData->Clone(getter_AddRefs(ignoreDevEditionProfile));
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = ignoreDevEditionProfile->AppendNative(
      NS_LITERAL_CSTRING("ignore-dev-edition-profile"));
  if (NS_FAILED(rv)) {
    return rv;
  }

  bool shouldIgnoreSeparateProfile;
  rv = ignoreDevEditionProfile->Exists(&shouldIgnoreSeparateProfile);
  if (NS_FAILED(rv)) return rv;

  mUseDevEditionProfile = !shouldIgnoreSeparateProfile;
#endif

  nsCOMPtr<nsIToolkitProfile> autoSelectProfile;

  unsigned int nonDevEditionProfiles = 0;
  unsigned int c = 0;
  for (c = 0; true; ++c) {
    nsAutoCString profileID("Profile");
    profileID.AppendInt(c);

    rv = parser.GetString(profileID.get(), "IsRelative", buffer);
    if (NS_FAILED(rv)) break;

    bool isRelative = buffer.EqualsLiteral("1");

    nsAutoCString filePath;

    rv = parser.GetString(profileID.get(), "Path", filePath);
    if (NS_FAILED(rv)) {
      NS_ERROR("Malformed profiles.ini: Path= not found");
      continue;
    }

    nsAutoCString name;

    rv = parser.GetString(profileID.get(), "Name", name);
    if (NS_FAILED(rv)) {
      NS_ERROR("Malformed profiles.ini: Name= not found");
      continue;
    }

    nsCOMPtr<nsIFile> rootDir;
    rv = NS_NewNativeLocalFile(EmptyCString(), true, getter_AddRefs(rootDir));
    NS_ENSURE_SUCCESS(rv, rv);

    if (isRelative) {
      rv = rootDir->SetRelativeDescriptor(mAppData, filePath);
    } else {
      rv = rootDir->SetPersistentDescriptor(filePath);
    }
    if (NS_FAILED(rv)) continue;

    nsCOMPtr<nsIFile> localDir;
    if (isRelative) {
      rv =
          NS_NewNativeLocalFile(EmptyCString(), true, getter_AddRefs(localDir));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = localDir->SetRelativeDescriptor(mTempData, filePath);
    } else {
      localDir = rootDir;
    }

    currentProfile =
        new nsToolkitProfile(name, rootDir, localDir, currentProfile);
    NS_ENSURE_TRUE(currentProfile, NS_ERROR_OUT_OF_MEMORY);

    rv = parser.GetString(profileID.get(), "Default", buffer);
    if (NS_SUCCEEDED(rv) && buffer.EqualsLiteral("1")) {
      mNormalDefault = currentProfile;
    }

    // Is this the default profile for this install?
    if (mUseDedicatedProfile && !mDedicatedProfile &&
        installProfilePath.Equals(filePath)) {
      // Found a profile for this install.
      mDedicatedProfile = currentProfile;
    }

    if (name.EqualsLiteral(DEV_EDITION_NAME)) {
      mDevEditionDefault = currentProfile;
    } else {
      nonDevEditionProfiles++;
      autoSelectProfile = currentProfile;
    }
  }

  // If there is only one non-dev-edition profile then mark it as the default.
  if (!mNormalDefault && nonDevEditionProfiles == 1) {
    mNormalDefault = autoSelectProfile;
  }

  if (!mUseDedicatedProfile) {
    if (mUseDevEditionProfile) {
      // When using the separate dev-edition profile not finding it means this
      // is a first run.
      mIsFirstRun = !mDevEditionDefault;
    } else {
      // If there are no normal profiles then this is a first run.
      mIsFirstRun = nonDevEditionProfiles == 0;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfileService::SetStartWithLastProfile(bool aValue) {
  if (mStartWithLast != aValue) {
    mStartWithLast = aValue;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfileService::GetStartWithLastProfile(bool* aResult) {
  *aResult = mStartWithLast;
  return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfileService::GetProfiles(nsISimpleEnumerator** aResult) {
  *aResult = new ProfileEnumerator(this->mFirst);
  if (!*aResult) return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfileService::ProfileEnumerator::HasMoreElements(bool* aResult) {
  *aResult = mCurrent ? true : false;
  return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfileService::ProfileEnumerator::GetNext(nsISupports** aResult) {
  if (!mCurrent) return NS_ERROR_FAILURE;

  NS_ADDREF(*aResult = mCurrent);

  mCurrent = mCurrent->mNext;
  return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfileService::GetCurrentProfile(nsIToolkitProfile** aResult) {
  NS_IF_ADDREF(*aResult = mCurrent);
  return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfileService::GetDefaultProfile(nsIToolkitProfile** aResult) {
  if (mUseDedicatedProfile) {
    NS_IF_ADDREF(*aResult = mDedicatedProfile);
    return NS_OK;
  }

  if (mUseDevEditionProfile) {
    NS_IF_ADDREF(*aResult = mDevEditionDefault);
    return NS_OK;
  }

  NS_IF_ADDREF(*aResult = mNormalDefault);
  return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfileService::SetDefaultProfile(nsIToolkitProfile* aProfile) {
  if (mUseDedicatedProfile) {
    if (mDedicatedProfile != aProfile) {
      if (!aProfile) {
        // Setting this to the empty string means no profile will be found on
        // startup but we'll recognise that this install has been used
        // previously.
        mInstallData.SetString(mInstallHash.get(), "Default", "");
      } else {
        nsCString profilePath;
        nsresult rv = GetProfileDescriptor(aProfile, profilePath, nullptr);
        NS_ENSURE_SUCCESS(rv, rv);

        mInstallData.SetString(mInstallHash.get(), "Default",
                               profilePath.get());
      }
      mDedicatedProfile = aProfile;

      // Some kind of choice has happened here, lock this profile to this
      // install.
      mInstallData.SetString(mInstallHash.get(), "Locked", "1");
    }
    return NS_OK;
  }

  if (mUseDevEditionProfile && aProfile != mDevEditionDefault) {
    // The separate profile is hardcoded.
    return NS_ERROR_FAILURE;
  }

  mNormalDefault = aProfile;
  return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfileService::GetCreatedAlternateProfile(bool* aResult) {
  *aResult = mCreatedAlternateProfile;
  return NS_OK;
}

// Gets the profile root directory descriptor for storing in profiles.ini or
// installs.ini.
nsresult nsToolkitProfileService::GetProfileDescriptor(
    nsIToolkitProfile* aProfile, nsACString& aDescriptor, bool* aIsRelative) {
  nsCOMPtr<nsIFile> profileDir;
  nsresult rv = aProfile->GetRootDir(getter_AddRefs(profileDir));
  NS_ENSURE_SUCCESS(rv, rv);

  // if the profile dir is relative to appdir...
  bool isRelative;
  rv = mAppData->Contains(profileDir, &isRelative);

  nsCString profilePath;
  if (NS_SUCCEEDED(rv) && isRelative) {
    // we use a relative descriptor
    rv = profileDir->GetRelativeDescriptor(mAppData, profilePath);
  } else {
    // otherwise, a persistent descriptor
    rv = profileDir->GetPersistentDescriptor(profilePath);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  aDescriptor.Assign(profilePath);
  if (aIsRelative) {
    *aIsRelative = isRelative;
  }

  return NS_OK;
}

nsresult nsToolkitProfileService::CreateDefaultProfile(
    nsIToolkitProfile** aResult) {
  // Create a new default profile
  nsAutoCString name;
  if (mUseDevEditionProfile) {
    name.AssignLiteral(DEV_EDITION_NAME);
  } else if (mUseDedicatedProfile) {
    name.AssignLiteral("default-" NS_STRINGIFY(MOZ_UPDATE_CHANNEL));
  } else {
    name.AssignLiteral(DEFAULT_NAME);
  }

  nsresult rv = CreateUniqueProfile(nullptr, name, aResult);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mUseDedicatedProfile) {
    SetDefaultProfile(mCurrent);
  } else if (mUseDevEditionProfile) {
    mDevEditionDefault = mCurrent;
  } else {
    mNormalDefault = mCurrent;
  }

  return NS_OK;
}

/**
 * An implementation of SelectStartupProfile callable from JavaScript via XPCOM.
 * See nsIToolkitProfileService.idl.
 */
NS_IMETHODIMP
nsToolkitProfileService::SelectStartupProfile(
    const nsTArray<nsCString>& aArgv, bool aIsResetting, nsIFile** aRootDir,
    nsIFile** aLocalDir, nsIToolkitProfile** aProfile, bool* aDidCreate) {
  int argc = aArgv.Length();
  // Our command line handling expects argv to be null-terminated so construct
  // an appropriate array.
  auto argv = MakeUnique<char*[]>(argc + 1);
  // Also, our command line handling removes things from the array without
  // freeing them so keep track of what we've created separately.
  auto allocated = MakeUnique<UniqueFreePtr<char>[]>(argc);

  for (int i = 0; i < argc; i++) {
    allocated[i].reset(ToNewCString(aArgv[i]));
    argv[i] = allocated[i].get();
  }
  argv[argc] = nullptr;

  bool wasDefault;
  nsresult rv = SelectStartupProfile(&argc, argv.get(), aIsResetting, aRootDir,
                                     aLocalDir, aProfile, aDidCreate,
                                     &wasDefault);

  // Since we were called outside of the normal startup path complete any
  // startup tasks.
  if (NS_SUCCEEDED(rv)) {
    CompleteStartup();
  }

  return rv;
}

/**
 * Selects or creates a profile to use based on the profiles database, any
 * environment variables and any command line arguments. Will not create
 * a profile if aIsResetting is true. The profile is selected based on this
 * order of preference:
 * * Environment variables (set when restarting the application).
 * * --profile command line argument.
 * * --createprofile command line argument (this also causes the app to exit).
 * * -p command line argument.
 * * A new profile created if this is the first run of the application.
 * * The default profile.
 * aRootDir and aLocalDir are set to the data and local directories for the
 * profile data. If a profile from the database was selected it will be
 * returned in aProfile.
 * aDidCreate will be set to true if a new profile was created.
 * This function should be called once at startup and will fail if called again.
 * aArgv should be an array of aArgc + 1 strings, the last element being null.
 * Both aArgv and aArgc will be mutated.
 */
nsresult nsToolkitProfileService::SelectStartupProfile(
    int* aArgc, char* aArgv[], bool aIsResetting, nsIFile** aRootDir,
    nsIFile** aLocalDir, nsIToolkitProfile** aProfile, bool* aDidCreate,
    bool* aWasDefaultSelection) {
  if (mStartupProfileSelected) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  mStartupProfileSelected = true;
  *aDidCreate = false;
  *aWasDefaultSelection = false;

  nsresult rv;
  const char* arg;

  // Use the profile specified in the environment variables (generally from an
  // app initiated restart).
  nsCOMPtr<nsIFile> lf = GetFileFromEnv("XRE_PROFILE_PATH");
  if (lf) {
    nsCOMPtr<nsIFile> localDir = GetFileFromEnv("XRE_PROFILE_LOCAL_PATH");
    if (!localDir) {
      localDir = lf;
    }

    // Clear out flags that we handled (or should have handled!) last startup.
    const char* dummy;
    CheckArg(*aArgc, aArgv, "p", &dummy);
    CheckArg(*aArgc, aArgv, "profile", &dummy);
    CheckArg(*aArgc, aArgv, "profilemanager");

    nsCOMPtr<nsIToolkitProfile> profile;
    GetProfileByDir(lf, localDir, getter_AddRefs(profile));

    if (profile && mIsFirstRun && mUseDedicatedProfile) {
      if (profile ==
          (mUseDevEditionProfile ? mDevEditionDefault : mNormalDefault)) {
        // This is the first run of a dedicated profile build where the selected
        // profile is the previous default so we should either make it the
        // default profile for this install or push the user to a new profile.

        if (MaybeMakeDefaultDedicatedProfile(profile)) {
          mStartupReason = NS_LITERAL_STRING("restart-claimed-default");

          mCurrent = profile;
        } else {
          if (aIsResetting) {
            // We don't want to create a fresh profile when we're attempting a
            // profile reset so just bail out here, the calling code will handle
            // it.
            *aProfile = nullptr;
            return NS_OK;
          }

          rv = CreateDefaultProfile(getter_AddRefs(mCurrent));
          if (NS_FAILED(rv)) {
            *aProfile = nullptr;
            return rv;
          }

          Flush();

          mStartupReason = NS_LITERAL_STRING("restart-skipped-default");
          *aDidCreate = true;
          mCreatedAlternateProfile = true;
        }

        NS_IF_ADDREF(*aProfile = mCurrent);
        mCurrent->GetRootDir(aRootDir);
        mCurrent->GetLocalDir(aLocalDir);

        return NS_OK;
      }
    }

    if (EnvHasValue("XRE_RESTARTED_BY_PROFILE_MANAGER")) {
      mStartupReason = NS_LITERAL_STRING("profile-manager");
    } else if (aIsResetting) {
      mStartupReason = NS_LITERAL_STRING("profile-reset");
    } else {
      mStartupReason = NS_LITERAL_STRING("restart");
    }

    mCurrent = profile;
    lf.forget(aRootDir);
    localDir.forget(aLocalDir);
    NS_IF_ADDREF(*aProfile = profile);
    return NS_OK;
  }

  // Check the -profile command line argument. It accepts a single argument that
  // gives the path to use for the profile.
  ArgResult ar = CheckArg(*aArgc, aArgv, "profile", &arg,
                          CheckArgFlag::CheckOSInt | CheckArgFlag::RemoveArg);
  if (ar == ARG_BAD) {
    PR_fprintf(PR_STDERR, "Error: argument --profile requires a path\n");
    return NS_ERROR_FAILURE;
  }
  if (ar) {
    nsCOMPtr<nsIFile> lf;
    rv = XRE_GetFileFromPath(arg, getter_AddRefs(lf));
    NS_ENSURE_SUCCESS(rv, rv);

    // Make sure that the profile path exists and it's a directory.
    bool exists;
    rv = lf->Exists(&exists);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!exists) {
      rv = lf->Create(nsIFile::DIRECTORY_TYPE, 0700);
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      bool isDir;
      rv = lf->IsDirectory(&isDir);
      NS_ENSURE_SUCCESS(rv, rv);
      if (!isDir) {
        PR_fprintf(
            PR_STDERR,
            "Error: argument --profile requires a path to a directory\n");
        return NS_ERROR_FAILURE;
      }
    }

    mStartupReason = NS_LITERAL_STRING("argument-profile");

    // If a profile path is specified directly on the command line, then
    // assume that the temp directory is the same as the given directory.
    GetProfileByDir(lf, lf, getter_AddRefs(mCurrent));
    NS_ADDREF(*aRootDir = lf);
    lf.forget(aLocalDir);
    NS_IF_ADDREF(*aProfile = mCurrent);
    return NS_OK;
  }

  // Check the -createprofile command line argument. It accepts a single
  // argument that is either the name for the new profile or the name followed
  // by the path to use.
  ar = CheckArg(*aArgc, aArgv, "createprofile", &arg,
                CheckArgFlag::CheckOSInt | CheckArgFlag::RemoveArg);
  if (ar == ARG_BAD) {
    PR_fprintf(PR_STDERR,
               "Error: argument --createprofile requires a profile name\n");
    return NS_ERROR_FAILURE;
  }
  if (ar) {
    const char* delim = strchr(arg, ' ');
    nsCOMPtr<nsIToolkitProfile> profile;
    if (delim) {
      nsCOMPtr<nsIFile> lf;
      rv = NS_NewNativeLocalFile(nsDependentCString(delim + 1), true,
                                 getter_AddRefs(lf));
      if (NS_FAILED(rv)) {
        PR_fprintf(PR_STDERR, "Error: profile path not valid.\n");
        return rv;
      }

      // As with --profile, assume that the given path will be used for the
      // main profile directory.
      rv = CreateProfile(lf, nsDependentCSubstring(arg, delim),
                         getter_AddRefs(profile));
    } else {
      rv = CreateProfile(nullptr, nsDependentCString(arg),
                         getter_AddRefs(profile));
    }
    // Some pathological arguments can make it this far
    if (NS_FAILED(rv)) {
      PR_fprintf(PR_STDERR, "Error creating profile.\n");
      return rv;
    }
    rv = NS_ERROR_ABORT;
    Flush();

    return rv;
  }

  // Check the -p command line argument. It either accepts a profile name and
  // uses that named profile or without a name it opens the profile manager.
  ar = CheckArg(*aArgc, aArgv, "p", &arg);
  if (ar == ARG_BAD) {
    ar = CheckArg(*aArgc, aArgv, "osint");
    if (ar == ARG_FOUND) {
      PR_fprintf(
          PR_STDERR,
          "Error: argument -p is invalid when argument --osint is specified\n");
      return NS_ERROR_FAILURE;
    }

    return NS_ERROR_SHOW_PROFILE_MANAGER;
  }
  if (ar) {
    ar = CheckArg(*aArgc, aArgv, "osint");
    if (ar == ARG_FOUND) {
      PR_fprintf(
          PR_STDERR,
          "Error: argument -p is invalid when argument --osint is specified\n");
      return NS_ERROR_FAILURE;
    }

    rv = GetProfileByName(nsDependentCString(arg), getter_AddRefs(mCurrent));
    if (NS_SUCCEEDED(rv)) {
      mStartupReason = NS_LITERAL_STRING("argument-p");

      mCurrent->GetRootDir(aRootDir);
      mCurrent->GetLocalDir(aLocalDir);

      NS_ADDREF(*aProfile = mCurrent);
      return NS_OK;
    }

    return NS_ERROR_SHOW_PROFILE_MANAGER;
  }

  ar = CheckArg(*aArgc, aArgv, "profilemanager", (const char**)nullptr,
                CheckArgFlag::CheckOSInt | CheckArgFlag::RemoveArg);
  if (ar == ARG_BAD) {
    PR_fprintf(PR_STDERR,
               "Error: argument --profilemanager is invalid when argument "
               "--osint is specified\n");
    return NS_ERROR_FAILURE;
  }
  if (ar == ARG_FOUND) {
    return NS_ERROR_SHOW_PROFILE_MANAGER;
  }

  // If this is a first run then create a new profile.
  if (mIsFirstRun) {
    if (aIsResetting) {
      // We don't want to create a fresh profile when we're attempting a
      // profile reset so just bail out here, the calling code will handle it.
      *aProfile = nullptr;
      return NS_OK;
    }

    // If we're configured to always show the profile manager then don't create
    // a new profile to use.
    if (!mStartWithLast) {
      return NS_ERROR_SHOW_PROFILE_MANAGER;
    }

    if (mUseDedicatedProfile) {
      // This is the first run of a dedicated profile install. We have to decide
      // whether to use the default profile used by non-dedicated-profile
      // installs or to create a new profile.

      // Find what would have been the default profile for old installs.
      nsCOMPtr<nsIToolkitProfile> profile = mNormalDefault;
      if (mUseDevEditionProfile) {
        profile = mDevEditionDefault;
      }

      if (profile) {
        nsCOMPtr<nsIFile> rootDir;
        profile->GetRootDir(getter_AddRefs(rootDir));

        nsCOMPtr<nsIFile> compat;
        rootDir->Clone(getter_AddRefs(compat));
        compat->Append(COMPAT_FILE);

        bool exists;
        rv = compat->Exists(&exists);
        NS_ENSURE_SUCCESS(rv, rv);

        // If the file is missing then either this is an empty profile (likely
        // generated by bug 1518591) or it is from an ancient version. We'll opt
        // to leave it for older versions in this case.
        if (exists) {
          if (MaybeMakeDefaultDedicatedProfile(profile)) {
            mStartupReason = NS_LITERAL_STRING("firstrun-claimed-default");

            mCurrent = profile;
            rootDir.forget(aRootDir);
            profile->GetLocalDir(aLocalDir);
            profile.forget(aProfile);
            return NS_OK;
          }

          // We're going to create a new profile for this install. If there was
          // a potential previous default to use then the user may be confused
          // over why we're not using that anymore so set a flag for the front
          // end to use to notify the user about what has happened.
          mCreatedAlternateProfile = true;
        }
      }
    }

    rv = CreateDefaultProfile(getter_AddRefs(mCurrent));
    if (NS_SUCCEEDED(rv)) {
      // If there is only one profile and it isn't meant to be the profile that
      // older versions of Firefox use then we must create a default profile
      // for older versions of Firefox to avoid the existing profile being
      // auto-selected.
      if ((mUseDedicatedProfile || mUseDevEditionProfile) && mFirst &&
          !mFirst->mNext) {
        CreateProfile(nullptr, NS_LITERAL_CSTRING(DEFAULT_NAME),
                      getter_AddRefs(mNormalDefault));
      }

      Flush();

      if (mCreatedAlternateProfile) {
        mStartupReason = NS_LITERAL_STRING("firstrun-skipped-default");
      } else {
        mStartupReason = NS_LITERAL_STRING("firstrun-created-default");
      }

      // Use the new profile.
      mCurrent->GetRootDir(aRootDir);
      mCurrent->GetLocalDir(aLocalDir);
      NS_ADDREF(*aProfile = mCurrent);

      *aDidCreate = true;
      return NS_OK;
    }
  }

  GetDefaultProfile(getter_AddRefs(mCurrent));

  // None of the profiles was marked as default (generally only happens if the
  // user modifies profiles.ini manually). Let the user choose.
  if (!mCurrent) {
    return NS_ERROR_SHOW_PROFILE_MANAGER;
  }

  // Let the caller know that the profile was selected by default.
  *aWasDefaultSelection = true;
  mStartupReason = NS_LITERAL_STRING("default");

  // Use the selected profile.
  mCurrent->GetRootDir(aRootDir);
  mCurrent->GetLocalDir(aLocalDir);
  NS_ADDREF(*aProfile = mCurrent);

  return NS_OK;
}

/**
 * Creates a new profile for reset and mark it as the current profile.
 */
nsresult nsToolkitProfileService::CreateResetProfile(
    nsIToolkitProfile** aNewProfile) {
  nsAutoCString oldProfileName;
  mCurrent->GetName(oldProfileName);

  nsCOMPtr<nsIToolkitProfile> newProfile;
  // Make the new profile name the old profile (or "default-") + the time in
  // seconds since epoch for uniqueness.
  nsAutoCString newProfileName;
  if (!oldProfileName.IsEmpty()) {
    newProfileName.Assign(oldProfileName);
    newProfileName.Append("-");
  } else {
    newProfileName.AssignLiteral("default-");
  }
  newProfileName.AppendPrintf("%" PRId64, PR_Now() / 1000);
  nsresult rv = CreateProfile(nullptr,  // choose a default dir for us
                              newProfileName, getter_AddRefs(newProfile));
  if (NS_FAILED(rv)) return rv;

  rv = Flush();
  if (NS_FAILED(rv)) return rv;

  mCurrent = newProfile;
  newProfile.forget(aNewProfile);

  return NS_OK;
}

/**
 * This is responsible for deleting the old profile, copying its name to the
 * current profile and if the old profile was default making the new profile
 * default as well.
 */
nsresult nsToolkitProfileService::ApplyResetProfile(
    nsIToolkitProfile* aOldProfile) {
  // If the old profile would have been the default for old installs then mark
  // the new profile as such.
  if (mNormalDefault == aOldProfile) {
    mNormalDefault = mCurrent;
  }

  if (mUseDedicatedProfile && mDedicatedProfile == aOldProfile) {
    bool wasLocked = false;
    nsCString val;
    if (NS_SUCCEEDED(
            mInstallData.GetString(mInstallHash.get(), "Locked", val))) {
      wasLocked = val.Equals("1");
    }

    SetDefaultProfile(mCurrent);

    // Make the locked state match if necessary.
    if (!wasLocked) {
      mInstallData.DeleteString(mInstallHash.get(), "Locked");
    }
  }

  nsCString name;
  nsresult rv = aOldProfile->GetName(name);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aOldProfile->Remove(false);
  NS_ENSURE_SUCCESS(rv, rv);

  // Switching the name will make this the default for dev-edition if
  // appropriate.
  rv = mCurrent->SetName(name);
  NS_ENSURE_SUCCESS(rv, rv);

  return Flush();
}

NS_IMETHODIMP
nsToolkitProfileService::GetProfileByName(const nsACString& aName,
                                          nsIToolkitProfile** aResult) {
  nsToolkitProfile* curP = mFirst;
  while (curP) {
    if (curP->mName.Equals(aName)) {
      NS_ADDREF(*aResult = curP);
      return NS_OK;
    }
    curP = curP->mNext;
  }

  return NS_ERROR_FAILURE;
}

/**
 * Finds a profile from the database that uses the given root and local
 * directories.
 */
void nsToolkitProfileService::GetProfileByDir(nsIFile* aRootDir,
                                              nsIFile* aLocalDir,
                                              nsIToolkitProfile** aResult) {
  nsToolkitProfile* curP = mFirst;
  while (curP) {
    bool equal;
    nsresult rv = curP->mRootDir->Equals(aRootDir, &equal);
    if (NS_SUCCEEDED(rv) && equal) {
      rv = curP->mLocalDir->Equals(aLocalDir, &equal);
      if (NS_SUCCEEDED(rv) && equal) {
        NS_ADDREF(*aResult = curP);
        return;
      }
    }
    curP = curP->mNext;
  }
}

nsresult NS_LockProfilePath(nsIFile* aPath, nsIFile* aTempPath,
                            nsIProfileUnlocker** aUnlocker,
                            nsIProfileLock** aResult) {
  RefPtr<nsToolkitProfileLock> lock = new nsToolkitProfileLock();
  if (!lock) return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = lock->Init(aPath, aTempPath, aUnlocker);
  if (NS_FAILED(rv)) return rv;

  lock.forget(aResult);
  return NS_OK;
}

static void SaltProfileName(nsACString& aName) {
  char salt[9];
  NS_MakeRandomString(salt, 8);
  salt[8] = '.';

  aName.Insert(salt, 0, 9);
}

NS_IMETHODIMP
nsToolkitProfileService::CreateUniqueProfile(nsIFile* aRootDir,
                                             const nsACString& aNamePrefix,
                                             nsIToolkitProfile** aResult) {
  nsCOMPtr<nsIToolkitProfile> profile;
  nsresult rv = GetProfileByName(aNamePrefix, getter_AddRefs(profile));
  if (NS_FAILED(rv)) {
    return CreateProfile(aRootDir, aNamePrefix, aResult);
  }

  uint32_t suffix = 1;
  while (true) {
    nsPrintfCString name("%s-%d", PromiseFlatCString(aNamePrefix).get(),
                         suffix);
    rv = GetProfileByName(name, getter_AddRefs(profile));
    if (NS_FAILED(rv)) {
      return CreateProfile(aRootDir, name, aResult);
    }
    suffix++;
  }
}

NS_IMETHODIMP
nsToolkitProfileService::CreateProfile(nsIFile* aRootDir,
                                       const nsACString& aName,
                                       nsIToolkitProfile** aResult) {
  nsresult rv = GetProfileByName(aName, aResult);
  if (NS_SUCCEEDED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIFile> rootDir(aRootDir);

  nsAutoCString dirName;
  if (!rootDir) {
    rv = gDirServiceProvider->GetUserProfilesRootDir(getter_AddRefs(rootDir));
    NS_ENSURE_SUCCESS(rv, rv);

    dirName = aName;
    SaltProfileName(dirName);

    if (NS_IsNativeUTF8()) {
      rootDir->AppendNative(dirName);
    } else {
      rootDir->Append(NS_ConvertUTF8toUTF16(dirName));
    }
  }

  nsCOMPtr<nsIFile> localDir;

  bool isRelative;
  rv = mAppData->Contains(rootDir, &isRelative);
  if (NS_SUCCEEDED(rv) && isRelative) {
    nsAutoCString path;
    rv = rootDir->GetRelativeDescriptor(mAppData, path);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = NS_NewNativeLocalFile(EmptyCString(), true, getter_AddRefs(localDir));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = localDir->SetRelativeDescriptor(mTempData, path);
  } else {
    localDir = rootDir;
  }

  bool exists;
  rv = rootDir->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (exists) {
    rv = rootDir->IsDirectory(&exists);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!exists) return NS_ERROR_FILE_NOT_DIRECTORY;
  } else {
    nsCOMPtr<nsIFile> profileDirParent;
    nsAutoString profileDirName;

    rv = rootDir->GetParent(getter_AddRefs(profileDirParent));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = rootDir->GetLeafName(profileDirName);
    NS_ENSURE_SUCCESS(rv, rv);

    // let's ensure that the profile directory exists.
    rv = rootDir->Create(nsIFile::DIRECTORY_TYPE, 0700);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = rootDir->SetPermissions(0700);
#ifndef ANDROID
    // If the profile is on the sdcard, this will fail but its non-fatal
    NS_ENSURE_SUCCESS(rv, rv);
#endif
  }

  rv = localDir->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!exists) {
    rv = localDir->Create(nsIFile::DIRECTORY_TYPE, 0700);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // We created a new profile dir. Let's store a creation timestamp.
  // Note that this code path does not apply if the profile dir was
  // created prior to launching.
  rv = CreateTimesInternal(rootDir);
  NS_ENSURE_SUCCESS(rv, rv);

  nsToolkitProfile* last = mFirst.get();
  if (last) {
    while (last->mNext) {
      last = last->mNext;
    }
  }

  nsCOMPtr<nsIToolkitProfile> profile =
      new nsToolkitProfile(aName, rootDir, localDir, last);
  if (!profile) return NS_ERROR_OUT_OF_MEMORY;

  if (aName.Equals(DEV_EDITION_NAME)) {
    mDevEditionDefault = profile;
  }

  profile.forget(aResult);
  return NS_OK;
}

/**
 * Snaps (https://snapcraft.io/) use a different installation directory for
 * every version of an application. Since dedicated profiles uses the
 * installation directory to determine which profile to use this would lead
 * snap users getting a new profile on every application update.
 *
 * However the only way to have multiple installation of a snap is to install
 * a new snap instance. Different snap instances have different user data
 * directories and so already will not share profiles, in fact one instance
 * will not even be able to see the other instance's profiles since
 * profiles.ini will be stored in different places.
 *
 * So we can just disable dedicated profile support in this case and revert
 * back to the old method of just having a single default profile and still
 * get essentially the same benefits as dedicated profiles provides.
 */
bool nsToolkitProfileService::IsSnapEnvironment() {
  return !!PR_GetEnv("SNAP_NAME");
}

struct FindInstallsClosure {
  nsINIParser* installData;
  nsTArray<nsCString>* installs;
};

static bool FindInstalls(const char* aSection, void* aClosure) {
  FindInstallsClosure* closure = static_cast<FindInstallsClosure*>(aClosure);

  nsCString install(aSection);
  closure->installs->AppendElement(install);

  return true;
}

nsTArray<nsCString> nsToolkitProfileService::GetKnownInstalls() {
  nsTArray<nsCString> result;
  FindInstallsClosure closure = {&mInstallData, &result};

  mInstallData.GetSections(&FindInstalls, &closure);

  return result;
}

nsresult nsToolkitProfileService::CreateTimesInternal(nsIFile* aProfileDir) {
  nsresult rv = NS_ERROR_FAILURE;
  nsCOMPtr<nsIFile> creationLog;
  rv = aProfileDir->Clone(getter_AddRefs(creationLog));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = creationLog->AppendNative(NS_LITERAL_CSTRING("times.json"));
  NS_ENSURE_SUCCESS(rv, rv);

  bool exists = false;
  creationLog->Exists(&exists);
  if (exists) {
    return NS_OK;
  }

  rv = creationLog->Create(nsIFile::NORMAL_FILE_TYPE, 0700);
  NS_ENSURE_SUCCESS(rv, rv);

  // We don't care about microsecond resolution.
  int64_t msec = PR_Now() / PR_USEC_PER_MSEC;

  // Write it out.
  PRFileDesc* writeFile;
  rv = creationLog->OpenNSPRFileDesc(PR_WRONLY, 0700, &writeFile);
  NS_ENSURE_SUCCESS(rv, rv);

  PR_fprintf(writeFile, "{\n\"created\": %lld,\n\"firstUse\": null\n}\n", msec);
  PR_Close(writeFile);
  return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfileService::GetProfileCount(uint32_t* aResult) {
  *aResult = 0;
  nsToolkitProfile* profile = mFirst;
  while (profile) {
    (*aResult)++;
    profile = profile->mNext;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfileService::Flush() {
  nsresult rv;

  if (mUseDedicatedProfile) {
    rv = mInstallData.WriteToFile(mInstallFile);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Errors during writing might cause unhappy semi-written files.
  // To avoid this, write the entire thing to a buffer, then write
  // that buffer to disk.

  uint32_t pCount = 0;
  nsToolkitProfile* cur;

  for (cur = mFirst; cur != nullptr; cur = cur->mNext) ++pCount;

  uint32_t length;
  const int bufsize = 100 + MAXPATHLEN * pCount;
  auto buffer = MakeUnique<char[]>(bufsize);

  char* pos = buffer.get();
  char* end = pos + bufsize;

  pos += snprintf(pos, end - pos,
                  "[General]\n"
                  "StartWithLastProfile=%s\n\n",
                  mStartWithLast ? "1" : "0");

  nsAutoCString path;
  cur = mFirst;
  pCount = 0;

  while (cur) {
    bool isRelative;
    nsresult rv = GetProfileDescriptor(cur, path, &isRelative);
    NS_ENSURE_SUCCESS(rv, rv);

    pos +=
        snprintf(pos, end - pos,
                 "[Profile%u]\n"
                 "Name=%s\n"
                 "IsRelative=%s\n"
                 "Path=%s\n",
                 pCount, cur->mName.get(), isRelative ? "1" : "0", path.get());

    if (cur == mNormalDefault) {
      pos += snprintf(pos, end - pos, "Default=1\n");
    }

    pos += snprintf(pos, end - pos, "\n");

    cur = cur->mNext;
    ++pCount;
  }

  FILE* writeFile;
  rv = mListFile->OpenANSIFileDesc("w", &writeFile);
  NS_ENSURE_SUCCESS(rv, rv);

  length = pos - buffer.get();

  if (fwrite(buffer.get(), sizeof(char), length, writeFile) != length) {
    fclose(writeFile);
    return NS_ERROR_UNEXPECTED;
  }

  fclose(writeFile);
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsToolkitProfileFactory, nsIFactory)

NS_IMETHODIMP
nsToolkitProfileFactory::CreateInstance(nsISupports* aOuter, const nsID& aIID,
                                        void** aResult) {
  if (aOuter) return NS_ERROR_NO_AGGREGATION;

  RefPtr<nsToolkitProfileService> profileService =
      nsToolkitProfileService::gService;
  if (!profileService) {
    nsresult rv = NS_NewToolkitProfileService(getter_AddRefs(profileService));
    if (NS_FAILED(rv)) return rv;
  }
  return profileService->QueryInterface(aIID, aResult);
}

NS_IMETHODIMP
nsToolkitProfileFactory::LockFactory(bool aVal) { return NS_OK; }

nsresult NS_NewToolkitProfileFactory(nsIFactory** aResult) {
  *aResult = new nsToolkitProfileFactory();
  if (!*aResult) return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}

nsresult NS_NewToolkitProfileService(nsToolkitProfileService** aResult) {
  nsToolkitProfileService* profileService = new nsToolkitProfileService();
  if (!profileService) return NS_ERROR_OUT_OF_MEMORY;
  nsresult rv = profileService->Init();
  if (NS_FAILED(rv)) {
    NS_ERROR("nsToolkitProfileService::Init failed!");
    delete profileService;
    return rv;
  }

  NS_ADDREF(*aResult = profileService);
  return NS_OK;
}

nsresult XRE_GetFileFromPath(const char* aPath, nsIFile** aResult) {
#if defined(XP_MACOSX)
  int32_t pathLen = strlen(aPath);
  if (pathLen > MAXPATHLEN) return NS_ERROR_INVALID_ARG;

  CFURLRef fullPath = CFURLCreateFromFileSystemRepresentation(
      nullptr, (const UInt8*)aPath, pathLen, true);
  if (!fullPath) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIFile> lf;
  nsresult rv = NS_NewNativeLocalFile(EmptyCString(), true, getter_AddRefs(lf));
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsILocalFileMac> lfMac = do_QueryInterface(lf, &rv);
    if (NS_SUCCEEDED(rv)) {
      rv = lfMac->InitWithCFURL(fullPath);
      if (NS_SUCCEEDED(rv)) {
        lf.forget(aResult);
      }
    }
  }
  CFRelease(fullPath);
  return rv;

#elif defined(XP_UNIX)
  char fullPath[MAXPATHLEN];

  if (!realpath(aPath, fullPath)) return NS_ERROR_FAILURE;

  return NS_NewNativeLocalFile(nsDependentCString(fullPath), true, aResult);
#elif defined(XP_WIN)
  WCHAR fullPath[MAXPATHLEN];

  if (!_wfullpath(fullPath, NS_ConvertUTF8toUTF16(aPath).get(), MAXPATHLEN))
    return NS_ERROR_FAILURE;

  return NS_NewLocalFile(nsDependentString(fullPath), true, aResult);

#else
#  error Platform-specific logic needed here.
#endif
}
