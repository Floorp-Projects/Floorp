/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
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

#include "nsCOMPtr.h"
#include "nsEventQueue.h"
#include "nsIEventQueueService.h"
#include "nsIThread.h"

#include "nsIServiceManager.h"
#include "nsIObserverService.h"

#include "nsString.h"

#include "prlog.h"

#if defined(PR_LOGGING) && defined(DEBUG_danm)
/* found these logs useful in conjunction with netlibStreamEvent logging
   from netwerk. */
PRLogModuleInfo* gEventQueueLog = 0;
PRUint32 gEventQueueLogCount = 0;
PRUint32 gEventQueueLogPPCount = 0;
static int gEventQueueLogPPLevel = 0;
static PLEventQueue *gEventQueueLogQueue = 0;
static PRThread *gEventQueueLogThread = 0;
#endif

// in a real system, these would be members in a header class...
static char *gActivatedNotification = "nsIEventQueueActivated";
static char *gDestroyedNotification = "nsIEventQueueDestroyed";

nsEventQueueImpl::nsEventQueueImpl()
{
  NS_INIT_REFCNT();
  NS_ADDREF_THIS();
  /* The slightly weird ownership model for eventqueues goes like this:
   
     General:
       There's an addref from the factory generally held by whoever asked for
     the queue. The queue addrefs itself (right here) and releases itself
     after someone calls StopAcceptingEvents() on the queue and when it is
     dark and empty (in CheckForDeactivation()).

     Chained queues:

       Eldest queue:
         The eldest queue in a chain is held on to by the EventQueueService
       in a hash table, so it is possible that the eldest queue may not be
       released until the EventQueueService is shutdown.
         You may not call StopAcceptingEvents() on this queue until you have
       done so on all younger queues.

       General:
         Each queue holds a reference to their immediate elder link and a weak
       reference to their immediate younger link.  Because you must shut down
       queues from youngest to eldest, all the references will be removed.

       It happens something like:
         queue->StopAcceptingEvents()
         {
           CheckForDeactivation()
           {
             -- hopefully we are able to shutdown now --
             Unlink()
             {
               -- remove the reference we hold to our elder queue  --
               -- NULL out our elder queues weak reference to us --
             }
             RELEASE ourself (to balance the ADDREF here in the constructor)
             -- and we should go away. --
           }
         }


     Notes:
       A dark queue no longer accepts events.  An empty queue simply has no events.
  */

#if defined(PR_LOGGING) && defined(DEBUG_danm)
  PR_LOG(gEventQueueLog, PR_LOG_DEBUG,
         ("EventQueue: Created [queue=%lx]",(long)mEventQueue));
  ++gEventQueueLogCount;
#endif

  mYoungerQueue = nsnull;
  mEventQueue = nsnull;
  mAcceptingEvents = PR_TRUE;
  mCouldHaveEvents = PR_TRUE;
}

nsEventQueueImpl::~nsEventQueueImpl()
{
  Unlink();

#if defined(PR_LOGGING) && defined(DEBUG_danm)
  PR_LOG(gEventQueueLog, PR_LOG_DEBUG,
         ("EventQueue: Destroyed [queue=%lx]",(long)mEventQueue));
  ++gEventQueueLogCount;
#endif

  if (mEventQueue) {
    NotifyObservers(gDestroyedNotification);
    PL_DestroyEventQueue(mEventQueue);
  }
}

NS_IMETHODIMP 
nsEventQueueImpl::Init(PRBool aNative)
{
  PRThread *thread = PR_GetCurrentThread();
  if (aNative)
    mEventQueue = PL_CreateNativeEventQueue("Thread event queue...", thread);
  else
    mEventQueue = PL_CreateMonitoredEventQueue("Thread event queue...", thread);
  NotifyObservers(gActivatedNotification);
  return NS_OK;
}

NS_IMETHODIMP 
nsEventQueueImpl::InitFromPRThread(PRThread* thread, PRBool aNative)
{
  if (thread == NS_CURRENT_THREAD) 
  {
     thread = PR_GetCurrentThread();
  }
  else if (thread == NS_UI_THREAD) 
  {
    nsCOMPtr<nsIThread>  mainIThread;
    nsresult rv;
  
    // Get the primordial thread
    rv = nsIThread::GetMainThread(getter_AddRefs(mainIThread));
    if (NS_FAILED(rv)) return rv;

    rv = mainIThread->GetPRThread(&thread);
    if (NS_FAILED(rv)) return rv;
  }  

  if (aNative)
    mEventQueue = PL_CreateNativeEventQueue("Thread event queue...", thread);
  else
    mEventQueue = PL_CreateMonitoredEventQueue("Thread event queue...", thread);
  NotifyObservers(gActivatedNotification);
  return NS_OK;
}

NS_IMETHODIMP
nsEventQueueImpl::InitFromPLQueue(PLEventQueue* aQueue)
{
  mEventQueue = aQueue;
  NotifyObservers(gActivatedNotification);
  return NS_OK;
}

/* nsISupports interface implementation... */
NS_IMPL_THREADSAFE_ISUPPORTS2(nsEventQueueImpl,
                              nsIEventQueue,
                              nsPIEventQueueChain)

/* nsIEventQueue interface implementation... */

NS_IMETHODIMP
nsEventQueueImpl::StopAcceptingEvents()
{
  // this assertion is bogus.  I should be able to shut down the eldest queue,
  //    as long as there are no younger children


  NS_ASSERTION(mElderQueue || !mYoungerQueue, "attempted to disable eldest queue in chain");
  mAcceptingEvents = PR_FALSE;
  CheckForDeactivation();
#if defined(PR_LOGGING) && defined(DEBUG_danm)
  PR_LOG(gEventQueueLog, PR_LOG_DEBUG,
         ("EventQueue: StopAccepting [queue=%lx, accept=%d, could=%d]",
         (long)mEventQueue,(int)mAcceptingEvents,(int)mCouldHaveEvents));
  ++gEventQueueLogCount;
#endif
  return NS_OK;
}

// utility funtion to send observers a notification
void
nsEventQueueImpl::NotifyObservers(const char *aTopic)
{
  nsresult rv;

  nsCOMPtr<nsIObserverService> os = do_GetService("@mozilla.org/observer-service;1", &rv);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIEventQueue> kungFuDeathGrip(this);
    nsCOMPtr<nsISupports> us(do_QueryInterface(kungFuDeathGrip));
    os->NotifyObservers(us, aTopic, NULL);
  }
}


NS_IMETHODIMP
nsEventQueueImpl::InitEvent(PLEvent* aEvent,
                            void* owner, 
                            PLHandleEventProc handler,
                            PLDestroyEventProc destructor)
{
	PL_InitEvent(aEvent, owner, handler, destructor);
	return NS_OK;
}



NS_IMETHODIMP_(PRStatus)
nsEventQueueImpl::PostEvent(PLEvent* aEvent)
{
  if (!mAcceptingEvents) {
#if defined(PR_LOGGING) && defined(DEBUG_danm)
    PR_LOG(gEventQueueLog, PR_LOG_DEBUG,
           ("EventQueue: Punt posted event [queue=%lx, accept=%d, could=%d]",
         (long)mEventQueue,(int)mAcceptingEvents,(int)mCouldHaveEvents));
  ++gEventQueueLogCount;
#endif
    PRStatus rv = PR_FAILURE;
    NS_ASSERTION(mElderQueue, "event dropped because event chain is dead");
    if (mElderQueue) {
      nsCOMPtr<nsIEventQueue> elder(do_QueryInterface(mElderQueue));
      if (elder)
        rv = elder->PostEvent(aEvent);
    }
    return rv;
  }
#if defined(PR_LOGGING) && defined(DEBUG_danm)
  PR_LOG(gEventQueueLog, PR_LOG_DEBUG,
         ("EventQueue: Posting event [queue=%lx]", (long)mEventQueue));
  ++gEventQueueLogCount;
#endif
  return PL_PostEvent(mEventQueue, aEvent);
}

NS_IMETHODIMP
nsEventQueueImpl::PostSynchronousEvent(PLEvent* aEvent, void** aResult)
{
  if (!mAcceptingEvents) {
#if defined(PR_LOGGING) && defined(DEBUG_danm)
    PR_LOG(gEventQueueLog, PR_LOG_DEBUG,
           ("EventQueue: Punt posted synchronous event [queue=%lx, accept=%d, could=%d]",
         (long)mEventQueue,(int)mAcceptingEvents,(int)mCouldHaveEvents));
  ++gEventQueueLogCount;
#endif
    nsresult rv = NS_ERROR_NO_INTERFACE;
    NS_ASSERTION(mElderQueue, "event dropped because event chain is dead");
    if (mElderQueue) {
      nsCOMPtr<nsIEventQueue> elder(do_QueryInterface(mElderQueue));
      if (elder)
        rv = elder->PostSynchronousEvent(aEvent, aResult);
      return rv;
    }
    return NS_ERROR_ABORT;
  }

#if defined(PR_LOGGING) && defined(DEBUG_danm)
  PR_LOG(gEventQueueLog, PR_LOG_DEBUG,
         ("EventQueue: Posting synchronous event [queue=%lx]", (long)mEventQueue));
  ++gEventQueueLogCount;
#endif
  void* result = PL_PostSynchronousEvent(mEventQueue, aEvent);
  if (aResult)
    *aResult = result;

  return NS_OK;
}

NS_IMETHODIMP
nsEventQueueImpl::EnterMonitor()
{
  PL_ENTER_EVENT_QUEUE_MONITOR(mEventQueue);
  return NS_OK;
}

NS_IMETHODIMP
nsEventQueueImpl::ExitMonitor()
{
  PL_EXIT_EVENT_QUEUE_MONITOR(mEventQueue);
  return NS_OK;
}


NS_IMETHODIMP
nsEventQueueImpl::RevokeEvents(void* owner)
{
  PL_RevokeEvents(mEventQueue, owner);
  if (mElderQueue) {
    nsCOMPtr<nsIEventQueue> elder(do_QueryInterface(mElderQueue));
    if (elder)
      elder->RevokeEvents(owner);
  }
  return NS_OK;
}


NS_IMETHODIMP
nsEventQueueImpl::GetPLEventQueue(PLEventQueue** aEventQueue)
{
  if (!mEventQueue)
    return NS_ERROR_NULL_POINTER;

  *aEventQueue = mEventQueue;
  return NS_OK;
}

NS_IMETHODIMP
nsEventQueueImpl::IsQueueOnCurrentThread(PRBool *aResult)
{
    *aResult = PL_IsQueueOnCurrentThread( mEventQueue );
    return NS_OK;
}


NS_IMETHODIMP
nsEventQueueImpl::IsQueueNative(PRBool *aResult)
{
  *aResult = PL_IsQueueNative(mEventQueue);
  return NS_OK;
}

NS_IMETHODIMP
nsEventQueueImpl::PendingEvents(PRBool *aResult)
{
    *aResult = PL_EventAvailable(mEventQueue);
    if (!*aResult && mElderQueue) {
        nsCOMPtr<nsIEventQueue> elder(do_QueryInterface(mElderQueue));
        if (elder)
            return elder->EventAvailable(*aResult);
    }
    return NS_OK;
}


NS_IMETHODIMP
nsEventQueueImpl::ProcessPendingEvents()
{
  PRBool correctThread = PL_IsQueueOnCurrentThread(mEventQueue);
  
  NS_ASSERTION(correctThread, "attemping to process events on the wrong thread");

  if (!correctThread)
    return NS_ERROR_FAILURE;
#if defined(PR_LOGGING) && defined(DEBUG_danm)
  ++gEventQueueLogPPLevel;
  if ((gEventQueueLogQueue != mEventQueue || gEventQueueLogThread != PR_GetCurrentThread() ||
       gEventQueueLogCount != gEventQueueLogPPCount) && gEventQueueLogPPLevel == 1) {
    PR_LOG(gEventQueueLog, PR_LOG_DEBUG,
           ("EventQueue: Process pending [queue=%lx, accept=%d, could=%d]",
           (long)mEventQueue,(int)mAcceptingEvents,(int)mCouldHaveEvents));
    gEventQueueLogPPCount = ++gEventQueueLogCount;
    gEventQueueLogQueue = mEventQueue;
    gEventQueueLogThread = PR_GetCurrentThread();
  }
#endif
  PL_ProcessPendingEvents(mEventQueue);
  CheckForDeactivation();

  if (mElderQueue) {
    nsCOMPtr<nsIEventQueue> elder(do_QueryInterface(mElderQueue));
    if (elder)
      elder->ProcessPendingEvents();
  }
#if defined(PR_LOGGING) && defined(DEBUG_danm)
  --gEventQueueLogPPLevel;
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsEventQueueImpl::EventLoop()
{
  PRBool correctThread = PL_IsQueueOnCurrentThread(mEventQueue);

  NS_ASSERTION(correctThread, "attemping to process events on the wrong thread");

  if (!correctThread)
    return NS_ERROR_FAILURE;

  PL_EventLoop(mEventQueue);
  return NS_OK;
}

NS_IMETHODIMP
nsEventQueueImpl::EventAvailable(PRBool& aResult)
{
  aResult = PL_EventAvailable(mEventQueue);
  return NS_OK;
}

NS_IMETHODIMP
nsEventQueueImpl::GetEvent(PLEvent** aResult)
{
  *aResult = PL_GetEvent(mEventQueue);
  CheckForDeactivation();
  return NS_OK;
}

NS_IMETHODIMP
nsEventQueueImpl::HandleEvent(PLEvent* aEvent)
{
  PRBool correctThread = PL_IsQueueOnCurrentThread(mEventQueue);
  NS_ASSERTION(correctThread, "attemping to process events on the wrong thread");
  if (!correctThread)
    return NS_ERROR_FAILURE;

#if defined(PR_LOGGING) && defined(DEBUG_danm)
  PR_LOG(gEventQueueLog, PR_LOG_DEBUG,
         ("EventQueue: handle event [queue=%lx, accept=%d, could=%d]",
         (long)mEventQueue,(int)mAcceptingEvents,(int)mCouldHaveEvents));
  ++gEventQueueLogCount;
#endif
  PL_HandleEvent(aEvent);
  return NS_OK;
}

NS_IMETHODIMP
nsEventQueueImpl::WaitForEvent(PLEvent** aResult)
{
    PRBool correctThread = PL_IsQueueOnCurrentThread(mEventQueue);
    NS_ASSERTION(correctThread, "attemping to process events on the wrong thread");
    if (!correctThread)
      return NS_ERROR_FAILURE;

#if defined(PR_LOGGING) && defined(DEBUG_danm)
  PR_LOG(gEventQueueLog, PR_LOG_DEBUG,
         ("EventQueue: wait for event [queue=%lx, accept=%d, could=%d]",
         (long)mEventQueue,(int)mAcceptingEvents,(int)mCouldHaveEvents));
  ++gEventQueueLogCount;
#endif
    *aResult = PL_WaitForEvent(mEventQueue);
    CheckForDeactivation();
    return NS_OK;
}

NS_IMETHODIMP_(PRInt32) 
nsEventQueueImpl::GetEventQueueSelectFD() 
{
  return PL_GetEventQueueSelectFD(mEventQueue);
}

NS_METHOD
nsEventQueueImpl::Create(nsISupports *aOuter,
                         REFNSIID aIID,
                         void **aResult)
{
  nsEventQueueImpl* evt = new nsEventQueueImpl();
  if (evt == NULL)
    return NS_ERROR_OUT_OF_MEMORY;
  nsresult rv = evt->QueryInterface(aIID, aResult);
  if (NS_FAILED(rv)) {
    delete evt;
  }
  return rv;
}

// ---------------- nsPIEventQueueChain -----------------

NS_IMETHODIMP
nsEventQueueImpl::AppendQueue(nsIEventQueue *aQueue)
{
  nsresult      rv;
  nsCOMPtr<nsIEventQueue> end;
  nsCOMPtr<nsPIEventQueueChain> queueChain(do_QueryInterface(aQueue));

  if (!aQueue)
    return NS_ERROR_NO_INTERFACE;

/* this would be nice
  NS_ASSERTION(aQueue->mYoungerQueue == NULL && aQueue->mElderQueue == NULL,
               "event queue repeatedly appended to queue chain");
*/
  rv = NS_ERROR_NO_INTERFACE;

  // (be careful doing this outside nsEventQueueService's mEventQMonitor)

  GetYoungest(getter_AddRefs(end));
  nsCOMPtr<nsPIEventQueueChain> endChain(do_QueryInterface(end));
  if (endChain) {
    endChain->SetYounger(queueChain);
    queueChain->SetElder(endChain);
    rv = NS_OK;
  }
  return rv;
}

NS_IMETHODIMP
nsEventQueueImpl::Unlink()
{
  nsCOMPtr<nsPIEventQueueChain> young = mYoungerQueue,
                                old = mElderQueue;

#if defined(PR_LOGGING) && defined(DEBUG_danm)
  PR_LOG(gEventQueueLog, PR_LOG_DEBUG,
         ("EventQueue: unlink [queue=%lx, younger=%lx, elder=%lx]",
         (long)mEventQueue,(long)mYoungerQueue, (long)mElderQueue.get()));
  ++gEventQueueLogCount;
#endif

  // this is probably OK, but shouldn't happen by design, so tell me if it does
  NS_ASSERTION(!mYoungerQueue, "event queue chain broken in middle");

  // break links early in case the Release cascades back onto us
  mYoungerQueue = nsnull;
  mElderQueue = nsnull;

  if (young)
    young->SetElder(old);
  if (old) {
    old->SetYounger(young);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsEventQueueImpl::GetYoungest(nsIEventQueue **aQueue)
{
  if (mYoungerQueue)
    return mYoungerQueue->GetYoungest(aQueue);

  nsIEventQueue *answer = NS_STATIC_CAST(nsIEventQueue *, this);
  NS_ADDREF(answer);
  *aQueue = answer;
  return NS_OK;
}

NS_IMETHODIMP
nsEventQueueImpl::GetYoungestActive(nsIEventQueue **aQueue)
{
  nsCOMPtr<nsIEventQueue> answer;

  if (mYoungerQueue)
    mYoungerQueue->GetYoungestActive(getter_AddRefs(answer));
  if (!answer) {
    if (mAcceptingEvents && mCouldHaveEvents)
      answer = NS_STATIC_CAST(nsIEventQueue *, this);
    else
      CheckForDeactivation();
  }
  *aQueue = answer;
  NS_IF_ADDREF(*aQueue);
  return NS_OK;
}

NS_IMETHODIMP
nsEventQueueImpl::SetYounger(nsPIEventQueueChain *aQueue)
{
  mYoungerQueue = aQueue;
  return NS_OK;
}

NS_IMETHODIMP
nsEventQueueImpl::SetElder(nsPIEventQueueChain *aQueue)
{
  mElderQueue = aQueue;
  return NS_OK;
}

NS_IMETHODIMP
nsEventQueueImpl::GetYounger(nsIEventQueue **aQueue)
{
  if (!mYoungerQueue) {
    *aQueue = nsnull;
    return NS_OK;
  }
  return mYoungerQueue->QueryInterface(NS_GET_IID(nsIEventQueue), (void**)&aQueue);
}

NS_IMETHODIMP
nsEventQueueImpl::GetElder(nsIEventQueue **aQueue)
{
  if (!mElderQueue) {
    *aQueue = nsnull;
    return NS_OK;
  }
  return mElderQueue->QueryInterface(NS_GET_IID(nsIEventQueue), (void**)&aQueue);
}

