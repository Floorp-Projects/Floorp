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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#include "nsProfileAccess.h"
#include "nsProfile.h"

#include "pratom.h"
#include "prmem.h"
#include "plstr.h"
#include "prenv.h"

#include "nsIEnumerator.h"
#include "prprf.h"
#include "nsCOMPtr.h"
#include "nsIComponentManager.h"
#include "nsEscape.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsILocalFile.h"
#include "nsReadableUtils.h"

#if defined(XP_MAC) || defined(XP_MACOSX)
#include <Processes.h>
#include <CFBundle.h>
#include "nsILocalFileMac.h"
#endif

#ifdef XP_UNIX
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include "prnetdb.h"
#include "prsystem.h"
#endif

#ifdef VMS
#include <rmsdef.h>
#endif

#include "nsICharsetConverterManager.h"
#include "nsIPlatformCharset.h"


#if defined (XP_UNIX)
#define USER_ENVIRONMENT_VARIABLE "USER"
#define LOGNAME_ENVIRONMENT_VARIABLE "LOGNAME"
#define HOME_ENVIRONMENT_VARIABLE "HOME"
#define PROFILE_NAME_ENVIRONMENT_VARIABLE "PROFILE_NAME"
#define PROFILE_HOME_ENVIRONMENT_VARIABLE "PROFILE_HOME"
#define DEFAULT_UNIX_PROFILE_NAME "default"
#ifndef XP_MACOSX   /* Don't use symlink-based locking on OS X */
#define USE_SYMLINK_LOCKING
#endif
#elif defined (XP_BEOS)
#endif

// IID and CIDs of all the services needed
static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

// Registry Keys

#define kRegistryYesString (NS_LITERAL_STRING("yes"))
#define kRegistryNoString (NS_LITERAL_STRING("no"))

#define kRegistryProfileSubtreeString (NS_LITERAL_STRING("Profiles"))
#define kRegistryCurrentProfileString (NS_LITERAL_STRING("CurrentProfile"))
#define kRegistryNCServiceDenialString (NS_LITERAL_STRING("NCServiceDenial"))
#define kRegistryNCProfileNameString (NS_LITERAL_STRING("NCProfileName"))
#define kRegistryNCUserEmailString (NS_LITERAL_STRING("NCEmailAddress"))
#define kRegistryNCHavePREGInfoString (NS_LITERAL_STRING("NCHavePregInfo"))
#define kRegistryHavePREGInfoString (NS_LITERAL_STRING("HavePregInfo"))
#define kRegistryMigratedString (NS_LITERAL_STRING("migrated"))
#define kRegistryDirectoryString (NS_LITERAL_STRING("directory"))
#define kRegistryNeedMigrationString (NS_LITERAL_STRING("NeedMigration"))
#define kRegistryMozRegDataMovedString (NS_LITERAL_STRING("OldRegDataMoved"))
#define kRegistryCreationTimeString (NS_LITERAL_CSTRING("CreationTime"))
#define kRegistryLastModTimeString (NS_LITERAL_CSTRING("LastModTime"))
#define kRegistryMigratedFromString (NS_LITERAL_CSTRING("MigFromDir"))
#define kRegistryVersionString (NS_LITERAL_STRING("Version"))
#define kRegistryVersion_1_0 (NS_LITERAL_STRING("1.0"))
#define kRegistryCurrentVersion (NS_LITERAL_STRING("1.0"))
#define kRegistryStartWithLastString (NS_LITERAL_CSTRING("AutoStartWithLast"))

// **********************************************************************
// class nsProfileAccess
// **********************************************************************

/*
 * Constructor/Destructor
 * FillProfileInfo reads the registry and fills profileStructs
 */
nsProfileAccess::nsProfileAccess()
{
    mProfileDataChanged  =  PR_FALSE;
    mForgetProfileCalled =  PR_FALSE;
    m4xProfilesAdded     =  PR_FALSE;
    mStartWithLastProfile = PR_FALSE;
    mProfiles            =  new nsVoidArray();

    // Get the profile registry path
    NS_GetSpecialDirectory(NS_APP_APPLICATION_REGISTRY_FILE, getter_AddRefs(mNewRegFile));

    // Read the data into internal data structure....
    FillProfileInfo(mNewRegFile);
}

// On the way out, close the registry if it is
// still opened and free up the resources.
nsProfileAccess::~nsProfileAccess()
{
    // Release all resources.
    mNewRegFile = nsnull;
    FreeProfileMembers(mProfiles);
}

// A wrapper function to call the interface to get a platform file charset.
nsresult
GetPlatformCharset(nsCString& aCharset)
{
    nsresult rv;

    // we may cache it since the platform charset will not change through application life
    nsCOMPtr <nsIPlatformCharset> platformCharset = do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv) && platformCharset) {
        rv = platformCharset->GetCharset(kPlatformCharsetSel_FileName, aCharset);
    }
    if (NS_FAILED(rv)) {
        aCharset.Assign(NS_LITERAL_CSTRING("ISO-8859-1"));  // use ISO-8859-1 in case of any error
    }
    return rv;
}

// Apply a charset conversion from the given charset to Unicode for input C string.
nsresult
ConvertStringToUnicode(nsCString& aCharset, const char* inString, nsAString& outString)
{
    nsresult rv;
    // convert result to unicode
    nsCOMPtr<nsICharsetConverterManager> ccm =
             do_GetService(kCharsetConverterManagerCID, &rv);

    if(NS_SUCCEEDED(rv)) {
        nsCOMPtr <nsIUnicodeDecoder> decoder; // this may be cached
        rv = ccm->GetUnicodeDecoderRaw(aCharset.get(), getter_AddRefs(decoder));

        if(NS_SUCCEEDED(rv) && decoder) {
            PRInt32 uniLength = 0;
            PRInt32 srcLength = strlen(inString);
            rv = decoder->GetMaxLength(inString, srcLength, &uniLength);

            if (NS_SUCCEEDED(rv)) {
                PRUnichar *unichars = new PRUnichar [uniLength];

                if (nsnull != unichars) {
                    // convert to unicode
                    rv = decoder->Convert(inString, &srcLength, unichars, &uniLength);

                    if (NS_SUCCEEDED(rv)) {
                        // Pass back the unicode string
                        outString.Assign(unichars, uniLength);
                    }
                    delete [] unichars;
                }
                else {
                    rv = NS_ERROR_OUT_OF_MEMORY;
                }
            }
        }
    }
    return rv;
}

// Free up the member profile structs
void
nsProfileAccess::FreeProfileMembers(nsVoidArray *profiles)
{
    NS_ASSERTION(profiles, "Invalid profiles");

    PRInt32 index = 0;
    PRInt32 numElems = profiles->Count();

    ProfileStruct* aProfile;
    if (profiles) {
        for (index = 0; index < numElems; index++)
        {
            aProfile = (ProfileStruct *) profiles->ElementAt(index);

            delete aProfile;
        }

        delete profiles;
    }
}


// Given the name of the profile, the structure that
// contains the relavant profile information will be filled.
// Caller must free up the profile struct.
nsresult
nsProfileAccess::GetValue(const PRUnichar* profileName, ProfileStruct** aProfile)
{
    NS_ENSURE_ARG(profileName);
    NS_ENSURE_ARG_POINTER(aProfile);
    *aProfile = nsnull;

    PRInt32 index = 0;
    index = FindProfileIndex(profileName, PR_FALSE);
    if (index < 0)
        return NS_ERROR_FAILURE;

    ProfileStruct* profileItem = (ProfileStruct *) (mProfiles->ElementAt(index));

    *aProfile = new ProfileStruct(*profileItem);
    if (!*aProfile)
        return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}

// This method writes all changes to the array of the
// profile structs. If it is an existing profile, it
// will be updated. If it is a new profile, it gets added
// to the list.
nsresult
nsProfileAccess::SetValue(ProfileStruct* aProfile)
{
    NS_ENSURE_ARG(aProfile);

    PRInt32     index = 0;
    ProfileStruct* profileItem;

    index = FindProfileIndex(aProfile->profileName.get(), aProfile->isImportType);

    if (index >= 0)
    {
        profileItem = (ProfileStruct *) (mProfiles->ElementAt(index));
        *profileItem = *aProfile;
        profileItem->updateProfileEntry = PR_TRUE;
    }
    else
    {
        profileItem     = new ProfileStruct(*aProfile);
        if (!profileItem)
            return NS_ERROR_OUT_OF_MEMORY;
    profileItem->updateProfileEntry = PR_TRUE;

        if (!mProfiles) {
            mProfiles = new nsVoidArray;
        if (!mProfiles)
                return NS_ERROR_OUT_OF_MEMORY;
        }
        mProfiles->AppendElement((void*)profileItem);
    }

    return NS_OK;
}

// Enumerates through the registry for profile
// information. Reads in the data into the array
// of profile structs. After this, all the callers
// requesting profile info will get thier data from
// profiles array. All the udates will be done to this
// data structure to reflect the latest status.
// Data will be flushed at the end.
nsresult
nsProfileAccess::FillProfileInfo(nsIFile* regName)
{
    nsresult rv = NS_OK;

    nsCOMPtr<nsIRegistry> registry(do_CreateInstance(NS_REGISTRY_CONTRACTID, &rv));
    if (NS_FAILED(rv)) return rv;
    rv = registry->Open(regName);
    if (NS_FAILED(rv)) return rv;

    // Enumerate all subkeys (immediately) under the given node.
    nsCOMPtr<nsIEnumerator> enumKeys;
    nsRegistryKey profilesTreeKey;

    rv = registry->GetKey(nsIRegistry::Common,
                            kRegistryProfileSubtreeString.get(),
                            &profilesTreeKey);

    if (NS_FAILED(rv))
    {
        rv = registry->AddKey(nsIRegistry::Common,
                                kRegistryProfileSubtreeString.get(),
                                &profilesTreeKey);
        if (NS_FAILED(rv)) return rv;
    }


    // introducing these tmp variables as nsString variables cannot be passed to
    // the resgitry methods
    nsXPIDLString tmpCurrentProfile;
    nsXPIDLString tmpVersion;
    nsXPIDLString tmpPREGInfo;


    // For the following variables, we do not check for the rv value
    // but check for the variable instead, because it is valid to proceed
    // without the variables having a value. That's why there are no returns
    // for invalid rv values.

    // Get the current profile
    rv = registry->GetString(profilesTreeKey,
                               kRegistryCurrentProfileString.get(),
                               getter_Copies(tmpCurrentProfile));

    if (tmpCurrentProfile)
    {
        // If current profile does not exist, mCurrentProfile will not be set
        // This is not harmful, as GetCurrentProfile method needs to return this value
        // And GetCurrentProfile returns:
        //    the current profile if set
        //    the first profile if profiles exist but no current profile is set
        //    an empty string if no profiles exist.

        mCurrentProfile = NS_STATIC_CAST(const PRUnichar*, tmpCurrentProfile);
    }

    // Get the profile version
    rv = registry->GetString(profilesTreeKey,
                             kRegistryVersionString.get(),
                             getter_Copies(tmpVersion));

    // Get the preg info
    rv = registry->GetString(profilesTreeKey,
                             kRegistryHavePREGInfoString.get(),
                             getter_Copies(tmpPREGInfo));

    if (tmpPREGInfo == nsnull)
    {
        mHavePREGInfo = kRegistryNoString;
        mProfileDataChanged = PR_TRUE;
    }
    
    // Get the StartWithLastProfile flag
    PRInt32 tempLong;
    rv = registry->GetInt(profilesTreeKey,
                           kRegistryStartWithLastString.get(),
                           &tempLong);
    if (NS_SUCCEEDED(rv))
        mStartWithLastProfile = tempLong;

    rv = registry->EnumerateSubtrees( profilesTreeKey, getter_AddRefs(enumKeys));
    if (NS_FAILED(rv)) return rv;

    rv = enumKeys->First();
    if (NS_FAILED(rv)) return rv;

    PRBool currentProfileValid = mCurrentProfile.IsEmpty();

    while (NS_OK != enumKeys->IsDone())
    {
        nsCOMPtr<nsISupports> base;

        rv = enumKeys->CurrentItem( getter_AddRefs(base) );
        if (NS_FAILED(rv)) return rv;

        // Get specific interface.
        nsCOMPtr <nsIRegistryNode> node;
        nsIID nodeIID = NS_IREGISTRYNODE_IID;

        rv = base->QueryInterface( nodeIID, getter_AddRefs(node));
        if (NS_FAILED(rv)) return rv;

        // Get node name.
        nsXPIDLString profile;
        nsXPIDLString isMigrated;
        nsXPIDLString NCProfileName;
        nsXPIDLString NCDeniedService;
        nsXPIDLString NCEmailAddress;
        nsXPIDLString NCHavePregInfo;

        rv = node->GetName(getter_Copies(profile));
        if (NS_FAILED(rv)) return rv;

        nsRegistryKey profKey;
        rv = node->GetKey(&profKey);
        if (NS_FAILED(rv)) return rv;

        rv = registry->GetString(profKey,
                                 kRegistryMigratedString.get(),
                                 getter_Copies(isMigrated));
        if (NS_FAILED(rv)) return rv;
        nsDependentString isMigratedString(isMigrated);

        // Not checking the return values of these variables as they
        // are for activation, they are optional and their values
        // do not call for a return
        registry->GetString(profKey,
                            kRegistryNCProfileNameString.get(),
                            getter_Copies(NCProfileName));

        registry->GetString(profKey,
                            kRegistryNCServiceDenialString.get(),
                            getter_Copies(NCDeniedService));

        registry->GetString(profKey,
                            kRegistryNCUserEmailString.get(),
                            getter_Copies(NCEmailAddress));

        registry->GetString(profKey,
                            kRegistryNCHavePREGInfoString.get(),
                            getter_Copies(NCHavePregInfo));

        // Make sure that mCurrentProfile is valid
        if (!mCurrentProfile.IsEmpty() && mCurrentProfile.Equals(profile))
          currentProfileValid = PR_TRUE;

        ProfileStruct*  profileItem     = new ProfileStruct();
        if (!profileItem)
            return NS_ERROR_OUT_OF_MEMORY;

        profileItem->updateProfileEntry     = PR_TRUE;
        profileItem->profileName      = NS_STATIC_CAST(const PRUnichar*, profile);

        PRInt64 tmpLongLong;
        rv = registry->GetLongLong(profKey,
                                   kRegistryCreationTimeString.get(),
                                   &tmpLongLong);
        if (NS_SUCCEEDED(rv))
            profileItem->creationTime = tmpLongLong;

        rv = registry->GetLongLong(profKey,
                                   kRegistryLastModTimeString.get(),
                                   &tmpLongLong);
        if (NS_SUCCEEDED(rv))
            profileItem->lastModTime = tmpLongLong;

        rv = profileItem->InternalizeLocation(registry, profKey, PR_FALSE);
        NS_ASSERTION(NS_SUCCEEDED(rv), "Internalizing profile location failed");
        // Not checking the error since most won't have this info
        profileItem->InternalizeMigratedFromLocation(registry, profKey);

        profileItem->isMigrated       = isMigratedString.Equals(kRegistryYesString);

        if (NCProfileName)
            profileItem->NCProfileName = NS_STATIC_CAST(const PRUnichar*, NCProfileName);

        if (NCDeniedService)
            profileItem->NCDeniedService = NS_STATIC_CAST(const PRUnichar*, NCDeniedService);

        if (NCEmailAddress)
            profileItem->NCEmailAddress = NS_STATIC_CAST(const PRUnichar*, NCEmailAddress);

        if (NCHavePregInfo)
            profileItem->NCHavePregInfo = NS_STATIC_CAST(const PRUnichar*, NCHavePregInfo);

        profileItem->isImportType = PR_FALSE;
        if (!mProfiles) {
            mProfiles = new nsVoidArray();

            if (!mProfiles) {
                delete profileItem;
                return NS_ERROR_OUT_OF_MEMORY;
            }
        }

        mProfiles->AppendElement((void*)profileItem);

        rv = enumKeys->Next();
        if (NS_FAILED(rv)) return rv;
    }

    if (!currentProfileValid)
      mCurrentProfile.SetLength(0);

    return rv;
}

// Return the number of 5x profiles.
void
nsProfileAccess::GetNumProfiles(PRInt32 *numProfiles)
{
    if (!numProfiles) {
        NS_ASSERTION(PR_FALSE, "invalid argument");
        return;
    }

    PRInt32 index, numElems = mProfiles->Count();

    *numProfiles = 0;

    for(index = 0; index < numElems; index++)
    {
        ProfileStruct* profileItem = (ProfileStruct *) (mProfiles->ElementAt(index));

        if (profileItem->isMigrated && !profileItem->isImportType)
        {
            (*numProfiles)++;
        }
    }
}

// Return the number of 4x (>=4.5 & < 5.0) profiles.
void
nsProfileAccess::GetNum4xProfiles(PRInt32 *numProfiles)
{
    if (!numProfiles) {
        NS_ASSERTION(PR_FALSE, "invalid argument");
        return;
    }

    PRInt32 index, numElems = mProfiles->Count();

    *numProfiles = 0;

    for(index = 0; index < numElems; index++)
    {
        ProfileStruct* profileItem = (ProfileStruct *) (mProfiles->ElementAt(index));

        if (!profileItem->isMigrated && !profileItem->isImportType)
        {
            (*numProfiles)++;
        }
    }
}

// If the application can't find the current profile,
// the first profile will be used as the current profile.
// This routine returns the first 5x profile.
// Caller must free up the string (firstProfile).
void
nsProfileAccess::GetFirstProfile(PRUnichar **firstProfile)
{
    if (!firstProfile) {
        NS_ASSERTION(PR_FALSE, "Invalid firstProfile pointer");
        return;
    }

    PRInt32 index, numElems = mProfiles->Count();

    *firstProfile = nsnull;

    for(index = 0; index < numElems; index++)
    {
        ProfileStruct* profileItem = (ProfileStruct *) (mProfiles->ElementAt(index));

        if (profileItem->isMigrated && !profileItem->isImportType)
        {
            *firstProfile = ToNewUnicode(profileItem->profileName);
            break;
        }
    }
}

// Set the current profile. Opearting directly on the tree.
// A separate struct should be maintained for the top level info.
// That way we can eliminate additional registry access. For
// now, we depend on registry operations.
// Capture the current profile information into mCurrentProfile.
void
nsProfileAccess::SetCurrentProfile(const PRUnichar *profileName)
{
    NS_ASSERTION(profileName, "Invalid profile name");

    mCurrentProfile = profileName;
    mProfileDataChanged = PR_TRUE;
}

// Return the current profile value.
// If mCurrent profile is already set, that value is returned.
// If there is only one profile that value is set to CurrentProfile.
void
nsProfileAccess::GetCurrentProfile(PRUnichar **profileName)
{
    *profileName = nsnull;

    if (!mCurrentProfile.IsEmpty() || mForgetProfileCalled)
    {
        *profileName = ToNewUnicode(mCurrentProfile);
    }

    // If there are profiles and profileName is not
    // set yet. Get the first one and set it as Current Profile.
    if (*profileName == nsnull)
    {
        GetFirstProfile(profileName); // We might not have any
        if (*profileName)
            SetCurrentProfile(*profileName);
    }
}

// Delete a profile from profile structs
void
nsProfileAccess::RemoveSubTree(const PRUnichar* profileName)
{
    NS_ASSERTION(profileName, "Invalid profile name");

    // delete this entry from the mProfiles array
    PRInt32     index = FindProfileIndex(profileName, PR_FALSE);

    if (index >= 0)
    {
        mProfiles->RemoveElementAt(index);

        if (mCurrentProfile.Equals(profileName))
        {
            mCurrentProfile.SetLength(0);
        }
    }
}

// Return the index of a given profiel from the arraf of profile structs.
PRInt32
nsProfileAccess::FindProfileIndex(const PRUnichar* profileName, PRBool forImport)
{
    NS_ASSERTION(profileName, "Invalid profile name");

    PRInt32 retval = -1;
    PRInt32 index, numElems = mProfiles->Count();

    for (index=0; index < numElems; index++)
    {
        ProfileStruct* profileItem = (ProfileStruct *) (mProfiles->ElementAt(index));

        if(profileItem->profileName.Equals(profileName) && (profileItem->isImportType == forImport))
        {
            retval = index;
            break;
        }
    }
    return retval;
}

// Flush profile information from the data structure to the registry.
nsresult
nsProfileAccess::UpdateRegistry(nsIFile* regName)
{
    nsresult rv;

    if (!mProfileDataChanged)
    {
        return NS_OK;
    }

    if (!regName)
    {
        regName = mNewRegFile;
    }

    nsCOMPtr<nsIRegistry> registry(do_CreateInstance(NS_REGISTRY_CONTRACTID, &rv));
    if (NS_FAILED(rv)) return rv;
    rv = registry->Open(regName);
    if (NS_FAILED(rv)) return rv;

    // Enumerate all subkeys (immediately) under the given node.
    nsCOMPtr<nsIEnumerator> enumKeys;
    nsRegistryKey profilesTreeKey;

    // Get the major subtree
    rv = registry->GetKey(nsIRegistry::Common,
                          kRegistryProfileSubtreeString.get(),
                          &profilesTreeKey);
    if (NS_FAILED(rv))
    {
        rv = registry->AddKey(nsIRegistry::Common,
                                kRegistryProfileSubtreeString.get(),
                                &profilesTreeKey);
        if (NS_FAILED(rv)) return rv;
    }

    // Set the current profile
    if (!mCurrentProfile.IsEmpty()) {

        rv = registry->SetString(profilesTreeKey,
                                 kRegistryCurrentProfileString.get(),
                                 mCurrentProfile.get());
        if (NS_FAILED(rv)) return rv;
    }

    // Set the registry version
    rv = registry->SetString(profilesTreeKey,
                             kRegistryVersionString.get(),
                             kRegistryCurrentVersion.get());
    if (NS_FAILED(rv)) return rv;

    // Set preg info
    rv = registry->SetString(profilesTreeKey,
                             kRegistryHavePREGInfoString.get(),
                             mHavePREGInfo.get());
    if (NS_FAILED(rv)) return rv;

    // Set the StartWithLastProfile flag
    rv = registry->SetInt(profilesTreeKey,
                          kRegistryStartWithLastString.get(),
                          mStartWithLastProfile);
    if (NS_FAILED(rv)) return rv;

    rv = registry->EnumerateSubtrees(profilesTreeKey, getter_AddRefs(enumKeys));
    if (NS_FAILED(rv)) return rv;

    rv = enumKeys->First();
    if (NS_FAILED(rv)) return rv;

    while (NS_OK != enumKeys->IsDone())
    {
        nsCOMPtr<nsISupports> base;

        rv = enumKeys->CurrentItem( getter_AddRefs(base) );
        if (NS_FAILED(rv)) return rv;

        // Get specific interface.
        nsCOMPtr <nsIRegistryNode> node;
        nsIID nodeIID = NS_IREGISTRYNODE_IID;

        rv = base->QueryInterface( nodeIID, getter_AddRefs(node));
        if (NS_FAILED(rv)) return rv;

        // Get node name.
        nsXPIDLString profile;
        nsXPIDLString isMigrated;
        nsXPIDLString directory;

        rv = node->GetName( getter_Copies(profile) );
        if (NS_FAILED(rv)) return rv;

        PRInt32 index = 0;

        index = FindProfileIndex(profile, PR_FALSE);

        if (index < 0)
        {
            // This profile is deleted.
            rv = registry->RemoveKey(profilesTreeKey, profile);
            if (NS_FAILED(rv)) return rv;
        }
        else
        {
            nsRegistryKey profKey;

            ProfileStruct* profileItem = (ProfileStruct *) (mProfiles->ElementAt(index));

            rv = node->GetKey(&profKey);
            if (NS_FAILED(rv)) return rv;

            rv = registry->SetString(profKey,
                                     kRegistryMigratedString.get(),
                                     profileItem->isMigrated ? kRegistryYesString.get() : kRegistryNoString.get());
            if (NS_FAILED(rv)) return rv;

            registry->SetString(profKey,
                                kRegistryNCProfileNameString.get(),
                                profileItem->NCProfileName.get());

            registry->SetString(profKey,
                                kRegistryNCServiceDenialString.get(),
                                profileItem->NCDeniedService.get());

            registry->SetString(profKey,
                                kRegistryNCUserEmailString.get(),
                                profileItem->NCEmailAddress.get());

            registry->SetString(profKey,
                                kRegistryNCHavePREGInfoString.get(),
                                profileItem->NCHavePregInfo.get());

            registry->SetLongLong(profKey,
                                  kRegistryCreationTimeString.get(),
                                  &profileItem->creationTime);

            registry->SetLongLong(profKey,
                                  kRegistryLastModTimeString.get(),
                                  &profileItem->lastModTime);

            rv = profileItem->ExternalizeLocation(registry, profKey);
            if (NS_FAILED(rv)) {
                NS_ASSERTION(PR_FALSE, "Could not update profile location");
                rv = enumKeys->Next();
                if (NS_FAILED(rv)) return rv;
                continue;
            }
            profileItem->ExternalizeMigratedFromLocation(registry, profKey);

            profileItem->updateProfileEntry = PR_FALSE;
        }
        rv = enumKeys->Next();
        if (NS_FAILED(rv)) return rv;
    }

    // Take care of new nodes
    PRInt32 numElems = mProfiles->Count();
    for (int i = 0; i < numElems; i++)
    {
        ProfileStruct* profileItem = (ProfileStruct *) (mProfiles->ElementAt(i));

        if (!profileItem->isImportType && profileItem->updateProfileEntry)
        {
            nsRegistryKey profKey;

            rv = registry->AddKey(profilesTreeKey,
                                    profileItem->profileName.get(),
                                    &profKey);
            if (NS_FAILED(rv)) return rv;

            rv = registry->SetString(profKey,
                                     kRegistryMigratedString.get(),
                                     profileItem->isMigrated ? kRegistryYesString.get() : kRegistryNoString.get());
            if (NS_FAILED(rv)) return rv;

            registry->SetString(profKey,
                                kRegistryNCProfileNameString.get(),
                                profileItem->NCProfileName.get());

            registry->SetString(profKey,
                                kRegistryNCServiceDenialString.get(),
                                profileItem->NCDeniedService.get());

            registry->SetString(profKey,
                                kRegistryNCUserEmailString.get(),
                                profileItem->NCEmailAddress.get());

            registry->SetString(profKey,
                                kRegistryNCHavePREGInfoString.get(),
                                profileItem->NCHavePregInfo.get());

            registry->SetLongLong(profKey,
                                  kRegistryCreationTimeString.get(),
                                  &profileItem->creationTime);

            registry->SetLongLong(profKey,
                                  kRegistryLastModTimeString.get(),
                                  &profileItem->lastModTime);

            rv = profileItem->ExternalizeLocation(registry, profKey);
            if (NS_FAILED(rv)) {
                NS_ASSERTION(PR_FALSE, "Could not update profile location");
                continue;
            }
            profileItem->ExternalizeMigratedFromLocation(registry, profKey);

            profileItem->updateProfileEntry = PR_FALSE;
        }
    }

    mProfileDataChanged = PR_FALSE;

    return rv;
}

nsresult
nsProfileAccess::GetOriginalProfileDir(const PRUnichar *profileName, nsILocalFile **originalDir)
{
    NS_ENSURE_ARG(profileName);
    NS_ENSURE_ARG_POINTER(originalDir);
    *originalDir = nsnull;
    nsresult rv = NS_OK;

    PRInt32 index = FindProfileIndex(profileName, PR_TRUE);
    if (index >= 0)
    {
        ProfileStruct* profileItem = (ProfileStruct *) (mProfiles->ElementAt(index));
        nsCOMPtr<nsILocalFile> profileDir;
        rv = profileItem->GetResolvedProfileDir(getter_AddRefs(profileDir));
        if (NS_SUCCEEDED(rv) && profileDir)
        {
#ifdef XP_MAC
            PRBool exists;
            rv = profileDir->Exists(&exists);
            if (NS_FAILED(rv))
                return rv;
            if (exists) {
                PRBool inTrash;
                nsCOMPtr<nsIFile> trashFolder;

                rv = NS_GetSpecialDirectory(NS_MAC_TRASH_DIR, getter_AddRefs(trashFolder));
                if (NS_FAILED(rv)) return rv;
                rv = trashFolder->Contains(profileDir, PR_TRUE, &inTrash);
                if (NS_FAILED(rv)) return rv;
                if (inTrash) {
                    return NS_ERROR_FILE_NOT_FOUND;
                }
            }
#endif
            NS_IF_ADDREF(*originalDir = profileDir);
        }
        return rv;
    }
    return NS_ERROR_FAILURE;
}

nsresult
nsProfileAccess::SetMigratedFromDir(const PRUnichar *profileName, nsILocalFile *originalDir)
{
    NS_ENSURE_ARG(profileName);
    NS_ENSURE_ARG(originalDir);

    PRInt32 index = FindProfileIndex(profileName, PR_FALSE);
    if (index >= 0)
    {
        ProfileStruct* profileItem = (ProfileStruct *) (mProfiles->ElementAt(index));
        profileItem->migratedFrom = originalDir;
        profileItem->updateProfileEntry = PR_TRUE;
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

nsresult
nsProfileAccess::SetProfileLastModTime(const PRUnichar *profileName, PRInt64 lastModTime)
{
    NS_ENSURE_ARG(profileName);

    PRInt32 index = FindProfileIndex(profileName, PR_FALSE);
    if (index >= 0)
    {
        ProfileStruct* profileItem = (ProfileStruct *) (mProfiles->ElementAt(index));
        profileItem->lastModTime = lastModTime;
        profileItem->updateProfileEntry = PR_TRUE;
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

nsresult
nsProfileAccess::GetStartWithLastUsedProfile(PRBool *aStartWithLastUsedProfile)
{
    NS_ENSURE_ARG_POINTER(aStartWithLastUsedProfile);
    *aStartWithLastUsedProfile = mStartWithLastProfile;
    return NS_OK;
}

nsresult
nsProfileAccess::SetStartWithLastUsedProfile(PRBool aStartWithLastUsedProfile)
{
    mStartWithLastProfile = aStartWithLastUsedProfile;
    mProfileDataChanged = PR_TRUE;
    return NS_OK;
}

// Return the list of profiles, 4x, 5x, or both.
nsresult
nsProfileAccess::GetProfileList(PRInt32 whichKind, PRUint32 *length, PRUnichar ***result)
{
    NS_ENSURE_ARG_POINTER(length);
    *length = 0;
    NS_ENSURE_ARG_POINTER(result);
    *result = nsnull;

    nsresult rv = NS_OK;
    PRInt32 count, localLength = 0;
    PRUnichar **outArray, **next;
    PRInt32 numElems = mProfiles->Count();
    PRInt32 profilesCount;

    switch (whichKind)
    {
        case nsIProfileInternal::LIST_ONLY_NEW:
            GetNumProfiles(&count);
            break;
        case nsIProfileInternal::LIST_ONLY_OLD:
            GetNum4xProfiles(&count);
            break;
        case nsIProfileInternal::LIST_ALL:
            GetNum4xProfiles(&count);
            GetNumProfiles(&profilesCount);
            count += profilesCount;
            break;
        case nsIProfileInternal::LIST_FOR_IMPORT:
            GetNum4xProfiles(&count);
            GetNumProfiles(&profilesCount);
            count = numElems - (count + profilesCount);
            break;
        default:
            NS_ASSERTION(PR_FALSE, "Bad parameter");
            return NS_ERROR_INVALID_ARG;
    }

    next = outArray = (PRUnichar **)nsMemory::Alloc(count * sizeof(PRUnichar *));
    if (!outArray)
        return NS_ERROR_OUT_OF_MEMORY;

    for (PRInt32 index=0; index < numElems && localLength < count; index++)
    {
        ProfileStruct* profileItem = (ProfileStruct *) (mProfiles->ElementAt(index));

        if (whichKind == nsIProfileInternal::LIST_ONLY_OLD && (profileItem->isMigrated || profileItem->isImportType))
            continue;
        else if (whichKind == nsIProfileInternal::LIST_ONLY_NEW && (!profileItem->isMigrated || profileItem->isImportType))
            continue;
        else if (whichKind == nsIProfileInternal::LIST_ALL && profileItem->isImportType)
            continue;
        else if (whichKind == nsIProfileInternal::LIST_FOR_IMPORT && !profileItem->isImportType)
            continue;
        *next = ToNewUnicode(profileItem->profileName);
        if (*next == nsnull)
        {
            rv = NS_ERROR_OUT_OF_MEMORY;
            break;
        }
        next++;
        localLength++;
    }

    if (NS_SUCCEEDED(rv))
    {
        *result = outArray;
        *length = localLength;
    }
    else
    {
        while (--next >= outArray)
            nsMemory::Free(*next);
        nsMemory::Free(outArray);
    }

    return rv;
}

// Return a boolean based on the profile existence.
PRBool
nsProfileAccess::ProfileExists(const PRUnichar *profileName)
{
    NS_ASSERTION(profileName, "Invalid profile name");

    PRBool exists = PR_FALSE;
    PRInt32 numElems = mProfiles->Count();

    for (PRInt32 index=0; index < numElems; index++)
    {
        ProfileStruct* profileItem = (ProfileStruct *) (mProfiles->ElementAt(index));
        if (!profileItem->isImportType && profileItem->profileName.Equals(profileName))
        {
            exists = PR_TRUE;
            break;
        }
    }
    return exists;
}

// Capture the 4x profile information from the old registry (4x)
nsresult
nsProfileAccess::Get4xProfileInfo(nsIFile *registryFile, PRBool fromImport)
{
    nsresult rv = NS_OK;
    if (fromImport && m4xProfilesAdded)
        return rv;

    nsCAutoString charSet;
    rv = GetPlatformCharset(charSet);
    if (NS_FAILED(rv)) return rv;

#if defined(XP_WIN) || defined(XP_OS2) || defined(XP_MAC) || defined(XP_MACOSX)
    NS_ENSURE_ARG(registryFile);

    nsCOMPtr<nsIRegistry> oldReg(do_CreateInstance(NS_REGISTRY_CONTRACTID, &rv));
    if (NS_FAILED(rv)) return rv;
    rv = oldReg->Open(registryFile);
    if (NS_FAILED(rv)) return rv;

    // Enumerate 4x tree and create an array of that information.
    // Enumerate all subkeys (immediately) under the given node.
    nsCOMPtr<nsIEnumerator> enumKeys;

    rv = oldReg->EnumerateSubtrees(nsIRegistry::Users,
                                   getter_AddRefs(enumKeys));
    if (NS_FAILED(rv)) return rv;

    rv = enumKeys->First();
    if (NS_FAILED(rv)) return rv;

    if (fromImport)
        m4xProfilesAdded = PR_TRUE;
    // Enumerate subkeys till done.
    while( (NS_OK != enumKeys->IsDone()))
    {
        nsCOMPtr<nsISupports> base;
        rv = enumKeys->CurrentItem(getter_AddRefs(base));
        if (NS_FAILED(rv)) return rv;

        // Get specific interface.
        nsCOMPtr <nsIRegistryNode> node;
        nsIID nodeIID = NS_IREGISTRYNODE_IID;
        rv = base->QueryInterface( nodeIID, getter_AddRefs(node));
        if (NS_FAILED(rv)) return rv;

        nsXPIDLString profile;
        rv = node->GetName(getter_Copies(profile));
        if (NS_FAILED(rv)) return rv;

        // Unescape is done on the profileName to interpret special characters like %, _ etc.
        // For example something like %20 would probably be interpreted as a space
        // There is some problem I guess in sending a space as itself
        // NOTE: This needs to be done BEFORE the test for existence.

#if defined(XP_MAC) || defined(XP_MACOSX)
        // 4.x profiles coming from japanese machine are already in unicode.
        // So, there is no need to decode into unicode further.
        NS_ConvertUCS2toUTF8 temp(profile);
        nsCAutoString profileName(nsUnescape(temp.BeginWriting()));
        nsAutoString convertedProfName(NS_ConvertUTF8toUCS2(profileName).get());
#else
        nsCAutoString temp; temp.AssignWithConversion(profile);

        nsCAutoString profileName(nsUnescape(temp.BeginWriting()));
        nsAutoString convertedProfName;
        ConvertStringToUnicode(charSet, profileName.get(), convertedProfName);
#endif

        PRBool exists = PR_FALSE;
        if (!fromImport) {
            exists = ProfileExists(convertedProfName.get());
            if (exists)
            {
                rv = enumKeys->Next();
                if (NS_FAILED(rv)) return rv;
                continue;
            }
        }

        nsRegistryKey key;
        rv = node->GetKey(&key);
        if (NS_FAILED(rv)) return rv;

        ProfileStruct*  profileItem  = new ProfileStruct();
        if (!profileItem)
            return NS_ERROR_OUT_OF_MEMORY;

        profileItem->updateProfileEntry    = PR_TRUE;
        profileItem->profileName  = convertedProfName;
        rv = profileItem->InternalizeLocation(oldReg, key, PR_TRUE);
        NS_ASSERTION(NS_SUCCEEDED(rv), "Could not get 4x profile location");
        profileItem->isMigrated = PR_FALSE;
        profileItem->isImportType = fromImport;

        SetValue(profileItem);

        rv = enumKeys->Next();
        if (NS_FAILED(rv)) return rv;
    }

#elif defined (XP_BEOS)
#else
/* XP_UNIX */
        nsCAutoString unixProfileName( PR_GetEnv(PROFILE_NAME_ENVIRONMENT_VARIABLE) );
        nsCAutoString unixProfileDirectory( PR_GetEnv(PROFILE_HOME_ENVIRONMENT_VARIABLE) );

        if (unixProfileName.IsEmpty()  ||
            unixProfileDirectory.IsEmpty())
        {
            unixProfileDirectory = PR_GetEnv(HOME_ENVIRONMENT_VARIABLE);
            unixProfileName = PR_GetEnv(LOGNAME_ENVIRONMENT_VARIABLE);
            if ( unixProfileName.IsEmpty() ) {
              unixProfileName = PR_GetEnv(USER_ENVIRONMENT_VARIABLE);
            }
            if ( unixProfileName.IsEmpty() ) {
              unixProfileName = DEFAULT_UNIX_PROFILE_NAME;
            }
        }

        PRBool exists = PR_FALSE;
        if (!fromImport) {
            exists = ProfileExists(NS_ConvertASCIItoUCS2(unixProfileName).get());
            if (exists)
            {
                return NS_OK;
            }
        }
        if ( ! unixProfileName.IsEmpty() && ! unixProfileDirectory.IsEmpty() ) {
            nsCAutoString profileLocation(unixProfileDirectory);
            profileLocation += "/.netscape";

            nsCOMPtr<nsILocalFile> fileInNSDir;
            rv = NS_NewNativeLocalFile(profileLocation + NS_LITERAL_CSTRING("/preferences.js"),
                                       PR_TRUE,
                                       getter_AddRefs(fileInNSDir));
            if (NS_FAILED(rv))
              return rv;
            rv = fileInNSDir->Exists(&exists);
#ifdef DEBUG
            printf("%s/preferences.js exists:  %d\n",profileLocation.get(), NS_SUCCEEDED(rv) && exists);
#endif
            if (NS_SUCCEEDED(rv) && exists) {
                ProfileStruct*  profileItem     = new ProfileStruct();
                if (!profileItem)
                    return NS_ERROR_OUT_OF_MEMORY;

                profileItem->updateProfileEntry = PR_TRUE;

                CopyASCIItoUTF16(unixProfileName, profileItem->profileName);

                nsCOMPtr<nsILocalFile> localFile;
                rv = NS_NewNativeLocalFile(profileLocation, PR_TRUE, getter_AddRefs(localFile));
                if (NS_FAILED(rv)) return rv;
                profileItem->SetResolvedProfileDir(localFile);
                profileItem->isMigrated = PR_FALSE;
                profileItem->isImportType = fromImport;

                SetValue(profileItem);
            }
            else {
#ifdef DEBUG
                printf("no 4.x profile\n");
#endif
            }
        }
#endif /* XP_UNIX */

    return rv;
}


// Set the PREG flag to indicate if that info exists
void
nsProfileAccess::SetPREGInfo(const char* pregInfo)
{
    NS_ASSERTION(pregInfo, "Invalid pregInfo");

    // This is always going to be just a yes/no string
    mHavePREGInfo.AssignWithConversion(pregInfo);
}

//Get the for PREG info.
void
nsProfileAccess::CheckRegString(const PRUnichar *profileName, char **info)
{
    NS_ASSERTION(profileName, "Invalid profile name");
    NS_ASSERTION(info, "Invalid info pointer");

    *info = nsnull;
    PRInt32 index = 0;

    index = FindProfileIndex(profileName, PR_FALSE);

    if (index >= 0 )
    {
        ProfileStruct* profileItem = (ProfileStruct *) (mProfiles->ElementAt(index));

        if (!profileItem->NCHavePregInfo.IsEmpty()) {
            *info = ToNewCString(profileItem->NCHavePregInfo);
        }
        else
        {
            *info = ToNewCString(kRegistryNoString);
        }
    }
}


// Clear the profile member data structure
// We need to fill in the data from the new registry
nsresult
nsProfileAccess::ResetProfileMembers()
{
    FreeProfileMembers(mProfiles);
    mProfiles = new nsVoidArray();
    return NS_OK;
}

nsresult
nsProfileAccess::DetermineForceMigration(PRBool *forceMigration)
{
    if (!forceMigration) return NS_ERROR_NULL_POINTER;

    PRInt32 numProfiles;
    GetNumProfiles(&numProfiles);

    if (numProfiles > 0) {
        // we have some 6.0 profiles, don't force migration:
        *forceMigration = PR_FALSE;
            return NS_OK;
    }

    // even if we don't any 4.x profiles, running -installer is safe.  so do it
    *forceMigration = PR_TRUE;
    return NS_OK;
}

// **********************************************************************
// class ProfileStruct
// **********************************************************************

ProfileStruct::ProfileStruct() :
    isMigrated(PR_FALSE), updateProfileEntry(PR_FALSE),
    isImportType(PR_FALSE),
    creationTime(LL_ZERO), lastModTime(LL_ZERO)
{
}

ProfileStruct::ProfileStruct(const ProfileStruct& src)
{
    *this = src;
}

ProfileStruct& ProfileStruct::operator=(const ProfileStruct& rhs)
{
    profileName = rhs.profileName;
    isMigrated = rhs.isMigrated;
    NCProfileName = rhs.NCProfileName;
    NCDeniedService = rhs.NCDeniedService;
    NCEmailAddress = rhs.NCEmailAddress;
    NCHavePregInfo = rhs.NCHavePregInfo;
    updateProfileEntry = rhs.updateProfileEntry;
    isImportType = rhs.isImportType;
    creationTime = rhs.creationTime;
    lastModTime = rhs.lastModTime;

    nsresult rv;
        nsCOMPtr<nsIFile> file;

    resolvedLocation = nsnull;
    if (rhs.resolvedLocation) {
        regLocationData.Truncate(0);
        rv = rhs.resolvedLocation->Clone(getter_AddRefs(file));
        if (NS_SUCCEEDED(rv))
            resolvedLocation = do_QueryInterface(file);
        file = nsnull;
    }
    else
        regLocationData = rhs.regLocationData;

    migratedFrom = nsnull;
    if (rhs.migratedFrom) {
        rv = rhs.migratedFrom->Clone(getter_AddRefs(file));
        if (NS_SUCCEEDED(rv))
            migratedFrom = do_QueryInterface(file);
    }

    return *this;
}

nsresult ProfileStruct::GetResolvedProfileDir(nsILocalFile **aDirectory)
{
    NS_ENSURE_ARG_POINTER(aDirectory);
    *aDirectory = nsnull;
    if (resolvedLocation)
    {
        *aDirectory = resolvedLocation;
        NS_ADDREF(*aDirectory);
        return NS_OK;
    }
    return NS_ERROR_FILE_NOT_FOUND; // The only reason it would not exist.
}

nsresult ProfileStruct::SetResolvedProfileDir(nsILocalFile *aDirectory)
{
    NS_ENSURE_ARG(aDirectory);
    resolvedLocation = aDirectory;
    regLocationData.SetLength(0);
    return NS_OK;
}

nsresult ProfileStruct::CopyProfileLocation(ProfileStruct *destStruct)
{
    if (resolvedLocation)
    {
        nsCOMPtr<nsIFile> file;
        nsresult rv = resolvedLocation->Clone(getter_AddRefs(file));
        if (NS_SUCCEEDED(rv))
            destStruct->resolvedLocation = do_QueryInterface(file, &rv);
        if (NS_FAILED(rv)) return rv;
    }
    destStruct->regLocationData = regLocationData;

    return NS_OK;
}

nsresult ProfileStruct::InternalizeLocation(nsIRegistry *aRegistry, nsRegistryKey profKey, PRBool is4x)
{
    nsresult rv;
    nsCOMPtr<nsILocalFile> tempLocal;

    // Reset ourselves
    regLocationData.SetLength(0);
    resolvedLocation = nsnull;

    if (is4x)
    {
        nsXPIDLString profLoc;

        rv = aRegistry->GetString( profKey, NS_LITERAL_STRING("ProfileLocation").get(), getter_Copies(profLoc));
        if (NS_FAILED(rv)) return rv;
        regLocationData = profLoc;

#if defined(XP_MAC) || defined(XP_MACOSX)
        // 4.x profiles coming from japanese machine are already in unicode.
        // So, there is no need to decode into unicode further.

        // Unescape profile location
        NS_ConvertUCS2toUTF8 tempLoc(profLoc);
        nsCAutoString profileLocation(nsUnescape(tempLoc.BeginWriting()));
        nsAutoString convertedProfLoc(NS_ConvertUTF8toUCS2(profileLocation).get());
#else
        nsCAutoString charSet;
        rv = GetPlatformCharset(charSet);
        if (NS_FAILED(rv)) return rv;

        // Unescape profile location and convert it to the right format
        nsCAutoString tempLoc; tempLoc.AssignWithConversion(profLoc);

        nsCAutoString profileLocation(nsUnescape(tempLoc.BeginWriting()));
        nsAutoString convertedProfLoc;
        ConvertStringToUnicode(charSet, profileLocation.get(), convertedProfLoc);
#endif

        // Now we have a unicode path - make it into a file
#if defined(XP_MACOSX)
        // This is an HFS style path, which can't be used with nsIFile, so convert it.
        rv = NS_ERROR_FAILURE;
        CFStringRef pathStrRef = ::CFStringCreateWithCharacters(NULL,
                                      convertedProfLoc.get(), convertedProfLoc.Length());
        if (pathStrRef)
        {
            CFURLRef pathURLRef = ::CFURLCreateWithFileSystemPath(NULL, pathStrRef, kCFURLHFSPathStyle, true);
            if (pathURLRef)
            {
                rv = NS_NewNativeLocalFile(EmptyCString(), PR_TRUE, getter_AddRefs(tempLocal));
                if (NS_SUCCEEDED(rv))
                {
                    nsCOMPtr<nsILocalFileMac> tempLocalMac(do_QueryInterface(tempLocal));
                    rv = tempLocalMac->InitWithCFURL(pathURLRef);
                }
                ::CFRelease(pathURLRef);
            }
            ::CFRelease(pathStrRef);
        }
#else        
        rv = NS_NewLocalFile(convertedProfLoc, PR_TRUE, getter_AddRefs(tempLocal));
#endif
    }
    else
    {
        nsXPIDLString regData;

        rv = aRegistry->GetString(profKey,
                                  kRegistryDirectoryString.get(),
                                  getter_Copies(regData));
        if (NS_FAILED(rv)) return rv;
        regLocationData = regData;

#if defined(XP_MAC) || defined(XP_MACOSX)
            rv = NS_NewNativeLocalFile(EmptyCString(), PR_TRUE, getter_AddRefs(tempLocal));
        if (NS_SUCCEEDED(rv)) // regLocationData is ASCII so no loss
            rv = tempLocal->SetPersistentDescriptor(NS_LossyConvertUCS2toASCII(regLocationData));
#else
        rv = NS_NewLocalFile(regLocationData, PR_TRUE, getter_AddRefs(tempLocal));
#endif
    }

    if (NS_SUCCEEDED(rv) && tempLocal)
    {
        PRBool exists;
        if (NS_SUCCEEDED(tempLocal->Exists(&exists)) && exists)
            SetResolvedProfileDir(tempLocal);
    }

    return NS_OK;
}

nsresult ProfileStruct::ExternalizeLocation(nsIRegistry *aRegistry, nsRegistryKey profKey)
{
    nsresult rv;

    if (resolvedLocation)
    {
        nsAutoString regData;

#if defined(XP_MAC) || defined(XP_MACOSX)
        PRBool leafCreated;
        nsCAutoString descBuf;

        // It must exist before we try to use GetPersistentDescriptor
        rv = EnsureDirPathExists(resolvedLocation, &leafCreated);
        if (NS_FAILED(rv)) return rv;
        rv = resolvedLocation->GetPersistentDescriptor(descBuf);
        if (NS_FAILED(rv)) return rv;
        if (leafCreated)
            resolvedLocation->Remove(PR_FALSE);
        regData = NS_ConvertUTF8toUCS2(descBuf);
#else
        rv = resolvedLocation->GetPath(regData);
        if (NS_FAILED(rv)) return rv;
#endif

        rv = aRegistry->SetString(profKey,
                                 kRegistryDirectoryString.get(),
                                 regData.get());

    }
    else if (!regLocationData.IsEmpty())
    {
        // Write the original data back out - maybe it can be resolved later.
        rv = aRegistry->SetString(profKey,
                                 kRegistryDirectoryString.get(),
                                 regLocationData.get());
    }
    else
    {
        NS_ASSERTION(PR_FALSE, "ProfileStruct has no location data!");
        rv = NS_ERROR_FAILURE;
    }

    return rv;
}

nsresult ProfileStruct::InternalizeMigratedFromLocation(nsIRegistry *aRegistry, nsRegistryKey profKey)
{
    nsresult rv;
    nsXPIDLCString regData;
    nsCOMPtr<nsILocalFile> tempLocal;

    rv = aRegistry->GetStringUTF8(profKey,
                                  kRegistryMigratedFromString.get(),
                                  getter_Copies(regData));
    if (NS_SUCCEEDED(rv))
    {
#if defined(XP_MAC) || defined(XP_MACOSX)
        rv = NS_NewLocalFile(nsString(), PR_TRUE, getter_AddRefs(tempLocal));
        if (NS_SUCCEEDED(rv))
        {
            // The persistent desc on Mac is base64 encoded so plain ASCII
            rv = tempLocal->SetPersistentDescriptor(regData);
            if (NS_SUCCEEDED(rv))
                migratedFrom = tempLocal;
        }
#else
        rv = NS_NewLocalFile(NS_ConvertUTF8toUCS2(regData), PR_TRUE, getter_AddRefs(tempLocal));
        if (NS_SUCCEEDED(rv))
            migratedFrom = tempLocal;
#endif
    }
    return rv;
}

nsresult ProfileStruct::ExternalizeMigratedFromLocation(nsIRegistry *aRegistry, nsRegistryKey profKey)
{
    nsresult rv = NS_OK;
    nsCAutoString regData;

    if (migratedFrom)
    {
#if defined(XP_MAC) || defined(XP_MACOSX)
        rv = migratedFrom->GetPersistentDescriptor(regData);
#else
        nsAutoString path;
        rv = migratedFrom->GetPath(path);
        regData = NS_ConvertUCS2toUTF8(path);
#endif

        if (NS_SUCCEEDED(rv))
            rv = aRegistry->SetStringUTF8(profKey,
                                          kRegistryMigratedFromString.get(),
                                          regData.get());
    }
    return rv;
}

nsresult ProfileStruct::EnsureDirPathExists(nsILocalFile *aDir, PRBool *wasCreated)
{
    NS_ENSURE_ARG(aDir);
    NS_ENSURE_ARG_POINTER(wasCreated);
    *wasCreated = PR_FALSE;

    nsresult rv;
    PRBool exists;
    rv = aDir->Exists(&exists);
    if (NS_SUCCEEDED(rv) && !exists)
    {
        rv = aDir->Create(nsIFile::DIRECTORY_TYPE, 0700);
        *wasCreated = NS_SUCCEEDED(rv);
    }
    return rv;
}

