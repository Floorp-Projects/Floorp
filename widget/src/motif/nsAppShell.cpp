/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/*
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

#include "nsIMotifAppContextService.h"

#include <stdlib.h>

#ifdef LINUX
#define DO_THE_EDITRES_THING
#endif

#ifdef DO_THE_EDITRES_THING
#include <X11/Xmu/Editres.h>
#endif

#include "xlibrgb.h"

#include "nsIPref.h"

//-------------------------------------------------------------------------
//
// XPCOM CIDs
//
//-------------------------------------------------------------------------
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kCmdLineServiceCID, NS_COMMANDLINE_SERVICE_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

//-------------------------------------------------------------------------
//
// nsISupports implementation macro
//
//-------------------------------------------------------------------------
NS_DEFINE_IID(kIAppShellIID, NS_IAPPSHELL_IID);
NS_IMPL_ISUPPORTS(nsAppShell,kIAppShellIID);

//-------------------------------------------------------------------------
NS_METHOD nsAppShell::SetDispatchListener(nsDispatchListener* aDispatchListener)
{
  mDispatchListener = aDispatchListener;
  return NS_OK;
}

XtAppContext nsAppShell::sAppContext = nsnull;

//-------------------------------------------------------------------------
//
// Create the application shell
//
//-------------------------------------------------------------------------

NS_METHOD nsAppShell::Create(int* bac, char ** bav)
{
  int argc = bac ? *bac : 0;
  char **argv = bav;

  nsresult rv;

  NS_WITH_SERVICE(nsICmdLineService, cmdLineArgs, kCmdLineServiceCID, &rv);
  if (NS_SUCCEEDED(rv))
  {
    rv = cmdLineArgs->GetArgc(&argc);
    if(NS_FAILED(rv))
      argc = bac ? *bac : 0;

    rv = cmdLineArgs->GetArgv(&argv);
    if(NS_FAILED(rv))
      argv = bav;
  }

  XtSetLanguageProc(NULL, NULL, NULL);
							
  mTopLevel = XtAppInitialize(&sAppContext,   // app_context_return
                              "nsAppShell",   // application_class
                              NULL,           // options
                              0,              // num_options
                              &argc,          // argc_in_out
                              argv,           // argv_in_out
                              NULL,           // fallback_resources
                              NULL,           // args
                              0);             // num_args

  printf("nsAppShell::Create() app_context = %p\n",sAppContext);

  xlib_rgb_init(XtDisplay(mTopLevel), XtScreen(mTopLevel));

  printf("xlib_rgb_init(display=%p,screen=%p)\n",
         XtDisplay(mTopLevel),
         XtScreen(mTopLevel));

  SetAppContext(sAppContext);

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Enter a message handler loop
//
//-------------------------------------------------------------------------

static void event_processor_callback(XtPointer       aClosure,
                                     int *           aFd,
                                     XtIntervalId *  aId)
{
  nsIEventQueue *eventQueue = (nsIEventQueue*)aClosure;
  eventQueue->ProcessPendingEvents();
}

NS_METHOD nsAppShell::Run()
{
  NS_ADDREF_THIS();
  nsresult   rv = NS_OK;
  nsIEventQueue * EQueue = nsnull;

  // Get the event queue service 
  NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueServiceCID, &rv);
  if (NS_FAILED(rv)) {
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
    goto done;

  // Create the event queue for the thread
  rv = eventQService->CreateThreadEventQueue();
  if (NS_OK != rv) {
    NS_ASSERTION("Could not create the thread event queue", PR_FALSE);
    return rv;
  }
  //Get the event queue for the thread
  rv = eventQService->GetThreadEventQueue(PR_GetCurrentThread(), &EQueue);
  if (NS_OK != rv) {
    NS_ASSERTION("Could not obtain the thread event queue", PR_FALSE);
    return rv;
  }    


done:

  printf("Calling XtAppAddInput() with event queue\n");

  XtAppAddInput(nsAppShell::GetAppContext(),
                EQueue->GetEventQueueSelectFD(),
                (XtPointer) XtInputReadMask, 
                event_processor_callback, 
                EQueue);

  XtRealizeWidget(mTopLevel);

#ifdef DO_THE_EDITRES_THING
	XtAddEventHandler(mTopLevel,
                    (EventMask) 0,
                    True,
                    (XtEventHandler) _XEditResCheckMessages,
                    (XtPointer)NULL);
#endif

  XEvent event;

  for (;;) 
  {
    XtAppNextEvent(sAppContext, &event);

    XtDispatchEvent(&event);

    if (mDispatchListener)
      mDispatchListener->AfterDispatch();
  } 

  NS_IF_RELEASE(EQueue);
  Release();
  return NS_OK;
}

NS_METHOD nsAppShell::Spinup()
{
  return NS_OK;
}

NS_METHOD nsAppShell::Spindown()
{
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// PushThreadEventQueue - begin processing events from a new queue
//   note this is the Windows implementation and may suffice, but
//   this is untested on motif.
//
//-------------------------------------------------------------------------
NS_METHOD nsAppShell::PushThreadEventQueue()
{
  nsresult rv;

  // push a nested event queue for event processing from netlib
  // onto our UI thread queue stack.
  NS_WITH_SERVICE(nsIEventQueueService, eQueueService, kEventQueueServiceCID, &rv);
  if (NS_SUCCEEDED(rv))
    rv = eQueueService->PushThreadEventQueue();
  else
    NS_ERROR("Appshell unable to obtain eventqueue service.");
  return rv;
}

//-------------------------------------------------------------------------
//
// PopThreadEventQueue - stop processing on a previously pushed event queue
//   note this is the Windows implementation and may suffice, but
//   this is untested on motif.
//
//-------------------------------------------------------------------------
NS_METHOD nsAppShell::PopThreadEventQueue()
{
  nsresult rv;

  NS_WITH_SERVICE(nsIEventQueueService, eQueueService, kEventQueueServiceCID, &rv);
  if (NS_SUCCEEDED(rv))
    rv = eQueueService->PopThreadEventQueue();
  else
    NS_ERROR("Appshell unable to obtain eventqueue service.");
  return rv;
}

NS_METHOD nsAppShell::GetNativeEvent(PRBool &aRealEvent, void *&aEvent)
{
  //XXX:Implement this.
  return NS_OK;
}

NS_METHOD nsAppShell::DispatchNativeEvent(PRBool aRealEvent, void * aEvent)
{
  //XXX:Implement this.
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Exit a message handler loop
//
//-------------------------------------------------------------------------

NS_METHOD nsAppShell::Exit()
{
  exit(0);
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// nsAppShell constructor
//
//-------------------------------------------------------------------------
nsAppShell::nsAppShell()
{ 
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
}

//-------------------------------------------------------------------------
//
// GetNativeData
//
//-------------------------------------------------------------------------
void* nsAppShell::GetNativeData(PRUint32 aDataType)
{
  if (aDataType == NS_NATIVE_SHELL) {
    return mTopLevel;
  }
  return nsnull;
}


NS_METHOD nsAppShell::EventIsForModalWindow(PRBool aRealEvent, void *aEvent, nsIWidget *aWidget,
                                            PRBool *aForWindow)
{
  //XXX:Implement this.
  return NS_OK;
}

static NS_DEFINE_CID(kCMotifAppContextServiceCID, NS_MOTIF_APP_CONTEXT_SERVICE_CID);

//-------------------------------------------------------------------------
//
// SetAppContext
//
//-------------------------------------------------------------------------
/* static */ void 
nsAppShell::SetAppContext(XtAppContext aAppContext)
{
  NS_ASSERTION(aAppContext != nsnull,"App context cant be null.");

  static PRBool once = PR_TRUE;

  if (once)
  {
    once = PR_FALSE;

    nsresult   rv;
    nsIMotifAppContextService * ac_service = nsnull;
    
    rv = nsComponentManager::CreateInstance(kCMotifAppContextServiceCID,
                                            nsnull,
                                            nsIMotifAppContextService::GetIID(),
                                            (void **)& ac_service);
    
    NS_ASSERTION(rv == NS_OK,"Cannot obtain app context service.");

    if (ac_service)
    {
      printf("nsAppShell::SetAppContext() ac_service = %p\n",ac_service);

      nsresult rv2 = ac_service->SetAppContext(aAppContext);

      NS_ASSERTION(rv2 == NS_OK,"Cannot set the app context.");

      printf("nsAppShell::SetAppContext() All is ok.\n");

      NS_RELEASE(ac_service);
    }
  }
}
