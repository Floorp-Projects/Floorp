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

static NS_DEFINE_CID(kEventQueueCID, NS_EVENTQUEUE_CID);

EventQueueStack::EventQueueStack(EventQueueStack* next)
:mNextQueue(next)
{
  // Create our thread queue using the component manager
	if (NS_FAILED(nsComponentManager::CreateInstance(kEventQueueCID, NULL, NS_GET_IID(nsIEventQueue),
		(void**)&mEventQueue))) {
		mEventQueue = NULL;
	}

	if (mEventQueue) {
		if (NS_FAILED(mEventQueue->Init()))
			mEventQueue = NULL;
	}
}

EventQueueStack::~EventQueueStack()
{
	// Destroy our thread queue.
	NS_IF_RELEASE(mEventQueue);

	if (mNextQueue)
	{
		delete mNextQueue;
	}
}

nsIEventQueue* EventQueueStack::GetEventQueue()
{
	if (mEventQueue) {
		NS_ADDREF(mEventQueue);
	}
	return mEventQueue;
}

EventQueueStack* EventQueueStack::GetNext()
{
	return mNextQueue;
}

void EventQueueStack::SetNext(EventQueueStack* aStack)
{
	mNextQueue = aStack;
}

////////////////////////////////////////////////////////////////////////////////

/* nsISupports interface implementation... */
NS_IMPL_ISUPPORTS0(EventQueueEntry)

EventQueueEntry::EventQueueEntry()
{
  NS_INIT_REFCNT();

	mQueueStack = new EventQueueStack();
}

EventQueueEntry::~EventQueueEntry()
{
  // Delete our stack
	delete mQueueStack;
}

nsIEventQueue* EventQueueEntry::GetEventQueue(void)
{
	// Return the current event queue on our stack.
  nsIEventQueue* answer = NULL;
	if (mQueueStack)
		answer = mQueueStack->GetEventQueue(); // This call addrefs the queue
	return answer;
}

void EventQueueEntry::PushQueue(void)
{
	// Make a new thread queue, connect it to our current stack, and then
	// set it to be the top of our stack.
	mQueueStack = new EventQueueStack(mQueueStack);
}

void EventQueueEntry::PopQueue(void)
{
	EventQueueStack* popped = mQueueStack;
	if (mQueueStack) {
		mQueueStack = mQueueStack->GetNext();
		popped->SetNext(NULL);
	}

	// This will result in the queue being released.
	delete popped;
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

  /* Only create one event queue per thread... */
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

NS_IMETHODIMP
nsEventQueueServiceImpl::PushThreadEventQueue()
{
	nsresult rv = NS_OK;
  ThreadKey  key(PR_GetCurrentThread());
  EventQueueEntry* evQueueEntry;

  /* Enter the lock which protects the EventQ hashtable... */
  PR_EnterMonitor(mEventQMonitor);

  /* Only create one event queue per thread... */
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
			evQueueEntry->PushQueue();
	}

  NS_ADDREF(evQueueEntry);

done:
  // Release the EventQ lock...
  PR_ExitMonitor(mEventQMonitor);
  return rv;
}

NS_IMETHODIMP
nsEventQueueServiceImpl::PopThreadEventQueue(void)
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
			// We must be popping.
			evQueueEntry->PopQueue();
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
