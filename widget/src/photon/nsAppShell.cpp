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
#include "nsPhWidgetLog.h"

#include <Pt.h>
#include <errno.h>

/* Global Definitions */
PRBool nsAppShell::gExitMainLoop = PR_FALSE;

//-------------------------------------------------------------------------
//
// XPCOM CIDs
//
//-------------------------------------------------------------------------
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kCmdLineServiceCID, NS_COMMANDLINE_SERVICE_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

static int
our_photon_input_add (int               fd,
                      PtFdProc_t        event_processor_callback,
                      void		         *data)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::our_photon_input_add fd=<%d>\n", fd));

	int err = PtAppAddFd(NULL, fd, (Pt_FD_READ | Pt_FD_NOPOLL), 
	                     event_processor_callback,data);
    if (err != 0)
	{
		NS_ASSERTION(0,"nsAppShell::our_photon_input_add Error calling PtAppAddFD\n");
		abort();
	}
}

//-------------------------------------------------------------------------
//
// nsAppShell constructor
//
//-------------------------------------------------------------------------
nsAppShell::nsAppShell()  
{ 
//  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::nsAppShell Constructor this=<%p>\n", this));

  mEventQueue  = nsnull;
  mFD          = -1;
  NS_INIT_REFCNT();
}

//-------------------------------------------------------------------------
//
// nsAppShell destructor
//
//-------------------------------------------------------------------------
nsAppShell::~nsAppShell()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::~nsAppShell this=<%p> mFD=<%d>\n", this, mFD));

  if (mFD != -1)
  {
    int err=PtAppRemoveFd(NULL,mFD);
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::~nsAppShell Calling PtAppRemoveFd for mFD=<%d> err=<%d>\n", mFD, err));

    if (err==-1)
    {
      PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsAppShell::~nsAppShell Error calling PtAppRemoveFd mFD=<%d> errno=<%d>\n", mFD, errno));
	  printf("nsAppShell::~EventQueueTokenQueue Run Error calling PtAppRemoveFd mFD=<%d> errno=<%d>\n", mFD, errno);
	  //abort();
    }  
  }
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
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::SetDispatchListener. this=<%p>\n", this));
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Enter a message handler loop
//
//-------------------------------------------------------------------------
static int event_processor_callback(int fd, void *data, unsigned mode)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::event_processor_callback fd=<%d> data=<%p> mode=<%d>\n", fd, data, mode));

  nsIEventQueue *eventQueue = (nsIEventQueue*)data;
  if (eventQueue)
    eventQueue->ProcessPendingEvents();
  else
    NS_WARNING("nsAppShell::event_processor_callback eventQueue is NULL\n");

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
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::Spinup  this=<%p>\n", this));

  nsresult   rv = NS_OK;
  
  // Get the event queue service 
  NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueServiceCID, &rv);
  if (NS_FAILED(rv)) {
    NS_ASSERTION("Could not obtain event queue service", PR_FALSE);
    return rv;
  }

  //Get the event queue for the thread.
  rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, &mEventQueue);
  
  // If a queue already present use it.
  if (mEventQueue)
    return rv;

  // Create the event queue for the thread
  rv = eventQService->CreateThreadEventQueue();
  if (NS_OK != rv) {
    NS_ASSERTION("Could not create the thread event queue", PR_FALSE);
    return rv;
  }

  //Get the event queue for the thread
  rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, &mEventQueue);

  return rv;
}

//-------------------------------------------------------------------------
//
// Spindown - do any cleanup necessary for finishing a message loop
//
//-------------------------------------------------------------------------
NS_METHOD nsAppShell::Spindown()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::Spindown this=<%p>\n", this));

  if (mEventQueue)
  { 
    mEventQueue->ProcessPendingEvents();
    NS_RELEASE(mEventQueue);
  }

  return NS_OK;
}

/* This routine replaces the standard PtMainLoop() for Photon */
/* We had to replace it to provide a mechanism (ExitMainLoop) to exit */
/* the loop. */

void MyMainLoop( void ) 
{
	nsAppShell::gExitMainLoop = PR_FALSE;
	while (! nsAppShell::gExitMainLoop)
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
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::Run this=<%p>\n", this));

  if (!mEventQueue)
    Spinup();
  
  if (!mEventQueue)
    return NS_ERROR_NOT_INITIALIZED;

  mFD = mEventQueue->GetEventQueueSelectFD();
  our_photon_input_add(mFD, event_processor_callback, mEventQueue);

  MyMainLoop();

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
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::Exit.\n"));
  NS_WARNING("nsAppShell::Exit Called...\n");
  gExitMainLoop = PR_TRUE;

  return NS_OK;
}


NS_METHOD nsAppShell::GetNativeEvent(PRBool &aRealEvent, void *&aEvent)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::GetNativeEvent this=<%p>\n", this));
      
  aEvent = NULL;
  aRealEvent = PR_FALSE;

  return NS_OK;
}


NS_METHOD nsAppShell::DispatchNativeEvent(PRBool aRealEvent, void * aEvent)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::DispatchNativeEvent aRealEvent=<%d> aEvent=<%p> mEventQueue=<%p>\n", aRealEvent, aEvent, mEventQueue));


  if (!mEventQueue)
    return NS_ERROR_NOT_INITIALIZED;  

  PtProcessEvent();

  return mEventQueue->ProcessPendingEvents();
}

NS_IMETHODIMP nsAppShell::ListenToEventQueue(nsIEventQueue *aQueue,
                                             PRBool aListen)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsAppShell::ListenToEventQueue aQueue=<%p> aListen=<%d>\n", aQueue, aListen));
  return NS_OK;
}
