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

#include "plstr.h"
#include "prenv.h"
#include "prmem.h"

// for profile manager
static NS_DEFINE_CID(kProfileCID,           NS_PROFILE_CID);

// Global variable for gProfileDir
static nsFileSpec* gProfileDir = nsnull;

#ifdef XP_MAC
#pragma export on
#endif
//========================================================================================
// Static functions that ain't nobody else's business.
//========================================================================================

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
        nsFileSpec currProfileDirSpec;
        if (numProfiles == 1)
        {
            // one profile exists: use that profile
            profileService->GetFirstProfile(&currProfileName);
            if (currProfileName && (nsCRT::strlen(currProfileName) > 0)) {
                profileService->GetProfileDir(currProfileName, &currProfileDirSpec);
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
                profileService->GetProfileDir(currProfileName, &currProfileDirSpec);
            }
            else
            {
                profileService->GetFirstProfile(&currProfileName);
                if (!currProfileName || (nsCRT::strlen(currProfileName) == 0)) {
                    profileService->GetProfileDir(currProfileName, &currProfileDirSpec);
                }
            }
        }
        if (!currProfileName || (nsCRT::strlen(currProfileName) == 0)) {
            // we don't have it, return false.
            PR_FREEIF(currProfileName);
            return PR_FALSE;
        }    
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
        if (NS_FAILED(profileService->GetCurrentProfileDir(gProfileDir)))
        {
            delete gProfileDir; // All that for nothing.  sigh.
            gProfileDir = nsnull;
            return PR_FALSE;
        }
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
// WIN    : Program Files\Netscape\Users50\  
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

#elif defined(XP_PC)
    // set its directory an aunt of the executable.
    nsSpecialSystemDirectory cwd(nsSpecialSystemDirectory::OS_CurrentProcessDirectory);

    // Users50 directory is kept 1 level above the executable directory.
    cwd.GetParent(cwd); 

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
                *this = nsSpecialSystemDirectory(nsSpecialSystemDirectory::OS_CurrentProcessDirectory);
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
                *this += "Mail";
                break;
            }
            break;
        case App_ImapMailDirectory50:
            {
                *this = nsSpecialFileSpec(App_UserProfileDirectory50);
                *this += "ImapMail";
                break;
            }
            break;
        case App_NewsDirectory50:
            {
                *this = nsSpecialFileSpec(App_UserProfileDirectory50);
                *this += "News";
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
}

//----------------------------------------------------------------------------------------
nsFileLocator::~nsFileLocator()
//----------------------------------------------------------------------------------------
{
}

NS_IMPL_ISUPPORTS1(nsFileLocator, nsIFileLocator);

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
	}

	return NS_OK;
}
