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
#ifdef NECKO
#include "nsICookieService.h"
#endif // NECKO

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
#endif // XP_PC

/*********************************************
 AppCores
*********************************************/

#include "nsAppCoresCIDs.h"
#include "nsIDOMAppCoresManager.h"

//static nsIDOMAppCoresManager *appCoresManager = nsnull;
static NS_DEFINE_CID(kAppCoresManagerCID,     NS_APPCORESMANAGER_CID);

/*********************************************/

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
  char* cmdResult;
  nsresult rv = cmdLineArgs->GetCmdLineValue("-pref", &cmdResult);
  if (NS_SUCCEEDED(rv) && cmdResult && (strcmp("1",cmdResult) == 0))
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

int main1(int argc, char* argv[])
{
  nsresult rv;

#ifndef XP_MAC
  // Unbuffer debug output (necessary for automated QA performance scripts).
  setbuf( stdout, 0 );
#endif
  
  nsICmdLineService *  cmdLineArgs = nsnull;
  NS_VERIFY(NS_SUCCEEDED(nsIThread::SetMainThread()), "couldn't set main thread");

  char *  urlstr=nsnull;
  //char *   progname = nsnull;
  char *   width=nsnull, *height=nsnull;
  //char *  iconic_state=nsnull;

  PRInt32 widthVal  = 790;
  PRInt32 heightVal = 580;

  /* 
   * initialize all variables that are NS_IF_RELEASE(...) during
   * cleanup...
   */

  nsIAppShellService* appShell = nsnull;
  nsIDOMAppCoresManager *appCoresManager = nsnull;
  nsIURI* url = nsnull;
  nsIPref *prefs = nsnull;
  nsIWalletService *walletService;

#ifdef NECKO
  nsICookieService *cookieService = nsnull;
#endif // NECKO

  // initialization for Full Circle
#ifdef MOZ_FULLCIRCLE
  {
	  FC_ERROR fcstatus = FC_ERROR_FAILED;
	  fcstatus = FCInitialize();
	  
	  // Print out error status.
	  switch(fcstatus) {
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
  }
#endif

  // initializations for profile manager
#if defined(NS_USING_PROFILES)
	nsFileSpec currProfileDirSpec;
	PRBool profileDirSet = PR_FALSE;
	nsIProfile *profileService = nsnull;
	char *profstr = nsnull;
	int numProfiles = 0;
#endif // defined(NS_USING_PROFILES)

  char* cmdResult = nsnull;

  // These manage funky stuff with ensuring onload handler getting called at
  // the correct time.  The problem is that upcoming (or recent)
  // changes will result in the .xul window's onload handler getting called
  // prior to execution of nsIXULWindowCallback's ContructBeforeJavaScript.
  // This will break windows whose onload handler must execute subsequent to
  // that callback mechanism (i.e., those that may need "args").  The short-term
  // fix (ultimately, these windows will receive arguments via different means)
  // is to trigger the pseudo-onload handler from the ConstructBeforeJavaScript
  // code.  The problem is those windows that are not always constructed with an
  // nsIXULWindowCallbacks, i.e., the editor.  The fix here is to ensure that
  // the toolkit core's ShowWindowWithArgs function is used to open the editor.
  // Whew.  For details, call me (law@netscape.com, x2296).  This code is only
  // temporary!
  nsString withArgs;
  PRBool useArgs = PR_FALSE;

  /*
   * Initialize XPCOM.  Ultimately, this should be a function call such as
   * NS_XPCOM_Initialize(...).
   *
   * - PL_EventsLib(...)
   * - Repository
   * - ServiceManager
   */
  // XXX: This call will be replaced by a registry initialization...
  NS_SetupRegistry_1();

  // get and start the ProfileManager service
#if defined(NS_USING_PROFILES)
  rv = nsServiceManager::GetService(kProfileCID, 
                                    nsIProfile::GetIID(), 
                                    (nsISupports **)&profileService);
  

  if (NS_FAILED(rv)) {
    goto done;			// don't use goto in C++!
  }
  profileService->Startup(nsnull);
  profileService->GetProfileCount(&numProfiles); 
#endif // defined(NS_USING_PROFILES)


  /*
   * Start up the core services:
   *  - Command-line processor.
   */

  rv = nsServiceManager::GetService(kCmdLineServiceCID,
                                    nsICmdLineService::GetIID(),
                                    (nsISupports **)&cmdLineArgs);
  if (NS_FAILED(rv)) {
    fprintf(stderr, "Could not obtain CmdLine processing service\n");
    goto done;			// don't use goto in C++!
  }

  // Initialize the cmd line service 
  rv = cmdLineArgs->Initialize(argc, argv);
  if (rv  == NS_ERROR_INVALID_ARG) {
    PrintUsage();
    goto done;			// don't use goto in C++!
  }
  
#if 0
  // Get the URL to load
  rv = cmdLineArgs->GetURLToLoad(&urlstr);
  if (rv  == NS_ERROR_INVALID_ARG) {
    PrintUsage();
    goto done;			// don't use goto in C++!
  }
#endif  /* 0 */

  // Default URL if one was not provided in the cmdline
  // Changed by kostello on 2/10/99 to look for -editor
  // or -mail command line and load the appropriate URL.
  // Please let me know if this is incorrect and I will
  // change it. -- Greg Kostello
  if (nsnull == urlstr){

#if defined(NS_USING_PROFILES)

	printf("Profile Manager : Command Line Options : Begin\n");
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

			fprintf(stderr, "ProfileName : %s\n", cmdResult);
			
			rv = profileService->GetProfileDir(currProfileName, &currProfileDirSpec);
			printf("** ProfileDir  :  %s **\n", currProfileDirSpec.GetCString());
			
			if (NS_SUCCEEDED(rv)){
				profileDirSet = PR_TRUE;
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
				nsIFileLocator* locator = nsnull;
				rv = nsServiceManager::GetService(kFileLocatorCID, nsIFileLocator::GetIID(), (nsISupports**)&locator);
				if (NS_FAILED(rv))
				    return rv;
				if (!locator)
				  return NS_ERROR_FAILURE;
				
				// Get current profile, make the new one a sibling...
				nsIFileSpec* spec;
				rv = locator->GetFileLocation(nsSpecialFileSpec::App_UserProfileDirectory50, &spec);
				if (NS_FAILED(rv) || !spec)
					return NS_ERROR_FAILURE;
				spec->GetFileSpec(&currProfileDirSpec);
				NS_RELEASE(spec);
				currProfileDirSpec.SetLeafName(currProfileName);

				if (locator)
				{
					rv = locator->ForgetProfileDir();
					nsServiceManager::ReleaseService(kFileLocatorCID, locator);
				}
			}

			fprintf(stderr, "profileName & profileDir are: %s\n", cmdResult);
			profileService->SetProfileDir(currProfileName, currProfileDirSpec);
	  		profileDirSet = PR_TRUE;
			
		}
    }

	// Start Profile Manager
	rv = cmdLineArgs->GetCmdLineValue("-ProfileManager", &cmdResult);
    if (NS_SUCCEEDED(rv))
    {		
		if (cmdResult) {
			profstr = "resource:/res/profile/pm.xul"; 
		}
    }

	// Start Profile Wizard
	rv = cmdLineArgs->GetCmdLineValue("-ProfileWizard", &cmdResult);
    if (NS_SUCCEEDED(rv))
    {		
		if (cmdResult) {
			profstr = "resource:/res/profile/cpw.xul"; 
		}
    }

	// Start Migaration activity
	rv = cmdLineArgs->GetCmdLineValue("-installer", &cmdResult);
    if (NS_SUCCEEDED(rv))
    {		
		if (cmdResult) {
#ifdef XP_PC
			profileService->MigrateProfileInfo();

			int num4xProfiles = 0;
			profileService->Get4xProfileCount(&num4xProfiles); 

			if (num4xProfiles == 0 && numProfiles == 0) {
				profstr = "resource:/res/profile/cpw.xul"; 
			}
			else if (num4xProfiles == 1) {
				profileService->MigrateAllProfiles();
				profileService->GetProfileCount(&numProfiles);
			}
			else if (num4xProfiles > 1) {
				profstr = "resource:/res/profile/pm.xul";
			}
#endif
		}
    }

    printf("Profile Manager : Command Line Options : End\n");

#endif // defined(NS_USING_PROFILES)
    
    rv = cmdLineArgs->GetCmdLineValue("-edit", &cmdResult);
    if (NS_SUCCEEDED(rv))
    {
      if (cmdResult && (strcmp("1",cmdResult)==0)) {
        urlstr = "chrome://editor/content/";
        useArgs = PR_TRUE;
        withArgs = "chrome://editor/content/EditorInitPage.html";
      }
    }
    // Check for -editor -- this will eventually go away
    if (nsnull == urlstr)
    {
      rv = cmdLineArgs->GetCmdLineValue("-editor", &cmdResult);
      if (NS_SUCCEEDED(rv))
      {
        if (cmdResult && (strcmp("1",cmdResult)==0)) {
          printf(" -editor no longer supported, use -edit instead!\n");
          goto done;			// don't use goto in C++!
        }
      }
    }
    if (nsnull == urlstr)
    {
      rv = cmdLineArgs->GetCmdLineValue("-mail", &cmdResult);
      if (NS_SUCCEEDED(rv))
      {
        if (cmdResult && (strcmp("1",cmdResult)==0))
          urlstr = "chrome://messenger/content/";
      }
    }
    if (nsnull == urlstr)
    {
      rv = cmdLineArgs->GetCmdLineValue("-news", &cmdResult);
      if (NS_SUCCEEDED(rv))
      {
        if (cmdResult && (strcmp("1",cmdResult)==0))
          urlstr = "chrome://messenger/content/";
      }
    }
    if (nsnull == urlstr)
    {
      rv = cmdLineArgs->GetCmdLineValue("-compose", &cmdResult);
      if (NS_SUCCEEDED(rv))
      {
        if (cmdResult && (strcmp("1",cmdResult)==0))
        {
          urlstr = "chrome://messengercompose/content/";
          useArgs = PR_TRUE;
          withArgs = "chrome://editor/content/EditorInitPage.html";
       }
     }
    }
    if (nsnull == urlstr)
    {
      rv = cmdLineArgs->GetCmdLineValue("-addressbook", &cmdResult);
      if (NS_SUCCEEDED(rv))
      {
        if (cmdResult && (strcmp("1",cmdResult)==0))
          urlstr = "chrome://addressbook/content/";
      }
    }
    if (nsnull == urlstr) { 
      rv = cmdLineArgs->GetCmdLineValue("-chrome", &cmdResult); 
      if (NS_SUCCEEDED(rv) && cmdResult) { 
          urlstr = cmdResult; 
      } 
    } 
    if (nsnull == urlstr)
    {    
      urlstr = "chrome://navigator/content/";
    }
  }
  else
      fprintf(stderr, "URL to load is %s\n", urlstr);


  // Get the value of -width option
  rv = cmdLineArgs->GetCmdLineValue("-width", &width);
  if (NS_FAILED(rv)) {
    goto done;			// don't use goto in C++!
  }
  if (width) {
    PR_sscanf(width, "%d", &widthVal);
  }
  
  // Get the value of -height option
  rv = cmdLineArgs->GetCmdLineValue("-height", &height);
  if (NS_FAILED(rv)) {
    goto done;			// don't use goto in C++!
  }
  if (height) {
    PR_sscanf(height, "%d", &heightVal);
  }

  /*
   * Load preferences, causing them to be initialized, and hold a reference to them.
   */
  rv = nsServiceManager::GetService(kPrefCID, 
                                    nsIPref::GetIID(), 
                                    (nsISupports **)&prefs);
  if (NS_FAILED(rv))
    goto done;			// don't use goto in C++!

  /*
   * Create the Application Shell instance...
   */
  rv = nsServiceManager::GetService(kAppShellServiceCID,
                                    nsIAppShellService::GetIID(),
                                    (nsISupports**)&appShell);
  if (NS_FAILED(rv)) {
    goto done;			// don't use goto in C++!
  }

  /*
   * Initialize the Shell...
   */
  rv = appShell->Initialize( cmdLineArgs );
  if (NS_FAILED(rv)) {
    goto done;			// don't use goto in C++!
  }
 
  /*
   * Post an event to the shell instance to load the AppShell 
   * initialization routines...  
   * 
   * This allows the application to enter its event loop before having to 
   * deal with GUI initialization...
   */
  ///write me...
  
#ifndef NECKO
  rv = NS_NewURL(&url, urlstr);
#else
  rv = NS_NewURI(&url, urlstr);
#endif // NECKO

  if (NS_FAILED(rv)) {
    goto done;			// don't use goto in C++!
  }


  // Support the "-pref" command-line option, which just puts up the pref window, so that
  // apprunner becomes a "control panel". The "OK" and "Cancel" buttons will quit
  // the application.
  rv = cmdLineArgs->GetCmdLineValue("-pref", &cmdResult);
  if (NS_SUCCEEDED(rv) && cmdResult && (strcmp("1",cmdResult) == 0))
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
	goto done;
  }

  // Kick off appcores
  rv = nsServiceManager::GetService(kAppCoresManagerCID,
                                    nsIDOMAppCoresManager::GetIID(),
                                    (nsISupports**)&appCoresManager);
	if (NS_SUCCEEDED(rv)) {
		if (appCoresManager->Startup() != NS_OK) {
		  appCoresManager->Shutdown();
      nsServiceManager::ReleaseService(kAppCoresManagerCID, appCoresManager);
		}
  }

#if defined (NS_USING_PROFILES)


    	printf("Profile Manager : Profile Wizard and Manager activites : Begin\n");

	/* 
	 * If default profile is current, launch CreateProfile Wizard. 
	 */ 
	if (!profileDirSet)
	{
		nsIURI* profURL = nsnull;

		PRInt32 profWinWidth  = 615;
		PRInt32 profWinHeight = 500;

		nsIAppShellService* profAppShell;

		/*
	 	 * Create the Application Shell instance...
		 */
		rv = nsServiceManager::GetService(kAppShellServiceCID,
                                    nsIAppShellService::GetIID(),
                                    (nsISupports**)&profAppShell);
		if (NS_FAILED(rv))
			goto done;

		char *isPregInfoSet = nsnull;
		profileService->IsPregCookieSet(&isPregInfoSet); 
		
		PRBool pregPref = PR_FALSE;
		rv = prefs->GetBoolPref(PREG_PREF, &pregPref);

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
#ifdef NECKO
			rv = NS_NewURI(&profURL, profstr);
#else
			rv = NS_NewURL(&profURL, profstr);
#endif
	
			if (NS_FAILED(rv)) {
				goto done;
			} 

	 		nsCOMPtr<nsIWebShellWindow>  profWindow;
			rv = profAppShell->CreateTopLevelWindow(nsnull, profURL,
				PR_TRUE, PR_TRUE, NS_CHROME_ALL_CHROME,
				nsnull, profWinWidth, profWinHeight,
				getter_AddRefs(profWindow));

			NS_RELEASE(profURL);
		
			if (NS_FAILED(rv)) 
			{
				goto done;
			}

			/*
			 * Start up the main event loop...
			 */	
			rv = profAppShell->Run();
		}

		if (pregPref && PL_strcmp(isPregInfoSet, "true") != 0)
			profileService->ProcessPRegCookie();
	}
    	printf("Profile Manager : Profile Wizard and Manager activites : End\n");
#endif

  // Now we have the right profile, read the user-specific prefs.
  prefs->ReadUserPrefs();
 
  // Support the "-pref" command-line option, which just puts up the pref window, so that
  // apprunner becomes a "control panel". The "OK" and "Cancel" buttons will quit
  // the application.
  if (CheckAndRunPrefs(cmdLineArgs))
    goto done;

#ifdef NECKO
  // fire up an instance of the cookie manager.
  // I'm doing this using the serviceManager for convenience's sake.
  // Presumably an application will init it's own cookie service a 
  // different way (this way works too though).
  rv = nsServiceManager::GetService(kCookieServiceCID,
                                    nsCOMTypeInfo<nsICookieService>::GetIID(),
                                    (nsISupports **)&cookieService);
#ifndef XP_MAC
// Until the cookie manager actually exists on the Mac we don't want to bail here
  if (NS_FAILED(rv))
      goto done;
#endif // XP_MAC
#endif // NECKO

 

  if ( !useArgs ) {
      nsCOMPtr<nsIWebShellWindow> newWindow;
      rv = appShell->CreateTopLevelWindow(nsnull, url,
                               PR_TRUE, PR_TRUE, NS_CHROME_ALL_CHROME,
                               nsnull, NS_SIZETOCONTENT, NS_SIZETOCONTENT,
                               getter_AddRefs(newWindow));
  } else {
      nsIDOMToolkitCore* toolkit = nsnull;
      rv = nsServiceManager::GetService(kToolkitCoreCID,
                                        nsIDOMToolkitCore::GetIID(),
                                        (nsISupports**)&toolkit);
      if (NS_SUCCEEDED(rv)) {
          
          toolkit->ShowWindowWithArgs( urlstr, nsnull, withArgs );
          
          /* Release the toolkit... */
          if (nsnull != toolkit) {
            nsServiceManager::ReleaseService(kToolkitCoreCID, toolkit);
          }
      }
  }

  NS_RELEASE(url);
  if (NS_FAILED(rv)) {
    goto done;
  }

  // Fire up the walletService
  rv = nsServiceManager::GetService(kWalletServiceCID,
                                    kIWalletServiceIID,
                                     (nsISupports **)&walletService);
 
  if (NS_SUCCEEDED(rv)) {
      walletService->WALLET_FetchFromNetCenter();
  }

  /*
   * Start up the main event loop...
   */
  rv = appShell->Run();

  /*
   * Shut down the Shell instance...  This is done even if the Run(...)
   * method returned an error.
   */
  (void) appShell->Shutdown();

done:
  /* Release the services... */
#if 0
  if (nsnull != cmdLineArgs) {
    nsServiceManager::ReleaseService(kCmdLineProcessorCID, cmdLineArgs);
  }
#endif

#ifdef NECKO
  if (nsnull != cookieService)
    nsServiceManager::ReleaseService(kCookieServiceCID, cookieService);
#endif // NECKO

  if (nsnull != walletService)
    nsServiceManager::ReleaseService(kWalletServiceCID, walletService);


  /* Release the shell... */
  if (nsnull != appShell) {
    nsServiceManager::ReleaseService(kAppShellServiceCID, appShell);
  }

  /* Release the AppCoresManager... */
  if (nsnull != appCoresManager) {
		appCoresManager->Shutdown();
    nsServiceManager::ReleaseService(kAppCoresManagerCID, appCoresManager);
  }

  /* Release the global preferences... */
  if (prefs) {
    prefs->SavePrefFile(); 
    prefs->ShutDown();
    nsServiceManager::ReleaseService(kPrefCID, prefs);
  }

#if defined(NS_USING_PROFILES)
  /* Release the global profile service... */
  if (profileService) {
    profileService->Shutdown();
    nsServiceManager::ReleaseService(kProfileCID, profileService);
  }
#endif // defined(NS_USING_PROFILES)

#ifdef XP_MAC
	(void)CloseTSMAwareApplication();
#endif
  /* 
   * Translate the nsresult into an appropriate platform-specific return code.
   */

  return TranslateReturnValue(rv);
}

int main(int argc, char* argv[])
{
    nsresult rv;
    rv = NS_InitXPCOM(NULL);
    NS_ASSERTION(NS_SUCCEEDED(rv), "NS_InitXPCOM failed");

    int result = main1(argc, argv);

    // calling this explicitly will cut down on a large number of leaks we're
    // seeing:
    rv = NS_ShutdownXPCOM(NULL);
    NS_ASSERTION(NS_SUCCEEDED(rv), "NS_ShutdownXPCOM failed");

    return result;
}
