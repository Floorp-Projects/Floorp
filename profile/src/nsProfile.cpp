/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nscore.h" 
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
#include "nsIPromptService.h"
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
#include "nsISupportsPrimitives.h"
#include "nsHashtable.h"

// Interfaces Needed
#include "nsIDocShell.h"
#include "nsIWebBrowserChrome.h"

#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsIBaseWindow.h"
#include "nsIDialogParamBlock.h"
#include "nsIDOMWindowInternal.h"
#include "nsIWindowMediator.h"
#include "nsIWindowWatcher.h"

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


// A default profile name, in case automigration 4x profile fails
#define DEFAULT_PROFILE_NAME           (NS_LITERAL_STRING("default").get())

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
#define UILOCALE_CMD_LINE_ARG "-UILocale"   
#define CONTENTLOCALE_CMD_LINE_ARG "-contentLocale"   

#define PREF_CONFIRM_AUTOMIGRATION     "profile.confirm_automigration"
#define SHRIMP_PREF                    "shrimp.startup.enable"

#if defined (XP_MAC)
#define CHROME_STYLE nsIWebBrowserChrome::CHROME_WINDOW_BORDERS | nsIWebBrowserChrome::CHROME_WINDOW_CLOSE | nsIWebBrowserChrome::CHROME_CENTER_SCREEN
#else /* the rest */
#define CHROME_STYLE nsIWebBrowserChrome::CHROME_ALL | nsIWebBrowserChrome::CHROME_CENTER_SCREEN
#endif 

const char* kWindowWatcherContractID = "@mozilla.org/embedcomp/window-watcher;1";
const char* kDialogParamBlockContractID = "@mozilla.org/embedcomp/dialogparam;1";

const char* kDefaultOpenWindowParams = "centerscreen,chrome,modal,titlebar";

const char* kBrandBundleURL = "chrome://global/locale/brand.properties";
const char* kMigrationBundleURL = "chrome://communicator/locale/profile/migration.properties";

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

// Profile database to remember which profile has been
// created with UILocale and contentLocale on profileManager
static nsHashtable *gLocaleProfiles = nsnull;

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
    mStartingUp = PR_FALSE;
    mAutomigrate = PR_FALSE;
    mOutofDiskSpace = PR_FALSE;
    mDiskSpaceErrorQuitCalled = PR_FALSE;
    mCurrentProfileAvailable = PR_FALSE;

    mIsUILocaleSpecified = PR_FALSE;
    mIsContentLocaleSpecified = PR_FALSE;

    if (gInstanceCount++ == 0) {
        
        gProfileDataAccess = new nsProfileAccess();

        gLocaleProfiles = new nsHashtable();
        
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
         nsCOMPtr<nsIDirectoryService> directoryService = 
                  do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
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

      delete gLocaleProfiles;

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
nsProfile::StartupWithArgs(nsICmdLineService *cmdLineArgs, PRBool canInteract)
{
    nsresult rv;

    struct ScopeFlag
    {
        ScopeFlag(PRBool *flagPtr) : mFlagPtr(flagPtr)
        { *mFlagPtr = PR_TRUE; }

        ~ScopeFlag()
        { *mFlagPtr = PR_FALSE; }

        PRBool *mFlagPtr;
    };

    // initializations for profile manager
    PRBool profileDirSet = PR_FALSE;
    nsCString profileURLStr("");

#ifdef DEBUG_profile_verbose
    printf("Profile Manager : Profile Wizard and Manager activites : Begin\n");
#endif

    ScopeFlag startupFlag(&mStartingUp);

    if (cmdLineArgs)
        rv = ProcessArgs(cmdLineArgs, canInteract, &profileDirSet, profileURLStr);

    // This boolean is set only when an automigrated user runs out of disk space
    // and chooses to cancel further operations from the dialogs presented...
    if (mDiskSpaceErrorQuitCalled)
        return NS_ERROR_FAILURE;

    if (!profileDirSet) {
        rv = LoadDefaultProfileDir(profileURLStr, canInteract);

        if (NS_FAILED(rv)) return rv;
    }

    // Ensure that by this point we have a current profile.
    // If -CreateProfile was used, we won't, and we're supposed to exit.
    nsXPIDLString currentProfileStr;
    rv = GetCurrentProfile(getter_Copies(currentProfileStr));
    if (NS_FAILED(rv) || (*(const PRUnichar*)currentProfileStr == 0)) {
        return NS_ERROR_ABORT;
    }


    // check UILocale is specified on profileManager, otherwise,
    // when -UILocale option is specified, install the UILocale

    // -UILocale or -contentLocale is not specified
    if (mIsUILocaleSpecified == PR_FALSE && mIsContentLocaleSpecified == PR_FALSE) {
        return NS_OK;
    }

    nsCOMPtr<nsIFile> profileDir;

    rv = GetCurrentProfileDir(getter_AddRefs(profileDir));
    if (NS_FAILED(rv)) return rv;

    nsXPIDLCString  pathBuf;
    rv = profileDir->GetPath(getter_Copies(pathBuf));
    if (NS_FAILED(rv)) return rv;

    // -UILocale or -contentLocale has been specified, but
    // user has selected UILocale and contentLocale for this profile
    // on profileManager
    // We should not set here
    nsCStringKey key((const char *)pathBuf);
    if (NS_PTR_TO_INT32(gLocaleProfiles->Get(&key)) == PR_TRUE) {
#ifdef DEBUG_profile_verbose
        printf(" already set UILocale and contentLocale: %s\n", NS_STATIC_CAST(const char*, pathBuf));
        printf(" will not install locale\n");
#endif
        return NS_OK;
    }
    gLocaleProfiles->Remove(&key);

    nsCOMPtr<nsIChromeRegistry> chromeRegistry = do_GetService(kChromeRegistryCID, &rv);
    if (NS_FAILED(rv)) return rv;

    // Install to the profile
    nsFileSpec fileSpec((const char *)pathBuf);
    nsFileURL  fileURL(fileSpec);
    const char* fileStr = fileURL.GetURLString();

    // NEED TO FIX: when current UILocale and contentLocale are same with specified locales,
    // we shouldn't install them again here. But chromeRegistry->GetSelectedLocale() doesn't
    // work here properly. It always returns global and global-region of default not current
    // profile
    const PRUnichar* uilocale = mUILocaleName.get() ;
    if (uilocale && uilocale[0]) {
#ifdef DEBUG_profile_verbose
        nsCAutoString temp1; temp1.AssignWithConversion(uilocale);
        printf(" install new UILocaleName: %s\n", NS_STATIC_CAST(const char*, temp1));
#endif
        rv = chromeRegistry->SelectLocaleForProfile(uilocale,
                                          NS_ConvertUTF8toUCS2(fileStr).get());
        if (NS_FAILED(rv)) return rv;
    }

    const PRUnichar* contentlocale = mContentLocaleName.get() ;
    if (contentlocale && contentlocale[0]) {
#ifdef DEBUG_profile_verbose
        nsCAutoString temp2; temp2.AssignWithConversion(contentlocale);
        printf(" install new mContentLocaleName: %s\n", NS_STATIC_CAST(const char*, temp2));
#endif
        rv = chromeRegistry->SelectLocaleForProfile(contentlocale,
                                          NS_ConvertUTF8toUCS2(fileStr).get());
        if (NS_FAILED(rv)) return rv;
    }

#ifdef DEBUG_profile_verbose
    printf("Profile Manager : Profile Wizard and Manager activites : End\n");
#endif

    return NS_OK;
}

NS_IMETHODIMP
nsProfile::GetIsStartingUp(PRBool *aIsStartingUp)
{
    NS_ENSURE_ARG_POINTER(aIsStartingUp);
    *aIsStartingUp = mStartingUp;
    return NS_OK;
}

nsresult
nsProfile::LoadDefaultProfileDir(nsCString & profileURLStr, PRBool canInteract)
{
    nsresult rv;
    nsCOMPtr<nsIURI> profileURL;
    PRInt32 numProfiles=0;
    nsXPIDLString currentProfileStr;
  
    nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &rv));
    if (NS_FAILED(rv)) return rv;

    GetProfileCount(&numProfiles);

    if (profileURLStr.Length() == 0)
    {
        // This means that there was no command-line argument to force
        // profile UI to come up. But we need the UI anyway if there
        // are no profiles yet, or if there is more than one.
        if (numProfiles == 0)
        {
            rv = CreateDefaultProfile();
            if (NS_FAILED(rv)) return rv;
            // Will get set in call to SetCurrentProfile() below
        }
        else if (numProfiles == 1)
        {
            // Make sure the profile dir exists. If not, we need the UI
            nsCOMPtr<nsIFile> curProfileDir;
            PRBool exists;
            
            rv = GetCurrentProfileDir(getter_AddRefs(curProfileDir));
            if (NS_SUCCEEDED(rv))
                rv = curProfileDir->Exists(&exists);
            if (NS_FAILED(rv) || !exists)
                profileURLStr = PROFILE_MANAGER_URL; 
        }
        else
            profileURLStr = PROFILE_SELECTION_URL;
    }

    if (profileURLStr.Length() != 0)
    {
        if (!canInteract) return NS_ERROR_PROFILE_REQUIRES_INTERACTION;

        nsCOMPtr<nsIWindowWatcher> windowWatcher(do_GetService(kWindowWatcherContractID, &rv));
        if (NS_FAILED(rv)) return rv;
 
        // We need to send a param to OpenWindow if the window is to be considered
        // a dialog. It needs to be for script security reasons. This param block
        // will be made use of soon. See bug 66833.
        nsCOMPtr<nsIDialogParamBlock> ioParamBlock(do_CreateInstance("@mozilla.org/embedcomp/dialogparam;1", &rv));
        if (NS_FAILED(rv)) return rv;
       
        nsCOMPtr<nsIDOMWindow> newWindow;
        rv = windowWatcher->OpenWindow(nsnull,
                                       profileURLStr.get(),
                                       "_blank",
                                       kDefaultOpenWindowParams,
                                       ioParamBlock,
                                       getter_AddRefs(newWindow));
        if (NS_FAILED(rv)) return rv;
    }

    // if we get here, and we don't have a current profile, 
    // return a failure so we will exit
    // this can happen, if the user hits Exit in the profile manager dialog
    rv = GetCurrentProfile(getter_Copies(currentProfileStr));
    if (NS_FAILED(rv) || (*(const PRUnichar*)currentProfileStr == 0)) {
        return NS_ERROR_FAILURE;
    }

    // if at this point we have a current profile but it is not set, set it
    if (!mCurrentProfileAvailable) {
        rv = SetCurrentProfile(currentProfileStr);
        if (NS_FAILED(rv)) return rv;
    }

    nsCOMPtr<nsICategoryManager> catman = 
             do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);

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
    
    return NS_OK;
}

nsresult
nsProfile::ConfirmAutoMigration(PRBool canInteract, PRBool *confirmed)
{
    NS_ENSURE_ARG_POINTER(confirmed);
    *confirmed = PR_FALSE;
    nsresult rv;
    
    // First check PREF_CONFIRM_AUTOMIGRATION.
    // If FALSE, we go ahead and migrate without asking.
    PRBool confirmAutomigration = PR_TRUE;
    nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &rv));
    if (NS_SUCCEEDED(rv))
        (void)prefs->GetBoolPref(PREF_CONFIRM_AUTOMIGRATION, &confirmAutomigration);
    if (!confirmAutomigration) {
        *confirmed = PR_TRUE;
        return NS_OK;
    }
    
    // If allowed, put up a confirm dialog and ask the user
    if (!canInteract)
        return NS_ERROR_PROFILE_REQUIRES_INTERACTION;

    nsCOMPtr<nsIStringBundleService> stringBundleService(do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIStringBundle> migrationBundle, brandBundle;
    rv = stringBundleService->CreateBundle(kMigrationBundleURL, getter_AddRefs(migrationBundle));
    if (NS_FAILED(rv)) return rv;
    rv = stringBundleService->CreateBundle(kBrandBundleURL, getter_AddRefs(brandBundle));
    if (NS_FAILED(rv)) return rv;
    
    nsXPIDLString brandName;
    rv = brandBundle->GetStringFromName(NS_LITERAL_STRING("brandShortName").get(), getter_Copies(brandName));
    if (NS_FAILED(rv)) return rv;

    nsXPIDLString msgString, dialogTitle, button0Title, button1Title;
    const PRUnichar *formatStrings[] = { brandName.get(), brandName.get() };
    rv = migrationBundle->FormatStringFromName(NS_LITERAL_STRING("confirmMigration").get(),
                                                 formatStrings, 2, getter_Copies(msgString));
    if (NS_FAILED(rv)) return rv;
    
    rv = migrationBundle->GetStringFromName(NS_LITERAL_STRING("dialogTitle").get(), getter_Copies(dialogTitle));
    if (NS_FAILED(rv)) return rv;
    rv = migrationBundle->GetStringFromName(NS_LITERAL_STRING("migrate").get(), getter_Copies(button0Title));
    if (NS_FAILED(rv)) return rv;
    rv = migrationBundle->GetStringFromName(NS_LITERAL_STRING("manage").get(), getter_Copies(button1Title));
    if (NS_FAILED(rv)) return rv;
    
    nsCOMPtr<nsIPromptService> promptService(do_GetService("@mozilla.org/embedcomp/prompt-service;1", &rv));
    if (NS_FAILED(rv)) return rv;
    PRInt32 buttonPressed;
    rv = promptService->ConfirmEx(nsnull, dialogTitle.get(), msgString.get(),
                                  (nsIPromptService::BUTTON_POS_0 * nsIPromptService::BUTTON_TITLE_IS_STRING) +
                                  (nsIPromptService::BUTTON_POS_1 * nsIPromptService::BUTTON_TITLE_IS_STRING),
                                  button0Title, button1Title, nsnull,
                                  nsnull, nsnull, &buttonPressed);
    if (NS_FAILED(rv)) return rv;
    *confirmed = (buttonPressed == 0);
    return NS_OK;
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
                       PRBool canInteract,
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
    // -UILocale command
    rv = cmdLineArgs->GetCmdLineValue(UILOCALE_CMD_LINE_ARG, getter_Copies(cmdResult));
    if (NS_SUCCEEDED(rv))
    {
        if (cmdResult) {
	    mIsUILocaleSpecified = PR_TRUE;
            mUILocaleName.AssignWithConversion(cmdResult);
        }
    }

    // -contentLocale command
    rv = cmdLineArgs->GetCmdLineValue(CONTENTLOCALE_CMD_LINE_ARG, getter_Copies(cmdResult));
    if (NS_SUCCEEDED(rv))
    {
        if (cmdResult) {
            mIsContentLocaleSpecified = PR_TRUE;
            mContentLocaleName.AssignWithConversion(cmdResult);
        }
    }

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
            rv = ProfileExists(currProfileName.get(), &exists);
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
                rv = SetCurrentProfile(currProfileName.get());
                if (NS_SUCCEEDED(rv))
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

    rv = cmdLineArgs->GetCmdLineValue(CREATE_PROFILE_CMD_LINE_ARG, getter_Copies(cmdResult));
    if (NS_SUCCEEDED(rv))
    {
        if (cmdResult) {

#ifdef DEBUG_profile_verbose
            printf("profileName & profileDir are: %s\n", (const char*)cmdResult);
#endif

			foundProfileCommandArg = PR_TRUE;
            nsAutoString currProfileName; currProfileName.AssignWithConversion(strtok(NS_CONST_CAST(char*,(const char*)cmdResult), " "));
            nsAutoString currProfileDirString; currProfileDirString.AssignWithConversion(strtok(NULL, " "));
        
            if (!currProfileDirString.IsEmpty()) {
                rv = NS_NewUnicodeLocalFile(currProfileDirString.get(), PR_TRUE, getter_AddRefs(currProfileDir));
                NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
            }
            else {
                // No directory name provided. Place it in
                // NS_APP_USER_PROFILES_ROOT_DIR
                nsCOMPtr<nsIProperties> directoryService(do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv));
                if (NS_FAILED(rv)) return rv;
                rv = directoryService->Get(NS_APP_USER_PROFILES_ROOT_DIR, NS_GET_IID(nsILocalFile), getter_AddRefs(currProfileDir));
                if (NS_FAILED(rv)) return rv;
            }

            nsXPIDLString currProfilePath;
            currProfileDir->GetUnicodePath(getter_Copies(currProfilePath));
            rv = CreateNewProfile(currProfileName.get(), currProfilePath, nsnull, PR_TRUE);
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
                PRBool confirmed = PR_FALSE;
                if (NS_SUCCEEDED(ConfirmAutoMigration(canInteract, &confirmed)) && confirmed)
                    AutoMigrate();
                else
                    profileURLStr = PROFILE_MANAGER_URL;
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
    NS_ENSURE_ARG(profileName);   
    NS_ENSURE_ARG_POINTER(profileDir);
    *profileDir = nsnull;

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
    rv = aProfile->GetResolvedProfileDir(getter_AddRefs(aProfileDir));
    if (NS_SUCCEEDED(rv) && aProfileDir)
    {
#ifdef XP_MAC
        PRBool exists;
        rv = aProfileDir->Exists(&exists);
        if (NS_FAILED(rv)) return rv;
        if (exists) {
            PRBool inTrash;
            nsCOMPtr<nsIFile> trashFolder;
                            
            rv = NS_GetSpecialDirectory(NS_MAC_TRASH_DIR, getter_AddRefs(trashFolder));
            if (NS_FAILED(rv)) return rv;
            rv = trashFolder->Contains(aProfileDir, PR_TRUE, &inTrash);
            if (NS_FAILED(rv)) return rv;
            if (inTrash) {
                aProfileDir = nsnull;
                rv = NS_ERROR_FILE_NOT_FOUND;
            }  
        }
#endif
        *profileDir = aProfileDir;
        NS_IF_ADDREF(*profileDir);
    }    
	delete aProfile;
    return rv;
}

NS_IMETHODIMP nsProfile::GetProfilePath(const PRUnichar *profileName, PRUnichar **_retval)
{
    NS_ENSURE_ARG(profileName);
    NS_ENSURE_ARG_POINTER(_retval);
    *_retval = nsnull;
    
    nsCOMPtr<nsIFile> profileDir;
    nsresult rv = GetProfileDir(profileName, getter_AddRefs(profileDir));
    if (NS_FAILED(rv)) return rv;
    
    PRBool isSalted;    
    nsCOMPtr<nsIFile> prettyDir(profileDir);
    rv = IsProfileDirSalted(profileDir, &isSalted);
    if (NS_SUCCEEDED(rv) && isSalted) {
        nsCOMPtr<nsIFile> parentDir;
        rv = profileDir->GetParent(getter_AddRefs(parentDir));
        if (NS_SUCCEEDED(rv))
            prettyDir = parentDir;
    }
    return prettyDir->GetUnicodePath(_retval);
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
    *profileName = nsnull;

    gProfileDataAccess->GetCurrentProfile(profileName);
    return (*profileName == nsnull) ? NS_ERROR_FAILURE : NS_OK;
}


NS_IMETHODIMP
nsProfile::SetCurrentProfile(const PRUnichar * aCurrentProfile)
{
    NS_ENSURE_ARG(aCurrentProfile);
    
    nsresult rv;
    nsCOMPtr<nsIFile> profileDir;
    PRBool exists;

    // Ensure that the profile exists and its directory too.
    rv = GetProfileDir(aCurrentProfile, getter_AddRefs(profileDir));
    if (NS_FAILED(rv)) return rv;
    rv = profileDir->Exists(&exists);
    if (NS_FAILED(rv)) return rv;
    if (!exists) return NS_ERROR_FILE_NOT_FOUND;

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
    
    nsCOMPtr<nsIObserverService> observerService = 
             do_GetService("@mozilla.org/observer-service;1", &rv);
    NS_ENSURE_TRUE(observerService, NS_ERROR_FAILURE);
    
    nsISupports *subject = (nsISupports *)((nsIProfile *)this);
    NS_NAMED_LITERAL_STRING(switchString, "switch");
    NS_NAMED_LITERAL_STRING(startupString, "startup");
    const nsAFlatString& context = isSwitch ? switchString : startupString;

    if (isSwitch)
    {
        // Phase 1: See if anybody objects to the profile being changed.
        mProfileChangeVetoed = PR_FALSE;        
        observerService->NotifyObservers(subject, "profile-approve-change", context.get());
        if (mProfileChangeVetoed)
            return NS_OK;

        // Phase 2: Send the "teardown" notification
        observerService->NotifyObservers(subject, "profile-change-teardown", context.get());
        
        // Phase 3: Notify observers of a profile change
        observerService->NotifyObservers(subject, "profile-before-change", context.get());        
    }

    // Flush the stringbundle cache
    nsCOMPtr<nsIStringBundleService> bundleService = 
             do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = bundleService->FlushBundles();
        NS_ASSERTION(NS_SUCCEEDED(rv), "failed to flush bundle cache");
    }
    
    // Do the profile switch    
    gProfileDataAccess->SetCurrentProfile(aCurrentProfile);
    gProfileDataAccess->mProfileDataChanged = PR_TRUE;
    gProfileDataAccess->UpdateRegistry(nsnull);
            
    if (NS_FAILED(rv)) return rv;
    mCurrentProfileAvailable = PR_TRUE;
    
    if (isSwitch)
    {
        rv = UndefineFileLocations();
        NS_ASSERTION(NS_SUCCEEDED(rv), "Could not undefine file locations");
    }

    // Phase 4: Notify observers that the profile has changed - Here they respond to new profile
    observerService->NotifyObservers(subject, "profile-do-change", context.get());

    // Phase 5: Now observers can respond to something another observer did in phase 4
    observerService->NotifyObservers(subject, "profile-after-change", context.get());
      
    // Now that a profile is established, set the profile defaults dir for the locale of this profile
    rv = DefineLocaleDefaultsDir();
    NS_ASSERTION(NS_SUCCEEDED(rv), "nsProfile::DefineLocaleDefaultsDir failed");
      
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

// Performs a "logout" by shutting down the current profile
NS_IMETHODIMP nsProfile::ShutDownCurrentProfile(PRUint32 shutDownType)
{
    nsresult rv;
    
    nsCOMPtr<nsIObserverService> observerService = 
             do_GetService("@mozilla.org/observer-service;1", &rv);
    NS_ENSURE_TRUE(observerService, NS_ERROR_FAILURE);
    
    nsISupports *subject = (nsISupports *)((nsIProfile *)this);

    NS_NAMED_LITERAL_STRING(cleanseString, "shutdown-cleanse");
    NS_NAMED_LITERAL_STRING(persistString, "shutdown-persist");
    const nsAFlatString& context = (shutDownType == SHUTDOWN_CLEANSE) ? cleanseString : persistString;
           
    // Phase 1: See if anybody objects to the profile being changed.
    mProfileChangeVetoed = PR_FALSE;        
    observerService->NotifyObservers(subject, "profile-approve-change", context.get());
    if (mProfileChangeVetoed)
        return NS_OK;

    // Phase 2: Send the "teardown" notification
    observerService->NotifyObservers(subject, "profile-change-teardown", context.get());
    
    // Phase 3: Notify observers of a profile change
    observerService->NotifyObservers(subject, "profile-before-change", context.get());        

    rv = UndefineFileLocations();
    NS_ASSERTION(NS_SUCCEEDED(rv), "Could not undefine file locations");
    mCurrentProfileAvailable = PR_FALSE;
    
    return NS_OK;
}


#define SALT_SIZE 8
#define TABLE_SIZE 36

NS_NAMED_LITERAL_CSTRING(kSaltExtensionCString, ".slt");

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
		  if (length == (SALT_SIZE + kSaltExtensionCString.Length())) {
			// check that the filename ends with ".slt"
			if (nsCRT::strncmp((const char *)leafName + SALT_SIZE, kSaltExtensionCString.get(), kSaltExtensionCString.Length()) == 0) {
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
  saltStr.Append(kSaltExtensionCString);
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

nsresult nsProfile::IsProfileDirSalted(nsIFile *profileDir, PRBool *isSalted)
{
    nsresult rv;
    NS_ENSURE_ARG_POINTER(isSalted);
    *isSalted = PR_FALSE;
    
    // 1. The name of the profile dir has to end in ".slt"
    nsXPIDLCString leafName;
    rv = profileDir->GetLeafName(getter_Copies(leafName));
    if (NS_FAILED(rv)) return rv;

    PRBool endsWithSalt = PR_FALSE;    
    nsDependentCString leafNameString(leafName.get());
    if (leafNameString.Length() >= kSaltExtensionCString.Length())
    {
        nsReadingIterator<char> stringEnd;
        leafNameString.EndReading(stringEnd);

        nsReadingIterator<char> stringStart = stringEnd;
        stringStart.advance( -(NS_STATIC_CAST(PRInt32, kSaltExtensionCString.Length())) );

        endsWithSalt = kSaltExtensionCString.Equals(Substring(stringStart, stringEnd));
    }
    if (!endsWithSalt)
        return NS_OK;
    
    // 2. The profile dir has to be its parent's only child.    
    nsCOMPtr<nsIFile> parentDir;
    rv = profileDir->GetParent(getter_AddRefs(parentDir));
    if (NS_FAILED(rv)) return rv;

    PRBool hasMore;
    nsCOMPtr<nsISimpleEnumerator> dirIterator;
    rv = parentDir->GetDirectoryEntries(getter_AddRefs(dirIterator));
    if (NS_FAILED(rv)) return rv;
    
    PRInt32 numChildren = 0;
    rv = dirIterator->HasMoreElements(&hasMore);
    
    while (NS_SUCCEEDED(rv) && hasMore && numChildren <= 1) {
        nsCOMPtr<nsIFile> child;
        rv = dirIterator->GetNext((nsISupports**)getter_AddRefs(child));    
        if (NS_SUCCEEDED(rv))
            ++numChildren;
        rv = dirIterator->HasMoreElements(&hasMore);
    }
    if (NS_SUCCEEDED(rv) && numChildren == 1)
        *isSalted = PR_TRUE;
    
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

// Creates a new profile with UILocale and contentLocale
NS_IMETHODIMP 
nsProfile::CreateNewProfileWithLocales(const PRUnichar* profileName, 
                            const PRUnichar* nativeProfileDir,
                            const PRUnichar* aUILocale,
                            const PRUnichar* aContentLocale,
                            PRBool useExistingDir)
{
    NS_ENSURE_ARG_POINTER(profileName);   

    nsresult rv = NS_OK;

#if defined(DEBUG_profile)
    {
      printf("ProfileManager : CreateNewProfileWithLocales\n");

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

    // Set the directory value and add the entry to the registry tree.
    rv = SetProfileDir(profileName, profileDir);

#if defined(DEBUG_profile_verbose)
    printf("after SetProfileDir\n");
#endif

    // Get profile defaults folder..
    nsCOMPtr <nsIFile> profDefaultsDir;
    rv = NS_GetSpecialDirectory(NS_APP_PROFILE_DEFAULTS_NLOC_50_DIR, getter_AddRefs(profDefaultsDir));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIChromeRegistry> chromeRegistry = do_GetService(kChromeRegistryCID, &rv);
    if (NS_SUCCEEDED(rv)) {

        const PRUnichar* uiLocale = aUILocale;
        const PRUnichar* contentLocale = aContentLocale;
        nsXPIDLString currentUILocaleName;
        nsXPIDLString currentContentLocaleName;

        // When aUILocale == null or aContentLocale == null, set those from default values
        // which are from default or from command line options

        // This fallback is for CreateNewProfile() of CreateDefaultProfile() and ProcessArgs()
        // Those functions call CreateNewProfile(locale=null). We should consider default values
        // or specified values of locales for CreateDefaultProfile() and ProcessArgs().

        // We can get preferred UILocale and contentLocale (specified -UILocale and -contentLocale)
        // by GetSelectedLocale() which is done in nsAppRunner.cpp::InstallGlobalLocale()

        if (!aUILocale || !aUILocale[0]) {
            rv = chromeRegistry->GetSelectedLocale(NS_LITERAL_STRING("global").get(),
                                           getter_Copies(currentUILocaleName));
            if (NS_SUCCEEDED(rv)) {
                uiLocale = currentUILocaleName.get();
            }
        }

        if (!aContentLocale || !aContentLocale[0]) {
            rv = chromeRegistry->GetSelectedLocale(NS_LITERAL_STRING("global-region").get(),
                                           getter_Copies(currentContentLocaleName));
            if (NS_SUCCEEDED(rv)) {
                contentLocale = currentContentLocaleName.get();
            }
        }

#if defined(DEBUG_profile_verbose)
        nsCAutoString temp1; temp1.AssignWithConversion(uiLocale);
        printf(" uiLocale=%s\n", NS_STATIC_CAST(const char*, temp1));

        nsCAutoString temp2; temp2.AssignWithConversion(contentLocale);
        printf(" contentLocale=%s\n", NS_STATIC_CAST(const char*, temp2));
#endif

        nsXPIDLCString  pathBuf;
        rv = profileDir->GetPath(getter_Copies(pathBuf));
        NS_ENSURE_SUCCESS(rv, rv);
        nsFileSpec fileSpec((const char *)pathBuf);
        nsFileURL  fileURL(fileSpec);
        const char* fileStr = fileURL.GetURLString();

        if (uiLocale && uiLocale[0]) {
            rv = chromeRegistry->SelectLocaleForProfile(uiLocale, 
                                                        NS_ConvertUTF8toUCS2(fileStr).get());
            // Remember which profile has been created with the UILocale
            // didn't use gProfileDataAccess because just needed one time
            if (NS_SUCCEEDED(rv)) {
                nsCStringKey key((const char *)pathBuf);
                gLocaleProfiles->Put(&key, (void*)PR_TRUE);
            }
        }

        if (contentLocale && contentLocale[0]) {
            // caller prefers locale subdir
            nsCOMPtr<nsIFile> locProfDefaultsDir;
            rv = profDefaultsDir->Clone(getter_AddRefs(locProfDefaultsDir));
            if (NS_FAILED(rv)) return rv;
        
            locProfDefaultsDir->AppendUnicode(contentLocale);
            rv = locProfDefaultsDir->Exists(&exists);
            if (NS_SUCCEEDED(rv) && exists) {
                profDefaultsDir = locProfDefaultsDir; // transfers ownership
#if defined(DEBUG_profile_verbose)
                nsXPIDLString profilePath;
                rv = profDefaultsDir->GetUnicodePath(getter_Copies(profilePath));
                if (NS_SUCCEEDED(rv)) {
                    nsCAutoString temp5; temp5.AssignWithConversion(profilePath);
                    printf(" profDefaultsDir is set to: %s\n", NS_STATIC_CAST(const char*, temp5));
                }
#endif
            }

            rv = chromeRegistry->SelectLocaleForProfile(contentLocale, 
                                                        NS_ConvertUTF8toUCS2(fileStr).get());
            // Remember which profile has been created with the UILocale
            // didn't use gProfileDataAccess because just needed one time
            if (NS_SUCCEEDED(rv)) {
                nsCStringKey key((const char *)pathBuf);
                gLocaleProfiles->Put(&key, (void*)PR_TRUE);
            }
        }
    }

    // Copy contents from defaults folder.
    rv = profDefaultsDir->Exists(&exists);
    if (NS_SUCCEEDED(rv) && exists && (!useExistingDir))
    {
        RecursiveCopy(profDefaultsDir, profileDir);
    }

    gProfileDataAccess->mProfileDataChanged = PR_TRUE;
    gProfileDataAccess->UpdateRegistry(nsnull);
    
    return NS_OK;
}	


// Rename a old profile to new profile.
// Copies all the keys from old profile to new profile.

// Creates a new profile
NS_IMETHODIMP 
nsProfile::CreateNewProfile(const PRUnichar* profileName, 
                            const PRUnichar* nativeProfileDir,
                            const PRUnichar* langcode,
                            PRBool useExistingDir)
{
    return CreateNewProfileWithLocales(profileName,nativeProfileDir,langcode,nsnull,useExistingDir);
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
    mCurrentProfileAvailable = PR_FALSE;
    
    return rv;
}

// Delete a profile from the registry
// Not deleting the directories on the harddisk yet.
// 4.x kind of confirmation need to be implemented yet
NS_IMETHODIMP nsProfile::DeleteProfile(const PRUnichar* profileName, PRBool canDeleteFiles)
{
    NS_ENSURE_ARG_POINTER(profileName);   

    nsresult rv;
 
    nsXPIDLString currProfile;
    rv = GetCurrentProfile(getter_Copies(currProfile));
    if (NS_SUCCEEDED(rv) && !nsCRT::strcmp(profileName, currProfile)) {
        rv = ForgetCurrentProfile();
        if (NS_FAILED(rv)) return rv;
    }
    rv = NS_OK;
    
    // If user asks for it, delete profile directory
    if (canDeleteFiles) {
    
        nsCOMPtr<nsIFile> profileDir;
        rv = GetProfileDir(profileName, getter_AddRefs(profileDir));
        if (NS_FAILED(rv)) return rv;
        
        PRBool exists;
        rv = profileDir->Exists(&exists);
        if (NS_FAILED(rv)) return rv;
        
        if (exists) {

            // The profile dir may be located inside a salted dir.
            // If so, according to IsProfileDirSalted,
            // delete the parent dir as well.
            
            nsCOMPtr<nsIFile> dirToDelete(profileDir);
            PRBool isSalted;
            rv = IsProfileDirSalted(profileDir, &isSalted);
            if (NS_SUCCEEDED(rv) && isSalted) {
                nsCOMPtr<nsIFile> parentDir;
                rv = profileDir->GetParent(getter_AddRefs(parentDir));
                if (NS_SUCCEEDED(rv))
                    dirToDelete = parentDir;
            }
            rv = dirToDelete->Remove(PR_TRUE);
        }
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

    rv = SetCurrentProfile(profileName);
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to set profile");

    return rv;
}

nsresult nsProfile::LoadNewProfilePrefs()
{
    nsresult rv;
    nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &rv));
    if (NS_FAILED(rv)) return rv;
    
    prefs->ResetUserPrefs();
    prefs->ReadUserPrefs(nsnull);

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

nsresult
nsProfile::DefineLocaleDefaultsDir()
{
    nsresult rv;
    
    nsCOMPtr<nsIProperties> directoryService = 
             do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
    NS_ENSURE_TRUE(directoryService, NS_ERROR_FAILURE);    

    nsCOMPtr<nsIFile> localeDefaults;
    rv = directoryService->Get(NS_APP_PROFILE_DEFAULTS_NLOC_50_DIR, NS_GET_IID(nsIFile), getter_AddRefs(localeDefaults));
    if (NS_SUCCEEDED(rv))
    {
        nsCOMPtr<nsIChromeRegistry> chromeRegistry = 
                 do_GetService(kChromeRegistryCID, &rv);
        if (NS_SUCCEEDED(rv))
        {
            nsXPIDLString localeName;
            rv = chromeRegistry->GetSelectedLocale(NS_LITERAL_STRING("global-region").get(), getter_Copies(localeName));
            if (NS_SUCCEEDED(rv))
                rv = localeDefaults->AppendUnicode(localeName);
        }
        (void) directoryService->Undefine(NS_APP_PROFILE_DEFAULTS_50_DIR);
        rv = directoryService->Define(NS_APP_PROFILE_DEFAULTS_50_DIR, localeDefaults);
    }
    return rv;
}

nsresult nsProfile::UndefineFileLocations()
{
    nsresult rv;
    
    nsCOMPtr<nsIProperties> directoryService = 
             do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
    NS_ENSURE_TRUE(directoryService, NS_ERROR_FAILURE);

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

    return NS_OK;
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

    // No longer copying the default 5.0 profile files into
    // the migrated profile. Check for them as requested.
	
    rv = SetProfileDir(profileName, newProfDir);
    if (NS_FAILED(rv)) return rv;

    gProfileDataAccess->mProfileDataChanged = PR_TRUE;
    gProfileDataAccess->UpdateRegistry(nsnull);

    return rv;
}

nsresult
nsProfile::ShowProfileWizard(void)
{
    nsresult rv;
    nsCOMPtr<nsIWindowWatcher> windowWatcher(do_GetService(kWindowWatcherContractID, &rv));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIDialogParamBlock> ioParamBlock(do_CreateInstance("@mozilla.org/embedcomp/dialogparam;1", &rv));
    if (NS_FAILED(rv)) return rv;
    ioParamBlock->SetInt(0,4); // standard wizard buttons
   
    nsCOMPtr<nsIDOMWindow> newWindow;
    rv = windowWatcher->OpenWindow(nsnull,
                                   PROFILE_WIZARD_URL,
                                   "_blank",
                                   kDefaultOpenWindowParams,
                                   ioParamBlock,
                                   getter_AddRefs(newWindow));
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
    nsresult rv;

    PRUint32    numOldProfiles = 0;
    PRUnichar   **nameArray = nsnull;
    rv = GetProfileListX(nsIProfileInternal::LIST_ONLY_OLD, &numOldProfiles, &nameArray);
    if (NS_FAILED(rv)) return rv;
    for (PRUint32 i = 0; i < numOldProfiles; i++)
    {
        rv = MigrateProfile(nameArray[i], PR_FALSE /* don't show progress as modal window */);
        if (NS_FAILED(rv)) break;
    }
    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(numOldProfiles, nameArray);
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

    rv = CreateNewProfile(DEFAULT_PROFILE_NAME, profilePath, nsnull, PR_TRUE);

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
        // Ensure that this file exists. If not, it
        // needs to be copied from the defaults.

        rv = CloneProfileDirectorySpec(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
        {
            rv = localFile->Append(LOCAL_STORE_FILE_50_NAME);
            if (NS_SUCCEEDED(rv))
                rv = EnsureProfileFileExists(localFile);
        }
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
        // folder. There is code which depends on this.
        
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
        // Ensure that this file exists. If not, it
        // needs to be copied from the defaults.
        
        rv = CloneProfileDirectorySpec(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
        {
            rv = localFile->Append(MIME_TYPES_FILE_50_NAME);
            if (NS_SUCCEEDED(rv))
                rv = EnsureProfileFileExists(localFile);
        }
    }
    else if (inAtom == sApp_BookmarksFile50)
    {
        // Ensure that this file exists. If not, it
        // needs to be copied from the defaults.

        rv = CloneProfileDirectorySpec(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
        {
            rv = localFile->Append(BOOKMARKS_FILE_50_NAME);
            if (NS_SUCCEEDED(rv))
                rv = EnsureProfileFileExists(localFile);
        }
    }
    else if (inAtom == sApp_SearchFile50)
    {
        // We do need to ensure that this file exists. If
        // not, it needs to be copied from the defaults
        // folder. There is code which depends on this.
         
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
