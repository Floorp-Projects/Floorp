/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#include "plstr.h"
#include "prenv.h"

#include "nsIRegistry.h"
#include "NSReg.h"
#include "nsIFactory.h"
#include "nsIComponentManager.h"
#include "nsIEnumerator.h"
#include "nsXPIDLString.h"
#include "nsIFileSpec.h"
#include "nsFileSpec.h"
#include "nsString.h"
#include "nsIFileLocator.h"
#include "nsFileLocations.h"
#include "nsEscape.h"
#include "nsIURL.h"

#include "nsIAppShellService.h"
#include "nsAppShellCIDs.h"
#include "nsIBrowserWindow.h"
#include "nsIWebShellWindow.h"
#include "prprf.h"

#ifdef XP_PC
#define AUTOMATICALLY_MIGRATE_IF_ONLY_ONE_PROFILE 1
#endif

#ifdef XP_UNIX
#define USER_ENVIRONMENT_VARIABLE "USER"
#define HOME_ENVIRONMENT_VARIABLE "HOME"
#define PROFILE_NAME_ENVIRONMENT_VARIABLE "PROFILE_NAME"
#define PROFILE_HOME_ENVIRONMENT_VARIABLE "PROFILE_HOME"
#endif

#ifndef NECKO
#include "nsINetService.h"
#else
#include "nsIIOService.h"
#include "nsNeckoUtil.h"
#endif // NECKO


#ifdef XP_PC
#include <direct.h>
#endif

#include "nsIPrefMigration.h"
#include "nsPrefMigrationCIDs.h"

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

#include "nsIBookmarksService.h"

#define _MAX_LENGTH			256
#define _MAX_NUM_PROFILES	50

// PREG information
#define PREG_COOKIE			"NS_REG2_PREG"
#define PREG_USERNAME		"PREG_USER_NAME"
#define PREG_DENIAL			"PREG_USER_DENIAL"
#define PREG_URL			"http://seaspace.mcom.com/"


#if defined(DEBUG_sspitzer) || defined(DEBUG_seth)
#define DEBUG_profile 1
#endif

// Globals to hold profile information
#ifdef XP_PC
static char *oldWinReg="nsreg.dat";
#endif

#ifdef XP_MAC
static char *oldMacReg = "Netscape Registry";
#endif

static char gNewProfileData[_MAX_NUM_PROFILES][_MAX_LENGTH] = {{'\0'}};
static int	g_Count = 0;

static char gProfiles[_MAX_NUM_PROFILES][_MAX_LENGTH] = {{'\0'}};
static int	g_numProfiles = 0;

static char gOldProfiles[_MAX_NUM_PROFILES][_MAX_LENGTH] = {{'\0'}};
static char gOldProfLocations[_MAX_NUM_PROFILES][_MAX_LENGTH] = {{'\0'}};
static int	g_numOldProfiles = 0;

static PRBool renameCurrProfile = PR_FALSE;

// IID and CIDs of all the services needed
static NS_DEFINE_CID(kProfileCID, NS_PROFILE_CID);
static NS_DEFINE_CID(kBookmarksCID, NS_BOOKMARKS_SERVICE_CID);      
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kFileLocatorCID, NS_FILELOCATOR_CID);
static NS_DEFINE_CID(kRegistryCID, NS_REGISTRY_CID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kAppShellServiceCID, NS_APPSHELL_SERVICE_CID);

#ifndef NECKO
static NS_DEFINE_IID(kINetServiceIID, NS_INETSERVICE_IID);
static NS_DEFINE_CID(kNetServiceCID, NS_NETSERVICE_CID);
#else
static NS_DEFINE_IID(kIIOServiceIID, NS_IIOSERVICE_IID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
#endif // NECKO

static NS_DEFINE_CID(kPrefMigrationCID, NS_PREFMIGRATION_CID);

static
nsresult GetStringFromSpec(nsFileSpec inSpec, char **string)
{
	nsresult rv;
	nsCOMPtr<nsIFileSpec> spec;
	rv = NS_NewFileSpecWithSpec(inSpec, getter_AddRefs(spec));
	if (NS_SUCCEEDED(rv)) {
        	rv = spec->GetPersistentDescriptorString(string);
		if (NS_SUCCEEDED(rv)) {
			return NS_OK;
                }
		else {
			PR_FREEIF(*string);
			return rv;
		}
        } 
	else {
		*string = nsnull;
		return rv;
	}
}
class nsProfile: public nsIProfile,
                 public nsIShutdownListener
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROFILE

  NS_IMETHOD OnShutdown(const nsCID& aClass, nsISupports *service);
  
private:
  static void useDefaultProfileFile(nsProfile *aProfileInst);
  static nsProfile *mInstance;
  nsIRegistry* m_reg;
  nsIPref* m_prefs;

  nsresult loadPrefs();
  nsresult unloadPrefs();

  nsresult ProcessArgs(nsICmdLineService *service,
                       PRBool *profileDirSet,
                       char **profdir);
  nsresult LoadDefaultProfileDir(char *profstr);
    
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

    NS_IMETHOD ProfileExists(const char *profileName);
	NS_IMETHOD UpdateMozProfileRegistry();
};

nsProfile* nsProfile::mInstance = nsnull;

static PRInt32 g_InstanceCount = 0;
static PRInt32 g_LockCount = 0;

/*
 * Constructor/Destructor
 */
nsProfile::nsProfile()
: m_reg(nsnull),
  m_prefs(nsnull)
{
  PR_AtomicIncrement(&g_InstanceCount);
  NS_INIT_REFCNT();
}

nsProfile::~nsProfile() {
  unloadPrefs();
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

NS_IMPL_ADDREF(nsProfile)
NS_IMPL_RELEASE(nsProfile)
NS_IMPL_QUERY_INTERFACE2(nsProfile, nsIProfile, nsIShutdownListener)

/*
 * nsIProfile Implementation
 */
// Creates an instance of the mozRegistry (m_reg)
// or any other registry that would be finalized for 5.0
// Adds subtree Profiles to facilitate profile operations
NS_IMETHODIMP nsProfile::Startup(const char *filename)
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
		rv = m_reg->OpenDefault();
    
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
  unloadPrefs();
  
  NS_RELEASE(m_reg);
  return NS_OK;
}

NS_IMETHODIMP
nsProfile::StartupWithArgs(nsICmdLineService *cmdLineArgs)
{
	nsresult rv;
  // initializations for profile manager
	PRBool profileDirSet = PR_FALSE;
	char *profstr=nsnull;
  
#if defined(DEBUG_profile)
  printf("Profile Manager : Profile Wizard and Manager activites : Begin\n");
#endif
  Startup(nsnull);


  
  if (cmdLineArgs)
    rv = ProcessArgs(cmdLineArgs, &profileDirSet, &profstr);
  
  if (!profileDirSet)
    rv = LoadDefaultProfileDir(profstr);

#if defined(DEBUG_profile)
  printf("Profile Manager : Profile Wizard and Manager activites : End\n");
#endif

  return NS_OK;
}


nsresult
nsProfile::LoadDefaultProfileDir(char *profstr)
{
    nsresult rv;
    nsCOMPtr<nsIURI> profURL;
    int numProfiles=0;
  
    PRInt32 profWinWidth  = 615;
    PRInt32 profWinHeight = 500;
  
    GetProfileCount(&numProfiles);
  /*
	 	 * Create the Application Shell instance...
		 */
    NS_WITH_SERVICE(nsIAppShellService, profAppShell,
                    kAppShellServiceCID, &rv);
    if (NS_FAILED(rv))
        return rv;

    char *isPregInfoSet = nsnull;
    IsPregCookieSet(&isPregInfoSet); 
		
    PRBool pregPref = PR_FALSE;
    rv = loadPrefs();
    if (NS_SUCCEEDED(rv))
        rv = m_prefs->GetBoolPref(PREG_PREF, &pregPref);

    if (!profstr)
        {
            // This means that there was no command-line argument to force
            // profile UI to come up. But we need the UI anyway if there
            // are no profiles yet, or if there is more than one.
            if (numProfiles == 0)
                {
                    if (pregPref)
                        profstr = "resource:/res/profile/cpwPreg.xul"; 
                    else
                        profstr = "resource:/res/profile/cpw.xul"; 
                }
            else if (numProfiles > 1)
                profstr = "resource:/res/profile/pm.xul"; 
        }


    // Provide Preg information
    if (pregPref && (PL_strcmp(isPregInfoSet, "true") != 0))
        profstr = "resource:/res/profile/cpwPreg.xul"; 


    if (profstr)
        {
            rv = NS_NewURI(getter_AddRefs(profURL), profstr);
	
            if (NS_FAILED(rv)) {
                return rv;
            } 

            nsCOMPtr<nsIWebShellWindow>  profWindow;
            rv = profAppShell->CreateTopLevelWindow(nsnull, profURL,
                                                    PR_TRUE, PR_TRUE, NS_CHROME_ALL_CHROME,
                                                    nsnull, profWinWidth, profWinHeight,
                                                    getter_AddRefs(profWindow));

            if (NS_FAILED(rv)) 
                {
                    return rv;
                }

            /*
             * Start up the main event loop...
             */	
            rv = profAppShell->Run();
        }

    if (pregPref && PL_strcmp(isPregInfoSet, "true") != 0)
        ProcessPRegCookie();
    
    // Now we have the right profile, read the user-specific prefs.
    m_prefs->ReadUserPrefs();
    return NS_OK;
}

nsresult
nsProfile::ProcessArgs(nsICmdLineService *cmdLineArgs,
                       PRBool* profileDirSet,
                       char **profstr)
{
    nsresult rv;
	char* cmdResult = nsnull;
	nsFileSpec currProfileDirSpec;
#ifdef DEBUG_profile
    printf("Profile Manager : Command Line Options : Begin\n");
#endif
  
    // check for command line arguments for profile manager
    //	
    // -P command line option works this way:
    // apprunner -P profilename 
    // runs the app using the profile <profilename> 
    // remembers profile for next time
    rv = cmdLineArgs->GetCmdLineValue("-P", &cmdResult);
    if (NS_SUCCEEDED(rv))
        {
            if (cmdResult) {
                char* currProfileName = cmdResult;
#ifdef DEBUG_profile
                printf("ProfileName : %s\n", cmdResult);
#endif /* DEBUG_profile */
			
                GetProfileDir(currProfileName, &currProfileDirSpec);
#ifdef DEBUG_profile
		char *currProfileDirSpecStr = nsnull;
		rv = GetStringFromSpec(currProfileDirSpec, &currProfileDirSpecStr);

		if (NS_SUCCEEDED(rv) && currProfileDirSpecStr && *currProfileDirSpecStr) {
			printf("** ProfileDir  :  %s **\n", currProfileDirSpecStr);
		}
		PR_FREEIF(currProfileDirSpecStr);
#endif /* DEBUG_profile */
			
                if (NS_SUCCEEDED(rv)){
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

    rv = cmdLineArgs->GetCmdLineValue("-CreateProfile", &cmdResult);
    if (NS_SUCCEEDED(rv))
        {
            if (cmdResult) {
                char* currProfileName = strtok(cmdResult, " ");
                char* currProfileDirString = strtok(NULL, " ");
			
                if (currProfileDirString)
                    currProfileDirSpec = currProfileDirString;
                else
                    {
                        // No directory name provided. Get File Locator
                        NS_WITH_SERVICE(nsIFileLocator, locator, kFileLocatorCID, &rv);
			if (NS_FAILED(rv) || !locator)
				return NS_ERROR_FAILURE;
				
                        // Get current profile, make the new one a sibling...
                        nsCOMPtr <nsIFileSpec> spec;
                        rv = locator->GetFileLocation(nsSpecialFileSpec::App_UserProfileDirectory50, getter_AddRefs(spec));
                        if (NS_FAILED(rv) || !spec)
                            return NS_ERROR_FAILURE;
                        spec->GetFileSpec(&currProfileDirSpec);
                        currProfileDirSpec.SetLeafName(currProfileName);

                        rv = locator->ForgetProfileDir();
                    }
#ifdef DEBUG_profile
                printf("profileName & profileDir are: %s\n", cmdResult);
#endif
                SetProfileDir(currProfileName, currProfileDirSpec);
                *profileDirSet = PR_TRUE;
			
            }
        }

    // Start Profile Manager
    rv = cmdLineArgs->GetCmdLineValue("-ProfileManager", &cmdResult);
    if (NS_SUCCEEDED(rv))
        {		
            if (cmdResult) {
                *profstr = "resource:/res/profile/pm.xul"; 
            }
        }

	// Start Profile Wizard
    rv = cmdLineArgs->GetCmdLineValue("-ProfileWizard", &cmdResult);
    if (NS_SUCCEEDED(rv))
        {		
            if (cmdResult) {
                *profstr = "resource:/res/profile/cpw.xul"; 
            }
        }

	// Start Migaration activity
    rv = cmdLineArgs->GetCmdLineValue("-installer", &cmdResult);
    if (NS_SUCCEEDED(rv))
        {		
            if (cmdResult) {
                rv = MigrateProfileInfo();
                if (NS_FAILED(rv)) return rv;

                int num4xProfiles = 0;
                rv = Get4xProfileCount(&num4xProfiles);
                if (NS_FAILED(rv)) return rv;
                
				int numProfiles = 0;
				GetProfileCount(&numProfiles);
                if (num4xProfiles == 0 && numProfiles == 0) {
                    *profstr = "resource:/res/profile/cpw.xul"; 
                }
#ifdef AUTOMATICALLY_MIGRATE_IF_ONLY_ONE_PROFILE
                else if (num4xProfiles == 1) {
                    MigrateAllProfiles();
                }
                else if (num4xProfiles > 1) {
#else
		else {
#endif /* AUTOMATICALLY_MIGRATE_IF_ONLY_ONE_PROFILE */
                    *profstr = "resource:/res/profile/pm.xul";
                }
            }
        }

#ifdef DEBUG_profile
    printf("Profile Manager : Command Line Options : End\n");
#endif



  /*
   * Load preferences, causing them to be initialized, and hold a reference to them.
   */
    rv = loadPrefs();
    if (NS_FAILED(rv))
        return rv;

  
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
        rv = m_reg->OpenDefault();

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
					nsXPIDLCString encodedProfileDir;

					// Get the value of entry "directory"
			        rv = m_reg->GetString( newKey, "directory", getter_Copies(encodedProfileDir));

					if (NS_SUCCEEDED(rv))
					{
						nsXPIDLCString isMigrated;
						
						// Use persistent classes to make the directory names XPlatform
						nsInputStringStream stream(encodedProfileDir);
						nsPersistentFileDescriptor descriptor;
						stream >> descriptor;
						*profileDir = descriptor;

						// Get the value of entry "migrated" to check the nature of the profile
						m_reg->GetString( newKey, "migrated",
                                          getter_Copies(isMigrated));
				
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

							nsFileSpec tmpFileSpec(*profileDir);
							if (!tmpFileSpec.Exists()) {
							    
								// Get profile defaults folder..
                              					NS_WITH_SERVICE(nsIFileLocator, locator, kFileLocatorCID, &rv);

								if (NS_FAILED(rv) || !locator)
									return NS_ERROR_FAILURE;

								nsCOMPtr <nsIFileSpec> profDefaultsDir;
								rv = locator->GetFileLocation(nsSpecialFileSpec::App_ProfileDefaultsFolder50, getter_AddRefs(profDefaultsDir));
        
								if (NS_FAILED(rv) || !profDefaultsDir)
									return NS_ERROR_FAILURE;

								nsFileSpec defaultsDirSpec;
			
								profDefaultsDir->GetFileSpec(&defaultsDirSpec);

								// Copy contents from defaults folder.
								if (defaultsDirSpec.Exists())
								{
									defaultsDirSpec.RecursiveCopy(tmpFileSpec);
								}

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
        rv = m_reg->OpenDefault();

		if (NS_SUCCEEDED(rv))
		{
			// Enumerate all subkeys (immediately) under the given node.
			nsCOMPtr<nsIEnumerator> enumKeys;
			nsIRegistry::Key key;

			rv = m_reg->GetSubtree(nsIRegistry::Common, "Profiles", &key);

			if (NS_SUCCEEDED(rv))
			{
		        rv = m_reg->EnumerateSubtrees( key, getter_AddRefs(enumKeys));

				if (NS_SUCCEEDED(rv))
				{
			        int numKeys=0;
					rv = enumKeys->First();

			        while( NS_SUCCEEDED( rv ) && !enumKeys->IsDone() ) 
					{
						nsCOMPtr<nsISupports> base;

						rv = enumKeys->CurrentItem( getter_AddRefs(base) );

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
		                        nsXPIDLCString profile;
								char *isMigrated = nsnull;

			                    rv = node->GetName( getter_Copies(profile) );

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
											nsCRT::free(isMigrated);
										}
									}
								}
								NS_RELEASE(node);
					        }
						}	
						rv = enumKeys->Next();
					}
					*numProfiles = numKeys;
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
        rv = m_reg->OpenDefault();

		if (NS_SUCCEEDED(rv))
		{
	        // Enumerate all subkeys (immediately) under the given node.
		    nsCOMPtr<nsIEnumerator> enumKeys;
			nsIRegistry::Key key;

	        rv = m_reg->GetSubtree(nsIRegistry::Common, "Profiles", &key);

			if (NS_SUCCEEDED(rv))
			{
		        rv = m_reg->EnumerateSubtrees( key, getter_AddRefs(enumKeys));

				if (NS_SUCCEEDED(rv))
				{
					// Go to beginning.
					rv = enumKeys->First();

			        while(NS_SUCCEEDED(rv)&& !enumKeys->IsDone() ) 
					{
						nsCOMPtr<nsISupports> base;
						rv = enumKeys->CurrentItem(getter_AddRefs(base));

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
		rv = m_reg->OpenDefault();

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
NS_IMETHODIMP nsProfile::SetProfileDir(const char *profileName, nsFileSpec& profileDir)
{
    nsresult rv = NS_OK;

#if defined(DEBUG_profile)
    printf("ProfileManager : SetProfileDir\n");
    printf("profileName : %s ", profileName);
    char *profileDirStr = nsnull;
    rv = GetStringFromSpec(profileDir, &profileDirStr);
    if (NS_SUCCEEDED(rv) && profileDirStr && *profileDirStr) {
	printf("profileDir  : %s\n", profileDirStr);
    }
    PR_FREEIF(profileDirStr);
#endif

    // Check result.
    if (m_reg != nsnull)
	{
		// Open the registry.
		rv = m_reg->OpenDefault();
    
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
					rv = GetStringFromSpec(profileDir, &profileDirString);
				        NS_ASSERTION((NS_SUCCEEDED(rv) && profileDirString && *profileDirString), "profileDir is bad?");
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
					PR_FREEIF(profileDirString);
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
NS_IMETHODIMP nsProfile::CreateNewProfile(const char* charData)
{

	nsresult rv = NS_OK;

#if defined(DEBUG_profile)
    printf("ProfileManager : CreateNewProfile\n");
    printf("ProfileManagerData*** : %s\n", charData);
#endif

    NS_WITH_SERVICE(nsIFileLocator, locator, kFileLocatorCID, &rv);

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
	    nsCOMPtr <nsIFileSpec> horribleCOMDirSpecThing;
        rv = locator->GetFileLocation(nsSpecialFileSpec::App_DefaultUserProfileRoot50, getter_AddRefs(horribleCOMDirSpecThing));
        
		if (NS_FAILED(rv) || !horribleCOMDirSpecThing)
			return NS_ERROR_FAILURE;

		//Append profile name to form a directory name
	    horribleCOMDirSpecThing->GetFileSpec(&dirSpec);
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
	{
		nsCRT::free(profileName);
		return rv;
	}

    // Get profile defaults folder..
    nsCOMPtr <nsIFileSpec> profDefaultsDir;
    rv = locator->GetFileLocation(nsSpecialFileSpec::App_ProfileDefaultsFolder50, getter_AddRefs(profDefaultsDir));
        
	if (NS_FAILED(rv) || !profDefaultsDir)
	{
		nsCRT::free(profileName);
		return NS_ERROR_FAILURE;
	}

	nsFileSpec defaultsDirSpec;

	profDefaultsDir->GetFileSpec(&defaultsDirSpec);

	// Copy contents from defaults folder.
	if (defaultsDirSpec.Exists())
	{
		defaultsDirSpec.RecursiveCopy(dirSpec);
	}

	if (dirName)
	{
		PR_DELETE(dirName);
	}
	nsCRT::free(profileName);

    return NS_OK;
}

// Create required user directories like ImapMail, Mail, News, Cache etc.
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
    tmpDir += "ImapMail";

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

	int idx = 0;
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
    
		PL_strcpy(gNewProfileData[idx], token);
		idx++;
		g_Count = idx;
      
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
#if defined(DEBUG_profile)  
		printf("ProfileManager : Rename Operation failed : Profile exists. Provide a different new name for profile.\n");
#endif
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
		rv = m_reg->OpenDefault();

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
	nsCOMPtr<nsIEnumerator> enumKeys;
    nsIRegistry::Key sourceKey, destKey, profileRootKey;
    
	nsresult rv = m_reg->OpenDefault();

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
	                rv = m_reg->EnumerateValues(sourceKey, getter_AddRefs(enumKeys));

					if (NS_SUCCEEDED(rv)) {
						rv = enumKeys->First();
                    
						// Enumerate subkeys till done.
						while( NS_SUCCEEDED( rv ) && !enumKeys->IsDone() ) 
						{
							nsCOMPtr<nsISupports> base;
							rv = enumKeys->CurrentItem(getter_AddRefs(base));
                    
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
        rv = m_reg->OpenDefault();

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
	
	int idx = 0;

    // Check result.
    if ( m_reg != nsnull ) 
	{
        // Open the registry.
        rv = m_reg->OpenDefault();

        if (NS_SUCCEEDED(rv)) 
		{
            // Enumerate all subkeys (immediately) under the given node.
            nsCOMPtr<nsIEnumerator> enumKeys;
            nsIRegistry::Key key;

            rv = m_reg->GetSubtree(nsIRegistry::Common, "Profiles", &key);

            if (NS_SUCCEEDED(rv)) 
			{
                rv = m_reg->EnumerateSubtrees( key, getter_AddRefs(enumKeys));

                if (NS_SUCCEEDED(rv)) 
				{

                    rv = enumKeys->First();

                    // Enumerate subkeys till done.
                    while( NS_SUCCEEDED( rv ) && !enumKeys->IsDone() ) 
                    {
						nsCOMPtr<nsISupports> base;
						rv = enumKeys->CurrentItem(getter_AddRefs(base));

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
												PL_strcpy(gProfiles[idx], profile);
												PL_strcat(gProfiles[idx], " - migrate");
											}
											else
											{
												PL_strcpy(gProfiles[idx], profile);
											}

#if defined(DEBUG_profile)
												printf("profile%d = %s\n", idx, gProfiles[idx]);
#endif
										}
									}
								}
								NS_RELEASE(node);
					        }
						}	
					    rv = enumKeys->Next();

						if (NS_FAILED(rv))
						{
#if defined(DEBUG_profile)
								printf( "Error advancing enumerator, rv=0x%08X\n", (int)rv );
#endif
						}
						idx++;
					}
					g_numProfiles = idx;
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

	/*
	 * Need to load new profile prefs.
	 */
    rv = loadPrefs();
	
	// First, set the profile to be the current profile.
	// So that FileLocation services grabs right directory when it needs to.
	if (NS_SUCCEEDED(rv))
	{
		rv = m_reg->OpenDefault();

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

        NS_WITH_SERVICE(nsIFileLocator, locator, kFileLocatorCID, &rv);
			
		if (NS_SUCCEEDED(rv) && locator)
			rv = locator->ForgetProfileDir();

		if (NS_SUCCEEDED(rv))
		{
			m_prefs->StartUp();
		}
		
	}

	// we need to re-read the bookmarks here, other wise the user
	// won't have there 4.x bookmarks until after they exit and come back
	NS_WITH_SERVICE(nsIBookmarksService, bookmarks, kBookmarksCID, &rv);
	if (NS_FAILED(rv)) return rv;
	rv = bookmarks->ReadBookmarks();

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

#if defined(XP_PC) || defined(XP_MAC)

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
	
                NS_WITH_SERVICE(nsIFileLocator, locator, kFileLocatorCID, &rv);
		if (NS_FAILED(rv) || !locator) return NS_ERROR_FAILURE;

		nsFileSpec oldAppRegistry;
	
		// Get current profile, make the new one a sibling...
		rv = locator->GetFileLocation(nsSpecialFileSpec::App_Registry50, &oldAppRegistry);
		nsServiceManager::ReleaseService(kFileLocatorCID, locator);
		if (NS_FAILED(rv))
			 return NS_ERROR_FAILURE;
		oldAppRegistry.SetLeafName(OLD_WINREG);
        
#error DONT_USE_GETCSTRING
		rv = m_reg->Open(oldAppRegistryGetCString());
		************************/

#ifdef XP_PC
		// Registry file has been traditionally stored in the windows directory (XP_PC).
		nsSpecialSystemDirectory systemDir(nsSpecialSystemDirectory::Win_WindowsDirectory);

		// Append the name of the old registry to the path obtained.
		PL_strcpy(oldRegFile, systemDir.GetNativePathCString());
		PL_strcat(oldRegFile, oldWinReg);
#endif


#ifdef XP_MAC
		nsSpecialSystemDirectory *regLocation = NULL;


        regLocation = new nsSpecialSystemDirectory(nsSpecialSystemDirectory::Mac_SystemDirectory);

		// Append the name of the old registry to the path obtained.
		*regLocation += "Preferences";
		*regLocation += oldMacReg;
		
        PL_strcpy(oldRegFile, regLocation->GetNativePathCString());
#endif
		rv = m_reg->Open(oldRegFile);

		// Enumerate 4x tree and create an array of that information.
		if (NS_SUCCEEDED(rv))
		{
            // Enumerate all subkeys (immediately) under the given node.
            nsCOMPtr<nsIEnumerator> enumKeys;

            if (NS_SUCCEEDED(rv)) 
			{
            
				rv = m_reg->EnumerateSubtrees(nsIRegistry::Users,
                                              getter_AddRefs(enumKeys));

                if (NS_SUCCEEDED(rv)) 
				{
                
					rv = enumKeys->First();
                    
					// Enumerate subkeys till done.
                    while( NS_SUCCEEDED( rv ) && !enumKeys->IsDone() ) 
                    {
						nsCOMPtr<nsISupports> base;
						rv = enumKeys->CurrentItem(getter_AddRefs(base));

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
				}
            }
			m_reg->Close();
			
			if (g_numOldProfiles > 0)
			{
				UpdateMozProfileRegistry();
			}
		}
	}
#else
    char *unixProfileName = PR_GetEnv(PROFILE_NAME_ENVIRONMENT_VARIABLE);
    char *unixProfileDirectory = PR_GetEnv(PROFILE_HOME_ENVIRONMENT_VARIABLE);
	
    if (!unixProfileName || !unixProfileDirectory || (PL_strlen(unixProfileName) == 0) || (PL_strlen(unixProfileDirectory) == 0)) {
	    unixProfileName = PR_GetEnv(USER_ENVIRONMENT_VARIABLE);
	    unixProfileDirectory = PR_GetEnv(HOME_ENVIRONMENT_VARIABLE);
    }

    if (unixProfileName && unixProfileDirectory) {
	PL_strcpy(gOldProfiles[g_numOldProfiles], nsUnescape(unixProfileName));
	PL_strcpy(gOldProfLocations[g_numOldProfiles], unixProfileDirectory);
	PL_strcat(gOldProfLocations[g_numOldProfiles], "/.netscape");
#ifdef DEBUG_profile
	printf("unix profile is %s, unix profile dir is %s\n",gOldProfiles[g_numOldProfiles],gOldProfLocations[g_numOldProfiles]);
#endif
	g_numOldProfiles++;
    }

    if (g_numOldProfiles > 0) {
        UpdateMozProfileRegistry();
    }

#endif /* XP_PC || XP_MAC */

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

#if defined(DEBUG_profile)
    printf("Entered UpdateMozProfileRegistry.\n");
#endif

	for (int idx = 0; idx < g_numOldProfiles; idx++)
	{
		// Check result.
		if (m_reg != nsnull)
		{
			// Open the registry file.
			rv = m_reg->OpenDefault();
    
			if (NS_SUCCEEDED(rv))
			{
				nsIRegistry::Key key;
				rv = m_reg->GetSubtree(nsIRegistry::Common, "Profiles", &key);

				if (NS_SUCCEEDED(rv))
				{
					nsIRegistry::Key newKey;

					rv = m_reg->SetString(key, "NeedMigration", "true");

					rv = m_reg->GetSubtree(key, gOldProfiles[idx], &newKey);
			
					if (NS_FAILED(rv))
					{
						rv = m_reg->AddSubtree(key, gOldProfiles[idx], &newKey);
		
						if (NS_SUCCEEDED(rv))
						{
							nsFileSpec profileDir(gOldProfLocations[idx]);
						
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

	return rv;
}

// Migrate a selected profile
// Set the profile to the current profile....debatable.
// Calls PrefMigration service to do the Copy and Diverge
// of 4x Profile information
NS_IMETHODIMP nsProfile::MigrateProfile(const char* profileName)
{

	nsresult rv = NS_OK;

#if defined(DEBUG_profile)
	printf("Inside Migrate Profile routine.\n" );
#endif

	nsFileSpec oldProfDir;
	nsFileSpec newProfDir;

	GetProfileDir(profileName, &oldProfDir);

	// Create new profile dir path
        NS_WITH_SERVICE(nsIFileLocator, locator, kFileLocatorCID, &rv);
	if (NS_FAILED(rv) || !locator) return NS_ERROR_FAILURE;
	
    // Get current profile, make the new one a sibling...
	nsCOMPtr<nsIFileSpec> newSpec;
       	rv = locator->GetFileLocation(nsSpecialFileSpec::App_DefaultUserProfileRoot50, getter_AddRefs(newSpec));
	if (!newSpec)
	     return NS_ERROR_FAILURE;
    	newSpec->GetFileSpec(&newProfDir);
	newProfDir += profileName;


	// Call migration service to do the work.
	nsCOMPtr <nsIPrefMigration> pPrefMigrator;


	rv = nsComponentManager::CreateInstance(kPrefMigrationCID, 
                                          nsnull,
					NS_GET_IID(nsIPrefMigration),
					getter_AddRefs(pPrefMigrator));
    if (NS_FAILED(rv)) return rv;
        
	char *oldProfDirStr = nsnull;
	char *newProfDirStr = nsnull;
	
	rv = GetStringFromSpec(newProfDir, &newProfDirStr); 
	if (NS_SUCCEEDED(rv)) {               
		rv = GetStringFromSpec(oldProfDir, &oldProfDirStr);               
		if (NS_SUCCEEDED(rv)) {                             
			rv = pPrefMigrator->ProcessPrefs(oldProfDirStr,  newProfDirStr);
		}
	}
	PR_FREEIF(oldProfDirStr);
	PR_FREEIF(newProfDirStr);

	if (NS_FAILED(rv)) return rv;

	//unmark migrate flag
	if (NS_SUCCEEDED(rv))
	{
		// Check result.
		if ( m_reg != nsnull ) 
		{
	        // Open the registry.
		    rv = m_reg->OpenDefault();

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

NS_IMETHODIMP nsProfile::ProcessPREGInfo(const char* data)
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
#if defined(DEBUG_profile)  
		printf("\nProfiles : PREG Cookie user profile name = %s\n", userProfileName.ToNewCString());
#endif
	}

	if (serviceState.Length())
	{
		serviceIndex  = serviceState.Find("=", 0);
		delimIndex    = serviceState.Find("[-]", serviceIndex-1);

		serviceState.Mid(userServiceDenial, serviceIndex+1,delimIndex-(serviceIndex+1));

#if defined(DEBUG_profile)  
		printf("\nProfiles : PREG Cookie netcenter service option = %s\n", userServiceDenial.ToNewCString());
#endif
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
				rv = m_reg->OpenDefault();
    
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
		rv = m_reg->OpenDefault();
    
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
		rv = m_reg->OpenDefault();
    
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
        rv = m_reg->OpenDefault();

		if (NS_SUCCEEDED(rv))
		{
			// Enumerate all subkeys (immediately) under the given node.
			nsCOMPtr<nsIEnumerator> enumKeys;
			nsIRegistry::Key key;

			rv = m_reg->GetSubtree(nsIRegistry::Common, "Profiles", &key);

			if (NS_SUCCEEDED(rv))
			{
		        rv = m_reg->EnumerateSubtrees( key, getter_AddRefs(enumKeys));

				if (NS_SUCCEEDED(rv))
				{
			        int numKeys=0;
					rv = enumKeys->First();

			        while( NS_SUCCEEDED( rv ) && !enumKeys->IsDone() ) 
					{
						nsCOMPtr<nsISupports> base;

						rv = enumKeys->CurrentItem(getter_AddRefs(base));

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
								NS_RELEASE(node);
					        }
						}	
						rv = enumKeys->Next();
					}
					*numProfiles = numKeys;
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
	for (int i=0; i < g_numOldProfiles; i++)
	{
		rv = MigrateProfile(gOldProfiles[i]);
		if (NS_FAILED(rv)) return rv;
	}

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

	if (currProfileDir.Exists())
	{
		NS_WITH_SERVICE(nsIFileLocator, locator, kFileLocatorCID, &rv);
		if (NS_FAILED(rv) || !locator) return NS_ERROR_FAILURE;

	    	nsCOMPtr <nsIFileSpec> dirSpec;
        	rv = locator->GetFileLocation(nsSpecialFileSpec::App_DefaultUserProfileRoot50, getter_AddRefs(dirSpec));
        
		if (NS_FAILED(rv) || !dirSpec)
			return NS_ERROR_FAILURE;

		//Append profile name to form a directory name
	    	dirSpec->GetFileSpec(&newProfileDir);
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

NS_IMETHODIMP
nsProfile::OnShutdown(const nsCID& serviceCID, nsISupports* service) {

  if (serviceCID.Equals(kPrefCID)) {
    unloadPrefs();
    // are we supposed to do this?
    //nsServiceManager::ReleaseService(serviceCID, service);
  }

  return NS_OK;
}

nsresult
nsProfile::loadPrefs() {
  if (m_prefs) return NS_OK;

  nsresult rv;
  rv = nsServiceManager::GetService(kPrefCID, NS_GET_IID(nsIPref),
                                    (nsISupports **)&m_prefs, this);
  NS_ASSERTION(NS_SUCCEEDED(rv), "may bail without prefs service..\n");
  return rv;
}

nsresult
nsProfile::unloadPrefs() {
  if (m_prefs) {
    m_prefs->SavePrefFile(); 
    m_prefs->ShutDown();
    nsServiceManager::ReleaseService(kPrefCID, m_prefs);
    m_prefs=nsnull;
  }
  return NS_OK;
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

NS_IMETHODIMP
nsProfile::GetCurrentProfileDirFromJS(nsIFileSpec **spec)
{
	nsresult rv;
	nsFileSpec dir;
	rv = GetCurrentProfileDir(&dir);
	if (NS_FAILED(rv)) return rv;

	rv = NS_NewFileSpecWithSpec(dir, spec);
	return rv;
}

extern "C" NS_EXPORT PRBool NSCanUnload(nsISupports* serviceMgr)
{
  return PRBool(g_InstanceCount == 0 && g_LockCount == 0);
}

extern "C" NS_EXPORT nsresult NSRegisterSelf(nsISupports* aServMgr, const char *path)
{
  nsresult rv;

  NS_WITH_SERVICE1(nsIComponentManager, compMgr, aServMgr,
                   kComponentManagerCID, &rv);
  if (NS_FAILED(rv)) return rv;
  
  rv = compMgr->RegisterComponent(kProfileCID,
                                  "Profile Manager",
                                  NS_PROFILE_PROGID, path, 
                                  PR_TRUE, PR_TRUE);

  return rv;
}

extern "C" NS_EXPORT nsresult NSUnregisterSelf(nsISupports* aServMgr, const char *path)
{
  nsresult rv;

  NS_WITH_SERVICE1(nsIComponentManager, compMgr, aServMgr,
                   kComponentManagerCID, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->UnregisterComponent(kProfileCID, path);

  return rv;
}

