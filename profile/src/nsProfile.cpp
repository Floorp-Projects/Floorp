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
#include "nsFileSpec.h"
#include "nsString.h"
#include "nsIFileLocator.h"
#include "nsFileLocations.h"
#include "nsEscape.h"

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

static char gNewProfileData[20][_MAX_LENGTH] = {'\0', '\0'};
static int g_Count = 0;

extern "C" NS_EXPORT nsresult
NS_RegistryGetFactory(const nsCID &cid, nsISupports* servMgr, nsIFactory** aFactory );


// profile manager IID and CID
static NS_DEFINE_IID(kIProfileIID, NS_IPROFILE_IID);
static NS_DEFINE_CID(kProfileCID, NS_PROFILE_CID);
static NS_DEFINE_IID(kIFileLocatorIID, NS_IFILELOCATOR_IID);

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kFileLocatorCID, NS_FILELOCATOR_CID);
static NS_DEFINE_CID(kRegistryCID, NS_REGISTRY_CID);

class nsProfile: public nsIProfile {
  NS_DECL_ISUPPORTS

private:
  static void useDefaultProfileFile(nsProfile *aProfileInst);
  static nsProfile *mInstance;
  nsIRegistry* m_reg;

public:
    nsProfile();
    virtual ~nsProfile();
    static nsProfile *GetInstance();

      void     CreateUserDirectories(const nsFileSpec& profileDir);
      void    SetDataArray(nsString data);
      char*   GetValue(char *name);
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
      // Creators
    NS_IMETHOD CreateNewProfile(char* data);
};

nsProfile* nsProfile::mInstance = nsnull;

static PRInt32 g_InstanceCount = 0;
static PRInt32 g_LockCount = 0;

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
  mInstance = nsnull;
}

nsProfile *nsProfile::GetInstance()
{
  if (mInstance == nsnull) {
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
#if defined(DEBUG_profile)
    printf("ProfileManager (nsProfile) : Startup : Get Registry handle\n");
#endif
    nsresult rv;
    rv = nsComponentManager::CreateInstance(kRegistryCID,
                                            nsnull,
                                            nsIRegistry::GetIID(),
                                            (void **) &m_reg);

    
    return NS_OK;
}

NS_IMETHODIMP nsProfile::Shutdown()
{
  NS_RELEASE(m_reg);
  return NS_OK;
}

/*
 * Getters
 */


// Gets the profiles directory for a given profile
NS_IMETHODIMP nsProfile::GetProfileDir(const char *profileName, nsFileSpec* profileDir)
{
    nsresult rv;
#if defined(DEBUG_profile)
    printf("ProfileManager : GetProfileDir\n");
#endif
    // Check result.
    if ( m_reg != nsnull ) {
        // Latch onto the registry object.
        NS_ADDREF(m_reg);

        // Open it against the input file name.
        rv = m_reg->Open();
        if (NS_FAILED(rv))
        {
#if defined(NS_DEBUG_profile)
            printf("Error opening Registry.\n" );
#endif
            return rv;
        }
#if defined(DEBUG_profile)
        printf("Registry opened OK.\n" );
#endif
        nsIRegistry::Key key;
        rv = m_reg->GetSubtree(nsIRegistry::Common, "Profiles", &key);

        if (NS_FAILED(rv))
        {
#if defined(DEBUG_profile)
            printf("Registry Error OK.\n" );
#endif
            return rv;
        }

#if defined(DEBUG_profile)
        printf("Registry:Profiles opened OK.\n" );
#endif
        nsIRegistry::Key newKey;
        rv = m_reg->GetSubtree(key, profileName, &newKey);

        if (NS_FAILED(rv))
           return rv;

        char* encodedProfileDir = nsnull;
        rv = m_reg->GetString( newKey, "directory", &encodedProfileDir );

        if (NS_FAILED(rv))
        {
#ifdef DEBUG_profile
            printf( "Error");
#endif
            return rv;
        }

        rv = m_reg->SetString(key, "CurrentProfile", profileName);
        if (NS_FAILED(rv))
        {
#ifdef DEBUG_profile
            printf( "Error");
#endif
            return rv;
        }
        nsInputStringStream stream(encodedProfileDir);
        nsPersistentFileDescriptor descriptor;
        stream >> descriptor;
        *profileDir = descriptor;

        PR_FREEIF(encodedProfileDir);
        m_reg->Close();
    }
    return NS_OK;
}


// Gets the number of profiles
// Location: Common/Profiles
NS_IMETHODIMP nsProfile::GetProfileCount(int *numProfiles)
{
    nsresult rv;
#if defined(DEBUG_profile)
    printf("ProfileManager : GetProfileCount\n");
#endif
    // Check result.
    if ( m_reg != nsnull ) {
        // Latch onto the registry object.
        NS_ADDREF(m_reg);

        // Open it against the input file name.
        rv = m_reg->Open();
        if (NS_SUCCEEDED(rv)) {
#if defined(DEBUG_profile)
            printf("Registry opened OK.\n" );
#endif
            // Enumerate all subkeys (immediately) under the given node.
            nsIEnumerator *enumKeys;
            nsIRegistry::Key key;
            rv = m_reg->GetSubtree(nsIRegistry::Common, "Profiles", &key);
            if (NS_SUCCEEDED(rv)) {
                rv = m_reg->EnumerateSubtrees( key, &enumKeys );

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
            m_reg->Close();
        }
    }

    return NS_OK;
}


// Called if only a single profile exists
// and returns the name of the single profile
NS_IMETHODIMP nsProfile::GetSingleProfile(char **profileName)
{
  nsresult rv;
#if defined(DEBUG_profile)
    printf("ProfileManager : GetSingleProfile\n");
#endif
    // Check result.
    if ( m_reg != nsnull ) {
        // Latch onto the registry object.
        NS_ADDREF(m_reg);

        // Open it against the input file name.
        rv = m_reg->Open();
        if (NS_SUCCEEDED(rv)) {
#if defined(DEBUG_profile) 
            printf("Registry opened OK.\n" );
#endif
            // Enumerate all subkeys (immediately) under the given node.
            nsIEnumerator *enumKeys;
            nsIRegistry::Key key;
            rv = m_reg->GetSubtree(nsIRegistry::Common, "Profiles", &key);
            if (NS_SUCCEEDED(rv)) {
                rv = m_reg->EnumerateSubtrees( key, &enumKeys );

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
                            *profileName = (char*) PR_Malloc(sizeof(char)*_MAX_LENGTH);
                            rv = node->GetName( profileName );

                            // Test result.
                            if (NS_SUCCEEDED(rv)) {
                                rv = m_reg->SetString(key, "CurrentProfile", *profileName);
                                // Print name:
#ifdef DEBUG_profile
                                printf( "\t\tProfile name: %s", *profileName);
#endif
                            }
                        }
                    }
                }
            }
            
            m_reg->Close();
        }
    }

    return NS_OK;
}


// Returns the name of the current profile i.e., the last used profile
NS_IMETHODIMP nsProfile::GetCurrentProfile(char **profileName)
{
  nsresult rv;
#if defined(DEBUG_profile) 
  printf("ProfileManager : GetCurrentProfile\n");
#endif
  // Check result.
  if ( m_reg != nsnull ) {
      // Latch onto the registry object.
      NS_ADDREF(m_reg);

      // Open it against the input file name.
      rv = m_reg->Open();
      if (NS_SUCCEEDED(rv)) 
      {
#if defined(DEBUG_profile)
          printf("Registry opened OK.\n" );
#endif
          nsIRegistry::Key key;
          rv = m_reg->GetSubtree(nsIRegistry::Common, "Profiles", &key);
          if (NS_SUCCEEDED(rv)) 
          {            
                *profileName = (char*) PR_Malloc(sizeof(char)*_MAX_LENGTH);

                 rv = m_reg->GetString( key, "CurrentProfile", profileName );
                if (NS_SUCCEEDED(rv)) {
                
                }
                else {
                    profileName = '\0';
                }
          }
          m_reg->Close();
      }
    }
  return NS_OK;
}


//  Returns the name of the first profile in the Registry
NS_IMETHODIMP nsProfile::GetFirstProfile(char **profileName)
{
#if defined(DEBUG_profile)
    printf("ProfileManager : GetFirstProfile\n");
#endif
    GetSingleProfile(profileName);
    return NS_OK;
}


// Returns the name of the current profile directory
NS_IMETHODIMP nsProfile::GetCurrentProfileDir(nsFileSpec* profileDir)
{
#if defined(DEBUG_profile)
    printf("ProfileManager : GetCurrentProfileDir\n");
#endif
    char *profileName;

    GetCurrentProfile(&profileName);
    nsresult rv = GetProfileDir(profileName, profileDir);
    PR_DELETE(profileName);
    
    return rv;

}

/*
 * Setters
 */

// Sets the current profile directory
NS_IMETHODIMP nsProfile::SetProfileDir(const char *profileName, const nsFileSpec& profileDir)
{
    nsresult rv;
#if defined(DEBUG_profile)
    printf("ProfileManager : SetProfileDir\n");
    printf("profileName : %s ", profileName);
    printf("profileDir  : %s\n", profileDir.GetCString());
#endif
    // Check result.
    if (m_reg == nsnull)
        return NS_ERROR_NULL_POINTER;

    // Latch onto the registry object.
    NS_ADDREF(m_reg);

    // Open it against the input file name.
    rv = m_reg->Open();
    if (NS_FAILED(rv))
    {
#if defined(DEBUG_profile) 
        printf("Registry NOT opened OK.\n" );
#endif
        return rv;
    }

#if defined(DEBUG_profile)
    printf("Registry opened OK.\n" );
#endif
    nsIRegistry::Key key;
    rv = m_reg->AddSubtree(nsIRegistry::Common, "Profiles", &key);
    if (NS_FAILED(rv)) 
    {
#if defined(DEBUG_profile)
        printf("Registry Subtree not added.\n" );
#endif
        return rv;
    }

#if defined(DEBUG_profile)
    printf("Registry:Profiles opened OK.\n" );
#endif
    nsIRegistry::Key newKey;
    rv = m_reg->AddSubtree(key, profileName, &newKey);
    if (NS_FAILED(rv))
        return rv;

    rv = m_reg->SetString(key, "CurrentProfile", profileName);
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
        rv = m_reg->SetString(newKey, "directory", profileDirString);
    delete [] profileDirString;

    m_reg->Close();

    return NS_OK;
}


// Creates a new profile
NS_IMETHODIMP nsProfile::CreateNewProfile(char* charData)
{
#if defined(DEBUG_profile)
    printf("ProfileManager : CreateNewProfile\n");
      printf("ProfileManagerData*** : %s\n", charData);
#endif
      nsresult rv;
      nsString data(charData);
      SetDataArray(data);
      char* dirName = GetValue("ProfileDir");
      char* unescapedProfileName = GetValue("ProfileName");
      if (!unescapedProfileName || !*unescapedProfileName)
		  return NS_ERROR_FAILURE;
	  char* profileName = nsEscape(unescapedProfileName, url_Path); // temp hack
      PR_DELETE(unescapedProfileName);
      nsFileSpec dirSpec(dirName);
      if (!dirName || !*dirName)
      {
		  // They didn't type a directory path...
		  nsIFileLocator* locator = nsnull;
		  rv = nsServiceManager::GetService(kFileLocatorCID, kIFileLocatorIID, (nsISupports**)&locator);
		  if (NS_FAILED(rv) || !locator)
		      return NS_ERROR_FAILURE;
          // Get current profile, make the new one a sibling...
          rv = locator->GetFileLocation(nsSpecialFileSpec::App_UserProfileDirectory50, &dirSpec);
          nsServiceManager::ReleaseService(kFileLocatorCID, locator);
		  if (NS_FAILED(rv))
		      return NS_ERROR_FAILURE;
		  dirSpec.SetLeafName(profileName);
      }
#ifdef DEBUG_profile
      printf("before SetProfileDir\n");
#endif
      rv = SetProfileDir(profileName, dirSpec);
#ifdef DEBUG_profile
      printf("after SetProfileDir\n");
#endif
      if (NS_FAILED(rv))
        return rv;

      if (dirName)
          PR_DELETE(dirName);
      delete [] profileName;
#ifdef DEBUG_profile
      printf("SMTP  %s\n", GetValue("SMTP"));
      printf("NNTP  %s\n", GetValue("NNTP"));
      printf("EMAIL %s\n", GetValue("EMAIL"));
#endif
      CreateUserDirectories(dirSpec);
    return NS_OK;
}
void nsProfile::CreateUserDirectories(const nsFileSpec& profileDir)
{
#if defined(DEBUG_profile)
    printf("ProfileManager : CreateUserDirectories\n");
#endif
      nsFileSpec tmpDir;
      tmpDir = profileDir;
      tmpDir += "News";
      if (!tmpDir.Exists())
    {
              tmpDir.CreateDirectory();
      }
      tmpDir = profileDir;
      tmpDir += "Mail";
      if (!tmpDir.Exists())
    {
              tmpDir.CreateDirectory();
      }
      tmpDir = profileDir;
      tmpDir += "Cache";
      if (!tmpDir.Exists())
    {
              tmpDir.CreateDirectory();
      }
}

// Set the data stream into an array
void nsProfile::SetDataArray(nsString data)
{
#if defined(DEBUG_profile)
  printf("ProfileManager : Setting new profile data\n");
  printf("SetDataArray data : %s\n", data.ToNewCString());
#endif
  char *newStr=nsnull;
  char *tokstr = data.ToNewCString();
  char *token = nsCRT::strtok(tokstr, "%", &newStr);
#ifdef DEBUG_profile
  printf("before while loop\n");
#endif
  while (token)
    {
#ifdef DEBUG_profile
      printf("subTok : %s\n", token);
#endif
      PL_strcpy(gNewProfileData[g_Count], token);
      g_Count++;
      
      token = nsCRT::strtok(newStr, "%", &newStr);
    }
  
  delete[] tokstr;
#ifdef DEBUG_profile
  printf("after while loop\n");
#endif
}

char* nsProfile::GetValue(char *name)
{
    int nameLength;
    char* value;
    value = (char *) PR_Malloc(sizeof(char) * _MAX_LENGTH);
    for (int i = 0; i < g_Count; i=i+1) {
        if (gNewProfileData[i]) {
            nameLength = PL_strlen(name);
            if (PL_strncmp(name, gNewProfileData[i], nameLength) == 0) {
                char* equalsPosition = PL_strchr(gNewProfileData[i], '=');
                if (equalsPosition) {
                    PL_strcpy(value, 1 + equalsPosition);
                    return value;
                }
            }
        }
    }
#ifdef DEBUG_profile
    printf("after for loop\n");
#endif
    return nsnull;
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
  if (aDelegate != nsnull) {
    return NS_ERROR_NO_AGGREGATION;
  }

  nsProfile *t = nsProfile::GetInstance();
  
  if (t == nsnull) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  nsresult res = t->QueryInterface(aIID, aResult);
  
  if (NS_FAILED(res)) {
    *aResult = nsnull;
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
  if (aFactory == nsnull) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aClass.Equals(kProfileCID)) {
    nsProfileFactory *factory = new nsProfileFactory();
    nsresult res = factory->QueryInterface(kFactoryIID, (void **) aFactory);
    if (NS_FAILED(res)) {
      *aFactory = nsnull;
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

  rv = compMgr->RegisterComponent(kProfileCID, nsnull, nsnull, path, 
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

  rv = compMgr->UnregisterComponent(kProfileCID, path);

  (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
  return rv;
}

