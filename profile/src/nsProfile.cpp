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
#include "nsIMsgAccountManager.h"
#include "nsMsgBaseCID.h"
#include "nsIMsgAccount.h"

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

// Activation cookie formats
#define NS_ACTIVATION_COOOKIE          "NS_C5A_REG"
#define NS_ACTIVATION_USERNAME         "NS_C5A_PN"
#define NS_ACTIVATION_USEREMAIL        "NS_C5A_E"
#define NS_ACTIVATION_DENIAL           "NS_C5A_DNY"

// JavaScript and Cookies prefs in the all.js
#define JAVASCRIPT_PREF                "javascript.enabled"
#define COOKIES_PREF                   "network.accept_cookies"
#define NEVER_ACCEPT_COOKIES           2

// hack for copying panels.rdf into migrated profile dir
#define PANELS_RDF_FILE                "panels.rdf"

// A default profile name, in case automigration 4x profile fails
#define DEFAULT_PROFILE_NAME           "default"

// Use PregUrlPref to extract the activation url
#define PREG_URL_PREF                  "browser.registration.url"
#define ACTIVATION_SERVER_URL          "browser.registration.domain"
#define ACTIVATION_ACCEPT_DOMAIN       "browser.registration.acceptdomain"
#define ACTIVATION_AIM_PREF            "aim.session.screenname"
#define ACTIVATION_EMAIL_SERVER_NAME   "browser.registration.mailservername"
#define ACTIVATION_EMAIL_SERVER_TYPE   "browser.registration.mailservertype"
#define ACTIVATION_COOKIE_EXPIRE_STR   "expires=31-Dec-1971 23:59:59 GMT"
#define SEMICOLON_DELIMITER            ";"
#define DOMAIN_STR                     "domain="
#define PATH_STR                       "path="
#define EXPIRES_STR                    "expires="

#define ACTIVATION_WINDOW_WIDTH        480 
#define ACTIVATION_WINDOW_HEIGHT       480 

#define ACTIVATION_FRAME_URL           "chrome://profile/content/activation.xul"
#define PROFILE_SELECTION_URL          "chrome://profile/content/profileSelection.xul"
#define PROFILE_SELECTION_CMD_LINE_ARG "-SelectProfile"
#define PROFILE_MANAGER_URL            "chrome://profile/content/profileSelection.xul?manage=true"
#define PROFILE_MANAGER_CMD_LINE_ARG   "-ProfileManager"
#define PROFILE_WIZARD_URL             "chrome://profile/content/createProfileWizard.xul"
#define CONFIRM_AUTOMIGRATE_URL        "chrome://profile/content/confirmMigration.xul"
#define PROFILE_WIZARD_CMD_LINE_ARG    "-ProfileWizard"
#define INSTALLER_CMD_LINE_ARG         "-installer"
#define CREATE_PROFILE_CMD_LINE_ARG    "-CreateProfile"
#define PROFILE_CMD_LINE_ARG "-P"   

#define PREF_CONFIRM_AUTOMIGRATION     "profile.confirm_automigration"

#if defined (XP_MAC)
#define CHROME_STYLE nsIWebBrowserChrome::windowBordersOn | nsIWebBrowserChrome::windowCloseOn
#else /* the rest */
#define CHROME_STYLE nsIWebBrowserChrome::allChrome
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

static PRBool           sHaveRedundantDirectory = PR_FALSE;
static nsFileSpec       sRedundantDirectory;

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

    CleanUp();
    gProfileDataAccess->mProfileDataChanged = PR_TRUE;
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

NS_INTERFACE_MAP_BEGIN(nsProfile)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIProfile)
    NS_INTERFACE_MAP_ENTRY(nsIProfile)
    NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
    NS_INTERFACE_MAP_ENTRY(nsIURIContentListener)
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

    PRBool pregEnabled = PR_FALSE;
    rv = prefs->GetBoolPref(PREG_PREF, &pregEnabled);

    if (profileURLStr.Length() == 0)
    {
        // This means that there was no command-line argument to force
        // profile UI to come up. But we need the UI anyway if there
        // are no profiles yet, or if there is more than one.
        if (numProfiles == 0)
        {
            if (pregEnabled)
            {
                rv = CreateDefaultProfile();
                if (NS_FAILED(rv)) return rv;
                
                GetProfileCount(&numProfiles);
                profileURLStr = "";

                mCurrentProfileAvailable = PR_TRUE;
                // Need to load new profile prefs.
                rv = LoadNewProfilePrefs();
            }
            else
                profileURLStr = PROFILE_WIZARD_URL;
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

    if (pregEnabled)
        TriggerActivation(currentProfileStr);

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
            rv = CreateNewProfile(currProfileName.GetUnicode(), currProfileDir.GetUnicode());
            if (NS_SUCCEEDED(rv)) {
                *profileDirSet = PR_TRUE;
                mCurrentProfileAvailable = PR_TRUE;

                // Need to load new profile prefs.
                rv = LoadNewProfilePrefs();
                gProfileDataAccess->mProfileDataChanged = PR_TRUE;
                gProfileDataAccess->UpdateRegistry();
            }
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

    nsCAutoString profileLocation(aProfile->profileLocation);
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
nsProfile::CreateNewProfile(const PRUnichar* profileName, const PRUnichar* nativeProfileDir)
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
        dirSpec.MakeUnique();
    }
    else {
        dirSpec = nativeProfileDir;

        // this prevents people from choosing there profile directory
        // or another directory, and remove it when they delete the profile.
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
    ("after SetProfileDir\n");
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
    if (defaultsDirSpec.Exists())
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


NS_IMETHODIMP nsProfile::GetCookie(char **cookie)
{
    NS_ENSURE_ARG_POINTER(cookie);

    nsresult rv = NS_OK;
    nsAutoString aCookie;

    NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsXPIDLCString pregURL;
    rv = prefs->CopyCharPref(ACTIVATION_SERVER_URL, 
                             getter_Copies(pregURL));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIURI> pregURI;
    rv = NS_NewURI(getter_AddRefs(pregURI), pregURL);

    NS_WITH_SERVICE(nsICookieService, service, kCookieServiceCID, &rv);
    if ((NS_OK == rv) && (nsnull != service) && (nsnull != pregURI)) {
        rv = service->GetCookieString(pregURI, aCookie);
        *cookie = nsCRT::strdup(nsCAutoString(aCookie).GetBuffer());
    }
    return rv;
}

NS_IMETHODIMP nsProfile::ProcessPRegCookie()
{
    nsresult rv = NS_OK;

    char *aCookie = nsnull;
    GetCookie(&aCookie);
    rv = ProcessPREGInfo(aCookie);

    // We processed the cookie. Remove it from cookies file.
    if (aCookie) {
        if (PL_strlen(aCookie) > 0) {
            RemoveCookie(aCookie);
        }
    }

    CRTFREEIF(aCookie);
    
    return rv;
}

NS_IMETHODIMP nsProfile::ProcessPREGInfo(const char* data)
{
    NS_ENSURE_ARG_POINTER(data);

    nsresult rv = NS_OK;
  
    char *pregCookie = nsnull;
    char *profileName = nsnull;
    char *userEmail = nsnull;
    char *service_denial = nsnull;

    pregCookie = PL_strstr(data, NS_ACTIVATION_COOOKIE);

    if (pregCookie)
    {
        profileName = PL_strstr(nsUnescape(pregCookie), 
                                NS_ACTIVATION_USERNAME);
        userEmail = PL_strstr(pregCookie, NS_ACTIVATION_USEREMAIL);
        service_denial = PL_strstr(pregCookie, NS_ACTIVATION_DENIAL);
    }
    else
    {
        // cookie information is not available
        return NS_ERROR_FAILURE;
    }

    nsAutoString pName; pName.AssignWithConversion(profileName);
    nsAutoString emailAddress; emailAddress.AssignWithConversion(userEmail);
    nsAutoString serviceState; serviceState.AssignWithConversion(service_denial);

    PRInt32 profileNameIndex, serviceIndex, delimIndex;

    nsAutoString userProfileName, userServiceDenial, userEmailAddress;

    if (pName.Length())
    {
        profileNameIndex = pName.Find("=", 0);
        delimIndex    = pName.Find("[-]", profileNameIndex-1);
    
        pName.Mid(userProfileName, profileNameIndex+1,delimIndex-(profileNameIndex+1));
#if defined(DEBUG_profile)  
        printf("\nProfiles : PREG Cookie user profile name = %s\n", nsCAutoString(userProfileName).GetBuffer());
#endif
    }

    if (emailAddress.Length())
    {
        PRInt32 emailIndex;
        emailIndex = emailAddress.Find("=", 0);
        delimIndex = emailAddress.Find("[-]", emailIndex-1);
    
        emailAddress.Mid(userEmailAddress, emailIndex+1,delimIndex-(emailIndex+1));
#if defined(DEBUG_profile)  
        printf("\nProfiles : PREG Cookie user email = %s\n", nsCAutoString(userEmailAddress).GetBuffer());
#endif
    }

    if (serviceState.Length())
    {
        serviceIndex  = serviceState.Find("=", 0);
        delimIndex    = serviceState.Find("[-]", serviceIndex-1);

        serviceState.Mid(userServiceDenial, serviceIndex+1,delimIndex-(serviceIndex+1));

#if defined(DEBUG_profile)  
        printf("\nProfiles : PREG Cookie netcenter service option = %s\n", nsCAutoString(userServiceDenial).GetBuffer());
#endif
    }

    // User didn't provide any information.
    // No Netcenter info is available.
    // User will hit the Preg info screens on the next run.
    if ((userProfileName.mLength == 0) && (userServiceDenial.mLength == 0))
        return NS_ERROR_FAILURE;

    // If user denies registration, ignore the information entered.
    if (userServiceDenial.mLength > 0)
        userProfileName.SetLength(0);

    nsXPIDLString curProfile;
    rv = GetCurrentProfile(getter_Copies(curProfile));
    if (NS_FAILED(rv)) return rv;

    nsFileSpec dirSpec;
    rv = GetProfileDir(curProfile, &dirSpec);
    if (NS_FAILED(rv)) return rv;

    if (userProfileName.mLength > 0)
    {
        // BETA1 FIX for bug 31409
        //rv = CloneProfile(userProfileName.GetUnicode());
        //if (NS_FAILED(rv)) return rv;

        // Saving netcenter profile name for AIM and Mail settings
        // This is required for the work around described below
        nsAutoString netcenterProfileName(userProfileName);

        // XXX Setting the name to curProfile. Not renaming for BETA1.
        userProfileName.Assign( NS_STATIC_CAST(const PRUnichar*, curProfile) );

        ProfileStruct*    aProfile;

        rv = gProfileDataAccess->GetValue(userProfileName.GetUnicode(), &aProfile);
        if (NS_FAILED(rv)) return rv;

        aProfile->NCProfileName = netcenterProfileName;

        NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv);
        NS_ENSURE_SUCCESS(rv, rv);
        
        prefs->SetCharPref(ACTIVATION_AIM_PREF, 
                           nsCAutoString(netcenterProfileName).GetBuffer());
        
        if ((userEmailAddress.Length()) > 0)
        {
            aProfile->NCEmailAddress = userEmailAddress;

            PRBool validDomain = PR_FALSE;
            nsAutoString domain;
            PRInt32 domainSep = userEmailAddress.FindChar('@');
            userEmailAddress.Mid(domain, domainSep+1, -1); // -1 means "the rest" 
                
            if (domain.Length())
            {
                //Check if it is a valid domain
                CheckDomain(&validDomain, nsCAutoString(domain).GetBuffer());
            }

            if (validDomain)
            {
                nsXPIDLCString serverName;
                nsXPIDLCString serverType;

                rv = prefs->CopyCharPref(ACTIVATION_EMAIL_SERVER_NAME, 
                                         getter_Copies(serverName));
                if (NS_FAILED(rv)) return rv;
                rv = prefs->CopyCharPref(ACTIVATION_EMAIL_SERVER_TYPE, 
                                         getter_Copies(serverType));
                if (NS_FAILED(rv)) return rv;

                if (!serverName || !serverType)
                    return NS_ERROR_FAILURE;
                if ( (PL_strlen(serverName) == 0) || (PL_strlen(serverType) == 0) )
                    return NS_ERROR_FAILURE;

                NS_WITH_SERVICE(nsIMsgAccountManager, accountManager, 
                                NS_MSGACCOUNTMANAGER_PROGID, &rv);
                NS_ENSURE_SUCCESS(rv, rv);

                // create a fresh identity
                nsCOMPtr<nsIMsgIdentity> identity;
                rv = accountManager->CreateIdentity(getter_AddRefs(identity));
                if (NS_FAILED(rv)) return rv;

                // Set Email address to the new identity
                rv = identity->SetEmail(nsCAutoString(aProfile->NCEmailAddress).GetBuffer());
                if (NS_FAILED(rv)) return rv;
                
                // Set the identity's valid attribute to FALSE, so that account 
                // wizard picks up the imcomplete account
                rv = identity->SetValid(PR_FALSE);
                if (NS_FAILED(rv)) return rv;

                // create the server
                nsCOMPtr<nsIMsgIncomingServer> server;
                rv = accountManager->CreateIncomingServer(
                                       nsCAutoString(aProfile->NCProfileName).GetBuffer(),
                                       serverName,
                                       serverType, 
                                       getter_AddRefs(server));
                if (NS_FAILED(rv)) return rv;

                // Set the IncomingServer's valid attribute to FALSE, so that account 
                // wizard picks up the imcomplete account
                rv = server->SetValid(PR_FALSE);
                if (NS_FAILED(rv)) return rv;

                // create the account
                nsCOMPtr<nsIMsgAccount> account;
                rv = accountManager->CreateAccount(getter_AddRefs(account));
                if (NS_FAILED(rv)) return rv;

                // Assign server and add newly created identity to the account
                rv = account->SetIncomingServer(server);
                if (NS_FAILED(rv)) return rv;

                rv = account->AddIdentity(identity);
                if (NS_FAILED(rv)) return rv;
            }
        }
        aProfile->NCHavePregInfo.AssignWithConversion(REGISTRY_YES_STRING);

        gProfileDataAccess->SetValue(aProfile);
        gProfileDataAccess->SetCurrentProfile(userProfileName.GetUnicode());

		delete aProfile;
    }
    else if (userServiceDenial.mLength > 0)
    {
        ProfileStruct*    aProfile;

        rv = gProfileDataAccess->GetValue(curProfile, &aProfile);
        if (NS_FAILED(rv)) return rv;

        aProfile->NCDeniedService = userServiceDenial;
        aProfile->NCHavePregInfo.AssignWithConversion(REGISTRY_YES_STRING);

        gProfileDataAccess->SetValue(aProfile);

        delete aProfile;
    }
    
    gProfileDataAccess->SetPREGInfo(REGISTRY_YES_STRING);

    gProfileDataAccess->mProfileDataChanged=PR_TRUE;
    gProfileDataAccess->UpdateRegistry();

    return rv;
}

NS_IMETHODIMP nsProfile::IsPregCookieSet(const PRUnichar *profileName, char **pregSet)
{
    NS_ENSURE_ARG_POINTER(profileName);   
    NS_ENSURE_ARG_POINTER(pregSet);

    gProfileDataAccess->GetPREGInfo(profileName, pregSet);
    return NS_OK;
}

NS_IMETHODIMP nsProfile::ProfileExists(const PRUnichar *profileName, PRBool *exists)
{
    NS_ENSURE_ARG_POINTER(profileName); 
    NS_ENSURE_ARG_POINTER(exists);

    *exists = gProfileDataAccess->ProfileExists(profileName);
    return NS_OK;
}

NS_IMETHODIMP nsProfile::IsCurrentProfileAvailable(PRBool *avialable)
{
    NS_ENSURE_ARG_POINTER(avialable);

    *avialable = mCurrentProfileAvailable;
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
nsProfile::TriggerActivation(const PRUnichar *profileName)
{
    NS_ASSERTION(profileName, "Invalid profileName");   

    nsresult rv = NS_OK;

    nsXPIDLCString isPregInfoSet;
    
    IsPregCookieSet(profileName, getter_Copies(isPregInfoSet)); 

    if (PL_strcmp(isPregInfoSet, REGISTRY_YES_STRING) != 0)
    {
        // fire up an instance of the cookie manager.
        // I'm doing this using the serviceManager for convenience's sake.
        // Presumably an application will init it's own cookie service a 
        // different way (this way works too though).
        nsCOMPtr<nsICookieService> cookieService 
                  = do_GetService(NS_COOKIESERVICE_PROGID, &rv);
        if (NS_FAILED(rv)) return rv;
        // quiet the compiler
        (void)cookieService;

        PRBool acceptCookies = PR_TRUE;
        cookieService->CookieEnabled(&acceptCookies);

        NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv);
        if (NS_FAILED(rv)) return rv;

        // Check if the javascript is enabled....
        PRBool javascriptEnabled = PR_TRUE;
        rv = prefs->GetBoolPref(JAVASCRIPT_PREF, &javascriptEnabled);

        // Check if cookies are accepted....
        // PRInt32 acceptCookies = 0;
        // rv = prefs->GetIntPref(COOKIES_PREF, &acceptCookies);

        // Set the boolean based on javascript and cookies prefs 
        PRBool requiredPrefsEnabled = PR_TRUE;

        if ((!(javascriptEnabled)) || (!acceptCookies))
            requiredPrefsEnabled = PR_FALSE;
    
        if (requiredPrefsEnabled)
        {
            /*
             * Create the Application Shell instance...
             */
            NS_WITH_SERVICE(nsIAppShellService, pregAppShell, kAppShellServiceCID, &rv);
            if (NS_FAILED(rv)) return rv;
			
            nsCOMPtr<nsIURI> registrationURL;
            rv = NS_NewURI(getter_AddRefs(registrationURL), ACTIVATION_FRAME_URL);

            if (NS_FAILED(rv)) return rv;

            rv = pregAppShell->CreateTopLevelWindow(nsnull, registrationURL, 
                                                    PR_TRUE, PR_TRUE, 
                                                    nsIWebBrowserChrome::allChrome, 
                                                    ACTIVATION_WINDOW_WIDTH,   // width 
                                                    ACTIVATION_WINDOW_HEIGHT,  // height 
                                                    getter_AddRefs(mPregWindow));

            if (NS_FAILED(rv)) return rv;

            // be sure to register ourself as the parent content listener on the
            // webshell window we are creating.
            nsCOMPtr<nsIDocShell> docShellForWindow;
            NS_ENSURE_SUCCESS(mPregWindow->GetDocShell(
                                           getter_AddRefs(docShellForWindow)), 
                                           NS_ERROR_FAILURE);
            nsCOMPtr<nsIURIContentListener> ctnListener (do_GetInterface(docShellForWindow));

            ctnListener->SetParentContentListener(NS_STATIC_CAST(nsIURIContentListener *, this));

            /*
             * Start up the main event loop...
             */	
            rv = pregAppShell->Run();
        }

        ProcessPRegCookie();
    }
    return rv;
}

nsresult
nsProfile::CleanUp()
{
    nsresult rv = NS_OK;

    if (sHaveRedundantDirectory)
        DeleteUserDirectories(sRedundantDirectory);

    return rv;
}

nsresult
nsProfile::CheckDomain(PRBool *valid, const char* domain)
{
    NS_ASSERTION(valid, "Invalid valid pointer");
    NS_ASSERTION(domain, "Invalid domain");

    nsresult rv = NS_OK;
    NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsXPIDLCString domainPref;
    rv = prefs->CopyCharPref(ACTIVATION_ACCEPT_DOMAIN, getter_Copies(domainPref));
    
    if (PL_strcasecmp(domainPref, domain) == 0)
        *valid = PR_TRUE;

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
    rv = CreateNewProfile(defaultProfileName.GetUnicode(), dirSpecStr.GetUnicode());

    return rv;
}

// The following implementations of nsIURIContentListener and nsIInterfaceRequestor are 
// required if the profile manageris going to bring up a dialog for registration directly
// before we've actually created the hidden window or an application window...

NS_IMETHODIMP 
nsProfile::GetInterface(const nsIID & aIID, void * *aInstancePtr)
{
    NS_ENSURE_ARG_POINTER(aInstancePtr);
    return QueryInterface(aIID, aInstancePtr);
}

// nsIURIContentListener support
NS_IMETHODIMP nsProfile::OnStartURIOpen(nsIURI* aURI, 
   const char* aWindowTarget, PRBool* aAbortOpen)
{
   return NS_OK;
}

NS_IMETHODIMP
nsProfile::GetProtocolHandler(nsIURI *aURI, nsIProtocolHandler **aProtocolHandler)
{
    NS_ENSURE_ARG_POINTER(aURI);
    NS_ENSURE_ARG_POINTER(aProtocolHandler);

    *aProtocolHandler = nsnull;
    return NS_OK;
}

NS_IMETHODIMP 
nsProfile::GetParentContentListener(nsIURIContentListener** aParent)
{
    NS_ENSURE_ARG_POINTER(aParent);
    *aParent = nsnull;
    return NS_OK;
}

NS_IMETHODIMP 
nsProfile::SetParentContentListener(nsIURIContentListener* aParent)
{
    return NS_OK;
}

NS_IMETHODIMP 
nsProfile::GetLoadCookie(nsISupports ** aLoadCookie)
{
    NS_ENSURE_ARG_POINTER(aLoadCookie);
    *aLoadCookie = nsnull;
    return NS_OK;
}

NS_IMETHODIMP 
nsProfile::SetLoadCookie(nsISupports * aLoadCookie)
{
    return NS_OK;
}

NS_IMETHODIMP 
nsProfile::IsPreferred(const char * aContentType,
                        nsURILoadCommand aCommand,
                        const char * aWindowTarget,
                        char ** aDesiredContentType,
                        PRBool * aCanHandleContent)

{
    NS_ENSURE_ARG_POINTER(aContentType);
    NS_ENSURE_ARG_POINTER(aWindowTarget);
    NS_ENSURE_ARG_POINTER(aDesiredContentType);
    NS_ENSURE_ARG_POINTER(aCanHandleContent);

    return CanHandleContent(aContentType, aCommand, aWindowTarget, 
                            aDesiredContentType, aCanHandleContent);
}

NS_IMETHODIMP 
nsProfile::CanHandleContent(const char * aContentType,
                            nsURILoadCommand aCommand,
                            const char * aWindowTarget,
                            char ** aDesiredContentType,
                            PRBool * aCanHandleContent)

{
    // the chrome window the profile manager is bringing up for registration
    // needs to say that it handles ANY Content type because we haven't
    // actually started to run any of the applications yet (which means
    // that none of the applications are up and ready to handle any content).

    if (nsCRT::strcasecmp(aContentType, "message/rfc822") == 0)
        *aDesiredContentType = nsCRT::strdup("text/xul");

    // since we explicilty loaded the url, we always want to handle it!
    *aCanHandleContent = PR_TRUE;
    return NS_OK;
} 

NS_IMETHODIMP 
nsProfile::DoContent(const char * aContentType,
                        nsURILoadCommand aCommand,
                        const char * aWindowTarget,
                        nsIChannel * aOpenedChannel,
                        nsIStreamListener ** aContentHandler,
                        PRBool * aAbortProcess)
{
    NS_ENSURE_ARG_POINTER(aContentType);
    NS_ENSURE_ARG_POINTER(aWindowTarget);
    NS_ENSURE_ARG_POINTER(aOpenedChannel);
    NS_ENSURE_ARG_POINTER(aContentHandler);
    NS_ENSURE_ARG_POINTER(aAbortProcess);

    nsresult rv = NS_OK;

    // if we are currently showing a chromeless window for the registration stuff,
    // then forward the call to the webshell window...then we are done...
    if (mPregWindow)
    {
        nsCOMPtr<nsIDocShell> docshell;
        NS_ENSURE_SUCCESS(mPregWindow->GetDocShell(
                            getter_AddRefs(docshell)), NS_ERROR_FAILURE);

        nsCOMPtr<nsIURIContentListener> ctnListener (do_GetInterface(docshell));
        NS_ENSURE_TRUE(ctnListener, NS_ERROR_FAILURE);

        return ctnListener->DoContent(aContentType, aCommand, 
                                      aWindowTarget, aOpenedChannel, 
                                      aContentHandler, aAbortProcess);
    }

    return NS_OK;
}


NS_IMETHODIMP nsProfile::RemoveCookie(const char *cookie)
{
    NS_ENSURE_ARG_POINTER(cookie);

    nsresult rv = NS_OK;
    nsAutoString inputCookie; inputCookie.AssignWithConversion(PL_strstr(cookie, NS_ACTIVATION_COOOKIE));
    nsAutoString actvCookie;
    
    inputCookie.Mid(actvCookie, 0, inputCookie.Find(SEMICOLON_DELIMITER, 0));

    NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsXPIDLCString pregURL;
    rv = prefs->CopyCharPref(ACTIVATION_SERVER_URL, 
                             getter_Copies(pregURL));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIURI> pregURI;
    rv = NS_NewURI(getter_AddRefs(pregURI), pregURL);

    nsXPIDLCString host;
    nsXPIDLCString path;

    pregURI->GetHost(getter_Copies(host));
    nsAutoString domain; domain.AssignWithConversion(PL_strchr(host, '.'));

    pregURI->GetPath(getter_Copies(path));

    // Add domain
    actvCookie.AppendWithConversion(SEMICOLON_DELIMITER);
    actvCookie.AppendWithConversion(DOMAIN_STR);
    actvCookie += domain;

    // Add path
    actvCookie.AppendWithConversion(SEMICOLON_DELIMITER);
    actvCookie.AppendWithConversion(PATH_STR);
    actvCookie.AppendWithConversion(path);
	
    // Add expires string
    actvCookie.AppendWithConversion(SEMICOLON_DELIMITER);
    actvCookie.AppendWithConversion(ACTIVATION_COOKIE_EXPIRE_STR);

    NS_WITH_SERVICE(nsICookieService, service, kCookieServiceCID, &rv);
    if ((NS_OK == rv) && (nsnull != service) && (nsnull != pregURI)) {
        rv = service->SetCookieString(pregURI, actvCookie);
    }
    return rv;
}

