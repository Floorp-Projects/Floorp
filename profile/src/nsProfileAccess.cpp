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
#define HOME_ENVIRONMENT_VARIABLE "HOME"
#define PROFILE_NAME_ENVIRONMENT_VARIABLE "PROFILE_NAME"
#define PROFILE_HOME_ENVIRONMENT_VARIABLE "PROFILE_HOME"
#elif defined (XP_BEOS)
#endif

#if defined(XP_PC)
#define WIN_MOZ_REG "mozregistry.dat"
#elif defined(XP_MAC)
#define MAC_MOZ_REG "Mozilla Registry"
#elif defined(XP_UNIX)
#define UNIX_MOZ_REG_FOLDER ".mozilla"
#define UNIX_MOZ_REG_FILE   "registry"
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
    mCount               =  0;
    mNumProfiles         =  0; 
    mNumOldProfiles      =  0;
    m4xCount             =  0;
    mProfileDataChanged	 =  PR_FALSE;
    mForgetProfileCalled =  PR_FALSE;
    mProfiles            =  new nsVoidArray();
    m4xProfiles          =  new nsVoidArray();

    // Get the profile registry path
    NS_GetSpecialDirectory(NS_APP_APPLICATION_REGISTRY_FILE, getter_AddRefs(mNewRegFile));

    PRBool regDataMoved = PR_FALSE;
    PRBool oldMozRegFileExists = PR_FALSE;

    // Get the old moz registry
    nsCOMPtr<nsIFile> mozRegFile;

#if defined(XP_OS2)
    NS_GetSpecialDirectory(NS_OS2_DIR, getter_AddRefs(mozRegFile));
    if (mozRegFile)
        mozRegFile->Append(WIN_MOZ_REG);
#elif defined(XP_PC)
    NS_GetSpecialDirectory(NS_WIN_WINDOWS_DIR, getter_AddRefs(mozRegFile));
    if (mozRegFile)
        mozRegFile->Append(WIN_MOZ_REG);
#elif defined(XP_MAC)
    NS_GetSpecialDirectory(NS_MAC_PREFS_DIR, getter_AddRefs(mozRegFile));
    if (mozRegFile)
        mozRegFile->Append(MAC_MOZ_REG);
#elif defined(XP_UNIX)
    NS_GetSpecialDirectory(NS_UNIX_HOME_DIR, getter_AddRefs(mozRegFile));
    if (mozRegFile)
    { 
        mozRegFile->Append(UNIX_MOZ_REG_FOLDER);
        mozRegFile->Append(UNIX_MOZ_REG_FILE);
    }
#endif

    // Check if the old profile registry exists, before we decide 
    // on transfering the data.
    if (mozRegFile)
        mozRegFile->Exists(&oldMozRegFileExists);

    if (oldMozRegFileExists) {
        // Check to see if there is a requirement to move the registry data..
        GetMozRegDataMovedFlag(&regDataMoved);

        // If we have not transfered the data from old registry,
        // do it now....
        if (!regDataMoved)
        {
            // Get the data from old moz registry
            FillProfileInfo(mozRegFile);

            // Internal data structure now has all the data from old
            // registry. Update the new registry with this info.
            mProfileDataChanged = PR_TRUE;
            UpdateRegistry(mNewRegFile);

            // Set the flag in the new registry to indicate that we have 
            // transfered the data from the old registry
            SetMozRegDataMovedFlag(mNewRegFile);

            // Time to clean the internal data structure and make it
            // ready for reading values from the new registry.
            ResetProfileMembers();
        }
    } 
    // Now the new registry is the one with all profile information
    // Read the data into internal data structure....
    FillProfileInfo(mNewRegFile);
}

// On the way out, close the registry if it is 
// still opened and free up the resources.
nsProfileAccess::~nsProfileAccess() 
{
    // Release all resources.
    mNewRegFile = nsnull;
    FreeProfileMembers(mProfiles, mCount);
    FreeProfileMembers(m4xProfiles, m4xCount);
}

// A wrapper function to call the interface to get a platform file charset.
static
nsresult 
GetPlatformCharset(nsAutoString& aCharset)
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
static
nsresult 
ConvertStringToUnicode(nsAutoString& aCharset, const char* inString, nsAutoString& outString)
{
    nsresult rv;
    // convert result to unicode
    NS_WITH_SERVICE(nsICharsetConverterManager, ccm, kCharsetConverterManagerCID, &rv);

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
nsProfileAccess::FreeProfileMembers(nsVoidArray *profiles, PRInt32 numElems)
{
    NS_ASSERTION(profiles, "Invalid profiles");

    PRInt32 index = 0;

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

    index = FindProfileIndex(aProfile->profileName.GetUnicode());

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
        mCount++;
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
                            kRegistryProfileSubtreeString, 
                            &profilesTreeKey);

    if (NS_FAILED(rv)) 
    {
        rv = registry->AddKey(nsIRegistry::Common, 
                                kRegistryProfileSubtreeString, 
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
                               kRegistryCurrentProfileString, 
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
                             kRegistryVersionString, 
                             getter_Copies(tmpVersion));

    if (tmpVersion == nsnull)
    {
        fixRegEntries = PR_TRUE;
        mProfileDataChanged = PR_TRUE;
    }

    // Get the preg info
    rv = registry->GetString(profilesTreeKey, 
                             kRegistryHavePREGInfoString, 
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

    mCount = 0;
    mNumProfiles = 0;
    mNumOldProfiles = 0;
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
                                 kRegistryMigratedString, 
                                 getter_Copies(isMigrated));
        if (NS_FAILED(rv)) return rv;
        nsLiteralString isMigratedString(isMigrated);

        // Not checking the return values of these variables as they
        // are for activation, they are optional and their values 
        // do not call for a return
        registry->GetString(profKey, 
                            kRegistryNCProfileNameString, 
                            getter_Copies(NCProfileName));

        registry->GetString(profKey, 
                            kRegistryNCServiceDenialString, 
                            getter_Copies(NCDeniedService));

        registry->GetString(profKey, 
                            kRegistryNCUserEmailString, 
                            getter_Copies(NCEmailAddress));

        registry->GetString(profKey, 
                            kRegistryNCHavePREGInfoString, 
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


        if (isMigratedString.Equals(kRegistryYesString))
            mNumProfiles++;
        else if (isMigratedString.Equals(kRegistryNoString))
            mNumOldProfiles++;

        if (!mProfiles) {
            mProfiles = new nsVoidArray();

            if (!mProfiles) {
                delete profileItem;
                return NS_ERROR_OUT_OF_MEMORY;
            }
        }

        mProfiles->AppendElement((void*)profileItem);			
        mCount++;
        
        rv = enumKeys->Next();
        if (NS_FAILED(rv)) return rv;
    }

    if (!currentProfileValid)
      mCurrentProfile.SetLength(0);
      
    return rv;
}

// Return the number of 5x profiles.
// A member variable mNumProfiles is used
// to keep track of 5x profiles. 
void
nsProfileAccess::GetNumProfiles(PRInt32 *numProfiles)
{
    NS_ASSERTION(numProfiles, "Invalid numProfiles");

    *numProfiles = mNumProfiles;
}

// Return the number of 4x (>=4.5 & < 5.0) profiles.
// A member variable mNumOldProfiles is used
// to keep track of 4x profiles. 
void
nsProfileAccess::GetNum4xProfiles(PRInt32 *numProfiles)
{
    NS_ASSERTION(numProfiles, "Invalid numProfiles");

    PRInt32 index = 0;

    *numProfiles = 0;

    for(index = 0; index < mCount; index++)
    {
        ProfileStruct* profileItem = (ProfileStruct *) (mProfiles->ElementAt(index));

        if (!profileItem->isMigrated)
        {
            (*numProfiles)++;
        }
    }

    // ******** This is a HACK -- to be changed later ********
    // When we run mozilla -installer for the second time, mNumOldProfiles is set to 0
    // This happens because MigrateProfileInfo realizes that the old profiles info 
    // already exists in mozRegistry (from the first run of -installer)
    // and does not fill m4xProfiles leaving it empty.

    // A default profile is created if there are 0 number of 4x and 5x profiles.
    // Setting mNumOldProfiles to 0 can result in this side effect if there are no
    // 5x profiles, although there are >0 number of 4x profiles.
    // This side effect would happen in nsProfile::ProcessArgs -- INSTALLER option.

    // So we query the mProfiles array for the latest numOfOldProfiles 
    // This returns the right value and we set mNumOldProfiles to this value
    // This results in the correct behaviour.

    mNumOldProfiles = *numProfiles;
}

// If the application can't find the current profile,
// the first profile will be used as the current profile.
// This routine returns the first 5x profile.
// Caller must free up the string (firstProfile).
void 
nsProfileAccess::GetFirstProfile(PRUnichar **firstProfile)
{
    NS_ASSERTION(firstProfile, "Invalid firstProfile pointer");

    PRInt32 index = 0;

    *firstProfile = nsnull;

    for(index = 0; index < mCount; index++)
    {
        ProfileStruct* profileItem = (ProfileStruct *) (mProfiles->ElementAt(index));

        if (profileItem->isMigrated)
        {
            *firstProfile = profileItem->profileName.ToNewUnicode();
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
        *profileName = mCurrentProfile.ToNewUnicode();
    }

    // If there are profiles and profileName is not
    // set yet. Get the first one and set it as Current Profile.
    if (mNumProfiles > 0 && (*profileName == nsnull))
    {
        GetFirstProfile(profileName);
        SetCurrentProfile(*profileName);
    }
}

// Delete a profile from profile structs
void
nsProfileAccess::RemoveSubTree(const PRUnichar* profileName)
{
    NS_ASSERTION(profileName, "Invalid profile name");

    // delete this entry from the mProfiles array
    // by moving the pointers with something like memmove
    // decrement mCount if it works.
    PRInt32	index = 0;
    PRBool  isOldProfile = PR_FALSE;

    index = FindProfileIndex(profileName);

    if (index >= 0)
    {
        ProfileStruct* profileItem = (ProfileStruct *) (mProfiles->ElementAt(index));

        if (!profileItem->isMigrated)
            isOldProfile = PR_TRUE;

        mProfiles->RemoveElementAt(index);

        mCount--;

        if (isOldProfile)
            mNumOldProfiles--;
        else
            mNumProfiles--;

        if (mCurrentProfile.EqualsWithConversion(profileName))
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
    PRInt32 index = 0;

    for (index=0; index < mCount; index++)
    {
        ProfileStruct* profileItem = (ProfileStruct *) (mProfiles->ElementAt(index));

        if(profileItem->profileName.EqualsWithConversion(profileName))
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
                          kRegistryProfileSubtreeString, 
                          &profilesTreeKey);
    if (NS_FAILED(rv)) return rv;

    // Set the current profile
    if (!mCurrentProfile.IsEmpty()) {

        rv = registry->SetString(profilesTreeKey, 
                                 kRegistryCurrentProfileString, 
                                 mCurrentProfile.GetUnicode());
        if (NS_FAILED(rv)) return rv;
    }

    // Set the registry version
    rv = registry->SetString(profilesTreeKey, 
                             kRegistryVersionString, 
                             kRegistryCurrentVersion);
    if (NS_FAILED(rv)) return rv;

    // Set preg info
    rv = registry->SetString(profilesTreeKey, 
                             kRegistryHavePREGInfoString, 
                             mHavePREGInfo.GetUnicode());
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
                                     kRegistryMigratedString, 
                                     profileItem->isMigrated ? kRegistryYesString : kRegistryNoString);
            if (NS_FAILED(rv)) return rv;

            registry->SetString(profKey, 
                                kRegistryNCProfileNameString,  
                                profileItem->NCProfileName.GetUnicode());

            registry->SetString(profKey, 
                                kRegistryNCServiceDenialString, 
                                profileItem->NCDeniedService.GetUnicode());

            registry->SetString(profKey, 
                                kRegistryNCUserEmailString, 
                                profileItem->NCEmailAddress.GetUnicode());

            registry->SetString(profKey, 
                                kRegistryNCHavePREGInfoString, 
                                profileItem->NCHavePregInfo.GetUnicode());

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
    for (int i = 0; i < mCount; i++)
    {
        ProfileStruct* profileItem = (ProfileStruct *) (mProfiles->ElementAt(i));

        if (profileItem->updateProfileEntry)
        {
            nsRegistryKey profKey;								

            rv = registry->AddKey(profilesTreeKey, 
                                    profileItem->profileName.GetUnicode(), 
                                    &profKey);
            if (NS_FAILED(rv)) return rv;

            rv = registry->SetString(profKey, 
                                     kRegistryMigratedString, 
                                     profileItem->isMigrated ? kRegistryYesString : kRegistryNoString);
            if (NS_FAILED(rv)) return rv;

            registry->SetString(profKey, 
                                kRegistryNCProfileNameString, 
                                profileItem->NCProfileName.GetUnicode());

            registry->SetString(profKey, 
                                kRegistryNCServiceDenialString, 
                                profileItem->NCDeniedService.GetUnicode());

            registry->SetString(profKey, 
                                kRegistryNCUserEmailString, 
                                profileItem->NCEmailAddress.GetUnicode());

            registry->SetString(profKey, 
                                kRegistryNCHavePREGInfoString, 
                                profileItem->NCHavePregInfo.GetUnicode());

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
// For 4x profiles text "- migrate" is appended
// to inform the JavaScript about the migration status.
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
    
    switch (whichKind)
    {
        case nsIProfileInternal::LIST_ONLY_NEW:
            count = mNumProfiles;
            break;
        case nsIProfileInternal::LIST_ONLY_OLD:
            GetNum4xProfiles(&count);
            break;
        case nsIProfileInternal::LIST_ALL:
            count = mCount;
            break;
        default:
            NS_ASSERTION(PR_FALSE, "Bad parameter");
            return NS_ERROR_INVALID_ARG;
    }
    
    next = outArray = (PRUnichar **)nsMemory::Alloc(count * sizeof(PRUnichar *));
    if (!outArray)
        return NS_ERROR_OUT_OF_MEMORY;
        
    for (PRInt32 index=0; index < mCount && localLength < count; index++)
    {
        ProfileStruct* profileItem = (ProfileStruct *) (mProfiles->ElementAt(index));
        
        if (whichKind == nsIProfileInternal::LIST_ONLY_OLD && profileItem->isMigrated)
            continue;
        else if (whichKind == nsIProfileInternal::LIST_ONLY_NEW && !profileItem->isMigrated)
            continue;

        *next = profileItem->profileName.ToNewUnicode();
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

    for (PRInt32 index=0; index < mCount; index++)
    {
        ProfileStruct* profileItem = (ProfileStruct *) (mProfiles->ElementAt(index));
        if (profileItem->profileName.EqualsWithConversion(profileName))
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
    NS_ASSERTION(registryName, "Invalid registryName");

    nsresult rv = NS_OK;
    mNumOldProfiles = 0;

    nsAutoString charSet;
    rv = GetPlatformCharset(charSet);
    if (NS_FAILED(rv)) return rv;

#if defined(XP_PC) || defined(XP_MAC)
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
        nsCAutoString temp; 
        temp = (const char*) NS_ConvertUCS2toUTF8(profile);
        nsCAutoString profileName(nsUnescape( NS_CONST_CAST(char*, temp.GetBuffer())));
        nsAutoString convertedProfName((const PRUnichar*) NS_ConvertUTF8toUCS2(profileName));
#else
        nsCAutoString temp; temp.AssignWithConversion(profile);

        nsCAutoString profileName(nsUnescape( NS_CONST_CAST(char*, temp.GetBuffer())));
        nsAutoString convertedProfName;
        ConvertStringToUnicode(charSet, profileName.GetBuffer(), convertedProfName);
#endif

        PRBool exists = PR_FALSE;
        exists = ProfileExists(convertedProfName.GetUnicode());
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

        if (!m4xProfiles) {
            m4xProfiles = new nsVoidArray();
            if (!m4xProfiles) {
                delete profileItem;
                return NS_ERROR_OUT_OF_MEMORY;
            }
        }

        m4xProfiles->AppendElement((void*)profileItem);

        mNumOldProfiles++;

        rv = enumKeys->Next();
        if (NS_FAILED(rv)) return rv;
    }

#elif defined (XP_BEOS)
#else
/* XP_UNIX */
        char *unixProfileName = PR_GetEnv(PROFILE_NAME_ENVIRONMENT_VARIABLE);
        char *unixProfileDirectory = PR_GetEnv(PROFILE_HOME_ENVIRONMENT_VARIABLE);

        if (!unixProfileName || 
            !unixProfileDirectory || 
            (PL_strlen(unixProfileName) == 0) || 
            (PL_strlen(unixProfileDirectory) == 0)) 
        {
            unixProfileName = PR_GetEnv(USER_ENVIRONMENT_VARIABLE);
            unixProfileDirectory = PR_GetEnv(HOME_ENVIRONMENT_VARIABLE);
        }

        PRBool exists = PR_FALSE;;
        exists = ProfileExists(NS_ConvertASCIItoUCS2(unixProfileName).GetUnicode());
        if (exists)
        {		
            return NS_OK;
        }

        if (unixProfileName && unixProfileDirectory) {
            nsCAutoString profileLocation(unixProfileDirectory);
            profileLocation += "/.netscape";
            nsCOMPtr<nsIFileSpec> users4xDotNetscapeDirectory;

            rv = NS_NewFileSpec(getter_AddRefs(users4xDotNetscapeDirectory));
            if (NS_FAILED(rv)) return rv;

            rv = users4xDotNetscapeDirectory->SetNativePath((const char *)profileLocation);
            if (NS_FAILED(rv)) return rv;
            rv = users4xDotNetscapeDirectory->Exists(&exists);

            if (NS_FAILED(rv)) return rv;

#ifdef DEBUG
            printf("%s exists:  %d\n",profileLocation.GetBuffer(), exists);
#endif
            if (exists) {
                ProfileStruct*  profileItem     = new ProfileStruct();
                if (!profileItem)
                    return NS_ERROR_OUT_OF_MEMORY;

                profileItem->updateProfileEntry = PR_TRUE;

                profileItem->profileName = NS_ConvertASCIItoUCS2(nsUnescape(unixProfileName)).ToNewUnicode();
                
                nsCOMPtr<nsILocalFile> localFile;
                rv = NS_NewLocalFile(profileLocation, PR_TRUE, getter_AddRefs(localFile));
                if (NS_FAILED(rv)) return rv;
                profileItem->SetResolvedProfileDir(localFile);
                profileItem->isMigrated = PR_FALSE;

                if (!m4xProfiles) {
                    m4xProfiles = new nsVoidArray();
                    if (!m4xProfiles) {
                        delete profileItem;
                        return NS_ERROR_OUT_OF_MEMORY;
                    }
                }

                m4xProfiles->AppendElement((void*)profileItem);

                mNumOldProfiles++;

            }
            else {
#ifdef DEBUG
                printf("no 4.x profile\n");
#endif
            }
        }
#endif /* XP_UNIX */

    m4xCount = mNumOldProfiles;

    if (m4xCount > 0) {
        UpdateProfileArray();
    }

    return rv;
}

// Update the mozregistry with the 4x profile names
// and thier locations. Entry REGISTRY_MIGRATED_STRING is set to REGISTRY_NO_STRING
// to differentiate these profiles from 5x profiles.
nsresult
nsProfileAccess::UpdateProfileArray()
{
    nsresult rv = NS_OK;

    for (PRInt32 idx = 0; idx < m4xCount; idx++)
    {
        ProfileStruct* profileItem = (ProfileStruct *) (m4xProfiles->ElementAt(idx));

        PRBool exists;
        exists = ProfileExists(profileItem->profileName.GetUnicode());

        // That profile already exists...
        // move on.....
        if (exists) {
            continue;
        }
        
        SetValue(profileItem);
    }
    mProfileDataChanged = PR_TRUE;
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
            nsCAutoString pregC;
            pregC.AssignWithConversion(profileItem->NCHavePregInfo);
            *info = nsCRT::strdup(NS_STATIC_CAST(const char*, pregC));
        }
        else
        {
            nsCAutoString noCString; noCString.AssignWithConversion(kRegistryNoString);
            *info = nsCRT::strdup(NS_STATIC_CAST(const char*, noCString));
        }
    }
}

// Get the flag that from the new reigstry which indicates that it  
// got the transfered data from old mozilla registry
nsresult
nsProfileAccess::GetMozRegDataMovedFlag(PRBool *isDataMoved)
{
    nsresult rv = NS_OK;
    nsXPIDLCString regFile;

    nsRegistryKey profilesTreeKey;
    nsXPIDLString tmpRegDataMoved;
    
    if (mNewRegFile)
        mNewRegFile->GetPath(getter_Copies(regFile));   

    nsCOMPtr<nsIRegistry> registry(do_CreateInstance(NS_REGISTRY_CONTRACTID, &rv));
    if (NS_FAILED(rv)) return rv;
    rv = registry->Open(regFile);
    if (NS_FAILED(rv)) return rv;

    rv = registry->GetKey(nsIRegistry::Common, 
                            kRegistryProfileSubtreeString, 
                            &profilesTreeKey);

    if (NS_SUCCEEDED(rv)) 
    {
        rv = registry->GetString(profilesTreeKey, 
                         kRegistryMozRegDataMovedString, 
                         getter_Copies(tmpRegDataMoved));
         
        nsAutoString isDataMovedString(tmpRegDataMoved);
        if (isDataMovedString.Equals(kRegistryYesString))
            *isDataMoved = PR_TRUE;
    }
    else
    {
        rv = registry->AddKey(nsIRegistry::Common, 
                                kRegistryProfileSubtreeString, 
                                &profilesTreeKey);
    }
    return rv;        
}

// Set the flag in the new reigstry which indicates that it  
// got the transfered data from old mozilla registry
nsresult
nsProfileAccess::SetMozRegDataMovedFlag(nsIFile* regName)
{
    nsresult rv = NS_OK;
    nsXPIDLCString regFile;

    if (regName)
        regName->GetPath(getter_Copies(regFile));   

    nsRegistryKey profilesTreeKey;
    
    nsCOMPtr<nsIRegistry> registry(do_CreateInstance(NS_REGISTRY_CONTRACTID, &rv));
    if (NS_FAILED(rv)) return rv;
    rv = registry->Open(regFile);
    if (NS_FAILED(rv)) return rv;

    rv = registry->GetKey(nsIRegistry::Common, 
                            kRegistryProfileSubtreeString, 
                            &profilesTreeKey);

    if (NS_SUCCEEDED(rv)) 
    {
        rv = registry->SetString(profilesTreeKey, 
                         kRegistryMozRegDataMovedString, 
                         kRegistryYesString);
    }

    return rv;        
}

// Clear the profile member data structure 
// We need to fill in the data from the new registry
nsresult
nsProfileAccess::ResetProfileMembers()
{
    FreeProfileMembers(mProfiles, mCount);
    mProfiles = new nsVoidArray();
    mCount = 0;
    return NS_OK;
}

nsresult
nsProfileAccess::DetermineForceMigration(PRBool *forceMigration)
{
	if (!forceMigration) return NS_ERROR_NULL_POINTER;

	*forceMigration = PR_FALSE;

	if (mNumProfiles > 0) {
		// we have some 6.0 profiles, don't force migration:
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
    }
    return NS_OK;
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
        
        rv = aRegistry->GetString( profKey, NS_LITERAL_STRING("ProfileLocation"), getter_Copies(profLoc));
        if (NS_FAILED(rv)) return rv;
        regLocationData = profLoc;
		
#if defined(XP_MAC)
        // 4.x profiles coming from japanese machine are already in unicode.
        // So, there is no need to decode into unicode further.
        
        // Unescape profile location
        nsCAutoString tempLoc; 
        tempLoc = (const char*) NS_ConvertUCS2toUTF8(profLoc);
        nsCAutoString profileLocation(nsUnescape( NS_CONST_CAST(char*, tempLoc.GetBuffer())));
        nsAutoString convertedProfLoc((const PRUnichar*) NS_ConvertUTF8toUCS2(profileLocation));
#else
		nsAutoString charSet;
		rv = GetPlatformCharset(charSet);
		if (NS_FAILED(rv)) return rv;

        // Unescape profile location and convert it to the right format
        nsCAutoString tempLoc; tempLoc.AssignWithConversion(profLoc);

        nsCAutoString profileLocation(nsUnescape( NS_CONST_CAST(char*, tempLoc.GetBuffer())));
        nsAutoString convertedProfLoc;
        ConvertStringToUnicode(charSet, profileLocation.GetBuffer(), convertedProfLoc);
#endif

        // Now we have a unicode path - make it into a file
        rv = NS_NewUnicodeLocalFile(convertedProfLoc.GetUnicode(), PR_TRUE, getter_AddRefs(tempLocal));
    }
    else
    {
        nsXPIDLString regData;

        if (isOld50) // Some format which was used around M10-M11. Can we forget about it?
        {
            nsAutoString dirNameString;

            rv = aRegistry->GetString(profKey, 
                                      kRegistryDirectoryString, 
                                      getter_Copies(regData));
            if (NS_FAILED(rv)) return rv;

            nsSimpleCharString decodedDirName;
            PRBool haveHexBytes = PR_TRUE;

            // Decode the directory name to return the ordinary string
            nsCAutoString regDataCString; regDataCString.AssignWithConversion(regData);
            nsInputStringStream stream(regDataCString);
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
                
            rv = NS_NewUnicodeLocalFile(dirNameString.GetUnicode(), PR_TRUE, getter_AddRefs(tempLocal));
        }
        else
        {
            rv = aRegistry->GetString(profKey, 
                                      kRegistryDirectoryString, 
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
                    rv = tempLocal->SetPersistentDescriptor(NS_ConvertUCS2toUTF8(regLocationData));
            }
            else
#endif
            rv = NS_NewUnicodeLocalFile(regLocationData.GetUnicode(), PR_TRUE, getter_AddRefs(tempLocal));
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
                tempLocal->Delete(PR_FALSE);
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
            resolvedLocation->Delete(PR_FALSE);
        regData = NS_ConvertUTF8toUCS2(descBuf);
#else
        nsXPIDLString ucPath;
        rv = resolvedLocation->GetUnicodePath(getter_Copies(ucPath));
        if (NS_FAILED(rv)) return rv;
        regData = ucPath;
#endif

        rv = aRegistry->SetString(profKey,
                                 kRegistryDirectoryString,
                                 regData.GetUnicode());

    }
    else if (regLocationData.Length() != 0)
    {
        // Write the original data back out - maybe it can be resolved later. 
        rv = aRegistry->SetString(profKey, 
                                 kRegistryDirectoryString, 
                                 regLocationData.GetUnicode());
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

