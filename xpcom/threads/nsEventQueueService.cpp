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
#include "prmon.h"
#include "nsComponentManager.h"
#include "nsIEventQueueService.h"
#include "nsIEventQueue.h"
#include "nsHashtable.h"
#include "nsXPComFactory.h"

static NS_DEFINE_IID(kIEventQueueServiceIID, NS_IEVENTQUEUESERVICE_IID);

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


/* This class is used with the EventQueueEntries to allow us to nest
   event queues */
class EventQueueStack
{
public:
	EventQueueStack(EventQueueStack* next = NULL);
	~EventQueueStack();

	nsIEventQueue* GetEventQueue();
	EventQueueStack* GetNext();
  void SetNext(EventQueueStack* aStack);

private:
	nsIEventQueue* mEventQueue;
	EventQueueStack* mNextQueue;
};

static NS_DEFINE_IID(kIEventQueueIID, NS_IEVENTQUEUE_IID);
static NS_DEFINE_IID(kEventQueueCID, NS_EVENTQUEUE_CID);

EventQueueStack::EventQueueStack(EventQueueStack* next)
:mNextQueue(next)
{
  // Create our thread queue using the component manager
	if (NS_FAILED(nsComponentManager::CreateInstance(kEventQueueCID, NULL, kIEventQueueIID,
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

/*
 * This class maintains the data associated with each entry in the EventQueue
 * service's hash table...
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

  nsIEventQueue* GetEventQueue(void);
  
	void PushQueue(void);
  void PopQueue(void);

private: 
	EventQueueStack* mQueueStack;
};

/* nsISupports interface implementation... */
NS_IMPL_ISUPPORTS(EventQueueEntry,kISupportsIID);

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

class nsEventQueueServiceImpl : public nsIEventQueueService
{
public:
  nsEventQueueServiceImpl();
  virtual ~nsEventQueueServiceImpl();

  // nsISupports interface...
  NS_DECL_ISUPPORTS

  // nsIEventQueueService interface...
  NS_IMETHOD CreateThreadEventQueue(void);
  NS_IMETHOD DestroyThreadEventQueue(void);
  NS_IMETHOD GetThreadEventQueue(PRThread* aThread, nsIEventQueue** aResult);

	NS_IMETHOD CreateFromPLEventQueue(PLEventQueue* aPLEventQueue, nsIEventQueue** aResult);

	NS_IMETHOD PushThreadEventQueue(void);
	NS_IMETHOD PopThreadEventQueue(void);

#ifdef XP_MAC
  NS_IMETHOD ProcessEvents();
#endif // XP_MAC

private:
  nsHashtable* mEventQTable;
  PRMonitor*   mEventQMonitor;
};


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

/* nsISupports interface implementation... */
NS_IMPL_ISUPPORTS(nsEventQueueServiceImpl,kIEventQueueServiceIID);

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
	if (NS_FAILED(rv = nsComponentManager::CreateInstance(kEventQueueCID, NULL, kIEventQueueIID,
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
//----------------------------------------------------------------------

static nsEventQueueServiceImpl* gServiceInstance = NULL;

NS_DEF_FACTORY(EventQueueServiceGen,nsEventQueueServiceImpl)

class nsEventQueueServiceFactory : public nsEventQueueServiceGenFactory
{
public:
  NS_IMETHOD CreateInstance(nsISupports *aOuter,
                            const nsIID &aIID,
                            void **aResult);
};

NS_IMETHODIMP
nsEventQueueServiceFactory::CreateInstance(nsISupports *aOuter,
                                           const nsIID &aIID,
                                           void **aResult)
{
  nsresult rv;
  nsISupports* inst;

  // Parameter validation...
  if (NULL == aResult) {
    rv = NS_ERROR_NULL_POINTER;
    goto done;
  }
  // Do not support aggregatable components...
  *aResult = NULL;
  if (NULL != aOuter) {
    rv = NS_ERROR_NO_AGGREGATION;
    goto done;
  }

  if (NULL == gServiceInstance) {
    // Create a new instance of the component...
    NS_NEWXPCOM(gServiceInstance, nsEventQueueServiceImpl);
    if (NULL == gServiceInstance) {
      rv = NS_ERROR_OUT_OF_MEMORY;
      goto done;
    }
    NS_ADDREF(gServiceInstance);
  }

  // If the QI fails, the component will be destroyed...
  //
  // Use a local copy so the NS_RELEASE() will not null the global
  // pointer...
  inst = gServiceInstance;

  NS_ADDREF(inst);
  rv = inst->QueryInterface(aIID, aResult);
  NS_RELEASE(inst);

done:
  return rv;
}


// Entry point to create nsEventQueueService factory instances...

nsresult NS_NewEventQueueServiceFactory(nsIFactory** aResult)
{
  nsresult rv = NS_OK;
  nsIFactory* inst = new nsEventQueueServiceFactory();
  if (NULL == inst) {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }
  else {
    NS_ADDREF(inst);
  }
  *aResult = inst;
  return rv;
}
