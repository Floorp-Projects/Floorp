/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/UniquePtr.h"

#include <stdio.h>
#include <stdlib.h>
#include <prprf.h>
#include <prtime.h>
#include "nsProfileLock.h"

#ifdef XP_WIN
#include <windows.h>
#include <shlobj.h>
#endif
#ifdef XP_UNIX
#include <unistd.h>
#endif

#include "nsIToolkitProfileService.h"
#include "nsIToolkitProfile.h"
#include "nsIFactory.h"
#include "nsIFile.h"
#include "nsISimpleEnumerator.h"

#ifdef XP_MACOSX
#include <CoreFoundation/CoreFoundation.h>
#include "nsILocalFileMac.h"
#endif

#include "nsAppDirectoryServiceDefs.h"
#include "nsNetCID.h"
#include "nsXULAppAPI.h"
#include "nsThreadUtils.h"

#include "nsIRunnable.h"
#include "nsINIParser.h"
#include "nsXREDirProvider.h"
#include "nsAppRunner.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsNativeCharsetUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/Sprintf.h"

using namespace mozilla;

class nsToolkitProfile final : public nsIToolkitProfile
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSITOOLKITPROFILE

    friend class nsToolkitProfileService;
    RefPtr<nsToolkitProfile> mNext;
    nsToolkitProfile          *mPrev;

private:
    ~nsToolkitProfile() { }

    nsToolkitProfile(const nsACString& aName,
                     nsIFile* aRootDir,
                     nsIFile* aLocalDir,
                     nsToolkitProfile* aPrev);

    nsresult
    RemoveInternal(bool aRemoveFiles, bool aInBackground);

    friend class nsToolkitProfileLock;

    nsCString                  mName;
    nsCOMPtr<nsIFile>          mRootDir;
    nsCOMPtr<nsIFile>          mLocalDir;
    nsIProfileLock*            mLock;
};

class nsToolkitProfileLock final : public nsIProfileLock
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPROFILELOCK

    nsresult Init(nsToolkitProfile* aProfile, nsIProfileUnlocker* *aUnlocker);
    nsresult Init(nsIFile* aDirectory, nsIFile* aLocalDirectory,
                  nsIProfileUnlocker* *aUnlocker);

    nsToolkitProfileLock() { }

private:
    ~nsToolkitProfileLock();

    RefPtr<nsToolkitProfile> mProfile;
    nsCOMPtr<nsIFile> mDirectory;
    nsCOMPtr<nsIFile> mLocalDirectory;

    nsProfileLock mLock;
};

class nsToolkitProfileFactory final : public nsIFactory
{
    ~nsToolkitProfileFactory() {}
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIFACTORY
};

class nsToolkitProfileService final : public nsIToolkitProfileService
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSITOOLKITPROFILESERVICE

private:
    friend class nsToolkitProfile;
    friend class nsToolkitProfileFactory;
    friend nsresult NS_NewToolkitProfileService(nsIToolkitProfileService**);

    nsToolkitProfileService() :
        mStartWithLast(true),
        mStartOffline(false)
    {
        gService = this;
    }
    ~nsToolkitProfileService()
    {
        gService = nullptr;
    }

    nsresult Init();

    nsresult CreateTimesInternal(nsIFile *profileDir);

    RefPtr<nsToolkitProfile>  mFirst;
    nsCOMPtr<nsIToolkitProfile> mChosen;
    nsCOMPtr<nsIToolkitProfile> mDefault;
    nsCOMPtr<nsIFile>           mAppData;
    nsCOMPtr<nsIFile>           mTempData;
    nsCOMPtr<nsIFile>           mListFile;
    bool mStartWithLast;
    bool mStartOffline;

    static nsToolkitProfileService *gService;

    class ProfileEnumerator final : public nsISimpleEnumerator
    {
    public:
        NS_DECL_ISUPPORTS
        NS_DECL_NSISIMPLEENUMERATOR

        explicit ProfileEnumerator(nsToolkitProfile *first)
          { mCurrent = first; }
    private:
        ~ProfileEnumerator() { }
        RefPtr<nsToolkitProfile> mCurrent;
    };
};

nsToolkitProfile::nsToolkitProfile(const nsACString& aName,
                                   nsIFile* aRootDir,
                                   nsIFile* aLocalDir,
                                   nsToolkitProfile* aPrev) :
    mPrev(aPrev),
    mName(aName),
    mRootDir(aRootDir),
    mLocalDir(aLocalDir),
    mLock(nullptr)
{
    NS_ASSERTION(aRootDir, "No file!");

    if (aPrev) {
        aPrev->mNext = this;
    } else {
        nsToolkitProfileService::gService->mFirst = this;
    }
}

NS_IMPL_ISUPPORTS(nsToolkitProfile, nsIToolkitProfile)

NS_IMETHODIMP
nsToolkitProfile::GetRootDir(nsIFile* *aResult)
{
    NS_ADDREF(*aResult = mRootDir);
    return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfile::GetLocalDir(nsIFile* *aResult)
{
    NS_ADDREF(*aResult = mLocalDir);
    return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfile::GetName(nsACString& aResult)
{
    aResult = mName;
    return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfile::SetName(const nsACString& aName)
{
    NS_ASSERTION(nsToolkitProfileService::gService,
                 "Where did my service go?");

    mName = aName;

    return NS_OK;
}

nsresult
nsToolkitProfile::RemoveInternal(bool aRemoveFiles, bool aInBackground)
{
    NS_ASSERTION(nsToolkitProfileService::gService,
                 "Whoa, my service is gone.");

    if (mLock)
        return NS_ERROR_FILE_IS_LOCKED;

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
          "nsToolkitProfile::RemoveInternal",
          [rootDir, localDir, lock]() {
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
            }
        );

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

    if (mNext)
        mNext->mPrev = mPrev;

    mPrev = nullptr;
    mNext = nullptr;

    if (nsToolkitProfileService::gService->mChosen == this)
        nsToolkitProfileService::gService->mChosen = nullptr;

    return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfile::Remove(bool removeFiles)
{
    return RemoveInternal(removeFiles, false /* in background */);
}

NS_IMETHODIMP
nsToolkitProfile::RemoveInBackground(bool removeFiles)
{
    return RemoveInternal(removeFiles, true /* in background */);
}

NS_IMETHODIMP
nsToolkitProfile::Lock(nsIProfileUnlocker* *aUnlocker, nsIProfileLock* *aResult)
{
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

nsresult
nsToolkitProfileLock::Init(nsToolkitProfile* aProfile, nsIProfileUnlocker* *aUnlocker)
{
    nsresult rv;
    rv = Init(aProfile->mRootDir, aProfile->mLocalDir, aUnlocker);
    if (NS_SUCCEEDED(rv))
        mProfile = aProfile;

    return rv;
}

nsresult
nsToolkitProfileLock::Init(nsIFile* aDirectory, nsIFile* aLocalDirectory,
                           nsIProfileUnlocker* *aUnlocker)
{
    nsresult rv;

    rv = mLock.Lock(aDirectory, aUnlocker);

    if (NS_SUCCEEDED(rv)) {
        mDirectory = aDirectory;
        mLocalDirectory = aLocalDirectory;
    }

    return rv;
}

NS_IMETHODIMP
nsToolkitProfileLock::GetDirectory(nsIFile* *aResult)
{
    if (!mDirectory) {
        NS_ERROR("Not initialized, or unlocked!");
        return NS_ERROR_NOT_INITIALIZED;
    }

    NS_ADDREF(*aResult = mDirectory);
    return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfileLock::GetLocalDirectory(nsIFile* *aResult)
{
    if (!mLocalDirectory) {
        NS_ERROR("Not initialized, or unlocked!");
        return NS_ERROR_NOT_INITIALIZED;
    }

    NS_ADDREF(*aResult = mLocalDirectory);
    return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfileLock::Unlock()
{
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
nsToolkitProfileLock::GetReplacedLockTime(PRTime *aResult)
{
    mLock.GetReplacedLockTime(aResult);
    return NS_OK;
}

nsToolkitProfileLock::~nsToolkitProfileLock()
{
    if (mDirectory) {
        Unlock();
    }
}

nsToolkitProfileService*
nsToolkitProfileService::gService = nullptr;

NS_IMPL_ISUPPORTS(nsToolkitProfileService,
                  nsIToolkitProfileService)

nsresult
nsToolkitProfileService::Init()
{
    NS_ASSERTION(gDirServiceProvider, "No dirserviceprovider!");
    nsresult rv;

    rv = nsXREDirProvider::GetUserAppDataDirectory(getter_AddRefs(mAppData));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = nsXREDirProvider::GetUserLocalDataDirectory(getter_AddRefs(mTempData));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mAppData->Clone(getter_AddRefs(mListFile));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mListFile->AppendNative(NS_LITERAL_CSTRING("profiles.ini"));
    NS_ENSURE_SUCCESS(rv, rv);

    bool exists;
    rv = mListFile->IsFile(&exists);
    if (NS_FAILED(rv) || !exists) {
        return NS_OK;
    }

    int64_t size;
    rv = mListFile->GetFileSize(&size);
    if (NS_FAILED(rv) || !size) {
        return NS_OK;
    }

    nsINIParser parser;
    rv = parser.Init(mListFile);
    // Init does not fail on parsing errors, only on OOM/really unexpected
    // conditions.
    if (NS_FAILED(rv))
        return rv;

    nsAutoCString buffer;
    rv = parser.GetString("General", "StartWithLastProfile", buffer);
    if (NS_SUCCEEDED(rv) && buffer.EqualsLiteral("0"))
        mStartWithLast = false;

    nsToolkitProfile* currentProfile = nullptr;

#ifdef MOZ_DEV_EDITION
    nsCOMPtr<nsIFile> ignoreSeparateProfile;
    rv = mAppData->Clone(getter_AddRefs(ignoreSeparateProfile));
    if (NS_FAILED(rv))
        return rv;

    rv = ignoreSeparateProfile->AppendNative(NS_LITERAL_CSTRING("ignore-dev-edition-profile"));
    if (NS_FAILED(rv))
        return rv;

    bool shouldIgnoreSeparateProfile;
    rv = ignoreSeparateProfile->Exists(&shouldIgnoreSeparateProfile);
    if (NS_FAILED(rv))
        return rv;
#endif

    unsigned int c = 0;
    bool foundAuroraDefault = false;
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
        rv = NS_NewNativeLocalFile(EmptyCString(), true,
                                   getter_AddRefs(rootDir));
        NS_ENSURE_SUCCESS(rv, rv);

        if (isRelative) {
            rv = rootDir->SetRelativeDescriptor(mAppData, filePath);
        } else {
            rv = rootDir->SetPersistentDescriptor(filePath);
        }
        if (NS_FAILED(rv)) continue;

        nsCOMPtr<nsIFile> localDir;
        if (isRelative) {
            rv = NS_NewNativeLocalFile(EmptyCString(), true,
                                       getter_AddRefs(localDir));
            NS_ENSURE_SUCCESS(rv, rv);

            rv = localDir->SetRelativeDescriptor(mTempData, filePath);
        } else {
            localDir = rootDir;
        }

        currentProfile = new nsToolkitProfile(name,
                                              rootDir, localDir,
                                              currentProfile);
        NS_ENSURE_TRUE(currentProfile, NS_ERROR_OUT_OF_MEMORY);

        rv = parser.GetString(profileID.get(), "Default", buffer);
        if (NS_SUCCEEDED(rv) && buffer.EqualsLiteral("1") && !foundAuroraDefault) {
            mChosen = currentProfile;
            this->SetDefaultProfile(currentProfile);
        }
#ifdef MOZ_DEV_EDITION
        // Use the dev-edition-default profile if this is an Aurora build and
        // ignore-dev-edition-profile is not present.
        if (name.EqualsLiteral("dev-edition-default") && !shouldIgnoreSeparateProfile) {
            mChosen = currentProfile;
            foundAuroraDefault = true;
        }
#endif
    }

#ifdef MOZ_DEV_EDITION
    if (!foundAuroraDefault && !shouldIgnoreSeparateProfile) {
        // If a single profile exists, it may not be already marked as default.
        // Do it now to avoid problems when we create the dev-edition-default profile.
        if (!mChosen && mFirst && !mFirst->mNext)
            this->SetDefaultProfile(mFirst);

        // Create a default profile for aurora, if none was found.
        nsCOMPtr<nsIToolkitProfile> profile;
        rv = CreateProfile(nullptr,
                           NS_LITERAL_CSTRING("dev-edition-default"),
                           getter_AddRefs(profile));
        if (NS_FAILED(rv)) return rv;
        mChosen = profile;
        rv = Flush();
        if (NS_FAILED(rv)) return rv;
    }
#endif

    if (!mChosen && mFirst && !mFirst->mNext) // only one profile
        mChosen = mFirst;
    return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfileService::SetStartWithLastProfile(bool aValue)
{
    if (mStartWithLast != aValue) {
        mStartWithLast = aValue;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfileService::GetStartWithLastProfile(bool *aResult)
{
    *aResult = mStartWithLast;
    return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfileService::GetStartOffline(bool *aResult)
{
    *aResult = mStartOffline;
    return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfileService::SetStartOffline(bool aValue)
{
    mStartOffline = aValue;
    return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfileService::GetProfiles(nsISimpleEnumerator* *aResult)
{
    *aResult = new ProfileEnumerator(this->mFirst);
    if (!*aResult)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aResult);
    return NS_OK;
}

NS_IMPL_ISUPPORTS(nsToolkitProfileService::ProfileEnumerator,
                  nsISimpleEnumerator)

NS_IMETHODIMP
nsToolkitProfileService::ProfileEnumerator::HasMoreElements(bool* aResult)
{
    *aResult = mCurrent ? true : false;
    return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfileService::ProfileEnumerator::GetNext(nsISupports* *aResult)
{
    if (!mCurrent) return NS_ERROR_FAILURE;

    NS_ADDREF(*aResult = mCurrent);

    mCurrent = mCurrent->mNext;
    return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfileService::GetSelectedProfile(nsIToolkitProfile* *aResult)
{
    if (!mChosen && mFirst && !mFirst->mNext) // only one profile
        mChosen = mFirst;

    if (!mChosen) return NS_ERROR_FAILURE;

    NS_ADDREF(*aResult = mChosen);
    return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfileService::SetSelectedProfile(nsIToolkitProfile* aProfile)
{
    if (mChosen != aProfile) {
        mChosen = aProfile;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfileService::GetDefaultProfile(nsIToolkitProfile* *aResult)
{
    if (!mDefault) return NS_ERROR_FAILURE;

    NS_ADDREF(*aResult = mDefault);
    return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfileService::SetDefaultProfile(nsIToolkitProfile* aProfile)
{
    if (mDefault != aProfile) {
        mDefault = aProfile;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfileService::GetProfileByName(const nsACString& aName,
                                          nsIToolkitProfile* *aResult)
{
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

NS_IMETHODIMP
nsToolkitProfileService::LockProfilePath(nsIFile* aDirectory,
                                         nsIFile* aLocalDirectory,
                                         nsIProfileLock* *aResult)
{
    return NS_LockProfilePath(aDirectory, aLocalDirectory, nullptr, aResult);
}

nsresult
NS_LockProfilePath(nsIFile* aPath, nsIFile* aTempPath,
                   nsIProfileUnlocker* *aUnlocker, nsIProfileLock* *aResult)
{
    RefPtr<nsToolkitProfileLock> lock = new nsToolkitProfileLock();
    if (!lock) return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = lock->Init(aPath, aTempPath, aUnlocker);
    if (NS_FAILED(rv)) return rv;

    lock.forget(aResult);
    return NS_OK;
}

static void SaltProfileName(nsACString& aName)
{
    char salt[9];
    NS_MakeRandomString(salt, 8);
    salt[8] = '.';

    aName.Insert(salt, 0, 9);
}

NS_IMETHODIMP
nsToolkitProfileService::CreateProfile(nsIFile* aRootDir,
                                       const nsACString& aName,
                                       nsIToolkitProfile** aResult)
{
    nsresult rv = GetProfileByName(aName, aResult);
    if (NS_SUCCEEDED(rv)) {
        return rv;
    }

    nsCOMPtr<nsIFile> rootDir (aRootDir);

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

        rv = NS_NewNativeLocalFile(EmptyCString(), true,
                                   getter_AddRefs(localDir));
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

        if (!exists)
            return NS_ERROR_FILE_NOT_DIRECTORY;
    }
    else {
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
        while (last->mNext)
            last = last->mNext;
    }

    nsCOMPtr<nsIToolkitProfile> profile =
        new nsToolkitProfile(aName, rootDir, localDir, last);
    if (!profile) return NS_ERROR_OUT_OF_MEMORY;

    profile.forget(aResult);
    return NS_OK;
}

nsresult
nsToolkitProfileService::CreateTimesInternal(nsIFile* aProfileDir)
{
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
    PRFileDesc *writeFile;
    rv = creationLog->OpenNSPRFileDesc(PR_WRONLY, 0700, &writeFile);
    NS_ENSURE_SUCCESS(rv, rv);

    PR_fprintf(writeFile, "{\n\"created\": %lld\n}\n", msec);
    PR_Close(writeFile);
    return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfileService::GetProfileCount(uint32_t *aResult)
{
    *aResult = 0;
    nsToolkitProfile* profile = mFirst;
    while (profile) {
        (*aResult)++;
        profile = profile->mNext;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfileService::Flush()
{
    // Errors during writing might cause unhappy semi-written files.
    // To avoid this, write the entire thing to a buffer, then write
    // that buffer to disk.

    nsresult rv;
    uint32_t pCount = 0;
    nsToolkitProfile *cur;

    for (cur = mFirst; cur != nullptr; cur = cur->mNext)
        ++pCount;

    uint32_t length;
    const int bufsize = 100+MAXPATHLEN*pCount;
    auto buffer = MakeUnique<char[]>(bufsize);

    char *pos = buffer.get();
    char *end = pos + bufsize;

    pos += snprintf(pos, end - pos,
                    "[General]\n"
                    "StartWithLastProfile=%s\n\n",
                    mStartWithLast ? "1" : "0");

    nsAutoCString path;
    cur = mFirst;
    pCount = 0;

    while (cur) {
        // if the profile dir is relative to appdir...
        bool isRelative;
        rv = mAppData->Contains(cur->mRootDir, &isRelative);
        if (NS_SUCCEEDED(rv) && isRelative) {
            // we use a relative descriptor
            rv = cur->mRootDir->GetRelativeDescriptor(mAppData, path);
        } else {
            // otherwise, a persistent descriptor
            rv = cur->mRootDir->GetPersistentDescriptor(path);
            NS_ENSURE_SUCCESS(rv, rv);
        }

        pos += snprintf(pos, end - pos,
                        "[Profile%u]\n"
                        "Name=%s\n"
                        "IsRelative=%s\n"
                        "Path=%s\n",
                        pCount, cur->mName.get(),
                        isRelative ? "1" : "0", path.get());

        nsCOMPtr<nsIToolkitProfile> profile;
        rv = this->GetDefaultProfile(getter_AddRefs(profile));
        if (NS_SUCCEEDED(rv) && profile == cur) {
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
                                        void** aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsCOMPtr<nsIToolkitProfileService> profileService =
        nsToolkitProfileService::gService;
    if (!profileService) {
        nsresult rv = NS_NewToolkitProfileService(getter_AddRefs(profileService));
        if (NS_FAILED(rv))
            return rv;
    }
    return profileService->QueryInterface(aIID, aResult);
}

NS_IMETHODIMP
nsToolkitProfileFactory::LockFactory(bool aVal)
{
    return NS_OK;
}

nsresult
NS_NewToolkitProfileFactory(nsIFactory* *aResult)
{
    *aResult = new nsToolkitProfileFactory();
    if (!*aResult)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aResult);
    return NS_OK;
}

nsresult
NS_NewToolkitProfileService(nsIToolkitProfileService* *aResult)
{
    nsToolkitProfileService* profileService = new nsToolkitProfileService();
    if (!profileService)
        return NS_ERROR_OUT_OF_MEMORY;
    nsresult rv = profileService->Init();
    if (NS_FAILED(rv)) {
        NS_ERROR("nsToolkitProfileService::Init failed!");
        delete profileService;
        return rv;
    }

    NS_ADDREF(*aResult = profileService);
    return NS_OK;
}

nsresult
XRE_GetFileFromPath(const char *aPath, nsIFile* *aResult)
{
#if defined(XP_MACOSX)
    int32_t pathLen = strlen(aPath);
    if (pathLen > MAXPATHLEN)
        return NS_ERROR_INVALID_ARG;

    CFURLRef fullPath =
        CFURLCreateFromFileSystemRepresentation(nullptr, (const UInt8 *) aPath,
                                                pathLen, true);
    if (!fullPath)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIFile> lf;
    nsresult rv = NS_NewNativeLocalFile(EmptyCString(), true,
                                        getter_AddRefs(lf));
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

    if (!realpath(aPath, fullPath))
        return NS_ERROR_FAILURE;

    return NS_NewNativeLocalFile(nsDependentCString(fullPath), true,
                                 aResult);
#elif defined(XP_WIN)
    WCHAR fullPath[MAXPATHLEN];

    if (!_wfullpath(fullPath, NS_ConvertUTF8toUTF16(aPath).get(), MAXPATHLEN))
        return NS_ERROR_FAILURE;

    return NS_NewLocalFile(nsDependentString(fullPath), true,
                           aResult);

#else
#error Platform-specific logic needed here.
#endif
}
