/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsIURL.h"
#ifdef NECKO
#include "nsNeckoUtil.h"
#endif // NECKO
#include "nsIWidget.h"
#include "nsIBrowserWindow.h"
#include "nsIWebShellWindow.h"
#include "nsIPref.h"
#include "plevent.h"
#include "prmem.h"

#include "nsIAppShell.h"
#include "nsICmdLineService.h"
#include "nsIThread.h"
#include "nsIAppShellService.h"
#include "nsIAppShellComponent.h"
#include "nsAppShellCIDs.h"
#include "prprf.h"
#include "nsCRT.h"
#include "nsFileSpec.h"
#include "nsIFileSpec.h"
#include "nsIFileLocator.h"
#include "nsIPrefWindow.h"
#include "nsFileLocations.h"
#include "nsFileStream.h"
#include "nsSpecialSystemDirectory.h"
#include "nsIWalletService.h"
#include "nsIWebShell.h"
#ifdef NECKO
#include "nsICookieService.h"
#endif // NECKO
#include "nsIWindowMediator.h"
static NS_DEFINE_IID(kIWindowMediatorIID,NS_IWINDOWMEDIATOR_IID);
static NS_DEFINE_CID(kWindowMediatorCID, NS_WINDOWMEDIATOR_CID);

#define PREF_GENERAL_STARTUP_BROWSER "general.startup.browser"
#define PREF_GENERAL_STARTUP_MAIL "general.startup.mail"
#define PREF_GENERAL_STARTUP_NEWS "general.startup.news"
#define PREF_GENERAL_STARTUP_EDITOR "general.startup.editor"
#define PREF_GENERAL_STARTUP_CALENDAR "general.startup.calendar"

#ifdef DEBUG
#include "prlog.h"
#endif

// Temporary stuff.
#include "nsIDOMToolkitCore.h"
#include "nsAppCoresCIDs.h"
static NS_DEFINE_CID( kToolkitCoreCID, NS_TOOLKITCORE_CID );

#ifdef MOZ_FULLCIRCLE
#include "fullsoft.h"
#endif

// header file for profile manager
#include "nsIProfile.h"
// Uncomment this line to skip the profile code compilation
//#include "profileSwitch.h"

#if defined(XP_MAC)

#include "macstdlibextras.h"
#include <TextServices.h>

// Set up the toolbox and (if DEBUG) the console.  Do this in a static initializer,
// to make it as unlikely as possible that somebody calls printf() before we get initialized.
static struct MacInitializer { MacInitializer() { InitializeMacToolbox(); } } gInitializer;


class stTSMCloser
{
public:
	stTSMCloser()
  {
    // TSM is initialized in InitializeMacToolbox
  };
	
	~stTSMCloser()
	{
		(void)CloseTSMAwareApplication();
	}
};
#endif // XP_MAC

/* Define Class IDs */
static NS_DEFINE_CID(kAppShellServiceCID,   NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_CID(kCmdLineServiceCID,    NS_COMMANDLINE_SERVICE_CID);
static NS_DEFINE_CID(kPrefCID,              NS_PREF_CID);
static NS_DEFINE_CID(kFileLocatorCID,       NS_FILELOCATOR_CID);
static NS_DEFINE_IID(kWalletServiceCID,     NS_WALLETSERVICE_CID);

static NS_DEFINE_IID(kIWalletServiceIID, NS_IWALLETSERVICE_IID);

#ifdef NECKO
static NS_DEFINE_CID(kCookieServiceCID,    NS_COOKIESERVICE_CID);
#endif // NECKO

// defined for profileManager
#if defined(NS_USING_PROFILES)
static NS_DEFINE_CID(kProfileCID,           NS_PROFILE_CID);
#endif // NS_USING_PROFILES

/*********************************************
 AppCores
*********************************************/

#include "nsAppCoresCIDs.h"
#include "nsIDOMAppCoresManager.h"
static NS_DEFINE_CID(kAppCoresManagerCID,     NS_APPCORESMANAGER_CID);
#include "nsNativeAppSupport.h"
/*********************************************/
// Default implemenations for nativeAppSupport
// If your platform implements these functions if def out this code.
#if !defined (XP_MAC )
void NS_ShowSplashScreen()
{	
}

void NS_HideSplashScreen()
{
}

PRBool NS_CanRun() 
{
	return PR_TRUE;
}
#endif 
/*
 * This routine translates the nsresult into a platform specific return
 * code for the application...
 */
static int TranslateReturnValue(nsresult aResult)
{
  if (NS_SUCCEEDED(aResult)) {
    return 0;
  }
  return 1;
}

#ifdef XP_MAC
#include "nsCommandLineServiceMac.h"
#endif

extern "C" void NS_SetupRegistry_1();

static void
PrintUsage(void)
{
  fprintf(stderr, "Usage: apprunner <url>\n");
  fprintf(stderr, "\t<url>:  a fully defined url string like http:// etc..\n");
}

//----------------------------------------------------------------------------------------
static PRBool CheckAndRunPrefs(nsICmdLineService* cmdLineArgs)
  //----------------------------------------------------------------------------------------
{
  // Support the "-pref" command-line option, which just puts up the pref window, so that
  // apprunner becomes a "control panel". The "OK" and "Cancel" buttons will quit
  // the application.
  char* cmdResult;
  nsresult rv = cmdLineArgs->GetCmdLineValue("-pref", &cmdResult);
  if (NS_SUCCEEDED(rv) && cmdResult && (PL_strcmp("1",cmdResult) == 0))
  {
    nsIPrefWindow* prefWindow;
    rv = nsComponentManager::CreateInstance(
                                            NS_PREFWINDOW_PROGID,
                                            nsnull,
                                            nsIPrefWindow::GetIID(),
                                            (void **)&prefWindow);
    if (NS_SUCCEEDED(rv))
      prefWindow->ShowWindow(nsString("Apprunner::main()").GetUnicode(), nsnull, nsnull);
    NS_IF_RELEASE(prefWindow);
    return PR_TRUE;
  }
  return PR_FALSE;
} // CheckandRunPrefs

static void InitFullCircle()
{
  // initialization for Full Circle
#ifdef MOZ_FULLCIRCLE
  FC_ERROR fcstatus = FC_ERROR_FAILED;
  fcstatus = FCInitialize();
	  
  // Print out error status.
  switch(fcstatus)
  {
    case FC_ERROR_OK:
      printf("Talkback loaded Ok.\n");
      break;
    case FC_ERROR_CANT_INITIALIZE:
      printf("Talkback error: Can't initialize.\n");
      break;
    case FC_ERROR_NOT_INITIALIZED:
      printf("Talkback error: Not initialized.\n");
      break;
    case FC_ERROR_ALREADY_INITIALIZED:
      printf("Talkback error: Already initialized.\n");
      break;
    case FC_ERROR_FAILED:
      printf("Talkback error: Failure.\n");
      break;
    case FC_ERROR_OUT_OF_MEMORY:
      printf("Talkback error: Out of memory.\n");
      break;
    case FC_ERROR_INVALID_PARAMETER:
      printf("Talkback error: Invalid parameter.\n");
      break;
    default:
      printf("Talkback error: Unknown error status.\n");
      break;
  }
#endif
}

static nsresult HandleEditorStartup( nsICmdLineService* cmdLineArgs, nsIPref *prefs,  PRBool heedGeneralStartupPrefs)
{
	char* cmdResult = nsnull;
	char *  urlstr=nsnull;
	nsString withArgs;
	nsresult rv;
	PRBool forceLaunchEditor = PR_FALSE;

	if (heedGeneralStartupPrefs) {
		prefs->GetBoolPref(PREF_GENERAL_STARTUP_EDITOR,&forceLaunchEditor);
	}
  
	rv = cmdLineArgs->GetCmdLineValue("-edit", &cmdResult);
  if (NS_SUCCEEDED(rv))
  {
    if (forceLaunchEditor || (cmdResult && (PL_strcmp("1",cmdResult)==0))) {
      urlstr = "chrome://editor/content/";
      withArgs = "chrome://editor/content/EditorInitPage.html";
     
     	NS_WITH_SERVICE(nsIDOMToolkitCore, toolkit, kToolkitCoreCID, &rv);
      if (NS_SUCCEEDED(rv))
      {
        toolkit->ShowWindowWithArgs( urlstr, nsnull, withArgs );
     	}
    }
    // Check for -editor -- this will eventually go away
    if (nsnull == urlstr)
    {
      rv = cmdLineArgs->GetCmdLineValue("-editor", &cmdResult);
      if (NS_SUCCEEDED(rv))
      {
        if (cmdResult && (PL_strcmp("1",cmdResult)==0)) {
          printf(" -editor no longer supported, use -edit instead!\n");
          return NS_ERROR_FAILURE;		
        }
      }
    }

     
  }
    
	return NS_OK;
}

static nsresult OpenChromURL( char * urlstr, PRInt32 height = NS_SIZETOCONTENT, PRInt32 width = NS_SIZETOCONTENT )
{
	nsIURI* url = nsnull;
	nsresult  rv;
	rv = NS_NewURI(&url, urlstr);
	if ( NS_FAILED( rv ) )
		return rv;
	nsCOMPtr<nsIWebShellWindow> newWindow;
	NS_WITH_SERVICE(nsIAppShellService, appShell, kAppShellServiceCID, &rv);
 	rv = appShell->CreateTopLevelWindow(nsnull, url,
                                      PR_TRUE, PR_TRUE, NS_CHROME_ALL_CHROME,
                                      nsnull, width, height,
                                      getter_AddRefs(newWindow));
  NS_IF_RELEASE( url );
  return rv;
}

static nsresult HandleMailStartup( nsICmdLineService* cmdLineArgs, nsIPref *prefs,  PRBool heedGeneralStartupPrefs)
{
	char* cmdResult = nsnull;
	char *  urlstr=nsnull;
	nsString withArgs;
	nsresult rv;
	PRBool forceLaunchMail = PR_FALSE;
	PRBool forceLaunchNews = PR_FALSE;

	
	PRInt32 height  = NS_SIZETOCONTENT;
	PRInt32 width  = NS_SIZETOCONTENT;
	// Get the value of -width option
	char* tempString = NULL;
	rv = cmdLineArgs->GetCmdLineValue("-width", &tempString);
	if (NS_FAILED(rv))
    return rv;
	
	if (tempString)
    PR_sscanf(tempString, "%d", &width);
	  
	// Get the value of -height option
	rv = cmdLineArgs->GetCmdLineValue("-height", &tempString);
	if (NS_FAILED(rv))
		return rv;
	  
	if (tempString)
		PR_sscanf(tempString, "%d", &height);
	  
	

  if (heedGeneralStartupPrefs) {
                prefs->GetBoolPref(PREF_GENERAL_STARTUP_MAIL,&forceLaunchMail);
  }    
  rv = cmdLineArgs->GetCmdLineValue("-mail", &cmdResult);
  if (NS_SUCCEEDED(rv))
  {
    if (forceLaunchMail || (cmdResult && (PL_strcmp("1",cmdResult)==0)))
    {
      OpenChromURL("chrome://messenger/content/", height, width);
          
    }
  }


  if (heedGeneralStartupPrefs) {
                prefs->GetBoolPref(PREF_GENERAL_STARTUP_NEWS,&forceLaunchNews);
  }    
  rv = cmdLineArgs->GetCmdLineValue("-news", &cmdResult);
  if (NS_SUCCEEDED(rv))
  {
    if (forceLaunchNews || (cmdResult && (PL_strcmp("1",cmdResult)==0)))
      OpenChromURL("chrome://messenger/content/", height, width);
  }
	
  rv = cmdLineArgs->GetCmdLineValue("-compose", &cmdResult);
  if (NS_SUCCEEDED(rv))
  {
    if (cmdResult && (PL_strcmp("1",cmdResult)==0))
    {
      urlstr = "chrome://messengercompose/content/";
      withArgs = "chrome://editor/content/EditorInitPage.html";
  	  
  	  NS_WITH_SERVICE(nsIDOMToolkitCore, toolkit, kToolkitCoreCID, &rv);
      if (NS_SUCCEEDED(rv))
      {
        toolkit->ShowWindowWithArgs( urlstr, nsnull, withArgs );
      }
    }
  }
	
  rv = cmdLineArgs->GetCmdLineValue("-addressbook", &cmdResult);
  if (NS_SUCCEEDED(rv))
  {
    if (cmdResult && (PL_strcmp("1",cmdResult)==0))
      OpenChromURL("chrome://addressbook/content/",height, width);
  }
	
	return NS_OK;    
}

static nsresult HandleBrowserStartup( nsICmdLineService* cmdLineArgs, nsIPref *prefs,  PRBool heedGeneralStartupPrefs)
{
	char* cmdResult = nsnull;
	
	nsString withArgs;
	nsresult rv;
	PRBool forceLaunchBrowser = PR_FALSE;
	
	PRInt32 height  = NS_SIZETOCONTENT;
	PRInt32 width  = NS_SIZETOCONTENT;
	// Get the value of -width option
	char* tempString = NULL;
	rv = cmdLineArgs->GetCmdLineValue("-width", &tempString);
	if (NS_FAILED(rv))
    return rv;
	
	if (tempString)
    PR_sscanf(tempString, "%d", &width);
	
	  
	// Get the value of -height option
	rv = cmdLineArgs->GetCmdLineValue("-height", &tempString);
	if (NS_FAILED(rv))
		return rv;
	
	  
	if (tempString)
		PR_sscanf(tempString, "%d", &height);
	
  if (heedGeneralStartupPrefs) {
  	prefs->GetBoolPref(PREF_GENERAL_STARTUP_BROWSER,&forceLaunchBrowser);
  }  
  rv = cmdLineArgs->GetCmdLineValue("-chrome", &cmdResult); 
  if (forceLaunchBrowser) {
	rv = OpenChromURL("chrome://navigator/content/", height, width ); 
  }
  else if (NS_SUCCEEDED(rv) && cmdResult) {
    rv = OpenChromURL(cmdResult, height, width);
  }
  
	return rv;    
}

// This should be done by app shell enumeration someday
static nsresult DoCommandLines( nsICmdLineService* cmdLine, PRBool heedGeneralStartupPrefs )
{
	nsresult  rv;
	
	NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv); 
	if (NS_FAILED(rv)) return rv;

	rv = HandleEditorStartup( cmdLine, prefs, heedGeneralStartupPrefs);
	if ( NS_FAILED( rv ) ) return rv;
		
	rv = HandleMailStartup( cmdLine, prefs, heedGeneralStartupPrefs);
	if ( NS_FAILED( rv ) ) return rv;
	
	rv = HandleBrowserStartup( cmdLine, prefs, heedGeneralStartupPrefs);
	return rv;
}

static nsresult Ensure1Window( nsICmdLineService* cmdLineArgs)
{
	nsresult rv;
	NS_WITH_SERVICE(nsIWindowMediator, windowMediator, kWindowMediatorCID, &rv);
	nsCOMPtr<nsISimpleEnumerator> windowEnumerator;

  if (NS_SUCCEEDED(windowMediator->GetEnumerator(nsnull, getter_AddRefs(windowEnumerator))))
  {
    PRBool more;
	      
    windowEnumerator->HasMoreElements(&more);
    if ( !more )
    {
      // No window exists so lets create a browser one
      PRInt32 height  = NS_SIZETOCONTENT;
      PRInt32 width  = NS_SIZETOCONTENT;
				
      // Get the value of -width option
      char* tempString = NULL;
      rv = cmdLineArgs->GetCmdLineValue("-width", &tempString);
      if (NS_FAILED(rv))
        return rv;
      if (tempString)
        PR_sscanf(tempString, "%d", &width);
				
				  
      // Get the value of -height option
      rv = cmdLineArgs->GetCmdLineValue("-height", &tempString);
      if (NS_FAILED(rv))
        return rv;

      if (tempString)
        PR_sscanf(tempString, "%d", &height);
				 
				
      rv = OpenChromURL("chrome://navigator/content/", height, width );
    }
	}
	return rv;
}

static nsresult main1(int argc, char* argv[])
{
  nsresult rv;

#ifndef XP_MAC
  // Unbuffer debug output (necessary for automated QA performance scripts).
  setbuf( stdout, 0 );
#endif
  
  NS_VERIFY(NS_SUCCEEDED(nsIThread::SetMainThread()), "couldn't set main thread");
 
  InitFullCircle();

 #if XP_MAC 
    stTSMCloser  tsmCloser;
  
  InitializeMacCommandLine( argc, argv);

 #endif

  // XXX: This call will be replaced by a registry initialization...
  NS_SetupRegistry_1();

  // Start up the core services:

  // Initialize the cmd line service
  NS_WITH_SERVICE(nsICmdLineService, cmdLineArgs, kCmdLineServiceCID, &rv);
 
  if (NS_FAILED(rv)) {
    fprintf(stderr, "Could not obtain CmdLine processing service\n");
    return rv;
  }
 
  rv = cmdLineArgs->Initialize(argc, argv);
  if (rv  == NS_ERROR_INVALID_ARG) {
    PrintUsage();
    return rv;
  }

  // Kick off appcores
  NS_WITH_SERVICE(nsIDOMAppCoresManager, appCoresManager, kAppCoresManagerCID, &rv);
  
  if (NS_SUCCEEDED(rv)){
		if (appCoresManager->Startup() != NS_OK) {
		  appCoresManager->Shutdown();
      nsServiceManager::ReleaseService(kAppCoresManagerCID, appCoresManager);
		}
  }
#ifdef DEBUG
  printf("started appcores\n");
#endif
  
    
  // Create the Application Shell instance...
  NS_WITH_SERVICE(nsIAppShellService, appShell, kAppShellServiceCID, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = appShell->Initialize( cmdLineArgs );
  if ( NS_FAILED(rv) ) 
  	return rv; 
#ifdef DEBUG
  printf("initialized appshell\n");
#endif

  NS_WITH_SERVICE(nsIProfile, profileMgr, kProfileCID, &rv);
  if (NS_SUCCEEDED(rv))
    profileMgr->StartupWithArgs(cmdLineArgs);

  if ( CheckAndRunPrefs(cmdLineArgs) )
  	return NS_OK;
  

#ifdef NECKO
  // fire up an instance of the cookie manager.
  // I'm doing this using the serviceManager for convenience's sake.
  // Presumably an application will init it's own cookie service a 
  // different way (this way works too though).
  NS_WITH_SERVICE(nsICookieService, cookieService, kCookieServiceCID, &rv);
  // quiet the compiler
  (void)cookieService;
#ifndef XP_MAC
  // Until the cookie manager actually exists on the Mac we don't want to bail here
  if (NS_FAILED(rv))
    return rv;
#endif // XP_MAC
#endif // NECKO
	
	// Enumerate AppShellComponenets
	appShell->EnumerateAndInitializeComponents();
	
	// This will go away once Components are handling there own commandlines
	// if we have no command line arguments, we need to heed the
	// "general.startup.*" prefs
	// if we had no command line arguments, argc == 1.
	rv = DoCommandLines( cmdLineArgs, (argc == 1) );
	if ( NS_FAILED(rv) )
    return rv;
     
  // Make sure there exists at least 1 window. 
	rv = Ensure1Window( cmdLineArgs );
  if (NS_FAILED(rv))
    return rv;

  // Fire up the walletService
  NS_WITH_SERVICE(nsIWalletService, walletService, kWalletServiceCID, &rv);
  if ( NS_SUCCEEDED(rv) )
  	walletService->WALLET_FetchFromNetCenter();
  NS_HideSplashScreen();
  // Start main event loop
  rv = appShell->Run();

  /*
   * Shut down the Shell instance...  This is done even if the Run(...)
   * method returned an error.
   */
  (void) appShell->Shutdown();

  return rv ;
}

static
void DumpHelp(char *appname)
{
	printf("%s: help info here\n", appname);
}

static
void DumpVersion(char *appname)
{
	printf("%s: version info\n", appname);
}

#ifdef DEBUG_ramiro
#define CRAWL_STACK_ON_SIGSEGV
#endif // DEBUG_ramiro

#ifdef CRAWL_STACK_ON_SIGSEGV

#include <signal.h>
#include <unistd.h>
#include "nsTraceRefcnt.h"

extern "C" char * strsignal(int);

static char _progname[1024] = "huh?";

void
ah_crap_handler(int signum)
{
  PR_CurrentThread();

  printf("prog = %s\npid = %d\nsignal = %s\n",
         _progname,
         getpid(),
         strsignal(signum));
  
  char stack[4096];
  
  nsTraceRefcnt::WalkTheStack(stack, sizeof(stack));
  
  // Convert all spaces between symbols to newlines for readability
  char * needle  = "+0x";
  char * haystack  = stack;
  char * sp = NULL;
  
  while((sp = strstr(haystack,needle)))
  {
    char * ws = strchr(sp,' ');

    if (ws)
    {
      *ws = '\n';

      haystack = ws + 1;
    }
  }

  printf("stack = %s\n\n",stack);

  printf("Sleeping for 5 minutes.\n");
  printf("Type 'gdb %s %d' to attatch your debugger to this thread.\n",
         _progname,
         getpid());

  sleep(300);

  printf("Done sleeping...\n");
} 
#endif // CRAWL_STACK_ON_SIGSEGV

int main(int argc, char* argv[])
{
#ifdef CRAWL_STACK_ON_SIGSEGV
  strcpy(_progname,argv[0]);
  signal(SIGSEGV, ah_crap_handler);
  signal(SIGILL, ah_crap_handler);
  signal(SIGABRT, ah_crap_handler);
#endif // CRAWL_STACK_ON_SIGSEGV

  nsresult rv;

  /* -help and -version should return quick */
  if (argc == 2) {
	if ((PL_strcmp(argv[1], "-h") == 0) || (PL_strcmp(argv[1], "-help") == 0) || (PL_strcmp(argv[1], "--help") == 0)) {
		DumpHelp(argv[0]);
		return 0;
	}
	else if ((PL_strcmp(argv[1], "-v") == 0) || (PL_strcmp(argv[1], "-version") == 0) || (PL_strcmp(argv[1], "--version") == 0)) {
		DumpVersion(argv[0]);
		return 0;
	}
  }
    
  if( !NS_CanRun() )
    return 1; 
  NS_ShowSplashScreen();
  rv = NS_InitXPCOM(NULL, NULL, NULL);
  NS_ASSERTION( NS_SUCCEEDED(rv), "NS_InitXPCOM failed" );


  nsresult result = main1( argc, argv );
	
  {
    // Scoping this in a block to force the profile service to be
    // released.

    // this is supposed to happen automatically when XPCOM shuts down, but
    // that doesn't always occur!
    NS_WITH_SERVICE(nsIProfile, profileMgr, kProfileCID, &rv);
    if (NS_SUCCEEDED(rv))
      profileMgr->Shutdown();
    // calling this explicitly will cut down on a large number of leaks we're
    // seeing:
  }
    
#if 0
  if ( unsigned long count = NS_TotalWebShellsInExistence() )
  {
    printf("!!!Warning: there are still %l instances of nsWebShell outstanding.  OK?\n", count);
    char cc;
    scanf("%c", &cc);
  }
#endif

  rv = NS_ShutdownXPCOM( NULL );
  NS_ASSERTION(NS_SUCCEEDED(rv), "NS_ShutdownXPCOM failed");
  return TranslateReturnValue( result );
}
