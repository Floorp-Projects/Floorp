/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

static PLHashTable *sQueueHashTable = nsnull;
static PLHashTable *sCountHashTable = nsnull;

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
  int err = PtAppAddFd( NULL, fd, (Pt_FD_READ | Pt_FD_NOPOLL | Pt_FD_DRAIN ), event_processor_callback, data );
  if (err != 0)
  {
    NS_ASSERTION(0,"nsAppShell::our_photon_input_add Error calling PtAppAddFD\n");
  }

  return (err);
}

//-------------------------------------------------------------------------
//
// nsAppShell constructor
//
//-------------------------------------------------------------------------
nsAppShell::nsAppShell()  
{ 
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
  if (mFD != -1)
  {
    int err=PtAppRemoveFd(NULL,mFD);

    if (err==-1)
    {
	  printf("nsAppShell::~EventQueueTokenQueue Run Error calling PtAppRemoveFd mFD=<%d> errno=<%d>\n", mFD, errno);
    }  
    mFD = -1;
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
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Enter a message handler loop
//
//-------------------------------------------------------------------------

static int event_processor_callback(int fd, void *data, unsigned mode)
{
	nsIEventQueue *eventQueue = (nsIEventQueue*)data;
	PtHold();
	if (eventQueue)
	   eventQueue->ProcessPendingEvents();
	PtRelease();
  return Pt_CONTINUE;
}


//-------------------------------------------------------------------------
//
// Create the application shell
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsAppShell::Create(int *bac, char **bav)
{
  if (!PhWidLog)
    PhWidLog =  PR_NewLogModule("PhWidLog");

  int argc = bac ? *bac : 0;
  char **argv = bav;

  nsresult rv;

  nsCOMPtr<nsICmdLineService> cmdLineArgs = 
           do_GetService(kCmdLineServiceCID, &rv);
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
  nsresult   rv = NS_OK;

  // Get the event queue service
  nsCOMPtr<nsIEventQueueService> eventQService = do_GetService(kEventQueueServiceCID, &rv);

  if (NS_FAILED(rv)) {
    NS_ASSERTION("Could not obtain event queue service", PR_FALSE);
    return rv;
  }

  //Get the event queue for the thread.
  //rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(mEventQueue));
  rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, &mEventQueue);

  // If we got an event queue, use it.
  if (mEventQueue)
    goto done;

  // otherwise create a new event queue for the thread
  rv = eventQService->CreateThreadEventQueue();
  if (NS_FAILED(rv)) {
    NS_ASSERTION("Could not create the thread event queue", PR_FALSE);
    return rv;
  }

  // Ask again nicely for the event queue now that we have created one.
  //rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(mEventQueue));
  rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, &mEventQueue);

  // XXX shouldn't this be automatic?
 done:
  ListenToEventQueue(mEventQueue, PR_TRUE);

  return rv;
}

//-------------------------------------------------------------------------
//
// Spindown - do any cleanup necessary for finishing a message loop
//
//-------------------------------------------------------------------------
NS_METHOD nsAppShell::Spindown()
{
  if (mEventQueue) {
    ListenToEventQueue(mEventQueue, PR_FALSE);
    mEventQueue->ProcessPendingEvents();
    mEventQueue = nsnull;
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
  if (!mEventQueue)
    Spinup();

  if (!mEventQueue)
    return NS_ERROR_NOT_INITIALIZED;

  // kick up gtk_main.  this won't return until gtk_main_quit is called
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
  gExitMainLoop = PR_TRUE;

  return NS_OK;
}


NS_METHOD nsAppShell::GetNativeEvent(PRBool &aRealEvent, void *&aEvent)
{
  aRealEvent = PR_FALSE;
  aEvent = 0;

  return NS_OK;
}


NS_METHOD nsAppShell::DispatchNativeEvent(PRBool aRealEvent, void * aEvent)
{
  if (!mEventQueue)
    return NS_ERROR_NOT_INITIALIZED;  

	PtProcessEvent();
	PtFlush();
  return NS_OK;
}

#define NUMBER_HASH_KEY(_num) ((PLHashNumber) _num)

static PLHashNumber
IntHashKey(PRInt32 key)
{
  return NUMBER_HASH_KEY(key);
}

NS_IMETHODIMP nsAppShell::ListenToEventQueue(nsIEventQueue *aQueue,
                                             PRBool aListen)
{
  if (!sQueueHashTable) {
    sQueueHashTable = PL_NewHashTable(3, (PLHashFunction)IntHashKey,
                                      PL_CompareValues, PL_CompareValues, 0, 0);
  }
  if (!sCountHashTable) {
    sCountHashTable = PL_NewHashTable(3, (PLHashFunction)IntHashKey,
                                      PL_CompareValues, PL_CompareValues, 0, 0);
  }

  if (aListen) {
    /* add listener */
    PRInt32 key = aQueue->GetEventQueueSelectFD();

    /* only add if we arn't already in the table */
    if (!PL_HashTableLookup(sQueueHashTable, (void *)(key))) {
      PRInt32 tag;
      tag = our_photon_input_add(aQueue->GetEventQueueSelectFD(),
                              event_processor_callback,
                              aQueue);
      if (tag >= 0) {
        PL_HashTableAdd(sQueueHashTable, (void *)(key), (void *)(key));
      }
    }
    /* bump up the count */
    int count = (int)(PL_HashTableLookup(sCountHashTable, (void *)(key)));
    PL_HashTableAdd(sCountHashTable, (void *)(key), (void *)(count+1));
  } else {
    /* remove listener */
    PRInt32 key = aQueue->GetEventQueueSelectFD();

    int count = (int)(PL_HashTableLookup(sCountHashTable, (void *)(key)));
    if (count - 1 == 0) {
      int tag = (int)(PL_HashTableLookup(sQueueHashTable, (void *)(key)));
      if (tag > 0) {
      	PtAppRemoveFd(NULL, key);
        PL_HashTableRemove(sQueueHashTable, (void *)(key));
      }
    }
    PL_HashTableAdd(sCountHashTable, (void *)(key), (void *)(count-1));

  }

  return NS_OK;
}
