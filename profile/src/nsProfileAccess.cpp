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
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsProfileAccess.h"
#include "nsProfile.h"

#include "pratom.h"
#include "prmem.h"
#include "plstr.h"
#include "prenv.h"

#include "nsIEnumerator.h"
#include "prprf.h"
#include "nsSpecialSystemDirectory.h"
#include "nsCOMPtr.h"
#include "nsIComponentManager.h"
#include "nsFileStream.h"
#include "nsEscape.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsILocalFile.h"
#include "nsReadableUtils.h"

#define NS_IMPL_IDS
#include "nsICharsetConverterManager.h"
#include "nsIPlatformCharset.h"
#undef NS_IMPL_IDS

#define MAX_PERSISTENT_DATA_SIZE  1000
#define NUM_HEX_BYTES             8
#define ISHEX(c) ( ((c) >= '0' && (c) <= '9') || ((c) >= 'a' && (c) <= 'f') || ((c) >= 'A' && (c) <= 'F') )

#if defined (XP_UNIX)
#define USER_ENVIRONMENT_VARIABLE "USER"
#define LOGNAME_ENVIRONMENT_VARIABLE "LOGNAME"
#define HOME_ENVIRONMENT_VARIABLE "HOME"
#define PROFILE_NAME_ENVIRONMENT_VARIABLE "PROFILE_NAME"
#define PROFILE_HOME_ENVIRONMENT_VARIABLE "PROFILE_HOME"
#define DEFAULT_UNIX_PROFILE_NAME "default"
#elif defined (XP_BEOS)
#endif

// IID and CIDs of all the services needed
static NS_DEFINE_CID(kRegistryCID, NS_REGISTRY_CID);
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

#define kRegistryVersionString (NS_LITERAL_STRING("Version"))
#define kRegistryVersion_1_0 (NS_LITERAL_STRING("1.0"))
#define kRegistryCurrentVersion (NS_LITERAL_STRING("1.0"))

// **********************************************************************
// class nsProfileAccess
// **********************************************************************

/*
 * Constructor/Destructor
 * FillProfileInfo reads the registry and fills profileStructs
 */
nsProfileAccess::nsProfileAccess()
{
    mProfileDataChanged	 =  PR_FALSE;
    mForgetProfileCalled =  PR_FALSE;
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
GetPlatformCharset(nsString& aCharset)
{
    nsresult rv;

    // we may cache it since the platform charset will not change through application life
    nsCOMPtr <nsIPlatformCharset> platformCharset = do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv) && platformCharset) {
        rv = platformCharset->GetCharset(kPlatformCharsetSel_FileName, aCharset);
    }
    if (NS_FAILED(rv)) {
        aCharset.AssignWithConversion("ISO-8859-1");  // use ISO-8859-1 in case of any error
    }
    return rv;
}

// Apply a charset conversion from the given charset to Unicode for input C string.
nsresult 
ConvertStringToUnicode(nsString& aCharset, const char* inString, nsAWritableString& outString)
{
    nsresult rv;
    // convert result to unicode
    nsCOMPtr<nsICharsetConverterManager> ccm = 
             do_GetService(kCharsetConverterManagerCID, &rv);

    if(NS_SUCCEEDED(rv)) {
        nsCOMPtr <nsIUnicodeDecoder> decoder; // this may be cached
        rv = ccm->GetUnicodeDecoder(&aCharset, getter_AddRefs(decoder));

        if(NS_SUCCEEDED(rv) && decoder) {
            PRInt32 uniLength = 0;
            PRInt32 srcLength = nsCRT::strlen(inString);
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
    index = FindProfileIndex(profileName);
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
    NS_ASSERTION(aProfile, "Invalid profile");

    PRInt32	index = 0;
    PRBool isNewProfile = PR_FALSE;
    ProfileStruct* profileItem;

    index = FindProfileIndex(aProfile->profileName.get());

    if (index >= 0)
    {
        profileItem = (ProfileStruct *) (mProfiles->ElementAt(index));
    }
    else
    {
        isNewProfile = PR_TRUE;

        profileItem	= new ProfileStruct();
        if (!profileItem)
            return NS_ERROR_OUT_OF_MEMORY;

        profileItem->profileName        = aProfile->profileName;
    }

    aProfile->CopyProfileLocation(profileItem);

    profileItem->isMigrated = aProfile->isMigrated;

    profileItem->updateProfileEntry = PR_TRUE;

    if (!aProfile->NCProfileName.IsEmpty())
        profileItem->NCProfileName = aProfile->NCProfileName;

    if (!aProfile->NCDeniedService.IsEmpty())
        profileItem->NCDeniedService = aProfile->NCDeniedService;

    if (!aProfile->NCEmailAddress.IsEmpty())
        profileItem->NCEmailAddress = aProfile->NCEmailAddress;

    if (!aProfile->NCHavePregInfo.IsEmpty())
        profileItem->NCHavePregInfo = aProfile->NCHavePregInfo;


    if (isNewProfile) {
        if (!mProfiles)
            mProfiles = new nsVoidArray();

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
    nsXPIDLCString regFile;
    PRBool fixRegEntries = PR_FALSE;

    if (regName)
        regName->GetPath(getter_Copies(regFile));

    nsCOMPtr<nsIRegistry> registry(do_CreateInstance(NS_REGISTRY_CONTRACTID, &rv));
    if (NS_FAILED(rv)) return rv;
    rv = registry->Open(regFile);
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

    if (tmpVersion == nsnull)
    {
        fixRegEntries = PR_TRUE;
        mProfileDataChanged = PR_TRUE;
    }

    // Get the preg info
    rv = registry->GetString(profilesTreeKey, 
                             kRegistryHavePREGInfoString.get(), 
                             getter_Copies(tmpPREGInfo));

    if (tmpPREGInfo == nsnull)
    {
        mHavePREGInfo = kRegistryNoString;
        mProfileDataChanged = PR_TRUE;
    }

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

        ProfileStruct*  profileItem	= new ProfileStruct();
        if (!profileItem)
            return NS_ERROR_OUT_OF_MEMORY;

        profileItem->updateProfileEntry     = PR_TRUE;
        profileItem->profileName      = NS_STATIC_CAST(const PRUnichar*, profile);        
        
        rv = profileItem->InternalizeLocation(registry, profKey, PR_FALSE, fixRegEntries);
        NS_ASSERTION(NS_SUCCEEDED(rv), "Internalizing profile location failed");
        
        profileItem->isMigrated       = isMigratedString.Equals(kRegistryYesString);

        if (NCProfileName)
            profileItem->NCProfileName = NS_STATIC_CAST(const PRUnichar*, NCProfileName);

        if (NCDeniedService)
            profileItem->NCDeniedService = NS_STATIC_CAST(const PRUnichar*, NCDeniedService);

        if (NCEmailAddress)
            profileItem->NCEmailAddress = NS_STATIC_CAST(const PRUnichar*, NCEmailAddress);

        if (NCHavePregInfo)
            profileItem->NCHavePregInfo = NS_STATIC_CAST(const PRUnichar*, NCHavePregInfo);

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

        if (profileItem->isMigrated)
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

        if (!profileItem->isMigrated)
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

        if (profileItem->isMigrated)
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
    PRInt32	index = FindProfileIndex(profileName);

    if (index >= 0)
    {
        ProfileStruct* profileItem = (ProfileStruct *) (mProfiles->ElementAt(index));

        mProfiles->RemoveElementAt(index);

        if (mCurrentProfile.Equals(profileName))
        {
            mCurrentProfile.SetLength(0);
        }
    }
}
    
// Return the index of a given profiel from the arraf of profile structs.
PRInt32
nsProfileAccess::FindProfileIndex(const PRUnichar* profileName)
{    
    NS_ASSERTION(profileName, "Invalid profile name");

    PRInt32 retval = -1;
    PRInt32 index, numElems = mProfiles->Count();

    for (index=0; index < numElems; index++)
    {
        ProfileStruct* profileItem = (ProfileStruct *) (mProfiles->ElementAt(index));

        if(profileItem->profileName.Equals(profileName))
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
    nsXPIDLCString regFile;

    if (!mProfileDataChanged)
    {
        return NS_OK;
    }

    if (!regName)
    {
        if (mNewRegFile)
            mNewRegFile->GetPath(getter_Copies(regFile));   
    }
    else
    {
        regName->GetPath(getter_Copies(regFile));   
    }

    nsCOMPtr<nsIRegistry> registry(do_CreateInstance(NS_REGISTRY_CONTRACTID, &rv));
    if (NS_FAILED(rv)) return rv;
    rv = registry->Open(regFile);
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

        index = FindProfileIndex(profile);

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

            rv = profileItem->ExternalizeLocation(registry, profKey);
            if (NS_FAILED(rv)) {
                NS_ASSERTION(PR_FALSE, "Could not update profile location");
                rv = enumKeys->Next();
                if (NS_FAILED(rv)) return rv;
                continue;
            }

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

        if (profileItem->updateProfileEntry)
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

            rv = profileItem->ExternalizeLocation(registry, profKey);
            if (NS_FAILED(rv)) {
                NS_ASSERTION(PR_FALSE, "Could not update profile location");
                continue;
            }

            profileItem->updateProfileEntry = PR_FALSE;
        }
    }

    mProfileDataChanged = PR_FALSE;

    return rv;
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
    
    switch (whichKind)
    {
        case nsIProfileInternal::LIST_ONLY_NEW:
            GetNumProfiles(&count);
            break;
        case nsIProfileInternal::LIST_ONLY_OLD:
            GetNum4xProfiles(&count);
            break;
        case nsIProfileInternal::LIST_ALL:
            count = numElems;
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
        
        if (whichKind == nsIProfileInternal::LIST_ONLY_OLD && profileItem->isMigrated)
            continue;
        else if (whichKind == nsIProfileInternal::LIST_ONLY_NEW && !profileItem->isMigrated)
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
        if (profileItem->profileName.Equals(profileName))
        {
            exists = PR_TRUE;
            break;
        }
    }
    return exists;
}

// Capture the 4x profile information from the old registry (4x)
nsresult
nsProfileAccess::Get4xProfileInfo(const char *registryName)
{
    nsresult rv = NS_OK;

    nsAutoString charSet;
    rv = GetPlatformCharset(charSet);
    if (NS_FAILED(rv)) return rv;

#if defined(XP_PC) || defined(XP_MAC)
    NS_ASSERTION(registryName, "Invalid registryName");

    nsCOMPtr<nsIRegistry> oldReg(do_CreateInstance(NS_REGISTRY_CONTRACTID, &rv));
    if (NS_FAILED(rv)) return rv;
    rv = oldReg->Open(registryName);
    if (NS_FAILED(rv)) return rv;

    // Enumerate 4x tree and create an array of that information.
    // Enumerate all subkeys (immediately) under the given node.
    nsCOMPtr<nsIEnumerator> enumKeys;

    rv = oldReg->EnumerateSubtrees(nsIRegistry::Users,
                                   getter_AddRefs(enumKeys));
    if (NS_FAILED(rv)) return rv;

    rv = enumKeys->First();
    if (NS_FAILED(rv)) return rv;

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

#if defined(XP_MAC)
        // 4.x profiles coming from japanese machine are already in unicode.
        // So, there is no need to decode into unicode further.
        NS_ConvertUCS2toUTF8 temp(profile);
        nsCAutoString profileName(nsUnescape( NS_CONST_CAST(char*, temp.get())));
        nsAutoString convertedProfName(NS_ConvertUTF8toUCS2(profileName).get());
#else
        nsCAutoString temp; temp.AssignWithConversion(profile);

        nsCAutoString profileName(nsUnescape( NS_CONST_CAST(char*, temp.get())));
        nsAutoString convertedProfName;
        ConvertStringToUnicode(charSet, profileName.get(), convertedProfName);
#endif

        PRBool exists = PR_FALSE;
        exists = ProfileExists(convertedProfName.get());
        if (exists)
        {		
            rv = enumKeys->Next();
            if (NS_FAILED(rv)) return rv;

            continue;
        }

        nsRegistryKey key;								
        rv = node->GetKey(&key);
        if (NS_FAILED(rv)) return rv;
		
        ProfileStruct*	profileItem  = new ProfileStruct();
        if (!profileItem)
            return NS_ERROR_OUT_OF_MEMORY;

        profileItem->updateProfileEntry    = PR_TRUE;
        profileItem->profileName  = convertedProfName;
        rv = profileItem->InternalizeLocation(oldReg, key, PR_TRUE, PR_FALSE);
        NS_ASSERTION(NS_SUCCEEDED(rv), "Could not get 4x profile location");
        profileItem->isMigrated = PR_FALSE;

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

        PRBool exists = PR_FALSE;;
        exists = ProfileExists(NS_ConvertASCIItoUCS2(unixProfileName).get());
        if (exists)
        {		
            return NS_OK;
        }

        if ( ! unixProfileName.IsEmpty() && ! unixProfileDirectory.IsEmpty() ) {
            nsCAutoString profileLocation(unixProfileDirectory);
            profileLocation += "/.netscape";
            nsCOMPtr<nsIFileSpec> users4xDotNetscapeDirectory;

            rv = NS_NewFileSpec(getter_AddRefs(users4xDotNetscapeDirectory));
            if (NS_FAILED(rv)) return rv;

            rv = users4xDotNetscapeDirectory->SetNativePath(profileLocation.get());
            if (NS_FAILED(rv)) return rv;
            rv = users4xDotNetscapeDirectory->Exists(&exists);

            if (NS_FAILED(rv)) return rv;

#ifdef DEBUG
            printf("%s exists:  %d\n",profileLocation.get(), exists);
#endif
            if (exists) {
                ProfileStruct*  profileItem     = new ProfileStruct();
                if (!profileItem)
                    return NS_ERROR_OUT_OF_MEMORY;

                profileItem->updateProfileEntry = PR_TRUE;

                profileItem->profileName = NS_ConvertASCIItoUCS2(unixProfileName).get();
                
                nsCOMPtr<nsILocalFile> localFile;
                rv = NS_NewLocalFile(profileLocation.get(), PR_TRUE, getter_AddRefs(localFile));
                if (NS_FAILED(rv)) return rv;
                profileItem->SetResolvedProfileDir(localFile);
                profileItem->isMigrated = PR_FALSE;

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

    index = FindProfileIndex(profileName);

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

ProfileStruct::ProfileStruct(const ProfileStruct& src) :
    profileName(src.profileName), isMigrated(src.isMigrated),
    NCProfileName(src.NCProfileName), NCDeniedService(src.NCDeniedService),
    NCEmailAddress(src.NCEmailAddress), NCHavePregInfo(src.NCHavePregInfo),
    updateProfileEntry(src.updateProfileEntry),
    regLocationData(src.regLocationData)
{
    if (src.resolvedLocation) {
        nsCOMPtr<nsIFile> file;
        nsresult rv = src.resolvedLocation->Clone(getter_AddRefs(file));
        if (NS_SUCCEEDED(rv))
            resolvedLocation = do_QueryInterface(file);
    }
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
    return NS_ERROR_FAILURE;
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

nsresult ProfileStruct::InternalizeLocation(nsIRegistry *aRegistry, nsRegistryKey profKey, PRBool is4x, PRBool isOld50)
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
		
#if defined(XP_MAC)
        // 4.x profiles coming from japanese machine are already in unicode.
        // So, there is no need to decode into unicode further.
        
        // Unescape profile location
        NS_ConvertUCS2toUTF8 tempLoc(profLoc);
        nsCAutoString profileLocation(nsUnescape( NS_CONST_CAST(char*, tempLoc.get())));
        nsAutoString convertedProfLoc(NS_ConvertUTF8toUCS2(profileLocation).get());
#else
		nsAutoString charSet;
		rv = GetPlatformCharset(charSet);
		if (NS_FAILED(rv)) return rv;

        // Unescape profile location and convert it to the right format
        nsCAutoString tempLoc; tempLoc.AssignWithConversion(profLoc);

        nsCAutoString profileLocation(nsUnescape( NS_CONST_CAST(char*, tempLoc.get())));
        nsAutoString convertedProfLoc;
        ConvertStringToUnicode(charSet, profileLocation.get(), convertedProfLoc);
#endif

        // Now we have a unicode path - make it into a file
        rv = NS_NewUnicodeLocalFile(convertedProfLoc.get(), PR_TRUE, getter_AddRefs(tempLocal));
    }
    else
    {
        nsXPIDLString regData;

        if (isOld50) // Some format which was used around M10-M11. Can we forget about it?
        {
            nsAutoString dirNameString;

            rv = aRegistry->GetString(profKey, 
                                      kRegistryDirectoryString.get(), 
                                      getter_Copies(regData));
            if (NS_FAILED(rv)) return rv;

            nsSimpleCharString decodedDirName;
            PRBool haveHexBytes = PR_TRUE;

            // Decode the directory name to return the ordinary string
            nsCAutoString regDataCString; regDataCString.AssignWithConversion(regData);
            nsInputStringStream stream(regDataCString.get());
            nsPersistentFileDescriptor descriptor;

            char bigBuffer[MAX_PERSISTENT_DATA_SIZE + 1];
            // The first 8 bytes of the data should be a hex version of the data size to follow.
            PRInt32 bytesRead = NUM_HEX_BYTES;
            bytesRead = stream.read(bigBuffer, bytesRead);

            if (bytesRead != NUM_HEX_BYTES)
                haveHexBytes = PR_FALSE;

            if (haveHexBytes)
            {
                bigBuffer[NUM_HEX_BYTES] = '\0';
                
                for (int i = 0; i < NUM_HEX_BYTES; i++)
                {
                    if (!(ISHEX(bigBuffer[i])))
                    {
                        haveHexBytes = PR_FALSE;
                        break;
                    }
                }
            }

            if (haveHexBytes)
            {
                PR_sscanf(bigBuffer, "%x", (PRUint32*)&bytesRead);
                if (bytesRead > MAX_PERSISTENT_DATA_SIZE)
                {
                    // Try to tolerate encoded values with no length header
                    bytesRead = NUM_HEX_BYTES + 
                                stream.read(bigBuffer + NUM_HEX_BYTES, 
                                     MAX_PERSISTENT_DATA_SIZE - NUM_HEX_BYTES);
                }
                else
                {
                    // Now we know how many bytes to read, do it.
                    bytesRead = stream.read(bigBuffer, bytesRead);
                }

                // Make sure we are null terminated
                bigBuffer[bytesRead]='\0';
                descriptor.SetData(bigBuffer, bytesRead);				
                descriptor.GetData(decodedDirName);

                dirNameString.AssignWithConversion(decodedDirName);
            }
            else
                dirNameString = regData;
                
            rv = NS_NewUnicodeLocalFile(dirNameString.get(), PR_TRUE, getter_AddRefs(tempLocal));
        }
        else
        {
            rv = aRegistry->GetString(profKey, 
                                      kRegistryDirectoryString.get(), 
                                      getter_Copies(regData));
            if (NS_FAILED(rv)) return rv;
            regLocationData = regData;

#ifdef XP_MAC
            // For a brief time, this was a unicode path
            PRInt32 firstColon = regLocationData.FindChar(PRUnichar(':'));
            if (firstColon == -1)
            {
                rv = NS_NewLocalFile(nsnull, PR_TRUE, getter_AddRefs(tempLocal));
                if (NS_SUCCEEDED(rv))
                    rv = tempLocal->SetPersistentDescriptor(NS_ConvertUCS2toUTF8(regLocationData).get());
            }
            else
#endif
            rv = NS_NewUnicodeLocalFile(regLocationData.get(), PR_TRUE, getter_AddRefs(tempLocal));
        }
    }

    if (NS_SUCCEEDED(rv) && tempLocal)
    {
        // Ensure that the parent of this dir exists - will catch
        // paths which point to unmounted drives, etc. Don't create
        // The actual directory though.
        PRBool leafCreated;
        rv = EnsureDirPathExists(tempLocal, &leafCreated);
        if (NS_SUCCEEDED(rv))
        {
            SetResolvedProfileDir(tempLocal);
            if (leafCreated)
                tempLocal->Remove(PR_FALSE);
        }
    }
    
    return NS_OK;
}

nsresult ProfileStruct::ExternalizeLocation(nsIRegistry *aRegistry, nsRegistryKey profKey)
{
    nsresult rv;
    
    if (resolvedLocation)
    {
        nsAutoString regData;

#if XP_MAC
        PRBool leafCreated;
        nsXPIDLCString descBuf;

        // It must exist before we try to use GetPersistentDescriptor
        rv = EnsureDirPathExists(resolvedLocation, &leafCreated);
        if (NS_FAILED(rv)) return rv;
        rv = resolvedLocation->GetPersistentDescriptor(getter_Copies(descBuf));
        if (NS_FAILED(rv)) return rv;
        if (leafCreated)
            resolvedLocation->Remove(PR_FALSE);
        regData = NS_ConvertUTF8toUCS2(descBuf);
#else
        nsXPIDLString ucPath;
        rv = resolvedLocation->GetUnicodePath(getter_Copies(ucPath));
        if (NS_FAILED(rv)) return rv;
        regData = ucPath;
#endif

        rv = aRegistry->SetString(profKey,
                                 kRegistryDirectoryString.get(),
                                 regData.get());

    }
    else if (regLocationData.Length() != 0)
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
        rv = aDir->Create(nsIFile::DIRECTORY_TYPE, 0775);
        *wasCreated = NS_SUCCEEDED(rv);
    }
    return rv;
}

