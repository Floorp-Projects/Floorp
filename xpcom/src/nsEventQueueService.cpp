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
#include "nsIEventQueueService.h"
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
  ~EventQueueEntry();

  // nsISupports interface...
  NS_DECL_ISUPPORTS

  PLEventQueue* GetEventQueue(void);

private: 
  PLEventQueue* mEventQueue; 
};

/* nsISupports interface implementation... */
NS_IMPL_ISUPPORTS(EventQueueEntry,kISupportsIID);

EventQueueEntry::EventQueueEntry()
{
  NS_INIT_REFCNT();

  mEventQueue = PL_CreateNativeEventQueue("Thread event queue...", PR_GetCurrentThread());
}

EventQueueEntry::~EventQueueEntry()
{
  if (NULL != mEventQueue) {
    PL_DestroyEventQueue(mEventQueue);
  }
}

PLEventQueue* EventQueueEntry::GetEventQueue(void)
{
  return mEventQueue;
}



class nsEventQueueServiceImpl : public nsIEventQueueService
{
public:
  nsEventQueueServiceImpl();

  // nsISupports interface...
  NS_DECL_ISUPPORTS

  // nsIEventQueueService interface...
  NS_IMETHOD CreateThreadEventQueue(void);
  NS_IMETHOD DestroyThreadEventQueue(void);
  NS_IMETHOD GetThreadEventQueue(PRThread* aThread, PLEventQueue** aResult);

protected:
  ~nsEventQueueServiceImpl();

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
nsEventQueueServiceImpl::GetThreadEventQueue(PRThread* aThread, PLEventQueue** aResult)
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
    *aResult = evQueueEntry->GetEventQueue();
  } else {
    // XXX: Need error code for requesting an event queue when none exists...
    rv = NS_ERROR_FAILURE;
    goto done;
  }

done:
  // Release the EventQ lock...
  PR_ExitMonitor(mEventQMonitor);
  return rv;
}

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
