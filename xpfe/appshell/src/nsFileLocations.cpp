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

#include "nsFileSpec.h"

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

#ifdef XP_MAC
#pragma export on
#endif
//========================================================================================
// Static functions that ain't nobody else's business.
//========================================================================================

//----------------------------------------------------------------------------------------
static void CreateDefaultProfileDirectorySpec(nsFileSpec& outSpec)
// to these. For now I am using these until the profile stuff picks up and
// we know how to get the absolute fallback for all platforms
// UNIX    : ~/.mozilla
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
    cwd += "Default";
#elif defined(XP_UNIX)
        // ~/.mozilla
    nsSpecialSystemDirectory cwd(nsSpecialSystemDirectory::Unix_HomeDirectory);
    cwd += ".mozilla";
#else
    // set its directory an aunt of the executable.
    nsSpecialSystemDirectory cwd(nsSpecialSystemDirectory::OS_CurrentProcessDirectory);
    // That's "program files\Netscape\Communicator\Program"
    nsFileSpec parent;
    cwd.GetParent(parent); // "program files\Netscape\Communicator"
    parent.GetParent(cwd); // "program files\Netscape\"
    cwd += "Users50";
    if (!cwd.Exists())
        cwd.CreateDir();
    cwd += "Default";
#endif
    outSpec = cwd;
} // CreateDefaultProfileDirectorySpec

//----------------------------------------------------------------------------------------
static void GetProfileDirectory(nsFileSpec& outSpec)
// The app profile directory comes from the profile manager.
// Once the profile manager knows which profile needs to be
// accessed it tells us about the directory. 

// And if the profile manager doesn't return anything, we use the routine above,
// CreateDefaultProfileDirectorySpec() above.
//----------------------------------------------------------------------------------------
{
    static nsFileSpec* gProfileDir = nsnull;
        // pointer so that we can detect whether it has been initialized
    if (!gProfileDir)
    {
        // First time, initialize gProfileDir
        nsIProfile *profileService = nsnull;
        gProfileDir = new nsFileSpec;
        if (!gProfileDir)
            return;
        nsresult rv = nsServiceManager::GetService(
            kProfileCID, 
            nsIProfile::GetIID(), 
            (nsISupports **)&profileService);
        if (NS_SUCCEEDED(rv))
        {
            char* currProfileName = nsnull;
            nsFileSpec currProfileDirSpec;
            
            profileService->Startup(nsnull);
            int numProfiles = 0;
            profileService->GetProfileCount(&numProfiles);
            if (numProfiles == 0)
            {    
		        // no profiles exist: create "default" profile
		        CreateDefaultProfileDirectorySpec(currProfileDirSpec);
	            profileService->SetProfileDir("default", currProfileDirSpec);
	            currProfileName = PL_strdup("default");
	        }
	        else if (numProfiles == 1)
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
		        {
		            profileService->GetProfileDir(currProfileName, &currProfileDirSpec);
		        }
		        else
		        {
		            profileService->GetFirstProfile(&currProfileName);
		            profileService->GetProfileDir(currProfileName, &currProfileDirSpec);
		        }
		    }

		    if (currProfileName)
		    {
#if defined(XP_PC) && defined(NS_DEBUG)
		        // Don't print in release version. Don't use stderr in XP code.
		        fprintf(stderr, "ProfileName : %s\n", currProfileName);
		        fprintf(stderr, "ProfileDir  : %s\n", currProfileDirSpec.GetCString());
#endif
		        PR_FREEIF(currProfileName);
            }
            if (!currProfileDirSpec.Exists())
                currProfileDirSpec.CreateDir();

            if (NS_FAILED(profileService->GetCurrentProfileDir(gProfileDir)))
            {
                delete gProfileDir; // All that for nothing.  sigh.
                gProfileDir = nsnull;
            }
                
            nsServiceManager::ReleaseService(kProfileCID, profileService);
        }
        if (!gProfileDir)
        {
            gProfileDir = new nsFileSpec;
            if (gProfileDir)
                CreateDefaultProfileDirectorySpec(*gProfileDir);
        }
        if (gProfileDir && !gProfileDir->Exists())
            gProfileDir->CreateDir();
    }
    if (gProfileDir)
    {
        outSpec = *gProfileDir;
    }
} // GetProfileDirectory

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
        
        case App_UserProfileDirectory30:
        case App_UserProfileDirectory40:
            NS_NOTYETIMPLEMENTED("Write me!");
            break;    
        case App_UserProfileDirectory50:
            GetProfileDirectory(*this);
            break;    
     
            
        case App_PreferencesFile30:
            {
                *this = nsSpecialFileSpec(App_PrefsDirectory30);
            #ifdef XP_MAC
                *this += "Netscape Preferences";
            #elif defined(XP_UNIX)
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
            #elif defined(XP_UNIX)
                *this += "preferences.js";
            #else
                *this += "prefs.js";
            #endif
            }
            break;    
        case App_PreferencesFile50:
            {
                *this = nsSpecialFileSpec(App_PrefsDirectory50);
                *this += "prefs50.js";
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
      PRUint32 aType, // NOTE: actually nsSpecialFileSpec:Type, see nsFileLocations.h
      nsFileSpec* outSpec);

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
      nsFileSpec* outSpec)
//----------------------------------------------------------------------------------------
{
    if (!outSpec)
        return NS_ERROR_NULL_POINTER;

   if (aType < nsSpecialFileSpec::App_DirectoryBase)
   {
       *(nsSpecialSystemDirectory*)outSpec
           = (nsSpecialSystemDirectory::SystemDirectories)aType;
       return NS_OK;
   }
   *(nsSpecialFileSpec*)outSpec = (nsSpecialFileSpec::Type)aType;
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
