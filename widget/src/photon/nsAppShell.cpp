/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/*
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

#include "prmon.h"
#include "nsCOMPtr.h"
#include "nsAppShell.h"
#include "nsIAppShell.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsICmdLineService.h"
#include <stdlib.h>

#include "nsIWidget.h"
#include "nsIPref.h"

#include <Pt.h>
#include <errno.h>

PRBool            nsAppShell::mPtInited = PR_FALSE;
int               nsAppShell::mModalCount = -1;

nsIEventQueue     *kedlEQueue = nsnull;

/* Global Definitions */
int ExitMainLoop = 0;
typedef int gint;			/* Need to define this for EventQueueToken */ 

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

  // if we reach this point with an empty token queue, well, fab. however,
  // we expect the first event queue to still be active. so we take
  // special care to unhook that queue (not that failing to do so seems
  // to hurt anything). more queues than that would be an error.
//NS_ASSERTION(!mHead || !mHead->mNext, "event queue token list deleted when not empty");
  // (and skip the assertion for now. we're leaking event queues because they
  // are referenced by things that leak, so this assertion goes off a lot.)
  if (mHead)
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("EventQueueTokenQueue::~EventQueueTokenQueue Destructor mToken=<%d>\n", mHead->mToken));

    int err=PtAppRemoveFd(NULL,mHead->mToken);
    if (err==-1)
    {
	  printf ("nsAppShell::~EventQueueTokenQueue Run Error calling PtAppRemoveFd mHead->mToken=<%d> errno=<%d>\n", mHead->mToken, errno);
	  //abort();
    }
    delete mHead;
    // and leak the rest. it's an error, anyway
  }
}

void EventQueueTokenQueue::PushToken(nsIEventQueue *aQueue, gint aToken)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("EventQueueTokenQueue::PushToken\n"));

  EventQueueToken *newToken = new EventQueueToken(aQueue, aToken);
  NS_ASSERTION(newToken, "couldn't allocate token queue element");
  if (!newToken)
    return NS_ERROR_OUT_OF_MEMORY;

  newToken->next = mHead;
  mHead = newToken;
  return NS_OK;
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
  mEventQueueTokens = new EventQueueTokenQueue();

  // throw on error would really be civilized here
  NS_ASSERTION(mEventQueueTokens, "couldn't allocate event queue token queue");

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

  delete mEventQueueTokens;
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
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::Spinup  this=<%p> mModalCount=<%d>\n", this, mModalCount));
  if (mModalCount != -1)
  {
    /* This should be -1 here... what is wrong? */
    NS_ASSERTION(0,"nsAppShell::Spinup  mModalCount is not -1!\n");
	abort();  
  }
  
  mModalCount = PtModalStart();
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Spindown - do any cleanup necessary for finishing a message loop
//
//-------------------------------------------------------------------------
NS_METHOD nsAppShell::Spindown()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::Spindown this=<%p> mModalCount=<%d>\n", this, mModalCount));
  PtModalEnd( mModalCount );
  mModalCount = -1;
  return NS_OK;
}


#if 0
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
      eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, &eQueue);

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
    eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, &eQueue);
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
#endif


/* This routine replaces the standard PtMainLoop() for Photon */
/* We had to replace it to provide a mechanism (ExitMainLoop) to exit */
/* the loop. */

void MyMainLoop( void ) 
{
	ExitMainLoop = 0;
	while (! ExitMainLoop)
	{
		PtProcessEvent();
	}

    NS_WARNING("MyMainLoop exiting!\n");

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
  rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, &EQueue);

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
  rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, &EQueue);
  if (NS_OK != rv) {
    NS_ASSERTION("Could not obtain the thread event queue", PR_FALSE);
    return rv;
  }    


done:

  // (has to be called explicitly for this, the primordial appshell, because
  // of startup ordering problems.)
  ListenToEventQueue(EQueue, PR_TRUE);

  MyMainLoop();		/* PtMainLoop() */

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
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::Exit.\n"));
  NS_WARNING("nsAppShell::Exit Called...\n");
  ExitMainLoop = 1;

  return NS_OK;
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

NS_IMETHODIMP nsAppShell::ListenToEventQueue(nsIEventQueue *aQueue,
                                             PRBool aListen)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::ListenToEventQueue\n"));

  gint queueToken;
  int  err;
  
  if (aListen)
  {
	err = PtAppAddFd(NULL,aQueue->GetEventQueueSelectFD(),Pt_FD_READ,
	        event_processor_callback,aQueue);
    if (err == -1)
	{
		printf("nsAppShell::ListenToEventQueue Error calling PtAppAddFd errno=<%d>\n", errno);
		if (errno != EBUSY)
		  abort();
	}

    mEventQueueTokens->PushToken(aQueue, queueToken);
  }
  else 
  {
    if (mEventQueueTokens->PopToken(aQueue, &queueToken))
	{
	  err=PtAppRemoveFd(NULL,aQueue->GetEventQueueSelectFD());
	  if (err == -1)
	  {	
		printf ("nsAppShell:: Run Error calling PtAppRemoveFd\n");
		abort();
	  }
	}
  }

  return NS_OK;
}
