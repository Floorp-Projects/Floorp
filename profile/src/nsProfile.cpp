/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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

#include "nscore.h" 
#include "nsProfile.h"
#ifdef MOZ_PROFILELOCKING
#include "nsProfileLock.h"
#endif
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"

#include "pratom.h"
#include "prmem.h"
#include "plstr.h"
#include "prenv.h"

#include "nsIFactory.h"
#include "nsIComponentManager.h"
#include "nsIEnumerator.h"
#include "nsXPIDLString.h"
#include "nsEscape.h"
#include "nsIURL.h"

#include "prprf.h"

#include "nsIIOService.h"
#include "nsNetUtil.h"
#include "nsPrefMigration.h"
#include "nsIPrefMigration.h"
#include "nsPrefMigrationCIDs.h"
#include "nsFileStream.h"
#include "nsIPromptService.h"
#include "nsIStreamListener.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsICookieService.h"
#include "nsICategoryManager.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIChromeRegistry.h" // chromeReg
#include "nsIStringBundle.h"
#include "nsIObserverService.h"
#include "nsHashtable.h"
#include "nsIAtom.h"
#include "nsProfileDirServiceProvider.h"
#include "nsISessionRoaming.h"

// Interfaces Needed
#include "nsIDocShell.h"
#include "nsIWebBrowserChrome.h"

#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsIBaseWindow.h"
#include "nsIDialogParamBlock.h"
#include "nsIDOMWindowInternal.h"
#include "nsIWindowWatcher.h"
#include "jsapi.h"
#include "nsIJSContextStack.h"


#if defined(XP_MAC) || defined(XP_MACOSX)
#define OLD_REGISTRY_FILE_NAME "Netscape Registry"
#elif defined(XP_WIN) || defined(XP_OS2)
#define OLD_REGISTRY_FILE_NAME "nsreg.dat"
#endif


// A default profile name, in case automigration 4x profile fails
#define DEFAULT_PROFILE_NAME           (NS_LITERAL_STRING("default").get())

#define PROFILE_SELECTION_URL          "chrome://communicator/content/profile/profileSelection.xul"
#define PROFILE_SELECTION_CMD_LINE_ARG "-SelectProfile"
#define PROFILE_MANAGER_URL            "chrome://communicator/content/profile/profileSelection.xul?manage=true"
#define PROFILE_MANAGER_CMD_LINE_ARG   "-ProfileManager"
#define PROFILE_WIZARD_URL             "chrome://communicator/content/profile/createProfileWizard.xul"
#define PROFILE_WIZARD_CMD_LINE_ARG    "-ProfileWizard"
#define INSTALLER_CMD_LINE_ARG         "-installer"
#define CREATE_PROFILE_CMD_LINE_ARG    "-CreateProfile"
#define PROFILE_CMD_LINE_ARG "-P"   
#define UILOCALE_CMD_LINE_ARG "-UILocale"   
#define CONTENTLOCALE_CMD_LINE_ARG "-contentLocale"   

#define PREF_CONFIRM_AUTOMIGRATION     "profile.confirm_automigration"
#define PREF_AUTOMIGRATION             "profile.allow_automigration"
#define PREF_MIGRATE_ALL               "profile.migrate_all"
#define PREF_MIGRATION_BEHAVIOR        "profile.migration_behavior"
#define PREF_MIGRATION_DIRECTORY       "profile.migration_directory"

#if defined (XP_MAC)
#define CHROME_STYLE nsIWebBrowserChrome::CHROME_WINDOW_BORDERS | nsIWebBrowserChrome::CHROME_WINDOW_CLOSE | nsIWebBrowserChrome::CHROME_CENTER_SCREEN
#else /* the rest */
#define CHROME_STYLE nsIWebBrowserChrome::CHROME_ALL | nsIWebBrowserChrome::CHROME_CENTER_SCREEN
#endif 

const char* kDefaultOpenWindowParams = "centerscreen,chrome,modal,titlebar";

const char* kBrandBundleURL = "chrome://global/locale/brand.properties";
const char* kMigrationBundleURL = "chrome://communicator/locale/profile/migration.properties";

// we want everyone to have the debugging info to the console for now
// to help track down profile manager problems
// when we ship, we'll turn this off
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
static nsProfileDirServiceProvider *gDirServiceProvider = nsnull;

// IID and CIDs of all the services needed
static NS_DEFINE_CID(kPrefMigrationCID, NS_PREFMIGRATION_CID);
static NS_DEFINE_CID(kPrefConverterCID, NS_PREFCONVERTER_CID);

#define NS_SESSIONROAMING_CONTRACTID "@mozilla.org/profile/session-roaming;1"


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
		                nsCAutoString leafName;
		                dirEntry->GetNativeLeafName(leafName);
		                newChild->AppendRelativeNativePath(leafName);
		                rv = RecursiveCopy(dirEntry, newChild);
		            }
		        }
		        else
		            rv = dirEntry->CopyToNative(destDir, nsCString());
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
    mStartingUp = PR_FALSE;
    mAutomigrate = PR_FALSE;
    mOutofDiskSpace = PR_FALSE;
    mDiskSpaceErrorQuitCalled = PR_FALSE;
    mCurrentProfileAvailable = PR_FALSE;

    mIsUILocaleSpecified = PR_FALSE;
    mIsContentLocaleSpecified = PR_FALSE;
    
    mShutdownProfileToreDownNetwork = PR_FALSE;
    
    mProfileChangeVetoed = PR_FALSE;
    mProfileChangeFailed = PR_FALSE;
}

nsProfile::~nsProfile() 
{
#if defined(DEBUG_profile_verbose)
    printf("~nsProfile \n");
#endif

   if (--gInstanceCount == 0) {
        
      delete gProfileDataAccess;
      delete gLocaleProfiles;
      NS_IF_RELEASE(gDirServiceProvider);
    }
}

nsresult
nsProfile::Init()
{
    nsresult rv = NS_OK;
    
    if (gInstanceCount++ == 0) {
        gProfileDataAccess = new nsProfileAccess;
        if (!gProfileDataAccess)
            return NS_ERROR_OUT_OF_MEMORY;
        gLocaleProfiles = new nsHashtable();
        if (!gLocaleProfiles)
            return NS_ERROR_OUT_OF_MEMORY;

        rv = NS_NewProfileDirServiceProvider(PR_FALSE, &gDirServiceProvider);
        if (NS_SUCCEEDED(rv))
            rv = gDirServiceProvider->Register();
    }
    return rv;
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

    nsCAutoString pathBuf;
    rv = profileDir->GetNativePath(pathBuf);
    if (NS_FAILED(rv)) return rv;

    // -UILocale or -contentLocale has been specified, but
    // user has selected UILocale and contentLocale for this profile
    // on profileManager
    // We should not set here
    nsCStringKey key(pathBuf);
    if (NS_PTR_TO_INT32(gLocaleProfiles->Get(&key)) == PR_TRUE) {
#ifdef DEBUG_profile_verbose
        printf(" already set UILocale and contentLocale: %s\n", pathBuf.get());
        printf(" will not install locale\n");
#endif
        return NS_OK;
    }
    gLocaleProfiles->Remove(&key);

    nsCOMPtr<nsIXULChromeRegistry> chromeRegistry =
        do_GetService(NS_CHROMEREGISTRY_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    // Install to the profile
    nsCAutoString fileStr;
    rv = NS_GetURLSpecFromFile(profileDir, fileStr);
    if (NS_FAILED(rv)) return rv;

    // NEED TO FIX: when current UILocale and contentLocale are same with specified locales,
    // we shouldn't install them again here. But packageRegistry->GetSelectedLocale() doesn't
    // work here properly. It always returns global and global-region of default not current
    // profile
    if (!mUILocaleName.IsEmpty()) {
#ifdef DEBUG_profile_verbose
        printf(" install new UILocaleName: %s\n", mUILocaleName.get());
#endif
        rv = chromeRegistry->SelectLocaleForProfile(mUILocaleName,
                                          NS_ConvertUTF8toUCS2(fileStr).get());
        if (NS_FAILED(rv)) return rv;
    }

    if (!mContentLocaleName.IsEmpty()) {
#ifdef DEBUG_profile_verbose
        printf(" install new mContentLocaleName: %s\n", mContentLocaleName.get());
#endif
        rv = chromeRegistry->SelectLocaleForProfile(mContentLocaleName,
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
    nsCOMPtr<nsIPrefBranch> prefBranch;
    nsCOMPtr<nsIURI> profileURL;
    PRInt32 numProfiles=0;
  
    nsCOMPtr<nsIPrefService> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    rv = prefs->GetBranch(nsnull, getter_AddRefs(prefBranch));
    if (NS_FAILED(rv)) return rv;
    
    GetProfileCount(&numProfiles);

    if (profileURLStr.IsEmpty())
    {
        // If this flag is TRUE, it makes the multiple profile case
        // just like the single profile case - the profile will be
        // set to that returned by GetCurrentProfile(). It will prevent
        // the profile selection dialog from being shown when we have
        // multiple profiles.
        
        PRBool startWithLastUsedProfile = PR_FALSE;

        // But first, make sure this app supports this.
        PRBool cantAutoSelect;
        rv = prefBranch->GetBoolPref("profile.manage_only_at_launch", &cantAutoSelect);
        if (NS_SUCCEEDED(rv) && !cantAutoSelect)
          GetStartWithLastUsedProfile(&startWithLastUsedProfile);
        
        // This means that there was no command-line argument to force
        // profile UI to come up. But we need the UI anyway if there
        // are no profiles yet, or if there is more than one.
        if (numProfiles == 0)
        {
            rv = CreateDefaultProfile();
            if (NS_FAILED(rv)) return rv;
            // Will get set in call to SetCurrentProfile() below
        }
        else if (numProfiles == 1 || startWithLastUsedProfile)
        {
            // If we get here and the 1 profile is the current profile,
            // which can happen with QuickLaunch, there's no need to do
            // any futher work.
            if (mCurrentProfileAvailable)
               return NS_OK;

            // Make sure the profile dir exists. If not, we need the UI
            nsCOMPtr<nsIFile> curProfileDir;
            PRBool exists = PR_FALSE;
            
            rv = GetCurrentProfileDir(getter_AddRefs(curProfileDir));
            if (NS_SUCCEEDED(rv))
                rv = curProfileDir->Exists(&exists);
            if (NS_FAILED(rv) || !exists)
                profileURLStr = PROFILE_MANAGER_URL; 
            if (exists)
            {
#ifdef MOZ_PROFILELOCKING
                // If the profile is locked, we need the UI
                nsCOMPtr<nsILocalFile> localFile(do_QueryInterface(curProfileDir));
                nsProfileLock tempLock;
                rv = tempLock.Lock(localFile);
                if (NS_FAILED(rv))
                    profileURLStr = PROFILE_MANAGER_URL;
#endif
            } 
        }
        else
            profileURLStr = PROFILE_SELECTION_URL;
    }

    if (!profileURLStr.IsEmpty())
    {
        if (!canInteract) return NS_ERROR_PROFILE_REQUIRES_INTERACTION;

        nsCOMPtr<nsIWindowWatcher> windowWatcher(do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIDialogParamBlock> ioParamBlock(do_CreateInstance(NS_DIALOGPARAMBLOCK_CONTRACTID, &rv));
        if (NS_FAILED(rv)) return rv;

        // String0  -> mode
        // Int0    <-  result code (1 == OK, 0 == Cancel)

        ioParamBlock->SetNumberStrings(1);
        ioParamBlock->SetString(0, NS_LITERAL_STRING("startup").get());

        nsCOMPtr<nsIDOMWindow> newWindow;
        rv = windowWatcher->OpenWindow(nsnull,
                                       profileURLStr.get(),
                                       "_blank",
                                       kDefaultOpenWindowParams,
                                       ioParamBlock,
                                       getter_AddRefs(newWindow));
        if (NS_FAILED(rv)) return rv;
        PRInt32 dialogConfirmed;
        ioParamBlock->GetInt(0, &dialogConfirmed);
        if (dialogConfirmed == 0) return NS_ERROR_ABORT;
    }

    nsXPIDLString currentProfileStr;    
    rv = GetCurrentProfile(getter_Copies(currentProfileStr));
    if (NS_FAILED(rv)) return rv;

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
               nsCOMPtr<nsISupportsCString> contractid;

               rv = enumItem->GetNext(getter_AddRefs(contractid));
               if (NS_FAILED(rv) || !contractid) break;

               nsCAutoString contractidString;
               contractid->GetData(contractidString);
        
               nsCOMPtr <nsIProfileStartupListener> listener = do_CreateInstance(contractidString.get(), &rv);
        
               if (listener) 
                   listener->OnProfileStartup(currentProfileStr);
           }
        }
    }

    PRBool prefs_converted = PR_FALSE;
    (void)prefBranch->GetBoolPref("prefs.converted-to-utf8", &prefs_converted);

    if (!prefs_converted) 
    {
        nsCOMPtr <nsIPrefConverter> pPrefConverter = do_CreateInstance(kPrefConverterCID, &rv);
        if (!pPrefConverter) return NS_ERROR_FAILURE;
        rv = pPrefConverter->ConvertPrefsToUTF8();
        if (NS_FAILED(rv)) return rv;
    }
    
    return NS_OK;
}

nsresult
nsProfile::ConfirmAutoMigration(PRBool canInteract, PRBool *confirmed)
{
    NS_ENSURE_ARG_POINTER(confirmed);
    nsCOMPtr<nsIPrefBranch> prefBranch;
    *confirmed = PR_FALSE;
    nsresult rv;
    
    // First check PREF_CONFIRM_AUTOMIGRATION.
    // If FALSE, we go ahead and migrate without asking.
    PRBool confirmAutomigration = PR_TRUE;
    nsCOMPtr<nsIPrefService> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    rv = prefs->GetBranch(nsnull, getter_AddRefs(prefBranch));
    if (NS_FAILED(rv)) return rv;

    (void)prefBranch->GetBoolPref(PREF_CONFIRM_AUTOMIGRATION, &confirmAutomigration);
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
            mUILocaleName = cmdResult;
        }
    }

    // -contentLocale command
    rv = cmdLineArgs->GetCmdLineValue(CONTENTLOCALE_CMD_LINE_ARG, getter_Copies(cmdResult));
    if (NS_SUCCEEDED(rv))
    {
        if (cmdResult) {
            mIsContentLocaleSpecified = PR_TRUE;
            mContentLocaleName = cmdResult;
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
            nsAutoString currProfileName; 
            if (nsCRT::IsAscii(cmdResult))  {
                currProfileName.AssignWithConversion(cmdResult);
            }
            else {
                // get a platform charset
                nsCAutoString charSet;
                rv = GetPlatformCharset(charSet);
                NS_ASSERTION(NS_SUCCEEDED(rv), "failed to get a platform charset");

                // convert the profile name to Unicode
                rv = ConvertStringToUnicode(charSet, cmdResult, currProfileName);
                NS_ASSERTION(NS_SUCCEEDED(rv), "failed to convert ProfileName to unicode");
            }
 
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
            nsAutoString currProfileName; 
 
            if (nsCRT::IsAscii(cmdResult))  {
                currProfileName.AssignWithConversion(strtok(cmdResult.BeginWriting(), " "));
            }
            else {
                // get a platform charset
                nsCAutoString charSet;
                rv = GetPlatformCharset(charSet);
                NS_ASSERTION(NS_SUCCEEDED(rv), "failed to get a platform charset");

                // convert the profile name to Unicode
                nsCAutoString profileName(strtok(cmdResult.BeginWriting(), " "));
                rv = ConvertStringToUnicode(charSet, profileName.get(), currProfileName);
                NS_ASSERTION(NS_SUCCEEDED(rv), "failed to convert ProfileName to unicode");
            }
            nsAutoString currProfileDirString; currProfileDirString.AssignWithConversion(strtok(NULL, " "));
        
            if (!currProfileDirString.IsEmpty()) {
                rv = NS_NewLocalFile(currProfileDirString, PR_TRUE, getter_AddRefs(currProfileDir));
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

            nsAutoString currProfilePath;
            currProfileDir->GetPath(currProfilePath);
            rv = CreateNewProfile(currProfileName.get(), currProfilePath.get(), nsnull, PR_TRUE);
            if (NS_SUCCEEDED(rv)) {
                *profileDirSet = PR_TRUE;
                mCurrentProfileAvailable = PR_TRUE;
                gProfileDataAccess->SetCurrentProfile(currProfileName.get());

                // Load new profile prefs for the sake of tinderbox scripts
                // which assume prefs.js exists after -CreateProfile.
                nsCOMPtr<nsIFile> newProfileDir;
                GetProfileDir(currProfileName.get(), getter_AddRefs(newProfileDir));
                if (newProfileDir) 
                  gDirServiceProvider->SetProfileDir(newProfileDir);
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

    nsCOMPtr<nsIPrefBranch> prefBranch;
   
    // First check PREF_AUTOMIGRATION. 
    PRBool allowAutoMigration = PR_TRUE;
    nsCOMPtr<nsIPrefService> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    rv = prefs->GetBranch(nsnull, getter_AddRefs(prefBranch));
    if (NS_FAILED(rv)) return rv;

    (void)prefBranch->GetBoolPref(PREF_AUTOMIGRATION, &allowAutoMigration);

    // Start Migaration activity
    rv = cmdLineArgs->GetCmdLineValue(INSTALLER_CMD_LINE_ARG, getter_Copies(cmdResult));
    if (allowAutoMigration && (NS_SUCCEEDED(rv) || forceMigration))
    {        
        if (cmdResult || forceMigration) {
        PRBool migrateAll = PR_FALSE;
        (void)prefBranch->GetBoolPref(PREF_MIGRATE_ALL, &migrateAll);

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
            else if ((num4xProfiles == 1 || migrateAll) && numProfiles == 0) {
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
    nsAutoString path;
    rv = prettyDir->GetPath(path);
    if (NS_FAILED(rv)) return rv;

    *_retval = ToNewUnicode(path);
    return *_retval ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP nsProfile::GetOriginalProfileDir(const PRUnichar *profileName, nsILocalFile **originalDir)
{
    NS_ENSURE_ARG(profileName);
    NS_ENSURE_ARG_POINTER(originalDir);
    *originalDir = nsnull;

    Update4xProfileInfo();
    return gProfileDataAccess->GetOriginalProfileDir(profileName, originalDir);
}

NS_IMETHODIMP nsProfile::GetProfileLastModTime(const PRUnichar *profileName, PRInt64 *_retval)
{
    NS_ENSURE_ARG(profileName);
    NS_ENSURE_ARG_POINTER(_retval);
    nsresult rv;
    
    // First, see if we can get the lastModTime from the registry.
    // We only started putting it there from mozilla1.0.1
    // The mod time will be zero if it has not been set.
    ProfileStruct *profileInfo = nsnull;
    rv = gProfileDataAccess->GetValue(profileName, &profileInfo);
    if (NS_SUCCEEDED(rv)) {
        PRInt64 lastModTime = profileInfo->lastModTime;
        delete profileInfo;
        if (!LL_IS_ZERO(lastModTime)) {
            *_retval = lastModTime;
            return NS_OK;
        }
    }

    // Since we couldn't get a valid mod time from the registry,
    // check the date of prefs.js. Since August, 2000 it is always
    // written out on quitting the application.
    nsCOMPtr<nsIFile> profileDir;
    rv = GetProfileDir(profileName, getter_AddRefs(profileDir));
    if (NS_FAILED(rv))
        return rv;
    rv = profileDir->AppendNative(NS_LITERAL_CSTRING("prefs.js"));
    if (NS_FAILED(rv))
        return rv;
    return profileDir->GetLastModifiedTime(_retval);
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

NS_IMETHODIMP nsProfile::GetStartWithLastUsedProfile(PRBool *aStartWithLastUsedProfile)
{
    NS_ENSURE_ARG_POINTER(aStartWithLastUsedProfile);
    return gProfileDataAccess->GetStartWithLastUsedProfile(aStartWithLastUsedProfile);
}

NS_IMETHODIMP nsProfile::SetStartWithLastUsedProfile(PRBool aStartWithLastUsedProfile)
{
    return gProfileDataAccess->SetStartWithLastUsedProfile(aStartWithLastUsedProfile);
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
#ifdef MOZ_PROFILELOCKING    
    nsProfileLock localLock;
    nsCOMPtr<nsILocalFile> localProfileDir(do_QueryInterface(profileDir, &rv));
    if (NS_FAILED(rv)) return rv;
    rv = localLock.Lock(localProfileDir);
    if (NS_FAILED(rv))
    {
        NS_ERROR("Could not get profile directory lock.");
        return rv;
    }
#endif
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

        // Phase 2a: Send the network teardown notification
        observerService->NotifyObservers(subject, "profile-change-net-teardown", context.get());
        mShutdownProfileToreDownNetwork = PR_TRUE;

        // Phase 2b: Send the "teardown" notification
        observerService->NotifyObservers(subject, "profile-change-teardown", context.get());
        if (mProfileChangeVetoed)
        {
            // Notify we will not proceed with changing the profile
            observerService->NotifyObservers(subject, "profile-change-teardown-veto", context.get());

            // Bring network back online and return
            observerService->NotifyObservers(subject, "profile-change-net-restore", context.get());
            return NS_OK;
        }
        
        // Phase 2c: Now that things are torn down, force JS GC so that things which depend on
        // resources which are about to go away in "profile-before-change" are destroyed first.
        nsCOMPtr<nsIThreadJSContextStack> stack =
          do_GetService("@mozilla.org/js/xpc/ContextStack;1", &rv);
        if (NS_SUCCEEDED(rv))
        {
          JSContext *cx = nsnull;
          stack->GetSafeJSContext(&cx);
          if (cx)
            ::JS_GC(cx);
        }
        
        // Phase 3: Notify observers of a profile change
        observerService->NotifyObservers(subject, "profile-before-change", context.get());        
        if (mProfileChangeFailed)
          return NS_ERROR_ABORT;
        
        UpdateCurrentProfileModTime(PR_FALSE);        
    }

#ifdef MOZ_PROFILELOCKING    
    // Do the profile switch
    localLock.Unlock(); // gDirServiceProvider will get and hold its own lock
#endif
    gDirServiceProvider->SetProfileDir(profileDir);  
    mCurrentProfileName.Assign(aCurrentProfile);    
    gProfileDataAccess->SetCurrentProfile(aCurrentProfile);
    gProfileDataAccess->mProfileDataChanged = PR_TRUE;
    gProfileDataAccess->UpdateRegistry(nsnull);
            
    if (NS_FAILED(rv)) return rv;
    mCurrentProfileAvailable = PR_TRUE;
    
    if (!isSwitch)
    {
        // Ensure that the prefs service exists so it can respond to
        // the notifications we're about to send around. It needs to.
        nsCOMPtr<nsIPrefService> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
        NS_ASSERTION(NS_SUCCEEDED(rv), "Could not get prefs service");
    }

    if (mShutdownProfileToreDownNetwork)
    {
        // Bring network back online
        observerService->NotifyObservers(subject, "profile-change-net-restore", context.get());
        mShutdownProfileToreDownNetwork = PR_FALSE;
        if (mProfileChangeFailed)
          return NS_ERROR_ABORT;
    }

    // Roaming download
    // Tolerate errors. Maybe the roaming extension isn't installed.
    nsCOMPtr <nsISessionRoaming> roam =
      do_GetService(NS_SESSIONROAMING_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv))
      roam->BeginSession();

    // Phase 4: Notify observers that the profile has changed - Here they respond to new profile
    observerService->NotifyObservers(subject, "profile-do-change", context.get());
    if (mProfileChangeFailed)
      return NS_ERROR_ABORT;

    // Phase 5: Now observers can respond to something another observer did in phase 4
    observerService->NotifyObservers(subject, "profile-after-change", context.get());
    if (mProfileChangeFailed)
      return NS_ERROR_ABORT;

    // Now that a profile is established, set the profile defaults dir for the locale of this profile
    rv = DefineLocaleDefaultsDir();
    NS_ASSERTION(NS_SUCCEEDED(rv), "nsProfile::DefineLocaleDefaultsDir failed");

    // Phase 6: One last notification after the new profile is established
    observerService->NotifyObservers(subject, "profile-initial-state", context.get());
    if (mProfileChangeFailed)
      return NS_ERROR_ABORT;

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
    
    // if shutDownType is not a well know value, skip the notifications
    // see DoOnShutdown() in nsAppRunner.cpp for where we use this behaviour to our benefit
    if (shutDownType == SHUTDOWN_PERSIST || shutDownType == SHUTDOWN_CLEANSE) {
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
      
      // Phase 2a: Send the network teardown notification
      observerService->NotifyObservers(subject, "profile-change-net-teardown", context.get());
      mShutdownProfileToreDownNetwork = PR_TRUE;

      // Phase 2b: Send the "teardown" notification
      observerService->NotifyObservers(subject, "profile-change-teardown", context.get());

      // Phase 2c: Now that things are torn down, force JS GC so that things which depend on
      // resources which are about to go away in "profile-before-change" are destroyed first.
      nsCOMPtr<nsIThreadJSContextStack> stack =
        do_GetService("@mozilla.org/js/xpc/ContextStack;1", &rv);
      if (NS_SUCCEEDED(rv))
      {
        JSContext *cx = nsnull;
        stack->GetSafeJSContext(&cx);
        if (cx)
          ::JS_GC(cx);
      }
      
      // Phase 3: Notify observers of a profile change
      observerService->NotifyObservers(subject, "profile-before-change", context.get());        
    }

    // Roaming upload
    // Tolerate errors. Maybe the roaming extension isn't installed.
    nsCOMPtr <nsISessionRoaming> roam =
      do_GetService(NS_SESSIONROAMING_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv))
      roam->EndSession();

    gDirServiceProvider->SetProfileDir(nsnull);
    UpdateCurrentProfileModTime(PR_TRUE);
    mCurrentProfileAvailable = PR_FALSE;
    mCurrentProfileName.Truncate(0);
    
    return NS_OK;
}


#define SALT_SIZE 8
#define TABLE_SIZE 36

static const char kSaltExtensionCString[] = ".slt";
#define kSaltExtensionCString_Len PRUint32(sizeof(kSaltExtensionCString)-1)

static const char table[] = 
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

  rv = prefFile->AppendNative(NS_LITERAL_CSTRING("prefs.js"));
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
        nsCAutoString leafName;
        rv = dirEntry->GetNativeLeafName(leafName);
	 	if (NS_SUCCEEDED(rv) && !leafName.IsEmpty()) {
		  PRUint32 length = leafName.Length();
		  // check if the filename is the right length, len("xxxxxxxx.slt")
		  if (length == (SALT_SIZE + kSaltExtensionCString_Len)) {
			// check that the filename ends with ".slt"
			if (nsCRT::strncmp(leafName.get() + SALT_SIZE,
                         kSaltExtensionCString,
                         kSaltExtensionCString_Len) == 0) {
			  // found a salt directory, use it
			  rv = aDir->AppendNative(leafName);
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
  saltStr.Append(kSaltExtensionCString, kSaltExtensionCString_Len);
#ifdef DEBUG_profile_verbose
  printf("directory name: %s\n",saltStr.get());
#endif

  rv = aDir->AppendNative(saltStr);
  NS_ENSURE_SUCCESS(rv,rv);

  exists = PR_FALSE;
  rv = aDir->Exists(&exists);
  NS_ENSURE_SUCCESS(rv,rv);
  if (!exists) {
    rv = aDir->Create(nsIFile::DIRECTORY_TYPE, 0700);
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
    nsCAutoString leafName;
    rv = profileDir->GetNativeLeafName(leafName);
    if (NS_FAILED(rv)) return rv;

    PRBool endsWithSalt = PR_FALSE;    
    if (leafName.Length() >= kSaltExtensionCString_Len)
    {
        nsReadingIterator<char> stringEnd;
        leafName.EndReading(stringEnd);

        nsReadingIterator<char> stringStart = stringEnd;
        stringStart.advance( -(NS_STATIC_CAST(PRInt32, kSaltExtensionCString_Len)) );

        endsWithSalt =
            Substring(stringStart, stringEnd).Equals(kSaltExtensionCString);
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
        rv = profileDir->Create(nsIFile::DIRECTORY_TYPE, 0700);
    if (NS_FAILED(rv)) 
        return rv;
    
    nsCOMPtr<nsILocalFile> localFile(do_QueryInterface(profileDir));
    NS_ENSURE_TRUE(localFile, NS_ERROR_FAILURE);                

    ProfileStruct* aProfile = new ProfileStruct();
    NS_ENSURE_TRUE(aProfile, NS_ERROR_OUT_OF_MEMORY);

    aProfile->profileName = profileName;
    aProfile->SetResolvedProfileDir(localFile);
    aProfile->isMigrated = PR_TRUE;
    aProfile->isImportType = PR_FALSE;

    // convert "now" from microsecs to millisecs
    PRInt64 oneThousand = LL_INIT(0, 1000);
    PRInt64 nowInMilliSecs = PR_Now();
    LL_DIV(aProfile->creationTime, nowInMilliSecs, oneThousand); 

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

      printf("Profile Name: %s\n", NS_LossyConvertUCS2toASCII(profileName).get());

      if (nativeProfileDir)
        printf("Profile Dir: %s\n", NS_LossyConvertUCS2toASCII(nativeProfileDir).get());
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
            profileDir->Create(nsIFile::DIRECTORY_TYPE, 0700);

        // append profile name
        profileDir->Append(nsDependentString(profileName));
    }
    else {
    
        rv = NS_NewLocalFile(nsDependentString(nativeProfileDir), PR_TRUE, (nsILocalFile **)((nsIFile **)getter_AddRefs(profileDir)));

        // this prevents people from choosing there profile directory
        // or another directory, and remove it when they delete the profile.
        // append profile name
        profileDir->Append(nsDependentString(profileName));
    }


    // Make profile directory unique only when the user 
    // decides to not use an already existing profile directory
    if (!useExistingDir) {
        rv = profileDir->CreateUnique(nsIFile::DIRECTORY_TYPE, 0700);
        if (NS_FAILED(rv)) return rv;
    }

#if defined(DEBUG_profile_verbose)
    printf("before SetProfileDir\n");
#endif

    rv = profileDir->Exists(&exists);
    if (NS_FAILED(rv)) return rv;        
    if (!exists)
    {
        rv = profileDir->Create(nsIFile::DIRECTORY_TYPE, 0700);
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

    nsCOMPtr<nsIXULChromeRegistry> chromeRegistry =
        do_GetService(NS_CHROMEREGISTRY_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {

        nsCAutoString uiLocale; uiLocale.AssignWithConversion(aUILocale);
        nsCAutoString contentLocale; contentLocale.AssignWithConversion(aContentLocale);

        // When aUILocale == null or aContentLocale == null, set those
        // from default values which are from default or from command
        // line options

        // This fallback is for CreateNewProfile() of
        // CreateDefaultProfile() and ProcessArgs() Those functions
        // call CreateNewProfile(locale=null). We should consider
        // default values or specified values of locales for
        // CreateDefaultProfile() and ProcessArgs().

        // We can get preferred UILocale and contentLocale (specified
        // -UILocale and -contentLocale) by GetSelectedLocale() which
        // is done in nsAppRunner.cpp::InstallGlobalLocale()

        nsCOMPtr<nsIXULChromeRegistry> packageRegistry = do_QueryInterface(chromeRegistry);
        if ((!aUILocale || !aUILocale[0]) && packageRegistry) {
            nsCAutoString currentUILocaleName;
            rv = packageRegistry->GetSelectedLocale(NS_LITERAL_CSTRING("global"),
                                                    currentUILocaleName);
            if (NS_SUCCEEDED(rv)) {
                uiLocale = currentUILocaleName;
            }
        }

        if (!aContentLocale || !aContentLocale[0]) {
            nsCAutoString currentContentLocaleName;
            rv = packageRegistry->GetSelectedLocale(NS_LITERAL_CSTRING("global-region"),
                                                    currentContentLocaleName);
            if (NS_SUCCEEDED(rv)) {
                contentLocale = currentContentLocaleName;
            }
        }

#if defined(DEBUG_profile_verbose)
        printf(" uiLocale=%s\n", uiLocale.get());
        printf(" contentLocale=%s\n", contentLocale.get());
#endif

        nsCAutoString pathBuf;
        rv = profileDir->GetNativePath(pathBuf);
        NS_ENSURE_SUCCESS(rv, rv);

        nsCAutoString fileStr;
        rv = NS_GetURLSpecFromFile(profileDir, fileStr);
        if (NS_FAILED(rv)) return rv;

        if (!uiLocale.IsEmpty()) {
            rv = chromeRegistry->SelectLocaleForProfile(uiLocale, 
                                                        NS_ConvertUTF8toUCS2(fileStr).get());
            // Remember which profile has been created with the UILocale
            // didn't use gProfileDataAccess because just needed one time
            if (NS_SUCCEEDED(rv)) {
                nsCStringKey key(pathBuf);
                gLocaleProfiles->Put(&key, (void*)PR_TRUE);
            }
        }

        //need to set skin info here
        nsCAutoString currentSkinName;
        rv = packageRegistry->GetSelectedSkin(NS_LITERAL_CSTRING("global"),currentSkinName);
        if (!currentSkinName.IsEmpty()) {
            rv = chromeRegistry->SelectSkinForProfile(currentSkinName,
                                         NS_ConvertUTF8toUCS2(fileStr).get());
        }

        if (!contentLocale.IsEmpty()) {
            // caller prefers locale subdir
            nsCOMPtr<nsIFile> locProfDefaultsDir;
            rv = profDefaultsDir->Clone(getter_AddRefs(locProfDefaultsDir));
            if (NS_FAILED(rv)) return rv;
        
            locProfDefaultsDir->AppendNative(contentLocale);
            rv = locProfDefaultsDir->Exists(&exists);
            if (NS_SUCCEEDED(rv) && exists) {
                profDefaultsDir = locProfDefaultsDir; // transfers ownership
#if defined(DEBUG_profile_verbose)
                nsCAutoString profilePath;
                rv = profDefaultsDir->GetNativePath(profilePath);
                if (NS_SUCCEEDED(rv))
                    printf(" profDefaultsDir is set to: %s\n", profilePath.get());
#endif
            }

            rv = chromeRegistry->SelectLocaleForProfile(contentLocale, 
                                                        NS_ConvertUTF8toUCS2(fileStr).get());
            // Remember which profile has been created with the UILocale
            // didn't use gProfileDataAccess because just needed one time
            if (NS_SUCCEEDED(rv)) {
                nsCStringKey key(pathBuf);
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
      printf("Old name:  %s\n", NS_LossyConvertUCS2toASCII(oldName).get());

      nsCAutoString temp2; temp2.AssignWithConversion(newName);
      printf("New name:  %s\n", NS_LossyConvertUCS2toASCII(newName).get());
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
    mCurrentProfileName.Truncate(0);
    
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
    
    if (whichKind == nsIProfileInternal::LIST_FOR_IMPORT)
        Update4xProfileInfo();
    return gProfileDataAccess->GetProfileList(whichKind, length, profileNames);
}

// this will add all the 4x profiles (with isImportType flag set) to the 
// profiles list. The adding of profiles is done only once per session.
nsresult nsProfile::Update4xProfileInfo()
{
    nsresult rv = NS_OK;

#ifndef XP_BEOS
    nsCOMPtr<nsIFile> oldRegFile;
    rv = GetOldRegLocation(getter_AddRefs(oldRegFile));
    if (NS_SUCCEEDED(rv))
    rv = gProfileDataAccess->Get4xProfileInfo(oldRegFile, PR_TRUE);
#endif

    return rv;
}

nsresult nsProfile::LoadNewProfilePrefs()
{
    nsresult rv;
    nsCOMPtr<nsIPrefService> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    
    prefs->ResetUserPrefs();
    prefs->ReadUserPrefs(nsnull);

    return NS_OK;
}

nsresult nsProfile::GetOldRegLocation(nsIFile **aOldRegFile)
{
    NS_ENSURE_ARG_POINTER(aOldRegFile);
    *aOldRegFile = nsnull;
    nsresult rv = NS_OK;
    
    // For XP_UNIX there was no registry for 4.x profiles.
    // Return nsnull for the file, and NS_OK - we succeeded in doing nothing.
    
#if defined(XP_WIN) || defined(XP_OS2) || defined(XP_MAC) || defined(XP_MACOSX)
    nsCOMPtr<nsIFile> oldRegFile;

#if defined(XP_WIN)
    rv = NS_GetSpecialDirectory(NS_WIN_WINDOWS_DIR, getter_AddRefs(oldRegFile));
#elif defined(XP_OS2)
    rv = NS_GetSpecialDirectory(NS_OS2_DIR, getter_AddRefs(oldRegFile));
#elif defined(XP_MAC) || defined(XP_MACOSX)
    rv = NS_GetSpecialDirectory(NS_MAC_PREFS_DIR, getter_AddRefs(oldRegFile));
#endif

    if (NS_FAILED(rv))
        return rv;
    rv = oldRegFile->AppendNative(nsDependentCString(OLD_REGISTRY_FILE_NAME));
    if (NS_FAILED(rv))
        return rv;
    NS_ADDREF(*aOldRegFile = oldRegFile);
#endif

    return rv;
}

nsresult nsProfile::UpdateCurrentProfileModTime(PRBool updateRegistry)
{
    nsresult rv;

    // convert "now" from microsecs to millisecs
    PRInt64 oneThousand = LL_INIT(0, 1000);
    PRInt64 nowInMilliSecs = PR_Now();
    LL_DIV(nowInMilliSecs, nowInMilliSecs, oneThousand); 
    
    rv = gProfileDataAccess->SetProfileLastModTime(mCurrentProfileName.get(), nowInMilliSecs);
    if (NS_SUCCEEDED(rv) && updateRegistry) {
        gProfileDataAccess->mProfileDataChanged = PR_TRUE;
        gProfileDataAccess->UpdateRegistry(nsnull);
    }
    return rv;
}

// Migrate profile information from the 4x registry to 5x registry.
NS_IMETHODIMP nsProfile::MigrateProfileInfo()
{
    nsresult rv = NS_OK;

#ifndef XP_BEOS
    nsCOMPtr<nsIFile> oldRegFile;
    rv = GetOldRegLocation(getter_AddRefs(oldRegFile));
    if (NS_SUCCEEDED(rv)) {
    rv = gProfileDataAccess->Get4xProfileInfo(oldRegFile, PR_FALSE);
    gProfileDataAccess->mProfileDataChanged = PR_TRUE;
    gProfileDataAccess->UpdateRegistry(nsnull);
    }
#endif

	return rv;
}

nsresult
nsProfile::CopyDefaultFile(nsIFile *profDefaultsDir, nsIFile *newProfDir, const nsACString &fileName)
{
    nsresult rv;
    nsCOMPtr<nsIFile> defaultFile;
    PRBool exists;
    
    rv = profDefaultsDir->Clone(getter_AddRefs(defaultFile));
    if (NS_FAILED(rv)) return rv;
    
    defaultFile->AppendNative(fileName);
    rv = defaultFile->Exists(&exists);
    if (NS_FAILED(rv)) return rv;
    if (exists)
        rv = defaultFile->CopyToNative(newProfDir, fileName);
    else
        rv = NS_ERROR_FILE_NOT_FOUND;

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
        nsCOMPtr<nsIXULChromeRegistry> packageRegistry = 
                 do_GetService(NS_CHROMEREGISTRY_CONTRACTID, &rv);
        if (NS_SUCCEEDED(rv))
        {
            nsCAutoString localeName;
            rv = packageRegistry->GetSelectedLocale(NS_LITERAL_CSTRING("global-region"), localeName);
            if (NS_SUCCEEDED(rv))
                rv = localeDefaults->AppendNative(localeName);
        }
        rv = directoryService->Set(NS_APP_PROFILE_DEFAULTS_50_DIR, localeDefaults);
    }
    return rv;
}

// Migrate a selected profile
// Set the profile to the current profile....debatable.
// Calls PrefMigration service to do the Copy and Diverge
// of 4x Profile information
nsresult 
nsProfile::MigrateProfileInternal(const PRUnichar* profileName,
                                  nsIFile* oldProfDir,
                                  nsIFile* newProfDir)
{
    NS_ENSURE_ARG_POINTER(profileName);   

#if defined(DEBUG_profile)
    printf("Inside Migrate Profile routine.\n" );
#endif

    // Call migration service to do the work.
    nsCOMPtr <nsIPrefMigration> pPrefMigrator;


    nsresult rv = nsComponentManager::CreateInstance(kPrefMigrationCID, 
                                            nsnull,
                                            NS_GET_IID(nsIPrefMigration),
                                            getter_AddRefs(pPrefMigrator));
    if (NS_FAILED(rv)) return rv;
    if (!pPrefMigrator) return NS_ERROR_FAILURE;
        
    nsCOMPtr<nsILocalFile> oldProfDirLocal(do_QueryInterface(oldProfDir, &rv));
    if (NS_FAILED(rv)) return rv;    
    nsCOMPtr<nsILocalFile> newProfDirLocal(do_QueryInterface(newProfDir, &rv));
    if (NS_FAILED(rv)) return rv;
        
    nsCAutoString oldProfDirStr;
    nsCAutoString newProfDirStr;

    rv = oldProfDirLocal->GetPersistentDescriptor(oldProfDirStr);
    if (NS_FAILED(rv)) return rv;
    rv = newProfDirLocal->GetPersistentDescriptor(newProfDirStr);
    if (NS_FAILED(rv)) return rv;

    // you can do this a bunch of times.
    rv = pPrefMigrator->AddProfilePaths(oldProfDirStr.get(), newProfDirStr.get());  

    rv = pPrefMigrator->ProcessPrefs(PR_TRUE); // param is ignored
    if (NS_FAILED(rv)) return rv;

    // check for diskspace errors  
    nsresult errorCode;   
    errorCode = pPrefMigrator->GetError();

    // In either of the cases below we have to return error to make
    // app understand that migration has failed.
    if (errorCode == MIGRATION_CREATE_NEW)
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
    else if (errorCode == MIGRATION_CANCEL) 
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
    else if (errorCode != MIGRATION_SUCCESS) 
    {
        return NS_ERROR_FAILURE;
    }

    // No longer copying the default 5.0 profile files into
    // the migrated profile. Check for them as requested.
	
    rv = SetProfileDir(profileName, newProfDir);
    if (NS_FAILED(rv)) return rv;

    gProfileDataAccess->SetMigratedFromDir(profileName, oldProfDirLocal);
    gProfileDataAccess->mProfileDataChanged = PR_TRUE;
    gProfileDataAccess->UpdateRegistry(nsnull);

    return rv;
}

NS_IMETHODIMP 
nsProfile::MigrateProfile(const PRUnichar* profileName)
{
    NS_ENSURE_ARG(profileName);   

    nsresult rv = NS_OK;

    nsCOMPtr<nsIFile> oldProfDir;    
    nsCOMPtr<nsIFile> newProfDir;
    nsCOMPtr<nsIPrefBranch> prefBranch;
    nsXPIDLCString profMigDir;
 
    rv = GetProfileDir(profileName, getter_AddRefs(oldProfDir));
    if (NS_FAILED(rv)) 
      return rv;
   
    // Check PREF_MIGRATION_BEHAVIOR for how to set profiles root
    // 0 - use NS_APP_USER_PROFILES_ROOT_DIR
    // 1 - create one based on the NS4.x profile root
    // 2 - use if not empty PREF_MIGRATION_DIRECTORY
    PRInt32 profRootBehavior = 0;
    nsCOMPtr<nsIPrefService> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
      rv = prefs->GetBranch(nsnull, getter_AddRefs(prefBranch));
      if (NS_SUCCEEDED(rv))
        (void) prefBranch->GetIntPref(PREF_MIGRATION_BEHAVIOR, &profRootBehavior);
    }

    switch (profRootBehavior) {

      case 1:
        rv = oldProfDir->Clone(getter_AddRefs(newProfDir));
        if (NS_FAILED(rv)) 
          return rv;
        rv = newProfDir->SetNativeLeafName(NS_LITERAL_CSTRING("Profiles"));
        if (NS_FAILED(rv)) 
          return rv;
        break;

      case 2:
        rv = prefBranch->GetCharPref(PREF_MIGRATION_DIRECTORY, getter_Copies(profMigDir));
        if (NS_SUCCEEDED(rv) && !profMigDir.IsEmpty()) {
          nsCOMPtr<nsILocalFile> localFile(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));
          NS_ENSURE_SUCCESS(rv, rv);
          rv = localFile->InitWithNativePath(nsDependentCString(profMigDir));
          if (NS_SUCCEEDED(rv)) {
            newProfDir = do_QueryInterface(localFile, &rv);
            if (NS_FAILED(rv)) 
              return rv;
          }
        }
        break;

      default:
        break;

    }
    if (!newProfDir) {
      rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILES_ROOT_DIR, getter_AddRefs(newProfDir));
      if (NS_FAILED(rv)) 
        return rv;
    }

    rv = newProfDir->Append(nsDependentString(profileName));
    if (NS_FAILED(rv)) 
      return rv;
    
    rv = newProfDir->CreateUnique(nsIFile::DIRECTORY_TYPE, 0700);
    if (NS_FAILED(rv)) 
      return rv;
    
    // always create level indirection when migrating
    rv = AddLevelOfIndirection(newProfDir);
    if (NS_FAILED(rv))
      return rv;

    return MigrateProfileInternal(profileName, oldProfDir, newProfDir);
}

NS_IMETHODIMP 
nsProfile::RemigrateProfile(const PRUnichar* profileName)
{
    NS_ENSURE_ARG_POINTER(profileName);
    
    nsCOMPtr<nsIFile> profileDir;
    nsresult rv = GetProfileDir(profileName, getter_AddRefs(profileDir));
    NS_ENSURE_SUCCESS(rv,rv);

    nsCOMPtr<nsIFile> newProfileDir;
    rv = profileDir->Clone(getter_AddRefs(newProfileDir));
    NS_ENSURE_SUCCESS(rv,rv);

    // The profile list used by GetOriginalProfileDir is the one with ALL 4.x
    // profiles - even ones for which there's a moz profile of the same name.
    nsCOMPtr<nsILocalFile> oldProfileDir;
    rv = GetOriginalProfileDir(profileName, getter_AddRefs(oldProfileDir));
    NS_ENSURE_SUCCESS(rv,rv);

    // In case of error, we'll restore what we've renamed.
    nsCAutoString origDirLeafName;
    rv = profileDir->GetNativeLeafName(origDirLeafName);
    NS_ENSURE_SUCCESS(rv,rv);
     
    // leave <xxxxxxxx>.slt alone, 
    // and remigrate the 4.x profile into <xxxxxxxx>.slt-new
    nsCAutoString newDirLeafName(origDirLeafName + NS_LITERAL_CSTRING("-new"));
    rv = newProfileDir->SetNativeLeafName(newDirLeafName);
    NS_ENSURE_SUCCESS(rv,rv);
    
    // Create a new directory for the remigrated profile
    rv = newProfileDir->CreateUnique(nsIFile::DIRECTORY_TYPE, 0700);
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to create new directory for the remigrated profile");
    if (NS_SUCCEEDED(rv)) {
        rv = MigrateProfileInternal(profileName, oldProfileDir, newProfileDir);
        NS_ASSERTION(NS_SUCCEEDED(rv), "MigrateProfileInternal failed");
    }
    return rv;
}

nsresult
nsProfile::ShowProfileWizard(void)
{
    nsresult rv;
    nsCOMPtr<nsIWindowWatcher> windowWatcher(do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIDialogParamBlock> ioParamBlock(do_CreateInstance(NS_DIALOGPARAMBLOCK_CONTRACTID, &rv));
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
        rv = MigrateProfile(nameArray[i]);
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
        destDir->AppendRelativePath(nsDependentString(newProfile));

        // Find a unique name in the dest dir
        rv = destDir->CreateUnique(nsIFile::DIRECTORY_TYPE, 0700);
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
      printf("The new profile is ->%s<-\n", temp.get());
    }
#endif

    gProfileDataAccess->mProfileDataChanged = PR_TRUE;

    return rv;
}

nsresult
nsProfile::CreateDefaultProfile(void)
{
    nsresult rv = NS_OK;

    // Get the default user profiles folder
    nsCOMPtr<nsIFile> profileRootDir;
    rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILES_ROOT_DIR, getter_AddRefs(profileRootDir));
    if (NS_FAILED(rv)) return rv;
    
    nsAutoString profilePath;
    rv = profileRootDir->GetPath(profilePath);
    if (NS_FAILED(rv)) return rv;

    rv = CreateNewProfile(DEFAULT_PROFILE_NAME, profilePath.get(), nsnull, PR_TRUE);

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

   if (aProfile == nsnull)
     return NS_ERROR_FAILURE;
   
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
 * nsIProfileChangeStatus Implementation
 */

NS_IMETHODIMP nsProfile::VetoChange()
{
    mProfileChangeVetoed = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP nsProfile::ChangeFailed()
{
    mProfileChangeFailed = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsProfile::GetRegStrings(const PRUnichar *aProfileName, 
                        PRUnichar **aRegString, 
                        PRUnichar **aRegName, 
                        PRUnichar **aRegEmail, 
                        PRUnichar **aRegOption)
{
    NS_ENSURE_ARG_POINTER(aProfileName);
    NS_ENSURE_ARG_POINTER(aRegString);
    NS_ENSURE_ARG_POINTER(aRegName);
    NS_ENSURE_ARG_POINTER(aRegEmail);
    NS_ENSURE_ARG_POINTER(aRegOption);

    ProfileStruct*    profileVal;

    nsresult rv = gProfileDataAccess->GetValue(aProfileName, &profileVal);
    if (NS_FAILED(rv)) 
      return rv;

    if (profileVal == nsnull)
      return NS_ERROR_FAILURE;

    *aRegString = ToNewUnicode(profileVal->NCHavePregInfo);
    if (!*aRegString)
      return NS_ERROR_OUT_OF_MEMORY;    

    *aRegName = ToNewUnicode(profileVal->NCProfileName);
    if (!*aRegName)
      return NS_ERROR_OUT_OF_MEMORY;    

    *aRegEmail = ToNewUnicode(profileVal->NCEmailAddress);
    if (!*aRegEmail)
      return NS_ERROR_OUT_OF_MEMORY;    

    *aRegOption = ToNewUnicode(profileVal->NCDeniedService);
    if (!*aRegOption)
      return NS_ERROR_OUT_OF_MEMORY;    

     delete profileVal;

     return NS_OK;
}
