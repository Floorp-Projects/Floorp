/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributors:
 *     John R. McMullen <mcmullen@netscape.com>
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
#include <windows.h>
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

static NS_DEFINE_IID(kIFileLocatorIID, NS_IFILELOCATOR_IID);

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
    //static nsFileSpec* gProfileDir = nsnull;
        // pointer so that we can detect whether it has been initialized
    if (!gProfileDir)
    {
        // First time, initialize gProfileDir
        nsresult rv;
        NS_WITH_SERVICE(nsIProfile, profileService, kProfileCID, &rv);
        if (NS_FAILED(rv))
            return PR_FALSE;
        
        profileService->Startup(nsnull);
        int numProfiles = 0;
        profileService->GetProfileCount(&numProfiles);
        if (numProfiles == 0)
            return PR_FALSE;

        char* currProfileName = nsnull;
        nsFileSpec currProfileDirSpec;
        if (numProfiles == 1)
        {
            // one profile exists: use that profile
            profileService->GetSingleProfile(&currProfileName);
            profileService->GetProfileDir(currProfileName, &currProfileDirSpec);
        }
	    else
	    {
		    // multiple profiles exist: we'll use the same profile as last time 
		    // (see following rules) 
		    // (if we can't figure out what the last profile used was for some reason, 
		    // we'll pick the first one as returned from the registry query) 
	        profileService->GetCurrentProfile(&currProfileName);
	        if (currProfileName)
	            profileService->GetProfileDir(currProfileName, &currProfileDirSpec);
	        else
	        {
	            profileService->GetFirstProfile(&currProfileName);
	            profileService->GetProfileDir(currProfileName, &currProfileDirSpec);
	        }
	    }
#if defined(NS_DEBUG)
	    if (currProfileName)
	    {
	        printf("ProfileName : %s\n", currProfileName);
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
        }
        NS_ASSERTION(*gProfileDir == currProfileDirSpec, "Profile spec does not match!");
    }
    if (!gProfileDir)
        return PR_FALSE;

    if (!gProfileDir->Exists())
        gProfileDir->CreateDir();
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
    // That's "program files\Netscape\Communicator\Program"
    nsFileSpec parent;
    cwd.GetParent(parent); // "program files\Netscape\Communicator"
    parent.GetParent(cwd); // "program files\Netscape\"

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
static void GetProfileDefaultsFolder(nsFileSpec& outSpec)
//----------------------------------------------------------------------------------------
{
    nsSpecialSystemDirectory cwd(nsSpecialSystemDirectory::OS_CurrentProcessDirectory);

#if defined(XP_MAC)
    cwd += "Defaults";
#else
    cwd += "defaults";
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
            NS_NOTYETIMPLEMENTED("Write me!");
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

static NS_DEFINE_IID(kIFactoryIID,         NS_IFACTORY_IID);

//========================================================================================
class nsFileLocator : public nsIFileLocator
//========================================================================================
{
public:
  nsFileLocator();
  
  NS_DECL_ISUPPORTS

  NS_IMETHOD GetFileLocation(
      PRUint32 aType,
      	// NOTE: actually either nsSpecialFileSpec:Type, see nsFileLocations.h
      	// or nsSpecialSystemDirectory::SystemDirectories, see nsSpecialSystemDirectory.h
      nsIFileSpec** outSpec);

  NS_IMETHOD ForgetProfileDir();

protected:
  virtual ~nsFileLocator();

};


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

NS_IMPL_ISUPPORTS(nsFileLocator, kIFileLocatorIID);

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
   return NS_SUCCEEDED(spec.Error()) ? NS_NewFileSpecWithSpec(spec, outSpec) : nsnull;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsFileLocator::ForgetProfileDir()
//----------------------------------------------------------------------------------------
{

	delete gProfileDir;
	gProfileDir = nsnull;

	return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_EXPORT nsresult NS_NewFileLocator(nsIFileLocator** aResult)
//----------------------------------------------------------------------------------------
{
  if (nsnull == aResult)
    return NS_ERROR_NULL_POINTER;

  *aResult = new nsFileLocator();
  if (nsnull == *aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}

//========================================================================================
class nsFileLocatorFactory : public nsIFactory
//========================================================================================
{
public:
  nsFileLocatorFactory();
  NS_DECL_ISUPPORTS

  // nsIFactory methods
  NS_IMETHOD CreateInstance(nsISupports *aOuter,
                            const nsIID &aIID,
                            void **aResult);
  
  NS_IMETHOD LockFactory(PRBool aLock);  
protected:
  virtual ~nsFileLocatorFactory();
};

//----------------------------------------------------------------------------------------
nsFileLocatorFactory::nsFileLocatorFactory()
//----------------------------------------------------------------------------------------
{
  NS_INIT_REFCNT();
}

//----------------------------------------------------------------------------------------
nsresult nsFileLocatorFactory::LockFactory(PRBool /*lock*/)
//----------------------------------------------------------------------------------------
{
  return NS_OK;
}

//----------------------------------------------------------------------------------------
nsFileLocatorFactory::~nsFileLocatorFactory()
//----------------------------------------------------------------------------------------
{
  NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
}

NS_IMPL_ISUPPORTS(nsFileLocatorFactory, kIFactoryIID);

//----------------------------------------------------------------------------------------
nsresult nsFileLocatorFactory::CreateInstance(nsISupports *aOuter,
                                  const nsIID &aIID,
                                  void **aResult)
//----------------------------------------------------------------------------------------
{
    nsresult rv;
    nsFileLocator* inst;

    if (!aResult)
        return NS_ERROR_NULL_POINTER;

    *aResult = NULL;
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    NS_NEWXPCOM(inst, nsFileLocator);
    if (!inst)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(inst);
    rv = inst->QueryInterface(aIID, aResult);
    NS_RELEASE(inst);

    return rv;
}


//----------------------------------------------------------------------------------------
extern "C" NS_APPSHELL nsresult NS_NewFileLocatorFactory(nsIFactory** aFactory)
//----------------------------------------------------------------------------------------
{
    nsresult rv = NS_OK;
    nsIFactory* inst = new nsFileLocatorFactory();
    if (!inst)
        rv = NS_ERROR_OUT_OF_MEMORY;
    else
        NS_ADDREF(inst);

    *aFactory = inst;
    return rv;
}
