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
 */

#include "nsAppShell.h"
#include "nsIAppShell.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsIUnixToolkitService.h"
#include "nsICmdLineService.h"
#include <stdlib.h>
#include "nsWidget.h"
#include <qwindowdefs.h>
#include "xlibrgb.h"

//-------------------------------------------------------------------------
//
// XPCOM CIDs
//
//-------------------------------------------------------------------------
static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kCmdLineServiceCID, NS_COMMANDLINE_SERVICE_CID);
static NS_DEFINE_CID(kCUnixToolkitServiceCID, NS_UNIX_TOOLKIT_SERVICE_CID);

nsAppShell::GfxToolkit nsAppShell::mGfxToolkit = nsAppShell::eInvalidGfxToolkit;

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
    delete mApplication;
}

//-------------------------------------------------------------------------
//
// nsISupports implementation macro
//
//-------------------------------------------------------------------------
NS_IMPL_ISUPPORTS(nsAppShell, nsIAppShell::GetIID());

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

#if 0
static void event_processor_callback(gpointer data,
                                     gint source,
                                     GdkInputCondition condition)
{
    nsIEventQueue *eventQueue = (nsIEventQueue*)data;
    eventQueue->ProcessPendingEvents();
}
#endif

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
    GfxToolkit aGfxToolkit = GetGfxToolkit();

    if (aGfxToolkit == eQtGfxToolkit)
    {
    }
    else if (aGfxToolkit == eXlibGfxToolkit)
    {
        // Open the display
        aDisplay = XOpenDisplay(NULL);
    
        if (aDisplay == NULL) 
        {
            fprintf(stderr, "%s: Cannot connect to X server\n",
                    argv[0]);
        
            exit(1);
        }

        aScreen = DefaultScreenOfDisplay(aDisplay);
    
        xlib_rgb_init(aDisplay, aScreen);
    }
    else
    {
        fprintf(stderr, "Invalid toolkit\n");
        exit(1);
    }

    NS_WITH_SERVICE(nsICmdLineService, cmdLineArgs, kCmdLineServiceCID, &rv);
    if (NS_SUCCEEDED(rv))
    {
        rv = cmdLineArgs->GetArgc(&argc);
        if(NS_FAILED(rv))
        {
            argc = bac ? *bac : 0;
        }

        rv = cmdLineArgs->GetArgv(&argv);
        if(NS_FAILED(rv))
        {
            argv = bav;
        }
    }

    if (aDisplay && aScreen)
    {
        mApplication = new nsQApplication(aDisplay);
    }
    else
    {
        mApplication = new nsQApplication(argc, argv);
    }

    if (!mApplication) 
    {
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
    NS_ADDREF_THIS();
    nsresult   rv = NS_OK;
    nsIEventQueue * EQueue = nsnull;

    // Get the event queue service 
    NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueServiceCID, &rv);
    if (NS_FAILED(rv)) 
    {
        NS_ASSERTION("Could not obtain event queue service", PR_FALSE);
        return rv;
    }

#ifdef DEBUG
    printf("Got the event queue from the service\n");
#endif /* DEBUG */

    //Get the event queue for the thread.
    rv = eventQService->GetThreadEventQueue(PR_GetCurrentThread(), &EQueue);

    // If a queue already present use it.
    if (EQueue)
    {
        goto done;
    }

    // Create the event queue for the thread
    rv = eventQService->CreateThreadEventQueue();
    if (NS_OK != rv) 
    {
        NS_ASSERTION("Could not create the thread event queue", PR_FALSE);
        return rv;
    }
    //Get the event queue for the thread
    rv = eventQService->GetThreadEventQueue(PR_GetCurrentThread(), &EQueue);
    if (NS_OK != rv) 
    {
        NS_ASSERTION("Could not obtain the thread event queue", PR_FALSE);
        return rv;
    }    


done:

    mApplication->SetEventProcessorCallback(EQueue);
    mApplication->exec();

    NS_IF_RELEASE(EQueue);
    Release();
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
    mApplication->exit(0);

    return NS_OK;
}

//-------------------------------------------------------------------------
//
// GetNativeData
//
//-------------------------------------------------------------------------
void* nsAppShell::GetNativeData(PRUint32 aDataType)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsAppShell::GetNativeData()\n"));
    if (aDataType == NS_NATIVE_SHELL) 
    {
//    return mTopLevel;
    }
    return nsnull;
}


NS_METHOD nsAppShell::GetNativeEvent(PRBool &aRealEvent, void *& aEvent)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsAppShell::GetNativeEvent()\n"));
    aRealEvent = PR_FALSE;
    aEvent = 0;
    return NS_ERROR_FAILURE;
}

NS_METHOD nsAppShell::DispatchNativeEvent(PRBool aRealEvent, void *aEvent)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsAppShell::DispatchNativeEvent()\n"));
    return NS_ERROR_FAILURE;
}

nsAppShell::GfxToolkit nsAppShell::GetGfxToolkit()
{
    nsString aGfxToolkit;
    nsresult rv;

    if (mGfxToolkit == eInvalidGfxToolkit)
    {
        nsIUnixToolkitService * unixToolkitService = nsnull;
    
        rv = nsComponentManager::CreateInstance(kCUnixToolkitServiceCID,
                                                nsnull,
                                                nsIUnixToolkitService::GetIID(),
                                                (void **) &unixToolkitService);
  
        NS_ASSERTION(NS_SUCCEEDED(rv),"Cannot obtain unix toolkit service.");

        if (NS_SUCCEEDED(rv))
        {
            rv = unixToolkitService->GetGfxToolkitName(aGfxToolkit);
        }

        NS_IF_RELEASE(unixToolkitService);
    }

    if (NS_SUCCEEDED(rv))
    {
        if (aGfxToolkit == "qt")
        {
            mGfxToolkit = eQtGfxToolkit;
        }
        else if (aGfxToolkit == "xlib")
        {
            mGfxToolkit = eXlibGfxToolkit;
        }
    }

    return mGfxToolkit;
}

