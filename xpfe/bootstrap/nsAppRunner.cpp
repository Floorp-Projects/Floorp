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
#include "nsIWidget.h"
#include "nsIPref.h"
#include "plevent.h"

#include "nsIAppShell.h"
#include "nsICmdLineService.h"
#include "nsIAppShellService.h"
#include "nsAppShellCIDs.h"
#include "prprf.h"
#include "nsCRT.h"
#include "nsFileSpec.h"


// header file for profile manager
#ifdef XP_PC
	// include this for _getcwd call
	#include <direct.h>

	#include "nsIProfile.h"
#endif // XP_PC

#if defined(XP_MAC)
#include "macstdlibextras.h"
  // Set up the toolbox and (if DEBUG) the console.  Do this in a static initializer,
  // to make it as unlikely as possible that somebody calls printf() before we get initialized.
static struct MacInitializer { MacInitializer() { InitializeMacToolbox(); } } gInitializer;
#endif // XP_MAC

/* Define Class IDs */
static NS_DEFINE_IID(kAppShellServiceCID,   NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_IID(kCmdLineServiceCID,    NS_COMMANDLINE_SERVICE_CID);
static NS_DEFINE_CID(kPrefCID,              NS_PREF_CID);

/* Define Interface IDs */
static NS_DEFINE_IID(kIAppShellServiceIID,  NS_IAPPSHELL_SERVICE_IID);
static NS_DEFINE_IID(kICmdLineServiceIID,   NS_ICOMMANDLINE_SERVICE_IID);

// defined for profileManager
#ifdef XP_PC
static NS_DEFINE_CID(kProfileCID,           NS_PROFILE_CID);
#endif // XP_PC

/*********************************************
 AppCores
*********************************************/

//#if defined(XP_PC) || defined(XP_MAC)
#include "nsAppCoresCIDs.h"
#include "nsIDOMAppCoresManager.h"

//static nsIDOMAppCoresManager *appCoresManager = nsnull;
static NS_DEFINE_IID(kIDOMAppCoresManagerIID, NS_IDOMAPPCORESMANAGER_IID);
static NS_DEFINE_IID(kAppCoresManagerCID,     NS_APPCORESMANAGER_CID);
//#endif
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


int main(int argc, char* argv[])
{
  nsresult rv;
  nsString controllerCID;

  nsICmdLineService *  cmdLineArgs;
  char *  urlstr=nsnull;
  char *   progname = nsnull;
  char *   width=nsnull, *height=nsnull;
  char *  iconic_state=nsnull;

  PRInt32 widthVal  = 615;
  PRInt32 heightVal = 480;

  nsIAppShellService* appShell;
  nsIDOMAppCoresManager *appCoresManager;
  nsIURL* url;
  nsIPref *prefs;

  // initializations for profile manager
  #ifdef XP_PC
	char * currProfileName=nsnull;
	char * currProfileDir=nsnull;
	PRBool profileDirSet = PR_FALSE;
	nsIProfile *profile;
  #endif // XP_PC

  /* 
   * initialize all variables that are NS_IF_RELEASE(...) during
   * cleanup...
   */
  url             = nsnull;
  prefs           = nsnull;
  appShell        = nsnull;
  cmdLineArgs     = nsnull;
  appCoresManager = nsnull;

  char* cmdResult = nsnull;

  #ifdef XP_PC
      profile  = nsnull;
  #endif // XP_PC

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
  #ifdef XP_PC
  rv = nsServiceManager::GetService(kProfileCID, 
                                    nsIProfile::GetIID(), 
                                    (nsISupports **)&profile);
  

  if (NS_FAILED(rv)) {
    goto done;
  }
  profile->Startup(nsnull);

  #endif // XP_PC



  /*
   * Start up the core services:
   *  - Command-line processor.
   */

  rv = nsServiceManager::GetService(kCmdLineServiceCID,
                                    kICmdLineServiceIID,
                                    (nsISupports **)&cmdLineArgs);
  if (NS_FAILED(rv)) {
    fprintf(stderr, "Could not obtain CmdLine processing service\n");
    goto done;
  }

  // Initialize the cmd line service 
  rv = cmdLineArgs->Initialize(argc, argv);
  if (rv  == NS_ERROR_INVALID_ARG) {
    PrintUsage();
    goto done;
  }
  
  // Get the URL to load
  rv = cmdLineArgs->GetURLToLoad(&urlstr);
  if (rv  == NS_ERROR_INVALID_ARG) {
    PrintUsage();
    goto done;
  }
  // Default URL if one was not provided in the cmdline
  // Changed by kostello on 2/10/99 to look for -editor
  // or -mail command line and load the appropriate URL.
  // Please let me know if this is incorrect and I will
  // change it. -- Greg Kostello
  if (nsnull == urlstr){

	// check for command line arguments for profile manager
	#ifdef XP_PC
	
	// -P command line option works this way:
	// apprunner -P profilename 
	// runs the app using the profile <profilename> 
	// remembers profile for next time 
	rv = cmdLineArgs->GetCmdLineValue("-P", &cmdResult);
    if (NS_SUCCEEDED(rv))
    {
		if (cmdResult) {
			nsFileSpec* profileDir = nsnull;
			currProfileName = cmdResult;

			fprintf(stderr, "ProfileName : %s\n", cmdResult);
			
			profile->GetProfileDir(currProfileName, &profileDir);
			printf("** ProfileDir  :  %s **\n", profileDir->GetCString());
			
			currProfileDir = (char *)profileDir->GetCString();
			if (currProfileDir){
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
			currProfileName = strtok(cmdResult, " ");
            currProfileDir = strtok(NULL, " ");
			
			if (!currProfileDir) {
				char currDir[_MAX_PATH] = {'\0'};
				_getcwd(currDir, _MAX_PATH);
				currProfileDir = currDir;
			}

			nsFileSpec profileDir(currProfileDir);

			fprintf(stderr, "profileName & profileDir are: %s\n", cmdResult);
			profile->SetProfileDir(currProfileName, profileDir);
	  		profileDirSet = PR_TRUE;
			
		}
    }
	#endif // XP_PC
    
    rv = cmdLineArgs->GetCmdLineValue("-editor", &cmdResult);
    if (NS_SUCCEEDED(rv))
    {
      if (cmdResult && (strcmp("1",cmdResult)==0))
        urlstr = "resource:/res/samples/EditorAppShell.xul";
    }
    if (nsnull == urlstr)
    {
      rv = cmdLineArgs->GetCmdLineValue("-mail", &cmdResult);
      if (NS_SUCCEEDED(rv))
      {
        if (cmdResult && (strcmp("1",cmdResult)==0))
          urlstr = "resource:/res/mailnews/messenger/mailshell.xul";
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
    goto done;
  }
  if (width) {
    PR_sscanf(width, "%d", &widthVal);
    fprintf(stderr, "Width is set to %d\n", widthVal);
  } else {
    fprintf(stderr, "width was not set\n");
  }
  
  // Get the value of -height option
  rv = cmdLineArgs->GetCmdLineValue("-height", &height);
  if (NS_FAILED(rv)) {
    goto done;
  }
  if (height) {
    PR_sscanf(height, "%d", &heightVal);
    fprintf(stderr, "height is set to %d\n", heightVal);
  } else {
    fprintf(stderr, "height was not set\n");
  }
  
  /*
   * check if we have a profile directory
   */

  #ifdef XP_PC
  if (!profileDirSet) {
	  int numProfiles = 0;
	  profile->GetProfileCount(&numProfiles);

	  // no profiles exists: creates "default" profile and 
	  // sets its directory to your CWD
	  if (numProfiles == 0) {	
		  char currDir[_MAX_PATH] = {'\0'};
		  //XP_MAC impact on this....!
		  //Is there a call to get current working directory ?
		  _getcwd(currDir, _MAX_PATH);
  		  nsFileSpec profileDir(currDir);
		  profile->SetProfileDir("default", profileDir);
		  currProfileName = "default";
		  currProfileDir  = currDir;
	  }
	  // one profile exists: use that profile
	  else if (numProfiles == 1) {
		  char *profileName = nsnull;
		  nsFileSpec* profileDir = nsnull;
		  profileDir = new nsFileSpec;

		  profile->GetSingleProfile(&profileName);
		  profile->GetProfileDir(profileName, &profileDir);
		  currProfileName = profileName;
		  currProfileDir = (char *)profileDir->GetCString();
	  }

	  // multiple profiles exist: we'll use the same profile as last time 
	  // (see following rules) 
      // (if we can't figure out what the last profile used was for some reason, 
	  // we'll pick the first one as returned from the registry query) 
	  else {
		  char *profileName = nsnull;
		  nsFileSpec* profileDir = nsnull;
		  profileDir = new nsFileSpec;
		
		  profile->GetCurrentProfile(&profileName);
		  if (profileName) {
			  profile->GetProfileDir(profileName, &profileDir);
			  currProfileName = profileName;
	  		  currProfileDir = (char *)profileDir->GetCString();
		  }
		  else {
		  	  profile->GetFirstProfile(&profileName);
			  profile->GetProfileDir(profileName, &profileDir);
			  currProfileName = profileName;
	  		  currProfileDir = (char *)profileDir->GetCString();
		  }
	  }

	  if (currProfileName && currProfileDir) {
		fprintf(stderr, "ProfileName : %s\n", currProfileName);
		fprintf(stderr, "ProfileDir  : %s\n", currProfileDir);
	  }
  }

 #endif // XP_PC


  /*
   * Load preferences
   */
  rv = nsServiceManager::GetService(kPrefCID, 
                                    nsIPref::GetIID(), 
                                    (nsISupports **)&prefs);
  if (NS_FAILED(rv)) {
    goto done;
  }
  prefs->Startup(nsnull);
  

  /*
   * Create the Application Shell instance...
   */
  rv = nsServiceManager::GetService(kAppShellServiceCID,
                                    kIAppShellServiceIID,
                                    (nsISupports**)&appShell);
  if (NS_FAILED(rv)) {
    goto done;
  }

  /*
   * Initialize the Shell...
   */
  rv = appShell->Initialize();
  if (NS_FAILED(rv)) {
    goto done;
  }
 
  /*
   * Post an event to the shell instance to load the AppShell 
   * initialization routines...  
   * 
   * This allows the application to enter its event loop before having to 
   * deal with GUI initialization...
   */
  ///write me...
  nsIWebShellWindow* newWindow;
  
  rv = NS_NewURL(&url, urlstr);
  if (NS_FAILED(rv)) {
    goto done;
  }

  /* ********************************************************************* */
  /* This is a temporary hack to get mail working with autoregistration
   * This won't succeed unless messenger was found
   * through autoregistration, and will go away when the service manager
     * is accessable from JavaScript
     */
  /* Comments/questions to alecf@netscape.com */
  {
	/*MESSENGER*/
    nsIAppShellService *messenger;
    const char *messengerProgID = "component://netscape/messenger";
    nsresult result;

    /* this is so ugly, but ProgID->CLSID mapping seems to be broken -alecf */
#define NS_MESSENGERBOOTSTRAP_CID                 \
    { /* 4a85a5d0-cddd-11d2-b7f6-00805f05ffa5 */      \
      0x4a85a5d0, 0xcddd, 0x11d2,                     \
      {0xb7, 0xf6, 0x00, 0x80, 0x5f, 0x05, 0xff, 0xa5}}

    NS_DEFINE_CID(kCMessengerBootstrapCID, NS_MESSENGERBOOTSTRAP_CID);
    
    result = nsComponentManager::CreateInstance(kCMessengerBootstrapCID,
                                                nsnull,
                                                nsIAppShellService::GetIID(),
                                                (void **)&messenger);
    if (NS_SUCCEEDED(result)) {
      printf("The Messenger component is available. Initializing...\n");
      result = messenger->Initialize();
    }
 
	/*COMPOSER*/
      nsIAppShellService *composer;
    const char *composerProgID = "component://netscape/composer";

    /* this is so ugly, but ProgID->CLSID mapping seems to be broken -alecf */
#define NS_COMPOSERBOOTSTRAP_CID                 \
	{ /* 82041531-D73E-11d2-82A9-00805F2A0107 */      \
		0x82041531, 0xd73e, 0x11d2,                     \
		{0x82, 0xa9, 0x0, 0x80, 0x5f, 0x2a, 0x1, 0x7}}

    NS_DEFINE_CID(kCComposerBootstrapCID, NS_COMPOSERBOOTSTRAP_CID);
    
    result = nsComponentManager::CreateInstance(kCComposerBootstrapCID,
                                                nsnull,
                                                nsIAppShellService::GetIID(),
                                                (void **)&composer);
    if (NS_SUCCEEDED(result)) {
      printf("The Composer component is available. Initializing...\n");
      result = composer->Initialize();
    }
  }
  /* End of mailhack */
  /* ********************************************************************* */

  /* Kick off appcores */
  rv = nsServiceManager::GetService(kAppCoresManagerCID,
                                    kIDOMAppCoresManagerIID,
                                    (nsISupports**)&appCoresManager);
	if (NS_SUCCEEDED(rv)) {
		if (appCoresManager->Startup() != NS_OK) {
		  appCoresManager->Shutdown();
      nsServiceManager::ReleaseService(kAppCoresManagerCID, appCoresManager);
		}
  }

  /*
   * XXX: Currently, the CID for the "controller" is passed in as an argument 
   *      to CreateTopLevelWindow(...).  Once XUL supports "controller" 
   *      components this will be specified in the XUL description...
   */
  controllerCID = "43147b80-8a39-11d2-9938-0080c7cb1081";
  rv = appShell->CreateTopLevelWindow(nsnull, url, controllerCID, newWindow,
                   nsnull, nsnull, widthVal, heightVal);

  NS_RELEASE(url);
  if (NS_FAILED(rv)) {
    goto done;
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
    prefs->Shutdown();
    nsServiceManager::ReleaseService(kPrefCID, prefs);
  }

  /* Release the global profile... */
  if (profile) {
    profile->Shutdown();
    nsServiceManager::ReleaseService(kProfileCID, profile);
  }

  /* 
   * Translate the nsresult into an appropriate platform-specific return code.
   */
  return TranslateReturnValue(rv);
}
