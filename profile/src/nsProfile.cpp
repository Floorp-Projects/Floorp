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
#include "nsIPref.h"

#include "pratom.h"
#include "prmem.h"
#include "nsIRegistry.h"
#include "NSReg.h"
#include "nsIFactory.h"
#include "nsIComponentManager.h"
#include "nsIEnumerator.h"
#include "plstr.h"
#include "nsIFileSpec.h"
#include "nsFileSpec.h"
#include "nsString.h"
#include "nsIFileLocator.h"
#include "nsFileLocations.h"
#include "nsEscape.h"
#include "nsIURL.h"

#ifndef NECKO
#include "nsINetService.h"
#else
#include "nsIIOService.h"
#include "nsNeckoUtil.h"
#endif // NECKO


#ifdef XP_PC
#include <direct.h>
#include "nsIPrefMigration.h"
#endif

// included for XPlatform coding 
#include "nsFileStream.h"
#include "nsSpecialSystemDirectory.h"


#ifdef XP_MAC
#ifdef NECKO
#include "nsIPrompt.h"
#else
#include "nsINetSupport.h"
#endif
#include "nsIStreamListener.h"
#endif
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"

#define _MAX_LENGTH			256
#define _MAX_NUM_PROFILES	50

// PREG information
#define PREG_COOKIE			"NS_REG2_PREG"
#define PREG_USERNAME		"PREG_USER_NAME"
#define PREG_DENIAL			"PREG_USER_DENIAL"
#define PREG_URL			"http://seaspace.mcom.com/"


// Globals to hold profile information
#ifdef XP_PC
static char *oldWinReg="nsreg.dat";
#endif

static char gNewProfileData[_MAX_NUM_PROFILES][_MAX_LENGTH] = {{'\0'}};
static int	g_Count = 0;

static char gProfiles[_MAX_NUM_PROFILES][_MAX_LENGTH] = {{'\0'}};
static int	g_numProfiles = 0;

// we only migrate prefs on windows right now.
#ifdef XP_PC
static char gOldProfiles[_MAX_NUM_PROFILES][_MAX_LENGTH] = {{'\0'}};
static char gOldProfLocations[_MAX_NUM_PROFILES][_MAX_LENGTH] = {{'\0'}};
static int	g_numOldProfiles = 0;
#endif 

static PRBool renameCurrProfile = PR_FALSE;

// IID and CIDs of all the services needed
static NS_DEFINE_IID(kIProfileIID, NS_IPROFILE_IID);
static NS_DEFINE_CID(kProfileCID, NS_PROFILE_CID);

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kFileLocatorCID, NS_FILELOCATOR_CID);
static NS_DEFINE_CID(kRegistryCID, NS_REGISTRY_CID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

#ifndef NECKO
static NS_DEFINE_IID(kINetServiceIID, NS_INETSERVICE_IID);
static NS_DEFINE_IID(kNetServiceCID, NS_NETSERVICE_CID);
#else
static NS_DEFINE_IID(kIIOServiceIID, NS_IIOSERVICE_IID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
#endif // NECKO


#ifdef XP_PC
static NS_DEFINE_IID(kIPrefMigration_IID, NS_IPrefMigration_IID);
static NS_DEFINE_CID(kPrefMigration_CID, NS_PrefMigration_CID);
#endif

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

	// Creates associated user directories on the creation of a new profile
    void		CreateUserDirectories(const nsFileSpec& profileDir);

	// Deletes associated user directories
	void		DeleteUserDirectories(const nsFileSpec& profileDir);

	// Sets profile information recived from the Javascript (routed via core service) into an array
    void		SetDataArray(nsString data);

	// Gets a particular value from the DataArray
    char*		GetValue(char *name);

	// Copies all the registry keys from old profile to new profile
	nsresult	CopyRegKey(const char *oldProfile, const char *newProfile);

	// Fills the global array gProfiles by enumerating registry entries
	void		GetAllProfiles();

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

	// General
    NS_IMETHOD ProfileExists(const char *profileName);

	// Migrate 4.x profile information
	NS_IMETHOD MigrateProfileInfo();
	NS_IMETHOD UpdateMozProfileRegistry();
      
	// Profile Core supporters
    NS_IMETHOD CreateNewProfile(char* data);
	NS_IMETHOD RenameProfile(const char* aOldName, const char* aNewName);  
	NS_IMETHOD DeleteProfile(const char* aProfileName, const char *canDeleteFiles);  
	NS_IMETHOD GetProfileList(nsString& profileList);
	NS_IMETHOD StartCommunicator(const char* aProfileName);
	NS_IMETHOD GetCurrProfile(nsString& currProfile);
	NS_IMETHOD MigrateProfile(const char* aProfileName);

	// Cookie processing
	NS_IMETHOD GetCookie(nsString& aCookie);
	NS_IMETHOD ProcessPRegCookie();
	NS_IMETHOD IsPregCookieSet(char **pregSet);
	NS_IMETHOD ProcessPREGInfo(char* data);

	// Get the count of 4x (unmigrated) profiles
	NS_IMETHOD Get4xProfileCount(int *numProfiles);
	NS_IMETHOD MigrateAllProfiles();

	// Clone a profile with current profile info
	NS_IMETHOD CloneProfile(const char* aProfileName);
};

nsProfile* nsProfile::mInstance = nsnull;

static PRInt32 g_InstanceCount = 0;
static PRInt32 g_LockCount = 0;

/*
 * Constructor/Destructor
 */
nsProfile::nsProfile()
: m_reg(nsnull)
{
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
// Creates an instance of the mozRegistry (m_reg)
// or any other registry that would be finalized for 5.0
// Adds subtree Profiles to facilitate profile operations
NS_IMETHODIMP nsProfile::Startup(char *filename)
{

#if defined(DEBUG_profile)
    printf("ProfileManager (nsProfile) : Startup : Get Registry handle\n");
#endif
    nsresult rv = NS_OK;
    rv = nsComponentManager::CreateInstance(kRegistryCID,
                                            nsnull,
                                            nsIRegistry::GetIID(),
                                            (void **) &m_reg);

    
	// Add the root Profiles node in the registry
	if (m_reg != nsnull)
	{
		// Latch onto the registry object.
		NS_ADDREF(m_reg);

		// Open it against the input file name.
		rv = m_reg->Open();
    
		if (NS_SUCCEEDED(rv))
		{
			nsIRegistry::Key key;
			rv = m_reg->AddSubtree(nsIRegistry::Common, "Profiles", &key);

			if (NS_FAILED(rv))
			{
				#if defined(DEBUG_profile)
					printf("Registry : Couldn't add Profiles subtree.\n");
				#endif
			}
			m_reg->Close();
		}
		else
		{
			#if defined(DEBUG_profile)
				printf("Couldn't open registry.\n");
			#endif
		}
	}
	else
	{
		#if defined(DEBUG_profile)
			printf("Registry Object is NULL.\n");
		#endif

		return NS_ERROR_FAILURE;
	}

    return rv;
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
// Sets the given profile to be a current profile
NS_IMETHODIMP nsProfile::GetProfileDir(const char *profileName, nsFileSpec* profileDir)
{
    nsresult rv = NS_OK;

#if defined(DEBUG_profile)
    printf("ProfileManager : GetProfileDir\n");
#endif

    // Check result.
    if ( m_reg != nsnull ) 
	{
        // Open the registry
        rv = m_reg->Open();

		if (NS_SUCCEEDED(rv))
		{
			nsIRegistry::Key key;

			// Get handle to subtree "Profiles"
			rv = m_reg->GetSubtree(nsIRegistry::Common, "Profiles", &key);

			if (NS_SUCCEEDED(rv))
			{
		        nsIRegistry::Key newKey;

				// Get handle to <profileName> passed
				rv = m_reg->GetSubtree(key, profileName, &newKey);

				if (NS_SUCCEEDED(rv))
				{
					char* encodedProfileDir = nsnull;

					// Get the value of entry "directory"
			        rv = m_reg->GetString( newKey, "directory", &encodedProfileDir );

					if (NS_SUCCEEDED(rv))
					{
						char* isMigrated = nsnull;
						char* orgProfileDir = nsnull;
						
						// Use persistent classes to make the directory names XPlatform
						nsInputStringStream stream(encodedProfileDir);
						nsPersistentFileDescriptor descriptor;
						stream >> descriptor;
						*profileDir = descriptor;

						PR_FREEIF(encodedProfileDir);

						orgProfileDir = PL_strdup(profileDir->GetCString());

						// Get the value of entry "migrated" to check the nature of the profile
						m_reg->GetString( newKey, "migrated", &isMigrated);
				
						// Set this to be a current profile only if it is a 5.0 profile
						if (PL_strcmp(isMigrated, "yes") == 0)
						{
							rv = m_reg->SetString(key, "CurrentProfile", profileName);
													
							if (NS_FAILED(rv))
							{
								#if defined(DEBUG_profile)
									printf("Profiles : Couldn't set CurrentProfile Name.\n");
								#endif
					        }

							nsFileSpec tmpFileSpec(orgProfileDir);
							if (!tmpFileSpec.Exists()) {
							    
								// Get profile defaults folder..
								nsIFileLocator* locator = nsnull;
		
								rv = nsServiceManager::GetService(kFileLocatorCID, nsIFileLocator::GetIID(), (nsISupports**)&locator);

								if (NS_FAILED(rv) || !locator)
									return NS_ERROR_FAILURE;

								nsIFileSpec* profDefaultsDir;
								rv = locator->GetFileLocation(nsSpecialFileSpec::App_ProfileDefaultsFolder50, &profDefaultsDir);
        
								if (NS_FAILED(rv) || !profDefaultsDir)
									return NS_ERROR_FAILURE;

								nsFileSpec defaultsDirSpec;
			
								profDefaultsDir->GetFileSpec(&defaultsDirSpec);
								NS_RELEASE(profDefaultsDir);

								// Copy contents from defaults folder.
								if (defaultsDirSpec.Exists())
								{
									defaultsDirSpec.RecursiveCopy(tmpFileSpec);
								}

								nsServiceManager::ReleaseService(kFileLocatorCID, locator);
							}

						}

					}
					else
					{
						#if defined(DEBUG_profile)
							printf("Profiles : Couldn't get Profile directory name.\n");
						#endif
					}
				}
				else
				{
					#if defined(DEBUG_profile)
						printf("Profiles : Couldn't get profileName subtree.\n");
					#endif
				}
			}
			else
			{
				#if defined(DEBUG_profile)
					printf("Registry : Couldn't get Profiles subtree.\n");
				#endif
			}
			m_reg->Close();
		}
		else
		{
			#if defined(DEBUG_profile)
				printf("Couldn't open registry.\n");
			#endif
		}
		
    }
	else
	{
		#if defined(DEBUG_profile)
			printf("Registry Object is NULL.\n");
		#endif

		return NS_ERROR_FAILURE;
	}
    return rv;
}


// Gets the number of profiles
// Location: Common/Profiles
NS_IMETHODIMP nsProfile::GetProfileCount(int *numProfiles)
{

    nsresult rv = NS_OK;

#if defined(DEBUG_profile)
    printf("ProfileManager : GetProfileCount\n");
#endif

    // Check result.
    if ( m_reg != nsnull ) 
	{
        // Open the registry
        rv = m_reg->Open();

		if (NS_SUCCEEDED(rv))
		{
			// Enumerate all subkeys (immediately) under the given node.
			nsIEnumerator *enumKeys;
			nsIRegistry::Key key;

			rv = m_reg->GetSubtree(nsIRegistry::Common, "Profiles", &key);

			if (NS_SUCCEEDED(rv))
			{
		        rv = m_reg->EnumerateSubtrees( key, &enumKeys );

				if (NS_SUCCEEDED(rv))
				{
			        int numKeys=0;
					rv = enumKeys->First();

			        while( NS_SUCCEEDED( rv ) && !enumKeys->IsDone() ) 
					{
						nsISupports *base;

						rv = enumKeys->CurrentItem( &base );

						if (NS_SUCCEEDED(rv)) 
						{
	                        // Get specific interface.
		                    nsIRegistryNode *node;
			                nsIID nodeIID = NS_IREGISTRYNODE_IID;

							rv = base->QueryInterface( nodeIID, (void**)&node );
					        
						    // Test that result.
							if (NS_SUCCEEDED(rv)) 
							{
								// Get node name.
		                        char *profile = nsnull;
								char *isMigrated = nsnull;

			                    rv = node->GetName( &profile );

								if (NS_SUCCEEDED(rv) && (profile))
								{
									nsIRegistry::Key profKey;								

									rv = m_reg->GetSubtree(key, profile, &profKey);
								
									if (NS_SUCCEEDED(rv)) 
									{
										rv = m_reg->GetString(profKey, "migrated", &isMigrated);

										if (NS_SUCCEEDED(rv) && (isMigrated))
										{
											if (PL_strcmp(isMigrated, "yes") == 0)
											{
												numKeys++;
											}
										}
									}
								}
								node->Release();
					        }
							base->Release();
						}	
						rv = enumKeys->Next();
					}
					*numProfiles = numKeys;
					NS_RELEASE(enumKeys);
				}
				else
				{
					#if defined(DEBUG_profile)
						printf("Profiles : Can't enumerate subtrees.\n" );
					#endif
				}
			}
			else
			{
				#if defined(DEBUG_profile)
					printf("Registry : Couldn't get Profiles subtree.\n");
				#endif
			}
			m_reg->Close();
		}
		else
		{
			#if defined(DEBUG_profile)
				printf("Couldn't open registry.\n");
			#endif
		}
		
    }
	else
	{
		#if defined(DEBUG_profile)
			printf("Registry Object is NULL.\n");
		#endif

		return NS_ERROR_FAILURE;

	}

	return rv;
}


// If only a single profile exists
// and returns the name of the single profile.
// Otherwise it returns the name of the first valid profile.
// Commonly used in picking the current profile name if it is not set.
NS_IMETHODIMP nsProfile::GetSingleProfile(char **profileName)
{
	nsresult rv = NS_OK;

#if defined(DEBUG_profile)
    printf("ProfileManager : GetSingleProfile\n");
#endif

    // Check result.
    if ( m_reg != nsnull ) 
	{
        // Open the registry
        rv = m_reg->Open();

		if (NS_SUCCEEDED(rv))
		{
	        // Enumerate all subkeys (immediately) under the given node.
		    nsIEnumerator *enumKeys;
			nsIRegistry::Key key;

	        rv = m_reg->GetSubtree(nsIRegistry::Common, "Profiles", &key);

			if (NS_SUCCEEDED(rv))
			{
		        rv = m_reg->EnumerateSubtrees( key, &enumKeys );

				if (NS_SUCCEEDED(rv))
				{
					// Go to beginning.
					rv = enumKeys->First();

			        while(NS_SUCCEEDED(rv)&& !enumKeys->IsDone() ) 
					{
						nsISupports *base;
						rv = enumKeys->CurrentItem( &base );		

						if (NS_SUCCEEDED(rv))
						{
							// Get specific interface.
							nsIRegistryNode *node;
							nsIID nodeIID = NS_IREGISTRYNODE_IID;
							rv = base->QueryInterface( nodeIID, (void**)&node );
		                        
							if (NS_SUCCEEDED(rv))
							{
								char *isMigrated = nsnull;
								nsIRegistry::Key profKey;								

					            // Get node name.
					            rv = node->GetName( profileName );	

								// Profiles need to be migrated are not considered as valid profiles
								// On finding the valid profile, breaks out of the while loop.
								if (NS_SUCCEEDED(rv))
								{
									rv = m_reg->GetSubtree(key, *profileName, &profKey);

									m_reg->GetString(profKey, "migrated", &isMigrated);

									if (PL_strcmp(isMigrated, "no") == 0)
									{
										rv = enumKeys->Next();
										continue;
									}
									else
									{
										if (NS_SUCCEEDED(rv))
										{
										    rv = m_reg->SetString(key, "CurrentProfile", *profileName);	
		
											if (NS_SUCCEEDED(rv))
											{
												break;
											}
											else
											{
												#if defined(DEBUG_profile)
													printf("RegistryNode : Can't set Current Profile name.\n" );
												#endif											
											}
										}
									}
								}
								else
								{
									#if defined(DEBUG_profile)
										printf("RegistryNode : Couldn't get the node name.\n");
									#endif
								}
							}
							else
							{
								#if defined(DEBUG_profile)
									printf("RegistryNode : Couldn't get the interface.\n");
								#endif
							}
						}
						else
						{
							#if defined(DEBUG_profile)
								printf("Profiles : Couldn't get current enumerated item.\n");
							#endif
						}
						rv = enumKeys->Next();
					} //end while loop
					NS_RELEASE(enumKeys);
				}
				else
				{
					#if defined(DEBUG_profile)
						printf("Profiles : Couldn't enumerate subtrees.\n");
					#endif
				}
			}
			else
			{
				#if defined(DEBUG_profile)
					printf("Registry : Couldn't get Profiles subtree.\n");
				#endif
			}
	        m_reg->Close();
		}
		else
		{
			#if defined(DEBUG_profile)
				printf("Couldn't open registry.\n");
			#endif
		}
	}
	else
	{
		#if defined(DEBUG_profile)
			printf("Registry Object is NULL.\n");
		#endif

		return NS_ERROR_FAILURE;
	}
    return rv;
}


// Returns the name of the current profile i.e., the last used profile
NS_IMETHODIMP nsProfile::GetCurrentProfile(char **profileName)
{
	nsresult rv = NS_OK;
#if defined(DEBUG_profile) 
	printf("ProfileManager : GetCurrentProfile\n");
#endif
  
	// Check result.
	if ( m_reg != nsnull ) 
	{
		// Open the registry.
		rv = m_reg->Open();

		if (NS_SUCCEEDED(rv))
		{
			nsIRegistry::Key key;

			rv = m_reg->GetSubtree(nsIRegistry::Common, "Profiles", &key);

			if (NS_SUCCEEDED(rv))
			{

				rv = m_reg->GetString( key, "CurrentProfile", profileName );

				// if fails to get the current profile set the value to null
				if (NS_FAILED(rv))
				{
		            profileName = '\0';

					#if defined(DEBUG_profile)
						printf("Profiles:Can't get Current Profile value.\n" );
					#endif
				}
			}
			else
			{
				#if defined(DEBUG_profile)
					printf("Registry : Couldn't get Profiles subtree.\n");
				#endif
			}
			m_reg->Close();
		}
		else
		{
			#if defined(DEBUG_profile)
				printf("Couldn't open registry.\n");
			#endif
		}
		
    }
	else
	{
		#if defined(DEBUG_profile)
			printf("Registry Object is NULL.\n");
		#endif

		return NS_ERROR_FAILURE;
	}
	return rv;
}


//  Returns the name of the first profile in the Registry
//  This is essentially GetSingleProfile(). Should go away.
//	Check for dependencies.
NS_IMETHODIMP nsProfile::GetFirstProfile(char **profileName)
{
	nsresult rv = NS_OK;

#if defined(DEBUG_profile)
    printf("ProfileManager : GetFirstProfile\n");
#endif

    rv = GetSingleProfile(profileName);

    if (NS_FAILED(rv))
    {
		#if defined(DEBUG_profile)
            printf("Couldn't get the first profile.\n" );
		#endif
    }

    return rv;
}


// Returns the name of the current profile directory
NS_IMETHODIMP nsProfile::GetCurrentProfileDir(nsFileSpec* profileDir)
{
	nsresult rv = NS_OK;

#if defined(DEBUG_profile)
    printf("ProfileManager : GetCurrentProfileDir\n");
#endif

    char *profileName = nsnull;

    rv = GetCurrentProfile(&profileName);

	if (NS_SUCCEEDED(rv))
	{
	   	rv = GetProfileDir(profileName, profileDir);

	    if (NS_FAILED(rv))
		{
			#if defined(DEBUG_profile)
	            printf("Couldn't get the profile dir.\n" );
			#endif
	    }

    }
	else
	{
		#if defined(DEBUG_profile)
		    printf("Couldn't get the current profile.\n" );
		#endif
	}


    if (profileName) {
   	    PR_DELETE(profileName);
    }
    
    return rv;
}


/*
 * Setters
 */

// Sets the current profile directory
NS_IMETHODIMP nsProfile::SetProfileDir(const char *profileName, const nsFileSpec& profileDir)
{
    nsresult rv = NS_OK;

#if defined(DEBUG_profile)
    printf("ProfileManager : SetProfileDir\n");
    printf("profileName : %s ", profileName);
    printf("profileDir  : %s\n", profileDir.GetCString());
#endif

    // Check result.
    if (m_reg != nsnull)
	{
		// Open the registry.
		rv = m_reg->Open();
    
		if (NS_SUCCEEDED(rv))
		{
			nsIRegistry::Key key;

			rv = m_reg->GetSubtree(nsIRegistry::Common, "Profiles", &key);

			if (NS_SUCCEEDED(rv))
			{
				nsIRegistry::Key newKey;

				rv = m_reg->AddSubtree(key, profileName, &newKey);
		
				if (NS_SUCCEEDED(rv))
				{
					// Create a tmp Filespec and create a directory if required
					nsFileSpec tmpDir(profileDir);

					if (!profileDir.Exists())
					{
						// nsPersistentFileDescriptor requires an existing
						// object. Make it first.
						tmpDir.CreateDirectory();
					}
					
					// Persistency
					nsPersistentFileDescriptor descriptor(profileDir);
					char* profileDirString = nsnull;
					profileDirString = nsCRT::strdup(profileDir.GetCString());
					nsOutputStringStream stream(profileDirString);
					stream << descriptor;
    
					// Set the entry "directory" for this profile
					if (profileDirString && *profileDirString)
					{
						rv = m_reg->SetString(newKey, "directory", profileDirString);

						CreateUserDirectories(tmpDir);

						if (NS_FAILED(rv))
						{
							#if defined(DEBUG_profile)
								printf("Couldn't set directory property.\n" );
							#endif
						}
						
						// A 5.0 profile needs no migration.
						// Hence setting migrated flag to "yes"
						// Need to change this logic to "migrate" = true/false.
						rv = m_reg->SetString( newKey, "migrated", "yes");
					}
					else
					{
						#if defined(DEBUG_profile)
							printf("NULL value received for directory name.\n" );
						#endif
					}
					nsCRT::free(profileDirString);
				}
				else
				{
					#if defined(DEBUG_profile)
						printf("Profiles : Could not add profile name subtree.\n" );
					#endif
				}
			}
			else
			{
				#if defined(DEBUG_profile)
					printf("Registry : Couldn't get Profiles subtree.\n");
				#endif
			}

			rv = m_reg->SetString(key, "CurrentProfile", profileName);

			if (NS_FAILED(rv))
			{
				#if defined(DEBUG_profile)
					printf("Couldn't set CurrentProfile name.\n" );
				#endif
			}
		    m_reg->Close();
		}
		else
		{
			#if defined(DEBUG_profile)
				printf("Couldn't open registry.\n");
			#endif
		}
		
	}
	else
	{
		#if defined(DEBUG_profile)
			printf("Registry Object is NULL.\n");
		#endif

		return NS_ERROR_FAILURE;
	}
    return rv;
}


// Creates a new profile
NS_IMETHODIMP nsProfile::CreateNewProfile(char* charData)
{

	nsresult rv = NS_OK;

#if defined(DEBUG_profile)
    printf("ProfileManager : CreateNewProfile\n");
    printf("ProfileManagerData*** : %s\n", charData);
#endif

	nsIFileLocator* locator = nsnull;
		
	rv = nsServiceManager::GetService(kFileLocatorCID, nsIFileLocator::GetIID(), (nsISupports**)&locator);

	if (NS_FAILED(rv) || !locator)
		return NS_ERROR_FAILURE;

    nsString data(charData);
    
	// Set the gathered info into an array
	SetDataArray(data);
    
	char* dirName = GetValue("ProfileDir");
    char* unescapedProfileName = GetValue("ProfileName");
    
	if (!unescapedProfileName || !*unescapedProfileName)
		return NS_ERROR_FAILURE;

	// Escape profile name to create a valid direrctory, 
	// if directory value is not provided
	char* profileName = nsEscape(unescapedProfileName, url_Path); // temp hack
      
	nsFileSpec dirSpec(dirName);

	if (!dirName || !*dirName)
    {
		// They didn't type a directory path...
		// Get current profile, make the new one a sibling...
	    nsIFileSpec* horribleCOMDirSpecThing;
        rv = locator->GetFileLocation(nsSpecialFileSpec::App_DefaultUserProfileRoot50, &horribleCOMDirSpecThing);
        
		if (NS_FAILED(rv) || !horribleCOMDirSpecThing)
			return NS_ERROR_FAILURE;

		//Append profile name to form a directory name
	    horribleCOMDirSpecThing->GetFileSpec(&dirSpec);
		NS_RELEASE(horribleCOMDirSpecThing);
		//dirSpec.SetLeafName(profileName);
		//dirSpec += profileName;
	}

#if defined(DEBUG_profile)
	printf("before SetProfileDir\n");
#endif

	if (!dirSpec.Exists())
		dirSpec.CreateDirectory();

	dirSpec += profileName;

	// Set the directory value and add the entry to the registry tree.
	// Creates required user directories.
    rv = SetProfileDir(unescapedProfileName, dirSpec);

	PR_DELETE(unescapedProfileName);

#if defined(DEBUG_profile)
    printf("after SetProfileDir\n");
#endif

    if (NS_FAILED(rv))
		return rv;

    // Get profile defaults folder..
    nsIFileSpec* profDefaultsDir;
    rv = locator->GetFileLocation(nsSpecialFileSpec::App_ProfileDefaultsFolder50, &profDefaultsDir);
        
	if (NS_FAILED(rv) || !profDefaultsDir)
		return NS_ERROR_FAILURE;

	nsFileSpec defaultsDirSpec;

	profDefaultsDir->GetFileSpec(&defaultsDirSpec);
	NS_RELEASE(profDefaultsDir);

	// Copy contents from defaults folder.
	if (defaultsDirSpec.Exists())
	{
		defaultsDirSpec.RecursiveCopy(dirSpec);
	}

	nsServiceManager::ReleaseService(kFileLocatorCID, locator);
		
	if (dirName)
	{
		PR_DELETE(dirName);
	}
	delete [] profileName;

    return NS_OK;
}

// Create required user directories like Mial, News, Cache etc.
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


// Delete all user directories associated with the a profile
// A FileSpec of the profile's directory is taken as input param
// Probably we should take profile name as param....?
void nsProfile::DeleteUserDirectories(const nsFileSpec& profileDir)
{

#if defined(DEBUG_profile)
    printf("ProfileManager : DeleteUserDirectories\n");
#endif

	if (profileDir.Exists())
	{
		profileDir.Delete(PR_TRUE);
	}
}


// Set the data stream into an array
void nsProfile::SetDataArray(nsString data)
{

#if defined(DEBUG_profile)
	printf("ProfileManager : Setting new profile data\n");
	printf("SetDataArray data : %s\n", data.ToNewCString());
#endif

	int index = 0;
	char *newStr=nsnull;
	char *tokstr = data.ToNewCString();
	char *token = nsCRT::strtok(tokstr, "%", &newStr);

#if defined(DEBUG_profile)
	printf("before while loop\n");
#endif

	while (token)
    {

#if defined(DEBUG_profile)
	printf("subTok : %s\n", token);
#endif
    
		PL_strcpy(gNewProfileData[index], token);
		index++;
		g_Count = index;
      
		token = nsCRT::strtok(newStr, "%", &newStr);
    }
  
	delete[] tokstr;

#if defined(DEBUG_profile)
  printf("after while loop\n");
#endif

}

//Get the value associated with the name from Data Array
// i.e., generated from the data passed by the JS reflection
// request to create a new profile (CreateProfile Wizard).
char* nsProfile::GetValue(char *name)
{
    int nameLength;
    char* value;
    value = (char *) PR_Malloc(sizeof(char) * _MAX_LENGTH);

    for (int i = 0; i < g_Count; i=i+1) 
	{
        if (gNewProfileData[i]) 
		{
            nameLength = PL_strlen(name);

            if (PL_strncmp(name, gNewProfileData[i], nameLength) == 0) 
			{
                char* equalsPosition = PL_strchr(gNewProfileData[i], '=');
                if (equalsPosition) 
				{
                    PL_strcpy(value, 1 + equalsPosition);
                    return value;
                }
            }
        }
    }

#if defined(DEBUG_profile)
    printf("after for loop\n");
#endif

    return nsnull;
} 


// Rename a old profile to new profile.
// Need to add ProfileExists() routine to avoid the illegal rename operation
// Copies all the keys from old profile to new profile.
// Deletes the old profile from the registry
NS_IMETHODIMP nsProfile::RenameProfile(const char* oldName, const char* newName)
{

	nsresult rv = NS_OK;

#if defined(DEBUG_profile)
    printf("ProfileManager : Renaming profile %s to %s \n", oldName, newName);
#endif

	rv = ProfileExists(newName);

	// That profile already exists...
	if (NS_SUCCEEDED(rv))
	{
		printf("ProfileManager : Rename Operation failed : Profile exists. Provide a different new name for profile.\n");
		return NS_ERROR_FAILURE;
	}


	char *currProfile = nsnull;

	GetCurrentProfile(&currProfile);

	// If renaming the current profile set the renameCurrProfile flag to true
	// By default the flag is set to false.
	if (currProfile)
	{
		if (PL_strcmp(oldName, currProfile) == 0)
		{
			renameCurrProfile = PR_TRUE;
	
			#if defined(DEBUG_profile)
				printf("ProfileManager : Renaming the current profile\n");
			#endif
		}
	}

	// Copy reg keys
	CopyRegKey(oldName, newName);

	// Delete old profile entry
		DeleteProfile(oldName, "false");


	// If we renamed current profile, the new profile will be the current profile
	if (renameCurrProfile)
	{
		rv = m_reg->Open();

		if (NS_SUCCEEDED(rv))
		{
			nsIRegistry::Key profileRootKey;

			rv = m_reg->GetSubtree(nsIRegistry::Common, "Profiles", &profileRootKey);

			if (NS_SUCCEEDED(rv))
			{
				rv = m_reg->SetString(profileRootKey, "CurrentProfile", newName);
			}
			m_reg->Close();
		}
		renameCurrProfile = PR_FALSE;
	}
   	return rv;
}

// Copy old profile entries to the new profile
// In the process creates new profile subtree.
nsresult nsProfile::CopyRegKey(const char *oldProfile, const char *newProfile)
{
	nsIEnumerator	 *enumKeys;
    nsIRegistry::Key sourceKey, destKey, profileRootKey;
    
	nsresult rv = m_reg->Open();

	// Need to add else statements to print the error stages.
	if (NS_SUCCEEDED(rv))
	{
		rv = m_reg->GetSubtree(nsIRegistry::Common, "Profiles", &profileRootKey);

		if (NS_SUCCEEDED(rv))
		{

			rv = m_reg->GetSubtree(profileRootKey, (char *) oldProfile, &sourceKey);

			if (NS_SUCCEEDED(rv))
			{
				rv = m_reg->AddSubtree(profileRootKey, (char *) newProfile, &destKey);

				if (NS_SUCCEEDED(rv))
				{
	                rv = m_reg->EnumerateValues(sourceKey, &enumKeys );

					if (NS_SUCCEEDED(rv)) {
						rv = enumKeys->First();
                    
						// Enumerate subkeys till done.
						while( NS_SUCCEEDED( rv ) && !enumKeys->IsDone() ) 
						{
							nsISupports *base;
							rv = enumKeys->CurrentItem( &base );
                    
							// Test result.
							if (NS_SUCCEEDED(rv)) 
							{
								// Get specific interface.
								nsIRegistryNode *value;
								nsIID valueIID = NS_IREGISTRYVALUE_IID;
								rv = base->QueryInterface( valueIID, (void**)&value );
                        
								// Test that result.
								if (NS_SUCCEEDED(rv)) 
								{
									// Get node name.
									char *entryName;
									char *entryValue;
								
									rv = value->GetName( &entryName );
								
									if (NS_SUCCEEDED(rv)) 
									{
										rv = m_reg->GetString( sourceKey, entryName, &entryValue);

										if (NS_SUCCEEDED(rv)) 
										{
											rv = m_reg->SetString(destKey, entryName, entryValue);
										}
									}
								
									if (entryName)
									{
										PR_DELETE(entryName);
									}
									if (entryValue)
									{
										PR_DELETE(entryValue);
									}
								}
							}
							rv = enumKeys->Next();
						}
						NS_RELEASE(enumKeys);
					}
				}
			}
		}
		m_reg->Close();
	}
	return rv;
}


// Delete a profile from the registry
// Not deleting the directories on the harddisk yet.
// 4.x kind of confirmation need to be implemented yet
NS_IMETHODIMP nsProfile::DeleteProfile(const char* profileName, const char* canDeleteFiles)
{

    nsresult rv = NS_OK;

#if defined(DEBUG_profile)
    printf("ProfileManager : DeleteProfile\n");
#endif
    
	nsFileSpec profileDirSpec;
	GetProfileDir(profileName, &profileDirSpec);

	// To be more uniform need to change the arragement
	// of the following with NS_SUCCEEDED()
	// Check result.
    if ( m_reg != nsnull ) 
	{
        // Open the registry.
        rv = m_reg->Open();

        if (NS_FAILED(rv))
        {
			#if defined(DEBUG_profile)
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
				printf("Can't obtain subtree Profiles.\n" );
			#endif

            return rv;
        }

		// Remove the subtree from the registry.
        rv = m_reg->RemoveSubtree(key, profileName);

        if (NS_FAILED(rv))
           return rv;
		else
		{
			#if defined(DEBUG_profile) 
				printf("DeleteProfile : deleted profile -> %s <-\n", profileName);
			#endif
		}

		// May need to delete directories, but not so directly.
		// DeleteUserDirectories(profileDirSpec);

		// Take care of the current profile, if the profile 
		// just deleted was a current profile.
		if (!renameCurrProfile)
		{
			char *oldCurrProfile = nsnull;
			char *newCurrProfile = nsnull;

			GetCurrentProfile(&oldCurrProfile);

			if (PL_strcmp(profileName, oldCurrProfile) == 0)
			{
				GetFirstProfile(&newCurrProfile);

				#if defined(DEBUG_profile) 
					printf("DeleteProfile : Old Current profile -> %s <-\n", oldCurrProfile);
					printf("DeleteProfile : New Current profile -> %s <-\n", newCurrProfile);
				#endif

				rv = m_reg->SetString(key, "CurrentProfile", newCurrProfile);

			    if (NS_FAILED(rv)) 
				{
				   #if defined(DEBUG_profile) 
					   printf("Did not take nsnull successfully\n");
			       #endif
				}
			}
		}
        m_reg->Close();
    }

	// If user asks for it, delete profile directory
	if (PL_strcmp(canDeleteFiles, "true") == 0)
	{
		DeleteUserDirectories(profileDirSpec);
	}


    return rv;
}


// Get all profiles fro the registry's Profiles subtree
// All the profiles that need to be migrated will be appended
// with the text " - migrate" (e.g. <unmigratedProfileName> - migrate).
// Need to change this lotic to have True/False to represent the 
// migration state.
// Names are stored in the global array gProfiles[].
void nsProfile::GetAllProfiles()
{
	nsresult rv = NS_OK;

#if defined(DEBUG_profile) 
    printf("ProfileManager : GetAllProfiles\n");
#endif
	
	int index = 0;

    // Check result.
    if ( m_reg != nsnull ) 
	{
        // Open the registry.
        rv = m_reg->Open();

        if (NS_SUCCEEDED(rv)) 
		{
            // Enumerate all subkeys (immediately) under the given node.
            nsIEnumerator *enumKeys;
            nsIRegistry::Key key;

            rv = m_reg->GetSubtree(nsIRegistry::Common, "Profiles", &key);

            if (NS_SUCCEEDED(rv)) 
			{
                rv = m_reg->EnumerateSubtrees( key, &enumKeys );

                if (NS_SUCCEEDED(rv)) 
				{

                    rv = enumKeys->First();

                    // Enumerate subkeys till done.
                    while( NS_SUCCEEDED( rv ) && !enumKeys->IsDone() ) 
                    {
						nsISupports *base;
						rv = enumKeys->CurrentItem( &base );

						if (NS_SUCCEEDED(rv)) 
						{
	                        // Get specific interface.
		                    nsIRegistryNode *node;
			                nsIID nodeIID = NS_IREGISTRYNODE_IID;
				            rv = base->QueryInterface( nodeIID, (void**)&node );
					        
						    // Test that result.
							if (NS_SUCCEEDED(rv)) 
							{
								// Get node name.
		                        char *profile = nsnull;
								char *isMigrated = nsnull;

			                    rv = node->GetName( &profile );

								if (NS_SUCCEEDED(rv) && (profile))
								{
									nsIRegistry::Key profKey;								

									rv = m_reg->GetSubtree(key, profile, &profKey);
								
									if (NS_SUCCEEDED(rv)) 
									{
										rv = m_reg->GetString(profKey, "migrated", &isMigrated);

										if (NS_SUCCEEDED(rv) && (isMigrated))
										{
											if (PL_strcmp(isMigrated, "no") == 0)
											{
												PL_strcpy(gProfiles[index], profile);
												PL_strcat(gProfiles[index], " - migrate");
											}
											else
											{
												PL_strcpy(gProfiles[index], profile);
											}

											#if defined(DEBUG_profile)
												printf("proflie%d = %s\n", index, gProfiles[index]);
											#endif
										}
									}
								}
								node->Release();
					        }
							base->Release();
						}	
					    rv = enumKeys->Next();

						if (NS_FAILED(rv))
						{
							#if defined(DEBUG_profile)
								printf( "Error advancing enumerator, rv=0x%08X\n", (int)rv );
							#endif
						}
						index++;
					}
					g_numProfiles = index;
					NS_RELEASE(enumKeys);
				}
            }
        }
        m_reg->Close();
    }
}



// Get the list of all profiles
// Populate the input param.
// Essentially calls GetAllProfiles to fill gProfiles[].
// This method is written to support the core service
// call to get the names all profiles.
NS_IMETHODIMP nsProfile::GetProfileList(nsString& profileList)
{

#if defined(DEBUG_profile)
	printf("Inside GetProfileList routine.\n" );
#endif

	GetAllProfiles();

	for (int i=0; i < g_numProfiles; i++)
	{
		if (i != 0)
		{
			profileList += ",";
		}
		profileList += gProfiles[i];
	}
	return NS_OK;
}


// Start Communicator with a profile of user's choice
// Prefs and FileLocation services are used here.
// FileLocation service to make ir forget about the global profile dir it had.
// Prefs service to kick off the startup to start the app with new profile's prefs.
NS_IMETHODIMP nsProfile::StartCommunicator(const char* profileName)
{

    nsresult rv = NS_OK;

#if defined(DEBUG_profile)
    printf("ProfileManager : Start AppRunner\n");
#endif

	nsIPref *prefs = nsnull;

	/*
	 * Need to load new profile prefs.
	 */
	rv = nsServiceManager::GetService(kPrefCID, 
                                    nsIPref::GetIID(), 
                                    (nsISupports **)&prefs);
	
	// First, set the profile to be the current profile.
	// So that FileLocation services grabs right directory when it needs to.
	if (NS_SUCCEEDED(rv))
	{
		rv = m_reg->Open();

		if (NS_SUCCEEDED(rv))
		{
			nsIRegistry::Key profileRootKey;
			rv = m_reg->GetSubtree(nsIRegistry::Common, "Profiles", &profileRootKey);

			if (NS_SUCCEEDED(rv))
			{
				rv = m_reg->SetString(profileRootKey, "CurrentProfile", profileName);
			}
			m_reg->Close();
		}

		nsIFileLocator* locator = nsnull;
    
		rv = nsServiceManager::GetService(kFileLocatorCID, nsIFileLocator::GetIID(), (nsISupports**)&locator);
			
		if (locator)
		{
			rv = locator->ForgetProfileDir();
			nsServiceManager::ReleaseService(kFileLocatorCID, locator);
		}

		if (NS_SUCCEEDED(rv))
		{
			prefs->StartUp();
		}
		
		// Release prefs service.
		if (prefs) {
			nsServiceManager::ReleaseService(kPrefCID, prefs);
		}
	}

    return rv;
}


// Get Current Profile (Support for Core service call).
// Populate the input param with current profile.
NS_IMETHODIMP nsProfile::GetCurrProfile(nsString& currProfile)
{

#if defined(DEBUG_profile)
	printf("Inside GetCurrentProfile (JSReflec) routine.\n" );
#endif

	char *localCurProf = nsnull;

	GetCurrentProfile(&localCurProf);

	currProfile += localCurProf;

	return NS_OK;
}

// Migrate profile information from the 4x registry to 5x registry.
NS_IMETHODIMP nsProfile::MigrateProfileInfo()
{
	nsresult rv = NS_OK;

#ifdef XP_PC

#if defined(DEBUG_profile)
    printf("Entered MigrateProfileInfo.\n");
#endif

	char oldRegFile[_MAX_LENGTH] = {'\0'};

    // Check result.
    if ( m_reg != nsnull ) 
	{
		/***********************
		Get the right registry file. Ideally from File Locations.
		File Locations is yet to implement this service.
	
		nsIFileLocator* locator = nsnull;
		nsFileSpec oldAppRegistry;
	
		rv = nsServiceManager::GetService(kFileLocatorCID, nsIFileLocator::GetIID(), (nsISupports**)&locator);
		if (NS_FAILED(rv) || !locator)
		      return NS_ERROR_FAILURE;
		// Get current profile, make the new one a sibling...
		rv = locator->GetFileLocation(nsSpecialFileSpec::App_Registry50, &oldAppRegistry);
		nsServiceManager::ReleaseService(kFileLocatorCID, locator);
		if (NS_FAILED(rv))
			 return NS_ERROR_FAILURE;
		oldAppRegistry.SetLeafName(OLD_WINREG);
        
		rv = m_reg->Open(oldAppRegistry.GetCString());
		************************/

		// Registry file has been traditionally stored in the windows directory (XP_PC).
		nsSpecialSystemDirectory systemDir(nsSpecialSystemDirectory::Win_WindowsDirectory);

		// Append the name of the old registry to the path obtained.
		PL_strcpy(oldRegFile, systemDir.GetNativePathCString());
		PL_strcat(oldRegFile, oldWinReg);

		rv = m_reg->Open(oldRegFile);

		// Enumerate 4x tree and create an array of that information.
		if (NS_SUCCEEDED(rv))
		{
            // Enumerate all subkeys (immediately) under the given node.
            nsIEnumerator *enumKeys;

            if (NS_SUCCEEDED(rv)) 
			{
            
				rv = m_reg->EnumerateSubtrees(nsIRegistry::Users, &enumKeys );

                if (NS_SUCCEEDED(rv)) 
				{
                
					rv = enumKeys->First();
                    
					// Enumerate subkeys till done.
                    while( NS_SUCCEEDED( rv ) && !enumKeys->IsDone() ) 
                    {
						nsISupports *base;
						rv = enumKeys->CurrentItem( &base );

						if (NS_SUCCEEDED(rv)) 
						{
	                        // Get specific interface.
		                    nsIRegistryNode *node;
			                nsIID nodeIID = NS_IREGISTRYNODE_IID;
				            rv = base->QueryInterface( nodeIID, (void**)&node );
					        
						    // Get Node details.
							if (NS_SUCCEEDED(rv)) 
							{
	                            // Get node name.
		                        char *profile = nsnull;
			                    rv = node->GetName( &profile );

								#if defined(DEBUG_profile)
									printf("oldProflie = %s\n", profile);
								#endif

								nsIRegistry::Key key;								
							
								rv = m_reg->GetSubtree(nsIRegistry::Users, profile, &key);

								if (NS_SUCCEEDED(rv)) 
								{
									PL_strcpy(gOldProfiles[g_numOldProfiles], nsUnescape(profile));
								}

								char *profLoc = nsnull;
								
								rv = m_reg->GetString( key, "ProfileLocation", &profLoc);

								#if defined(DEBUG_profile)
									printf("oldProflie Location = %s\n", profLoc);
								#endif
							
								if (NS_SUCCEEDED(rv)) 
								{
									PL_strcpy(gOldProfLocations[g_numOldProfiles], profLoc);
									g_numOldProfiles++;
								}

								PR_FREEIF(profile);
								PR_FREEIF(profLoc);
							}
						}	
					    rv = enumKeys->Next();
					}
					NS_RELEASE(enumKeys);
				}
            }
			m_reg->Close();
			
			if (g_numOldProfiles > 0)
			{
				UpdateMozProfileRegistry();
			}
		}
	}

#endif
	return rv;
}

// Update the mozregistry with the 4x profile names
// and thier locations. Entry "migrated" is set to "no"
// to differentiate these profiles from 5x profiles.
// gOldProfiles, gOldProfLocations carried info about
// profile names and locations respectively. They are
// populated inthe routine MigrateProfileInfo()
NS_IMETHODIMP nsProfile::UpdateMozProfileRegistry()
{

	nsresult rv = NS_OK;

#ifdef XP_PC

#if defined(DEBUG_profile)
    printf("Entered MigrateProfileInfo.\n");
#endif

	for (int index = 0; index < g_numOldProfiles; index++)
	{
		// Check result.
		if (m_reg != nsnull)
		{
			// Open the registry file.
			rv = m_reg->Open();
    
			if (NS_SUCCEEDED(rv))
			{
				nsIRegistry::Key key;
				rv = m_reg->GetSubtree(nsIRegistry::Common, "Profiles", &key);

				if (NS_SUCCEEDED(rv))
				{
					nsIRegistry::Key newKey;

					rv = m_reg->SetString(key, "NeedMigration", "true");

					rv = m_reg->GetSubtree(key, gOldProfiles[index], &newKey);
			
					if (NS_FAILED(rv))
					{
						rv = m_reg->AddSubtree(key, gOldProfiles[index], &newKey);
		
						if (NS_SUCCEEDED(rv))
						{
							nsFileSpec profileDir(gOldProfLocations[index]);
						
							nsPersistentFileDescriptor descriptor(profileDir);
							char* profileDirString = nsnull;
							nsOutputStringStream stream(profileDirString);
							stream << descriptor;
  
							if (profileDirString && *profileDirString)
							{

								rv = m_reg->SetString(newKey, "directory", profileDirString);
	
								if (NS_FAILED(rv))
								{
									#if defined(DEBUG_profile)
										printf("Couldn't set directory property.\n" );
									#endif
								}
							
								rv = m_reg->SetString(newKey, "migrated", "no");
							
								if (NS_FAILED(rv))
								{
									#if defined(DEBUG_profile)
										printf("Couldn't add migrate flag.\n" );
									#endif
								}
							}
							else
							{
								#if defined(DEBUG_profile)
									printf("NULL value received for directory name.\n" );
								#endif
							}
							delete [] profileDirString;
						}
						else
						{
							#if defined(DEBUG_profile)
								printf("Profiles : Could not add profile subtree.\n" );
							#endif
						}
					}
					else
					{
						#if defined(DEBUG_profile)
							printf("Profiles : Profile name subtree already exists.\n" );
						#endif
					}
				}
				else
				{
					#if defined(DEBUG_profile)
						printf("Registry : Couldn't get Profiles subtree.\n");
					#endif
				}
			    m_reg->Close();
			}
			else
			{
				#if defined(DEBUG_profile)
					printf("Couldn't open registry.\n");
				#endif
			}
		}	
	}

#endif

	return rv;
}

// Migrate a selected profile
// Set the profile to the current profile....debatable.
// Calls PrefMigration service to do the Copy and Diverge
// of 4x Profile information
NS_IMETHODIMP nsProfile::MigrateProfile(const char* profileName)
{

	nsresult rv = NS_OK;

#ifdef XP_PC

#if defined(DEBUG_profile)
	printf("Inside Migrate Profile routine.\n" );
#endif

	nsFileSpec oldProfDir;
	nsFileSpec newProfDir;

	GetProfileDir(profileName, &oldProfDir);

	// Create new profile dir path
	nsIFileLocator* locator = nsnull;
	
	// Get the new directory path the migrated profile to reside.
	rv = nsServiceManager::GetService(kFileLocatorCID, nsIFileLocator::GetIID(), (nsISupports**)&locator);
	
	if (NS_FAILED(rv) || !locator)
	{
	      return NS_ERROR_FAILURE;
	}

    // Get current profile, make the new one a sibling...
	nsIFileSpec* newSpec = NS_LocateFileOrDirectory(
        nsSpecialFileSpec::App_DefaultUserProfileRoot50);
	if (!newSpec)
	     return NS_ERROR_FAILURE;
    newSpec->GetFileSpec(&newProfDir);
	NS_RELEASE(newSpec);
	newProfDir += profileName;


	// Call migration service to do the work.
	nsIPrefMigration *pPrefMigrator = nsnull;

	rv = nsComponentManager::CreateInstance(kPrefMigration_CID, 
                                          nsnull, 
                                          kIPrefMigration_IID, 
                                          (void**) &pPrefMigrator); 
	if (NS_SUCCEEDED(rv)) 
	{ 
		pPrefMigrator->ProcessPrefs((char *)oldProfDir.GetCString(), 
                                (char *)newProfDir.GetCString(), 
                                &rv); 
	} 

	if (NS_FAILED(rv))
	     return NS_ERROR_FAILURE;

	//unmark migrate flag
	if (NS_SUCCEEDED(rv))
	{
		// Check result.
		if ( m_reg != nsnull ) 
		{
	        // Open the registry.
		    rv = m_reg->Open();

			if (NS_SUCCEEDED(rv))
			{
				nsIRegistry::Key key;

				rv = m_reg->GetSubtree(nsIRegistry::Common, "Profiles", &key);

				if (NS_SUCCEEDED(rv))
				{
			        nsIRegistry::Key newKey;
					rv = m_reg->GetSubtree(key, profileName, &newKey);

					if (NS_SUCCEEDED(rv))
					{
				        rv = m_reg->SetString( newKey, "migrated", "yes");

						if (NS_SUCCEEDED(rv))
						{
							rv = m_reg->SetString(key, "CurrentProfile", profileName);
													
							if (NS_FAILED(rv))
							{
								#if defined(DEBUG_profile)
									printf("Profiles : Couldn't set CurrentProfile Name.\n");
								#endif
					        }
						}
						else
						{
							#if defined(DEBUG_profile)
								printf("Profiles : Couldn't set migrate flag.\n");
							#endif
						}
						SetProfileDir(profileName, newProfDir);
					}
					else
					{
						#if defined(DEBUG_profile)
							printf("Profiles : Couldn't get profileName subtree.\n");
						#endif
					}
				}
				else
				{
					#if defined(DEBUG_profile)
						printf("Registry : Couldn't get Profiles subtree.\n");
					#endif
				}
				m_reg->Close();
			}
			else
			{	
				#if defined(DEBUG_profile)
					printf("Couldn't open registry.\n");
				#endif
			}
		}
	}

#endif

	return rv;
}


NS_IMETHODIMP nsProfile::GetCookie(nsString& aCookie)
{
#ifndef NECKO
  nsINetService *service;
  nsIURI	*aURL;

  nsresult rv = NS_NewURL(&aURL, PREG_URL);
  if (NS_FAILED(rv)) return rv;

  nsresult res = nsServiceManager::GetService(kNetServiceCID,
                                          kINetServiceIID,
                                          (nsISupports **)&service);
  if ((NS_OK == res) && (nsnull != service) && (nsnull != aURL)) {

    res = service->GetCookieString(aURL, aCookie);

    NS_RELEASE(service);
  }

  return res;
#else
  // XXX NECKO we need to use the cookie module for this info instead of 
  // XXX the IOService
  return NS_ERROR_NOT_IMPLEMENTED;
#endif // NECKO
}

NS_IMETHODIMP nsProfile::ProcessPRegCookie()
{

	nsresult rv = NS_OK;

	nsString aCookie;
	GetCookie(aCookie);

	rv = ProcessPREGInfo(aCookie.ToNewCString());

	return rv;
}

NS_IMETHODIMP nsProfile::ProcessPREGInfo(char* data)
{
	nsresult rv = NS_OK;

	nsString aCookie(data);

	char *pregCookie = nsnull;
	char *profileName = nsnull;
	char *service_denial = nsnull;

	if ( (aCookie.ToNewCString()) != nsnull )
	{
		pregCookie = PL_strstr(aCookie.ToNewCString(), PREG_COOKIE);
		//In the original scenario you must UnEscape the string
		//PL_strstr(nsUnescape(uregCookie),PREG_USERNAME);
		if (pregCookie)
		{
			profileName		= PL_strstr(pregCookie, PREG_USERNAME);
			service_denial	= PL_strstr(pregCookie, PREG_DENIAL);
		}
	}
	else
	{
		// cookie information is not available
		return NS_ERROR_FAILURE;
	}

	nsAutoString pName;
	nsAutoString serviceState;

	if (profileName) 
		pName.SetString(profileName);
	if (service_denial) 
		serviceState.SetString(service_denial);

	int profileNameIndex, delimIndex;
	int serviceIndex;

	nsString userProfileName, userServiceDenial;

	if (pName.Length())
	{
		profileNameIndex = pName.Find("=", 0);
		delimIndex    = pName.Find("[-]", profileNameIndex-1);
    
		pName.Mid(userProfileName, profileNameIndex+1,delimIndex-(profileNameIndex+1));

		printf("\nProfiles : PREG Cookie user profile name = %s\n", userProfileName.ToNewCString());
	}

	if (serviceState.Length())
	{
		serviceIndex  = serviceState.Find("=", 0);
		delimIndex    = serviceState.Find("[-]", serviceIndex-1);

		serviceState.Mid(userServiceDenial, serviceIndex+1,delimIndex-(serviceIndex+1));

		printf("\nProfiles : PREG Cookie netcenter service option = %s\n", userServiceDenial.ToNewCString());
	}

	// User didn't provide any information.
	// No Netcenter info is available.
	// User will hit the Preg info screens on the next run.
	if ((userProfileName.mLength == 0) && (userServiceDenial.mLength == 0))
		return NS_ERROR_FAILURE;

	// If user denies registration, ignore the information entered.
	if (userServiceDenial.mLength > 0)
		userProfileName.SetString("");

	char *curProfile = nsnull;

	rv = GetCurrentProfile(&curProfile);

	if (NS_SUCCEEDED(rv))
	{
		if (userProfileName.mLength > 0)
			rv = RenameProfile(curProfile, userProfileName.ToNewCString());

		if (NS_SUCCEEDED(rv))
		{
		    // Check result.
			if (m_reg != nsnull)
			{
				// Open the registry.
				rv = m_reg->Open();
    
				if (NS_SUCCEEDED(rv))
				{
					nsIRegistry::Key key;

					rv = m_reg->GetSubtree(nsIRegistry::Common, "Profiles", &key);

					if (NS_SUCCEEDED(rv))
					{
						nsIRegistry::Key newKey;
		
						if (userProfileName.mLength > 0)
						{
							rv = m_reg->GetSubtree(key, userProfileName.ToNewCString(), &newKey);
				
							if (NS_SUCCEEDED(rv))
							{
								// Register new info
								rv = m_reg->SetString(newKey, "NCProfileName", userProfileName.ToNewCString());
							}
							else
							{
								#if defined(DEBUG_profile)
									printf("Profiles : Could not set NetCenter properties.\n" );
								#endif
							}
							
							rv = m_reg->SetString(key, "CurrentProfile", userProfileName.ToNewCString());

							if (NS_FAILED(rv))
							{
								#if defined(DEBUG_profile)
									printf("Couldn't set CurrentProfile name.\n" );
								#endif
							}
						}
					}
					else
					{
						#if defined(DEBUG_profile)
							printf("Registry : Couldn't get Profiles subtree.\n");
						#endif
					}

					if (userServiceDenial.mLength > 0)
						rv = m_reg->SetString(key, "NCServiceDenial", userServiceDenial.ToNewCString());

					rv = m_reg->SetString(key, "HavePregInfo", "true");

					if (NS_FAILED(rv))
					{
						#if defined(DEBUG_profile)
							printf("Couldn't set Preg info flag.\n" );
						#endif
					}

					m_reg->Close();
				}
				else
				{
					#if defined(DEBUG_profile)
						printf("Couldn't open registry.\n");
					#endif
				}
			}
			else
			{
				#if defined(DEBUG_profile)
					printf("Registry Object is NULL.\n");
				#endif

				return NS_ERROR_FAILURE;
			}
		}
	}
	return rv;
}


NS_IMETHODIMP nsProfile::IsPregCookieSet(char **pregSet)
{

	nsresult rv = NS_OK;

	// Check result.
	if (m_reg != nsnull)
	{
		// Open the registry.
		rv = m_reg->Open();
    
		if (NS_SUCCEEDED(rv))
		{
			nsIRegistry::Key key;

			rv = m_reg->GetSubtree(nsIRegistry::Common, "Profiles", &key);

			if (NS_SUCCEEDED(rv))
			{
				rv = m_reg->GetString(key, "HavePregInfo", pregSet);
			}
			else
			{
				#if defined(DEBUG_profile)
					printf("Registry : Couldn't get Profiles subtree.\n");
				#endif
			}
			m_reg->Close();
		}
		else
		{
			#if defined(DEBUG_profile)
				printf("Couldn't open registry.\n");
			#endif
		}
	}
	else
	{
		#if defined(DEBUG_profile)
			printf("Registry Object is NULL.\n");
		#endif

		return NS_ERROR_FAILURE;
	}

	return rv;
}

NS_IMETHODIMP nsProfile::ProfileExists(const char *profileName)
{

	nsresult rv = NS_OK;

	// Check result.
	if (m_reg != nsnull)
	{
		// Open the registry.
		rv = m_reg->Open();
    
		if (NS_SUCCEEDED(rv))
		{
			nsIRegistry::Key key;

			rv = m_reg->GetSubtree(nsIRegistry::Common, "Profiles", &key);

			if (NS_SUCCEEDED(rv))
			{
		        nsIRegistry::Key newKey;

				// Get handle to <profileName> passed
				rv = m_reg->GetSubtree(key, profileName, &newKey);			
			}
			else
			{
				#if defined(DEBUG_profile)
					printf("Registry : Couldn't get Profiles subtree.\n");
				#endif
			}
			m_reg->Close();
		}
		else
		{
			#if defined(DEBUG_profile)
				printf("Couldn't open registry.\n");
			#endif
		}
	}
	else
	{
		#if defined(DEBUG_profile)
			printf("Registry Object is NULL.\n");
		#endif

		return NS_ERROR_FAILURE;
	}

	return rv;
}


// Gets the number of unmigrated 4x profiles
// Location: Common/Profiles
NS_IMETHODIMP nsProfile::Get4xProfileCount(int *numProfiles)
{

    nsresult rv = NS_OK;

#if defined(DEBUG_profile)
    printf("ProfileManager : Get4xProfileCount\n");
#endif

    // Check result.
    if ( m_reg != nsnull ) 
	{
        // Open the registry
        rv = m_reg->Open();

		if (NS_SUCCEEDED(rv))
		{
			// Enumerate all subkeys (immediately) under the given node.
			nsIEnumerator *enumKeys;
			nsIRegistry::Key key;

			rv = m_reg->GetSubtree(nsIRegistry::Common, "Profiles", &key);

			if (NS_SUCCEEDED(rv))
			{
		        rv = m_reg->EnumerateSubtrees( key, &enumKeys );

				if (NS_SUCCEEDED(rv))
				{
			        int numKeys=0;
					rv = enumKeys->First();

			        while( NS_SUCCEEDED( rv ) && !enumKeys->IsDone() ) 
					{
						nsISupports *base;

						rv = enumKeys->CurrentItem( &base );

						if (NS_SUCCEEDED(rv)) 
						{
	                        // Get specific interface.
		                    nsIRegistryNode *node;
			                nsIID nodeIID = NS_IREGISTRYNODE_IID;

							rv = base->QueryInterface( nodeIID, (void**)&node );
					        
						    // Test that result.
							if (NS_SUCCEEDED(rv)) 
							{
								// Get node name.
		                        char *profile = nsnull;
								char *isMigrated = nsnull;

			                    rv = node->GetName( &profile );

								if (NS_SUCCEEDED(rv) && (profile))
								{
									nsIRegistry::Key profKey;								

									rv = m_reg->GetSubtree(key, profile, &profKey);
								
									if (NS_SUCCEEDED(rv)) 
									{
										rv = m_reg->GetString(profKey, "migrated", &isMigrated);

										if (NS_SUCCEEDED(rv) && (isMigrated))
										{
											if (PL_strcmp(isMigrated, "no") == 0)
											{
												numKeys++;
											}
										}
									}
								}
								node->Release();
					        }
							base->Release();
						}	
						rv = enumKeys->Next();
					}
					*numProfiles = numKeys;
					NS_RELEASE(enumKeys);
				}
				else
				{
					#if defined(DEBUG_profile)
						printf("Profiles : Can't enumerate subtrees.\n" );
					#endif
				}
			}
			else
			{
				#if defined(DEBUG_profile)
					printf("Registry : Couldn't get Profiles subtree.\n");
				#endif
			}
			m_reg->Close();
		}
		else
		{
			#if defined(DEBUG_profile)
				printf("Couldn't open registry.\n");
			#endif
		}
		
    }
	else
	{
		#if defined(DEBUG_profile)
			printf("Registry Object is NULL.\n");
		#endif

		return NS_ERROR_FAILURE;

	}

	return rv;
}


// Migrates all unmigrated profiles
NS_IMETHODIMP nsProfile::MigrateAllProfiles()
{

    nsresult rv = NS_OK;

#ifdef XP_PC
	for (int i=0; i < g_numOldProfiles; i++)
	{
		rv = MigrateProfile(gOldProfiles[i]);
	}
#endif

	return NS_OK;
}


NS_IMETHODIMP nsProfile::CloneProfile(const char* newProfile)
{
    nsresult rv = NS_OK;

#if defined(DEBUG_profile)
    printf("ProfileManager : CloneProfile\n");
#endif

	nsFileSpec currProfileDir;
	nsFileSpec newProfileDir;

	GetCurrentProfileDir(&currProfileDir);

	if (currProfileDir)
	{
		nsIFileLocator* locator = nsnull;
		
		rv = nsServiceManager::GetService(kFileLocatorCID, nsIFileLocator::GetIID(), (nsISupports**)&locator);

		if (NS_FAILED(rv) || !locator)
			return NS_ERROR_FAILURE;

	    nsIFileSpec* dirSpec;
        rv = locator->GetFileLocation(nsSpecialFileSpec::App_DefaultUserProfileRoot50, &dirSpec);
        
		if (NS_FAILED(rv) || !dirSpec)
			return NS_ERROR_FAILURE;

		//Append profile name to form a directory name
	    dirSpec->GetFileSpec(&newProfileDir);
		NS_RELEASE(dirSpec);

		newProfileDir += newProfile;

		currProfileDir.RecursiveCopy(newProfileDir);
		
		rv = SetProfileDir(newProfile, newProfileDir);
	}


#if defined(DEBUG_profile)
	if (NS_SUCCEEDED(rv))
		printf("ProfileManager : Cloned CurrentProfile to new Profile ->%s<-\n", newProfile);
#endif

	return rv;
}


/***************************************************************************************/
/***********                           PROFILE FACTORY                      ************/
/***************************************************************************************/
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

NS_IMPL_ISUPPORTS(nsProfileFactory, nsIFactory::GetIID());

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

	if (factory == nsnull) {
		return NS_ERROR_OUT_OF_MEMORY;
	}

	nsresult res = factory->QueryInterface(nsIFactory::GetIID(), (void **) aFactory);
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

