/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsIProfile.h"

#include "pratom.h"
#include "prmem.h"
#include "nsIRegistry.h"
#include "nsIFactory.h"
#include "nsIComponentManager.h"
#include "nsIEnumerator.h"
#include "plstr.h"

#ifdef XP_PC
#include <direct.h>
#endif

// It is important to include this header file as it contains the
// registry CIDs.
#include "nsXPComCIID.h"

// included for XPlatform coding 
#include "nsFileStream.h"
#include "nsSpecialSystemDirectory.h"

#ifdef XP_MAC
#include "nsINetSupport.h"
#include "nsIStreamListener.h"
#endif
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"

#define _MAX_LENGTH 256

extern "C" NS_EXPORT nsresult
NS_RegistryGetFactory(const nsCID &cid, nsISupports* servMgr, nsIFactory** aFactory );


// profile manager IID and CID
static NS_DEFINE_IID(kIProfileIID, NS_IPROFILE_IID);
static NS_DEFINE_CID(kProfileCID, NS_PROFILE_CID);

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

class nsProfile: public nsIProfile {
  NS_DECL_ISUPPORTS

private:
  static void useDefaultProfileFile(nsProfile *aProfileInst);
  static nsProfile *mInstance;

public:
    nsProfile();
    virtual ~nsProfile();
    static nsProfile *GetInstance();

    // Initialize/shutdown
    NS_IMETHOD Startup(char *registry);
    NS_IMETHOD Shutdown();

    // Getters
    NS_IMETHOD GetProfileDir(const char *profileName, nsFileSpec* profileDir);
    NS_IMETHOD GetProfileCount(int *numProfiles);
    NS_IMETHOD GetSingleProfile(char **profileName);
    NS_IMETHOD GetCurrentProfile(char **profileName);
    NS_IMETHOD GetFirstProfile(char **profileName);
    NS_IMETHOD GetCurrentProfileDir(nsFileSpec* profileDir);

    // Setters
    NS_IMETHOD SetProfileDir(const char *profileName, const nsFileSpec& profileDir);
};

nsProfile* nsProfile::mInstance = NULL;

static PRInt32 g_InstanceCount = 0;
static PRInt32 g_LockCount = 0;
nsIComponentManager *compMgr = NULL;
nsresult rv;
nsIRegistry *reg;

#ifndef XP_UNIX
/* sspitzer:  what is this for?  */
static nsresult _convertRes(int res)
{
  nsresult nsres = NS_OK;

  return nsres;
}
#endif

/*
 * Constructor/Destructor
 */

nsProfile::nsProfile() {
  PR_AtomicIncrement(&g_InstanceCount);
  NS_INIT_REFCNT();
}

nsProfile::~nsProfile() {
  PR_AtomicDecrement(&g_InstanceCount);
  mInstance = NULL;
}

nsProfile *nsProfile::GetInstance()
{
  if (mInstance == NULL) {
    mInstance = new nsProfile();
  }
  return mInstance;
}

/*
 * nsISupports Implementation
 */

NS_IMPL_ISUPPORTS(nsProfile, kIProfileIID);

/*
 * nsIProfile Implementation
 */
// Creates an instance of the mozRegistry
// or any other registry that would be finalized for 5.0
NS_IMETHODIMP nsProfile::Startup(char *filename)
{
#if defined(NS_DEBUG)
    printf("ProfileManager (nsProfile) : Startup : Get Registry handle\n");
#endif
    nsresult rv;
    nsIServiceManager *servMgr = NULL;
    rv = nsServiceManager::GetGlobalServiceManager(&servMgr);

    if (NS_FAILED(rv))
    {
        // Cannot initialize XPCOM
        printf("servMan failed. Exit. [rv=0x%08X]\n", (int)rv);
        exit(-1);
    }

    static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
    static NS_DEFINE_IID(kComponentManagerIID, NS_ICOMPONENTMANAGER_IID);
    rv = servMgr->GetService(kComponentManagerCID, kComponentManagerIID, (nsISupports **)&compMgr);
    if (NS_FAILED(rv))
    {
        // Cant get component manager
        printf("Cannot get component manager from service manager.. Exit. [rv=0x%08X]\n", (int)rv);
        exit(-1);
    }

    static NS_DEFINE_CID(kRegistryCID, NS_REGISTRY_CID);
    static NS_DEFINE_IID(kRegistryIID, NS_IREGISTRY_IID);
    rv = compMgr->CreateInstance(kRegistryCID, NULL, kRegistryIID, (void **) &reg);


    return NS_OK;
}

NS_IMETHODIMP nsProfile::Shutdown()
{
  
  return NS_OK;
}

/*
 * Getters
 */


// Gets the profiles directory for a given profile
NS_IMETHODIMP nsProfile::GetProfileDir(const char *profileName, nsFileSpec* profileDir)
{
#if defined(NS_DEBUG)
    printf("ProfileManager : GetProfileDir\n");
#endif
    // Check result.
    if ( reg != NULL ) {
        // Latch onto the registry object.
        NS_ADDREF(reg);

        // Open it against the input file name.
        rv = reg->Open();
        if (NS_FAILED(rv))
        {
#if defined(NS_DEBUG)
            printf("Error opening Registry.\n" );
#endif
            return rv;
        }
#if defined(NS_DEBUG)
        printf("Registry opened OK.\n" );
#endif
        nsIRegistry::Key key;
        rv = reg->GetSubtree(nsIRegistry::Common, "Profiles", &key);

        if (NS_FAILED(rv))
        {
#if defined(NS_DEBUG)
            printf("Registry Error OK.\n" );
#endif
            return rv;
        }

#if defined(NS_DEBUG) 
        printf("Registry:Profiles opened OK.\n" );
#endif
        nsIRegistry::Key newKey;
        rv = reg->GetSubtree(key, profileName, &newKey);

        if (NS_FAILED(rv))
           return rv;

        char* encodedProfileDir = nsnull;
        rv = reg->GetString( newKey, "directory", &encodedProfileDir );

        if (NS_FAILED(rv))
        {
#ifdef NS_DEBUG
            printf( "Error");
#endif
            return rv;
        }

        rv = reg->SetString(key, "CurrentProfile", profileName);
        if (NS_FAILED(rv))
        {
#ifdef NS_DEBUG
            printf( "Error");
#endif
            return rv;
        }
        nsInputStringStream stream(encodedProfileDir);
        nsPersistentFileDescriptor descriptor;
        stream >> descriptor;
        *profileDir = descriptor;

        PR_FREEIF(encodedProfileDir);
        reg->Close();
    }
    return NS_OK;
}


// Gets the number of profiles
// Location: Common/Profiles
NS_IMETHODIMP nsProfile::GetProfileCount(int *numProfiles)
{
#if defined(NS_DEBUG) 
    printf("ProfileManager : GetProfileCount\n");
#endif
    // Check result.
    if ( reg != NULL ) {
        // Latch onto the registry object.
        NS_ADDREF(reg);

        // Open it against the input file name.
        rv = reg->Open();
        if (NS_SUCCEEDED(rv)) {
#if defined(NS_DEBUG)
            printf("Registry opened OK.\n" );
#endif
            // Enumerate all subkeys (immediately) under the given node.
            nsIEnumerator *enumKeys;
            nsIRegistry::Key key;
            rv = reg->GetSubtree(nsIRegistry::Common, "Profiles", &key);
            if (NS_SUCCEEDED(rv)) {
                rv = reg->EnumerateSubtrees( key, &enumKeys );

                if (NS_SUCCEEDED(rv)) {
                    int numKeys=0;
                    rv = enumKeys->First();
                    // Enumerate subkeys till done.
                    while( NS_SUCCEEDED( rv ) && !enumKeys->IsDone() ) 
                    {
                        rv = enumKeys->Next();
                        numKeys++;
                    }
                    *numProfiles = numKeys;
                    NS_RELEASE(enumKeys);
                }
            }
            reg->Close();
        }
    }

    return NS_OK;
}


// Called if only a single profile exists
// and returns the name of the single profile
NS_IMETHODIMP nsProfile::GetSingleProfile(char **profileName)
{
#if defined(NS_DEBUG)
    printf("ProfileManager : GetSingleProfile\n");
#endif
    // Check result.
    if ( reg != NULL ) {
        // Latch onto the registry object.
        NS_ADDREF(reg);

        // Open it against the input file name.
        rv = reg->Open();
        if (NS_SUCCEEDED(rv)) {
#if defined(NS_DEBUG) 
            printf("Registry opened OK.\n" );
#endif
            // Enumerate all subkeys (immediately) under the given node.
            nsIEnumerator *enumKeys;
            nsIRegistry::Key key;
            rv = reg->GetSubtree(nsIRegistry::Common, "Profiles", &key);
            if (NS_SUCCEEDED(rv)) {
                rv = reg->EnumerateSubtrees( key, &enumKeys );

                // Go to beginning.
                rv = enumKeys->First();
                if(NS_SUCCEEDED(rv)&& !enumKeys->IsDone() ) {
                    nsISupports *base;
                    rv = enumKeys->CurrentItem( &base );
                    
                    // Test result.
                    if (NS_SUCCEEDED(rv)) {
                        // Get specific interface.
                        nsIRegistryNode *node;
                        nsIID nodeIID = NS_IREGISTRYNODE_IID;
                        rv = base->QueryInterface( nodeIID, (void**)&node );
                        
                        // Test that result.
                        if (NS_SUCCEEDED(rv)) {
                            // Get node name.
                            *profileName = (char*) malloc(sizeof(char)*_MAX_LENGTH);
                            rv = node->GetName( profileName );

                            // Test result.
                            if (NS_SUCCEEDED(rv)) {
                                rv = reg->SetString(key, "CurrentProfile", *profileName);
                                // Print name:
                                printf( "\t\t%s", *profileName);
                            }
                        }
                    }
                }
            }
            
            reg->Close();
        }
    }

    return NS_OK;
}


// Returns the name of the current profile i.e., the last used profile
NS_IMETHODIMP nsProfile::GetCurrentProfile(char **profileName)
{
#if defined(NS_DEBUG) 
  printf("ProfileManager : GetCurrentProfile\n");
#endif
  // Check result.
  if ( reg != NULL ) {
      // Latch onto the registry object.
      NS_ADDREF(reg);

      // Open it against the input file name.
      rv = reg->Open();
      if (NS_SUCCEEDED(rv)) 
      {
#if defined(NS_DEBUG)
          printf("Registry opened OK.\n" );
#endif
          nsIRegistry::Key key;
          rv = reg->GetSubtree(nsIRegistry::Common, "Profiles", &key);
          if (NS_SUCCEEDED(rv)) 
          {            
                *profileName = (char*) malloc(sizeof(char)*_MAX_LENGTH);

                 rv = reg->GetString( key, "CurrentProfile", profileName );
                if (NS_SUCCEEDED(rv)) {
                
                }
                else {
                    profileName = '\0';
                }
          }
          reg->Close();
      }
    }
  return NS_OK;
}


//  Returns the name of the first profile in the Registry
NS_IMETHODIMP nsProfile::GetFirstProfile(char **profileName)
{
#if defined(NS_DEBUG)
    printf("ProfileManager : GetFirstProfile\n");
#endif
    GetSingleProfile(profileName);
    return NS_OK;
}


// Returns the name of the current profile directory
NS_IMETHODIMP nsProfile::GetCurrentProfileDir(nsFileSpec* profileDir)
{
#if defined(NS_DEBUG)
    printf("ProfileManager : GetCurrentProfileDir\n");
#endif
    char *profileName;

    GetCurrentProfile(&profileName);
    nsresult rv = GetProfileDir(profileName, profileDir);
    free(profileName);
    
    return rv;

}

/*
 * Setters
 */

// Sets the current profile directory
NS_IMETHODIMP nsProfile::SetProfileDir(const char *profileName, const nsFileSpec& profileDir)
{
#if defined(NS_DEBUG)
    printf("ProfileManager : SetProfileDir\n");
    printf("profileName : %s ", profileName);
    printf("profileDir  : %s\n", profileDir.GetCString());
#endif
    // Check result.
    if (reg == nsnull)
        return NS_ERROR_NULL_POINTER;

    // Latch onto the registry object.
    NS_ADDREF(reg);

    // Open it against the input file name.
    rv = reg->Open();
    if (NS_FAILED(rv))
    {
#if defined(NS_DEBUG) 
        printf("Registry NOT opened OK.\n" );
#endif
        return rv;
    }

#if defined(NS_DEBUG)
    printf("Registry opened OK.\n" );
#endif
    nsIRegistry::Key key;
    rv = reg->AddSubtree(nsIRegistry::Common, "Profiles", &key);
    if (NS_FAILED(rv)) 
    {
#if defined(NS_DEBUG)
        printf("Registry Subtree not added.\n" );
#endif
        return rv;
    }

#if defined(NS_DEBUG)
    printf("Registry:Profiles opened OK.\n" );
#endif
    nsIRegistry::Key newKey;
    rv = reg->AddSubtree(key, profileName, &newKey);
    if (NS_FAILED(rv))
        return rv;

    rv = reg->SetString(key, "CurrentProfile", profileName);
    if (NS_FAILED(rv))
        return rv;

    if (!profileDir.Exists())
    {
        // nsPersistentFileDescriptor requires an existing
        // object. Make it first.
        nsFileSpec tmp(profileDir);
        tmp.CreateDirectory();
    }
    nsPersistentFileDescriptor descriptor(profileDir);
    char* profileDirString = nsnull;
    nsOutputStringStream stream(profileDirString);
    stream << descriptor;
    if (profileDirString && *profileDirString)
        rv = reg->SetString(newKey, "directory", profileDirString);
    delete [] profileDirString;

    reg->Close();

    return NS_OK;
}




// Factory
class nsProfileFactory: public nsIFactory {
  NS_DECL_ISUPPORTS
  
  nsProfileFactory() {
    NS_INIT_REFCNT();
    PR_AtomicIncrement(&g_InstanceCount);
  }

  virtual ~nsProfileFactory() {
    PR_AtomicDecrement(&g_InstanceCount);
  }

  NS_IMETHOD CreateInstance(nsISupports *aDelegate,
                            const nsIID &aIID,
                            void **aResult);

  NS_IMETHOD LockFactory(PRBool aLock) {
    if (aLock) {
      PR_AtomicIncrement(&g_LockCount);
    } else {
      PR_AtomicDecrement(&g_LockCount);
    }
    return NS_OK;
  };
};

static NS_DEFINE_IID(kFactoryIID, NS_IFACTORY_IID);

NS_IMPL_ISUPPORTS(nsProfileFactory, kFactoryIID);

nsresult nsProfileFactory::CreateInstance(nsISupports *aDelegate,
                                            const nsIID &aIID,
                                            void **aResult)
{
  if (aDelegate != NULL) {
    return NS_ERROR_NO_AGGREGATION;
  }

  nsProfile *t = nsProfile::GetInstance();
  
  if (t == NULL) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  nsresult res = t->QueryInterface(aIID, aResult);
  
  if (NS_FAILED(res)) {
    *aResult = NULL;
  }

  return res;
}

extern "C" NS_EXPORT nsresult 
NSGetFactory(nsISupports* serviceMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
{
  if (aFactory == NULL) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aClass.Equals(kProfileCID)) {
    nsProfileFactory *factory = new nsProfileFactory();
    nsresult res = factory->QueryInterface(kFactoryIID, (void **) aFactory);
    if (NS_FAILED(res)) {
      *aFactory = NULL;
      delete factory;
    }
    return res;
  }
  return NS_NOINTERFACE;
}

extern "C" NS_EXPORT PRBool NSCanUnload(nsISupports* serviceMgr)
{
  return PRBool(g_InstanceCount == 0 && g_LockCount == 0);
}

extern "C" NS_EXPORT nsresult NSRegisterSelf(nsISupports* aServMgr, const char *path)
{
  nsresult rv;

  nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) return rv;

  nsIComponentManager* compMgr;
  rv = servMgr->GetService(kComponentManagerCID, 
                           nsIComponentManager::GetIID(), 
                           (nsISupports**)&compMgr);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->RegisterComponent(kProfileCID, NULL, NULL, path, 
                                  PR_TRUE, PR_TRUE);

  (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
  return rv;
}

extern "C" NS_EXPORT nsresult NSUnregisterSelf(nsISupports* aServMgr, const char *path)
{
  nsresult rv;

  nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) return rv;

  nsIComponentManager* compMgr;
  rv = servMgr->GetService(kComponentManagerCID, 
                           nsIComponentManager::GetIID(), 
                           (nsISupports**)&compMgr);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->UnregisterFactory(kProfileCID, path);

  (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
  return rv;
}

