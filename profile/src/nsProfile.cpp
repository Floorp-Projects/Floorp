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
#include "nsEscape.h"
#include "nsIURL.h"

#include "nsIAppShellService.h"
#include "nsAppShellCIDs.h"
#include "prprf.h"

#include "nsIIOService.h"
#include "nsNetUtil.h"
#include "nsPrefMigration.h"
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
#include "nsIDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIChromeRegistry.h" // chromeReg
#include "nsIStringBundle.h"
#include "nsIObserverService.h"

// Interfaces Needed
#include "nsIDocShell.h"
#include "nsIWebBrowserChrome.h"

#include "nsIScriptGlobalObject.h"
#include "nsIBaseWindow.h"
#include "nsICommonDialogs.h"
#include "nsIDOMWindowInternal.h"
#include "nsIWindowMediator.h"

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

// hack for copying panels.rdf, localstore.rdf & mimeTypes.rdf into migrated profile dir
#define PANELS_RDF_FILE                "panels.rdf"
#define LOCALSTORE_RDF_FILE                "localstore.rdf"
#define MIMETYPES_RDF_FILE                "mimeTypes.rdf"

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
#define SHRIMP_PREF                    "shrimp.startup.enable"

#if defined (XP_MAC)
#define CHROME_STYLE nsIWebBrowserChrome::CHROME_WINDOW_BORDERS | nsIWebBrowserChrome::CHROME_WINDOW_CLOSE | nsIWebBrowserChrome::CHROME_CENTER_SCREEN
#else /* the rest */
#define CHROME_STYLE nsIWebBrowserChrome::CHROME_ALL | nsIWebBrowserChrome::CHROME_CENTER_SCREEN
#endif 

// we want everyone to have the debugging info to the console for now
// to help track down profile manager problems
// when we ship, we'll turn this off
#define DEBUG_profile 1
#undef DEBUG_profile_verbose
#ifdef DEBUG_seth
#define DEBUG_profile_verbose 1
#endif

// ProfileAccess varaible (gProfileDataAccess) to access registry operations
// gDataAccessInstCount is used to keep track of instance count to activate
// destructor at the right time (count == 0)
static nsProfileAccess*    gProfileDataAccess = nsnull;
static PRInt32          gInstanceCount = 0;
static PRBool           mCurrentProfileAvailable = PR_FALSE;

// Atoms for file locations
static nsIAtom* sApp_PrefsDirectory50         = nsnull;
static nsIAtom* sApp_PreferencesFile50        = nsnull;
static nsIAtom* sApp_UserProfileDirectory50   = nsnull;
static nsIAtom* sApp_UserChromeDirectory      = nsnull;
static nsIAtom* sApp_LocalStore50             = nsnull;
static nsIAtom* sApp_History50                = nsnull;
static nsIAtom* sApp_UsersPanels50            = nsnull;
static nsIAtom* sApp_UsersMimeTypes50         = nsnull;
static nsIAtom* sApp_BookmarksFile50          = nsnull;
static nsIAtom* sApp_SearchFile50             = nsnull;
static nsIAtom* sApp_MailDirectory50          = nsnull;
static nsIAtom* sApp_ImapMailDirectory50      = nsnull;
static nsIAtom* sApp_NewsDirectory50          = nsnull;
static nsIAtom* sApp_MessengerFolderCache50   = nsnull;

// IID and CIDs of all the services needed
static NS_DEFINE_CID(kIProfileIID, NS_IPROFILE_IID);
static NS_DEFINE_CID(kBookmarksCID, NS_BOOKMARKS_SERVICE_CID);      
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kRegistryCID, NS_REGISTRY_CID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kAppShellServiceCID, NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_IID(kIFactoryIID,  NS_IFACTORY_IID);
static NS_DEFINE_IID(kIIOServiceIID, NS_IIOSERVICE_IID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_CID(kPrefMigrationCID, NS_PREFMIGRATION_CID);
static NS_DEFINE_CID(kPrefConverterCID, NS_PREFCONVERTER_CID);
static NS_DEFINE_IID(kCookieServiceCID, NS_COOKIESERVICE_CID);
static NS_DEFINE_CID(kDialogParamBlockCID, NS_DialogParamBlock_CID);
static NS_DEFINE_CID(kWindowMediatorCID, NS_WINDOWMEDIATOR_CID);

static NS_DEFINE_CID(kChromeRegistryCID,    NS_CHROMEREGISTRY_CID);
static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);


/*
    Copies the contents of srcDir into destDir.
    destDir will be created if it doesn't exist.
*/

static
nsresult RecursiveCopy(nsIFile* srcDir, nsIFile* destDir)
{
    nsresult rv;
    PRBool isDir;
    
    rv = srcDir->IsDirectory(&isDir);
    if (NS_FAILED(rv)) return rv;
	if (!isDir) return NS_ERROR_INVALID_ARG;

    PRBool exists;
    rv = destDir->Exists(&exists);
	if (NS_SUCCEEDED(rv) && !exists)
		rv = destDir->Create(nsIFile::DIRECTORY_TYPE, 0775);
	if (NS_FAILED(rv)) return rv;

    PRBool hasMore = PR_FALSE;
    nsCOMPtr<nsISimpleEnumerator> dirIterator;
    rv = srcDir->GetDirectoryEntries(getter_AddRefs(dirIterator));
    if (NS_FAILED(rv)) return rv;
    
    rv = dirIterator->HasMoreElements(&hasMore);
    if (NS_FAILED(rv)) return rv;
    
    nsCOMPtr<nsIFile> dirEntry;
    
	while (hasMore)
	{
		rv = dirIterator->GetNext((nsISupports**)getter_AddRefs(dirEntry));
		if (NS_SUCCEEDED(rv))
		{
		    rv = dirEntry->IsDirectory(&isDir);
		    if (NS_SUCCEEDED(rv))
		    {
		        if (isDir)
		        {
		            nsCOMPtr<nsIFile> destClone;
		            rv = destDir->Clone(getter_AddRefs(destClone));
		            if (NS_SUCCEEDED(rv))
		            {
		                nsCOMPtr<nsILocalFile> newChild(do_QueryInterface(destClone));
		                nsXPIDLCString leafName;
		                dirEntry->GetLeafName(getter_Copies(leafName));
		                newChild->AppendRelativePath(leafName);
		                rv = RecursiveCopy(dirEntry, newChild);
		            }
		        }
		        else
		            rv = dirEntry->CopyTo(destDir, nsnull);
		    }
		
		}
        rv = dirIterator->HasMoreElements(&hasMore);
        if (NS_FAILED(rv)) return rv;
	}

	return rv;
}


/*
 * Constructor/Destructor
 */
nsProfile::nsProfile()
{
    NS_INIT_REFCNT();
    mAutomigrate = PR_FALSE;
    mOutofDiskSpace = PR_FALSE;
    mDiskSpaceErrorQuitCalled = PR_FALSE;

    if (gInstanceCount++ == 0) {
        
        gProfileDataAccess = new nsProfileAccess();
        
       // Make our directory atoms
        
       // Preferences:
         sApp_PrefsDirectory50         = NS_NewAtom(NS_APP_PREFS_50_DIR);
         sApp_PreferencesFile50        = NS_NewAtom(NS_APP_PREFS_50_FILE);
        
       // Profile:
         sApp_UserProfileDirectory50   = NS_NewAtom(NS_APP_USER_PROFILE_50_DIR);
        
       // Application Directories:
         sApp_UserChromeDirectory      = NS_NewAtom(NS_APP_USER_CHROME_DIR);
         
       // Aplication Files:
         sApp_LocalStore50             = NS_NewAtom(NS_APP_LOCALSTORE_50_FILE);
         sApp_History50                = NS_NewAtom(NS_APP_HISTORY_50_FILE);
         sApp_UsersPanels50            = NS_NewAtom(NS_APP_USER_PANELS_50_FILE);
         sApp_UsersMimeTypes50         = NS_NewAtom(NS_APP_USER_MIMETYPES_50_FILE);
         
       // Bookmarks:
         sApp_BookmarksFile50          = NS_NewAtom(NS_APP_BOOKMARKS_50_FILE);
         
       // Search
         sApp_SearchFile50             = NS_NewAtom(NS_APP_SEARCH_50_FILE);
         
       // MailNews
         sApp_MailDirectory50          = NS_NewAtom(NS_APP_MAIL_50_DIR);
         sApp_ImapMailDirectory50      = NS_NewAtom(NS_APP_IMAP_MAIL_50_DIR);
         sApp_NewsDirectory50          = NS_NewAtom(NS_APP_NEWS_50_DIR);
         sApp_MessengerFolderCache50   = NS_NewAtom(NS_APP_MESSENGER_FOLDER_CACHE_50_DIR);
         
         nsresult rv;
         NS_WITH_SERVICE(nsIDirectoryService, directoryService, NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
         if (NS_SUCCEEDED(rv))
            directoryService->RegisterProvider(this);
    }
}

nsProfile::~nsProfile() 
{
#if defined(DEBUG_profile_verbose)
    printf("~nsProfile \n");
#endif

   if (--gInstanceCount == 0) {
        
      delete gProfileDataAccess;

      NS_IF_RELEASE(sApp_PrefsDirectory50);
      NS_IF_RELEASE(sApp_PreferencesFile50);
      NS_IF_RELEASE(sApp_UserProfileDirectory50);
      NS_IF_RELEASE(sApp_UserChromeDirectory);
      NS_IF_RELEASE(sApp_LocalStore50);
      NS_IF_RELEASE(sApp_History50);
      NS_IF_RELEASE(sApp_UsersPanels50);
      NS_IF_RELEASE(sApp_UsersMimeTypes50);
      NS_IF_RELEASE(sApp_BookmarksFile50);
      NS_IF_RELEASE(sApp_SearchFile50);
      NS_IF_RELEASE(sApp_MailDirectory50);
      NS_IF_RELEASE(sApp_ImapMailDirectory50);
      NS_IF_RELEASE(sApp_NewsDirectory50);
      NS_IF_RELEASE(sApp_MessengerFolderCache50);
    }
}

/*
 * nsISupports Implementation
 */
NS_IMPL_THREADSAFE_ADDREF(nsProfile)
NS_IMPL_THREADSAFE_RELEASE(nsProfile)

NS_INTERFACE_MAP_BEGIN(nsProfile)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIProfile)
    NS_INTERFACE_MAP_ENTRY(nsIProfile)
    NS_INTERFACE_MAP_ENTRY(nsIProfileInternal)
    NS_INTERFACE_MAP_ENTRY(nsIDirectoryServiceProvider)
    NS_INTERFACE_MAP_ENTRY(nsIProfileChangeStatus)
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

#ifdef DEBUG_profile_verbose
    printf("Profile Manager : Profile Wizard and Manager activites : Begin\n");
#endif

    if (cmdLineArgs)
        rv = ProcessArgs(cmdLineArgs, &profileDirSet, profileURLStr);

    // This boolean is set only when an automigrated user runs out of disk space
    // and chooses to cancel further operations from the dialogs presented...
    if (mDiskSpaceErrorQuitCalled)
        return NS_ERROR_FAILURE;

    if (!profileDirSet) {
        rv = LoadDefaultProfileDir(profileURLStr);

        if (NS_FAILED(rv)) return rv;
    }

#ifdef DEBUG_profile_verbose
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

    PRBool shrimpPrefEnabled = PR_FALSE;
    prefs->GetBoolPref(SHRIMP_PREF, &shrimpPrefEnabled);

    if (shrimpPrefEnabled)
        profileURLStr = "";

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

    if ((profileURLStr.Length() != 0) && !(shrimpPrefEnabled))
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
        if (profileURLStr.Equals(CONFIRM_AUTOMIGRATE_URL)) {
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

    // Now we have the right profile, read the user-specific prefs.
    rv = prefs->ReadUserPrefs();
    if (NS_FAILED(rv)) return rv;

    NS_WITH_SERVICE(nsICategoryManager, catman, NS_CATEGORYMANAGER_CONTRACTID, &rv);

    if(NS_SUCCEEDED(rv) && catman) 
    {
        nsCOMPtr<nsISimpleEnumerator> enumItem;
        rv = catman->EnumerateCategory(NS_PROFILE_STARTUP_CATEGORY, getter_AddRefs(enumItem));
        if(NS_SUCCEEDED(rv) && enumItem) 
        {
           while (PR_TRUE) 
           {
               nsCOMPtr<nsISupportsString> contractid;

               rv = enumItem->GetNext(getter_AddRefs(contractid));
               if (NS_FAILED(rv) || !contractid) break;

               nsXPIDLCString contractidString;
               contractid->ToString (getter_Copies(contractidString));
        
               nsCOMPtr <nsIProfileStartupListener> listener = do_CreateInstance(contractidString, &rv);
        
               if (listener) 
                   listener->OnProfileStartup(currentProfileStr);
           }
        }
    }

    PRBool prefs_converted = PR_FALSE;
    rv = prefs->GetBoolPref("prefs.converted-to-utf8",&prefs_converted);
    if(NS_FAILED(rv)) return rv;

    if (!prefs_converted) 
    {
        nsCOMPtr <nsIPrefConverter> pPrefConverter = do_CreateInstance(kPrefConverterCID, &rv);
        if (NS_FAILED(rv)) return rv;
        if (!pPrefConverter) return NS_ERROR_FAILURE;

        rv = pPrefConverter->ConvertPrefsToUTF8();
    }
    return rv;
}

nsresult 
nsProfile::AutoMigrate()
{
    nsresult rv = NS_OK;
    // automatically migrate the one 4.x profile
    rv = MigrateAllProfiles();

    // Create a default profile if automigration failed for reasons
    // other than out of disk space case...
    if (NS_FAILED(rv) && !mOutofDiskSpace) 
    {
#ifdef DEBUG_profile
        printf("AutoMigration failed. Let's create a default 5.0 profile.\n");
#endif
        
        rv = CreateDefaultProfile();
        if (NS_FAILED(rv)) return rv;
    }   

    gProfileDataAccess->mProfileDataChanged = PR_TRUE;
    gProfileDataAccess->UpdateRegistry(nsnull);

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
    nsXPIDLCString cmdResult;
    nsCOMPtr<nsILocalFile> currProfileDir;

	// keep track of if the user passed us any profile related command line args
	// if they did, we won't force migration
	PRBool foundProfileCommandArg = PR_FALSE;

#ifdef DEBUG_profile_verbose
    printf("Profile Manager : Command Line Options : Begin\n");
#endif
 
    // check for command line arguments for profile manager
    //    
    // -P command line option works this way:
    // apprunner -P profilename 
    // runs the app using the profile <profilename> 
    // remembers profile for next time
    rv = cmdLineArgs->GetCmdLineValue(PROFILE_CMD_LINE_ARG, getter_Copies(cmdResult));
    if (NS_SUCCEEDED(rv))
    {
        if (cmdResult) {
			foundProfileCommandArg = PR_TRUE;
            nsAutoString currProfileName; currProfileName.AssignWithConversion(cmdResult);

#ifdef DEBUG_profile
            printf("ProfileName : %s\n", (const char*)cmdResult);
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
                nsCOMPtr<nsIFile> aFile;
                rv = GetProfileDir(currProfileName.GetUnicode(), getter_AddRefs(aFile));
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

    rv = cmdLineArgs->GetCmdLineValue(CREATE_PROFILE_CMD_LINE_ARG, getter_Copies(cmdResult));
    if (NS_SUCCEEDED(rv))
    {
        if (cmdResult) {
			foundProfileCommandArg = PR_TRUE;
            nsAutoString currProfileName; currProfileName.AssignWithConversion(strtok(NS_CONST_CAST(char*,(const char*)cmdResult), " "));
            nsAutoString currProfileDirString; currProfileDirString.AssignWithConversion(strtok(NULL, " "));
        
            if (!currProfileDirString.IsEmpty()) {
                rv = NS_NewUnicodeLocalFile(currProfileDirString.GetUnicode(), PR_TRUE, getter_AddRefs(currProfileDir));
                NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
            }
            else {
                // No directory name provided. Get File Locator
                // Get current profile, make the new one a sibling...
                nsCOMPtr<nsIFile> tempResult;                 
                rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILES_ROOT_DIR, getter_AddRefs(tempResult));
                NS_ENSURE_SUCCESS(rv, rv);
                rv = tempResult->QueryInterface(NS_GET_IID(nsILocalFile), getter_AddRefs(currProfileDir));
                NS_ENSURE_SUCCESS(rv, rv);
                rv = currProfileDir->AppendUnicode(currProfileName.GetUnicode());
                NS_ENSURE_SUCCESS(rv, rv);
            }
#ifdef DEBUG_profile_verbose
            printf("profileName & profileDir are: %s\n", (const char*)cmdResult);
#endif /* DEBUG_profile */

            nsXPIDLString currProfilePath;
            currProfileDir->GetUnicodePath(getter_Copies(currProfilePath));
            rv = CreateNewProfile(currProfileName.GetUnicode(), currProfilePath, nsnull, PR_TRUE);
            if (NS_SUCCEEDED(rv)) {
                *profileDirSet = PR_TRUE;
                mCurrentProfileAvailable = PR_TRUE;

                // Need to load new profile prefs.
                rv = LoadNewProfilePrefs();
                gProfileDataAccess->mProfileDataChanged = PR_TRUE;
                gProfileDataAccess->UpdateRegistry(nsnull);
            }
            rv = ForgetCurrentProfile();
            if (NS_FAILED(rv)) return rv;
        }
    }

    // Start Profile Manager
    rv = cmdLineArgs->GetCmdLineValue(PROFILE_MANAGER_CMD_LINE_ARG, getter_Copies(cmdResult));
    if (NS_SUCCEEDED(rv))
    {        
        if (cmdResult) {
			foundProfileCommandArg = PR_TRUE;
            profileURLStr = PROFILE_MANAGER_URL;
        }
    }
    
    // Start Profile Selection
    rv = cmdLineArgs->GetCmdLineValue(PROFILE_SELECTION_CMD_LINE_ARG, getter_Copies(cmdResult));
    if (NS_SUCCEEDED(rv))
    {        
        if (cmdResult) {
			foundProfileCommandArg = PR_TRUE;
            profileURLStr = PROFILE_SELECTION_URL;
        }
    }
    
    
    // Start Profile Wizard
    rv = cmdLineArgs->GetCmdLineValue(PROFILE_WIZARD_CMD_LINE_ARG, getter_Copies(cmdResult));
    if (NS_SUCCEEDED(rv))
    {        
        if (cmdResult) {
			foundProfileCommandArg = PR_TRUE;
            profileURLStr = PROFILE_WIZARD_URL;
        }
    }

	PRBool forceMigration = PR_FALSE;
	if (!foundProfileCommandArg) {
		rv = gProfileDataAccess->DetermineForceMigration(&forceMigration);
		NS_ASSERTION(NS_SUCCEEDED(rv),"failed to determine if we should force migration");
	}

    // Start Migaration activity
    rv = cmdLineArgs->GetCmdLineValue(INSTALLER_CMD_LINE_ARG, getter_Copies(cmdResult));
    if (NS_SUCCEEDED(rv) || forceMigration)
    {        
        if (cmdResult || forceMigration) {
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

#ifdef DEBUG_profile_verbose
    printf("Profile Manager : Command Line Options : End\n");
#endif

    return NS_OK;
}


/*
 * Getters
 */

// Gets the profiles directory for a given profile
// Sets the given profile to be a current profile
NS_IMETHODIMP nsProfile::GetProfileDir(const PRUnichar *profileName, nsIFile **profileDir)
{
    NS_ENSURE_ARG_POINTER(profileName);   
    NS_ENSURE_ARG_POINTER(profileDir);

    nsresult rv = NS_OK;

#if defined(DEBUG_profile_verbose)
    printf("ProfileManager : GetProfileDir\n");
#endif

    ProfileStruct    *aProfile;

    rv = gProfileDataAccess->GetValue(profileName, &aProfile);
    if (NS_FAILED(rv)) return rv;

	if (aProfile == nsnull)
		return NS_ERROR_FAILURE;

    nsCOMPtr<nsILocalFile>aProfileDir;
    (void)aProfile->GetResolvedProfileDir(getter_AddRefs(aProfileDir));
                           
    // Set this to be a current profile only if it is a 5.0 profile
    if (aProfile->isMigrated)
    {
        gProfileDataAccess->SetCurrentProfile(profileName);                          
        
        PRBool exists = PR_FALSE;

        if (aProfileDir)
        {
            rv = aProfileDir->Exists(&exists);
            if (NS_FAILED(rv)) return rv;
            
#ifdef XP_MAC
            if (exists)
            {
                PRBool inTrash;
                nsCOMPtr<nsIFile> trashFolder;
                            
                rv = NS_GetSpecialDirectory(NS_MAC_TRASH_DIR, getter_AddRefs(trashFolder));
                if (NS_FAILED(rv)) return rv;
                rv = trashFolder->Contains(aProfileDir, PR_TRUE, &inTrash);
                if (NS_FAILED(rv)) return rv;
                if (inTrash)
                    aProfileDir = nsnull;   // Reset it - we'll make a new one
            }
#endif  
        }
                
        if (!aProfileDir || !exists) 
        {
            // NEEDS IMPROVEMENT: Here we are creating a new profile dir for a missing one and
            // we're not even telling the user about this!! What if they moved it, had it on a ZIP
            // drive which is not mounted, etc. 4.x at least used to prompt the user to find it.
                                                                  
            if (!aProfileDir) {
                // Create the directory in the default location
                // Since we're copying to default location - make it unique
                nsCOMPtr<nsIFile> tempFile;
                rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILES_ROOT_DIR, getter_AddRefs(tempFile));
                if (NS_FAILED(rv)) return rv;
                aProfileDir = do_QueryInterface(tempFile, &rv);
                if (NS_FAILED(rv)) return rv;
                aProfileDir->AppendUnicode(profileName);
                nsXPIDLCString suggestedName;
                rv = aProfileDir->GetLeafName(getter_Copies(suggestedName));
                if (NS_FAILED(rv)) return rv;
                rv = aProfileDir->CreateUnique(suggestedName, nsIFile::DIRECTORY_TYPE, 0775);
                if (NS_FAILED(rv)) return rv;
            }
            else if (!exists) {
                // Simply create the directory where the file specifies
                // We have a valid file spec in a given location - don't CreateUnique
                rv = aProfileDir->Create(nsIFile::DIRECTORY_TYPE, 0775);
                if (NS_FAILED(rv)) return rv;
            }

	        // creating a new profile, add the indirection
            rv = AddLevelOfIndirection(aProfileDir);
            if (NS_FAILED(rv)) return rv;

            // create the sub directories
            rv = CreateUserDirectories(aProfileDir);
            if (NS_FAILED(rv)) return rv;
            
			// since we might be changing the location of
			// profile dir, reset it in the registry.
			// see bug #56002 for details
			rv = SetProfileDir(profileName, aProfileDir);
            if (NS_FAILED(rv)) return rv;

            // update the registry
            gProfileDataAccess->mProfileDataChanged = PR_TRUE;
            gProfileDataAccess->UpdateRegistry(nsnull);

            // Copy contents from defaults folder.
            nsCOMPtr<nsIFile> profDefaultsDir;
            rv = NS_GetSpecialDirectory(NS_APP_PROFILE_DEFAULTS_50_DIR, getter_AddRefs(profDefaultsDir));
            if (NS_FAILED(rv)) return rv;
            rv = profDefaultsDir->Exists(&exists);
            if (NS_SUCCEEDED(rv) && exists)
                rv = RecursiveCopy(profDefaultsDir, aProfileDir);
        }
    }

    *profileDir = aProfileDir;
    NS_ADDREF(*profileDir);
    
	 delete aProfile;
    return rv;

}

NS_IMETHODIMP nsProfile::GetDefaultProfileParentDir(nsIFile **aDefaultProfileParentDir)
{
    NS_ENSURE_ARG_POINTER(aDefaultProfileParentDir);

    nsresult rv;
    
    nsCOMPtr<nsIFile> aDir;
    rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILES_ROOT_DIR, getter_AddRefs(aDir));
    NS_ENSURE_SUCCESS(rv, rv);

    *aDefaultProfileParentDir = aDir;
    NS_ADDREF(*aDefaultProfileParentDir);

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


NS_IMETHODIMP
nsProfile::SetCurrentProfile(const PRUnichar * aCurrentProfile)
{
    NS_ENSURE_ARG(aCurrentProfile);
    
    nsresult rv;
    PRBool exists;
    rv = ProfileExists(aCurrentProfile, &exists);
    if (NS_FAILED(rv)) return rv;
    if (!exists) return NS_ERROR_FAILURE;

    PRBool isSwitch;

    if (mCurrentProfileAvailable)
    {
        nsXPIDLString currProfileName;
        rv = GetCurrentProfile(getter_Copies(currProfileName));
        if (NS_FAILED(rv)) return rv;
        if (nsCRT::strcmp(aCurrentProfile, currProfileName.get()) == 0)
            return NS_OK;
        else
            isSwitch = PR_TRUE;
    }
    else
        isSwitch = PR_FALSE;
            
    NS_WITH_SERVICE(nsIProperties, directoryService, NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
    NS_ENSURE_TRUE(directoryService, NS_ERROR_FAILURE);
    
    NS_WITH_SERVICE(nsIObserverService, observerService, NS_OBSERVERSERVICE_CONTRACTID, &rv);
    NS_ENSURE_TRUE(observerService, NS_ERROR_FAILURE);
    
    nsISupports *subject = (nsISupports *)((nsIProfile *)this);
            
    if (isSwitch)
    {
        // Phase 1: See if anybody objects to the profile being changed.
        mProfileChangeVetoed = PR_FALSE;        
        observerService->Notify(subject, PROFILE_APPROVE_CHANGE_TOPIC, nsnull);
        if (mProfileChangeVetoed)
            return NS_OK;

        // Phase 2: Send the "teardown" notification
        observerService->Notify(subject, PROFILE_CHANGE_TEARDOWN_TOPIC, nsnull);
        
        // Phase 3: Notify observers of a profile change
        observerService->Notify(subject, PROFILE_BEFORE_CHANGE_TOPIC, nsnull);        
    }
    
    // Do the profile switch    
    gProfileDataAccess->SetCurrentProfile(aCurrentProfile);
    gProfileDataAccess->mProfileDataChanged = PR_TRUE;
    gProfileDataAccess->UpdateRegistry(nsnull);
            
    if (NS_FAILED(rv)) return rv;
    mCurrentProfileAvailable = PR_TRUE;
    
    if (isSwitch)
    {
        (void) directoryService->Undefine(NS_APP_PREFS_50_DIR);
        (void) directoryService->Undefine(NS_APP_PREFS_50_FILE);
        (void) directoryService->Undefine(NS_APP_USER_PROFILE_50_DIR);
        (void) directoryService->Undefine(NS_APP_USER_CHROME_DIR);
        (void) directoryService->Undefine(NS_APP_LOCALSTORE_50_FILE);
        (void) directoryService->Undefine(NS_APP_HISTORY_50_FILE);
        (void) directoryService->Undefine(NS_APP_USER_PANELS_50_FILE);
        (void) directoryService->Undefine(NS_APP_USER_MIMETYPES_50_FILE);
        (void) directoryService->Undefine(NS_APP_BOOKMARKS_50_FILE);
        (void) directoryService->Undefine(NS_APP_SEARCH_50_FILE);
        (void) directoryService->Undefine(NS_APP_MAIL_50_DIR);
        (void) directoryService->Undefine(NS_APP_IMAP_MAIL_50_DIR);
        (void) directoryService->Undefine(NS_APP_NEWS_50_DIR);
        (void) directoryService->Undefine(NS_APP_MESSENGER_FOLDER_CACHE_50_DIR);

        // Phase 4: Notify observers that the profile has changed - Here they respond to new profile
        observerService->Notify(subject, PROFILE_DO_CHANGE_TOPIC, nsnull);

        // Phase 5: Now observers can respond to something another observer did in phase 4
        observerService->Notify(subject, PROFILE_AFTER_CHANGE_TOPIC, nsnull);
    }
    else
      rv = LoadNewProfilePrefs();
      
    return NS_OK;
}


// Returns the name of the current profile directory
NS_IMETHODIMP nsProfile::GetCurrentProfileDir(nsIFile **profileDir)
{
    NS_ENSURE_ARG_POINTER(profileDir);
    nsresult rv;

    nsXPIDLString profileName;
    rv = GetCurrentProfile(getter_Copies(profileName));
    if (NS_FAILED(rv)) return rv;

    rv = GetProfileDir(profileName, profileDir);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

#define SALT_SIZE 8
#define TABLE_SIZE 36
#define SALT_EXTENSION ".slt"

const char table[] = 
	{ 'a','b','c','d','e','f','g','h','i','j',
	  'k','l','m','n','o','p','q','r','s','t',
	  'u','v','w','x','y','z','0','1','2','3',
	  '4','5','6','7','8','9'};

// for security, add a level of indirection:
// an extra directory with a hard to guess name.

nsresult
nsProfile::AddLevelOfIndirection(nsIFile *aDir)
{
  nsresult rv;
  PRBool exists = PR_FALSE;
  if (!aDir) return NS_ERROR_NULL_POINTER;

  // check if aDir/prefs.js exists, if so, use it.
  // else, check if aDir/*.slt exists, if so, use it.
  // else, do the salt
  nsCOMPtr<nsIFile> prefFile;
  rv = aDir->Clone(getter_AddRefs(prefFile));
  NS_ENSURE_SUCCESS(rv,rv);

  rv = prefFile->Append("prefs.js");
  NS_ENSURE_SUCCESS(rv,rv);

  rv = prefFile->Exists(&exists);
  NS_ENSURE_SUCCESS(rv,rv);

  if (exists) {
	// there is a prefs.js file in aDir, so just use aDir and don't salt
	return NS_OK;
  }

  // no prefs.js, now search for a .slt directory
  PRBool hasMore = PR_FALSE;
  PRBool isDir = PR_FALSE;
  nsCOMPtr<nsISimpleEnumerator> dirIterator;
  rv = aDir->GetDirectoryEntries(getter_AddRefs(dirIterator));
  NS_ENSURE_SUCCESS(rv,rv);

  rv = dirIterator->HasMoreElements(&hasMore);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr<nsIFile> dirEntry;

  while (hasMore) {
    rv = dirIterator->GetNext((nsISupports**)getter_AddRefs(dirEntry));
    if (NS_SUCCEEDED(rv)) {
      rv = dirEntry->IsDirectory(&isDir);
      if (NS_SUCCEEDED(rv) && isDir) {
        nsXPIDLCString leafName;
        rv = dirEntry->GetLeafName(getter_Copies(leafName));
	 	if (NS_SUCCEEDED(rv) && (const char *)leafName) {
		  PRUint32 length = nsCRT::strlen((const char *)leafName);
		  // check if the filename is the right length, len("xxxxxxxx.slt")
		  if (length == (SALT_SIZE + nsCRT::strlen(SALT_EXTENSION))) {
			// check that the filename ends with ".slt"
			if (nsCRT::strncmp((const char *)leafName + SALT_SIZE, SALT_EXTENSION, nsCRT::strlen(SALT_EXTENSION)) == 0) {
			  // found a salt directory, use it
			  rv = aDir->Append((const char *)leafName);
			  return rv;
			}
		  }
		}
      }
    }
    rv = dirIterator->HasMoreElements(&hasMore);
    NS_ENSURE_SUCCESS(rv,rv);
  }
  
  // if we get here, we need to add the extra directory

  // turn PR_Now() into milliseconds since epoch
  // and salt rand with that.
  double fpTime;
  LL_L2D(fpTime, PR_Now());
  srand((uint)(fpTime * 1e-6 + 0.5));	// use 1e-6, granularity of PR_Now() on the mac is seconds

  nsCAutoString saltStr;
  PRInt32 i;
  for (i=0;i<SALT_SIZE;i++) {
  	saltStr.Append(table[rand()%TABLE_SIZE]);
  }
  saltStr.Append(SALT_EXTENSION);
#ifdef DEBUG_profile_verbose
  printf("directory name: %s\n",(const char *)saltStr);
#endif

  rv = aDir->Append((const char *)saltStr);
  NS_ENSURE_SUCCESS(rv,rv);

  exists = PR_FALSE;
  rv = aDir->Exists(&exists);
  NS_ENSURE_SUCCESS(rv,rv);
  if (!exists) {
    rv = aDir->Create(nsIFile::DIRECTORY_TYPE, 0775);
    NS_ENSURE_SUCCESS(rv,rv);
  }
	
  return NS_OK;
}


/*
 * Setters
 */

// Sets the current profile directory
nsresult nsProfile::SetProfileDir(const PRUnichar *profileName, nsIFile *profileDir)
{
    NS_ENSURE_ARG(profileName);
    NS_ENSURE_ARG(profileDir);   

    nsresult rv = NS_OK;
 
    // Need to ensure that this directory exists
    PRBool exists;
    rv = profileDir->Exists(&exists);
    if (NS_SUCCEEDED(rv) && !exists)
        rv = profileDir->Create(nsIFile::DIRECTORY_TYPE, 0775);
    if (NS_FAILED(rv)) return rv;
    
    nsCOMPtr<nsILocalFile> localFile(do_QueryInterface(profileDir));
    NS_ENSURE_TRUE(localFile, NS_ERROR_FAILURE);                

    ProfileStruct* aProfile = new ProfileStruct();
    NS_ENSURE_TRUE(aProfile, NS_ERROR_OUT_OF_MEMORY);


    aProfile->profileName     = profileName;
    aProfile->SetResolvedProfileDir(localFile);
    aProfile->isMigrated = PR_TRUE;

    gProfileDataAccess->SetValue(aProfile);
    
    delete aProfile;

    return rv;
}


// Creates a new profile
NS_IMETHODIMP 
nsProfile::CreateNewProfile(const PRUnichar* profileName, 
                            const PRUnichar* nativeProfileDir,
                            const PRUnichar* langcode,
                            PRBool useExistingDir)
{
    NS_ENSURE_ARG_POINTER(profileName);   

    nsresult rv = NS_OK;

#if defined(DEBUG_profile)
    {
      printf("ProfileManager : CreateNewProfile\n");

      nsCAutoString temp1; temp1.AssignWithConversion(profileName);
      printf("Profile Name: %s\n", NS_STATIC_CAST(const char*, temp1));

      if (nativeProfileDir) {
      nsCAutoString temp2; temp2.AssignWithConversion(nativeProfileDir);
      printf("Profile Dir: %s\n", NS_STATIC_CAST(const char*, temp2));
    }
    }
#endif

    nsCOMPtr<nsIFile> profileDir;
    PRBool exists;
    
    if (!nativeProfileDir)
    {
        // They didn't specify a directory path...
                
        rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILES_ROOT_DIR, getter_AddRefs(profileDir));
        if (NS_FAILED(rv)) return rv;
        rv = profileDir->Exists(&exists);
        if (NS_FAILED(rv)) return rv;        
        if (!exists)
            profileDir->Create(nsIFile::DIRECTORY_TYPE, 0775);

        // append profile name
        profileDir->AppendUnicode(profileName);
    }
    else {
    
        rv = NS_NewUnicodeLocalFile(nativeProfileDir, PR_TRUE, (nsILocalFile **)((nsIFile **)getter_AddRefs(profileDir)));

        // this prevents people from choosing there profile directory
        // or another directory, and remove it when they delete the profile.
        // append profile name
        profileDir->AppendUnicode(profileName);
    }


    // Make profile directory unique only when the user 
    // decides to not use an already existing profile directory
    if (!useExistingDir) {
        nsXPIDLCString  suggestedName;
        profileDir->GetLeafName(getter_Copies(suggestedName));
        rv = profileDir->CreateUnique(suggestedName, nsIFile::DIRECTORY_TYPE, 0775);
        if (NS_FAILED(rv)) return rv;
    }

#if defined(DEBUG_profile_verbose)
    printf("before SetProfileDir\n");
#endif

    rv = profileDir->Exists(&exists);
    if (NS_FAILED(rv)) return rv;        
    if (!exists)
    {
        rv = profileDir->Create(nsIFile::DIRECTORY_TYPE, 0775);
        if (NS_FAILED(rv)) return rv;
        useExistingDir = PR_FALSE;
    }

    // since the directory didn't exist, add the indirection
    rv = AddLevelOfIndirection(profileDir);
    if (NS_FAILED(rv)) return rv;

    // Create the sub directories
    rv = CreateUserDirectories(profileDir);  
    // Set the directory value and add the entry to the registry tree.
    rv = SetProfileDir(profileName, profileDir);

#if defined(DEBUG_profile_verbose)
    printf("after SetProfileDir\n");
#endif

    // Get profile defaults folder..
    nsCOMPtr <nsIFile> profDefaultsDir;
    rv = NS_GetSpecialDirectory(NS_APP_PROFILE_DEFAULTS_NLOC_50_DIR, getter_AddRefs(profDefaultsDir));
    if (NS_FAILED(rv)) return rv;

    if (langcode && nsCRT::strlen(langcode) != 0) {
        // caller prefers locale subdir
        nsCOMPtr<nsIFile> locProfDefaultsDir;
        rv = profDefaultsDir->Clone(getter_AddRefs(locProfDefaultsDir));
        if (NS_FAILED(rv)) return rv;
        
        locProfDefaultsDir->AppendUnicode(langcode);
        rv = locProfDefaultsDir->Exists(&exists);
        if (NS_SUCCEEDED(rv) && exists) {
            profDefaultsDir = locProfDefaultsDir; // transfers ownership
        }

        nsCOMPtr<nsIChromeRegistry> chromeRegistry = do_GetService(kChromeRegistryCID, &rv);
        if (NS_SUCCEEDED(rv)) {
            nsXPIDLCString  pathBuf;
            rv = profileDir->GetPath(getter_Copies(pathBuf));
            NS_ENSURE_SUCCESS(rv, rv);
            nsFileSpec fileSpec((const char *)pathBuf);
            nsFileURL  fileURL(fileSpec);
            const char* fileStr = fileURL.GetURLString();
            rv = chromeRegistry->SelectLocaleForProfile(langcode, 
                                                        NS_ConvertUTF8toUCS2(fileStr));
        }
    }
    // Copy contents from defaults folder.
    rv = profDefaultsDir->Exists(&exists);
    if (NS_SUCCEEDED(rv) && exists && (!useExistingDir))
    {
        RecursiveCopy(profDefaultsDir, profileDir);
    }

    gProfileDataAccess->mNumProfiles++;
    gProfileDataAccess->mProfileDataChanged = PR_TRUE;
    gProfileDataAccess->UpdateRegistry(nsnull);
    
    return NS_OK;
}	


// Create required user directories like ImapMail, Mail, News, Cache etc.
nsresult nsProfile::CreateUserDirectories(nsIFile *profileDir)
{
    nsresult rv = NS_OK;

#if defined(DEBUG_profile_verbose)
    printf("ProfileManager : CreateUserDirectories\n");
#endif

    const char* subDirNames[] = {
        NEW_NEWS_DIR_NAME,
        NEW_IMAPMAIL_DIR_NAME,
        NEW_MAIL_DIR_NAME,
        "Cache"
    };
    
    for (PRUint32 i = 0; i < sizeof(subDirNames) / sizeof(char *); i++)
    {
      PRBool exists;
      
      nsCOMPtr<nsIFile> newDir;    
      rv = profileDir->Clone(getter_AddRefs(newDir));
      if (NS_FAILED(rv)) return rv;
      rv = newDir->Append(subDirNames[i]);
      if (NS_FAILED(rv)) return rv;
      rv = newDir->Exists(&exists);
      if (NS_SUCCEEDED(rv) && !exists)
          rv = newDir->Create(nsIFile::DIRECTORY_TYPE, 0775);
      if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}


// Delete all user directories associated with the a profile
// A FileSpec of the profile's directory is taken as input param
nsresult nsProfile::DeleteUserDirectories(nsIFile *profileDir)
{
    NS_ENSURE_ARG(profileDir);
    nsresult rv;

#if defined(DEBUG_profile_verbose)
    printf("ProfileManager : DeleteUserDirectories\n");
#endif

    PRBool exists;
    rv = profileDir->Exists(&exists);
    if (NS_FAILED(rv)) return rv;
    
    if (exists)
        rv = profileDir->Delete(PR_TRUE);
    
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
     
	/* note, we do not rename the directory on disk to the new name
	 * this is on purpose.
     *
	 * we don't require the directory name to match the profile name, 
	 * but it usually does.  
	 * (the pairing of values occurs in the profile registry)
	 * 
	 * Imagine this scenario:
	 * 1) user creates a profile "foo" and the directory gets named "foo".
 	 * 2) user creates a profile "bar" and the directory gets named "bar"
	 * 3) user deletes the profile "foo", but chooses not to delete the files on disk.  (they are given this option when deleting a profile)
	 * 4) user renames "bar" profile to "foo", but still uses the directory named "bar" on disk
	 *
	 * bad things would happen if we tried to rename the directory
	 */
    
    /* profile is just replaced. But Keep up the count */
    gProfileDataAccess->mNumProfiles++;

    rv = ForgetCurrentProfile();
    if (NS_FAILED(rv)) return rv;

    gProfileDataAccess->mProfileDataChanged = PR_TRUE;
    gProfileDataAccess->UpdateRegistry(nsnull);

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
        
        nsCOMPtr<nsIFile> profileDir;
        rv = GetProfileDir(profileName, getter_AddRefs(profileDir));
        if (NS_FAILED(rv)) return rv;
        
        nsCOMPtr<nsILocalFile> localProfileDir(do_QueryInterface(profileDir));
        if (!localProfileDir) return NS_ERROR_FAILURE;
        rv = DeleteUserDirectories(localProfileDir);
        if (NS_FAILED(rv)) return rv;
    }

    // Remove the subtree from the registry.
    gProfileDataAccess->RemoveSubTree(profileName);
    if (NS_FAILED(rv)) return rv;

    gProfileDataAccess->mProfileDataChanged = PR_TRUE;
    gProfileDataAccess->UpdateRegistry(nsnull);

    return rv;
}


NS_IMETHODIMP nsProfile::GetProfileList(PRUint32 *length, PRUnichar ***profileNames)
{
    NS_ENSURE_ARG_POINTER(length);
    *length = 0;
    NS_ENSURE_ARG_POINTER(profileNames);
    *profileNames = nsnull;
    
    return gProfileDataAccess->GetProfileList(nsIProfileInternal::LIST_ONLY_NEW, length, profileNames);
}
 

NS_IMETHODIMP nsProfile::GetProfileListX(PRUint32 whichKind, PRUint32 *length, PRUnichar ***profileNames)
{
    NS_ENSURE_ARG_POINTER(length);
    *length = 0;
    NS_ENSURE_ARG_POINTER(profileNames);
    *profileNames = nsnull;
    
    return gProfileDataAccess->GetProfileList(whichKind, length, profileNames);
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
      printf("profileName passed in: %s\n", NS_STATIC_CAST(const char*, temp));
    }
#endif

    // flush the stringbundle cache first
#if defined(DEBUG_tao)
    printf("\n--> nsProfile::StartApprunner: FlushBundles() \n");
#endif
    nsCOMPtr<nsIStringBundleService> bundleService =
        do_GetService(kStringBundleServiceCID, &rv);

    if (NS_SUCCEEDED(rv)) {
        rv = bundleService->FlushBundles();
        NS_ASSERTION(NS_SUCCEEDED(rv), "failed to flush bundle cache");
    }

    gProfileDataAccess->SetCurrentProfile(profileName);
    mCurrentProfileAvailable = PR_TRUE;

    // Update registry entries
    gProfileDataAccess->mProfileDataChanged = PR_TRUE;
    gProfileDataAccess->UpdateRegistry(nsnull);
    
    // Need to load new profile prefs.
    rv = LoadNewProfilePrefs();
    return rv;
}

nsresult nsProfile::LoadNewProfilePrefs()
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

#if defined(DEBUG_profile_verbose)
    printf("Entered MigrateProfileInfo.\n");
#endif

    char oldRegFile[_MAX_LENGTH] = {'\0'};

#ifdef XP_PC
#ifdef XP_OS2
    nsSpecialSystemDirectory systemDir(nsSpecialSystemDirectory::OS2_OS2Directory);
#else
    // Registry file has been traditionally stored in the windows directory (XP_PC).
    nsSpecialSystemDirectory systemDir(nsSpecialSystemDirectory::Win_WindowsDirectory);
#endif
    
    // Append the name of the old registry to the path obtained.
    PL_strcpy(oldRegFile, systemDir.GetNativePathCString());
    PL_strcat(oldRegFile, OLD_REGISTRY_FILE_NAME);
#else /* XP_MAC */
    nsSpecialSystemDirectory regLocation(nsSpecialSystemDirectory::Mac_PreferencesDirectory);
    
    // Append the name of the old registry to the path obtained.
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
    gProfileDataAccess->UpdateRegistry(nsnull);

	return rv;
}

nsresult
nsProfile::CopyDefaultFile(nsIFile *profDefaultsDir, nsIFile *newProfDir, const char *fileName)
{
    nsresult rv;
    nsCOMPtr<nsIFile> defaultFile;
    PRBool exists;
    
    rv = profDefaultsDir->Clone(getter_AddRefs(defaultFile));
    if (NS_FAILED(rv)) return rv;
    
    defaultFile->Append(fileName);
    rv = defaultFile->Exists(&exists);
    if (NS_FAILED(rv)) return rv;
    if (exists)
        rv = defaultFile->CopyTo(newProfDir, fileName);
    else
        rv = NS_ERROR_FILE_NOT_FOUND;

	return rv;
}


nsresult
nsProfile::EnsureProfileFileExists(nsIFile *aFile)
{
    nsresult rv;
    PRBool exists;
    
    rv = aFile->Exists(&exists);
    if (NS_FAILED(rv)) return rv;
    if (exists)
        return NS_OK;
        
    nsCOMPtr<nsIFile> defaultsDir;
    nsCOMPtr<nsILocalFile> profDir;
    
    rv = NS_GetSpecialDirectory(NS_APP_PROFILE_DEFAULTS_50_DIR, getter_AddRefs(defaultsDir));
    if (NS_FAILED(rv)) return rv;
    rv = CloneProfileDirectorySpec(getter_AddRefs(profDir));
    if (NS_FAILED(rv)) return rv;
    
    char *leafName;
    rv = aFile->GetLeafName(&leafName);
    if (NS_FAILED(rv)) return rv;    
    rv = CopyDefaultFile(defaultsDir, profDir, leafName);
    Recycle(leafName);
    
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

    nsCOMPtr<nsIFile> oldProfDir;    
    nsCOMPtr<nsIFile> newProfDir;
 
    rv = GetProfileDir(profileName, getter_AddRefs(oldProfDir));
    if (NS_FAILED(rv)) return rv;
   
    rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILES_ROOT_DIR, getter_AddRefs(newProfDir));
    if (NS_FAILED(rv)) return rv;
    rv = newProfDir->AppendUnicode(profileName);
    if (NS_FAILED(rv)) return rv;
    
    // This is unfortunate: There is no CreateUniqueUnicode
    nsXPIDLCString suggestedName;
    rv = newProfDir->GetLeafName(getter_Copies(suggestedName));
    if (NS_FAILED(rv)) return rv;
    rv = newProfDir->CreateUnique(suggestedName, nsIFile::DIRECTORY_TYPE, 0775);
    if (NS_FAILED(rv)) return rv;
    
    // always create level indirection when migrating
    rv = AddLevelOfIndirection(newProfDir);
    if (NS_FAILED(rv)) return rv;

    // Call migration service to do the work.
    nsCOMPtr <nsIPrefMigration> pPrefMigrator;


    rv = nsComponentManager::CreateInstance(kPrefMigrationCID, 
                                            nsnull,
                                            NS_GET_IID(nsIPrefMigration),
                                            getter_AddRefs(pPrefMigrator));
    if (NS_FAILED(rv)) return rv;
    if (!pPrefMigrator) return NS_ERROR_FAILURE;
        
    nsCOMPtr<nsILocalFile> oldProfDirLocal(do_QueryInterface(oldProfDir, &rv));
    if (NS_FAILED(rv)) return rv;    
    nsCOMPtr<nsILocalFile> newProfDirLocal(do_QueryInterface(newProfDir, &rv));
    if (NS_FAILED(rv)) return rv;
        
    nsXPIDLCString oldProfDirStr;
    nsXPIDLCString newProfDirStr;

    rv = oldProfDirLocal->GetPersistentDescriptor(getter_Copies(oldProfDirStr));
    if (NS_FAILED(rv)) return rv;
    rv = newProfDirLocal->GetPersistentDescriptor(getter_Copies(newProfDirStr));
    if (NS_FAILED(rv)) return rv;

    // you can do this a bunch of times.
    rv = pPrefMigrator->AddProfilePaths(oldProfDirStr, newProfDirStr);  

    rv = pPrefMigrator->ProcessPrefs(showProgressAsModalWindow);
    if (NS_FAILED(rv)) return rv;

    // check for diskspace errors  
    nsresult errorCode;   
    errorCode = pPrefMigrator->GetError();

    // In either of the cases below we have to return error to make
    // app understand that migration has failed.
    if (errorCode == CREATE_NEW)
    {
        PRInt32 numProfiles = 0;
        ShowProfileWizard();

        // When the automigration process fails because of disk space error,
        // we present user a create profile wizard if the user chooses to create a 
        // a profile then. But then the user may click on cancel on that dialog...
        // So, if the user clicks on cancel, the number of profiles should be 
        // ZERO at the point for the user who failed to automigrate single 4x profile.
        // On such condition, set mDiskSpaceErrorQuitCalled to allow user to quit the app.
        // If the user is presented with profilemanager dialog with multiple 4x profiles
        // to migrate, value of mDiskSpaceErrorQuitCalled does not matter as it gets ignored..
        // If a single profile needs automigration and no confirmation 
        // is needed for that operation mAutomigrate is set to false. 
        if (!mAutomigrate)
        {
            GetProfileCount(&numProfiles);
            if (numProfiles == 0)
                mDiskSpaceErrorQuitCalled = PR_TRUE;
        }
        mOutofDiskSpace = PR_TRUE;
        return NS_ERROR_FAILURE;
    }
    else if (errorCode == CANCEL) 
    {
        // When the automigration process fails because of disk space error,
        // user may choose to simply quit the app from the dialog presented 
        // by pref-migrator. So, set mDiskSpaceErrorQuitCalled to allow user 
        // to quit the app in such a case.
        // If the user is presented with profilemanager dialog with multiple 4x profiles
        // to migrate, value of mDiskSpaceErrorQuitCalled does not matter as it gets ignored..
        // If a single profile needs automigration and no confirmation 
        // is needed for that operation mAutomigrate is set to false. 
        if (!mAutomigrate)
            mDiskSpaceErrorQuitCalled = PR_TRUE;

        ForgetCurrentProfile();
        mOutofDiskSpace = PR_TRUE;
        return NS_ERROR_FAILURE;
    }
    else if (errorCode != SUCCESS) 
    {
        return NS_ERROR_FAILURE;
    }

    // Copy the default 5.0 profile files into the migrated profile
    // Get profile defaults folder..
    nsCOMPtr<nsIFile> profDefaultsDir;
    rv = NS_GetSpecialDirectory(NS_APP_PROFILE_DEFAULTS_50_DIR, getter_AddRefs(profDefaultsDir));
    if (NS_FAILED(rv)) return rv;

    // Copy panels.rdf, localstore.rdf & mimeTypes.rdf files
    // This is a hack. Once the localFileSpec implementation
    // is complete, this will be removed.
	rv = CopyDefaultFile(profDefaultsDir, newProfDir, LOCALSTORE_RDF_FILE);
	if (NS_FAILED(rv)) return rv;
	rv = CopyDefaultFile(profDefaultsDir, newProfDir, PANELS_RDF_FILE);
	if (NS_FAILED(rv)) return rv;
	rv = CopyDefaultFile(profDefaultsDir, newProfDir, MIMETYPES_RDF_FILE);
	NS_ASSERTION(NS_SUCCEEDED(rv), "failed to copy default mimeTypes.rdf file");
    // hack finish.
	
	rv = CreateUserDirectories(newProfDir);
	if (NS_FAILED(rv)) return rv;
    rv = SetProfileDir(profileName, newProfDir);
    if (NS_FAILED(rv)) return rv;

    gProfileDataAccess->mNumProfiles++;
    gProfileDataAccess->mNumOldProfiles--;

    gProfileDataAccess->mProfileDataChanged = PR_TRUE;
    gProfileDataAccess->UpdateRegistry(nsnull);

    return rv;
}

nsresult
nsProfile::ShowProfileWizard(void)
{
    nsresult rv = NS_OK;
    PRBool hasParentWindow = PR_FALSE;
    nsCOMPtr<nsIDOMWindowInternal> PMDOMWindow;

    // Get the window mediator
    NS_WITH_SERVICE(nsIWindowMediator, windowMediator, kWindowMediatorCID, &rv);
    if (NS_SUCCEEDED(rv)) 
    {
        nsCOMPtr<nsISimpleEnumerator> windowEnumerator;

        if (NS_SUCCEEDED(windowMediator->GetEnumerator(nsnull, getter_AddRefs(windowEnumerator)))) 
        {
            // Get each dom window
            PRBool more;
            windowEnumerator->HasMoreElements(&more);
            while (more) 
            {
                nsCOMPtr<nsISupports> protoWindow;
                rv = windowEnumerator->GetNext(getter_AddRefs(protoWindow));
                if (NS_SUCCEEDED(rv) && protoWindow) 
                {
                    PMDOMWindow = do_QueryInterface(protoWindow);
                    if (PMDOMWindow) 
                    {
                        hasParentWindow = PR_TRUE;
                        break;
                    }
                }
                windowEnumerator->HasMoreElements(&more);
            }
        }
    }

    if (hasParentWindow)
    {
        // Get the script global object for the window
        nsCOMPtr<nsIScriptGlobalObject> sgo;
        sgo = do_QueryInterface(PMDOMWindow);
        if (!sgo) return NS_ERROR_FAILURE;

        // Get the script context from the global context
        nsCOMPtr<nsIScriptContext> scriptContext;
        sgo->GetContext( getter_AddRefs(scriptContext));
        if (!scriptContext) return NS_ERROR_FAILURE;

        // Get the JSContext from the script context
        JSContext* jsContext = (JSContext*)scriptContext->GetNativeContext();
        if (!jsContext) return NS_ERROR_FAILURE;

    
        //-----------------------------------------------------
        // Create the nsIDialogParamBlock to pass the trigger
        // list to the dialog
        //-----------------------------------------------------
        nsCOMPtr<nsIDialogParamBlock> ioParamBlock;
        rv = nsComponentManager::CreateInstance(kDialogParamBlockCID,
                                            nsnull,
                                            NS_GET_IID(nsIDialogParamBlock),
                                            getter_AddRefs(ioParamBlock));

    
        if ( NS_SUCCEEDED( rv ) ) 
            ioParamBlock->SetInt(0,4); // standard wizard buttons

 
        void* stackPtr;
        jsval *argv = JS_PushArguments( jsContext,
                                    &stackPtr,
                                    "sss%ip",
                                    PROFILE_WIZARD_URL,
                                    "_blank",
                                    "chrome,modal",
                                    (const nsIID*)(&NS_GET_IID(nsIDialogParamBlock)),
                                    (nsISupports*)ioParamBlock);

        if (argv)
        {
            nsCOMPtr<nsIDOMWindowInternal> newWindow;
            rv = PMDOMWindow->OpenDialog(jsContext,
                                         argv,
                                         4,
                                         getter_AddRefs(newWindow));
            if (NS_SUCCEEDED(rv))
            {
                JS_PopArguments( jsContext, stackPtr);
            }
            else
                return NS_ERROR_FAILURE;
        }
        else
            return NS_ERROR_FAILURE;     

    }
    else
    {
        // No parent window is available.
        // So, Create top level window with create profile wizard
        NS_WITH_SERVICE(nsIAppShellService, wizAppShell,
                          kAppShellServiceCID, &rv);
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIURI> profURI;
        rv = NS_NewURI(getter_AddRefs(profURI), NS_LITERAL_CSTRING(PROFILE_WIZARD_URL).get());
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIXULWindow> newWindow;
        rv = wizAppShell->CreateTopLevelWindow(nsnull, profURI,
                                                PR_TRUE, PR_TRUE, CHROME_STYLE,
                                                NS_SIZETOCONTENT,           // width 
                                                NS_SIZETOCONTENT,           // height
                                                getter_AddRefs(newWindow));

        if (NS_FAILED(rv)) return rv;

        /*
         * Bring up the wizard...
         */    
        rv = wizAppShell->Run();
    }
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


NS_IMETHODIMP nsProfile::CloneProfile(const PRUnichar* newProfile)
{
    NS_ENSURE_ARG_POINTER(newProfile);   

    nsresult rv = NS_OK;

#if defined(DEBUG_profile)
    printf("ProfileManager : CloneProfile\n");
#endif
    
    nsCOMPtr<nsIFile> currProfileDir;
    rv = GetCurrentProfileDir(getter_AddRefs(currProfileDir));
    if (NS_FAILED(rv)) return rv;

    PRBool exists;    
    rv = currProfileDir->Exists(&exists);
    if (NS_SUCCEEDED(rv) && exists)
    {
        nsCOMPtr<nsIFile> aFile;
        rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILES_ROOT_DIR, getter_AddRefs(aFile));
        if (NS_FAILED(rv)) return rv;
        nsCOMPtr<nsILocalFile> destDir(do_QueryInterface(aFile, &rv));
        if (NS_FAILED(rv)) return rv;
        destDir->AppendRelativeUnicodePath(newProfile);

        // Find a unique name in the dest dir
        nsCAutoString suggestedName; suggestedName.AssignWithConversion(newProfile);
        rv = destDir->CreateUnique(suggestedName, nsIFile::DIRECTORY_TYPE, 0775);
        if (NS_FAILED(rv)) return rv;
        
        rv = RecursiveCopy(currProfileDir, destDir);
        if (NS_FAILED(rv)) return rv;           
        rv = SetProfileDir(newProfile, destDir);
    }

#if defined(DEBUG_profile_verbose)
    {
      if (NS_SUCCEEDED(rv))
      printf("ProfileManager : Cloned CurrentProfile\n");

      nsCAutoString temp; temp.AssignWithConversion(newProfile);
      printf("The new profile is ->%s<-\n", NS_STATIC_CAST(const char*, temp));
    }
#endif

    gProfileDataAccess->mNumProfiles++;
    gProfileDataAccess->mProfileDataChanged = PR_TRUE;

    return rv;
}

nsresult
nsProfile::CreateDefaultProfile(void)
{
    nsresult rv = NS_OK;

    nsFileSpec profileDirSpec;
    
    // Get the default user profiles folder
    nsCOMPtr<nsIFile> profileRootDir;
    rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILES_ROOT_DIR, getter_AddRefs(profileRootDir));
    if (NS_FAILED(rv)) return rv;
    
    nsXPIDLString profilePath;
    rv = profileRootDir->GetUnicodePath(getter_Copies(profilePath));
    if (NS_FAILED(rv)) return rv;

    nsAutoString defaultProfileName; defaultProfileName.AssignWithConversion(DEFAULT_PROFILE_NAME);
    rv = CreateNewProfile(defaultProfileName.GetUnicode(), profilePath, nsnull, PR_TRUE);

    return rv;
}

NS_IMETHODIMP 
nsProfile::UpdateRegistry(nsIFile* regName)
{
   nsresult rv = NS_OK;

   gProfileDataAccess->mProfileDataChanged = PR_TRUE;
   rv= gProfileDataAccess->UpdateRegistry(regName);

   return rv;
}

NS_IMETHODIMP 
nsProfile::SetRegStrings(const PRUnichar* profileName, 
                         const PRUnichar* regString,
                         const PRUnichar* regName,
                         const PRUnichar* regEmail,
                         const PRUnichar* regOption)
{
   nsresult rv = NS_OK;

   ProfileStruct*    aProfile;

   rv = gProfileDataAccess->GetValue(profileName, &aProfile);
   if (NS_FAILED(rv)) return rv;
   
   aProfile->NCHavePregInfo = regString;

   if (regName)    aProfile->NCProfileName   = regName;
   if (regEmail)   aProfile->NCEmailAddress  = regEmail;
   if (regOption)  aProfile->NCDeniedService = regOption;

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

/*
 * nsIDirectoryServiceProvider Implementation
 */
 
// File Name Defines

#define PREFS_FILE_50_NAME          "prefs.js"
#define USER_CHROME_DIR_50_NAME     "chrome"
#define LOCAL_STORE_FILE_50_NAME    "localstore.rdf"
#define HISTORY_FILE_50_NAME        "history.dat"
#define PANELS_FILE_50_NAME         "panels.rdf"
#define MIME_TYPES_FILE_50_NAME     "mimeTypes.rdf"
#define BOOKMARKS_FILE_50_NAME      "bookmarks.html"
#define SEARCH_FILE_50_NAME         "search.rdf" 
#define MAIL_DIR_50_NAME            "Mail"
#define IMAP_MAIL_DIR_50_NAME       "ImapMail"
#define NEWS_DIR_50_NAME            "News"
#define MSG_FOLDER_CACHE_DIR_50_NAME "panacea.dat"

NS_IMETHODIMP
nsProfile::GetFile(const char *prop, PRBool *persistant, nsIFile **_retval)
{
    nsCOMPtr<nsILocalFile>  localFile;
    nsresult rv = NS_ERROR_FAILURE;

    *_retval = nsnull;
    
    // Set persistant to TRUE - When the day comes when we switch profiles, we'll just
    // Undefine our properties. This is more efficent because these files are accessed
    // a lot more frequently than the profile will change.
    
    *persistant = PR_TRUE;

    nsIAtom* inAtom = NS_NewAtom(prop);
    NS_ENSURE_TRUE(inAtom, NS_ERROR_OUT_OF_MEMORY);

    if (inAtom == sApp_PrefsDirectory50)
    {
        rv = CloneProfileDirectorySpec(getter_AddRefs(localFile));
    }
    else if (inAtom == sApp_PreferencesFile50)
    {
        rv = CloneProfileDirectorySpec(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->Append(PREFS_FILE_50_NAME); // AppendRelativePath
    }
    else if (inAtom == sApp_UserProfileDirectory50)
    {
        rv = CloneProfileDirectorySpec(getter_AddRefs(localFile));
    }
    else if (inAtom == sApp_UserChromeDirectory)
    {
        rv = CloneProfileDirectorySpec(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->Append(USER_CHROME_DIR_50_NAME);
    }
    else if (inAtom == sApp_LocalStore50)
    {
        rv = CloneProfileDirectorySpec(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->Append(LOCAL_STORE_FILE_50_NAME);
    }
    else if (inAtom == sApp_History50)
    {
        rv = CloneProfileDirectorySpec(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->Append(HISTORY_FILE_50_NAME);
    }
    else if (inAtom == sApp_UsersPanels50)
    {
        // We do need to ensure that this file exists. If
        // not, it needs to be copied from the defaults
        // folder. There is JS code which depends on this.
        
        rv = CloneProfileDirectorySpec(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
        {
            rv = localFile->Append(PANELS_FILE_50_NAME);
            if (NS_SUCCEEDED(rv))
                rv = EnsureProfileFileExists(localFile);
        }
    }
    else if (inAtom == sApp_UsersMimeTypes50)
    {
        // Here we differ from nsFileLocator - It checks for the
        // existance of this file and if it does not exist, copies
        // it from the defaults folder to the profile folder. Since
        // WE set up any profile folder, we'll make sure it's copied then.
        
        rv = CloneProfileDirectorySpec(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->Append(MIME_TYPES_FILE_50_NAME);
    }
    else if (inAtom == sApp_BookmarksFile50)
    {
        rv = CloneProfileDirectorySpec(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->Append(BOOKMARKS_FILE_50_NAME);
    }
    else if (inAtom == sApp_SearchFile50)
    {
        // We do need to ensure that this file exists. If
        // not, it needs to be copied from the defaults
        // folder.
         
        rv = CloneProfileDirectorySpec(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
        {
            rv = localFile->Append(SEARCH_FILE_50_NAME);
            if (NS_SUCCEEDED(rv))
                rv = EnsureProfileFileExists(localFile);
        }
    }
    else if (inAtom == sApp_MailDirectory50)
    {
        rv = CloneProfileDirectorySpec(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->Append(MAIL_DIR_50_NAME);
    }
    else if (inAtom == sApp_ImapMailDirectory50)
    {
        rv = CloneProfileDirectorySpec(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->Append(IMAP_MAIL_DIR_50_NAME);
    }
    else if (inAtom == sApp_NewsDirectory50)
    {
        rv = CloneProfileDirectorySpec(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->Append(NEWS_DIR_50_NAME);
    }
    else if (inAtom == sApp_MessengerFolderCache50)
    {
        rv = CloneProfileDirectorySpec(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->Append(MSG_FOLDER_CACHE_DIR_50_NAME);
    }

    NS_RELEASE(inAtom);

    if (localFile && NS_SUCCEEDED(rv))
    	return localFile->QueryInterface(NS_GET_IID(nsIFile), (void**)_retval);
    	
    return rv;
}


/*
 * nsIProfileChangeStatus Implementation
 */

NS_IMETHODIMP nsProfile::VetoChange()
{
    mProfileChangeVetoed = PR_TRUE;
    return NS_OK;
}

nsresult nsProfile::CloneProfileDirectorySpec(nsILocalFile **aLocalFile)
{
    NS_ENSURE_ARG_POINTER(aLocalFile);
    *aLocalFile = nsnull;
    if (!mCurrentProfileAvailable)
        return NS_ERROR_FAILURE;
        
    nsresult rv;    
    nsCOMPtr<nsIFile> aFile;
    
    rv = GetCurrentProfileDir(getter_AddRefs(aFile)); // This should probably be cached...
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aFile->QueryInterface(NS_GET_IID(nsILocalFile), (void **)aLocalFile);
    NS_ENSURE_SUCCESS(rv, rv);
    
    return NS_OK;
}
