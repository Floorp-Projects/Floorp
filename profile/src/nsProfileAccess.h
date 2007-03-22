/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef __nsProfileAccess_h___
#define __nsProfileAccess_h___

#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsString.h"
#include "nsIRegistry.h"
#include "nsXPIDLString.h"
#include "nsVoidArray.h"
#include "nsIFile.h"
#include "nsILocalFile.h"

#ifdef XP_WIN
#include <windows.h>
#endif

#ifdef XP_OS2
#define INCL_DOSERRORS
#define INCL_DOSFILEMGR
#include <os2.h>
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
    nsresult Get4xProfileInfo(nsIFile *registryFile, PRBool fromImport);

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


#endif // __nsProfileAccess_h___

