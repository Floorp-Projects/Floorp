/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIWidget.h"
#include "nsIBrowserWindow.h"
#include "nsIWebShellWindow.h"
#include "nsIPref.h"
#include "plevent.h"
#include "prmem.h"
#include "prnetdb.h"

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
#include "nsFileLocations.h"
#include "nsFileStream.h"
#include "nsSpecialSystemDirectory.h"
#include "nsIWalletService.h"
#include "nsIWebShell.h"
#include "nsIWindowMediator.h"
#include "nsIDOMWindow.h"
#include "nsIClipboard.h"
#include "nsISoftwareUpdate.h"
#include "nsSoftwareUpdateIIDs.h"
#include "nsISupportsPrimitives.h"
#include "nsICmdLineHandler.h"
#include "nsICategoryManager.h"
#include "nsXPIDLString.h"

#include "nsIContentHandler.h"
#include "nsIBrowserInstance.h"

#ifndef XP_MAC
#include "nsTimeBomb.h"
#endif

#if defined(DEBUG_sspitzer_) || defined(DEBUG_seth_)
#define DEBUG_CMD_LINE
#endif

static NS_DEFINE_CID(kSoftUpdateCID,     NS_SoftwareUpdate_CID);
static NS_DEFINE_IID(kIWindowMediatorIID,NS_IWINDOWMEDIATOR_IID);
static NS_DEFINE_CID(kWindowMediatorCID, NS_WINDOWMEDIATOR_CID);
static NS_DEFINE_CID(kWalletServiceCID,     NS_WALLETSERVICE_CID);
static NS_DEFINE_CID(kBrowserContentHandlerCID, NS_BROWSERCONTENTHANDLER_CID);


#ifndef XP_MAC
static NS_DEFINE_CID(kTimeBombCID,     NS_TIMEBOMB_CID);
#endif

#define HELP_SPACER_1   "\t"
#define HELP_SPACER_2   "\t\t"

#ifdef DEBUG
#include "prlog.h"
#endif

#ifdef MOZ_JPROF
#include "jprof.h"
#endif

// header file for profile manager
#include "nsIProfile.h"

#if defined(XP_UNIX)
  extern void InstallUnixSignalHandlers(const char *ProgramName);
#endif

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
static NS_DEFINE_CID(kProfileCID,           NS_PROFILE_CID);

#include "nsNativeAppSupport.h"

/*********************************************/
// Default implemenations for nativeAppSupport
// If your platform implements these functions if def out this code.
#if !defined (XP_MAC ) && !defined(NTO) && ( !defined( XP_PC ) || !defined( WIN32 ) )

nsresult NS_CreateSplashScreen( nsISplashScreen **aResult )
{	
    nsresult rv = NS_OK;
    if ( aResult ) {
        *aResult = 0;
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
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

extern "C" void NS_SetupRegistry_1(PRBool aNeedAutoreg);

static void
PrintUsage(void)
{
  fprintf(stderr, "Usage: apprunner <url>\n");
  fprintf(stderr, "\t<url>:  a fully defined url string like http:// etc..\n");
}

static nsresult OpenWindow( const char*urlstr, const PRUnichar *args ) {
    nsresult rv;
    NS_WITH_SERVICE(nsIAppShellService, appShellService, kAppShellServiceCID, &rv)
    if ( NS_SUCCEEDED( rv ) ) {
        nsCOMPtr<nsIDOMWindow> hiddenWindow;
        JSContext *jsContext;
        rv = appShellService->GetHiddenWindowAndJSContext( getter_AddRefs( hiddenWindow ),
                                                           &jsContext );
        if ( NS_SUCCEEDED( rv ) ) {
            void *stackPtr;
            jsval *argv = JS_PushArguments( jsContext,
                                            &stackPtr,
                                            "sssW",
                                            urlstr,
                                            "_blank",
                                            "chrome,dialog=no,all",
                                            args );
            if( argv ) {
                nsCOMPtr<nsIDOMWindow> newWindow;
                rv = hiddenWindow->OpenDialog( jsContext,
                                               argv,
                                               4,
                                               getter_AddRefs( newWindow ) );
                JS_PopArguments( jsContext, stackPtr );
            }
        }
    }
    return rv;
}

static nsresult OpenChromURL( const char * urlstr, PRInt32 height = NS_SIZETOCONTENT, PRInt32 width = NS_SIZETOCONTENT )
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


static void DumpArbitraryHelp() 
{
  nsresult rv;
  NS_WITH_SERVICE(nsICategoryManager, catman, "mozilla.categorymanager.1", &rv);
  if(NS_SUCCEEDED(rv) && catman) {
    nsCOMPtr<nsISimpleEnumerator> e;
    rv = catman->EnumerateCategory(COMMAND_LINE_ARGUMENT_HANDLERS, getter_AddRefs(e));
    if(NS_SUCCEEDED(rv) && e) {
      while (PR_TRUE) {
        nsCOMPtr<nsISupportsString> progid;
        rv = e->GetNext(getter_AddRefs(progid));
        if (NS_FAILED(rv) || !progid) break;

        nsXPIDLCString progidString;
        progid->ToString (getter_Copies(progidString));
        
#ifdef DEBUG_CMD_LINE
        printf("cmd line hander progid = %s\n", (const char *)progidString);
#endif /* DEBUG_CMD_LINE */
        
        nsCOMPtr <nsICmdLineHandler> handler = do_GetService((const char *)progidString, &rv);
        
        if (handler) {
          nsXPIDLCString commandLineArg;
          rv = handler->GetCommandLineArgument(getter_Copies(commandLineArg));
          if (NS_FAILED(rv)) continue;
          
          nsXPIDLCString helpText;
          rv = handler->GetHelpText(getter_Copies(helpText));
          if (NS_FAILED(rv)) continue;
          
          if ((const char *)commandLineArg) {
            printf("%s%s", HELP_SPACER_1,(const char *)commandLineArg);

            PRBool handlesArgs = PR_FALSE;
            rv = handler->GetHandlesArgs(&handlesArgs);
            if (NS_SUCCEEDED(rv) && handlesArgs) {
                printf(" <url>");
            }
            if ((const char *)helpText) {
              printf("%s%s\n",HELP_SPACER_2,(const char *)helpText);
            }
          }
        }
        
      }
    }
  }
  return;    
}

static nsresult HandleArbitraryStartup( nsICmdLineService* cmdLineArgs, nsIPref *prefs,  PRBool heedGeneralStartupPrefs)
{
	char* cmdResult = nsnull;
	nsresult rv;
	PRBool forceLaunchTask = PR_FALSE;
	PRInt32 height  = NS_SIZETOCONTENT;
	PRInt32 width  = NS_SIZETOCONTENT;
	char* tempString = NULL;

	// Get the value of -width option
	rv = cmdLineArgs->GetCmdLineValue("-width", &tempString);
	if (NS_FAILED(rv)) return rv;
	
	if (tempString) PR_sscanf(tempString, "%d", &width);
	  
	// Get the value of -height option
	rv = cmdLineArgs->GetCmdLineValue("-height", &tempString);
	if (NS_FAILED(rv)) return rv;
	
	if (tempString) PR_sscanf(tempString, "%d", &height);
  
  NS_WITH_SERVICE(nsICategoryManager, catman, "mozilla.categorymanager.1", &rv);
  if(NS_SUCCEEDED(rv) && catman) {
    
    nsCOMPtr<nsISimpleEnumerator> e;
    rv = catman->EnumerateCategory(COMMAND_LINE_ARGUMENT_HANDLERS, getter_AddRefs(e));
    if(NS_SUCCEEDED(rv) && e) {
      while (PR_TRUE) {
        nsCOMPtr<nsISupportsString> progid;
        rv = e->GetNext(getter_AddRefs(progid));
        if (NS_FAILED(rv) || !progid) break;

       	nsXPIDLCString progidString;
        progid->ToString(getter_Copies(progidString));
        
#ifdef DEBUG_CMD_LINE
        printf("cmd line hander progid = %s\n", (const char *)progidString);
#endif /* DEBUG_CMD_LINE */
        
        nsCOMPtr <nsICmdLineHandler> handler = do_GetService((const char *)progidString, &rv);
        if (NS_FAILED(rv)) continue;
        
        if (handler) {
          
          nsXPIDLCString commandLineArg;
          rv = handler->GetCommandLineArgument(getter_Copies(commandLineArg));
          if (NS_FAILED(rv)) continue;
          
          nsXPIDLCString chromeUrlForTask;
          rv = handler->GetChromeUrlForTask(getter_Copies(chromeUrlForTask));
          if (NS_FAILED(rv)) continue;
          
          nsXPIDLCString prefNameForStartup;
          rv = handler->GetPrefNameForStartup(getter_Copies(prefNameForStartup));
          if (NS_FAILED(rv)) continue;
          
#ifdef DEBUG_CMD_LINE
          printf("got this one:\t%s\n\t%s\n\t%s\n\n",(const char *)commandLineArg,(const char *)chromeUrlForTask,(const char *)prefNameForStartup);
#endif /* DEBUG_CMD_LINE */
          
          if (heedGeneralStartupPrefs) {
            rv = prefs->GetBoolPref((const char *)prefNameForStartup,&forceLaunchTask);
            if (NS_FAILED(rv)) {
              forceLaunchTask = PR_FALSE;
            }
          }  
          
          rv = cmdLineArgs->GetCmdLineValue((const char *)commandLineArg, &cmdResult);
#ifdef DEBUG_CMD_LINE
          printf("%s, cmdResult = %s\n",(const char *)commandLineArg,cmdResult);
#endif /* DEBUG_CMD_LINE */

          PRBool handlesArgs = PR_FALSE;
          rv = handler->GetHandlesArgs(&handlesArgs);
          if (handlesArgs) {
            if (forceLaunchTask || cmdResult) {
              if (cmdResult && PL_strcmp("1",cmdResult)) {
                PRBool openWindowWithArgs = PR_TRUE;
                rv = handler->GetOpenWindowWithArgs(&openWindowWithArgs);
                if (openWindowWithArgs) {
                   nsString cmdArgs(cmdResult);
#ifdef DEBUG_CMD_LINE
                    printf("opening %s with %s\n",(const char *)chromeUrlForTask,"OpenWindow");
#endif /* DEBUG_CMD_LINE */
                   OpenWindow((const char *)chromeUrlForTask, cmdArgs.GetUnicode());
                }
                else {
#ifdef DEBUG_CMD_LINE
                    printf("opening %s with %s\n",cmdResult,"OpenChromURL");
#endif /* DEBUG_CMD_LINE */
                    OpenChromURL(cmdResult,height, width);
                }
              }
              else {
                PRUnichar *defaultArgs;
                rv = handler->GetDefaultArgs(&defaultArgs);
                OpenWindow((const char *)chromeUrlForTask, defaultArgs);
                Recycle(defaultArgs);
              }
            }
          }
          else {
            if (forceLaunchTask) {
              OpenChromURL((const char *)chromeUrlForTask,height, width);
            }
            else if (NS_SUCCEEDED(rv) && cmdResult) {
              if (PL_strcmp("1",cmdResult) == 0) {
                OpenChromURL((const char *)chromeUrlForTask,height, width);
              }
              else {
                OpenChromURL(cmdResult, height, width);
              }
            }
          }
        }
      }
    }
  }
  
  return NS_OK;    
}

// This should be done by app shell enumeration someday
static nsresult DoCommandLines( nsICmdLineService* cmdLine, PRBool heedGeneralStartupPrefs )
{
	nsresult  rv;
	
	NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv); 
	if (NS_FAILED(rv)) return rv;

    rv = HandleArbitraryStartup( cmdLine, prefs, heedGeneralStartupPrefs);
	return rv;
}

static nsresult OpenBrowserWindow(PRInt32 height, PRInt32 width)
{
    printf("XXX: OpenBrowserWindow()\n");

    nsresult rv;
    NS_WITH_SERVICE(nsICmdLineHandler, handler, NS_IBROWSERCMDLINEHANDLER_PROGID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsXPIDLCString chromeUrlForTask;
    rv = handler->GetChromeUrlForTask(getter_Copies(chromeUrlForTask));
    if (NS_FAILED(rv)) return rv;

    PRUnichar *defaultArgs;
    rv = handler->GetDefaultArgs(&defaultArgs);
    if (NS_FAILED(rv)) return rv;
    rv = OpenWindow((const char *)chromeUrlForTask, defaultArgs);
    if (NS_FAILED(rv)) return rv;

    Recycle(defaultArgs);
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
				 
#if 0
      rv = OpenBrowserWindow(height, width);
#else
      rv = OpenChromURL("chrome://navigator/content/", height, width );
#endif
    }
	}
	return rv;
}

#ifdef DEBUG_warren
#ifdef XP_PC
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif
#endif

#if defined(FREEBSD)
// pick up fpsetmask prototype.
#include <floatingpoint.h>
#endif

// Note: splashScreen is an owning reference that this function has responsibility
//       to release.  This responsibility is delegated to the app shell service
//       (see nsAppShellService::Initialize call, below).
static nsresult main1(int argc, char* argv[], nsISplashScreen *splashScreen )
{
  nsresult rv;

#ifdef DEBUG_warren
//  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF);
#endif


#ifndef XP_MAC
  // Unbuffer debug output (necessary for automated QA performance scripts).
  setbuf( stdout, 0 );
#endif

#if defined(FREEBSD)
  // Disable all SIGFPE's on FreeBSD, as it has non-IEEE-conformant fp
  // trap behavior that trips up on floating-point tests performed by
  // the JS engine.  See bugzilla bug 9967 details.
  fpsetmask(0);
#endif

  //----------------------------------------------------------------
  // XPInstall needs to clean up after any updates that couldn't
  // be completed because components were in use. This must be done 
  // **BEFORE** any other libraries are loaded!
  //
  // Will also check to see if AutoReg is required due to version
  // change or installation of new components. If for some reason
  // XPInstall can't be loaded we assume Autoreg is required.
  //
  // (scoped in a block to force release of COMPtr)
  //----------------------------------------------------------------
  PRBool needAutoreg = PR_TRUE;
  {
    nsCOMPtr<nsISoftwareUpdate> su = do_GetService(kSoftUpdateCID,&rv);
    if (NS_SUCCEEDED(rv))
    {
      su->StartupTasks( &needAutoreg );
    }
  }
//  nsServiceManager::UnregisterService(kSoftUpdateCID);
   
 #if XP_MAC 
    stTSMCloser  tsmCloser;
  
  rv = InitializeMacCommandLine( argc, argv);
  NS_ASSERTION(NS_SUCCEEDED(rv), "Initializing AppleEvents failed");
 #endif

  // XXX: This call will be replaced by a registry initialization...
  NS_SetupRegistry_1( needAutoreg );

  // Start up the core services:

  // Initialize the cmd line service
  NS_WITH_SERVICE(nsICmdLineService, cmdLineArgs, kCmdLineServiceCID, &rv);
  NS_ASSERTION(NS_SUCCEEDED(rv), "failed to get command line service");
 
  if (NS_FAILED(rv)) {
    fprintf(stderr, "Could not obtain CmdLine processing service\n");
    return rv;
  }
 
  rv = cmdLineArgs->Initialize(argc, argv);
  NS_ASSERTION(NS_SUCCEEDED(rv), "failed to initialize command line args");
  if (rv  == NS_ERROR_INVALID_ARG) {
    PrintUsage();
    return rv;
  }

  // Create the Application Shell instance...
  NS_WITH_SERVICE(nsIAppShellService, appShell, kAppShellServiceCID, &rv);
  NS_ASSERTION(NS_SUCCEEDED(rv), "failed to get the appshell service");
  if (NS_FAILED(rv)) {
    splashScreen->Hide();
    return rv;
  }

  rv = appShell->Initialize( cmdLineArgs, splashScreen );
  // We are done with the splash screen here; the app shell owns it now.
  NS_IF_RELEASE( splashScreen );
  NS_ASSERTION(NS_SUCCEEDED(rv), "failed to initialize appshell");
  if ( NS_FAILED(rv) ) return rv; 

#ifdef DEBUG
  printf("initialized appshell\n");
#endif


  NS_WITH_SERVICE(nsIProfile, profileMgr, kProfileCID, &rv);
  NS_ASSERTION(NS_SUCCEEDED(rv), "failed to get profile manager");
  if ( NS_FAILED(rv) ) return rv; 

  rv = profileMgr->StartupWithArgs(cmdLineArgs);
  if (NS_FAILED(rv)) {
	return rv;
  }

  // if we get here, and we don't have a current profile, return a failure so we will exit
  // this can happen, if the user hits Cancel or Exit in the profile manager dialogs
  nsXPIDLCString currentProfileStr;
  rv = profileMgr->GetCurrentProfile(getter_Copies(currentProfileStr));
  if (NS_FAILED(rv) || !((const char *)currentProfileStr) || (PL_strlen((const char *)currentProfileStr) == 0)) {
  	return NS_ERROR_FAILURE;
  }

  // rjc: now must explicitly call appshell's CreateHiddenWindow() function AFTER profile manager.
  //      if the profile manager ever switches to using nsIDOMWindow stuff, this might have to change
  appShell->CreateHiddenWindow();

#ifndef XP_MAC
  PRBool expired;
  NS_WITH_SERVICE(nsITimeBomb, timeBomb, kTimeBombCID, &rv);
  if ( NS_FAILED(rv) ) return rv; 
    
  rv = timeBomb->Init();
  if ( NS_FAILED(rv) ) return rv; 

  rv = timeBomb->CheckWithUI(&expired);
  if ( NS_FAILED(rv) ) return rv; 
    
  if ( expired ) 
  {
      rv = timeBomb->LoadUpdateURL();
      if ( NS_FAILED(rv) ) return rv; 
  }
#endif

#ifdef NS_BUILD_REFCNT_LOGGING  
  nsTraceRefcnt::SetPrefServiceAvailability(PR_TRUE);
#endif

	// Enumerate AppShellComponenets
	appShell->EnumerateAndInitializeComponents();
	
	// This will go away once Components are handling there own commandlines
	// if we have no command line arguments, we need to heed the
	// "general.startup.*" prefs
	// if we had no command line arguments, argc == 1.
	rv = DoCommandLines( cmdLineArgs, (argc == 1) );
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to process command line");
	if ( NS_FAILED(rv) )
    return rv;
     
  // Make sure there exists at least 1 window. 
  rv = Ensure1Window( cmdLineArgs );
  NS_ASSERTION(NS_SUCCEEDED(rv), "failed to Ensure1Window");
  if (NS_FAILED(rv)) return rv;

  // Fire up the walletService
  NS_WITH_SERVICE(nsIWalletService, walletService, kWalletServiceCID, &rv);
  if ( NS_SUCCEEDED(rv) )
      walletService->WALLET_FetchFromNetCenter();

  // Start main event loop
  rv = appShell->Run();
  NS_ASSERTION(NS_SUCCEEDED(rv), "failed to run appshell");
  
  /*
   * Shut down the Shell instance...  This is done even if the Run(...)
   * method returned an error.
   */
  (void) appShell->Shutdown();

  return rv ;
}


// English text needs to go into a dtd file.
static
void DumpHelp(char *appname)
{
  printf("Usage: %s [ options ... ] [URL]\n", appname);
  printf("       where options include:\n");
  printf("\n");
  printf("%s-height <value>%sSet height of startup window to <value>.\n",HELP_SPACER_1,HELP_SPACER_2);
  printf("%s-h or -help%sPrint this message.\n",HELP_SPACER_1,HELP_SPACER_2);
  printf("%s-installer%sStart with 4.x migration window.\n",HELP_SPACER_1,HELP_SPACER_2);
  printf("%s-pref%sStart with pref window.\n",HELP_SPACER_1,HELP_SPACER_2);
  printf("%s-width <value>%sSet width of startup window to <value>.\n",HELP_SPACER_1,HELP_SPACER_2);
  printf("%s-v or -version%sPrint %s version.\n",HELP_SPACER_1,HELP_SPACER_2, appname);
  printf("%s-CreateProfile <profile>%sCreate and start with <profile>.\n",HELP_SPACER_1,HELP_SPACER_2);
  printf("%s-P <profile>%sStart with <profile>.\n",HELP_SPACER_1,HELP_SPACER_2);
  printf("%s-ProfileWizard%sStart with profile wizard.\n",HELP_SPACER_1,HELP_SPACER_2);
  printf("%s-ProfileManager%sStart with profile manager.\n",HELP_SPACER_1,HELP_SPACER_2);
  printf("%s-SelectProfile%sStart with profile selection dialog.\n",HELP_SPACER_1,HELP_SPACER_2);
#ifdef XP_UNIX
  printf("%s-splash%sEnable splash screen.\n",HELP_SPACER_1,HELP_SPACER_2);
#else
  printf("%s-nosplash%sDisable splash screen.\n",HELP_SPACER_1,HELP_SPACER_2);
#endif

  // not working yet, because we handle -h too early, and components
  // havent registered yet
  DumpArbitraryHelp();
}

static
void DumpVersion(char *appname)
{
	printf("%s: version info\n", appname);
}


int main(int argc, char* argv[])
{
#if defined(XP_UNIX)
  InstallUnixSignalHandlers(argv[0]);
#endif

  nsresult rv;

  /* -help and -version should return quick */
  if (argc == 2) {
	if ((PL_strcasecmp(argv[1], "-h") == 0) || (PL_strcasecmp(argv[1], "-help") == 0) 
#ifdef XP_UNIX
|| (PL_strcasecmp(argv[1], "--help") == 0)
#endif /* XP_UNIX */
#ifdef XP_PC
|| (PL_strcasecmp(argv[1], "/h") == 0) || (PL_strcasecmp(argv[1], "/help") == 0) || (PL_strcasecmp(argv[1], "/?") == 0)
#endif /* XP_PC */
    ) {
		DumpHelp(argv[0]);
		return 0;
	}
	else if ((PL_strcasecmp(argv[1], "-v") == 0) || (PL_strcasecmp(argv[1], "-version") == 0) 
#ifdef XP_UNIX
|| (PL_strcasecmp(argv[1], "--version") == 0)
#endif /* XP_UNIX */
#ifdef XP_PC
|| (PL_strcasecmp(argv[1], "/v") == 0) || (PL_strcasecmp(argv[1], "/version") == 0)
#endif /* XP_PC */
    ) {
		DumpVersion(argv[0]);
		return 0;
	}
  }
    
  // Call the code to install our handler
#ifdef MOZ_JPROF
  setupProfilingStuff();
#endif

  if( !NS_CanRun() )
    return 1; 
  // Note: this object is not released here.  It is passed to main1 which
  //       has responsibility to release it.
  nsISplashScreen *splash = 0;
  // We can't use the command line service here because it isn't running yet
#ifdef XP_UNIX
  PRBool dosplash = PR_FALSE;
  for (int i=0; i<argc; i++)
    if ((PL_strcasecmp(argv[i], "-splash") == 0)
        || (PL_strcasecmp(argv[i], "--splash") == 0))
      dosplash = PR_TRUE;
#else        
  PRBool dosplash = PR_TRUE;
  for (int i=0; i<argc; i++)
    if ((PL_strcasecmp(argv[i], "-nosplash") == 0)
        || (PL_strcasecmp(argv[i], "/nosplash") == 0))
      dosplash = PR_FALSE;
#endif
  if (dosplash) {
      rv = NS_CreateSplashScreen( &splash );
      NS_ASSERTION( NS_SUCCEEDED(rv), "NS_CreateSplashScreen failed" );
  }
  // If the platform has a splash screen, show it ASAP.
  if ( splash ) {
      splash->Show();
  }
  rv = NS_InitXPCOM(NULL, NULL);
  NS_ASSERTION( NS_SUCCEEDED(rv), "NS_InitXPCOM failed" );


  nsresult result = main1( argc, argv, splash );


  {
        // Scoping this in a block to force the pref service to be           
        // released. 
        //
	// save the prefs, in case they weren't saved
	NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv);
	NS_ASSERTION(NS_SUCCEEDED(rv), "failed to get prefs, so unable to save them");
	if (NS_SUCCEEDED(rv)) {
		prefs->SavePrefFile();
	}  
  }

#ifdef DETECT_WEBSHELL_LEAKS
  unsigned long count;
  if ( count = NS_TotalWebShellsInExistence() )  {
    printf("XXX WARNING: Number of webshells being leaked: %d \n", count);
  }
#endif

  // at this point, all that is on the clipboard is a proxy object, but that object
  // won't be valid once the app goes away. As a result, we need to force the data
  // out of that proxy and properly onto the clipboard. This can't be done in the
  // clipboard service's shutdown routine because it requires the parser/etc which
  // has already been shutdown by the time the clipboard is shut down.
  NS_WITH_SERVICE(nsIClipboard, clipService, "component://netscape/widget/clipboard", &rv);
  if ( clipService )
    clipService->ForceDataToClipboard();

  rv = NS_ShutdownXPCOM( NULL );
  NS_ASSERTION(NS_SUCCEEDED(rv), "NS_ShutdownXPCOM failed");
  return TranslateReturnValue( result );
}

#if defined( XP_PC ) && defined( WIN32 )
// We need WinMain in order to not be a console app.  This function is
// unused if we are a console application.
int WINAPI WinMain( HINSTANCE, HINSTANCE, LPSTR args, int ) {
    // Do the real work.
    return main( __argc, __argv );
}
#endif // XP_PC && WIN32
