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


class ProfileStruct
{    
public:
     explicit ProfileStruct() { }
     explicit ProfileStruct(const ProfileStruct& src);
                
    ~ProfileStruct() { }
    
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
    nsresult    InternalizeLocation(nsIRegistry *aRegistry, nsRegistryKey profKey, PRBool is4x, PRBool isOld50);
    nsresult    ExternalizeLocation(nsIRegistry *aRegistry, nsRegistryKey profKey);
    
public:
    nsString    profileName;
    PRBool      isMigrated;
    nsString    NCProfileName;
    nsString    NCDeniedService;
    nsString    NCEmailAddress;
    nsString    NCHavePregInfo;
    PRBool      updateProfileEntry;

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

    // Represents the size of the mProfiles array
    // This value keeps changing as profiles are created/deleted/migrated
    PRInt32       mCount;

    nsString      mCurrentProfile;
    nsString      mHavePREGInfo;

    // Represents the size of the m4xProfiles array
    // This value gets set after the profile information is migrated
    // and does not change subsequently.
    PRInt32       m4xCount;


    // It looks like mCount and m4xCount are not required.
    // But retaining them for now to avoid some problems.
    // Will re-evaluate them in future.

public:
    PRBool         mProfileDataChanged;
    PRBool         mForgetProfileCalled;

    // This is the num of 5.x profiles at any given time
    PRInt32        mNumProfiles;

    // This is the num of 4.x profiles at any given time
    PRInt32        mNumOldProfiles;

    nsVoidArray*   m4xProfiles;

    nsProfileAccess();
    virtual ~nsProfileAccess();

    nsresult SetValue(ProfileStruct* aProfile);
    nsresult FillProfileInfo(nsIFile* regName);

    void GetNumProfiles(PRInt32 *numProfiles);
    void GetNum4xProfiles(PRInt32 *numProfiles);
    void GetFirstProfile(PRUnichar **firstProfile);

    void SetCurrentProfile(const PRUnichar *profileName);
    void GetCurrentProfile(PRUnichar **profileName);

    void RemoveSubTree(const PRUnichar* profileName);

    nsresult HavePregInfo(char **info);
    nsresult GetValue(const PRUnichar* profileName, ProfileStruct** aProfile);
    PRInt32	 FindProfileIndex(const PRUnichar* profileName);

    nsresult UpdateRegistry(nsIFile* regName);
    nsresult GetProfileList(PRInt32 whichKind, PRUint32 *length, PRUnichar ***result);
    PRBool ProfileExists(const PRUnichar *profileName);
    nsresult Get4xProfileInfo(const char *registryName);
    nsresult UpdateProfileArray();
    void SetPREGInfo(const char* pregInfo);
    void CheckRegString(const PRUnichar *profileName, char** regString);
    void FreeProfileMembers(nsVoidArray *aProfile, PRInt32 numElems);
    nsresult GetMozRegDataMovedFlag(PRBool *regDataMoved);
    nsresult SetMozRegDataMovedFlag(nsIFile* regName);
    nsresult ResetProfileMembers();
    nsresult DetermineForceMigration(PRBool *forceMigration);
};

#endif // __nsProfileAccess_h___

