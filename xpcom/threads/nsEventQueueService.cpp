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
 * Contributor(s): Rick Potts <rpotts@netscape.com>
 *                 Ramiro Estrugo <ramiro@netscape.com>
 *                 Warren Harris <warren@netscape.com>
 *                 Leaf Nunes <leaf@mozilla.org>
 *                 David Matiskella <davidm@netscape.com>
 *                 David Hyatt <hyatt@netscape.com>
 *                 Seth Spitzer <sspitzer@netscape.com>
 *                 Suresh Duddi <dp@netscape.com>
 *                 Bruce Mitchener <bruce@cybersight.com>
 *                 Scott Collins <scc@netscape.com>
 *                 Dan Matejka <danm@netscape.com>
 *                 Doug Turner <dougt@netscape.com>
 *                 Stuart Parmenter <pavlov@netscape.com>
 *                 Mike Kaply <mkaply@us.ibm.com>
 *                 Dan Mosedale <dmose@mozilla.org>
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

#include "nsEventQueueService.h"
#include "prmon.h"
#include "nsIComponentManager.h"
#include "nsIEventQueue.h"
#include "nsIThread.h"
#include "nsPIEventQueueChain.h"

#include "prlog.h"

#if defined(PR_LOGGING) || defined(DEBUG_danm)
extern PRLogModuleInfo* gEventQueueLog;
extern PRUint32 gEventQueueLogCount;
#endif

static NS_DEFINE_CID(kEventQueueCID, NS_EVENTQUEUE_CID);

nsEventQueueServiceImpl::nsEventQueueServiceImpl()
{
  NS_INIT_REFCNT();

  mEventQMonitor = PR_NewMonitor();
#if defined(PR_LOGGING) && defined(DEBUG_danm)
  if (!gEventQueueLog)
    gEventQueueLog = PR_NewLogModule("nseventqueue");
#endif
}

PRBool hash_enum_remove_queues(nsHashKey *aKey, void *aData, void* closure)
{
  // 'queue' should be the eldest queue.
  nsIEventQueue *tmpQueue = (nsIEventQueue*)aData;

  nsCOMPtr<nsPIEventQueueChain> pie(do_QueryInterface(tmpQueue));
  nsCOMPtr<nsIEventQueue> q;

  // stop accepting events for youngest to oldest
  pie->GetYoungest(getter_AddRefs(q));
  while (q) {
    q->StopAcceptingEvents();

    nsCOMPtr<nsPIEventQueueChain> pq(do_QueryInterface(q));
    pq->GetElder(getter_AddRefs(q));
  }

  return PR_TRUE;
}

nsEventQueueServiceImpl::~nsEventQueueServiceImpl()
{
  // XXX make it so we only enum over this once
  mEventQTable.Enumerate(NS_STATIC_CAST(nsHashtableEnumFunc, hash_enum_remove_queues), nsnull); // call StopAcceptingEvents on everything
  mEventQTable.Reset(); // this should release all the 

  PR_DestroyMonitor(mEventQMonitor);
}

NS_METHOD
nsEventQueueServiceImpl::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  if (aOuter)
    return NS_ERROR_NO_AGGREGATION;
  nsEventQueueServiceImpl* eqs = new nsEventQueueServiceImpl();
  if (eqs == NULL)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(eqs);
  nsresult rv = eqs->QueryInterface(aIID, aResult);
  NS_RELEASE(eqs);
  return rv;
}

/* nsISupports interface implementation... */
NS_IMPL_THREADSAFE_ISUPPORTS1(nsEventQueueServiceImpl, nsIEventQueueService)

/* nsIEventQueueService interface implementation... */

NS_IMETHODIMP
nsEventQueueServiceImpl::CreateThreadEventQueue()
{
  return CreateEventQueue(PR_GetCurrentThread(), PR_TRUE);
}

NS_IMETHODIMP
nsEventQueueServiceImpl::CreateMonitoredThreadEventQueue()
{
  return CreateEventQueue(PR_GetCurrentThread(), PR_FALSE);
}

NS_IMETHODIMP
nsEventQueueServiceImpl::CreateFromIThread(nsIThread *aThread, PRBool aNative,
                                           nsIEventQueue **aResult)
{
  nsresult rv;
  PRThread *prThread;

  rv = aThread->GetPRThread(&prThread);
  if (NS_SUCCEEDED(rv)) {
    rv = CreateEventQueue(prThread, aNative); // addrefs
    if (NS_SUCCEEDED(rv))
      rv = GetThreadEventQueue(prThread, aResult); // addrefs
  }
  return rv;
}

// private method
NS_IMETHODIMP
nsEventQueueServiceImpl::MakeNewQueue(PRThread* thread,
                                      PRBool aNative,
                                      nsIEventQueue **aQueue)
{
  nsresult rv;
  nsCOMPtr<nsIEventQueue> queue = do_CreateInstance(kEventQueueCID, &rv);

  if (NS_SUCCEEDED(rv)) {
    rv = queue->InitFromPRThread(thread, aNative);
  }	
  *aQueue = queue;
  NS_IF_ADDREF(*aQueue);
  return rv;
}

// private method
NS_IMETHODIMP
nsEventQueueServiceImpl::CreateEventQueue(PRThread *aThread, PRBool aNative)
{
  nsresult rv = NS_OK;
  nsVoidKey  key(aThread);
  nsCOMPtr<nsIEventQueue> queue;

  /* Enter the lock which protects the EventQ hashtable... */
  PR_EnterMonitor(mEventQMonitor);

  /* create only one event queue chain per thread... */
  queue = getter_AddRefs((nsIEventQueue*)mEventQTable.Get(&key));

  if (!queue) {
    // we don't have one in the table
    rv = MakeNewQueue(PR_GetCurrentThread(), aNative, getter_AddRefs(queue)); // create new queue
    mEventQTable.Put(&key, queue); // add to the table (initial addref)
  }

  // Release the EventQ lock...
  PR_ExitMonitor(mEventQMonitor);
  return rv;
}


NS_IMETHODIMP
nsEventQueueServiceImpl::DestroyThreadEventQueue(void)
{
  nsresult rv = NS_OK;
  nsVoidKey key(PR_GetCurrentThread());

  /* Enter the lock which protects the EventQ hashtable... */
  PR_EnterMonitor(mEventQMonitor);

  nsCOMPtr<nsIEventQueue> queue;
  queue = getter_AddRefs((nsIEventQueue*)mEventQTable.Get(&key)); // remove nsIEventQueue from hash table (releases)
  if (queue) {
    queue->StopAcceptingEvents(); // tell the queue to stop accepting events
    queue = nsnull; // release the ref we hold on to now so that the queue might go away when we release below
    mEventQTable.Remove(&key); // remove nsIEventQueue from hash table (releases)
  }

  // Release the EventQ lock...
  PR_ExitMonitor(mEventQMonitor);
  return rv;
}

NS_IMETHODIMP
nsEventQueueServiceImpl::CreateFromPLEventQueue(PLEventQueue* aPLEventQueue, nsIEventQueue** aResult)
{
	// Create our thread queue using the component manager
	nsresult rv;
	nsCOMPtr<nsIEventQueue> queue = do_CreateInstance(kEventQueueCID, &rv);
	if (NS_FAILED(rv)) return rv;

  rv = queue->InitFromPLQueue(aPLEventQueue);
	if (NS_FAILED(rv)) return rv;

	*aResult = queue;
  NS_IF_ADDREF(*aResult);
	return NS_OK;
}


// Return the active event queue on our chain
/* inline */
nsresult nsEventQueueServiceImpl::GetYoungestEventQueue(nsIEventQueue *queue, nsIEventQueue **aResult)
{
  nsCOMPtr<nsIEventQueue> answer;

  if (queue) {
    nsCOMPtr<nsPIEventQueueChain> ourChain(do_QueryInterface(queue));
    if (ourChain)
      ourChain->GetYoungest(getter_AddRefs(answer));
    else
      answer = queue;
  }

  *aResult = answer;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}


// create new event queue, append it to the current thread's chain of event queues.
// return it, addrefed.
NS_IMETHODIMP
nsEventQueueServiceImpl::PushThreadEventQueue(nsIEventQueue **aNewQueue)
{
  nsresult rv = NS_OK;
  nsVoidKey  key(PR_GetCurrentThread());
  PRBool native = PR_TRUE; // native by default as per old comment


  NS_ASSERTION(aNewQueue, "PushThreadEventQueue called with null param");

  /* Enter the lock which protects the EventQ hashtable... */
  PR_EnterMonitor(mEventQMonitor);

  nsCOMPtr<nsIEventQueue> queue = getter_AddRefs((nsIEventQueue*)mEventQTable.Get(&key));
  NS_ASSERTION(queue, "pushed event queue on top of nothing");

  if (queue) { // find out what kind of queue our relatives are
    nsCOMPtr<nsIEventQueue> youngQueue;
    GetYoungestEventQueue(queue, getter_AddRefs(youngQueue));
    if (youngQueue) {
      queue->IsQueueNative(&native);
    }
  }

  nsCOMPtr<nsIEventQueue> newQueue;
  MakeNewQueue((PRThread*)key.GetValue(), native, getter_AddRefs(newQueue)); // create new queue

  if (!queue) {
    // shouldn't happen. as a fallback, we guess you wanted a native queue
    mEventQTable.Put(&key, newQueue);
  }

  // append to the event queue chain
  nsCOMPtr<nsPIEventQueueChain> ourChain(do_QueryInterface(queue)); // QI the queue in the hash table
  if (ourChain)
    ourChain->AppendQueue(newQueue); // append new queue to it

  *aNewQueue = newQueue;
  NS_IF_ADDREF(*aNewQueue);

#if defined(PR_LOGGING) && defined(DEBUG_danm)
  PLEventQueue *equeue;
  (*aNewQueue)->GetPLEventQueue(&equeue);
  PR_LOG(gEventQueueLog, PR_LOG_DEBUG,
         ("EventQueue: Service push queue [queue=%lx]",(long)equeue));
  ++gEventQueueLogCount;
#endif

  // Release the EventQ lock...
  PR_ExitMonitor(mEventQMonitor);
  return rv;
}

// disable and release the given queue (though the last one won't be released)
NS_IMETHODIMP
nsEventQueueServiceImpl::PopThreadEventQueue(nsIEventQueue *aQueue)
{
  nsresult rv = NS_OK;
  nsVoidKey key(PR_GetCurrentThread());

  /* Enter the lock which protects the EventQ hashtable... */
  PR_EnterMonitor(mEventQMonitor);


  nsCOMPtr<nsIEventQueue> queue = getter_AddRefs((nsIEventQueue*)mEventQTable.Get(&key)); // addrefs
  if (queue) {
#if defined(PR_LOGGING) && defined(DEBUG_danm)
    PLEventQueue *equeue;
    aQueue->GetPLEventQueue(&equeue);
    PR_LOG(gEventQueueLog, PR_LOG_DEBUG,
           ("EventQueue: Service pop queue [queue=%lx]",(long)equeue));
    ++gEventQueueLogCount;
#endif
    aQueue->StopAcceptingEvents();

    if (aQueue == queue.get()) { // are we reomving the eldest queue?
      mEventQTable.Remove(&key); // remove nsIEventQueue from hash table (releases)
    }

    // should we release aQueue ?? hmm...  only if it is in the hash table i think
  } else {
    rv = NS_ERROR_FAILURE;
  }

  // Release the EventQ lock...
  PR_ExitMonitor(mEventQMonitor);
  return rv;
}

NS_IMETHODIMP
nsEventQueueServiceImpl::GetThreadEventQueue(PRThread* aThread, nsIEventQueue** aResult)
{
  nsresult rv = NS_OK;

  /* Parameter validation... */
  if (NULL == aResult) return NS_ERROR_NULL_POINTER;
  
  PRThread* keyThread = aThread;

  if (keyThread == NS_CURRENT_THREAD) 
  {
     keyThread = PR_GetCurrentThread();
  }
  else if (keyThread == NS_UI_THREAD)
  {
    nsCOMPtr<nsIThread>  mainIThread;
    
    // Get the primordial thread
    rv = nsIThread::GetMainThread(getter_AddRefs(mainIThread));
    if (NS_FAILED(rv)) return rv;

    rv = mainIThread->GetPRThread(&keyThread);
    if (NS_FAILED(rv)) return rv;
  }

  nsVoidKey key(keyThread);

  /* Enter the lock which protects the EventQ hashtable... */
  PR_EnterMonitor(mEventQMonitor);

  nsCOMPtr<nsIEventQueue> queue = getter_AddRefs((nsIEventQueue*)mEventQTable.Get(&key));

  PR_ExitMonitor(mEventQMonitor);

  if (queue) {
    nsCOMPtr<nsIEventQueue> youngestQueue;
    GetYoungestEventQueue(queue, getter_AddRefs(youngestQueue)); // get the youngest active queue
    *aResult = youngestQueue;
    NS_IF_ADDREF(*aResult);
  } else {
    // XXX: Need error code for requesting an event queue when none exists...
    *aResult = NULL;
    rv = NS_ERROR_FAILURE;
  }

  return rv;
}


NS_IMETHODIMP
nsEventQueueServiceImpl::ResolveEventQueue(nsIEventQueue* queueOrConstant, nsIEventQueue* *resultQueue)
{
  if (queueOrConstant == NS_CURRENT_EVENTQ) {
    return GetThreadEventQueue(NS_CURRENT_THREAD, resultQueue);
  }
  else if (queueOrConstant == NS_UI_THREAD_EVENTQ) {
    return GetThreadEventQueue(NS_UI_THREAD, resultQueue);
  }

  *resultQueue = queueOrConstant;
  NS_ADDREF(*resultQueue);
  return NS_OK;
}

NS_IMETHODIMP
nsEventQueueServiceImpl::GetSpecialEventQueue(PRInt32 aQueue, 
                                              nsIEventQueue* *_retval)
{
  nsresult rv;

  // barf if someone gave us a zero pointer
  //
  if (!_retval) {
    return NS_ERROR_NULL_POINTER;
  }

  // try and get the requested event queue, returning NS_ERROR_FAILURE if there
  // is a problem.  GetThreadEventQueue() does the AddRef() for us.
  //
  switch (aQueue) {
  case CURRENT_THREAD_EVENT_QUEUE:
    rv = GetThreadEventQueue(NS_CURRENT_THREAD, _retval);
    if (NS_FAILED(rv)) {
      return NS_ERROR_FAILURE;
    }
    break;

  case UI_THREAD_EVENT_QUEUE:
    rv = GetThreadEventQueue(NS_UI_THREAD, _retval);
    if (NS_FAILED(rv)) {
      return NS_ERROR_FAILURE;
    }
    break;

    // somebody handed us a bogus constant
    //
  default:
    return NS_ERROR_ILLEGAL_VALUE;
  }

  return NS_OK;
}

