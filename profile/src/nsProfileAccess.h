/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef __nsProfileAccess_h___
#define __nsProfileAccess_h___

#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsString.h"
#include "nsIRegistry.h"
#include "nsXPIDLString.h"
#include "nsVoidArray.h"
#include "nsIFileSpec.h"
#include "nsIFile.h"
#include "nsILocalFile.h"

#ifdef XP_WIN
#include <windows.h>
#endif

#ifdef XP_UNIX
#include <signal.h>
#endif

class ProfileStruct
{    
public:
     ProfileStruct();
     ProfileStruct(const ProfileStruct& src);
                
    ~ProfileStruct() { }
    
     ProfileStruct& operator=(const ProfileStruct& rhs);
    
    /*
     * GetResolvedProfileDir returns the directory specified in the
     * registry. It will return NULL if the spec in the registry
     * could not be resolved to a path. This directory does not
     * nescesarily exist, although it can be created (i.e. the path
     * is not on an unmounted drive or something).
     */            
    nsresult    GetResolvedProfileDir(nsILocalFile **aDirectory);
    
    /*
     * SetResolvedProfileDir will update the directory in the profile
     * entry. The next time this entry is externalized, this directory
     * will replace the existing entry in the registry.
     */
    nsresult    SetResolvedProfileDir(nsILocalFile *aDirectory);
    
    /*
     * Copies private members to another ProfileStruct
     */
    nsresult    CopyProfileLocation(ProfileStruct *destStruct);
    
    /*
     * Methods used by routines which internalize
     * and externalize profile info.
     */
    nsresult    InternalizeLocation(nsIRegistry *aRegistry, nsRegistryKey profKey, PRBool is4x);
    nsresult    ExternalizeLocation(nsIRegistry *aRegistry, nsRegistryKey profKey);
    nsresult    InternalizeMigratedFromLocation(nsIRegistry *aRegistry, nsRegistryKey profKey);
    nsresult    ExternalizeMigratedFromLocation(nsIRegistry *aRegistry, nsRegistryKey profKey);
    
public:
    nsString    profileName;
    PRBool      isMigrated;

    // The directory from which this profile was migrated from (if any)
    // Added in mozilla1.0.1 and maintained in the registry
    nsCOMPtr<nsILocalFile> migratedFrom;

    nsString    NCProfileName;
    nsString    NCDeniedService;
    nsString    NCEmailAddress;
    nsString    NCHavePregInfo;
    PRBool      updateProfileEntry;
    // this flag detemines if we added this profile to the list for the import module.
    PRBool      isImportType; 
    // These fields were added in mozilla1.0.1 and maintained in the registry.
    // Values are in milliseconds since midnight Jan 1, 1970 GMT (same as nsIFile)
    // Their values will be LL_ZERO if undefined.
    PRInt64     creationTime;
    PRInt64     lastModTime; 

private:
    nsresult    EnsureDirPathExists(nsILocalFile *aFile, PRBool *wasCreated);
    
private:
    // These are mutually exclusive - We have one or the other.    
    nsString regLocationData;
    nsCOMPtr<nsILocalFile> resolvedLocation;
};


class nsProfileAccess
{

private:
    nsCOMPtr <nsIFile> mNewRegFile;

    // This is an array that holds all the profile information--migrated/unmigrated
    // unmigrated: if the profileinfo is migrated--i.e. -installer option is used
    nsVoidArray*  mProfiles;

    nsString      mCurrentProfile;
    nsString      mHavePREGInfo;
    PRBool        m4xProfilesAdded;
    PRBool        mStartWithLastProfile;
public:
    PRBool        mProfileDataChanged;
    PRBool        mForgetProfileCalled;

public:

    nsProfileAccess();
    virtual ~nsProfileAccess();

    void GetNumProfiles(PRInt32 *numProfiles);
    void GetNum4xProfiles(PRInt32 *numProfiles);
    void GetFirstProfile(PRUnichar **firstProfile);
    nsresult GetProfileList(PRInt32 whichKind, PRUint32 *length, PRUnichar ***result);
    nsresult GetOriginalProfileDir(const PRUnichar *profileName, nsILocalFile **orginalDir);
    nsresult SetMigratedFromDir(const PRUnichar *profileName, nsILocalFile *orginalDir);
    nsresult SetProfileLastModTime(const PRUnichar *profileName, PRInt64 lastModTime);
    nsresult GetStartWithLastUsedProfile(PRBool *aStartWithLastUsedProfile);
    nsresult SetStartWithLastUsedProfile(PRBool aStartWithLastUsedProfile);
    
    // if fromImport is true all the 4.x profiles will be added to mProfiles with the isImportType flag set.
    // pass fromImport as True only if you are calling from the Import Module.
    nsresult Get4xProfileInfo(const char *registryName, PRBool fromImport);

    void SetCurrentProfile(const PRUnichar *profileName);
    void GetCurrentProfile(PRUnichar **profileName);
    
    nsresult GetValue(const PRUnichar* profileName, ProfileStruct** aProfile);
    nsresult SetValue(ProfileStruct* aProfile);
    void CheckRegString(const PRUnichar *profileName, char** regString);
    void RemoveSubTree(const PRUnichar* profileName);

    PRBool ProfileExists(const PRUnichar *profileName);

    nsresult DetermineForceMigration(PRBool *forceMigration);
    nsresult UpdateRegistry(nsIFile* regName);

private:
    nsresult FillProfileInfo(nsIFile* regName);



    nsresult HavePregInfo(char **info);

    // if forImport is true searches only the ImportType profiles
    // else searches the non-ImportType profiles.
    PRInt32	 FindProfileIndex(const PRUnichar* profileName, PRBool forImport);

    void SetPREGInfo(const char* pregInfo);
    void FreeProfileMembers(nsVoidArray *aProfile);
    nsresult ResetProfileMembers();
};


#include "prclist.h"

class nsProfileLock
#if defined (XP_UNIX)
  : public PRCList
#endif
{
public:
                            nsProfileLock();
                            nsProfileLock(nsProfileLock& src);

                            ~nsProfileLock();
 
    nsProfileLock&          operator=(nsProfileLock& rhs);
                       
    nsresult                Lock(nsILocalFile* aFile);
    nsresult                Unlock();
        
private:
    PRPackedBool            mHaveLock;

#if defined (XP_MAC)
  #if TARGET_CARBON
    int                     mLockFileDesc;
  #endif
    nsCOMPtr<nsILocalFile>  mLockFile;
#elif defined (XP_WIN)
    HANDLE                  mLockFileHandle;
#elif defined (XP_OS2)
    LHANDLE                 mLockFileHandle;
#elif defined (XP_UNIX)
    static void             RemovePidLockFiles();
#ifdef HAVE_SIGINFO_T
    static void             FatalSignalHandler(int signo, siginfo_t* info,
                                               void* context);
#else
    static void             FatalSignalHandler(int signo);
#endif
    static PRCList          mPidLockList;
    char*                   mPidLockFileName;
    int                     mLockFileDesc;
#endif

};

#endif // __nsProfileAccess_h___

