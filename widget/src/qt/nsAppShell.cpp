/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsAppShell.h"
#include "nsIAppShell.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsICmdLineService.h"
#include <stdlib.h>
#include "nsWidget.h"

//-------------------------------------------------------------------------
//
// XPCOM CIDs
//
//-------------------------------------------------------------------------
static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kCmdLineServiceCID, NS_COMMANDLINE_SERVICE_CID);

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

    int argc = bac ? *bac : 0;
    char **argv = bav;
#if 1
    nsresult rv;

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
#endif

    mApplication = new nsQApplication(argc, argv);

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
// PushThreadEventQueue - begin processing events from a new queue
//   note this is the Windows implementation and may suffice, but
//   this is untested with Qt.
//
//-------------------------------------------------------------------------
NS_METHOD nsAppShell::PushThreadEventQueue()
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsAppShell::PushThreadEventQueue()\n"));
    nsresult rv;

    // push a nested event queue for event processing from netlib
    // onto our UI thread queue stack.
    NS_WITH_SERVICE(nsIEventQueueService, eQueueService, kEventQueueServiceCID, &rv);
    if (NS_SUCCEEDED(rv))
    {
        rv = eQueueService->PushThreadEventQueue();
    }
    else
    {
        NS_ERROR("Appshell unable to obtain eventqueue service.");
    }

    return rv;
}

//-------------------------------------------------------------------------
//
// PopThreadEventQueue - stop processing on a previously pushed event queue
//   note this is the Windows implementation and may suffice, but
//   this is untested with Qt.
//
//-------------------------------------------------------------------------
NS_METHOD nsAppShell::PopThreadEventQueue()
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsAppShell::PopThreadEventQueue()\n"));
    nsresult rv;

    NS_WITH_SERVICE(nsIEventQueueService, eQueueService, kEventQueueServiceCID, &rv);
    if (NS_SUCCEEDED(rv))
    {
        rv = eQueueService->PopThreadEventQueue();
    }
    else
    {
        NS_ERROR("Appshell unable to obtain eventqueue service.");
    }

    return rv;
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

NS_METHOD nsAppShell::EventIsForModalWindow(PRBool aRealEvent, 
                                            void *aEvent,
                                            nsIWidget *aWidget, 
                                            PRBool *aForWindow)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsAppShell::EventIsForModalWindow()\n"));
    *aForWindow = PR_TRUE;
    return NS_OK;
}


