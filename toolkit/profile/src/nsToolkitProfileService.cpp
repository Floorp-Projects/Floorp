/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is the new Mozilla toolkit.
 *
 * The Initial Developer of the Original Code is
 * Benjamin Smedberg <bsmedberg@covad.net>
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include <stdio.h>
#include <stdlib.h>
#include "nsProfileLock.h"

#ifdef XP_WIN
#include <windows.h>
#include <shlobj.h>
#endif
#ifdef XP_BEOS
#include <Path.h>
#endif
#ifdef XP_UNIX
#include <unistd.h>
#endif

#include "nsIToolkitProfileService.h"
#include "nsIToolkitProfile.h"
#include "nsIFactory.h"
#include "nsILocalFile.h"
#include "nsISimpleEnumerator.h"

#ifdef XP_MACOSX
#include <CFURL.h>
#include "nsILocalFileMac.h"
#endif

#include "nsAppDirectoryServiceDefs.h"
#include "nsXULAppAPI.h"

#include "nsINIParser.h"
#include "nsXREDirProvider.h"
#include "nsAppRunner.h"
#include "nsString.h"
#include "nsReadableUtils.h"


class nsToolkitProfile : public nsIToolkitProfile
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSITOOLKITPROFILE

    friend class nsToolkitProfileService;
    nsCOMPtr<nsToolkitProfile> mNext;
    nsToolkitProfile          *mPrev;

private:
    nsToolkitProfile(const nsACString& aName, nsILocalFile* aFile,
                     nsToolkitProfile* aPrev);
    ~nsToolkitProfile() { }

    friend class nsToolkitProfileLock;

    nsCString                  mName;
    nsCOMPtr<nsILocalFile>     mFile;
    nsIProfileLock*            mLock;
};

class nsToolkitProfileLock : public nsIProfileLock
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPROFILELOCK

    nsresult Init(nsToolkitProfile* aProfile);
    nsresult Init(nsILocalFile* aDirectory);

    nsToolkitProfileLock() { }

private:
    ~nsToolkitProfileLock();

    nsCOMPtr<nsToolkitProfile> mProfile;
    nsCOMPtr<nsILocalFile> mDirectory;

    nsProfileLock mLock;
};

class nsToolkitProfileService : public nsIToolkitProfileService,
                                public nsIFactory
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSITOOLKITPROFILESERVICE

    // we implement nsIFactory because we can't be registered by location,
    // like most ordinary components are. Instead, during startup we register
    // our factory. Then we return our singleton-self when asked.
    NS_DECL_NSIFACTORY

private:
    friend class nsToolkitProfile;
    friend nsresult NS_NewToolkitProfileService(nsIToolkitProfileService**);

    nsToolkitProfileService() :
        mDirty(PR_FALSE),
        mStartWithLast(PR_TRUE),
        mStartOffline(PR_FALSE)
    {
        gService = this;
    }
    ~nsToolkitProfileService()
    {
        gService = nsnull;
    }

    NS_HIDDEN_(nsresult) Init();

    nsCOMPtr<nsToolkitProfile> mFirst;
    nsCOMPtr<nsToolkitProfile> mChosen;
    nsCOMPtr<nsILocalFile>     mAppData;
    nsCOMPtr<nsILocalFile>     mListFile;
    PRBool mDirty;
    PRBool mStartWithLast;
    PRBool mStartOffline;

    static nsToolkitProfileService *gService;

    class ProfileEnumerator : public nsISimpleEnumerator
    {
    public:
        NS_DECL_ISUPPORTS
        NS_DECL_NSISIMPLEENUMERATOR

        ProfileEnumerator(nsToolkitProfile *first)
          { mCurrent = first; }
    private:
        ~ProfileEnumerator() { }
        nsCOMPtr<nsToolkitProfile> mCurrent;
    };
};

nsToolkitProfile::nsToolkitProfile(const nsACString& aName, nsILocalFile* aFile,
                                   nsToolkitProfile* aPrev) :
    mPrev(aPrev),
    mName(aName),
    mFile(aFile),
    mLock(nsnull)
{
    NS_ASSERTION(aFile, "No file!");

    if (aPrev)
        aPrev->mNext = this;
    else
        nsToolkitProfileService::gService->mFirst = this;
}

NS_IMPL_ISUPPORTS1(nsToolkitProfile, nsIToolkitProfile)

NS_IMETHODIMP
nsToolkitProfile::GetRootDir(nsILocalFile* *aResult)
{
    NS_ADDREF(*aResult = mFile);
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
    nsToolkitProfileService::gService->mDirty = PR_TRUE;

    return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfile::Remove(PRBool removeFiles)
{
    NS_ASSERTION(nsToolkitProfileService::gService,
                 "Whoa, my service is gone.");

    if (mLock)
        return NS_ERROR_FILE_IS_LOCKED;

    if (removeFiles)
        mFile->Remove(PR_TRUE);

    if (mPrev)
        mPrev->mNext = mNext;
    else
        nsToolkitProfileService::gService->mFirst = mNext;

    if (mNext)
        mNext->mPrev = mPrev;

    mPrev = nsnull;
    mNext = nsnull;

    if (nsToolkitProfileService::gService->mChosen == this)
        nsToolkitProfileService::gService = nsnull;

    nsToolkitProfileService::gService->mDirty = PR_TRUE;

    return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfile::Lock(nsIProfileLock* *aResult)
{
    if (mLock) {
        NS_ADDREF(*aResult = mLock);
        return NS_OK;
    }

    nsCOMPtr<nsToolkitProfileLock> lock = new nsToolkitProfileLock();
    if (!lock) return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = lock->Init(this);
    if (NS_FAILED(rv)) return rv;

    NS_ADDREF(*aResult = lock);
    return NS_OK;
}

NS_IMPL_ISUPPORTS1(nsToolkitProfileLock, nsIProfileLock)

nsresult
nsToolkitProfileLock::Init(nsToolkitProfile* aProfile)
{
    nsresult rv;
    rv = Init(aProfile->mFile);
    if (NS_SUCCEEDED(rv))
        mProfile = aProfile;

    return rv;
}

nsresult
nsToolkitProfileLock::Init(nsILocalFile* aDirectory)
{
    nsresult rv;

    rv = mLock.Lock(aDirectory);

    if (NS_SUCCEEDED(rv))
        mDirectory = aDirectory;

    return rv;
}

NS_IMETHODIMP
nsToolkitProfileLock::GetDirectory(nsILocalFile* *aResult)
{
    if (!mDirectory) {
        NS_ERROR("Not initialized, or unlocked!");
        return NS_ERROR_NOT_INITIALIZED;
    }

    NS_ADDREF(*aResult = mDirectory);
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
        mProfile->mLock = nsnull;
        mProfile = nsnull;
    }
    mDirectory = nsnull;

    return NS_OK;
}

nsToolkitProfileLock::~nsToolkitProfileLock()
{
    if (mDirectory) {
        Unlock();
    }
}

nsToolkitProfileService*
nsToolkitProfileService::gService = nsnull;

NS_IMPL_ISUPPORTS2(nsToolkitProfileService,
                   nsIToolkitProfileService,
                   nsIFactory)

nsresult
nsToolkitProfileService::Init()
{
    NS_ASSERTION(gDirServiceProvider, "No dirserviceprovider!");
    nsresult rv;

    rv = gDirServiceProvider->GetUserAppDataDirectory(getter_AddRefs(mAppData));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIFile> listFile;
    rv = mAppData->Clone(getter_AddRefs(listFile));
    NS_ENSURE_SUCCESS(rv, rv);

    mListFile = do_QueryInterface(listFile);
    NS_ENSURE_TRUE(listFile, NS_ERROR_NO_INTERFACE);

    rv = mListFile->AppendNative(NS_LITERAL_CSTRING("profiles.ini"));
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool exists;
    rv = mListFile->IsFile(&exists);
    if (NS_FAILED(rv) || !exists) {
        return NS_OK;
    }

    nsINIParser parser;
    rv = parser.Init(mListFile);
    // Parsing errors are troublesome... we're gonna continue even on
    // parsing errors, and let people manually re-locate their profile
    // if something goes wacky

    char parserBuf[MAXPATHLEN];
    rv = parser.GetString("General", "StartWithLastProfile", parserBuf, MAXPATHLEN);
    if (NS_SUCCEEDED(rv) && strcmp("0", parserBuf) == 0)
        mStartWithLast = PR_FALSE;

    nsToolkitProfile* currentProfile = nsnull;
    nsCAutoString filePath;

    unsigned int c = 0;
    for (c = 0; PR_TRUE; ++c) {
        char profileID[12];
        sprintf(profileID, "Profile%u", c);

        rv = parser.GetString(profileID, "IsRelative", parserBuf, MAXPATHLEN);
        if (NS_FAILED(rv)) break;

        PRBool isRelative = (strcmp(parserBuf, "1") == 0);

        rv = parser.GetString(profileID, "Path", parserBuf, MAXPATHLEN);
        if (NS_FAILED(rv)) {
            NS_ERROR("Malformed profiles.ini: Path= not found");
            continue;
        }

        filePath = parserBuf;

        rv = parser.GetString(profileID, "Name", parserBuf, MAXPATHLEN);
        if (NS_FAILED(rv)) {
            NS_ERROR("Malformed profiles.ini: Name= not found");
            continue;
        }

        nsCOMPtr<nsILocalFile> rootDir;
        rv = NS_NewNativeLocalFile(EmptyCString(), PR_TRUE,
                                   getter_AddRefs(rootDir));
        NS_ENSURE_SUCCESS(rv, rv);

        if (isRelative) {
            rv = rootDir->SetRelativeDescriptor(mAppData, filePath);
        } else {
            rv = rootDir->SetPersistentDescriptor(filePath);
        }
        if (NS_FAILED(rv)) continue;

        currentProfile = new nsToolkitProfile(nsDependentCString(parserBuf),
                                              rootDir, currentProfile);
        NS_ENSURE_TRUE(currentProfile, NS_ERROR_OUT_OF_MEMORY);

        rv = parser.GetString(profileID, "Default", parserBuf, MAXPATHLEN);
        if (NS_SUCCEEDED(rv) && strcmp("1", parserBuf) == 0)
            mChosen = currentProfile;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfileService::SetStartWithLastProfile(PRBool aValue)
{
    if (mStartWithLast != aValue) {
        mStartWithLast = aValue;
        mDirty = PR_TRUE;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfileService::GetStartWithLastProfile(PRBool *aResult)
{
    *aResult = mStartWithLast;
    return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfileService::GetStartOffline(PRBool *aResult)
{
    *aResult = mStartOffline;
    return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfileService::SetStartOffline(PRBool aValue)
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

NS_IMPL_ISUPPORTS1(nsToolkitProfileService::ProfileEnumerator,
                   nsISimpleEnumerator)

NS_IMETHODIMP
nsToolkitProfileService::ProfileEnumerator::HasMoreElements(PRBool* aResult)
{
    *aResult = mCurrent ? PR_TRUE : PR_FALSE;
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
        mDirty = PR_TRUE;
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
nsToolkitProfileService::LockProfilePath(nsILocalFile* aDirectory,
                                         nsIProfileLock* *aResult)
{
    return NS_LockProfilePath(aDirectory, aResult);
}

nsresult
NS_LockProfilePath(nsILocalFile* aPath, nsIProfileLock* *aResult)
{
    nsCOMPtr<nsToolkitProfileLock> lock = new nsToolkitProfileLock();
    if (!lock) return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = lock->Init(aPath);
    if (NS_FAILED(rv)) return rv;

    NS_ADDREF(*aResult = lock);
    return NS_OK;
}

static const char kTable[] =
    { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
      'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
      '1', '2', '3', '4', '5', '6', '7', '8', '9', '0' };

static void SaltProfileName(nsACString& aName)
{
    double fpTime;
    LL_L2D(fpTime, PR_Now());

    // use 1e-6, granularity of PR_Now() on the mac is seconds
    srand((uint)(fpTime * 1e-6 + 0.5));

    aName.Append('.');
    int i;
    for (i = 0; i < 3; ++i)
        aName.Append(kTable[rand() % NS_ARRAY_LENGTH(kTable)]);
}

NS_IMETHODIMP
nsToolkitProfileService::CreateProfile(nsILocalFile* aRootDir,
                                       const nsACString& aName,
                                       nsIToolkitProfile* *aResult)
{
    nsresult rv;

    rv = GetProfileByName(aName, aResult);
    if (NS_SUCCEEDED(rv)) return rv;

    nsCOMPtr<nsILocalFile> rootDir (aRootDir);

    if (!rootDir) {
        nsCOMPtr<nsIFile> file;
        PRBool dummy;
        rv = gDirServiceProvider->GetFile(NS_APP_USER_PROFILES_ROOT_DIR, &dummy,
                                          getter_AddRefs(file));
        NS_ENSURE_SUCCESS(rv, rv);

        rootDir = do_QueryInterface(file);
        NS_ENSURE_TRUE(rootDir, NS_ERROR_UNEXPECTED);

        nsCAutoString dirName(aName);
        SaltProfileName(dirName);

        rootDir->AppendNative(dirName);
    }

    PRBool exists;
    rv = rootDir->Exists(&exists);
    NS_ENSURE_SUCCESS(rv, rv);

    if (exists) {
        rv = rootDir->IsDirectory(&exists);
        NS_ENSURE_SUCCESS(rv, rv);

        if (!exists)
            return NS_ERROR_FILE_NOT_DIRECTORY;
    }
    else {
        nsCOMPtr<nsIFile> profileDefaultsDir;
        nsCOMPtr<nsIFile> profileDirParent;
        nsCAutoString profileDirName;

        rv = rootDir->GetParent(getter_AddRefs(profileDirParent));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = rootDir->GetNativeLeafName(profileDirName);
        NS_ENSURE_SUCCESS(rv, rv);

        PRBool dummy;
        rv = gDirServiceProvider->GetFile(NS_APP_PROFILE_DEFAULTS_50_DIR, &dummy,
                                          getter_AddRefs(profileDefaultsDir));

        if (NS_SUCCEEDED(rv))
            rv = profileDefaultsDir->CopyToNative(profileDirParent,
                                                  profileDirName);
        if (NS_FAILED(rv)) {
            // if copying failed, lets just ensure that the profile directory exists.
            rv = rootDir->Create(nsIFile::DIRECTORY_TYPE, 0700);
            NS_ENSURE_SUCCESS(rv, rv);
        }
      
        rv = rootDir->SetPermissions(0700);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    nsToolkitProfile* last = mFirst;
    if (last) {
        while (last->mNext)
            last = last->mNext;
    }

    nsCOMPtr<nsIToolkitProfile> profile =
        new nsToolkitProfile(aName, rootDir, last);
    if (!profile) return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aResult = profile);
    return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfileService::GetProfileCount(PRUint32 *aResult)
{
    if (!mFirst)
        *aResult = 0;
    else if (! mFirst->mNext)
        *aResult = 1;
    else
        *aResult = 2;

    return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfileService::Flush()
{
    // Errors during writing might cause unhappy semi-written files.
    // To avoid this, write the entire thing to a buffer, then write
    // that buffer to disk.

    nsresult rv;
    PRUint32 pCount = 0;
    nsToolkitProfile *cur;

    for (cur = mFirst; cur != nsnull; cur = cur->mNext)
        ++pCount;

    PRUint32 length;

    char* buffer = (char*) malloc(100 + MAXPATHLEN * pCount);
    NS_ENSURE_TRUE(buffer, NS_ERROR_OUT_OF_MEMORY);

    char *end = buffer;

    end += sprintf(end,
                   "[General]\n"
                   "StartWithLastProfile=%s\n\n",
                   mStartWithLast ? "1" : "0");

    nsCAutoString path;
    cur = mFirst;
    pCount = 0;

    while (cur) {
        // if the profile dir is relative to appdir...
        PRBool isRelative;
        rv = mAppData->Contains(cur->mFile, PR_TRUE, &isRelative);
        if (NS_SUCCEEDED(rv) && isRelative) {
            // we use a relative descriptor
            rv = cur->mFile->GetRelativeDescriptor(mAppData, path);
        } else {
            // otherwise, a persistent descriptor
            rv = cur->mFile->GetPersistentDescriptor(path);
            NS_ENSURE_SUCCESS(rv, rv);
        }

        end += sprintf(end,
                       "[Profile%u]\n"
                       "Name=%s\n"
                       "IsRelative=%s\n"
                       "Path=%s\n",
                       pCount, cur->mName.get(),
                       isRelative ? "1" : "0", path.get());

        if (mChosen == cur) {
            end += sprintf(end, "Default=1\n");
        }

        end += sprintf(end, "\n");

        cur = cur->mNext;
        ++pCount;
    }

    FILE* writeFile;
    rv = mListFile->OpenANSIFileDesc("w", &writeFile);
    NS_ENSURE_SUCCESS(rv, rv);

    if (buffer) {
        length = end - buffer;

        if (fwrite(buffer, sizeof(char), length, writeFile) != length) {
            fclose(writeFile);
            return NS_ERROR_UNEXPECTED;
        }
    }

    fclose(writeFile);
    return NS_OK;
}

NS_IMETHODIMP
nsToolkitProfileService::CreateInstance(nsISupports* aOuter, const nsID& aIID,
                                        void** aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    // return this object
    return QueryInterface(aIID, aResult);
}

NS_IMETHODIMP
nsToolkitProfileService::LockFactory(PRBool aVal)
{
    return NS_OK;
}

nsresult
NS_NewToolkitProfileService(nsIToolkitProfileService* *aResult)
{
    nsToolkitProfileService* aThis = new nsToolkitProfileService();
    nsresult rv = aThis->Init();
    if (NS_FAILED(rv)) {
        NS_ERROR("nsToolkitProfileService::Init failed!");
        delete aThis;
        return rv;
    }

    NS_ADDREF(*aResult = aThis);
    return NS_OK;
}

nsresult
NS_GetFileFromPath(const char *aPath, nsILocalFile* *aResult)
{
#if defined(XP_MACOSX)
    PRInt32 pathLen = strlen(aPath);
    if (pathLen > MAXPATHLEN)
        return NS_ERROR_INVALID_ARG;

    CFURLRef fullPath =
        CFURLCreateFromFileSystemRepresentation(NULL, aPath, pathLen, true);
    if (!fullPath)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsILocalFile> lf;
    nsresult rv = NS_NewNativeLocalFile(EmptyCString(), PR_TRUE,
                                        getter_AddRefs(lf));
    if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsILocalFileMac> lfMac = do_QueryInterface(lf, &rv);
        if (NS_SUCCEEDED(rv)) {
            rv = lfMac->InitWithCFURL(fullPath);
            if (NS_SUCCEEDED(rv))
                NS_ADDREF(*aResult = lf);
        }
    }
    CFRelease(fullPath);
    return rv;

#elif defined(XP_UNIX) || defined(XP_OS2)
    char fullPath[MAXPATHLEN];

    if (!realpath(aPath, fullPath))
        return NS_ERROR_FAILURE;

    return NS_NewNativeLocalFile(nsDependentCString(fullPath), PR_TRUE,
                                 aResult);

#elif defined(XP_WIN)
    char fullPath[MAXPATHLEN];

    if (!_fullpath(fullPath, aPath, MAXPATHLEN))
        return NS_ERROR_FAILURE;

    return NS_NewNativeLocalFile(nsDependentCString(fullPath), PR_TRUE,
                                 aResult);

#elif defined(XP_BEOS)
    BPath fullPath;
    if (fullPath.SetTo(aPath, NULL, true))
        return NS_ERROR_FAILURE;

    return NS_NewNativeLocalFile(nsDependentCString(fullPath.Leaf()), PR_TRUE,
                                 aResult);

#else
#error Platform-specific logic needed here.
#endif
}
