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
#include "nsIURL.h"
#include "nsIWidget.h"
#include "plevent.h"

#include "nsIAppShell.h"
#include "nsIAppShellService.h"

/* Define Class IDs */
static NS_DEFINE_IID(kAppShellServiceCID, NS_APPSHELL_SERVICE_CID);

/* Define Interface IDs */
static NS_DEFINE_IID(kIAppShellServiceIID, NS_IAPPSHELL_SERVICE_IID);


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



int main(int argc, char* argv[])
{
  nsresult rv;
#if 0
  nsICmdLineArguements* cmdLineArgs = nsnull;
#endif
  nsIAppShellService* appShell = nsnull;

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
#ifdef XP_PC
  PL_InitializeEventsLib("");
#endif


  /*
   * Start up the core services:
   *  - Command-line processor.
   */
#if 0
  rv = nsServiceManager::GetService(kCmdLineProcessorCID,
                                    kICmdLineArgumentsIID,
                                    (nsISupports **)&cmdLineArgs);
  if (!NS_SUCCEEDED(rv)) {
    goto done;
  }

  rv = cmdLineArgs->Initialize(argc, argv);
  if (PR_FALSE == NS_SUCCEEDED(rv)) {
    goto done;
  }

#endif

  /*
   * Create the Application Shell instance...
   */
  rv = nsServiceManager::GetService(kAppShellServiceCID,
                                    kIAppShellServiceIID,
                                    (nsISupports**)&appShell);
  if (!NS_SUCCEEDED(rv)) {
    goto done;
  }

  /*
   * Initialize the Shell...
   */
  rv = appShell->Initialize();
  if (!NS_SUCCEEDED(rv)) {
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
  nsIURL* url;
  nsIWidget* newWindow;

  rv = NS_NewURL(&url, "resource:/res/samples/test0.html");
  if (NS_FAILED(rv)) {
    goto done;
  }
  appShell->CreateTopLevelWindow(url, newWindow);
  NS_RELEASE(url);

  /*
   * Start up the main event loop...
   */
  rv = appShell->Run();

  /*
   * Shutdown the Shell instance...  This is done even if the Run(...)
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

  /* 
   * Translate the nsresult into an appropriate platform-specific return code.
   */
  return TranslateReturnValue(rv);
}
