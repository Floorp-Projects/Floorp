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

/*
 * Constructor/Destructor
 * FillProfileInfo reads the registry and fills profileStructs
 */
nsProfileAccess::nsProfileAccess()
{
    m_registry           =  null_nsCOMPtr();
    mCount               =  0;
    mNumProfiles         =  0; 
    mNumOldProfiles      =  0;
    m4xCount             =  0;
    mFixRegEntries       =  PR_FALSE;
    mProfileDataChanged	 =  PR_FALSE;
    mForgetProfileCalled =  PR_FALSE;
    mProfiles            =  new nsVoidArray();
    m4xProfiles          =  new nsVoidArray();

    // Get the profile registry path
    NS_GetSpecialDirectory(NS_XPCOM_APPLICATION_REGISTRY_FILE, getter_AddRefs(mNewRegFile));

    PRBool regDataMoved = PR_FALSE;
    PRBool oldMozRegFileExists = PR_FALSE;

    // Get the old moz registry
    nsCOMPtr<nsIFile> mozRegFile;

#if defined(XP_PC)
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
            // Flush the registry to be on safe side.
            CloseRegistry();

            // Internal data structure now has all the data from old
            // registry. Update the Flush the new registry with this info.
            mProfileDataChanged = PR_TRUE;
            UpdateRegistry(mNewRegFile);
            CloseRegistry();

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
    nsCOMPtr <nsIPlatformCharset> platformCharset = do_GetService(NS_PLATFORMCHARSET_PROGID, &rv);
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

// Close the registry.
nsresult
nsProfileAccess::CloseRegistry()
{
    m_registry = 0;
    return NS_OK;
}

// Open the registry.
// If already opened, just use it.
nsresult
nsProfileAccess::OpenRegistry(const char* regName)
{
    nsresult rv;
    PRBool openalready = PR_FALSE;

    if (!regName)
        return NS_ERROR_FAILURE;

    if (!m_registry) {
        rv = nsComponentManager::CreateInstance(kRegistryCID,
                                                nsnull,
                                                NS_GET_IID(nsIRegistry),
                                                getter_AddRefs(m_registry));
        if (NS_FAILED(rv)) return rv;
        if (!m_registry) return NS_ERROR_FAILURE;
    }

    // Open the registry
    rv = m_registry->IsOpen( &openalready);
    if (NS_FAILED(rv)) return rv;

    if (!openalready)
        rv = m_registry->Open(regName);   

    return rv;
}

// Given the name of the profile, the structure that
// contains the relavant profile information will be filled.
// Caller must free up the profile struct.
nsresult	
nsProfileAccess::GetValue(const PRUnichar* profileName, ProfileStruct** aProfile)
{
    NS_ASSERTION(profileName, "Invalid profile name");
    NS_ASSERTION(aProfile, "Invalid profile pointer");

    PRInt32 index = 0;
    index = FindProfileIndex(profileName);

    if (index < 0) 
        return NS_ERROR_FAILURE; 

    *aProfile = new ProfileStruct();
    if (!*aProfile)
        return NS_ERROR_OUT_OF_MEMORY;

    ProfileStruct* profileItem = (ProfileStruct *) (mProfiles->ElementAt(index));

    (*aProfile)->profileName        = profileItem->profileName;
    (*aProfile)->profileLocation    = profileItem->profileLocation;
    (*aProfile)->isMigrated         = profileItem->isMigrated;

    if (!profileItem->NCProfileName.IsEmpty())
        (*aProfile)->NCProfileName	= profileItem->NCProfileName;

    if (!profileItem->NCDeniedService.IsEmpty())
        (*aProfile)->NCDeniedService	= profileItem->NCDeniedService;

    if (!profileItem->NCEmailAddress.IsEmpty())
        (*aProfile)->NCEmailAddress	= profileItem->NCEmailAddress;

    if (!profileItem->NCHavePregInfo.IsEmpty())
        (*aProfile)->NCHavePregInfo	= profileItem->NCHavePregInfo;

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

    profileItem->profileLocation = aProfile->profileLocation;

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

    if (regName)
        regName->GetPath(getter_Copies(regFile));

    rv = OpenRegistry(regFile);
    if (NS_FAILED(rv)) return rv;       

    // Enumerate all subkeys (immediately) under the given node.
    nsCOMPtr<nsIEnumerator> enumKeys;
    nsRegistryKey profilesTreeKey;

    nsAutoString registryProfileSubtreeString;
    registryProfileSubtreeString.AssignWithConversion(REGISTRY_PROFILE_SUBTREE_STRING);

    rv = m_registry->GetKey(nsIRegistry::Common, 
                            registryProfileSubtreeString.GetUnicode(), 
                            &profilesTreeKey);

    if (NS_FAILED(rv)) 
    {
        rv = m_registry->AddKey(nsIRegistry::Common, 
                                registryProfileSubtreeString.GetUnicode(), 
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
    nsAutoString registryCurrentProfileString;
    registryCurrentProfileString.AssignWithConversion(REGISTRY_CURRENT_PROFILE_STRING);
    rv = m_registry->GetString(profilesTreeKey, 
                               registryCurrentProfileString.GetUnicode(), 
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
    {
      nsAutoString registryVersionString;
      registryVersionString.AssignWithConversion(REGISTRY_VERSION_STRING);
      rv = m_registry->GetString(profilesTreeKey, 
                                 registryVersionString.GetUnicode(), 
                                 getter_Copies(tmpVersion));
    }

    if (tmpVersion == nsnull)
    {
        mFixRegEntries = PR_TRUE;
        mVersion.AssignWithConversion(REGISTRY_VERSION_1_0);

        mProfileDataChanged = PR_TRUE;
    }

    // Get the preg info
    {
      nsAutoString registryHavePRegInfoString;
      registryHavePRegInfoString.AssignWithConversion(REGISTRY_HAVE_PREG_INFO_STRING);
      rv = m_registry->GetString(profilesTreeKey, 
                                 registryHavePRegInfoString.GetUnicode(), 
                                 getter_Copies(tmpPREGInfo));
    }

    if (tmpPREGInfo == nsnull)
    {
        mHavePREGInfo.AssignWithConversion(REGISTRY_NO_STRING);

        mProfileDataChanged = PR_TRUE;
    }

    rv = m_registry->EnumerateSubtrees( profilesTreeKey, getter_AddRefs(enumKeys));
    if (NS_FAILED(rv)) return rv;

    rv = enumKeys->First();
    if (NS_FAILED(rv)) return rv;

    mCount = 0;
    mNumProfiles = 0;
    mNumOldProfiles = 0;

    while( (NS_OK != enumKeys->IsDone()) ) 
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
        nsXPIDLString directory;

        rv = node->GetName(getter_Copies(profile));
        if (NS_FAILED(rv)) return rv;

        nsRegistryKey profKey;								
        rv = node->GetKey(&profKey);
        if (NS_FAILED(rv)) return rv;

        {
          nsAutoString registryDirectoryString;
          registryDirectoryString.AssignWithConversion(REGISTRY_DIRECTORY_STRING);
          rv = m_registry->GetString(profKey, 
                               registryDirectoryString.GetUnicode(), 
                               getter_Copies(directory));
          if (NS_FAILED(rv)) return rv;
        }

        if (mFixRegEntries)
            FixRegEntry(getter_Copies(directory));

        {
          nsAutoString registryMigratedString;
          registryMigratedString.AssignWithConversion(REGISTRY_MIGRATED_STRING);
          rv = m_registry->GetString(profKey, 
                               registryMigratedString.GetUnicode(), 
                               getter_Copies(isMigrated));
          if (NS_FAILED(rv)) return rv;
        }


        // Not checking the return values of these variables as they
        // are for activation, they are optional and their values 
        // do not call for a return
        {
          nsAutoString registryNCProfileNameString;
          registryNCProfileNameString.AssignWithConversion(REGISTRY_NC_PROFILE_NAME_STRING);
          m_registry->GetString(profKey, 
                                registryNCProfileNameString.GetUnicode(), 
                                getter_Copies(NCProfileName));
        }

        {
          nsAutoString registryNCServiceDenialString;
          registryNCServiceDenialString.AssignWithConversion(REGISTRY_NC_SERVICE_DENIAL_STRING);
          m_registry->GetString(profKey, 
                                registryNCServiceDenialString.GetUnicode(), 
                                getter_Copies(NCDeniedService));
        }

        {
          nsAutoString registryNCUserEmailString;
          registryNCUserEmailString.AssignWithConversion(REGISTRY_NC_USER_EMAIL_STRING);
          m_registry->GetString(profKey, 
                                registryNCUserEmailString.GetUnicode(), 
                                getter_Copies(NCEmailAddress));
        }

        {
          nsAutoString registryNCHavePRegInfoString;
          registryNCHavePRegInfoString.AssignWithConversion(REGISTRY_NC_HAVE_PREG_INFO_STRING);
          m_registry->GetString(profKey, 
                                registryNCHavePRegInfoString.GetUnicode(), 
                                getter_Copies(NCHavePregInfo));
        }


        ProfileStruct*  profileItem	= new ProfileStruct();
        if (!profileItem)
            return NS_ERROR_OUT_OF_MEMORY;

        profileItem->updateProfileEntry     = PR_TRUE;

        profileItem->profileName      = NS_STATIC_CAST(const PRUnichar*, profile);
        profileItem->profileLocation  = NS_STATIC_CAST(const PRUnichar*, directory);
        profileItem->isMigrated       = NS_STATIC_CAST(const PRUnichar*, isMigrated);

        if (NCProfileName)
            profileItem->NCProfileName = NS_STATIC_CAST(const PRUnichar*, NCProfileName);

        if (NCDeniedService)
            profileItem->NCDeniedService = NS_STATIC_CAST(const PRUnichar*, NCDeniedService);

        if (NCEmailAddress)
            profileItem->NCEmailAddress = NS_STATIC_CAST(const PRUnichar*, NCEmailAddress);

        if (NCHavePregInfo)
            profileItem->NCHavePregInfo = NS_STATIC_CAST(const PRUnichar*, NCHavePregInfo);


        {
          nsAutoString isMigratedAutoString(isMigrated);
          if (isMigratedAutoString.EqualsWithConversion(REGISTRY_YES_STRING))
              mNumProfiles++;
          else if (isMigratedAutoString.EqualsWithConversion(REGISTRY_NO_STRING))
              mNumOldProfiles++;
        }

        if (!mProfiles) {
            mProfiles = new nsVoidArray();

            if (!mProfiles)
                return NS_ERROR_OUT_OF_MEMORY;
        }

        mProfiles->AppendElement((void*)profileItem);			

        mCount++;

        rv = enumKeys->Next();
    }

    mFixRegEntries = PR_FALSE;
    rv = CloseRegistry();
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

        if (profileItem->isMigrated.EqualsWithConversion(REGISTRY_NO_STRING))
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

        if (profileItem->isMigrated.EqualsWithConversion(REGISTRY_YES_STRING))
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

        if (profileItem->isMigrated.EqualsWithConversion(REGISTRY_NO_STRING))
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

// Fix registry incompatabilities with previous builds
void 
nsProfileAccess::FixRegEntry(PRUnichar** dirName) 
{
    NS_ASSERTION(dirName, "Invalid dirName pointer");

    nsSimpleCharString decodedDirName;
    PRBool haveHexBytes = PR_TRUE;

    // Decode the directory name to return the ordinary string
    nsInputStringStream stream(*dirName);
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
        //stream(dirName);
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

        nsAutoString dirNameString;
        dirNameString.AssignWithConversion(decodedDirName);
        *dirName = dirNameString.ToNewUnicode();
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

    rv = OpenRegistry(regFile);
    if (NS_FAILED(rv)) return rv;

    // Enumerate all subkeys (immediately) under the given node.
    nsCOMPtr<nsIEnumerator> enumKeys;
    nsRegistryKey profilesTreeKey;

    // Get the major subtree
    {
      nsAutoString registryProfileSubtreeString;
      registryProfileSubtreeString.AssignWithConversion(REGISTRY_PROFILE_SUBTREE_STRING);
      rv = m_registry->GetKey(nsIRegistry::Common, 
                              registryProfileSubtreeString.GetUnicode(), 
                              &profilesTreeKey);
      if (NS_FAILED(rv)) return rv;
    }

    // Set the current profile
    if (!mCurrentProfile.IsEmpty()) {

        nsAutoString registry_current_profile_string;
        registry_current_profile_string.AssignWithConversion(REGISTRY_CURRENT_PROFILE_STRING);
        rv = m_registry->SetString(profilesTreeKey, 
                                   registry_current_profile_string.GetUnicode(), 
                                   mCurrentProfile.GetUnicode());
        if (NS_FAILED(rv)) return rv;
    }

    // Set the registry version
    {
      nsAutoString registry_version_string;
      registry_version_string.AssignWithConversion(REGISTRY_VERSION_STRING);
      rv = m_registry->SetString(profilesTreeKey, 
                                 registry_version_string.GetUnicode(), 
                                 mVersion.GetUnicode());
      if (NS_FAILED(rv)) return rv;
    }

    // Set preg info
    {
      nsAutoString registry_have_preg_info_string;
      registry_have_preg_info_string.AssignWithConversion(REGISTRY_HAVE_PREG_INFO_STRING);
      rv = m_registry->SetString(profilesTreeKey, 
                                 registry_have_preg_info_string.GetUnicode(), 
                                 mHavePREGInfo.GetUnicode());
      if (NS_FAILED(rv)) return rv;
    }

    rv = m_registry->EnumerateSubtrees(profilesTreeKey, getter_AddRefs(enumKeys));
    if (NS_FAILED(rv)) return rv;

    rv = enumKeys->First();
    if (NS_FAILED(rv)) return rv;

    while( (NS_OK != enumKeys->IsDone()) ) 
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
            rv = m_registry->RemoveKey(profilesTreeKey, profile);
            if (NS_FAILED(rv)) return rv;
        }
        else
        {
            nsRegistryKey profKey;								

            ProfileStruct* profileItem = (ProfileStruct *) (mProfiles->ElementAt(index));

            rv = node->GetKey(&profKey);
            if (NS_FAILED(rv)) return rv;

            {
              nsAutoString registry_directory_string;
              registry_directory_string.AssignWithConversion(REGISTRY_DIRECTORY_STRING);
              rv = m_registry->SetString(profKey, 
                                  registry_directory_string.GetUnicode(), 
                                  profileItem->profileLocation.GetUnicode());
              if (NS_FAILED(rv)) return rv;
            }

            {
              nsAutoString registry_migrated_string;
              registry_migrated_string.AssignWithConversion(REGISTRY_MIGRATED_STRING);
              rv = m_registry->SetString(profKey, 
                                  registry_migrated_string.GetUnicode(), 
                                  profileItem->isMigrated.GetUnicode());
              if (NS_FAILED(rv)) return rv;
            }

            {
              nsAutoString registry_nc_profile_name_string;
              registry_nc_profile_name_string.AssignWithConversion(REGISTRY_NC_PROFILE_NAME_STRING);
              m_registry->SetString(profKey, 
                                registry_nc_profile_name_string.GetUnicode(),  
                                profileItem->NCProfileName.GetUnicode());
            }

            {
              nsAutoString registry_nc_service_denial_string;
              registry_nc_service_denial_string.AssignWithConversion(REGISTRY_NC_SERVICE_DENIAL_STRING);
              m_registry->SetString(profKey, 
                                    registry_nc_service_denial_string.GetUnicode(), 
                                    profileItem->NCDeniedService.GetUnicode());
            }

            {
              nsAutoString registry_nc_user_email_string;
              registry_nc_user_email_string.AssignWithConversion(REGISTRY_NC_USER_EMAIL_STRING);
              m_registry->SetString(profKey, 
                                    registry_nc_user_email_string.GetUnicode(), 
                                    profileItem->NCEmailAddress.GetUnicode());
            }

            {
              nsAutoString registry_nc_have_preg_info_string;
              registry_nc_have_preg_info_string.AssignWithConversion(REGISTRY_NC_HAVE_PREG_INFO_STRING);
              m_registry->SetString(profKey, 
                                    registry_nc_have_preg_info_string.GetUnicode(), 
                                    profileItem->NCHavePregInfo.GetUnicode());
            }

            profileItem->updateProfileEntry = PR_FALSE;
        }
        rv = enumKeys->Next();
    }

    // Take care of new nodes
    for (int i = 0; i < mCount; i++)
    {
        ProfileStruct* profileItem = (ProfileStruct *) (mProfiles->ElementAt(i));

        if (profileItem->updateProfileEntry)
        {
            nsRegistryKey profKey;								

            rv = m_registry->AddKey(profilesTreeKey, 
                                    profileItem->profileName.GetUnicode(), 
                                    &profKey);
            if (NS_FAILED(rv)) return rv;

            {
              nsAutoString registry_directory_string;
              registry_directory_string.AssignWithConversion(REGISTRY_DIRECTORY_STRING);
              m_registry->SetString(profKey, 
                                    registry_directory_string.GetUnicode(), 
                                    profileItem->profileLocation.GetUnicode());
              if (NS_FAILED(rv)) return rv;
            }

            {
              nsAutoString registry_migrated_string;
              registry_migrated_string.AssignWithConversion(REGISTRY_MIGRATED_STRING);
              rv = m_registry->SetString(profKey, 
                                         registry_migrated_string.GetUnicode(), 
                                         profileItem->isMigrated.GetUnicode());
              if (NS_FAILED(rv)) return rv;
            }

            {
              nsAutoString registry_nc_profile_name_string;
              registry_nc_profile_name_string.AssignWithConversion(REGISTRY_NC_PROFILE_NAME_STRING);
              m_registry->SetString(profKey, 
                                    registry_nc_profile_name_string.GetUnicode(), 
                                    profileItem->NCProfileName.GetUnicode());
            }

            {
              nsAutoString registry_nc_service_denial_string;
              registry_nc_service_denial_string.AssignWithConversion(REGISTRY_NC_SERVICE_DENIAL_STRING);
              m_registry->SetString(profKey, 
                                    registry_nc_service_denial_string.GetUnicode(), 
                                    profileItem->NCDeniedService.GetUnicode());
            }

            {
              nsAutoString registry_nc_user_email_string;
              registry_nc_user_email_string.AssignWithConversion(REGISTRY_NC_USER_EMAIL_STRING);
              m_registry->SetString(profKey, 
                                    registry_nc_user_email_string.GetUnicode(), 
                                    profileItem->NCEmailAddress.GetUnicode());
            }

            {
              nsAutoString registry_nc_have_preg_info_string;
              registry_nc_have_preg_info_string.AssignWithConversion(REGISTRY_NC_HAVE_PREG_INFO_STRING);
              m_registry->SetString(profKey, 
                                    registry_nc_have_preg_info_string.GetUnicode(), 
                                    profileItem->NCHavePregInfo.GetUnicode());
            }

            profileItem->updateProfileEntry = PR_FALSE;
        }
    }

    rv = CloseRegistry();
    mProfileDataChanged = PR_FALSE;

    return rv;
}

// Return the list of profiles, 4x and 5x.
// For 4x profiles text "- migrate" is appended
// to inform the JavaScript about the migration status.
void
nsProfileAccess::GetProfileList(PRUnichar **profileListStr)
{
    NS_ASSERTION(profileListStr, "Invalid profileListStr pointer");

    nsAutoString profileList;

    for (PRInt32 index=0; index < mCount; index++)
    {
        ProfileStruct* profileItem = (ProfileStruct *) (mProfiles->ElementAt(index));

        if (index != 0)
        {
            profileList.AppendWithConversion(",");
        }
        profileList += profileItem->profileName;

        if (profileItem->isMigrated.EqualsWithConversion(REGISTRY_NO_STRING))
            profileList.AppendWithConversion(" - migrate");
    }

    *profileListStr = profileList.ToNewUnicode();
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
    nsCOMPtr <nsIRegistry> oldReg;
    rv = nsComponentManager::CreateInstance(kRegistryCID,
                                            nsnull,
                                            NS_GET_IID(nsIRegistry),
                                            getter_AddRefs(oldReg));
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

        PRBool exists = PR_FALSE;;
        exists = ProfileExists(profile);
        if (exists)
        {		
            rv = enumKeys->Next();
            if (NS_FAILED(rv)) return rv;

            continue;
        }

        nsRegistryKey key;								
        rv = node->GetKey(&key);
        if (NS_FAILED(rv)) return rv;

        nsXPIDLString profLoc;
        nsAutoString locString;
        locString.AssignWithConversion("ProfileLocation");
        
        rv = oldReg->GetString( key, locString.GetUnicode(), getter_Copies(profLoc));
        if (NS_FAILED(rv)) return rv;
		
        ProfileStruct*	profileItem  = new ProfileStruct();
        if (!profileItem)
            return NS_ERROR_OUT_OF_MEMORY;

        profileItem->updateProfileEntry    = PR_TRUE;

        // Unescape is done on the profileName to interpret special characters like %, _ etc.
        // For example something like %20 would probably be interpreted as a space
        // There is some problem I guess in sending a space as itself

        nsCAutoString temp; temp.AssignWithConversion(profile);

        nsCAutoString profileName(nsUnescape( NS_CONST_CAST(char*, temp.GetBuffer())));
        nsAutoString convertedProfName;
        ConvertStringToUnicode(charSet, profileName.GetBuffer(), convertedProfName);

        profileItem->profileName  = convertedProfName;

        // Unescape profile location and convert it to the right format
        nsCAutoString tempLoc; tempLoc.AssignWithConversion(profLoc);

        nsCAutoString profileLocation(nsUnescape( NS_CONST_CAST(char*, tempLoc.GetBuffer())));
        nsAutoString convertedProfLoc;
        ConvertStringToUnicode(charSet, profileLocation.GetBuffer(), convertedProfLoc);

        profileItem->profileLocation = convertedProfLoc;

        profileItem->isMigrated.AssignWithConversion(REGISTRY_NO_STRING);

        if (!m4xProfiles)
            m4xProfiles = new nsVoidArray();

        m4xProfiles->AppendElement((void*)profileItem);

        mNumOldProfiles++;

        //delete profileItem;

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
                profileItem->profileLocation = NS_ConvertASCIItoUCS2(profileLocation).ToNewUnicode();
                profileItem->isMigrated = NS_ConvertASCIItoUCS2(REGISTRY_NO_STRING).ToNewUnicode();

                if (!m4xProfiles)
                    m4xProfiles = new nsVoidArray();

                m4xProfiles->AppendElement((void*)profileItem);

                mNumOldProfiles++;

				//delete profileItem;
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
        nsFileSpec profileDir(profileItem->profileLocation);

        PRBool exists;
        exists = ProfileExists(profileItem->profileName.GetUnicode());
        if (NS_FAILED(rv)) return rv;

        // That profile already exists...
        // move on.....
        if (exists) {
            continue;
        }

        nsXPIDLCString profileDirString;	
        nsCOMPtr<nsIFileSpec>spec;
        rv = NS_NewFileSpecWithSpec(profileDir, getter_AddRefs(spec));
        if (NS_FAILED(rv)) return rv;

        rv = spec->GetPersistentDescriptorString(getter_Copies(profileDirString));

        if (NS_SUCCEEDED(rv) && profileDirString)
        {
            profileItem->profileLocation.AssignWithConversion(profileDirString);
            SetValue(profileItem);
        }
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
            *info = nsCRT::strdup(REGISTRY_NO_STRING);
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
        
    nsAutoString mozRegDataMovedString;
    mozRegDataMovedString.AssignWithConversion(REGISTRY_MOZREG_DATA_MOVED_STRING);
    nsAutoString registryProfileSubtreeString;
    registryProfileSubtreeString.AssignWithConversion(REGISTRY_PROFILE_SUBTREE_STRING);

    if (mNewRegFile)
        mNewRegFile->GetPath(getter_Copies(regFile));   

    rv = OpenRegistry(regFile);
    if (NS_FAILED(rv)) return rv;

    rv = m_registry->GetKey(nsIRegistry::Common, 
                            registryProfileSubtreeString.GetUnicode(), 
                            &profilesTreeKey);

    if (NS_SUCCEEDED(rv)) 
    {
        rv = m_registry->GetString(profilesTreeKey, 
                         mozRegDataMovedString.GetUnicode(), 
                         getter_Copies(tmpRegDataMoved));
         
        nsAutoString isDataMovedString(tmpRegDataMoved);
        if (isDataMovedString.EqualsWithConversion(REGISTRY_YES_STRING))
            *isDataMoved = PR_TRUE;
    }
    else
    {
        rv = m_registry->AddKey(nsIRegistry::Common, 
                                registryProfileSubtreeString.GetUnicode(), 
                                &profilesTreeKey);
    }
    CloseRegistry();
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
    nsXPIDLString tmpRegDataMoved;
        
    nsAutoString mozRegDataMovedString;
    mozRegDataMovedString.AssignWithConversion(REGISTRY_MOZREG_DATA_MOVED_STRING);
    nsAutoString registryProfileSubtreeString;
    registryProfileSubtreeString.AssignWithConversion(REGISTRY_PROFILE_SUBTREE_STRING);
    nsAutoString regYesString;
    regYesString.AssignWithConversion(REGISTRY_YES_STRING);

    rv = OpenRegistry(regFile);
    if (NS_FAILED(rv)) return rv;

    rv = m_registry->GetKey(nsIRegistry::Common, 
                            registryProfileSubtreeString.GetUnicode(), 
                            &profilesTreeKey);

    if (NS_SUCCEEDED(rv)) 
    {
        rv = m_registry->SetString(profilesTreeKey, 
                         mozRegDataMovedString.GetUnicode(), 
                         regYesString.GetUnicode());
    }
    CloseRegistry();
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

