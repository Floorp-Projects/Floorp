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
#define VC_EXTRALEAN            // Exclude rarely-used stuff from Windows headers

//#include <afxwin.h>         // MFC core and standard components
//#include <afxext.h>         // MFC extensions


#include "dialshr.h"

#include "nsIAccount.h"
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
#include "nsString.h"
#include "nsIFileLocator.h"
#include "nsFileLocations.h"
#include "nsEscape.h"

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
//MUC variables
enum
{
//	kGetPluginVersion,
	kCheckEnv,
	kSelectAcctConfig,
	kSelectModemConfig,
	kSelectDialOnDemand,
	kConfigureDialer,
	kConnect,
	kHangup,
	kEditEntry,
	kDeleteEntry,
	kRenameEntry,
	kMonitor,
};

int                             gPlatformOS;
HINSTANCE               gDLL;

//CMucApp gMUCDLL;


//end of muc variables

// Globals to hold profile information
#ifdef XP_PC
static char *oldWinReg="nsreg.dat";
#endif

//static char gNewAccountData[_MAX_NUM_PROFILES][_MAX_LENGTH] = {'\0'};
static int	g_Count = 0;

//static char gProfiles[_MAX_NUM_PROFILES][_MAX_LENGTH] = {'\0'};
//static int	g_numProfiles = 0;

//static char gOldProfiles[_MAX_NUM_PROFILES][_MAX_LENGTH] = {'\0'};
//static char gOldProfLocations[_MAX_NUM_PROFILES][_MAX_LENGTH] = {'\0'};
//static int	g_numOldProfiles = 0;

//static PRBool renameCurrProfile = PR_FALSE;

// IID and CIDs of all the services needed
static NS_DEFINE_IID(kIAccountIID, NS_IAccount_IID);
static NS_DEFINE_CID(kAccountCID, NS_Account_CID);
//static NS_DEFINE_IID(kIFileLocatorIID, NS_IFILELOCATOR_IID);

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
//static NS_DEFINE_CID(kFileLocatorCID, NS_FILELOCATOR_CID);
//static NS_DEFINE_CID(kRegistryCID, NS_REGISTRY_CID);
//static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

#ifdef XP_PC
static NS_DEFINE_IID(kIPrefMigration_IID, NS_IPrefMigration_IID);
static NS_DEFINE_CID(kPrefMigration_CID, NS_PrefMigration_CID);
#endif

class nsAccount: public nsIAccount {
  NS_DECL_ISUPPORTS

private:
  static void useDefaultAccountFile(nsAccount *aAccountInst);
  static nsAccount *mInstance;
  nsIRegistry* m_reg;

public:
    nsAccount();
    virtual ~nsAccount();
    static nsAccount *GetInstance();

	//extra stuff from dial
//	BOOL LoadRasFunctions( LPCSTR lpszLibrary );
//	SizeofRAS95();		

    // Initialize/shutdown
    NS_IMETHOD Startup(void);
    NS_IMETHOD Shutdown();

    // Getters
    NS_IMETHOD GetAcctConfig(nsString returnData);
//    NS_IMETHOD GetModemConfig(nsString returnData);
    NS_IMETHOD SetDialerConfig(char* returnData);

	//**NS_IMETHOD PEPluginFunc( long selectorCode, void* paramBlock, void* returnData ); 
//    char* GetModemConfig(void);

};

nsAccount* nsAccount::mInstance = nsnull;

static PRInt32 g_InstanceCount = 0;
static PRInt32 g_LockCount = 0;

/*
 * Constructor/Destructor
 */
nsAccount::nsAccount()
: m_reg(nsnull)
{
  PR_AtomicIncrement(&g_InstanceCount);
  NS_INIT_REFCNT();
}

nsAccount::~nsAccount() {
  PR_AtomicDecrement(&g_InstanceCount);
  mInstance = nsnull;
}

nsAccount *nsAccount::GetInstance()
{
  if (mInstance == nsnull) {
    mInstance = new nsAccount();
  }
  return mInstance;
}

/*
 * nsISupports Implementation
 */

NS_IMPL_ISUPPORTS(nsAccount, kIAccountIID);

/*
 * nsIProfile Implementation
 */
// Creates an instance of the mozRegistry (m_reg)
// or any other registry that would be finalized for 5.0
// Adds subtree Profiles to facilitate profile operations
NS_IMETHODIMP nsAccount::Startup(void)
{
	nsresult rv = NS_OK;

//	SetDialogBkColor();        // Set dialog background color to gray
//	LoadStdProfileSettings();  // Load standard INI file options (including MRU)
	

	OSVERSIONINFO*  lpOsVersionInfo = new OSVERSIONINFO;
	lpOsVersionInfo->dwOSVersionInfoSize = sizeof( OSVERSIONINFO );

	if ( GetVersionEx( lpOsVersionInfo ) ) 
		gPlatformOS = (int)lpOsVersionInfo->dwPlatformId;
	else
		gPlatformOS = VER_PLATFORM_WIN32_WINDOWS;  // Make reasonable assumption

	switch ( gPlatformOS )
	{
		case VER_PLATFORM_WIN32_WINDOWS: //win95
			if( !LoadRasFunctions( "rasapi32.dll" ) )
				return FALSE;
		break;
	
		case VER_PLATFORM_WIN32_NT:             // win nt
			if( !LoadRasFunctionsNT( "rasapi32.dll" ) )
				return FALSE;
		break;

		default:
		break;
	} 

    return rv;
}


NS_IMETHODIMP nsAccount::Shutdown()
{
	if ( lpOsVersionInfo != NULL )
	{
		delete lpOsVersionInfo;
		lpOsVersionInfo = NULL;
	}

	return NS_OK;
}


/*
 * Getters
 */


// Gets the profiles directory for a given profile
// Sets the given profile to be a current profile
NS_IMETHODIMP nsAccount::GetAcctConfig(nsString returnData)
{
	nsresult rv=NS_OK;
	CONNECTIONPARAMS*               connectionNames;
	int                             numNames = 0;
	int                             i = 0, rtn; 
	nsString                                 str, tmp;

	switch ( gPlatformOS )
	{
		case VER_PLATFORM_WIN32_WINDOWS: //win95
			rtn = GetDialUpConnection95( &connectionNames, &numNames );
		break;

		case VER_PLATFORM_WIN32_NT:             // win nt
			rtn = GetDialUpConnectionNT( &connectionNames, &numNames );
		break;

		default:
			return FALSE;
	}

	if ( rtn )
	{
		returnData = "";
		if ( connectionNames != NULL )
		{    
			// pile up account names in a single array, separated by a ()
			for ( i = 0; i < numNames; i++ ) 
			{   
				tmp = connectionNames[ i ].szEntryName;
				str += tmp;
				str += "()";
			}
//			strcpy( returnData, (const char*)str ); 
			returnData =str;
			printf("this is the modem %s \n", returnData.ToNewCString());
			delete []connectionNames;
		}
	}

    return rv;
}


NS_IMETHODIMP nsAccount::SetDialerConfig(char* returnData)
{
	OSVERSIONINFO*  lpOsVersionInfo = new OSVERSIONINFO;
	lpOsVersionInfo->dwOSVersionInfoSize = sizeof( OSVERSIONINFO );

	if ( GetVersionEx( lpOsVersionInfo ) ) 
		gPlatformOS = (int)lpOsVersionInfo->dwPlatformId;
	else
		gPlatformOS = VER_PLATFORM_WIN32_WINDOWS;  // Make reasonable assumption

	switch ( gPlatformOS )
	{
		case VER_PLATFORM_WIN32_WINDOWS: //win95
			if( !LoadRasFunctions( "rasapi32.dll" ) )
				return FALSE;
		break;
	
		case VER_PLATFORM_WIN32_NT:             // win nt
			if( !LoadRasFunctionsNT( "rasapi32.dll" ) )
				return FALSE;
		break;

		default:
		break;
	} 

		long returnCode;
		nsresult rv=NS_OK;
		returnCode = (long) DialerConfig(returnData);
		printf("this is the dialer config \n");
return rv;
}



/***************************************************************************************/
/***********                           Account FACTORY                      ************/
/***************************************************************************************/
// Factory
class nsAccountFactory: public nsIFactory {
  NS_DECL_ISUPPORTS
  
  nsAccountFactory() {
    NS_INIT_REFCNT();
    PR_AtomicIncrement(&g_InstanceCount);
  }

  virtual ~nsAccountFactory() {
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

NS_IMPL_ISUPPORTS(nsAccountFactory, kFactoryIID);

nsresult nsAccountFactory::CreateInstance(nsISupports *aDelegate,
                                            const nsIID &aIID,
                                            void **aResult)
{
  if (aDelegate != nsnull) {
    return NS_ERROR_NO_AGGREGATION;
  }

  nsAccount *t = nsAccount::GetInstance();
  
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
  if (aClass.Equals(kAccountCID)) {
    nsAccountFactory *factory = new nsAccountFactory();
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

  rv = compMgr->RegisterComponent(kAccountCID, nsnull, nsnull, path, 
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

  rv = compMgr->UnregisterComponent(kAccountCID, path);

  (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
  return rv;
}

