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

#include "plevent.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include <stdlib.h>


static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kIEventQueueServiceIID, NS_IEVENTQUEUESERVICE_IID);

#include "nsIWidget.h"

#include <Pt.h>
#include <errno.h>


NS_DEFINE_IID(kIAppShellIID, NS_IAPPSHELL_IID);
NS_IMPL_ISUPPORTS(nsAppShell,kIAppShellIID);

PRBool            nsAppShell::mPtInited = PR_FALSE;

#include <prlog.h>
PRLogModuleInfo  *PhWidLog =  PR_NewLogModule("PhWidLog");
#include "nsPhWidgetLog.h"


//-------------------------------------------------------------------------
//
// nsAppShell constructor
//
//-------------------------------------------------------------------------
nsAppShell::nsAppShell()  
{ 
  NS_INIT_REFCNT();
  mDispatchListener = nsnull;
  mEventBufferSz = sizeof( PhEvent_t ) + 1000;
  mEvent = (PhEvent_t*) malloc( mEventBufferSz );
  NS_ASSERTION( mEvent, "Out of memory" );

  /* Run this only once per application startup */
  if( !mPtInited )
  {
    PtInit( NULL );
    PtChannelCreate(); // Force use of pulses
    mPtInited = PR_TRUE;
  }
}


NS_METHOD nsAppShell::Spinup()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::Spinup - Not Implemented.\n"));
  return NS_OK;
}


NS_METHOD nsAppShell::Spindown()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::Spindown - Not Implemented.\n"));
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// PushThreadEventQueue - begin processing events from a new queue
//   note this is the Windows implementation and may suffice, but
//   this is untested on photon.
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
//   this is untested on photon.
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
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::GetNativeEvent - Not Implemented.\n"));
  return NS_OK;
}


NS_METHOD nsAppShell::DispatchNativeEvent(PRBool aRealEvent, void * aEvent)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::DispatchNativeEvent - Not Implemented.\n"));
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Create the application shell
//
//-------------------------------------------------------------------------

NS_METHOD nsAppShell::Create(int* argc, char ** argv)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::Create - Not Implemented.\n"));
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsAppShell::SetDispatchListener(nsDispatchListener* aDispatchListener) 
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
//struct PLEventQueue;
nsIEventQueue * kedlEQueue = nsnull;

int event_processor_callback(int fd, void *data, unsigned mode)
{
//	printf ("kedl: event processor callback!\n"); fflush (stdout);
//  printf ("kedl: to pr process events!\n"); fflush(stdout);
//    PR_ProcessPendingEvents(kedlEQueue);
//  printf ("kedl: back from pr process events!\n"); fflush(stdout);

nsIEventQueue * eventQueue = (nsIEventQueue *) data;
eventQueue->ProcessPendingEvents();
}

nsresult nsAppShell::Run()
{
  nsresult   rv = NS_OK;
  nsIEventQueue * EQueue = nsnull;

  // Get the event queue service 
  rv = nsServiceManager::GetService(kEventQueueServiceCID, 
                                    kIEventQueueServiceIID,
                                    (nsISupports **) &mEventQService);

  if (NS_OK != rv) {
    NS_ASSERTION(0,"Could not obtain event queue service");
    return rv;
  }

  printf("Got thew event queue from the service\n");
  //Get the event queue for the thread.
  rv = mEventQService->GetThreadEventQueue(PR_GetCurrentThread(), &EQueue);

  // If a queue already present use it.
  if (nsnull != EQueue)
     goto done;

  // Create the event queue for the thread
  rv = mEventQService->CreateThreadEventQueue();
  if (NS_OK != rv) {
    NS_ASSERTION(0,"Could not create the thread event queue");
    return rv;
  }
  //Get the event queue for the thread
  rv = mEventQService->GetThreadEventQueue(PR_GetCurrentThread(), &EQueue);
  if (NS_OK != rv) {
      NS_ASSERTION(0,"Could not obtain the thread event queue");
      return rv;
  }    

done:
	kedlEQueue=EQueue;
//	printf("setting up input with event queue\n");
	PtAppAddFd(NULL,EQueue->GetEventQueueSelectFD(),Pt_FD_READ,event_processor_callback,EQueue);

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::Run.\n"));

  NS_ADDREF_THIS();

  nsresult        result = NS_OK;
//  PhEvent_t       *event;
//  unsigned long   locEventBufferSz = sizeof( PhEvent_t ) + 4000; /* initially */

  PtMainLoop();

#if 0
  if( nsnull != ( event = (PhEvent_t*) malloc( locEventBufferSz )))
  {
    PRBool done = PR_FALSE;

    while(!done)
    {
      switch( PhEventNext( event, locEventBufferSz ))
      {
      case Ph_EVENT_MSG:
        PtEventHandler( event );
        if( mDispatchListener )
          mDispatchListener->AfterDispatch();
        break;

      case Ph_RESIZE_MSG:
        locEventBufferSz = PhGetMsgSize( event );
        printf( "nsAppShell::Run got resize msg (%lu).\n", locEventBufferSz );
        if(( event = (PhEvent_t*) realloc( event, locEventBufferSz )) == nsnull )
        {
          printf( "realloc barfed.\n" );
          result = 0; // Meaningful error code?
          done = PR_TRUE;
        }
        break;

      case -1:
        break;
      }
    }

    printf( "nsAppShell::Run exiting event loop.\n" );

    free( event );
  }
#endif

  Release();

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::Run - done.\n"));

  return result;
}


nsresult nsAppShell::GetNativeEvent(void *& aEvent, nsIWidget* aWidget, PRBool &aIsInWindow, PRBool &aIsMouseEvent)
{
  nsresult  result = NS_ERROR_FAILURE;
  PRBool   done = PR_FALSE;

  aIsInWindow = PR_FALSE;

//kedl
//  printf ("kedl: to pr process events!\n"); fflush(stdout);
//  PR_ProcessPendingEvents(kedlEQueue);
kedlEQueue->ProcessPendingEvents();
//  printf ("kedl: back from pr process events!\n"); fflush(stdout);

  while(!done)
  {
    // This method uses the class event buffer, m_Event, and the class event
    // buffer size, m_EventBufferSz. If we receive a Ph_RESIZE_MSG event, we
    // try to realloc the buffer to the new size specified in the event, then
    // we wait for another "real" event.

    switch( PhEventNext( mEvent, mEventBufferSz ))
    {
    case Ph_EVENT_MSG:
      switch( mEvent->type )
      {
        case Ph_EV_BUT_PRESS:
        case Ph_EV_BUT_RELEASE:
        case Ph_EV_BUT_REPEAT:
        case Ph_EV_PTR_MOTION_BUTTON:
        case Ph_EV_PTR_MOTION_NOBUTTON:
          aIsMouseEvent = PR_TRUE;
          break;

        default:
          aIsMouseEvent = PR_FALSE;
          break;
      }

      aEvent = mEvent;

      if( nsnull != aWidget )
      {
        // Get Native Window for dialog window
        PtWidget_t* win;
        win = (PtWidget_t*)aWidget->GetNativeData(NS_NATIVE_WINDOW);

        aIsInWindow = PR_TRUE;
      }
      done = PR_TRUE;
      break;

    case Ph_RESIZE_MSG:
      mEventBufferSz = PhGetMsgSize( mEvent );
      if(( mEvent = (PhEvent_t*)realloc( mEvent, mEventBufferSz )) == nsnull )
      {
        done = PR_TRUE;
      }
      break;

    case -1:
      done = PR_TRUE;
      break;
    }
  }

  return result;
}

nsresult nsAppShell::DispatchNativeEvent(void * aEvent)
{
  PtEventHandler( (PhEvent_t*)aEvent );

// REVISIT - not sure if we're supposed to to this here:
//  if( mDispatchListener )
//    mDispatchListener->AfterDispatch();

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
  exit ( EXIT_SUCCESS );

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// nsAppShell destructor
//
//-------------------------------------------------------------------------
nsAppShell::~nsAppShell()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::~nsAppShell - Not Implemented.\n"));
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

#if 0
NS_METHOD nsAppShell::GetSelectionMgr(nsISelectionMgr** aSelectionMgr)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::GetSelectionMgr - Not Implemented.\n"));

  return NS_OK;
}
#endif

NS_METHOD nsAppShell::EventIsForModalWindow(PRBool aRealEvent, void *aEvent, nsIWidget *aWidget, PRBool *aForWindow)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::EventIsForModalWindow - Not Implemented.\n"));

  return NS_OK;
}
