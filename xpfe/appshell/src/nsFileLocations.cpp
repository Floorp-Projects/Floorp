/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     John R. McMullen <mcmullen@netscape.com>
 *     Seth Spitzer <sspitzer@netscape.com>
 */

#include "nsFileLocations.h"
#include "nsIFileLocator.h"

#include "nsIFileSpec.h"

#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsSpecialSystemDirectory.h"
#include "nsDebug.h"

#include "nsIChromeRegistry.h" // chromeReg
#include "nsXPIDLString.h"     // nsXPIDLString

#ifdef XP_MAC
#include <Folders.h>
#include <Files.h>
#include <Memory.h>
#include <Processes.h>
#elif defined(XP_PC)
#if !defined(XP_OS2)
#include <windows.h>
#endif // !XP_OS2 
#include <stdlib.h>
#include <stdio.h>
#elif defined(XP_UNIX)
#include <unistd.h>
#include <sys/param.h>
#endif

#include "nsIProfile.h"
#include "nsIPrefMigration.h" // for NEW_IMAPMAIL_DIR_NAME, etc

#include "plstr.h"
#include "prenv.h"
#include "prmem.h"

// for profile manager
static NS_DEFINE_CID(kProfileCID,           NS_PROFILE_CID);
static NS_DEFINE_CID(kChromeRegistryCID,    NS_CHROMEREGISTRY_CID);

// Global variable for gProfileDir
static nsFileSpec* gProfileDir = nsnull;
static PRInt32 gRegisteredWithDirService = PR_FALSE;
#ifdef XP_MAC
#pragma export on
#endif


struct DirectoryTable
{
    char *  directoryName;          /* The formal directory name */
    PRInt32 folderEnum;             /* Directory ID */
};

struct DirectoryTable DirectoryTable[] = 
{
// Preferences:

    {"app.prefs.directory.3",        nsSpecialFileSpec::App_PrefsDirectory30 },
    {"app.prefs.directory.4",        nsSpecialFileSpec::App_PrefsDirectory40 },
    {"app.prefs.directory.5",        nsSpecialFileSpec::App_PrefsDirectory50 },
    {"app.pref.default.directory.5", nsSpecialFileSpec::App_PrefDefaultsFolder50 },
    
    {"app.prefs.file.3",        nsSpecialFileSpec::App_PreferencesFile30 },
    {"app.prefs.file.4",        nsSpecialFileSpec::App_PreferencesFile40 },
    {"app.prefs.file.5",        nsSpecialFileSpec::App_PreferencesFile50 },

// Profile:

    {"app.profile.user.directory.3",        nsSpecialFileSpec::App_UserProfileDirectory30 },
    {"app.profile.user.directory.4",        nsSpecialFileSpec::App_UserProfileDirectory40 },
    {"app.profile.user.directory.5",        nsSpecialFileSpec::App_UserProfileDirectory50 },
    {"app.profile.default.user.directory.3",nsSpecialFileSpec::App_DefaultUserProfileRoot30 },
    {"app.profile.default.user.directory.4",nsSpecialFileSpec::App_DefaultUserProfileRoot40 },
    {"app.profile.default.user.directory.5",nsSpecialFileSpec::App_DefaultUserProfileRoot50 },
    {"app.profile.defaults.directory.3",    nsSpecialFileSpec::App_ProfileDefaultsFolder30 },
    {"app.profile.defaults.directory.4",    nsSpecialFileSpec::App_ProfileDefaultsFolder40 },
    {"app.profile.defaults.directory.5",    nsSpecialFileSpec::App_ProfileDefaultsFolder50 },
    {"app.profile.defaults.directory.5.nloc",    nsSpecialFileSpec::App_ProfileDefaultsFolder50_nloc },


// Application Directories: 
    {"app.res.directory",            nsSpecialFileSpec::App_ResDirectory },
    {"app.defaults.directory",       nsSpecialFileSpec::App_DefaultsFolder50 },
    {"app.chrome.directory",           nsSpecialFileSpec::App_ChromeDirectory },
    {"app.chrome.user.directory",      nsSpecialFileSpec::App_UserChromeDirectory },
    {"app.plugins.directory",         nsSpecialFileSpec::App_PluginsDirectory },

// Bookmarks:

    {"app.bookmark.file.3",     nsSpecialFileSpec::App_BookmarksFile30 },
    {"app.bookmark.file.4",     nsSpecialFileSpec::App_BookmarksFile40 },
    {"app.bookmark.file.5",     nsSpecialFileSpec::App_BookmarksFile50 },

// Search
    {"app.search.file.5",            nsSpecialFileSpec::App_SearchFile50 },
    {"app.search.directory.5",       nsSpecialFileSpec::App_SearchDirectory50 },
    
// Application Files:

    {"app.registry.file.4",          nsSpecialFileSpec::App_Registry40 },
    {"app.registry.file.5",          nsSpecialFileSpec::App_Registry50 },
    {"app.local.store.file.5",       nsSpecialFileSpec::App_LocalStore50 },
    {"app.history.file.5",           nsSpecialFileSpec::App_History50 },
    {"app.user.panels.5",            nsSpecialFileSpec::App_UsersPanels50 },
    {"app.user.mimeTypes.5",            nsSpecialFileSpec::App_UsersMimeTypes50},

// MailNews:

    {"app.mail.directory.5",            nsSpecialFileSpec::App_MailDirectory50 },
    {"app.mail.imap.directory.5",       nsSpecialFileSpec::App_ImapMailDirectory50 },
    {"app.mail.news.directory.5",       nsSpecialFileSpec::App_NewsDirectory50 },
    {"app.mail.messenger.cache.directory.5",     nsSpecialFileSpec::App_MessengerFolderCache50 },

    {"",     0 },
};











//========================================================================================
// Static functions that ain't nobody else's business.
//========================================================================================

//----------------------------------------------------------------------------------------
static nsresult GetChromeLocale(PRUnichar** lc_name)
// Inquire the current chrome UI locale 
//----------------------------------------------------------------------------------------
{
  nsresult rv = NS_ERROR_FAILURE;
  *lc_name = nsnull;
  nsCOMPtr<nsIChromeRegistry> chromeRegistry = do_GetService(kChromeRegistryCID, &rv);

  if (NS_SUCCEEDED(rv)) {
      nsString tmpstr; tmpstr.AssignWithConversion("navigator");
      rv = chromeRegistry->GetSelectedLocale(tmpstr.GetUnicode(), lc_name);
  }
  return rv;
}

//----------------------------------------------------------------------------------------
static PRBool GetProfileDirectory(nsFileSpec& outSpec)
// The app profile directory comes from the profile manager.
// Once the profile manager knows which profile needs to be
// accessed it tells us about the directory. 

// And if the profile manager doesn't return anything, we use the routine above,
// CreateDefaultProfileDirectorySpec() above.
//----------------------------------------------------------------------------------------
{
        // pointer so that we can detect whether it has been initialized
    if (!gProfileDir)
    {
        // First time, initialize gProfileDir
        nsresult rv;
        NS_WITH_SERVICE(nsIProfile, profileService, kProfileCID, &rv);
        if (NS_FAILED(rv))
            return PR_FALSE;
        
        profileService->Startup(nsnull);

        PRBool available = PR_FALSE;
        profileService->IsCurrentProfileAvailable(&available);
        if (!available)
            return PR_FALSE;

        int numProfiles = 0;
        profileService->GetProfileCount(&numProfiles);
        if (numProfiles == 0)
            return PR_FALSE;

        PRUnichar* currProfileName;
        nsCOMPtr<nsIFile> currProfileDir;
        if (numProfiles == 1)
        {
            // one profile exists: use that profile
            profileService->GetFirstProfile(&currProfileName);
            if (currProfileName && (nsCRT::strlen(currProfileName) > 0)) {
                profileService->GetProfileDir(currProfileName, getter_AddRefs(currProfileDir));
            }
            else {
                // this should never happen
                PR_FREEIF(currProfileName);
                return PR_FALSE;
            }
        }
        else
        {
            // multiple profiles exist: we'll use the same profile as last time 
            // (see following rules) 
            // (if we can't figure out what the last profile used was for some reason, 
            // we'll pick the first one as returned from the registry query) 
            profileService->GetCurrentProfile(&currProfileName);
            if (currProfileName && (nsCRT::strlen(currProfileName) > 0)) {
                profileService->GetProfileDir(currProfileName, getter_AddRefs(currProfileDir));
            }
            else
            {
                profileService->GetFirstProfile(&currProfileName);
                if (!currProfileName || (nsCRT::strlen(currProfileName) == 0)) {
                    profileService->GetProfileDir(currProfileName, getter_AddRefs(currProfileDir));
                }
            }
        }
        if (!currProfileName || (nsCRT::strlen(currProfileName) == 0)) {
            // we don't have it, return false.
            PR_FREEIF(currProfileName);
            return PR_FALSE;
        }    
        
        nsFileSpec currProfileDirSpec;
        nsXPIDLCString pathBuf;
        rv = currProfileDir->GetPath(getter_Copies(pathBuf));
        if (NS_FAILED(rv)) return PR_FALSE;
        currProfileDirSpec = (const char *)pathBuf;
    
#if defined(NS_DEBUG)
    if (currProfileName) {
        nsCAutoString currProfileNameCStr; currProfileNameCStr.AssignWithConversion(currProfileName);
        printf("ProfileName : %s\n", currProfileNameCStr.GetBuffer());
        printf("ProfileDir  : %s\n", currProfileDirSpec.GetNativePathCString());
    }
#endif /* NS_DEBUG */
        PR_FREEIF(currProfileName);
        if (!currProfileDirSpec.Exists())
            currProfileDirSpec.CreateDir();

        // Copy into our cached pointer so we'll only do this once (unless told to 'forget').
        // We also do this by querying the profile manager, instead of just using
        // our local copy (currProfileDirSpec), though they should be the same.
        gProfileDir = new nsFileSpec("Default");
        if (!gProfileDir)
            return PR_FALSE;
        if (NS_FAILED(profileService->GetCurrentProfileDir(getter_AddRefs(currProfileDir))))
        {
            delete gProfileDir; // All that for nothing.  sigh.
            gProfileDir = nsnull;
            return PR_FALSE;
        }
        nsXPIDLCString pathBuf2;
        rv = currProfileDir->GetPath(getter_Copies(pathBuf2));
        if (NS_FAILED(rv)) return PR_FALSE;
        *gProfileDir = (const char *)pathBuf2;

        NS_ASSERTION(*gProfileDir == currProfileDirSpec, "Profile spec does not match!");

        if (!gProfileDir->Exists())
            gProfileDir->CreateDir();
    }

    if (!gProfileDir)
        return PR_FALSE;

    outSpec = *gProfileDir;
    return PR_TRUE;
} // GetProfileDirectory


//----------------------------------------------------------------------------------------
static void GetDefaultUserProfileRoot(nsFileSpec& outSpec)
// UNIX    : ~/.mozilla/
// WIN    : <Application Data folder on user's machine>\Mozilla\Users50  
// Mac    : :Documents:Mozilla:Users50:
//----------------------------------------------------------------------------------------
{
#if defined(XP_MAC)
    nsSpecialSystemDirectory cwd(nsSpecialSystemDirectory::Mac_DocumentsDirectory);
    cwd += "Mozilla";
    if (!cwd.Exists())
        cwd.CreateDir();

    cwd += "Users50";
    if (!cwd.Exists())
        cwd.CreateDir();
#elif defined(XP_UNIX)  
    nsSpecialSystemDirectory cwd(nsSpecialSystemDirectory::Unix_HomeDirectory);
    cwd += ".mozilla";
    if (!cwd.Exists())
        cwd.CreateDir();
#elif defined(XP_OS2)
    nsSpecialSystemDirectory cwd(nsSpecialSystemDirectory::OS2_OS2Directory);

    cwd += "Mozilla";
    if (!cwd.Exists())
        cwd.CreateDir();

    cwd += "Users50";
    if (!cwd.Exists())
        cwd.CreateDir();
#elif defined(XP_PC)
    // set its directory an aunt of the moz bin directory
    nsSpecialSystemDirectory cwd(nsSpecialSystemDirectory::Win_Appdata);

    // (some) Win95 machines are not returning anything back for Appdata
    // Give them all the windows directory.
    if (!(cwd.Exists()))
        cwd = nsSpecialSystemDirectory::Win_WindowsDirectory;

    cwd += "Mozilla";
    if (!cwd.Exists())
        cwd.CreateDir();

    cwd += "Users50";
    if (!cwd.Exists())
        cwd.CreateDir();

#elif defined(XP_BEOS)
    nsSpecialSystemDirectory cwd(nsSpecialSystemDirectory::BeOS_SettingsDirectory);
    cwd += "mozilla";
    if (!cwd.Exists())
        cwd.CreateDir();

#else
#error dont_know_how_to_do_profiles_on_your_platform
#endif
    outSpec = cwd;
} // GetDefaultUserProfileRoot


//----------------------------------------------------------------------------------------
static void GetDefaultsFolder(nsFileSpec& outSpec)
//----------------------------------------------------------------------------------------
{
    nsSpecialSystemDirectory cwd(nsSpecialSystemDirectory::Moz_BinDirectory);

#if defined(XP_MAC)
    cwd += "Defaults";
#else
    cwd += "defaults";
#endif

    outSpec = cwd;
} // GetDefaultsFolder

//----------------------------------------------------------------------------------------
static void GetProfileDefaultsFolder_nloc(nsFileSpec& outSpec)
//----------------------------------------------------------------------------------------
{
    nsFileSpec cwd;
    GetDefaultsFolder(cwd);

#if defined(XP_MAC)
    cwd += "Profile";
#else
    cwd += "profile";
#endif

    outSpec = cwd;
    
} // GetProfileDefaultsFolder_nloc

//----------------------------------------------------------------------------------------
static void GetProfileDefaultsFolder(nsFileSpec& outSpec)
//----------------------------------------------------------------------------------------
{
    nsFileSpec cwd;
    GetDefaultsFolder(cwd);

#if defined(XP_MAC)
    cwd += "Profile";
#else
    cwd += "profile";
#endif

    nsXPIDLString lc_name;
    nsresult rv = GetChromeLocale(getter_Copies(lc_name));
    if (NS_SUCCEEDED(rv)) {
        nsFileSpec tmpdir; 
        tmpdir = cwd;
        tmpdir += nsAutoString(NS_STATIC_CAST(const PRUnichar*, lc_name));

        if (tmpdir.Exists())
            cwd = tmpdir;
    }
    outSpec = cwd;
    
} // GetProfileDefaultsFolder

//----------------------------------------------------------------------------------------
static void GetPrefDefaultsFolder(nsFileSpec& outSpec)
//----------------------------------------------------------------------------------------
{
    nsFileSpec cwd;
    GetDefaultsFolder(cwd);

#if defined(XP_MAC)
    cwd += "Pref";
#else
    cwd += "pref";
#endif

    outSpec = cwd;
} // GetProfileDefaultsFolder


//========================================================================================
// Implementation of nsSpecialFileSpec
//========================================================================================

//----------------------------------------------------------------------------------------
nsSpecialFileSpec::nsSpecialFileSpec(Type aType)
//----------------------------------------------------------------------------------------
:    nsFileSpec((const char*)nsnull)
{
    *this = aType;
}

//----------------------------------------------------------------------------------------
nsSpecialFileSpec::~nsSpecialFileSpec()
//----------------------------------------------------------------------------------------
{
}

//----------------------------------------------------------------------------------------
void nsSpecialFileSpec::operator = (Type aType)
//----------------------------------------------------------------------------------------
{
    *this = (const char*)nsnull;
    
    switch (aType)
    {
        case App_PrefsDirectory30:
        case App_PrefsDirectory40:
            NS_NOTYETIMPLEMENTED("Write me!");
            break;    
        case App_PrefsDirectory50:
            {
                *this = nsSpecialFileSpec(App_UserProfileDirectory50);
                break;
            }
        case App_ShrimpDirectory:
            {
                *this = nsSpecialSystemDirectory(nsSpecialSystemDirectory::OS_CurrentProcessDirectory);
                *this += "NIMUser60";
                break;
            }        
        case App_ResDirectory:
            {
                *this = nsSpecialSystemDirectory(nsSpecialSystemDirectory::OS_CurrentProcessDirectory);
#ifdef XP_MAC
                *this += "Res";
#else
                *this += "res";
#endif
            }
            break;

        case App_ComponentsDirectory:
            {
                *this = nsSpecialSystemDirectory(nsSpecialSystemDirectory::OS_CurrentProcessDirectory);
#ifdef XP_MAC
                *this += "Components";
#else
                *this += "components";
#endif
            }
            break;
        case App_ChromeDirectory:
            {
                *this = nsSpecialSystemDirectory(nsSpecialSystemDirectory::Moz_BinDirectory);
#ifdef XP_MAC
                *this += "Chrome";
#else
                *this += "chrome";
#endif
            }
            break;
        case App_PluginsDirectory:
            {
                *this = nsSpecialSystemDirectory(nsSpecialSystemDirectory::OS_CurrentProcessDirectory);
#ifdef XP_MAC
                *this += "Plugins";
#else
                *this += "plugins";
#endif
            }
            break;

        case App_UserChromeDirectory:
            {
                *this = nsSpecialFileSpec(App_UserProfileDirectory50);
                *this += "Chrome";
                break;
            }
            break;    

        case App_DefaultsFolder50:
            GetDefaultsFolder(*this);
            break;
        case App_UserProfileDirectory30:
        case App_UserProfileDirectory40:
            NS_NOTYETIMPLEMENTED("Write me!");
            break;    
        case App_UserProfileDirectory50:
            if (!GetProfileDirectory(*this))
                mError = NS_ERROR_NOT_INITIALIZED;
            break;    
     
        case App_DefaultUserProfileRoot30:
        case App_DefaultUserProfileRoot40:
            NS_NOTYETIMPLEMENTED("Write me!");
            break;    
        case App_DefaultUserProfileRoot50:
            GetDefaultUserProfileRoot(*this);
            break;    

        case App_ProfileDefaultsFolder30:
        case App_ProfileDefaultsFolder40:
            NS_NOTYETIMPLEMENTED("Write me!");
            break;    
        case App_ProfileDefaultsFolder50:
            GetProfileDefaultsFolder(*this);
            break; 
        case App_ProfileDefaultsFolder50_nloc:
            GetProfileDefaultsFolder_nloc(*this);
            break; 
        case App_PrefDefaultsFolder50:
            GetPrefDefaultsFolder(*this);
            break;    
            
            
        case App_PreferencesFile30:
            {
                *this = nsSpecialFileSpec(App_PrefsDirectory30);
            #ifdef XP_MAC
                *this += "Netscape Preferences";
            #elif defined(XP_UNIX) || defined(XP_BEOS)
                *this += "preferences.js";
            #else
                *this += "prefs.js";
            #endif
            }
            break;
        case App_PreferencesFile40:
            {
                *this = nsSpecialFileSpec(App_PrefsDirectory40);
            #ifdef XP_MAC
                *this += "Netscape Preferences";
            #elif defined(XP_UNIX) || defined(XP_BEOS)
                *this += "preferences.js";
            #else
                *this += "prefs.js";
            #endif
            }
            break;    
        case App_PreferencesFile50:
            {
                *this = nsSpecialFileSpec(App_PrefsDirectory50);
                *this += "prefs.js";
                break;
            }
            break;    
        
        case App_BookmarksFile30:
            #ifdef XP_MAC
            {
                // This is possibly correct on all platforms
                *this = nsSpecialFileSpec(App_PrefsDirectory30);
                *this += "Bookmarks.html";
                break;
            }
            #endif
        case App_BookmarksFile40:
            #ifdef XP_MAC
            {
                // This is possibly correct on all platforms
                *this = nsSpecialFileSpec(App_PrefsDirectory40);
                *this += "Bookmarks.html";
                break;
            }
            #endif
        case App_BookmarksFile50:
            {
                *this = nsSpecialFileSpec(App_UserProfileDirectory50);
                *this += "bookmarks.html";
                break;
            }
            break;    
        
        case App_Registry40:
            #ifdef XP_MAC
            {
                *this = nsSpecialFileSpec(App_PrefsDirectory30);
                *this += "Netscape Registry";
                break;
            }
            #endif
        case App_Registry50:
            #ifdef XP_MAC
            {
                *this = nsSpecialFileSpec(App_PrefsDirectory30);
                *this += "Mozilla Registry";
                break;
            }
            #endif
        
            NS_NOTYETIMPLEMENTED("Write me!");
            break;    

    case App_LocalStore50:
            {
                *this = nsSpecialFileSpec(App_UserProfileDirectory50);
                *this += "localstore.rdf";
                break;
            }
            break;    
    case App_History50:
        {
        *this = nsSpecialFileSpec(App_UserProfileDirectory50);
                *this += "history.dat";
                break;
            }
        break;
        case App_MailDirectory50:
            {
                *this = nsSpecialFileSpec(App_UserProfileDirectory50);
                *this += NEW_MAIL_DIR_NAME;
                break;
            }
            break;
        case App_ImapMailDirectory50:
            {
                *this = nsSpecialFileSpec(App_UserProfileDirectory50);
                *this += NEW_IMAPMAIL_DIR_NAME;
                break;
            }
            break;
        case App_NewsDirectory50:
            {
                *this = nsSpecialFileSpec(App_UserProfileDirectory50);
                *this += NEW_NEWS_DIR_NAME;
                break;
            }
            break;
    case App_MessengerFolderCache50:
            {
                *this = nsSpecialFileSpec(App_UserProfileDirectory50);
                *this += "panacea.dat";
                break;
            }
            break;
	case App_UsersMimeTypes50:
        {
        *this = nsSpecialFileSpec(App_UserProfileDirectory50);
        *this += "mimeTypes.rdf";

        if (!(this->Exists())) {
            // find the default mimeTypes.rdf file
            // something like bin/defaults/profile/mimeTypes.rdf
            nsFileSpec defaultMimeTypesFile;
            GetProfileDefaultsFolder(defaultMimeTypesFile);
            defaultMimeTypesFile += "mimeTypes.rdf";

            // get the users profile directory
            *this = nsSpecialFileSpec(App_UserProfileDirectory50);
            
            // copy the default mimeTypes.rdf to <profile>/mimeTypes.rdf
            nsresult rv = defaultMimeTypesFile.CopyToDir(*this);
            NS_ASSERTION(NS_SUCCEEDED(rv), "failed to copy mimeTypes.rdf");
            if (NS_SUCCEEDED(rv)) {
                // set this to <profile>/mimeTypes.rdf
                *this += "mimeTypes.rdf";
            }
        }
        break;
        }
            break;
    case App_UsersPanels50:
        {
        *this = nsSpecialFileSpec(App_UserProfileDirectory50);
        *this += "panels.rdf";

        if (!(this->Exists())) {
            // find the default panels.rdf file
            // something like bin/defaults/profile/panels.rdf
            nsFileSpec defaultPanelsFile;
            GetProfileDefaultsFolder(defaultPanelsFile);
            defaultPanelsFile += "panels.rdf";

            // get the users profile directory
            *this = nsSpecialFileSpec(App_UserProfileDirectory50);
            
            // copy the default panels.rdf to <profile>/panels.rdf
            nsresult rv = defaultPanelsFile.CopyToDir(*this);
            NS_ASSERTION(NS_SUCCEEDED(rv), "failed to copy panels.rdf");
            if (NS_SUCCEEDED(rv)) {
                // set this to <profile>/panels.rdf
                *this += "panels.rdf";
            }
        }
        break;
        }
            break;
    case App_SearchFile50:
        {
        *this = nsSpecialFileSpec(App_UserProfileDirectory50);
        *this += "search.rdf";

        if (!(this->Exists())) {
            // find the default search.rdf file
            // something like bin/defaults/profile/search.rdf
            nsFileSpec defaultPanelsFile;
            GetProfileDefaultsFolder(defaultPanelsFile);
            defaultPanelsFile += "search.rdf";

            // get the users profile directory
            *this = nsSpecialFileSpec(App_UserProfileDirectory50);
            
            // copy the default search.rdf to <profile>/search.rdf
            nsresult rv = defaultPanelsFile.CopyToDir(*this);
            NS_ASSERTION(NS_SUCCEEDED(rv), "failed to copy search.rdf");
            if (NS_SUCCEEDED(rv)) {
                // set this to <profile>/search.rdf
                *this += "search.rdf";
            }
        }
        break;
        }
            break;
    case App_SearchDirectory50:
        {
                *this = nsSpecialSystemDirectory(nsSpecialSystemDirectory::OS_CurrentProcessDirectory);
#ifdef XP_MAC
                *this += "Search Plugins";
#else
                *this += "searchplugins";
#endif
        }
            break;
        case App_DirectoryBase:
        case App_FileBase:
        default:
            NS_ERROR("Invalid location type");
            break;    
    }
} // nsSpecialFileSpec::operator =

#ifdef XP_MAC
#pragma export off
#endif

//========================================================================================
// Implementation of nsIFileLocator
//========================================================================================

//========================================================================================
//----------------------------------------------------------------------------------------
nsFileLocator::nsFileLocator()
//----------------------------------------------------------------------------------------
{
  NS_INIT_REFCNT();
    
  if (gRegisteredWithDirService == 0)
  {
      PR_AtomicIncrement(&gRegisteredWithDirService);
      new nsFileLocationProvider();
  }

}
//----------------------------------------------------------------------------------------
nsFileLocator::~nsFileLocator()
//----------------------------------------------------------------------------------------
{
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsFileLocator, nsIFileLocator);

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileLocator::GetFileLocation(
      PRUint32 aType,
      nsIFileSpec** outSpec)
//----------------------------------------------------------------------------------------
{
    if (!outSpec)
        return NS_ERROR_NULL_POINTER;

   nsFileSpec spec;
   if (aType < nsSpecialFileSpec::App_DirectoryBase)
   {
       *(nsSpecialSystemDirectory*)outSpec
           = (nsSpecialSystemDirectory::SystemDirectories)aType;
       return NS_OK;
   }
   *(nsSpecialFileSpec*)&spec = (nsSpecialFileSpec::Type)aType;
   return NS_SUCCEEDED(spec.Error()) ? NS_NewFileSpecWithSpec(spec, outSpec) : NS_ERROR_ILLEGAL_VALUE;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileLocator::ForgetProfileDir()
//----------------------------------------------------------------------------------------
{
    if (gProfileDir) {
        delete gProfileDir;
        gProfileDir = nsnull;
        
        nsresult rv;
        NS_WITH_SERVICE(nsIProperties, directoryService, NS_DIRECTORY_SERVICE_PROGID, &rv);
        if (NS_FAILED(rv)) return rv;
    
        directoryService->Undefine("app.profile.user.directory.5"); 
        directoryService->Undefine("app.profile.default.user.directory.5"); 
        directoryService->Undefine("app.profile.defaults.directory.5"); 
    }

    return NS_OK;
}




nsFileLocationProvider::nsFileLocationProvider()
{
  NS_INIT_REFCNT();

  nsresult rv;
  NS_WITH_SERVICE(nsIDirectoryService, dirService, NS_DIRECTORY_SERVICE_PROGID, &rv);

  if (dirService)
      dirService->RegisterProvider( NS_STATIC_CAST(nsIDirectoryServiceProvider*, this) );
}

nsFileLocationProvider::~nsFileLocationProvider()
{
}


NS_IMPL_THREADSAFE_ISUPPORTS1(nsFileLocationProvider, nsIDirectoryServiceProvider);

/* MapNameToEnum
 * maps name from the directory table to its enum */
PRInt32 
static MapNameToEnum(const char* name)
{
    int i = 0;

    if ( !name )
        return -1;

    while ( DirectoryTable[i].directoryName[0] != 0 )
    {
        if ( strcmp(DirectoryTable[i].directoryName, name) == 0 )
            return DirectoryTable[i].folderEnum;
        i++;
    }
    return -1;
}
NS_IMETHODIMP
nsFileLocationProvider::GetFile(const char *prop, PRBool *persistant, nsIFile **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);

    *persistant = PR_TRUE;
    nsFileSpec spec;
    PRInt32 value = MapNameToEnum(prop);
    if (value == -1)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsILocalFile> localFile;
    nsresult res;

    if (value < nsSpecialFileSpec::App_DirectoryBase)
    {
       nsSpecialSystemDirectory ssd = (nsSpecialSystemDirectory::SystemDirectories)value;
       res = NS_FileSpecToIFile(&ssd, getter_AddRefs(localFile));
    }
    else
    {
        nsSpecialFileSpec sfs = (nsSpecialFileSpec::Type)value;
        res = NS_FileSpecToIFile(&sfs, getter_AddRefs(localFile));
    }
     
    if (localFile && NS_SUCCEEDED(res))
        return localFile->QueryInterface(NS_GET_IID(nsIFile), (void**)_retval);

    return NS_ERROR_FAILURE;
}




