/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsEventQueueService.h"
#include "prmon.h"
#include "nsIComponentManager.h"
#include "nsIEventQueue.h"
#include "nsIThread.h"
#include "nsPIEventQueueChain.h"

static NS_DEFINE_CID(kEventQueueCID, NS_EVENTQUEUE_CID);

////////////////////////////////////////////////////////////////////////////////

// XXX move to nsID.h or nsHashtable.h? (copied from nsComponentManager.cpp)
class ThreadKey: public nsHashKey {
private:
  const PRThread* id;
  
public:
  ThreadKey(const PRThread* aID) {
    id = aID;
  }
  
  ThreadKey(const ThreadKey &aKey) {
    id = aKey.id;
  }
  
  PRUint32 HashValue(void) const {
    return (PRUint32)id;
  }

  PRBool Equals(const nsHashKey *aKey) const {
    return (id == ((const ThreadKey *) aKey)->id);
  }

  nsHashKey *Clone(void) const {
    return new ThreadKey(id);
  }
};

////////////////////////////////////////////////////////////////////////////////

/*
 * EventQueueEntry maintains the data associated with each entry in
 * the EventQueue service's hash table...
 *
 * It derives from nsISupports merely as a convienence since the entries are
 * reference counted...
 */
class EventQueueEntry : public nsISupports 
{
public:
  EventQueueEntry(nsEventQueueServiceImpl *aService, ThreadKey &aKey);
  virtual ~EventQueueEntry();

  // nsISupports interface...
  NS_DECL_ISUPPORTS

  nsIEventQueue* GetEventQueue(void); // addrefs!
  nsresult       MakeNewQueue(nsIEventQueue **aQueue);
  
  nsresult       AddQueue(void);
  void           RemoveQueue(nsIEventQueue *aQueue); // queue goes dark, and is released
  ThreadKey     *TheThreadKey(void)
                   { return &mHashKey; }

  // methods for accessing the linked list of event queue entries
  void           Link(EventQueueEntry *aAfter);
  void           Unlink(void);
  EventQueueEntry *Next(void)
                   { return mNextEntry; }

private:
  nsIEventQueue           *mQueue;
  ThreadKey                mHashKey;
  nsEventQueueServiceImpl *mService; // weak reference, obviously
  EventQueueEntry         *mPrevEntry,
                          *mNextEntry;
};

/* nsISupports interface implementation... */
NS_IMPL_ISUPPORTS0(EventQueueEntry)

EventQueueEntry::EventQueueEntry(nsEventQueueServiceImpl *aService, ThreadKey &aKey) :
                 mHashKey(aKey), mPrevEntry(0), mNextEntry(0)
{
  NS_INIT_REFCNT();
  mService = aService;
  MakeNewQueue(&mQueue);
  NS_ASSERTION(mQueue, "EventQueueEntry constructor failed");
  if (mService)
    mService->AddEventQueueEntry(this);
}

EventQueueEntry::~EventQueueEntry()
{
  if (mService)
    mService->RemoveEventQueueEntry(this);
  NS_IF_RELEASE(mQueue);
}

// Return the active event queue on our chain
nsIEventQueue* EventQueueEntry::GetEventQueue(void)
{
  nsIEventQueue* answer = NULL;

  if (mQueue) {
    nsCOMPtr<nsPIEventQueueChain> ourChain(do_QueryInterface(mQueue));
    if (ourChain)
      ourChain->GetYoungestActive(&answer);
    else {
      NS_ADDREF(mQueue);
      answer = mQueue;
    }
  }
  return answer;
}

nsresult EventQueueEntry::MakeNewQueue(nsIEventQueue **aQueue)
{
  nsIEventQueue *queue = 0;
  nsresult      rv;

  rv = nsComponentManager::CreateInstance(kEventQueueCID, NULL,
                NS_GET_IID(nsIEventQueue), (void**) &queue);

  if (NS_SUCCEEDED(rv)) {
    rv = queue->Init();
    if (NS_FAILED(rv)) {
      NS_RELEASE(queue);
      queue = 0;  // redundant, but makes me feel better
    }
  }	
  *aQueue = queue;
  return rv;
}

nsresult EventQueueEntry::AddQueue(void)
{
  nsIEventQueue *newQueue = NULL;
  nsresult      rv = NS_ERROR_NOT_INITIALIZED;

  if (mQueue) {
    rv = MakeNewQueue(&newQueue);

    // add it to our chain of queues
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsPIEventQueueChain> ourChain(do_QueryInterface(mQueue));
      if (ourChain)
        ourChain->AppendQueue(newQueue);
      else
        NS_RELEASE(newQueue);
    }
  }
  return rv;
}

void EventQueueEntry::RemoveQueue(nsIEventQueue *aQueue)
{
  aQueue->StopAcceptingEvents();
  NS_RELEASE(aQueue);
  // it's now gone dark, and will be deleted and unlinked as soon as
  // everyone else lets go
}

// this is an apology for symmetry breaking between EventQueueEntry and nsEventQueueServiceImpl.
// the latter handles both sides of the dual data structure containing the former, those being
// nsEventQueueServiceImpl's hashtable and linked list, while the former contains the code
// for handling linked lists, on which the latter relies. if that was complicated, it's because
// it is, and thus the apology.
void EventQueueEntry::Link(EventQueueEntry *aAfter)
{
  if (aAfter) {
    mNextEntry = aAfter->mNextEntry;
    if (mNextEntry)
      mNextEntry->mPrevEntry = this;
    aAfter->mNextEntry = this;
  } else
    mNextEntry = 0;
  mPrevEntry = aAfter;
}

void EventQueueEntry::Unlink(void)
{
  if (mNextEntry)
    mNextEntry->mPrevEntry = mPrevEntry;
  if (mPrevEntry)
    mPrevEntry->mNextEntry = mNextEntry;
  mNextEntry = 0;
  mPrevEntry = 0;
}

////////////////////////////////////////////////////////////////////////////////

EventQueueEntryEnumerator::EventQueueEntryEnumerator()
{
  mCurrent = 0;
}

EventQueueEntryEnumerator::~EventQueueEntryEnumerator()
{
}

void EventQueueEntryEnumerator::Reset(EventQueueEntry *aStart)
{
  mCurrent = aStart;
}

void EventQueueEntryEnumerator::Skip(EventQueueEntry *aSkip)
{
  if (mCurrent == aSkip)
    mCurrent = aSkip->Next();
}

EventQueueEntry *
EventQueueEntryEnumerator::Get(void)
{
  EventQueueEntry *rtnval = mCurrent;
  if (mCurrent)
    mCurrent = mCurrent->Next();
  return rtnval;
}

////////////////////////////////////////////////////////////////////////////////
/* TODO: unify the dual, redundant data structures holding EventQueueEntrys:
   they're held simultaneously in both a hashtable and a linked list.
*/

nsEventQueueServiceImpl::nsEventQueueServiceImpl()
{
  NS_INIT_REFCNT();

  mEventQTable   = new nsHashtable(16);
  mEventQMonitor = PR_NewMonitor();
  mBaseEntry     = 0;
}

nsEventQueueServiceImpl::~nsEventQueueServiceImpl()
{
  EventQueueEntry *entry;

  // Destroy any remaining PLEventQueues
  mEnumerator.Reset(mBaseEntry);
  while ((entry = mEnumerator.Get()) != 0)
    delete entry;

  delete mEventQTable;

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
NS_IMPL_ISUPPORTS1(nsEventQueueServiceImpl,nsIEventQueueService)

/* nsIEventQueueService interface implementation... */

NS_IMETHODIMP
nsEventQueueServiceImpl::CreateThreadEventQueue(void)
{
  return CreateEventQueue(PR_GetCurrentThread());
}

NS_IMETHODIMP
nsEventQueueServiceImpl::CreateFromIThread(
    nsIThread *aThread, nsIEventQueue **aResult)
{
  nsresult rv;
  PRThread *prThread;

  rv = aThread->GetPRThread(&prThread);
  if (NS_SUCCEEDED(rv)) {
    rv = CreateEventQueue(prThread); // addrefs
    if (NS_SUCCEEDED(rv))
      rv = GetThreadEventQueue(prThread, aResult); // doesn't addref
  }
  return rv;
}

NS_IMETHODIMP
nsEventQueueServiceImpl::CreateEventQueue(PRThread *aThread)
{
  nsresult rv = NS_OK;
  ThreadKey  key(aThread);
  EventQueueEntry* evQueueEntry;

  /* Enter the lock which protects the EventQ hashtable... */
  PR_EnterMonitor(mEventQMonitor);

  /* create only one event queue chain per thread... */
  evQueueEntry = (EventQueueEntry*)mEventQTable->Get(&key);
  if (NULL == evQueueEntry) {
    evQueueEntry = new EventQueueEntry(this, key);  
    if (NULL == evQueueEntry) {
      rv = NS_ERROR_OUT_OF_MEMORY;
      goto done;
    }
  }
  NS_ADDREF(evQueueEntry);

done:
  // Release the EventQ lock...
  PR_ExitMonitor(mEventQMonitor);
  return rv;
}

void
nsEventQueueServiceImpl::AddEventQueueEntry(EventQueueEntry *aEntry)
{
  EventQueueEntry *last, *current;

  // add to the hashtable, then to the end of the linked list
  mEventQTable->Put(aEntry->TheThreadKey(), aEntry);
  if (mBaseEntry) {
    for (last = 0, current = mBaseEntry; current; current = current->Next())
      last = current;
    aEntry->Link(last);
  } else
    mBaseEntry = aEntry;
}

void
nsEventQueueServiceImpl::RemoveEventQueueEntry(EventQueueEntry *aEntry)
{
  mEventQTable->Remove(aEntry->TheThreadKey());
  if (mBaseEntry == aEntry)
    mBaseEntry = aEntry->Next();
  mEnumerator.Skip(aEntry);
  aEntry->Unlink();
}

NS_IMETHODIMP
nsEventQueueServiceImpl::DestroyThreadEventQueue(void)
{
  nsresult rv = NS_OK;
  ThreadKey key(PR_GetCurrentThread());
  EventQueueEntry* evQueueEntry;

  /* Enter the lock which protects the EventQ hashtable... */
  PR_EnterMonitor(mEventQMonitor);

  evQueueEntry = (EventQueueEntry*)mEventQTable->Get(&key);
  if (NULL != evQueueEntry)
    NS_RELEASE(evQueueEntry);
  else
    rv = NS_ERROR_FAILURE;

  // Release the EventQ lock...
  PR_ExitMonitor(mEventQMonitor);
  return rv;
}

NS_IMETHODIMP
nsEventQueueServiceImpl::CreateFromPLEventQueue(PLEventQueue* aPLEventQueue, nsIEventQueue** aResult)
{
	// Create our thread queue using the component manager
	nsresult rv;
	nsIEventQueue* aQueue;
	if (NS_FAILED(rv = nsComponentManager::CreateInstance(kEventQueueCID, NULL, NS_GET_IID(nsIEventQueue),
		            (void**)&aQueue))) {
		return rv;
	}

	if (NS_FAILED(rv = aQueue->InitFromPLQueue(aPLEventQueue))) {
		NS_IF_RELEASE(aQueue);
		return rv;
	}

	*aResult = aQueue;
	return NS_OK;
}

// create new event queue, append it to the current thread's chain of event queues.
// return it, addrefed.
NS_IMETHODIMP
nsEventQueueServiceImpl::PushThreadEventQueue(nsIEventQueue **aNewQueue)
{
  nsresult rv = NS_OK;
  ThreadKey  key(PR_GetCurrentThread());
  EventQueueEntry* evQueueEntry;

  NS_ASSERTION(aNewQueue, "PushThreadEventQueue called with null param");
  *aNewQueue = NULL;

  /* Enter the lock which protects the EventQ hashtable... */
  PR_EnterMonitor(mEventQMonitor);

  evQueueEntry = (EventQueueEntry*)mEventQTable->Get(&key);
  if (NULL == evQueueEntry) {
    evQueueEntry = new EventQueueEntry(this, key);  
    if (NULL == evQueueEntry) {
      rv = NS_ERROR_OUT_OF_MEMORY;
      goto done;
    }
  }
  else {
    // An entry was already present. We need to push a new
    // queue onto our stack.
    rv = evQueueEntry->AddQueue();
  }

  if (NS_SUCCEEDED(rv)) {
    *aNewQueue = evQueueEntry->GetEventQueue();
    NS_ADDREF(evQueueEntry);
  }

done:
  // Release the EventQ lock...
  PR_ExitMonitor(mEventQMonitor);
  return rv;
}

// disable and release the given queue (though the last one won't be released)
NS_IMETHODIMP
nsEventQueueServiceImpl::PopThreadEventQueue(nsIEventQueue *aQueue)
{
  nsresult rv = NS_OK;
  ThreadKey key(PR_GetCurrentThread());
  EventQueueEntry* evQueueEntry;

  /* Enter the lock which protects the EventQ hashtable... */
  PR_EnterMonitor(mEventQMonitor);

  evQueueEntry = (EventQueueEntry*)mEventQTable->Get(&key);
  if (NULL != evQueueEntry) {
    nsrefcnt refcnt;

    NS_RELEASE2(evQueueEntry, refcnt);
    // If this wasn't the last reference, we must be popping.
    if (refcnt > 0)
      evQueueEntry->RemoveQueue(aQueue);
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
  EventQueueEntry* evQueueEntry;

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

  ThreadKey key(keyThread);

  /* Enter the lock which protects the EventQ hashtable... */
  PR_EnterMonitor(mEventQMonitor);

  evQueueEntry = (EventQueueEntry*)mEventQTable->Get(&key);
  
  PR_ExitMonitor(mEventQMonitor);

  if (NULL != evQueueEntry) {
    *aResult = evQueueEntry->GetEventQueue(); // Queue addrefing is done by this call.
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
    if (queueOrConstant == NS_CURRENT_EVENTQ)
    {
        return GetThreadEventQueue(NS_CURRENT_THREAD, resultQueue);
    }
    else if (queueOrConstant == NS_UI_THREAD_EVENTQ)
    {
        return GetThreadEventQueue(NS_UI_THREAD, resultQueue);
    }

    *resultQueue = queueOrConstant;
    NS_ADDREF(*resultQueue);
    return NS_OK;
}


#ifdef XP_MAC
// MAC specific. Will go away someday
// Bwah ha ha h ha ah aha ha ha
NS_IMETHODIMP nsEventQueueServiceImpl::ProcessEvents() 
{
  if (mEventQTable) {
    EventQueueEntry *entry;
    nsIEventQueue   *queue;

    // never use the hashtable enumerator if there's a chance (there is) that an
    // event queue entry could be destroyed while inside. this enumerator can
    // handle that.
    PR_EnterMonitor(mEventQMonitor);
    mEnumerator.Reset(mBaseEntry);
    while ((entry = mEnumerator.Get()) != 0) {
      PR_ExitMonitor(mEventQMonitor);
      queue = entry->GetEventQueue();
	  if (queue) {
	    queue->ProcessPendingEvents();
	    NS_RELEASE(queue);
      }
      PR_EnterMonitor(mEventQMonitor);
    }
    PR_ExitMonitor(mEventQMonitor);
  }
  return NS_OK;
}

#endif 

////////////////////////////////////////////////////////////////////////////////
