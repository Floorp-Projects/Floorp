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

#include "prmon.h"
#include "nsAppShell.h"
#include "nsIAppShell.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsICmdLineService.h"
#include <stdlib.h>

#ifdef MOZ_GLE
#include <gle/gle.h>
#endif

#include "nsIWidget.h"
#include "nsIPref.h"
#include "nsIWidget.h"

#include <Pt.h>
#include <errno.h>


//NS_DEFINE_IID(kIAppShellIID, NS_IAPPSHELL_IID);
//NS_IMPL_ISUPPORTS(nsAppShell,kIAppShellIID);

PRBool            nsAppShell::mPtInited = PR_FALSE;
nsIEventQueue     *kedlEQueue = nsnull;

//static int modal_count;

typedef int gint;

#include <prlog.h>
PRLogModuleInfo  *PhWidLog =  PR_NewLogModule("PhWidLog");
#include "nsPhWidgetLog.h"

//-------------------------------------------------------------------------
//
// XPCOM CIDs
//
//-------------------------------------------------------------------------
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kCmdLineServiceCID, NS_COMMANDLINE_SERVICE_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

// a linked, ordered list of event queues and their tokens
class EventQueueToken {
public:
  EventQueueToken(const nsIEventQueue *aQueue, const gint aToken);

  const nsIEventQueue *mQueue;
  gint mToken;
  EventQueueToken *next;
};

EventQueueToken::EventQueueToken(const nsIEventQueue *aQueue, const gint aToken)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("EventQueueToken::EventQueueToken Constructor aQueue=<%p> aToken=<%d>\n", aQueue, aToken));

  mQueue = aQueue;
  mToken = aToken;
  next = 0;
}

class EventQueueTokenQueue {
public:
  EventQueueTokenQueue();
  virtual ~EventQueueTokenQueue();
  void PushToken(nsIEventQueue *aQueue, gint aToken);
  PRBool PopToken(nsIEventQueue *aQueue, gint *aToken);

private:
  EventQueueToken *mHead;
};

EventQueueTokenQueue::EventQueueTokenQueue()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("EventQueueTokenQueue::EventQueueTokenQueue Constructor\n"));
  mHead = 0;
}

EventQueueTokenQueue::~EventQueueTokenQueue()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("EventQueueTokenQueue::~EventQueueTokenQueue Destructor\n"));

  NS_ASSERTION(!mHead, "event queue token deleted when not empty");
  // and leak. it's an error, anyway
}

void EventQueueTokenQueue::PushToken(nsIEventQueue *aQueue, gint aToken)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("EventQueueTokenQueue::PushToken\n"));

  EventQueueToken *newToken = new EventQueueToken(aQueue, aToken);
  NS_ASSERTION(newToken, "couldn't allocate token queue element");
  if (newToken) {
    newToken->next = mHead;
    mHead = newToken;
  }
}

PRBool EventQueueTokenQueue::PopToken(nsIEventQueue *aQueue, gint *aToken)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("EventQueueTokenQueue::PopToken\n"));

  EventQueueToken *token, *lastToken;
  PRBool          found = PR_FALSE;

  NS_ASSERTION(mHead, "attempt to retrieve event queue token from empty queue");
  if (mHead)
    NS_ASSERTION(mHead->mQueue == aQueue, "retrieving event queue from past head of queue queue");

  token = mHead;
  lastToken = 0;
  while (token && token->mQueue != aQueue) {
    lastToken = token;
    token = token->next;
  }
  if (token) {
    if (lastToken)
      lastToken->next = token->next;
    else
      mHead = token->next;
    found = PR_TRUE;
    *aToken = token->mToken;
    delete token;
  }
  return found;
}

//-------------------------------------------------------------------------
//
// nsAppShell constructor
//
//-------------------------------------------------------------------------
nsAppShell::nsAppShell()  
{ 
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::nsAppShell Constructor\n"));

  NS_INIT_REFCNT();
  mDispatchListener = nsnull;
  mLock = PR_NewLock();
  mEventQueueTokens = new EventQueueTokenQueue();
  mEventBufferSz = sizeof( PhEvent_t ) + 1000;
  mEvent = (PhEvent_t*) PR_Malloc( mEventBufferSz );

  // throw on error would really be civilized here
  NS_ASSERTION(mLock, "couldn't obtain lock in appshell");
  NS_ASSERTION(mEventQueueTokens, "couldn't allocate event queue token queue");
  NS_ASSERTION( mEvent, "Out of memory" );

  /* Run this only once per application startup */
  if( !mPtInited )
  {
    PtInit( NULL );
    PtChannelCreate(); // Force use of pulses
    mPtInited = PR_TRUE;
  }
}

//-------------------------------------------------------------------------
//
// nsAppShell destructor
//
//-------------------------------------------------------------------------
nsAppShell::~nsAppShell()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::~nsAppShell\n"));

  PR_DestroyLock(mLock);
  delete mEventQueueTokens;
  
  if (mEvent)
    PR_Free(mEvent);
}


//-------------------------------------------------------------------------
//
// nsISupports implementation macro
//
//-------------------------------------------------------------------------
NS_IMPL_ISUPPORTS1(nsAppShell, nsIAppShell)


//-------------------------------------------------------------------------
NS_IMETHODIMP nsAppShell::SetDispatchListener(nsDispatchListener* aDispatchListener)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::SetDispatchListener.\n"));

  mDispatchListener = aDispatchListener;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Enter a message handler loop
//
//-------------------------------------------------------------------------
int event_processor_callback(int fd, void *data, unsigned mode)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::event_processor_callback fd=<%d> data=<%p> mode=<%d>\n", fd, data, mode));

  nsIEventQueue * eventQueue = (nsIEventQueue *) data;
  eventQueue->ProcessPendingEvents();
  return Pt_CONTINUE;
}


//-------------------------------------------------------------------------
//
// Create the application shell
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsAppShell::Create(int *bac, char **bav)
{
  char *home=nsnull;
  char *path=nsnull;

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::Create\n"));

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

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Spinup - do any preparation necessary for running a message loop
//
//-------------------------------------------------------------------------
NS_METHOD nsAppShell::Spinup()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::Spinup - Not Implemented.\n"));
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Spindown - do any cleanup necessary for finishing a message loop
//
//-------------------------------------------------------------------------
NS_METHOD nsAppShell::Spindown()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::Spindown - Not Implemented.\n"));
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// PushThreadEventQueue
//
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsAppShell::PushThreadEventQueue()
{
PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::PushThreadEventQueue\n"));

  nsresult      rv;
  gint          inputToken;
  nsIEventQueue *eQueue;

  // push a nested event queue for event processing from netlib
  // onto our UI thread queue stack.
  NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueServiceCID, &rv);
  if (NS_SUCCEEDED(rv)) {
    PR_Lock(mLock);
    rv = eventQService->PushThreadEventQueue();
    if (NS_SUCCEEDED(rv)) {
      eventQService->GetThreadEventQueue(PR_GetCurrentThread(), &eQueue);

    int err;
	err = PtAppAddFd(NULL,eQueue->GetEventQueueSelectFD(),Pt_FD_READ,event_processor_callback,eQueue);
    if (err == -1)
	{
		printf("nsAppShell::PushThreadEventQueue Error calling PtAppAddFd\n");
		exit(1);
	}

      mEventQueueTokens->PushToken(eQueue, inputToken);
    }
    PR_Unlock(mLock);
  } else
    NS_ERROR("Appshell unable to obtain eventqueue service.");
  return rv;
}

//-------------------------------------------------------------------------
//
// PopThreadEventQueue
//
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsAppShell::PopThreadEventQueue()
{
PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::PopThreadEventQueue\n"));

  nsresult      rv;
  nsIEventQueue *eQueue;

  NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueServiceCID, &rv);
  if (NS_SUCCEEDED(rv)) {
    gint queueToken;
    PR_Lock(mLock);
    eventQService->GetThreadEventQueue(PR_GetCurrentThread(), &eQueue);
    eventQService->PopThreadEventQueue();
    if (mEventQueueTokens->PopToken(eQueue, &queueToken))
	{
        int err;
        err=PtAppRemoveFd(NULL,eQueue->GetEventQueueSelectFD());
		if ( err == -1)
		{
		  printf("nsAppShell::PopThreadEventQueue Error calling  PtAppRemoveFd\n");
		  exit(1);
		}
	}
    PR_Unlock(mLock);
    NS_IF_RELEASE(eQueue);
  } else
    NS_ERROR("Appshell unable to obtain eventqueue service.");
  return rv;
}


//-------------------------------------------------------------------------
//
// Run
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsAppShell::Run()
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

#ifdef DEBUG
  printf("Calling PtAppAddFd with event queue\n");
#endif /* DEBUG */

    int err;
	err = PtAppAddFd(NULL,EQueue->GetEventQueueSelectFD(),Pt_FD_READ,event_processor_callback,EQueue);
    if (err == -1)
	{
		printf("nsAppShell::Run Error calling PtAppAddFd\n");
		exit(1);
	}
    PtMainLoop();

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
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::Exit - Not Implemented.\n"));

  // REVISIT - How do we do this under Photon??? 
  // PtSendEventToWidget( m_window, quit_event );

  /* kirk: Hack until we figure out what to do here */
  /* No way to exit the Graphical portion without a raw EXIT */
  exit ( EXIT_SUCCESS );

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// GetNativeData
//
//-------------------------------------------------------------------------
void* nsAppShell::GetNativeData(PRUint32 aDataType)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::GetNativeShell - Not Implemented.\n"));

  if( aDataType == NS_NATIVE_SHELL )
  {
    return nsnull;
  }
  return nsnull;
}


NS_METHOD nsAppShell::GetNativeEvent(PRBool &aRealEvent, void *&aEvent)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::GetNativeEvent\n"));

  nsresult res = NS_ERROR_FAILURE;
      
  aEvent = NULL;
  aRealEvent = PR_FALSE;

#if 0
  const int EVENT_SIZE = sizeof(struct PhEvent_t) + 1000;
  PhEvent_t *event = NULL;
  event = PR_Malloc(EVENT_SIZE);
  if (NULL != event)
  {
    int status;
	status = PhEventPeek(event, EVENT_SIZE);
	if (status == Ph_EVENT_MSG)
	{
  	  aRealEvent = PR_TRUE;
      aEvent = event;
	  res = NS_OK;
	}
    else
    {
	  PtProcessEvent();
      res = NS_OK;
    }
  }
  else
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::GetNativeEvent out of memory\n"));
    NS_ASSERTION(event, "couldn't allocate a Photon event");

  }
#else
  PtProcessEvent();
  res = NS_OK;
#endif

  return res;
}


NS_METHOD nsAppShell::DispatchNativeEvent(PRBool aRealEvent, void * aEvent)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::DispatchNativeEvent aRealEvent=<%d> aEvent=<%p>\n", aRealEvent, aEvent));

  int err;
  
  if ( aRealEvent == PR_TRUE )
  {
    err = PtEventHandler( (PhEvent_t*) aEvent );
    if (err == -1)
     {
        PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::DispatchNativeEvent aEvent=<%p> error=<%d>\n", aEvent, err));
        return NS_ERROR_FAILURE;
     }
  }

  return NS_OK;
}

NS_METHOD nsAppShell::EventIsForModalWindow(PRBool aRealEvent, void *aEvent, nsIWidget *aWidget, PRBool *aForWindow)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::EventIsForModalWindow aEvent=<%p> - Not Implemented.\n", aEvent));

#if 1
    *aForWindow = PR_FALSE;
    return NS_OK;
#else
  PRBool isInWindow, isMouseEvent;
  PhEvent_t *msg = (PhEvent_t *) aEvent;

  if (aRealEvent == PR_FALSE)
  {
    *aForWindow = PR_FALSE;
    return NS_OK;
  }

  isInWindow = PR_FALSE;
  if (aWidget != nsnull)
  {
    // Get Native Window for dialog window
    PtWidget_t *win;
    win = (PtWidget_t *)aWidget->GetNativeData(NS_NATIVE_WINDOW);
    PtWidget_t *eWin = (PtWidget_t *) msg->collector.handle;
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::EventIsForModalWindow win=<%p> eWin=<%p>\n",win, eWin));

    if (nsnull != eWin) {
      if (win == eWin) {
        isInWindow = PR_TRUE;
      }
    }
  }
  
  switch(msg->type)
  {
    case Ph_EV_BUT_PRESS:
    case Ph_EV_BUT_RELEASE:
    case Ph_EV_BUT_REPEAT:
    case Ph_EV_PTR_MOTION_BUTTON:
    case Ph_EV_PTR_MOTION_NOBUTTON:
       isMouseEvent = PR_TRUE;
       break;
    default:
       isMouseEvent = PR_FALSE;
       break;
  }

  *aForWindow = isInWindow == PR_TRUE || isMouseEvent == PR_FALSE ? PR_TRUE : PR_FALSE;

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::EventIsForModalWindow isInWindow=<%d> isMouseEvent=<%d> aForWindow=<%d>\n", isInWindow, isMouseEvent, *aForWindow));
  return NS_OK;
#endif
}
