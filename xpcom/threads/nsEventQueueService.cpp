/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include "nsEventQueueService.h"
#include "prmon.h"
#include "nsComponentManager.h"
#include "nsIEventQueue.h"
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
  EventQueueEntry();
  virtual ~EventQueueEntry();

  // nsISupports interface...
  NS_DECL_ISUPPORTS

  nsIEventQueue* GetEventQueue(void); // addrefs!
  nsresult       MakeNewQueue(nsIEventQueue **aQueue);
  
  nsresult       AddQueue(void);
  void           RemoveQueue(nsIEventQueue *aQueue); // queue goes dark, and is released

private: 
  nsIEventQueue *mQueue;
};

/* nsISupports interface implementation... */
NS_IMPL_ISUPPORTS0(EventQueueEntry)

EventQueueEntry::EventQueueEntry()
{
  NS_INIT_REFCNT();
  MakeNewQueue(&mQueue);
  NS_ASSERTION(mQueue, "EventQueueEntry constructor failed");
}

EventQueueEntry::~EventQueueEntry()
{
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

////////////////////////////////////////////////////////////////////////////////

nsEventQueueServiceImpl::nsEventQueueServiceImpl()
{
  NS_INIT_REFCNT();

  mEventQTable   = new nsHashtable(16);
  mEventQMonitor = PR_NewMonitor();
}

static PRBool
DeleteEntry(nsHashKey *aKey, void *aData, void* closure)
{
  EventQueueEntry* evQueueEntry = (EventQueueEntry*)aData;

  if (NULL != evQueueEntry) {
    delete evQueueEntry;
  }
  return PR_TRUE;
}

nsEventQueueServiceImpl::~nsEventQueueServiceImpl()
{
  // Destroy any remaining PLEventQueue's still in the hash table...
  mEventQTable->Enumerate(DeleteEntry);
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
  nsresult rv = NS_OK;
  ThreadKey  key(PR_GetCurrentThread());
  EventQueueEntry* evQueueEntry;

  /* Enter the lock which protects the EventQ hashtable... */
  PR_EnterMonitor(mEventQMonitor);

  /* create only one event queue chain per thread... */
  evQueueEntry = (EventQueueEntry*)mEventQTable->Get(&key);
  if (NULL == evQueueEntry) {
    evQueueEntry = new EventQueueEntry();  
    if (NULL == evQueueEntry) {
      rv = NS_ERROR_OUT_OF_MEMORY;
      goto done;
    }

    mEventQTable->Put(&key, evQueueEntry);
  }
  NS_ADDREF(evQueueEntry);

done:
  // Release the EventQ lock...
  PR_ExitMonitor(mEventQMonitor);
  return rv;
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
  if (NULL != evQueueEntry) {
    nsrefcnt refcnt;

    NS_RELEASE2(evQueueEntry, refcnt);
    // Remove the entry from the hash table if this was the last reference...
    if (0 == refcnt) {
      mEventQTable->Remove(&key);
    }
  } else {
    rv = NS_ERROR_FAILURE;
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
    evQueueEntry = new EventQueueEntry();  
    if (NULL == evQueueEntry) {
      rv = NS_ERROR_OUT_OF_MEMORY;
      goto done;
    }
    mEventQTable->Put(&key, evQueueEntry);
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
    // Remove the entry from the hash table if this was the last reference...
    if (0 == refcnt) {
      mEventQTable->Remove(&key);
    }
    else {
      // We must be popping. (note: doesn't this always want to be executed?)
      // aQueue->ProcessPendingEvents(); // (theoretically not necessary)
      evQueueEntry->RemoveQueue(aQueue);
    }
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
  ThreadKey key(aThread);

  /* Enter the lock which protects the EventQ hashtable... */
  PR_EnterMonitor(mEventQMonitor);

  /* Parameter validation... */
  if ((NULL == aThread) || (NULL == aResult)) {
    rv = NS_ERROR_NULL_POINTER;
    goto done;
  }

  evQueueEntry = (EventQueueEntry*)mEventQTable->Get(&key);
  if (NULL != evQueueEntry) {
    *aResult = evQueueEntry->GetEventQueue(); // Queue addrefing is done by this call.
  } else {
    // XXX: Need error code for requesting an event queue when none exists...
    *aResult = NULL;
    rv = NS_ERROR_FAILURE;
    goto done;
  }

done:
  // Release the EventQ lock...
  PR_ExitMonitor(mEventQMonitor);
  return rv;
}

#ifdef XP_MAC
// Callback from the enumeration of the HashTable.
static  PRBool EventDispatchingFunc(nsHashKey *aKey, void *aData, void* closure)
{
	EventQueueEntry* entry = (EventQueueEntry*) aData;
	nsIEventQueue* eventQueue = entry->GetEventQueue();
	
	if (eventQueue) {
		eventQueue->ProcessPendingEvents();
		NS_RELEASE(eventQueue);
	}

	return true;
}

// MAC specific. Will go away someday
NS_IMETHODIMP nsEventQueueServiceImpl::ProcessEvents() 
{
	if ( mEventQTable )
		mEventQTable->Enumerate( EventDispatchingFunc, NULL  );
	return NS_OK;
}

#endif 

////////////////////////////////////////////////////////////////////////////////
