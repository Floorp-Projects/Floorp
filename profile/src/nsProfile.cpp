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
#include "nsICookieService.h"
#include "nsICategoryManager.h"
#include "nsISupportsPrimitives.h"

// Interfaces Needed
#include "nsIDocShell.h"
#include "nsIWebShell.h"
#include "nsIWebBrowserChrome.h"

#if defined (XP_UNIX)
#elif defined (XP_MAC)
#define OLD_REGISTRY_FILE_NAME "Netscape Registry"
#elif defined (XP_BEOS)
#else /* assume XP_PC */
#ifndef XP_OS2
#include <direct.h>
#endif
#define OLD_REGISTRY_FILE_NAME "nsreg.dat"
#endif /* XP_UNIX */

// hack for copying panels.rdf into migrated profile dir
#define PANELS_RDF_FILE                "panels.rdf"

// A default profile name, in case automigration 4x profile fails
#define DEFAULT_PROFILE_NAME           "default"

#define PROFILE_SELECTION_URL          "chrome://communicator/content/profile/profileSelection.xul"
#define PROFILE_SELECTION_CMD_LINE_ARG "-SelectProfile"
#define PROFILE_MANAGER_URL            "chrome://communicator/content/profile/profileSelection.xul?manage=true"
#define PROFILE_MANAGER_CMD_LINE_ARG   "-ProfileManager"
#define PROFILE_WIZARD_URL             "chrome://communicator/content/profile/createProfileWizard.xul"
#define CONFIRM_AUTOMIGRATE_URL        "chrome://communicator/content/profile/confirmMigration.xul"
#define PROFILE_WIZARD_CMD_LINE_ARG    "-ProfileWizard"
#define INSTALLER_CMD_LINE_ARG         "-installer"
#define CREATE_PROFILE_CMD_LINE_ARG    "-CreateProfile"
#define PROFILE_CMD_LINE_ARG "-P"   

#define PREF_CONFIRM_AUTOMIGRATION     "profile.confirm_automigration"

#if defined (XP_MAC)
#define CHROME_STYLE nsIWebBrowserChrome::windowBordersOn | nsIWebBrowserChrome::windowCloseOn | nsIWebBrowserChrome::centerScreen
#else /* the rest */
#define CHROME_STYLE nsIWebBrowserChrome::allChrome | nsIWebBrowserChrome::centerScreen
#endif 

// we want everyone to have the debugging info to the console for now
// to help track down profile manager problems
// when we ship, we'll turn this off
#define DEBUG_profile 1

// ProfileAccess varaible (gProfileDataAccess) to access registry operations
// gDataAccessInstCount is used to keep track of instance count to activate
// destructor at the right time (count == 0)
static nsProfileAccess*    gProfileDataAccess = nsnull;
static PRInt32          gDataAccessInstCount = 0;
static PRBool           mCurrentProfileAvailable = PR_FALSE;

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
static NS_DEFINE_CID(kPrefConverterCID, NS_PREFCONVERTER_CID);
static NS_DEFINE_IID(kCookieServiceCID, NS_COOKIESERVICE_CID);


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

    gProfileDataAccess->mProfileDataChanged = PR_TRUE;
    gProfileDataAccess->UpdateRegistry();
    
    gDataAccessInstCount--;
    if (gDataAccessInstCount == 0)
        delete gProfileDataAccess;
}

/*
 * nsISupports Implementation
 */
NS_IMPL_THREADSAFE_ADDREF(nsProfile)
NS_IMPL_THREADSAFE_RELEASE(nsProfile)

NS_INTERFACE_MAP_BEGIN(nsProfile)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIProfile)
    NS_INTERFACE_MAP_ENTRY(nsIProfile)
NS_INTERFACE_MAP_END

/*
 * nsIProfile Implementation
 */
NS_IMETHODIMP nsProfile::Startup(const PRUnichar *filename)
{
    return NS_OK;
}

NS_IMETHODIMP
nsProfile::GetAutomigrate(PRBool *aAutomigrate)
{
    NS_ENSURE_ARG_POINTER(aAutomigrate);

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

    if (profileURLStr.Length() == 0)
    {
        // This means that there was no command-line argument to force
        // profile UI to come up. But we need the UI anyway if there
        // are no profiles yet, or if there is more than one.
        if (numProfiles == 0)
        {
            rv = CreateDefaultProfile();
            if (NS_FAILED(rv)) return rv;
                
            GetProfileCount(&numProfiles);
            profileURLStr = "";

            mCurrentProfileAvailable = PR_TRUE;
            // Need to load new profile prefs.
            rv = LoadNewProfilePrefs();
        }
        else if (numProfiles > 1)
            profileURLStr = PROFILE_SELECTION_URL;
    }

    if (profileURLStr.Length() != 0)
    {
        rv = NS_NewURI(getter_AddRefs(profileURL), (const char *)profileURLStr);

        if (NS_FAILED(rv)) {
            return rv;
        } 

        nsCOMPtr<nsIXULWindow> profWindow;
        rv = profAppShell->CreateTopLevelWindow(nsnull, profileURL,
                                                PR_TRUE, PR_TRUE, CHROME_STYLE,
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
        if (profileURLStr == (const char*)CONFIRM_AUTOMIGRATE_URL) {
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
    nsXPIDLString currentProfileStr;
    rv = GetCurrentProfile(getter_Copies(currentProfileStr));

    if (NS_FAILED(rv) || (*(const PRUnichar*)currentProfileStr == 0)) {
        return NS_ERROR_FAILURE;
    }
    mCurrentProfileAvailable = PR_TRUE;

    NS_WITH_SERVICE(nsICategoryManager, catman, NS_CATEGORYMANAGER_PROGID, &rv);

    if(NS_SUCCEEDED(rv) && catman) 
    {
        nsCOMPtr<nsISimpleEnumerator> enumItem;
        rv = catman->EnumerateCategory(NS_PROFILE_STARTUP_CATEGORY, getter_AddRefs(enumItem));
        if(NS_SUCCEEDED(rv) && enumItem) 
        {
           while (PR_TRUE) 
           {
               nsCOMPtr<nsISupportsString> progid;

               rv = enumItem->GetNext(getter_AddRefs(progid));
               if (NS_FAILED(rv) || !progid) break;

               nsXPIDLCString progidString;
               progid->ToString (getter_Copies(progidString));
        
               nsCOMPtr <nsIProfileStartupListener> listener = do_CreateInstance(progidString, &rv);
        
               if (listener) 
                   listener->OnProfileStartup(currentProfileStr);
	   }
	}
    }

    // Now we have the right profile, read the user-specific prefs.
    rv = prefs->ReadUserPrefs();
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr <nsIPrefConverter> pPrefConverter = do_CreateInstance(kPrefConverterCID, &rv);

    if (NS_FAILED(rv)) return rv;
    if (!pPrefConverter) return NS_ERROR_FAILURE;

    rv = pPrefConverter->ConvertPrefsToUTF8IfNecessary();
    return rv;
}

nsresult 
nsProfile::AutoMigrate()
{
    nsresult rv = NS_OK;
    // automatically migrate the one 4.x profile
    rv = MigrateAllProfiles();

    if (NS_FAILED(rv)) 
    {
#ifdef DEBUG_profile
        printf("AutoMigration failed. Let's create a default 5.0 profile.\n");
#endif
        
        rv = CreateDefaultProfile();
        if (NS_FAILED(rv)) return rv;
    }

    gProfileDataAccess->mProfileDataChanged = PR_TRUE;
    gProfileDataAccess->UpdateRegistry();

    return rv;
}

nsresult
nsProfile::ProcessArgs(nsICmdLineService *cmdLineArgs,
                       PRBool* profileDirSet,
                       nsCString & profileURLStr)
{
    NS_ASSERTION(cmdLineArgs, "Invalid cmdLineArgs");   
    NS_ASSERTION(profileDirSet, "Invalid profileDirSet");   

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
            nsAutoString currProfileName; currProfileName.AssignWithConversion(cmdResult);

#ifdef DEBUG_profile
            printf("ProfileName : %s\n", cmdResult);
#endif /* DEBUG_profile */
            PRBool exists;
            rv = ProfileExists(currProfileName.GetUnicode(), &exists);
            if (NS_FAILED(rv)) return rv;            
            
            if (!exists) {
                PRInt32 num5xProfiles = 0;
                PRInt32 num4xProfiles = 0;

                GetProfileCount(&num5xProfiles);
                Get4xProfileCount(&num4xProfiles);

                if (num5xProfiles == 0 && num4xProfiles == 0) {
                    profileURLStr = PROFILE_WIZARD_URL;
                }
                else if (num5xProfiles > 0) {
                    profileURLStr = PROFILE_SELECTION_URL;
                }
                else if (num4xProfiles > 0) {
                    profileURLStr = PROFILE_MANAGER_URL;
                }
                *profileDirSet = PR_FALSE;
            }
            else {
                rv = GetProfileDir(currProfileName.GetUnicode(), &currProfileDirSpec);
                if (NS_SUCCEEDED(rv)){
                    *profileDirSet = PR_TRUE;
                    mCurrentProfileAvailable = PR_TRUE;

                    // Need to load new profile prefs.
                    rv = LoadNewProfilePrefs();
                }
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
            nsAutoString currProfileName; currProfileName.AssignWithConversion(strtok(cmdResult, " "));
            nsAutoString currProfileDirString; currProfileDirString.AssignWithConversion(strtok(NULL, " "));
        
            if (!currProfileDirString.IsEmpty()) {
                currProfileDirSpec = currProfileDirString;
            }
            else {
                // No directory name provided. Get File Locator
                NS_WITH_SERVICE(nsIFileLocator, locator, kFileLocatorCID, &rv);
                if (NS_FAILED(rv) || !locator)
                    return NS_ERROR_FAILURE;
        
                // Get current profile, make the new one a sibling...
                nsCOMPtr <nsIFileSpec> spec;
                rv = locator->GetFileLocation(
                                 nsSpecialFileSpec::App_DefaultUserProfileRoot50, 
                                 getter_AddRefs(spec));

                if (NS_FAILED(rv) || !spec)
                    return NS_ERROR_FAILURE;
                spec->GetFileSpec(&currProfileDirSpec);

                rv = locator->ForgetProfileDir();
            }
#ifdef DEBUG_profile
            printf("profileName & profileDir are: %s\n", cmdResult);
#endif /* DEBUG_profile */

            nsAutoString currProfileDir; currProfileDir.AssignWithConversion(currProfileDirSpec.GetNativePathCString());
            rv = CreateNewProfile(currProfileName.GetUnicode(), currProfileDir.GetUnicode(), PR_TRUE);
            if (NS_SUCCEEDED(rv)) {
                *profileDirSet = PR_TRUE;
                mCurrentProfileAvailable = PR_TRUE;

                // Need to load new profile prefs.
                rv = LoadNewProfilePrefs();
                gProfileDataAccess->mProfileDataChanged = PR_TRUE;
                gProfileDataAccess->UpdateRegistry();
            }
            rv = ForgetCurrentProfile();
            if (NS_FAILED(rv)) return rv;
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
                // Let us create a default 5.0 profile
                CreateDefaultProfile();
                if (NS_FAILED(rv)) return rv;
            }
            else if (num4xProfiles == 0 && numProfiles == 1) {
                profileURLStr = "";
            }
            else if (num4xProfiles == 1 && numProfiles == 0) {
                PRBool confirmAutomigration = PR_FALSE;
                NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv)
                if (NS_SUCCEEDED(rv) && prefs) {
                    rv = prefs->GetBoolPref(PREF_CONFIRM_AUTOMIGRATION, 
                                            &confirmAutomigration);
                    if (NS_FAILED(rv)) confirmAutomigration = PR_FALSE;
                }
                if (confirmAutomigration) {
                    profileURLStr = CONFIRM_AUTOMIGRATE_URL;
                }
                else {
                    AutoMigrate();
                }
            }
            else if (numProfiles > 1)
            {
                profileURLStr = PROFILE_SELECTION_URL;
            }
            else {
                // show the profile manager
                profileURLStr = PROFILE_MANAGER_URL;
            }
        }
    }

/* hack so we don't do this for tinderbox.  otherwise the tree goes orange */
#if !defined(DEBUG_cltbld) && !defined(MOZ_BYPASS_PROFILE_AT_STARTUP)
#ifdef DEBUG
    if (profileURLStr.Length() == 0) {
        printf("DEBUG BUILDS ONLY:  we are forcing you to use the profile manager to help smoke test it.\n");
        profileURLStr = PROFILE_MANAGER_URL;
    }
#endif /* DEBUG */
#endif /* !DEBUG_cltbld && !MOZ_BYPASS_PROFILE_AT_STARTUP */

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
NS_IMETHODIMP nsProfile::GetProfileDir(const PRUnichar *profileName, nsFileSpec* profileDir)
{
    NS_ENSURE_ARG_POINTER(profileName);   
    NS_ENSURE_ARG_POINTER(profileDir);

    nsresult rv = NS_OK;

#if defined(DEBUG_profile)
    printf("ProfileManager : GetProfileDir\n");
#endif

    ProfileStruct    *aProfile;

    rv = gProfileDataAccess->GetValue(profileName, &aProfile);
    if (NS_FAILED(rv)) return rv;

	if (aProfile == nsnull)
		return NS_ERROR_FAILURE;

    nsCOMPtr<nsIFileSpec>spec;
    rv = NS_NewFileSpec(getter_AddRefs(spec));
    if (NS_FAILED(rv)) return rv;

    nsCAutoString profileLocation;
    profileLocation.AssignWithConversion(aProfile->profileLocation);
    rv = spec->SetPersistentDescriptorString(profileLocation.GetBuffer());
    if (NS_FAILED(rv)) return rv;

    rv = spec->GetFileSpec(profileDir);
    if (NS_FAILED(rv)) return rv;
                           
    // Set this to be a current profile only if it is a 5.0 profile
    if (aProfile->isMigrated.EqualsWithConversion(REGISTRY_YES_STRING))
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
            
            rv = locator->GetFileLocation(
                             nsSpecialFileSpec::App_ProfileDefaultsFolder50, 
                             getter_AddRefs(profDefaultsDir));
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

	delete aProfile;
    return rv;
}

NS_IMETHODIMP nsProfile::GetDefaultProfileParentDir(nsIFileSpec **aDefaultProfileDir)
{
    NS_ENSURE_ARG_POINTER(aDefaultProfileDir);

    nsresult rv;

    NS_WITH_SERVICE(nsIFileLocator, locator, kFileLocatorCID, &rv);
    if (NS_FAILED(rv) || !locator) return NS_ERROR_FAILURE;

    rv = locator->GetFileLocation(
                     nsSpecialFileSpec::App_DefaultUserProfileRoot50, 
                     aDefaultProfileDir);
    if (NS_FAILED(rv) || !aDefaultProfileDir || !*aDefaultProfileDir) 
        return NS_ERROR_FAILURE;

    return NS_OK;
}

// Gets the number of profiles
// Location: Common/Profiles
NS_IMETHODIMP nsProfile::GetProfileCount(PRInt32 *numProfiles)
{
    NS_ENSURE_ARG_POINTER(numProfiles);

    *numProfiles = 0;

    gProfileDataAccess->GetNumProfiles(numProfiles);
    return NS_OK;
}


// If only a single profile exists
// return the name of the single profile.
// Otherwise it return the name of the first valid profile.
NS_IMETHODIMP nsProfile::GetFirstProfile(PRUnichar **profileName)
{
    NS_ENSURE_ARG_POINTER(profileName);

    gProfileDataAccess->GetFirstProfile(profileName);
    gProfileDataAccess->SetCurrentProfile(*profileName);

    return NS_OK;
}


// Returns the name of the current profile i.e., the last used profile
NS_IMETHODIMP
nsProfile::GetCurrentProfile(PRUnichar **profileName)
{
    NS_ENSURE_ARG_POINTER(profileName);

    gProfileDataAccess->GetCurrentProfile(profileName);
    return NS_OK;
}

// Returns the name of the current profile directory
NS_IMETHODIMP nsProfile::GetCurrentProfileDir(nsFileSpec* profileDir)
{
    NS_ENSURE_ARG_POINTER(profileDir);

    nsresult rv = NS_OK;

    nsXPIDLString profileName;

    GetCurrentProfile(getter_Copies(profileName));

    rv = GetProfileDir(profileName, profileDir);
    if (NS_FAILED(rv)) return rv;

    return rv;
}


/*
 * Setters
 */

// Sets the current profile directory
NS_IMETHODIMP nsProfile::SetProfileDir(const PRUnichar *profileName, nsFileSpec& profileDir)
{
    NS_ENSURE_ARG_POINTER(profileName);   

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

    // Do I need to check for NS_ERROR_OUT_OF_MEMORY when I do a new on a class?
    ProfileStruct* aProfile = new ProfileStruct();

    nsAutoString profileLocation; profileLocation.AssignWithConversion(profileDirString);


    aProfile->profileName     = profileName;
    aProfile->profileLocation = profileLocation;
    aProfile->isMigrated.AssignWithConversion(REGISTRY_YES_STRING);


    rv = CreateUserDirectories(tmpDir);
    if (NS_FAILED(rv)) {
        delete aProfile;
        return rv;
    }

    gProfileDataAccess->SetValue(aProfile);
                           
    gProfileDataAccess->SetCurrentProfile(profileName);
    
    delete aProfile;

    return rv;
}


// Creates a new profile
NS_IMETHODIMP 
nsProfile::CreateNewProfile(const PRUnichar* profileName, 
                            const PRUnichar* nativeProfileDir, 
                            PRBool useExistingDir)
{
    NS_ENSURE_ARG_POINTER(profileName);   

    nsresult rv = NS_OK;

#if defined(DEBUG_profile)
    {
      printf("ProfileManager : CreateNewProfile\n");

      nsCAutoString temp1; temp1.AssignWithConversion(profileName);
      printf("Profile Name: %s\n", NS_STATIC_CAST(const char*, temp1));

      nsCAutoString temp2; temp2.AssignWithConversion(nativeProfileDir);
      printf("Profile Dir: %s\n", NS_STATIC_CAST(const char*, temp2));
    }
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
        rv = locator->GetFileLocation(
                         nsSpecialFileSpec::App_DefaultUserProfileRoot50, 
                         getter_AddRefs(defaultRoot));
        
        if (NS_FAILED(rv) || !defaultRoot)
            return NS_ERROR_FAILURE;

        defaultRoot->GetFileSpec(&dirSpec);
        if (!dirSpec.Exists())
            dirSpec.CreateDirectory();

        // append profile name
        dirSpec += profileName;

        // Make profile directory unique only when the user 
        // decides to not use an already existing profile directory
        if (!useExistingDir)
            dirSpec.MakeUnique();
    }
    else {
        dirSpec = nativeProfileDir;

        // this prevents people from choosing there profile directory
        // or another directory, and remove it when they delete the profile.
        // append profile name
        dirSpec += profileName;

        // Make profile directory unique only when the user 
        // decides to not use an already existing profile directory
        if (!useExistingDir)
            dirSpec.MakeUnique();
    }

#if defined(DEBUG_profile)
    printf("before SetProfileDir\n");
#endif

    if (!dirSpec.Exists())
    {
        dirSpec.CreateDirectory();
        useExistingDir = PR_FALSE;
    }

    // Set the directory value and add the entry to the registry tree.
    // Creates required user directories.
    rv = SetProfileDir(profileName, dirSpec);

#if defined(DEBUG_profile)
    printf("after SetProfileDir\n");
#endif

    // Get profile defaults folder..
    nsCOMPtr <nsIFileSpec> profDefaultsDir;
    rv = locator->GetFileLocation(
                     nsSpecialFileSpec::App_ProfileDefaultsFolder50, 
                     getter_AddRefs(profDefaultsDir));
        
    if (NS_FAILED(rv) || !profDefaultsDir)
    {
        return NS_ERROR_FAILURE;
    }

    nsFileSpec defaultsDirSpec;

    profDefaultsDir->GetFileSpec(&defaultsDirSpec);

    // Copy contents from defaults folder.
    if (defaultsDirSpec.Exists() && (!useExistingDir))
    {
        defaultsDirSpec.RecursiveCopy(dirSpec);
    }

    gProfileDataAccess->mNumProfiles++;
    gProfileDataAccess->mProfileDataChanged = PR_TRUE;
    gProfileDataAccess->UpdateRegistry();

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
NS_IMETHODIMP 
nsProfile::RenameProfile(const PRUnichar* oldName, const PRUnichar* newName)
{
    NS_ENSURE_ARG_POINTER(oldName);   
    NS_ENSURE_ARG_POINTER(newName);   

    nsresult rv = NS_OK;

#if defined(DEBUG_profile)
    {
      printf("ProfileManager : Renaming profile\n");

      nsCAutoString temp1; temp1.AssignWithConversion(oldName);
      printf("Old name:  %s\n", NS_STATIC_CAST(const char*, temp1));

      nsCAutoString temp2; temp2.AssignWithConversion(newName);
      printf("New name:  %s\n", NS_STATIC_CAST(const char*, temp2));
    }
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
     
    // Delete old profile entry
    rv = DeleteProfile(oldName, PR_FALSE /* don't delete files */);
    if (NS_FAILED(rv)) return rv;
         
    /* profile is just replaced. But Keep up the count */
    gProfileDataAccess->mNumProfiles++;

    rv = ForgetCurrentProfile();
    if (NS_FAILED(rv)) return rv;

    gProfileDataAccess->mProfileDataChanged = PR_TRUE;
    gProfileDataAccess->UpdateRegistry();

    return NS_OK;
}

// Copy old profile entries to the new profile
// In the process creates new profile subtree.
nsresult nsProfile::CopyRegKey(const PRUnichar *oldProfile, const PRUnichar *newProfile)
{
    NS_ENSURE_ARG_POINTER(oldProfile);   
    NS_ENSURE_ARG_POINTER(newProfile);   

    nsresult rv = NS_OK;

    ProfileStruct    *aProfile;

    rv = gProfileDataAccess->GetValue(oldProfile, &aProfile);
    if (NS_FAILED(rv)) return rv;

    aProfile->profileName        = newProfile;

    rv = gProfileDataAccess->SetValue(aProfile);

	delete aProfile;

    return rv;
}

NS_IMETHODIMP nsProfile::ForgetCurrentProfile()
{
    nsresult rv = NS_OK;

    // Remove the current profile subtree from the registry.
    PRUnichar tmp[] = { '\0' };

    gProfileDataAccess->SetCurrentProfile(tmp);
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
NS_IMETHODIMP nsProfile::DeleteProfile(const PRUnichar* profileName, PRBool canDeleteFiles)
{
    NS_ENSURE_ARG_POINTER(profileName);   

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
    gProfileDataAccess->UpdateRegistry();

    return rv;
}

// Get the list of all profiles
// Populate the input param.
// This method is written to support the core service
// call to get the names all profiles.
NS_IMETHODIMP nsProfile::GetProfileList(PRUnichar **profileListStr)
{
    NS_ENSURE_ARG_POINTER(profileListStr);

    gProfileDataAccess->GetProfileList(profileListStr);
    return NS_OK;
}


// launch the application with a profile of user's choice
// Prefs and FileLocation services are used here.
// FileLocation service to make ir forget about the global profile dir it had.
// Prefs service to kick off the startup to start the app with new profile's prefs.
NS_IMETHODIMP nsProfile::StartApprunner(const PRUnichar* profileName)
{
    NS_ENSURE_ARG_POINTER(profileName);   

    nsresult rv = NS_OK;

#if defined(DEBUG_profile)
    {
      printf("ProfileManager : StartApprunner\n");

      nsCAutoString temp; temp.AssignWithConversion(profileName);
      printf("profileName passed in: %s", NS_STATIC_CAST(const char*, temp));
    }
#endif

    gProfileDataAccess->SetCurrentProfile(profileName);
    mCurrentProfileAvailable = PR_TRUE;

    NS_WITH_SERVICE(nsIFileLocator, locator, kFileLocatorCID, &rv);
    if (NS_FAILED(rv)) return rv;

    if (!locator) return NS_ERROR_FAILURE;
    
    rv = locator->ForgetProfileDir();
    if (NS_FAILED(rv)) {
#ifdef DEBUG_profile
        printf("failed to forget the profile dir\n"); 
#endif /* DEBUG_profile */
        return rv;
    }

    // Need to load new profile prefs.
    rv = LoadNewProfilePrefs();
    return rv;
}

NS_IMETHODIMP nsProfile::LoadNewProfilePrefs()
{
    nsresult rv;
    NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv);
    if (NS_FAILED(rv)) return rv;

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
    nsSpecialSystemDirectory regLocation(nsSpecialSystemDirectory::Mac_SystemDirectory);
    
    // Append the name of the old registry to the path obtained.
    regLocation += "Preferences";
    regLocation += OLD_REGISTRY_FILE_NAME;
    
    PL_strcpy(oldRegFile, regLocation.GetNativePathCString());
#endif /* XP_PC */

    rv = gProfileDataAccess->Get4xProfileInfo(oldRegFile);

#elif defined (XP_BEOS)
#else
    /* XP_UNIX */
    rv = gProfileDataAccess->Get4xProfileInfo(nsnull);
#endif /* XP_PC || XP_MAC */

    gProfileDataAccess->mProfileDataChanged = PR_TRUE;
    gProfileDataAccess->UpdateRegistry();

	return rv;
}


// Migrate a selected profile
// Set the profile to the current profile....debatable.
// Calls PrefMigration service to do the Copy and Diverge
// of 4x Profile information
NS_IMETHODIMP 
nsProfile::MigrateProfile(const PRUnichar* profileName, PRBool showProgressAsModalWindow)
{
    NS_ENSURE_ARG_POINTER(profileName);   

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
    rv = locator->GetFileLocation(
                     nsSpecialFileSpec::App_DefaultUserProfileRoot50, 
                     getter_AddRefs(newSpec));
    if (!newSpec)
        return NS_ERROR_FAILURE;

    newSpec->GetFileSpec(&newProfDir);
    newProfDir += profileName;
	newProfDir.MakeUnique();
    if (newProfDir.Exists()) {
#ifdef DEBUG_profile
    	printf("directory already exists\n");
#endif
    	return NS_ERROR_FAILURE;
    }

    // Call migration service to do the work.
    nsCOMPtr <nsIPrefMigration> pPrefMigrator;


    rv = nsComponentManager::CreateInstance(kPrefMigrationCID, 
                                            nsnull,
                                            NS_GET_IID(nsIPrefMigration),
                                            getter_AddRefs(pPrefMigrator));
    if (NS_FAILED(rv)) return rv;
    if (!pPrefMigrator) return NS_ERROR_FAILURE;
        
    nsXPIDLCString oldProfDirStr;
    nsXPIDLCString newProfDirStr;
    
    if (!newProfDir.Exists()) {
        newProfDir.CreateDirectory();
    }
    rv = GetStringFromSpec(newProfDir, getter_Copies(newProfDirStr));
    if (NS_FAILED(rv)) return rv;
    
    if (!oldProfDir.Exists()) {
        return NS_ERROR_FAILURE;
    }
    rv = GetStringFromSpec(oldProfDir, getter_Copies(oldProfDirStr));
    if (NS_FAILED(rv)) return rv;

    // you can do this a bunch of times.
    rv = pPrefMigrator->AddProfilePaths(oldProfDirStr,  newProfDirStr);  

    rv = pPrefMigrator->ProcessPrefs(showProgressAsModalWindow);
    if (NS_FAILED(rv)) return rv;

    // Copy the default 5.0 profile files into the migrated profile
    // Get profile defaults folder..
    nsCOMPtr <nsIFileSpec> profDefaultsDir;
    rv = locator->GetFileLocation(
                     nsSpecialFileSpec::App_ProfileDefaultsFolder50, 
                     getter_AddRefs(profDefaultsDir));
        
    if (NS_FAILED(rv) || !profDefaultsDir)
    {
        return NS_ERROR_FAILURE;
    }

    // Copy panels.rdf file
    // This is a hack. Once the localFileSpec implementation
    // is complete, this will be removed.
    nsFileSpec defaultsDirSpecFile;

    profDefaultsDir->GetFileSpec(&defaultsDirSpecFile);

    defaultsDirSpecFile += PANELS_RDF_FILE;

    if (defaultsDirSpecFile.Exists())
    {
        defaultsDirSpecFile.CopyToDir(newProfDir);
    }
    // hack finish.


    rv = SetProfileDir(profileName, newProfDir);
    if (NS_FAILED(rv)) return rv;

    gProfileDataAccess->mNumProfiles++;
    gProfileDataAccess->mNumOldProfiles--;

    gProfileDataAccess->mProfileDataChanged = PR_TRUE;
    gProfileDataAccess->UpdateRegistry();

    return rv;
}

NS_IMETHODIMP nsProfile::ProfileExists(const PRUnichar *profileName, PRBool *exists)
{
    NS_ENSURE_ARG_POINTER(profileName); 
    NS_ENSURE_ARG_POINTER(exists);

    *exists = gProfileDataAccess->ProfileExists(profileName);
    return NS_OK;
}

NS_IMETHODIMP nsProfile::IsCurrentProfileAvailable(PRBool *available)
{
    NS_ENSURE_ARG_POINTER(available);

    *available = mCurrentProfileAvailable;
    return NS_OK;
}

// Gets the number of unmigrated 4x profiles
// Location: Common/Profiles
NS_IMETHODIMP nsProfile::Get4xProfileCount(PRInt32 *numProfiles)
{
    NS_ENSURE_ARG_POINTER(numProfiles);

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
        ProfileStruct* profileItem = (ProfileStruct *)
                                     (gProfileDataAccess->m4xProfiles->ElementAt(i));
        rv = MigrateProfile(profileItem->profileName.GetUnicode(), 
                            PR_FALSE /* don't show progress as modal window */);
        if (NS_FAILED(rv)) return rv;
    }

    return rv;
}


nsresult nsProfile::RenameProfileDir(const PRUnichar* newProfileName) 
{
    NS_ASSERTION(newProfileName, "Invalid new profile name");      

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

NS_IMETHODIMP nsProfile::CloneProfile(const PRUnichar* newProfile)
{
    NS_ENSURE_ARG_POINTER(newProfile);   

    nsresult rv = NS_OK;

#if defined(DEBUG_profile)
    printf("ProfileManager : CloneProfile\n");
#endif
    nsFileSpec currProfileDir;
    nsFileSpec newProfileDir;

    NS_WITH_SERVICE(nsIFileLocator, locator, kFileLocatorCID, &rv);
    if (NS_FAILED(rv) || !locator) return NS_ERROR_FAILURE;

    GetCurrentProfileDir(&currProfileDir);

    if (currProfileDir.Exists())
    {
        nsCOMPtr <nsIFileSpec> dirSpec;
        rv = locator->GetFileLocation(
                         nsSpecialFileSpec::App_DefaultUserProfileRoot50, 
                         getter_AddRefs(dirSpec));
        
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
    {
      if (NS_SUCCEEDED(rv))
      printf("ProfileManager : Cloned CurrentProfile\n");

      nsCAutoString temp; temp.AssignWithConversion(newProfile);
      printf("The new profile is ->%s<-\n", NS_STATIC_CAST(const char*, temp));
    }
#endif

    gProfileDataAccess->mNumProfiles++;
    gProfileDataAccess->mProfileDataChanged = PR_TRUE;

    rv = locator->ForgetProfileDir();
    if (NS_FAILED(rv)) return rv;  

    return rv;
}

nsresult
nsProfile::CreateDefaultProfile(void)
{
    nsresult rv = NS_OK;

    nsFileSpec profileDirSpec;
    
    // Get the default user profiles folder
    NS_WITH_SERVICE(nsIFileLocator, locator, kFileLocatorCID, &rv);
    if (NS_FAILED(rv) || !locator)
        return NS_ERROR_FAILURE;

    nsCOMPtr <nsIFileSpec> spec;
    rv = locator->GetFileLocation(
                     nsSpecialFileSpec::App_DefaultUserProfileRoot50, 
                     getter_AddRefs(spec));
    if (NS_FAILED(rv) || !spec)
        return NS_ERROR_FAILURE;
    spec->GetFileSpec(&profileDirSpec);

    rv = locator->ForgetProfileDir();

    nsAutoString dirSpecStr; dirSpecStr.AssignWithConversion(profileDirSpec.GetNativePathCString());

    nsAutoString defaultProfileName; defaultProfileName.AssignWithConversion(DEFAULT_PROFILE_NAME);
    rv = CreateNewProfile(defaultProfileName.GetUnicode(), dirSpecStr.GetUnicode(), PR_TRUE);

    return rv;
}

NS_IMETHODIMP 
nsProfile::UpdateRegistry(void)
{
   nsresult rv = NS_OK;

   gProfileDataAccess->mProfileDataChanged = PR_TRUE;
   rv= gProfileDataAccess->UpdateRegistry();

   return rv;
}

NS_IMETHODIMP 
nsProfile::SetRegString(const PRUnichar* profileName, const PRUnichar* regString)
{
   nsresult rv = NS_OK;

   ProfileStruct*    aProfile;

   rv = gProfileDataAccess->GetValue(profileName, &aProfile);
   if (NS_FAILED(rv)) return rv;
   
   aProfile->NCHavePregInfo = regString;

   gProfileDataAccess->SetValue(aProfile);

   delete aProfile;

   return rv;
}

NS_IMETHODIMP 
nsProfile::IsRegStringSet(const PRUnichar *profileName, char **regString)
{
    NS_ENSURE_ARG_POINTER(profileName);   
    NS_ENSURE_ARG_POINTER(regString);

    gProfileDataAccess->CheckRegString(profileName, regString);
    return NS_OK;
}

