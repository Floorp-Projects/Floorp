/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsProfile.h"
#include "nsIPref.h"

#include "pratom.h"
#include "prmem.h"
#include "plstr.h"
#include "prenv.h"

#include "NSReg.h"
#include "nsIFactory.h"
#include "nsIComponentManager.h"
#include "nsIEnumerator.h"
#include "nsXPIDLString.h"
#include "nsIFileSpec.h"
#include "nsIFileLocator.h"
#include "nsFileLocations.h"
#include "nsEscape.h"
#include "nsIURL.h"

#include "nsIAppShellService.h"
#include "nsAppShellCIDs.h"
#include "nsIBrowserWindow.h"
#include "nsIWebShellWindow.h"
#include "prprf.h"

#include "nsIIOService.h"
#include "nsNetUtil.h"
#include "nsIPrefMigration.h"
#include "nsPrefMigrationCIDs.h"
#include "nsFileStream.h"
#include "nsSpecialSystemDirectory.h"
#include "nsIPrompt.h"
#include "nsIStreamListener.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "nsIBookmarksService.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"

#if defined (XP_UNIX)
#elif defined (XP_MAC)
#define OLD_REGISTRY_FILE_NAME "Netscape Registry"
#elif defined (XP_BEOS)
#else /* assume XP_PC */
#include <direct.h>
#define OLD_REGISTRY_FILE_NAME "nsreg.dat"
#endif /* XP_UNIX */

// PREG information
#define PREG_COOKIE		"NS_REG2_PREG"
#define PREG_USERNAME	"PREG_USER_NAME"
#define PREG_DENIAL		"PREG_USER_DENIAL"

// kill me now.
#define REGISTRY_YES_STRING		"yes"
#define REGISTRY_NO_STRING		"no"

// this used to be cpwPreg.xul, that no longer exists.  we need to fix this.
#define PROFILE_PREG_URL "chrome://profile/content/createProfileWizard.xul"

#define PROFILE_SELECTION_URL "chrome://profile/content/profileSelection.xul"
#define PROFILE_SELECTION_CMD_LINE_ARG "-SelectProfile"
#define PROFILE_MANAGER_URL "chrome://profile/content/profileManager.xul"
#define PROFILE_MANAGER_CMD_LINE_ARG "-ProfileManager"
#define PROFILE_WIZARD_URL "chrome://profile/content/createProfileWizard.xul"
#define CONFIRM_AUTOMIGRATE_URL "chrome://profile/content/confirmMigration.xul"
#define PROFILE_WIZARD_CMD_LINE_ARG "-ProfileWizard"
#define INSTALLER_CMD_LINE_ARG "-installer"
#define CREATE_PROFILE_CMD_LINE_ARG "-CreateProfile"
#define PROFILE_CMD_LINE_ARG "-P"   

#define PREF_CONFIRM_AUTOMIGRATION	"profile.confirm_automigration"

#if defined(DEBUG_sspitzer) || defined(DEBUG_seth)
#define DEBUG_profile_ 1
#endif

// ProfileAccess varaible (gProfileDataAccess) to access registry operations
// gDataAccessInstCount is used to keep track of instance count to activate
// destructor at the right time (count == 0)
static nsProfileAccess*	gProfileDataAccess = nsnull;
static PRInt32			gDataAccessInstCount = 0;

// IID and CIDs of all the services needed
static NS_DEFINE_CID(kIProfileIID, NS_IPROFILE_IID);
static NS_DEFINE_CID(kBookmarksCID, NS_BOOKMARKS_SERVICE_CID);      
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kFileLocatorCID, NS_FILELOCATOR_CID);
static NS_DEFINE_CID(kRegistryCID, NS_REGISTRY_CID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kAppShellServiceCID, NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_IID(kIFactoryIID,  NS_IFACTORY_IID);
static NS_DEFINE_IID(kIIOServiceIID, NS_IIOSERVICE_IID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_CID(kPrefMigrationCID, NS_PREFMIGRATION_CID);

static
nsresult GetStringFromSpec(nsFileSpec inSpec, char **string)
{
	nsresult rv;
	nsCOMPtr<nsIFileSpec> spec;
	rv = NS_NewFileSpecWithSpec(inSpec, getter_AddRefs(spec));
	if (NS_SUCCEEDED(rv)) {
        rv = spec->GetPersistentDescriptorString(string);
		if (NS_SUCCEEDED(rv)) {
			return NS_OK;
        }
		else {
            nsCRT::free(*string);
			return rv;
		}
    } 
	else {
		*string = nsnull;
		return rv;
	}
}

/*
 * Constructor/Destructor
 */
nsProfile::nsProfile()
{
	mAutomigrate = PR_FALSE;

	if(!gProfileDataAccess)
		gProfileDataAccess = new nsProfileAccess();

	gDataAccessInstCount++;

	NS_INIT_REFCNT();
}

nsProfile::~nsProfile() 
{
#if defined(DEBUG_profile)
	printf("~nsProfile \n");
#endif

	gProfileDataAccess->UpdateRegistry();
	
	gDataAccessInstCount--;
	if (gDataAccessInstCount == 0)
		delete gProfileDataAccess;
}

/*
 * nsISupports Implementation
 */

NS_IMPL_ADDREF(nsProfile)
NS_IMPL_RELEASE(nsProfile)
NS_IMPL_QUERY_INTERFACE(nsProfile, kIProfileIID)

/*
 * nsIProfile Implementation
 */
NS_IMETHODIMP nsProfile::Startup(const char *filename)
{
	return NS_OK;
}

NS_IMETHODIMP
nsProfile::GetAutomigrate(PRBool *aAutomigrate)
{
	if (!aAutomigrate) return NS_ERROR_NULL_POINTER;
	*aAutomigrate = mAutomigrate;
	return NS_OK;
}
NS_IMETHODIMP
nsProfile::SetAutomigrate(PRBool aAutomigrate)
{
	mAutomigrate = aAutomigrate;
	return NS_OK;
}

NS_IMETHODIMP
nsProfile::StartupWithArgs(nsICmdLineService *cmdLineArgs)
{
	nsresult rv;
	// initializations for profile manager
	PRBool profileDirSet = PR_FALSE;
	nsCString profileURLStr("");

#ifdef DEBUG_profile
	printf("Profile Manager : Profile Wizard and Manager activites : Begin\n");
#endif

	if (cmdLineArgs)
		rv = ProcessArgs(cmdLineArgs, &profileDirSet, profileURLStr);

	if (!profileDirSet) {
		rv = LoadDefaultProfileDir(profileURLStr);
		if (NS_FAILED(rv)) return rv;
	}

#ifdef DEBUG_profile
	printf("Profile Manager : Profile Wizard and Manager activites : End\n");
#endif

	return NS_OK;
}


nsresult
nsProfile::LoadDefaultProfileDir(nsCString & profileURLStr)
{
    nsresult rv;
    nsCOMPtr<nsIURI> profileURL;
    PRInt32 numProfiles=0;
  
    NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv);
    if (NS_FAILED(rv)) return rv;

    GetProfileCount(&numProfiles);
  /*
	 	 * Create the Application Shell instance...
		 */
    NS_WITH_SERVICE(nsIAppShellService, profAppShell,
                    kAppShellServiceCID, &rv);
    if (NS_FAILED(rv))
        return rv;

    char *isPregInfoSet = nsnull;
    IsPregCookieSet(&isPregInfoSet); 
		
    PRBool pregPref = PR_FALSE;

    rv = prefs->GetBoolPref(PREG_PREF, &pregPref);
    //if (NS_FAILED(rv)) return rv;

    if (profileURLStr.Length() == 0)
    {
        // This means that there was no command-line argument to force
        // profile UI to come up. But we need the UI anyway if there
        // are no profiles yet, or if there is more than one.
        if (numProfiles == 0)
        {
            if (pregPref)
                profileURLStr = PROFILE_PREG_URL;
            else
                profileURLStr = PROFILE_WIZARD_URL;
        }
        else if (numProfiles > 1)
            profileURLStr = PROFILE_SELECTION_URL;
    }


    // Provide Preg information
    if (pregPref && (PL_strcmp(isPregInfoSet, REGISTRY_YES_STRING) != 0))
        profileURLStr = PROFILE_PREG_URL;


    if (profileURLStr.Length() != 0)
    {
        rv = NS_NewURI(getter_AddRefs(profileURL), (const char *)profileURLStr);

        if (NS_FAILED(rv)) {
            return rv;
        } 

        nsCOMPtr<nsIWebShellWindow>  profWindow;
        rv = profAppShell->CreateTopLevelWindow(nsnull, profileURL,
                                                PR_TRUE, PR_TRUE, NS_CHROME_ALL_CHROME,
                                                nsnull, 
						NS_SIZETOCONTENT,           // width 
						NS_SIZETOCONTENT,           // height
                                                getter_AddRefs(profWindow));

        if (NS_FAILED(rv)) return rv;

        /*
         * Start up the main event loop...
         */	
        rv = profAppShell->Run();
    }

    PRBool confirmAutomigration = PR_FALSE;
    if (NS_SUCCEEDED(rv) && prefs) {
	rv = prefs->GetBoolPref(PREF_CONFIRM_AUTOMIGRATION, &confirmAutomigration);
	if (NS_FAILED(rv)) confirmAutomigration = PR_FALSE;
    }
	
    if (confirmAutomigration) {
	    if (profileURLStr == (const char *)(CONFIRM_AUTOMIGRATE_URL)) {
		PRBool automigrate = PR_FALSE;
		rv = GetAutomigrate(&automigrate);
		if (NS_SUCCEEDED(rv) && automigrate) {
			AutoMigrate();
		}
		else {
			// the user hit cancel.
			// so they don't want to automatically migrate
			// so call this again with the profile manager ui
			nsCString profileManagerUrl(PROFILE_MANAGER_URL);
			rv = LoadDefaultProfileDir(profileManagerUrl);
			return rv;
		}
	    }
    }

	// if we get here, and we don't have a current profile, 
	// return a failure so we will exit
	// this can happen, if the user hits Exit in the profile manager dialog
	char *currentProfileStr = nsnull;
	rv = GetCurrentProfile(&currentProfileStr);
	if (NS_FAILED(rv) || !currentProfileStr || (PL_strlen(currentProfileStr) == 0)) {
		return NS_ERROR_FAILURE;
	}
	PR_FREEIF(currentProfileStr); 

    if (pregPref && PL_strcmp(isPregInfoSet, REGISTRY_YES_STRING) != 0)
        ProcessPRegCookie();
	
	CRTFREEIF(isPregInfoSet);      

    // Now we have the right profile, read the user-specific prefs.
    rv = prefs->ReadUserPrefs();
    return rv;
}

nsresult 
nsProfile::AutoMigrate()
{
	// automatically migrate the one 4.x profile
	MigrateAllProfiles();
	gProfileDataAccess->UpdateRegistry();

	return NS_OK;
}

nsresult
nsProfile::ProcessArgs(nsICmdLineService *cmdLineArgs,
                       PRBool* profileDirSet,
                       nsCString & profileURLStr)
{
    nsresult rv;
	char* cmdResult = nsnull;
	nsFileSpec currProfileDirSpec;

#ifdef DEBUG_profile
    printf("Profile Manager : Command Line Options : Begin\n");
#endif
 
    // check for command line arguments for profile manager
    //	
    // -P command line option works this way:
    // apprunner -P profilename 
    // runs the app using the profile <profilename> 
    // remembers profile for next time
    rv = cmdLineArgs->GetCmdLineValue(PROFILE_CMD_LINE_ARG, &cmdResult);
    if (NS_SUCCEEDED(rv))
    {
        if (cmdResult) {
            char* currProfileName = cmdResult;
#ifdef DEBUG_profile
            printf("ProfileName : %s\n", cmdResult);
#endif /* DEBUG_profile */
            
            GetProfileDir(currProfileName, &currProfileDirSpec);
		
            if (NS_SUCCEEDED(rv)){
                *profileDirSet = PR_TRUE;
            }
        }
    }

	// -CreateProfile command line option works this way:
	// apprunner -CreateProfile profilename 
	// creates a new profile named <profilename> and sets the directory to your CWD 
	// runs app using that profile 
	// remembers profile for next time 
	//                         - OR -
	// apprunner -CreateProfile "profilename profiledir" 
	// creates a new profile named <profilename> and sets the directory to <profiledir> 
	// runs app using that profile 
	// remembers profile for next time

    rv = cmdLineArgs->GetCmdLineValue(CREATE_PROFILE_CMD_LINE_ARG, &cmdResult);
    if (NS_SUCCEEDED(rv))
    {
        if (cmdResult) {
            char* currProfileName = strtok(cmdResult, " ");
            char* currProfileDirString = strtok(NULL, " ");
		
            if (currProfileDirString)
                currProfileDirSpec = currProfileDirString;
            else
            {
                // No directory name provided. Get File Locator
                NS_WITH_SERVICE(nsIFileLocator, locator, kFileLocatorCID, &rv);
				if (NS_FAILED(rv) || !locator)
					return NS_ERROR_FAILURE;
		
                // Get current profile, make the new one a sibling...
                nsCOMPtr <nsIFileSpec> spec;
                rv = locator->GetFileLocation(nsSpecialFileSpec::App_DefaultUserProfileRoot50, getter_AddRefs(spec));
                if (NS_FAILED(rv) || !spec)
                    return NS_ERROR_FAILURE;
                spec->GetFileSpec(&currProfileDirSpec);

                rv = locator->ForgetProfileDir();
            }
#ifdef DEBUG_profile
            printf("profileName & profileDir are: %s\n", cmdResult);
#endif
			CreateNewProfile(currProfileName, currProfileDirSpec.GetNativePathCString());
            *profileDirSet = PR_TRUE;
        }
    }

    // Start Profile Manager
    rv = cmdLineArgs->GetCmdLineValue(PROFILE_MANAGER_CMD_LINE_ARG, &cmdResult);
    if (NS_SUCCEEDED(rv))
    {		
        if (cmdResult) {
            profileURLStr = PROFILE_MANAGER_URL;
        }
    }
    
    // Start Profile Selection
    rv = cmdLineArgs->GetCmdLineValue(PROFILE_SELECTION_CMD_LINE_ARG, &cmdResult);
    if (NS_SUCCEEDED(rv))
    {		
        if (cmdResult) {
            profileURLStr = PROFILE_SELECTION_URL;
        }
    }
    
    
    // Start Profile Wizard
    rv = cmdLineArgs->GetCmdLineValue(PROFILE_WIZARD_CMD_LINE_ARG, &cmdResult);
    if (NS_SUCCEEDED(rv))
    {		
        if (cmdResult) {
            profileURLStr = PROFILE_WIZARD_URL;
        }
    }

	// Start Migaration activity
    rv = cmdLineArgs->GetCmdLineValue(INSTALLER_CMD_LINE_ARG, &cmdResult);
    if (NS_SUCCEEDED(rv))
    {		
        if (cmdResult) {
            rv = MigrateProfileInfo();
            if (NS_FAILED(rv)) return rv;

            PRInt32 num4xProfiles = 0;
            rv = Get4xProfileCount(&num4xProfiles);
            if (NS_FAILED(rv)) return rv;
            
			PRInt32 numProfiles = 0;
			GetProfileCount(&numProfiles);
            if (num4xProfiles == 0 && numProfiles == 0) {
				// show the create profile wizard
				profileURLStr = PROFILE_WIZARD_URL;
            }
            else if (num4xProfiles == 1 && numProfiles == 0) {
		PRBool confirmAutomigration = PR_FALSE;
		NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv)
		if (NS_SUCCEEDED(rv) && prefs) {
			rv = prefs->GetBoolPref(PREF_CONFIRM_AUTOMIGRATION, &confirmAutomigration);
			if (NS_FAILED(rv)) confirmAutomigration = PR_FALSE;
		}
		if (confirmAutomigration) {
			profileURLStr = CONFIRM_AUTOMIGRATE_URL;
		}
		else {
			AutoMigrate();
		}
	    }
            else {
		// show the profile manager
                profileURLStr = PROFILE_MANAGER_URL;
            }
        }
    }
#ifdef DEBUG_profile
    printf("Profile Manager : Command Line Options : End\n");
#endif

    return NS_OK;
}


/*
 * Getters
 */

// Gets the profiles directory for a given profile
// Sets the given profile to be a current profile
NS_IMETHODIMP nsProfile::GetProfileDir(const char *profileName, nsFileSpec* profileDir)
{
    nsresult rv = NS_OK;

#if defined(DEBUG_profile)
    printf("ProfileManager : GetProfileDir\n");
#endif

	ProfileStruct	*aProfile;

	gProfileDataAccess->GetValue(profileName, &aProfile);

	if (aProfile == nsnull)
		return NS_ERROR_FAILURE;

    nsCOMPtr<nsIFileSpec>spec;
    rv = NS_NewFileSpec(getter_AddRefs(spec));
    if (NS_FAILED(rv)) return rv;
    rv = spec->SetPersistentDescriptorString(aProfile->profileLocation);
    if (NS_FAILED(rv)) return rv;
    rv = spec->GetFileSpec(profileDir);
    if (NS_FAILED(rv)) return rv;
                           
    // Set this to be a current profile only if it is a 5.0 profile
    if (PL_strcmp(aProfile->isMigrated, REGISTRY_YES_STRING) == 0)
	{
		gProfileDataAccess->SetCurrentProfile(profileName);
                                  
        nsFileSpec tmpFileSpec(*profileDir);
        if (!tmpFileSpec.Exists()) 
		{
			// Get profile defaults folder..
			NS_WITH_SERVICE(nsIFileLocator, locator, kFileLocatorCID, &rv);
                                        
            if (NS_FAILED(rv) || !locator)
				return NS_ERROR_FAILURE;
                                        
            nsCOMPtr <nsIFileSpec> profDefaultsDir;
            
			rv = locator->GetFileLocation(nsSpecialFileSpec::App_ProfileDefaultsFolder50, getter_AddRefs(profDefaultsDir));
            if (NS_FAILED(rv) || !profDefaultsDir)
				return NS_ERROR_FAILURE;
                                        
			nsFileSpec defaultsDirSpec;
            profDefaultsDir->GetFileSpec(&defaultsDirSpec);

			// Need a separate hack for Mac. For now app folder is the fall back on Mac.
			nsFilePath(tmpFileSpec.GetNativePathCString(), PR_TRUE);
                                        
			// Copy contents from defaults folder.
            if (defaultsDirSpec.Exists())
                	defaultsDirSpec.RecursiveCopy(tmpFileSpec);
		}
	}

	FreeProfileStruct(aProfile);
    return rv;
}


// Gets the number of profiles
// Location: Common/Profiles
NS_IMETHODIMP nsProfile::GetProfileCount(PRInt32 *numProfiles)
{
    *numProfiles = 0;

	gProfileDataAccess->GetNumProfiles(numProfiles);
	return NS_OK;
}


// If only a single profile exists
// return the name of the single profile.
// Otherwise it return the name of the first valid profile.
NS_IMETHODIMP nsProfile::GetFirstProfile(char **profileName)
{
	gProfileDataAccess->GetFirstProfile(profileName);
	gProfileDataAccess->SetCurrentProfile(*profileName);

    return NS_OK;
}


// Returns the name of the current profile i.e., the last used profile
NS_IMETHODIMP
nsProfile::GetCurrentProfile(char **profileName)
{
	gProfileDataAccess->GetCurrentProfile(profileName);
    return NS_OK;
}

// Returns the name of the current profile directory
NS_IMETHODIMP nsProfile::GetCurrentProfileDir(nsFileSpec* profileDir)
{
	nsresult rv = NS_OK;

    char *profileName = nsnull;

	GetCurrentProfile(&profileName);

   	rv = GetProfileDir(profileName, profileDir);
	if (NS_FAILED(rv)) return rv;

    PR_FREEIF(profileName);
    return rv;
}


/*
 * Setters
 */

// Sets the current profile directory
NS_IMETHODIMP nsProfile::SetProfileDir(const char *profileName, nsFileSpec& profileDir)
{
    nsresult rv = NS_OK;
 
	// Create a tmp Filespec and create a directory if required
	nsFileSpec tmpDir(profileDir);
                    
	if (!profileDir.Exists())
    {
		// nsPersistentFileDescriptor requires an existing
        // object. Make it first.
        tmpDir.CreateDirectory();
    }
					
	nsXPIDLCString profileDirString;
                    
	nsCOMPtr<nsIFileSpec>spec;
	rv = NS_NewFileSpecWithSpec(profileDir, getter_AddRefs(spec));
	if (NS_SUCCEEDED(rv)) {
		rv = spec->GetPersistentDescriptorString(getter_Copies(profileDirString));
	}
	if (NS_FAILED(rv)) return rv;

	ProfileStruct	*aProfile;

	aProfile = (ProfileStruct *) PR_Malloc(sizeof(ProfileStruct));
	if (!aProfile)
		return NS_ERROR_OUT_OF_MEMORY;

	aProfile->profileName		= nsCRT::strdup(profileName);
	aProfile->profileLocation	= nsCRT::strdup(profileDirString);
	aProfile->isMigrated		= nsCRT::strdup(REGISTRY_YES_STRING);
	aProfile->NCProfileName		= nsnull;
	aProfile->NCDeniedService	= nsnull;

    rv = CreateUserDirectories(tmpDir);
	if (NS_FAILED(rv)) return rv;

    gProfileDataAccess->SetValue(aProfile);
	if (NS_FAILED(rv)) return rv;
                           
    gProfileDataAccess->SetCurrentProfile(profileName);
	if (NS_FAILED(rv)) return rv;
    
    return rv;
}


// Creates a new profile
NS_IMETHODIMP nsProfile::CreateNewProfile(const char* profileName, const char* nativeProfileDir)
{
	nsresult rv = NS_OK;

#if defined(DEBUG_profile)
    printf("ProfileManager : CreateNewProfile\n");
    printf("Profile Name: %s\n", profileName);
    printf("Profile Dir: %s\n", (!nativeProfileDir || !*nativeProfileDir)?"(use default)":nativeProfileDir);
#endif

    NS_WITH_SERVICE(nsIFileLocator, locator, kFileLocatorCID, &rv);

	if (NS_FAILED(rv) || !locator)
		return NS_ERROR_FAILURE;

    if (!profileName) return NS_ERROR_FAILURE;

    nsFileSpec dirSpec;
    
	if (!nativeProfileDir || !*nativeProfileDir)
    {
		// They didn't specify a directory path...
	    nsCOMPtr <nsIFileSpec> defaultRoot;
        rv = locator->GetFileLocation(nsSpecialFileSpec::App_DefaultUserProfileRoot50, getter_AddRefs(defaultRoot));
        
		if (NS_FAILED(rv) || !defaultRoot)
			return NS_ERROR_FAILURE;

	    defaultRoot->GetFileSpec(&dirSpec);
        if (!dirSpec.Exists())
            dirSpec.CreateDirectory();

        // append profile name
        dirSpec += profileName;
		dirSpec.MakeUnique();
	}
    else {
        dirSpec = nativeProfileDir;

		// this prevents people from choosing there profile directory
	    // or another directory, and remove it when they delete the profile
		//
		// append profile name
		dirSpec += profileName;
        dirSpec.MakeUnique();    
	}

#if defined(DEBUG_profile)
	printf("before SetProfileDir\n");
#endif

	if (!dirSpec.Exists())
		dirSpec.CreateDirectory();

	// Set the directory value and add the entry to the registry tree.
	// Creates required user directories.
    rv = SetProfileDir(profileName, dirSpec);

#if defined(DEBUG_profile)
    printf("after SetProfileDir\n");
#endif

    // Get profile defaults folder..
    nsCOMPtr <nsIFileSpec> profDefaultsDir;
    rv = locator->GetFileLocation(nsSpecialFileSpec::App_ProfileDefaultsFolder50, getter_AddRefs(profDefaultsDir));
        
	if (NS_FAILED(rv) || !profDefaultsDir)
	{
		return NS_ERROR_FAILURE;
	}

	nsFileSpec defaultsDirSpec;

	profDefaultsDir->GetFileSpec(&defaultsDirSpec);

	// Copy contents from defaults folder.
	if (defaultsDirSpec.Exists())
	{
		defaultsDirSpec.RecursiveCopy(dirSpec);
	}

	gProfileDataAccess->mNumProfiles++;
	gProfileDataAccess->mProfileDataChanged = PR_TRUE;

    return NS_OK;
}

// Create required user directories like ImapMail, Mail, News, Cache etc.
nsresult nsProfile::CreateUserDirectories(const nsFileSpec& profileDir)
{
	nsresult rv = NS_OK;

#if defined(DEBUG_profile)
    printf("ProfileManager : CreateUserDirectories\n");
#endif

	nsFileSpec tmpDir;
    
	tmpDir = profileDir;
    tmpDir += "News";

    if (!tmpDir.Exists())
		tmpDir.CreateDirectory();

    tmpDir = profileDir;
    tmpDir += "ImapMail";

    if (!tmpDir.Exists())
		tmpDir.CreateDirectory();
    
	tmpDir = profileDir;
    tmpDir += "Mail";

    if (!tmpDir.Exists())
		tmpDir.CreateDirectory();
    
	tmpDir = profileDir;
    tmpDir += "Cache";

    if (!tmpDir.Exists())
		tmpDir.CreateDirectory();

    return rv;
}


// Delete all user directories associated with the a profile
// A FileSpec of the profile's directory is taken as input param
nsresult nsProfile::DeleteUserDirectories(const nsFileSpec& profileDir)
{
	nsresult rv = NS_OK;

#if defined(DEBUG_profile)
    printf("ProfileManager : DeleteUserDirectories\n");
#endif

	if (profileDir.Exists())
		profileDir.Delete(PR_TRUE);
	
	return rv;
}

// Rename a old profile to new profile.
// Copies all the keys from old profile to new profile.
// Deletes the old profile from the registry
NS_IMETHODIMP nsProfile::RenameProfile(const char* oldName, const char* newName)
{
	nsresult rv = NS_OK;

#if defined(DEBUG_profile)
    printf("ProfileManager : Renaming profile %s to %s \n", oldName, newName);
#endif

	PRBool exists;
	rv = ProfileExists(newName, &exists);
	if (NS_FAILED(rv)) return rv;
    
	// That profile already exists...
	if (exists) {
#if defined(DEBUG_profile)  
		printf("ProfileManager : Rename Operation failed : Profile exists. Provide a different new name for profile.\n");
#endif
		return NS_ERROR_FAILURE;
	}

	// Copy reg keys
	rv = CopyRegKey(oldName, newName);
	if (NS_FAILED(rv)) return rv;
	 
	rv = RenameProfileDir(newName);
	if (NS_FAILED(rv)) {
		rv = DeleteProfile(newName, PR_FALSE /* don't delete files */);
		gProfileDataAccess->mNumProfiles++; /* profile is just replaced. Keep the count same */
		NS_ASSERTION(NS_SUCCEEDED(rv), "failed to delete the aborted profile in rename");
		return NS_ERROR_FAILURE;
	}

	// Delete old profile entry
	rv = DeleteProfile(oldName, PR_FALSE /* don't delete files */);
	if (NS_FAILED(rv)) return rv;
		 
	/* profile is just replaced. But Keep up the count */
	gProfileDataAccess->mNumProfiles++;

	rv = ForgetCurrentProfile();
	if (NS_FAILED(rv)) return rv;

	gProfileDataAccess->mProfileDataChanged = PR_TRUE;

	return NS_OK;
}

// Copy old profile entries to the new profile
// In the process creates new profile subtree.
nsresult nsProfile::CopyRegKey(const char *oldProfile, const char *newProfile)
{
	nsresult rv = NS_OK;

	ProfileStruct	*aProfile;

	gProfileDataAccess->GetValue(oldProfile, &aProfile);

	if (aProfile == nsnull)
		return NS_ERROR_FAILURE;

	aProfile->profileName		= nsCRT::strdup(newProfile);
	aProfile->NCProfileName		= nsnull;
	aProfile->NCDeniedService	= nsnull;

	rv = gProfileDataAccess->SetValue(aProfile);

	FreeProfileStruct(aProfile);
	return rv;
}

NS_IMETHODIMP nsProfile::ForgetCurrentProfile()
{
    nsresult rv = NS_OK;

    // Remove the current profile subtree from the registry.
    gProfileDataAccess->SetCurrentProfile("");
    if (NS_FAILED(rv)) return rv;  

	gProfileDataAccess->mForgetProfileCalled = PR_TRUE;
    
	NS_WITH_SERVICE(nsIFileLocator, locator, kFileLocatorCID, &rv);
    if (NS_FAILED(rv)) return rv;

    if (!locator) return NS_ERROR_FAILURE;

    rv = locator->ForgetProfileDir();
    if (NS_FAILED(rv)) return rv;  

    return rv;
}

// Delete a profile from the registry
// Not deleting the directories on the harddisk yet.
// 4.x kind of confirmation need to be implemented yet
NS_IMETHODIMP nsProfile::DeleteProfile(const char* profileName, PRBool canDeleteFiles)
{
    nsresult rv = NS_OK;
 
    rv = ForgetCurrentProfile();
    if (NS_FAILED(rv)) return rv;
    
    // If user asks for it, delete profile directory
    if (canDeleteFiles) {
        nsFileSpec profileDirSpec;
        
        rv = GetProfileDir(profileName, &profileDirSpec);
        if (NS_FAILED(rv)) return rv;
        
        rv = DeleteUserDirectories(profileDirSpec);
        if (NS_FAILED(rv)) return rv;
    }

    // Remove the subtree from the registry.
	gProfileDataAccess->RemoveSubTree(profileName);
    if (NS_FAILED(rv)) return rv;

	gProfileDataAccess->mProfileDataChanged = PR_TRUE;

    return rv;
}

// Get the list of all profiles
// Populate the input param.
// This method is written to support the core service
// call to get the names all profiles.
NS_IMETHODIMP nsProfile::GetProfileList(char **profileListStr)
{
	gProfileDataAccess->GetProfileList(profileListStr);
    return NS_OK;
}


// launch the application with a profile of user's choice
// Prefs and FileLocation services are used here.
// FileLocation service to make ir forget about the global profile dir it had.
// Prefs service to kick off the startup to start the app with new profile's prefs.
NS_IMETHODIMP nsProfile::StartApprunner(const char* profileName)
{

    nsresult rv = NS_OK;

#if defined(DEBUG_profile)
    printf("ProfileManager : StartApprunner\n");
#endif

	gProfileDataAccess->SetCurrentProfile(profileName);
      
    NS_WITH_SERVICE(nsIFileLocator, locator, kFileLocatorCID, &rv);
    if (NS_FAILED(rv)) return rv;

    if (!locator) return NS_ERROR_FAILURE;
    
    rv = locator->ForgetProfileDir();
    if (NS_FAILED(rv)) {
	//temporary printf
	printf("failed to forget the profile dir\n"); 
	return rv;
    }

	/*
	 * Need to load new profile prefs.
	 */
    NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv);
    prefs->ShutDown();
    prefs->StartUp();
    prefs->ResetPrefs();
    prefs->ReadUserPrefs();

    return NS_OK;
}


// Migrate profile information from the 4x registry to 5x registry.
NS_IMETHODIMP nsProfile::MigrateProfileInfo()
{
	nsresult rv = NS_OK;

#if defined(XP_PC) || defined(XP_MAC)

#if defined(DEBUG_profile)
    printf("Entered MigrateProfileInfo.\n");
#endif

	char oldRegFile[_MAX_LENGTH] = {'\0'};

#ifdef XP_PC
    // Registry file has been traditionally stored in the windows directory (XP_PC).
    nsSpecialSystemDirectory systemDir(nsSpecialSystemDirectory::Win_WindowsDirectory);
    
    // Append the name of the old registry to the path obtained.
    PL_strcpy(oldRegFile, systemDir.GetNativePathCString());
    PL_strcat(oldRegFile, OLD_REGISTRY_FILE_NAME);
#else /* XP_MAC */
    nsSpecialSystemDirectory *regLocation = NULL;
    
    regLocation = new nsSpecialSystemDirectory(nsSpecialSystemDirectory::Mac_SystemDirectory);
    
    // Append the name of the old registry to the path obtained.
    *regLocation += "Preferences";
    *regLocation += OLD_REGISTRY_FILE_NAME;
    
    PL_strcpy(oldRegFile, regLocation->GetNativePathCString());
#endif /* XP_PC */

	rv = gProfileDataAccess->Get4xProfileInfo(oldRegFile);

#elif defined (XP_BEOS)
#else
    /* XP_UNIX */
	rv = gProfileDataAccess->Get4xProfileInfo(nsnull);

#endif /* XP_PC || XP_MAC */

	return rv;
}


// Migrate a selected profile
// Set the profile to the current profile....debatable.
// Calls PrefMigration service to do the Copy and Diverge
// of 4x Profile information
NS_IMETHODIMP nsProfile::MigrateProfile(const char* profileName, PRBool showProgressAsModalWindow)
{
	nsresult rv = NS_OK;

#if defined(DEBUG_profile)
	printf("Inside Migrate Profile routine.\n" );
#endif

	nsFileSpec oldProfDir;
	nsFileSpec newProfDir;

	rv = GetProfileDir(profileName, &oldProfDir);
	if (NS_FAILED(rv)) return rv;
	
	// Create new profile dir path
    NS_WITH_SERVICE(nsIFileLocator, locator, kFileLocatorCID, &rv);
	if (NS_FAILED(rv) || !locator) return NS_ERROR_FAILURE;
	
    // Get current profile, make the new one a sibling...
	nsCOMPtr<nsIFileSpec> newSpec;
    rv = locator->GetFileLocation(nsSpecialFileSpec::App_DefaultUserProfileRoot50, getter_AddRefs(newSpec));
	if (!newSpec)
        return NS_ERROR_FAILURE;
    newSpec->GetFileSpec(&newProfDir);
	newProfDir += profileName;


	// Call migration service to do the work.
	nsCOMPtr <nsIPrefMigration> pPrefMigrator;


	rv = nsComponentManager::CreateInstance(kPrefMigrationCID, 
                                          nsnull,
					NS_GET_IID(nsIPrefMigration),
					getter_AddRefs(pPrefMigrator));
    if (NS_FAILED(rv)) return rv;
        
    nsXPIDLCString oldProfDirStr;
    nsXPIDLCString newProfDirStr;
	
    if (!newProfDir.Exists()) {
	newProfDir.CreateDirectory();
    }
    rv = GetStringFromSpec(newProfDir, getter_Copies(newProfDirStr));
    if (NS_FAILED(rv)) return rv;
    
    if (!oldProfDir.Exists()) {
	oldProfDir.CreateDirectory();
    }
    rv = GetStringFromSpec(oldProfDir, getter_Copies(oldProfDirStr));
    if (NS_FAILED(rv)) return rv;
    rv = pPrefMigrator->AddProfilePaths(oldProfDirStr,  newProfDirStr);  // you can do this a bunch of times.
    rv = pPrefMigrator->ProcessPrefs(showProgressAsModalWindow);
	if (NS_FAILED(rv)) return rv;

    rv = SetProfileDir(profileName, newProfDir);
	if (NS_FAILED(rv)) return rv;

	gProfileDataAccess->mNumProfiles++;
	gProfileDataAccess->mNumOldProfiles--;

	gProfileDataAccess->mProfileDataChanged = PR_TRUE;

    return rv;
}


NS_IMETHODIMP nsProfile::GetCookie(char **cookie)
{
  // XXX NECKO we need to use the cookie module for this info instead of 
  // XXX the IOService
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsProfile::ProcessPRegCookie()
{

	nsresult rv = NS_OK;

	char *aCookie = nsnull;
    GetCookie(&aCookie);
	rv = ProcessPREGInfo(aCookie);
    PR_FREEIF(aCookie);
    
	return rv;
}

NS_IMETHODIMP nsProfile::ProcessPREGInfo(const char* data)
{
	nsresult rv = NS_OK;
  
	nsString aCookie(data);

	char *pregCookie = nsnull;
	char *profileName = nsnull;
	char *service_denial = nsnull;

	if ( (aCookie.ToNewCString()) != nsnull )
	{
		pregCookie = PL_strstr(aCookie.ToNewCString(), PREG_COOKIE);
		//In the original scenario you must UnEscape the string
		//PL_strstr(nsUnescape(uregCookie),PREG_USERNAME);
		if (pregCookie)
		{
			profileName		= PL_strstr(pregCookie, PREG_USERNAME);
			service_denial	= PL_strstr(pregCookie, PREG_DENIAL);
		}
	}
	else
	{
		// cookie information is not available
		return NS_ERROR_FAILURE;
	}

	nsAutoString pName;
	nsAutoString serviceState;

	if (profileName) 
		pName.SetString(profileName);
	if (service_denial) 
		serviceState.SetString(service_denial);

	PRInt32 profileNameIndex, delimIndex;
	PRInt32 serviceIndex;

	nsString userProfileName, userServiceDenial;

	if (pName.Length())
	{
		profileNameIndex = pName.Find("=", 0);
		delimIndex    = pName.Find("[-]", profileNameIndex-1);
    
		pName.Mid(userProfileName, profileNameIndex+1,delimIndex-(profileNameIndex+1));
#if defined(DEBUG_profile)  
		printf("\nProfiles : PREG Cookie user profile name = %s\n", userProfileName.ToNewCString());
#endif
	}

	if (serviceState.Length())
	{
		serviceIndex  = serviceState.Find("=", 0);
		delimIndex    = serviceState.Find("[-]", serviceIndex-1);

		serviceState.Mid(userServiceDenial, serviceIndex+1,delimIndex-(serviceIndex+1));

#if defined(DEBUG_profile)  
		printf("\nProfiles : PREG Cookie netcenter service option = %s\n", userServiceDenial.ToNewCString());
#endif
	}

	// User didn't provide any information.
	// No Netcenter info is available.
	// User will hit the Preg info screens on the next run.
	if ((userProfileName.mLength == 0) && (userServiceDenial.mLength == 0))
		return NS_ERROR_FAILURE;

	// If user denies registration, ignore the information entered.
	if (userServiceDenial.mLength > 0)
		userProfileName.SetString("");

	char *curProfile = nsnull;
	rv = GetCurrentProfile(&curProfile);
    if (NS_FAILED(rv)) return rv;

	if (userProfileName.mLength > 0)
	{
		rv = RenameProfile(curProfile, userProfileName.ToNewCString());

		ProfileStruct*	aProfile;

		gProfileDataAccess->GetValue(userProfileName.ToNewCString(), &aProfile);

		PRInt32 length = PL_strlen(userProfileName.ToNewCString());
		aProfile->NCProfileName = (char *) PR_Realloc(aProfile->NCProfileName, length+1);
		PL_strcpy(aProfile->NCProfileName, userProfileName.ToNewCString());

		gProfileDataAccess->SetValue(aProfile);
		gProfileDataAccess->SetCurrentProfile(userProfileName.ToNewCString());
		FreeProfileStruct(aProfile);
	}
	else if (userServiceDenial.mLength > 0)
	{
		ProfileStruct*	aProfile;

		gProfileDataAccess->GetValue(curProfile, &aProfile);

		PRInt32 length = PL_strlen(userServiceDenial.ToNewCString());
		aProfile->NCDeniedService = (char *) PR_Realloc(aProfile->NCDeniedService, length+1);
		PL_strcpy(aProfile->NCDeniedService, userServiceDenial.ToNewCString());

		gProfileDataAccess->SetValue(aProfile);
		FreeProfileStruct(aProfile);
	}
    
	gProfileDataAccess->SetPREGInfo(REGISTRY_YES_STRING);

	PR_FREEIF(curProfile);
	return rv;
}


NS_IMETHODIMP nsProfile::IsPregCookieSet(char **pregSet)
{
	gProfileDataAccess->GetPREGInfo(pregSet);
	return NS_OK;
}

NS_IMETHODIMP nsProfile::ProfileExists(const char *profileName, PRBool *exists)
{
	*exists = gProfileDataAccess->ProfileExists(profileName);
	return NS_OK;
}


// Gets the number of unmigrated 4x profiles
// Location: Common/Profiles
NS_IMETHODIMP nsProfile::Get4xProfileCount(PRInt32 *numProfiles)
{

    *numProfiles = 0;

	gProfileDataAccess->GetNum4xProfiles(numProfiles);
    return NS_OK;
}


// Migrates all unmigrated profiles
NS_IMETHODIMP nsProfile::MigrateAllProfiles()
{
	nsresult rv = NS_OK;
	for (PRInt32 i=0; i < gProfileDataAccess->mNumOldProfiles; i++)
	{
		rv = MigrateProfile(gProfileDataAccess->m4xProfiles[i]->profileName, PR_FALSE /* don't show progress as modal window */);
		if (NS_FAILED(rv)) return rv;
	}

	return rv;
}


nsresult nsProfile::RenameProfileDir(const char* newProfileName) 
{
	nsresult rv = NS_OK;
	nsFileSpec dirSpec;

    rv = GetProfileDir(newProfileName, &dirSpec);
	if (NS_FAILED(rv)) return rv;
	
	nsFileSpec renamedDirSpec = dirSpec;
	renamedDirSpec.SetLeafName(newProfileName);
	renamedDirSpec.MakeUnique();

	// rename the directory
	rv = dirSpec.Rename(renamedDirSpec.GetLeafName());
	if (NS_FAILED(rv)) return rv;

	// update the registry
    rv = SetProfileDir(newProfileName, dirSpec);
	if (NS_FAILED(rv)) return rv;

	return NS_OK;
}

NS_IMETHODIMP nsProfile::CloneProfile(const char* newProfile)
{
    nsresult rv = NS_OK;

#if defined(DEBUG_profile)
    printf("ProfileManager : CloneProfile\n");
#endif
	nsFileSpec currProfileDir;
	nsFileSpec newProfileDir;

	GetCurrentProfileDir(&currProfileDir);

	if (currProfileDir.Exists())
	{
		NS_WITH_SERVICE(nsIFileLocator, locator, kFileLocatorCID, &rv);
		if (NS_FAILED(rv) || !locator) return NS_ERROR_FAILURE;

	    	nsCOMPtr <nsIFileSpec> dirSpec;
        	rv = locator->GetFileLocation(nsSpecialFileSpec::App_DefaultUserProfileRoot50, getter_AddRefs(dirSpec));
        
		if (NS_FAILED(rv) || !dirSpec)
			return NS_ERROR_FAILURE;

		//Append profile name to form a directory name
	    	dirSpec->GetFileSpec(&newProfileDir);

		// TODO:
		// hash profileName  (will MakeUnique do that for us?)
		// don't allow special characters (like ..)
		// make acceptable length (will MakeUnique do that for us?)
		newProfileDir += newProfile;
		newProfileDir.MakeUnique();

		if (newProfileDir.Exists()) {
#ifdef DEBUG_profile
			printf("directory already exists\n");
#endif
			return NS_ERROR_FAILURE;
		}

		currProfileDir.RecursiveCopy(newProfileDir);
		
		rv = SetProfileDir(newProfile, newProfileDir);
	}


#if defined(DEBUG_profile)
	if (NS_SUCCEEDED(rv))
		printf("ProfileManager : Cloned CurrentProfile to new Profile ->%s<-\n", newProfile);
#endif

	gProfileDataAccess->mProfileDataChanged = PR_TRUE;
	return rv;
}

void
nsProfile::FreeProfileStruct(ProfileStruct* aProfile)
{
	if (aProfile)
	{
		CRTFREEIF(aProfile->profileName);
		CRTFREEIF(aProfile->profileLocation);
		CRTFREEIF(aProfile->isMigrated);
		CRTFREEIF(aProfile->NCProfileName);
		CRTFREEIF(aProfile->NCDeniedService);

		PR_FREEIF(aProfile);
	}
}
