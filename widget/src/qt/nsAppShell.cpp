/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsCOMPtr.h"
#include "nsAppShell.h"
#include "nsIAppShell.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsICmdLineService.h"
#include <stdlib.h>
#include "nsWidget.h"
#include <qwindowdefs.h>
#include "X11/Xlib.h"
#include "xlibrgb.h"
#include "nslog.h"

NS_IMPL_LOG(nsAppShellLog, 0)
#define PRINTF NS_LOG_PRINTF(nsAppShellLog)
#define FLUSH  NS_LOG_FLUSH(nsAppShellLog)

//-------------------------------------------------------------------------
//
// XPCOM CIDs
//
//-------------------------------------------------------------------------
static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kCmdLineServiceCID, NS_COMMANDLINE_SERVICE_CID);

PRBool nsAppShell::mRunning = PR_FALSE;

//-------------------------------------------------------------------------
//
// nsAppShell constructor
//
//-------------------------------------------------------------------------
nsAppShell::nsAppShell()
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsAppShell::nsAppShell()\n"));
    NS_INIT_REFCNT();
    mDispatchListener = 0;
    mEventQueue = nsnull;
    mApplication = nsnull;
}

//-------------------------------------------------------------------------
//
// nsAppShell destructor
//
//-------------------------------------------------------------------------
nsAppShell::~nsAppShell()
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsAppShell::~nsAppShell()\n"));
      mApplication->Release();
      mApplication = nsnull;
}

//-------------------------------------------------------------------------
//
// nsISupports implementation macro
//
//-------------------------------------------------------------------------
NS_IMPL_ISUPPORTS1(nsAppShell, nsIAppShell)

//-------------------------------------------------------------------------
NS_METHOD 
nsAppShell::SetDispatchListener(nsDispatchListener* aDispatchListener)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsAppShell::SetDispatchListener()\n"));
    mDispatchListener = aDispatchListener;
    return NS_OK;
}

//-------------------------------------------------------------------------
//
// Create the application shell
//
//-------------------------------------------------------------------------

NS_METHOD nsAppShell::Create(int *bac, char **bav)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsAppShell::Create()\n"));

    int        argc        = bac ? *bac : 0;
    char    ** argv        = bav;
    nsresult   rv          = NS_OK;
    Display  * aDisplay    = nsnull;
    Screen   * aScreen     = nsnull;

    // Open the display
    aDisplay = XOpenDisplay(NULL);
    
    if (aDisplay == NULL) {
      PRINTF("%s: Cannot connect to X server\n",argv[0]);
      exit(1);
    }

    aScreen = DefaultScreenOfDisplay(aDisplay);
    
    nsCOMPtr<nsICmdLineService> cmdLineArgs = do_GetService(kCmdLineServiceCID);
    if (cmdLineArgs) {
        rv = cmdLineArgs->GetArgc(&argc);
        if(NS_FAILED(rv)) {
            argc = bac ? *bac : 0;
        }

        rv = cmdLineArgs->GetArgv(&argv);
        if(NS_FAILED(rv)) {
            argv = bav;
        }
    }

    if (aDisplay && aScreen) {
        mApplication = nsQApplication::Instance(aDisplay);
    }
    else {
        mApplication = nsQApplication::Instance(argc, argv);
    }

    if (!mApplication) {
        return NS_ERROR_NOT_INITIALIZED;
    }

    mApplication->setStyle(new QWindowsStyle);

    return NS_OK;
}

//-------------------------------------------------------------------------
//
// Spinup - do any preparation necessary for running a message loop
//
//-------------------------------------------------------------------------
NS_METHOD nsAppShell::Spinup()
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsAppShell::Spinup()\n"));
    nsresult   rv = NS_OK;

    // Get the event queue service 
     nsCOMPtr<nsIEventQueueService> eventQService = do_GetService(kEventQueueServiceCID, &rv);
    if (NS_FAILED(rv)) {
        NS_ASSERTION("Could not obtain event queue service", PR_FALSE);
        return rv;
    }
    //Get the event queue for the thread.
    rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(mEventQueue));

    // If a queue already present use it.
    if (mEventQueue) {
        goto done;
    }
    // Create the event queue for the thread
    rv = eventQService->CreateThreadEventQueue();
    if (NS_OK != rv) {
        NS_ASSERTION("Could not create the thread event queue", PR_FALSE);
        return rv;
    }
    //Get the event queue for the thread
    rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(mEventQueue));
    if (NS_OK != rv) {
        NS_ASSERTION("Could not obtain the thread event queue", PR_FALSE);
        return rv;
    }    
done:
    mApplication->AddEventProcessorCallback(mEventQueue);
    return NS_OK;
}

//-------------------------------------------------------------------------
//
// Spindown - do any cleanup necessary for finishing a message loop
//
//-------------------------------------------------------------------------
NS_METHOD nsAppShell::Spindown()
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsAppShell::Spindown()\n"));
    if (mEventQueue) {
      mApplication->RemoveEventProcessorCallback(mEventQueue);
      mEventQueue = nsnull;
    }
    return NS_OK;
}


//-------------------------------------------------------------------------
//
// Run
//
//-------------------------------------------------------------------------
NS_METHOD nsAppShell::Run()
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsAppShell::Run()\n"));

    if (!mEventQueue)
      Spinup();

    if (!mEventQueue)
      return NS_ERROR_NOT_INITIALIZED;

    mRunning = PR_TRUE;
    mApplication->exec();
    mRunning = PR_FALSE;

    Spindown();
    return NS_OK;
}

//-------------------------------------------------------------------------
//
// Exit a message handler loop
//
//-------------------------------------------------------------------------

NS_METHOD nsAppShell::Exit()
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsAppShell::Exit()\n"));
    if (mRunning)
      mApplication->exit(0);

    return NS_OK;
}

NS_METHOD nsAppShell::GetNativeEvent(PRBool &aRealEvent, void *& aEvent)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsAppShell::GetNativeEvent()\n"));
    aRealEvent = PR_FALSE;
    aEvent = 0;
    return NS_OK;
}

NS_METHOD nsAppShell::DispatchNativeEvent(PRBool aRealEvent, void *aEvent)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsAppShell::DispatchNativeEvent()\n"));
    if (!mRunning)
      return NS_ERROR_NOT_INITIALIZED;

    mApplication->processEvents();

    return NS_OK;
}

NS_IMETHODIMP nsAppShell::ListenToEventQueue(nsIEventQueue *aQueue,
                                             PRBool aListen)
{
  if (mApplication == nsnull)
    return NS_ERROR_NOT_INITIALIZED;

  if (aListen) {
    mApplication->AddEventProcessorCallback(aQueue);
  }
  else {
    mApplication->RemoveEventProcessorCallback(aQueue);
  }
  return NS_OK;
} 
