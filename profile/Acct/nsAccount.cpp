/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#define VC_EXTRALEAN            // Exclude rarely-used stuff from Windows headers

//#include <afxwin.h>         // MFC core and standard components
//#include <afxext.h>         // MFC extensions

#define NS_IMPL_IDS

#include "dialshr.h"

#include "nsIAccount.h"
#include "nsIPref.h"

//#include "pratom.h"
#include "prmem.h"
//#include "nsIRegistry.h"
#include "nsIFactory.h"
#include "plstr.h"
#include "nsIFileSpec.h"
#include "nsFileStream.h"
#include "nsString.h"
#include "nsReadableUtils.h"
//#include "nsIFileLocator.h"
//#include "nsFileLocations.h"
//#include "nsEscape.h"

#include "nsIPersistentProperties2.h"
#include "nsIServiceManager.h"
#include "nsNetUtil.h"
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
#include "nsIComponentManager.h"
#include "nsIEnumerator.h"
#include <iostream.h>  //BAD DOG -- no biscuit!

#include "nsSpecialSystemDirectory.h"

#define TEST_URL "resource:/res/test.properties"

#define NETLIB_DLL "netlib.dll"
#define RAPTORBASE_DLL "raptorbase.dll"
#define XPCOM_DLL "xpcom32.dll"

static NS_DEFINE_IID(kIPersistentPropertiesIID, NS_IPERSISTENTPROPERTIES_IID);

//#ifdef XP_PC
#include <direct.h>
//#endif

// included for XPlatform coding 
//#include "nsFileStream.h"
//#include "nsSpecialSystemDirectory.h"


#ifdef XP_MAC
//#include "nsIPrompt.h"
//#include "nsIStreamListener.h"
#endif
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"

#define _MAX_LENGTH			256
#define _MAX_NUM_PROFILES	50
//MUC variables
/**
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
**/
int                             gPlatformOS;
HINSTANCE               gDLL;

//CMucApp gMUCDLL;
#define _MAX_NUM_PROFILES 50
#define _MAX_LENGTH 256

//end of muc variables

// Globals to hold profile information
#ifdef XP_PC
//static char *oldWinReg="nsreg.dat";
#endif

//static char gNewAccountData[_MAX_NUM_PROFILES][_MAX_LENGTH] = {'\0'};
static int	g_Count = 0;

static char gNCIInfo[_MAX_NUM_PROFILES][_MAX_LENGTH] = {'\0'};
static int	NCICount = 0;

static char IspLocation[_MAX_NUM_PROFILES][_MAX_LENGTH] = {'\0'};
static int	LocCount = 0;

static char IspPhone[_MAX_NUM_PROFILES][_MAX_LENGTH] = {'\0'};
static int	PhoneCount = 0;

char domainname[256];
char dnsp[24];
char dnss[24];

// IID and CIDs of all the services needed
static NS_DEFINE_CID(kAccountCID, NS_Account_CID);
//static NS_DEFINE_IID(kIFileLocatorIID, NS_IFILELOCATOR_IID);

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
//static NS_DEFINE_CID(kFileLocatorCID, NS_FILELOCATOR_CID);
//static NS_DEFINE_CID(kRegistryCID, NS_REGISTRY_CID);
//static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);


class nsAccount: public nsIAccount {
  NS_DECL_ISUPPORTS

private:
  static void useDefaultAccountFile(nsAccount *aAccountInst);
  static nsAccount *mInstance;
//  nsIRegistry* m_reg;

public:
    nsAccount();
    virtual ~nsAccount();
    static nsAccount *GetInstance();

    int Parent(const char* relativePath, nsFileSpec& outParent);
	int IterateDirectoryChildren(nsFileSpec& startChild);
	int GetNCIValues(nsString MiddleValue);
	int GetConfigValues(nsString fileName);
	//extra stuff from dial
//	BOOL LoadRasFunctions( LPCSTR lpszLibrary );
//	SizeofRAS95();		

    // Initialize/shutdown
    NS_IMETHOD Startup(void);
    NS_IMETHOD Shutdown();

    // Getters
    NS_IMETHOD GetAcctConfig(nsString& AccountList);
	NS_IMETHOD GetModemConfig(nsString& ModemList);
	NS_IMETHOD GetLocation(nsString& Locat);
	NS_IMETHOD GetSiteName(nsString& SiteList);
	NS_IMETHOD GetPhone(nsString& PhoneList);
	NS_IMETHOD LoadValues(void);
	NS_IMETHOD CheckForDun(nsString& dun);



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
//: m_reg(nsnull)
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

NS_IMPL_ISUPPORTS1(nsAccount, nsIAccount)

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

//----------------------------------------------------------------------------------------
int nsAccount::Parent(
    const char* relativePath,
    nsFileSpec& outParent)
//----------------------------------------------------------------------------------------
{
    nsFilePath myTextFilePath(relativePath, PR_TRUE);
    const char* pathAsString = (const char*)myTextFilePath;
	nsOutputConsoleStream mConsole;

    nsFileSpec mySpec(myTextFilePath);

    mySpec.GetParent(outParent);
    nsFilePath parentPath(outParent);
    nsFileURL url(parentPath);
    mConsole
        << "GetParent() on "
        << "\n\t" << pathAsString
        << "\n yields "
        << "\n\t" << (const char*)parentPath
        << "\n or as a URL"
        << "\n\t" << (const char*)url
        << nsEndl;
    return 0;
}

//----------------------------------------------------------------------------------------
int nsAccount::IterateDirectoryChildren(nsFileSpec& startChild)
//----------------------------------------------------------------------------------------
{
    // - Test of directory iterator
	nsresult rv=NS_OK;
	nsOutputConsoleStream mConsole;

    nsFileSpec grandparent;
    startChild.GetParent(grandparent); // should be the original default directory.
    nsFilePath grandparentPath(startChild);
    mConsole << "Forwards listing of " << (const char*)grandparentPath << ":" << nsEndl;
	printf ("Outside the iteration \n");
    for (nsDirectoryIterator i(startChild, +1); i.Exists(); i++)
    {
	printf ("Inside the iteration \n");

        char* itemName = (i.Spec()).GetLeafName();
		PL_strcpy(gNCIInfo[NCICount], itemName);
        mConsole << '\t' << itemName << nsEndl;
		mConsole << '\t' << "This is the arraynumber "<<NCICount<<"and value" <<gNCIInfo[NCICount] << nsEndl;
        nsCRT::free(itemName);
		NCICount++;
    }
    return 0;
}

int nsAccount::GetNCIValues(nsString MiddleValue)
{
  nsresult ret;

  nsIInputStream* in = nsnull;
  nsAutoString Trial(NS_LITERAL_STRING("resource:/res/acct/NCI_Dir/") +
                     MiddleValue);
  printf("this is the trial value %s \n", NS_LossyConvertUCS2toASCII(Trial).get());
  nsCOMPtr<nsIIOService> service(do_GetService(kIOServiceCID, &ret));
  if (NS_FAILED(ret)) return ret;


  nsCOMPtr<nsIURI> uri;
  ret = NS_NewURI(getter_AddRefs(uri), Trial, nsnull, service);
  if (NS_FAILED(ret)) return ret;

  nsIChannel *channel = nsnull;
  ret = NS_OpenURI(&channel, uri, service);
  if (NS_FAILED(ret)) return ret;

  ret = channel->Open(&in);
  if (NS_FAILED(ret)) return ret;

  nsIPersistentProperties* props = nsnull;

  ret = nsComponentManager::CreateInstance(kPersistentPropertiesCID, NULL,
    kIPersistentPropertiesIID, (void**) &props);
  if (NS_FAILED(ret) || (!props)) {
    printf("create nsIPersistentProperties failed\n");
    return 1;
  }
  ret = props->Load(in);
  if (NS_FAILED(ret)) {
    printf("cannot load properties\n");
    return 1;
  }

  int ind = 1;
 
//  while (1) {
    char name[256];
    name[0] = 0;
    nsAutoString v;
    sprintf(name, "%s", "SiteName");
    ret = props->GetStringProperty(NS_ConvertASCIItoUCS2(name), v);

    if (NS_FAILED(ret) || (!v.Length())) {

	printf("Failed to return the Get property \n");
    }

	PL_strcpy(IspLocation[LocCount], v.get());

      cout << "the value is " << IspLocation[LocCount] << "." << endl;
	LocCount++;

    sprintf(name, "%s", "Phone");
    ret = props->GetStringProperty(NS_ConvertASCIItoUCS2(name), v);

    if (NS_FAILED(ret) || (!v.Length())) {

      	printf("Failed to return the Get property \n");
    }
	char* value = ToNewCString(v);
	PL_strcpy(IspPhone[PhoneCount], value);
//    if (value) {
      cout << "the value is " << IspPhone[PhoneCount] << "." << endl;
//	  printf ("this is the %d and its value \n ",ind);
      delete[] value;
  	PhoneCount++;

//    }
//    else {
//      printf("%d: ToNewCString failed\n", ind);
//    }
//    ind++;
//  }

    return 1;
}

int nsAccount::GetConfigValues(nsString fileName)
{
	  nsresult ret;

  nsIInputStream* in = nsnull;
	nsString Trial; Trial.AssignWithConversion("resource:/res/acct/NCI_Dir/");
	Trial = Trial + fileName;
	printf("this is the trial value %s \n", NS_LossyConvertUCS2toASCII(Trial).get());
  nsCOMPtr<nsIIOService> service(do_GetService(kIOServiceCID, &ret));
  if (NS_FAILED(ret)) return ret;


  nsCOMPtr<nsIURI> uri;
  ret = NS_NewURI(getter_AddRefs(uri), Trial, nsnull, service);
  if (NS_FAILED(ret)) return ret;

  nsIChannel *channel = nsnull;
  ret = NS_OpenURI(&channel, uri, service);
  if (NS_FAILED(ret)) return ret;

  ret = channel->Open(&in);
  if (NS_FAILED(ret)) return ret;

	nsIPersistentProperties* props = nsnull;

  ret = nsComponentManager::CreateInstance(kPersistentPropertiesCID, NULL,
    kIPersistentPropertiesIID, (void**) &props);
  if (NS_FAILED(ret) || (!props)) {
    printf("create nsIPersistentProperties failed\n");
    return 1;
  }
  ret = props->Load(in);
  if (NS_FAILED(ret)) {
    printf("cannot load properties\n");
    return 1;
  }

  int ind = 1;
 
//  while (1) {
    char name[256];
    name[0] = 0;
    nsAutoString v;

    sprintf(name, "%s", "DomainName");
    ret = props->GetStringProperty(NS_ConvertASCIItoUCS2(name), v);

    if (NS_FAILED(ret) || (!v.Length())) {

	printf("Failed to return the Get property \n");
    }

	PL_strcpy(domainname, ToNewCString(v));

    sprintf(name, "%s", "DNSAddress");
    ret = props->GetStringProperty(NS_ConvertASCIItoUCS2(name), v);

    if (NS_FAILED(ret) || (!v.Length())) {

      	printf("Failed to return the Get property \n");
    }
	PL_strcpy(dnsp, ToNewCString(v));

    sprintf(name, "%s", "DNSAddress2");
    ret = props->GetStringProperty(NS_ConvertASCIItoUCS2(name), v);

    if (NS_FAILED(ret) || (!v.Length())) {

      	printf("Failed to return the Get property \n");
    }
	PL_strcpy(dnss, ToNewCString(v));

			printf (" This is the dnsp name %s \n",dnsp);
			printf (" This is the dnss name %s \n",dnss);
			printf (" This is the domain name %s \n",domainname);

//    }
//    else {
//      printf("%d: ToNewCString failed\n", ind);
//    }
//    ind++;
//  }

return 1;
}

NS_IMETHODIMP nsAccount::GetLocation(nsString& Locat)
{
	nsresult rv=NS_OK;
		for ( int index=0; index < 10; index++)
	{
			PL_strcpy(IspLocation[index],"San Jose");
	}

	for (int i=0; i < 10; i++)
	{
		if (i != 0)
		{
			Locat.AppendWithConversion(",");
		}
		Locat.AppendWithConversion(gNCIInfo[i]);
	}
	return rv;

}

NS_IMETHODIMP nsAccount::GetSiteName(nsString& SiteList)
{
	nsresult rv=NS_OK;
		for ( int i=0; i < LocCount; i++)
	{
		if (i != 0)
		{
			SiteList.AppendWithConversion(",");
		}
		SiteList.AppendWithConversion(IspLocation[i]);
		SiteList.AppendWithConversion("^");
		SiteList.AppendWithConversion(gNCIInfo[i]);

	}
	LocCount=0;
	return rv;
}

NS_IMETHODIMP nsAccount::GetPhone(nsString& PhoneList)
{
	nsresult rv=NS_OK;
		for (int i=0; i < PhoneCount; i++)
	{
		if (i != 0)
		{
			PhoneList.AppendWithConversion(",");
		}
		PhoneList.AppendWithConversion(IspPhone[i]);
	}
		PhoneCount =0;
	return rv;
}

NS_IMETHODIMP nsAccount::LoadValues(void)
{
	nsresult rv =NS_OK;
	nsOutputConsoleStream mConsole;
	nsFileSpec parent;
	int	rv1 = Parent("./NCI_Dir/*", parent);

	int rv2 = IterateDirectoryChildren(parent);
	mConsole << '\t' << "Before entering the sitenames and phone "<<NCICount<<" :"<< nsEndl;

	for (int icount =0; icount < NCICount; icount++)
	{
		int rv3 = GetNCIValues(NS_ConvertASCIItoUCS2(gNCIInfo[icount]));

		mConsole << '\t' << "This is the count number "<< icount <<" :"<< nsEndl;

	}
		mConsole << '\t' << "This is the arraynumber "<<NCICount<<" :"<< nsEndl;
	NCICount = 0;

		mConsole << '\t' << "This is the after setting to zero "<<NCICount<<" :"<< nsEndl;

	return rv;
}

NS_IMETHODIMP nsAccount::GetModemConfig(nsString& ModemList)
{
	nsresult rv=NS_OK;
	char**          modemResults;
	nsresult rve = nsAccount::Startup();


	int             numDevices;
	int             i;
	nsString                 str, tmp, returnData;
	modemResults=(char**)PR_MALLOC(20*256);

	if ( !::GetModemList( &modemResults, &numDevices ) )
	{
		if ( modemResults != NULL )
		{
			for ( i = 0; i < numDevices; i++ )
			{
				if ( modemResults[ i ] != NULL )
					delete []modemResults[ i ];
			}
			delete []modemResults;
		}
		return NULL;
	}

	// copy all entries to the array
//	returnData[ 0 ] = 0x00;
//	returnData ="";

	// pile up account names in a single array, separated by a ()
/*	for ( i = 0; i < numDevices; i++ ) 
	{   
		tmp = modemResults[ i ];
		str += tmp;
		str += "()";  
		delete []modemResults[ i ];
	}
*/
				for ( i=0; i < numDevices; i++)
			{
				if (i != 0)
				{
					ModemList.AppendWithConversion(",");
				}
				ModemList.AppendWithConversion(modemResults[i]);
      }

//	strcpy( returnData, (const char*)str ); 
//	returnData = modemResults[0];
//	returnData = tmp;
	printf("this is the modem inside the modemconfig %s \n", NS_LossyConvertUCS2toASCII(ModemList).get());
	delete []modemResults;


return rv;
}


// Gets the profiles directory for a given profile
// Sets the given profile to be a current profile
NS_IMETHODIMP nsAccount::GetAcctConfig(nsString& AccountList)
{
	nsresult rv=NS_OK;
	CONNECTIONPARAMS*               connectionNames;
	int                             numNames = 0;
	int                             i = 0, rtn; 
	nsString                                 str, tmp;

	nsresult rvu = nsAccount::Startup();

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
		if ( connectionNames != NULL )
		{    
			// pile up account names in a single array, separated by a ()
			for ( i=0; i < numNames; i++)
			{
					PL_strcpy(gNCIInfo[i],connectionNames[ i ].szEntryName);
			}

			for ( i=0; i < numNames; i++)
			{
				if (i != 0)
				{
					AccountList.AppendWithConversion(",");
				}
				AccountList.AppendWithConversion(gNCIInfo[i]);
			}

//			printf("this is the acct strings %s\n", NS_LossyConvertUCS2toASCII(AccountList).get());
			delete []connectionNames;
		}
	}
    return rv;
}


NS_IMETHODIMP nsAccount::SetDialerConfig(char* returnData)
{
/**	OSVERSIONINFO*  lpOsVersionInfo = new OSVERSIONINFO;
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
**/
		nsresult rve = nsAccount::Startup();
		long returnCode;
		nsresult rv=NS_OK;
		printf ("This is the return data %s \n", returnData);

//		if (NCICount > 0)
		    nsString data; data.AssignWithConversion(returnData);
    
			// Set the gathered info into an array
			SetDataArray(data);

			char* ncivalue = GetValue ("ncivalue");
			printf ("this is the nci value inside nsaccount.cpp %s \n",ncivalue );
		if (strcpy(ncivalue,"1"))
		{
			char* filename = GetValue("filename");
			printf (" This is the file name %s \n",filename);
			GetConfigValues(NS_ConvertASCIItoUCS2(filename));
			printf (" This is the dnsp name %s \n",dnsp);
			printf (" This is the dnss name %s \n",dnss);
			printf (" This is the domain name %s \n",domainname);

			printf ("This is the returndata %s \n",returnData);
			strcat(returnData,"domainname=");
			printf ("This is the returndata %s \n",returnData);
			strcat(returnData,domainname);
			printf ("This is the returndata %s \n",returnData);
			strcat(returnData,"%dnsp=");
			printf ("This is the returndata %s \n",returnData);
			strcat(returnData,dnsp);
			printf ("This is the returndata %s \n",returnData);
			strcat(returnData,"%dnss=");
			printf ("This is the returndata %s \n",returnData);
			strcat(returnData,dnss);
			printf ("This is the returndata %s \n",returnData);
			strcat(returnData,"%");
			printf ("This is the returndata %s \n",returnData);

		}

		returnCode = (long) DialerConfig(returnData);
		printf("this is the dialer config \n");
return rv;
}

NS_IMETHODIMP nsAccount::CheckForDun(nsString& dun)
{
	nsresult rv=NS_OK;
	dun.AppendWithConversion("0");
  PRBool dunvalue = CheckEnvironment();
	if (dunvalue )
	{
		dun.AppendWithConversion("1");
	}
	
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

NS_IMPL_ISUPPORTS1(nsAccountFactory, nsIFactory)

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
             const char *aContractID,
             nsIFactory **aFactory)
{
  if (aFactory == nsnull) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aClass.Equals(kAccountCID)) {
    nsAccountFactory *factory = new nsAccountFactory();
    nsresult res = CallQueryInterface(factory, aFactory);
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
                           NS_GET_IID(nsIComponentManager), 
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
                           NS_GET_IID(nsIComponentManager), 
                           (nsISupports**)&compMgr);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->UnregisterComponent(kAccountCID, path);

  (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
  return rv;
}

