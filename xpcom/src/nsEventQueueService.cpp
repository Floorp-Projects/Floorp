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



// XXX move to nsID.h or nsHashtable.h? (copied from nsRepository.cpp)
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
  mEventQTable   = new nsHashtable(16);
  mEventQMonitor = PR_NewMonitor();
}

nsEventQueueServiceImpl::~nsEventQueueServiceImpl()
{
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
  PLEventQueue* evQueue;

  /* Enter the lock which protects the EventQ hashtable... */
  PR_EnterMonitor(mEventQMonitor);

  /* Only create one event queue per thread... */
  if (NULL != mEventQTable->Get(&key)) {
  // XXX: Need error code for creating an event queue more than once...
    rv = NS_ERROR_FAILURE;
    goto done;
  }
  
  evQueue = PL_CreateEventQueue("Thread event queue...", PR_GetCurrentThread());
  if (NULL == evQueue) {
    rv = NS_ERROR_OUT_OF_MEMORY;
    goto done;
  }

  mEventQTable->Put(&key, evQueue);

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
  PLEventQueue* evQueue;

  /* Enter the lock which protects the EventQ hashtable... */
  PR_EnterMonitor(mEventQMonitor);

  evQueue = (PLEventQueue*)mEventQTable->Remove(&key);
  if (NULL != evQueue) {
    PL_DestroyEventQueue(evQueue);
  } else {
  // XXX: Need error code for destroying an event queue more than once...
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
  PLEventQueue* evQueue;
  ThreadKey key(aThread);

  /* Enter the lock which protects the EventQ hashtable... */
  PR_EnterMonitor(mEventQMonitor);

  /* Parameter validation... */
  if ((NULL == aThread) || (NULL == aResult)) {
    rv = NS_ERROR_NULL_POINTER;
    goto done;
  }

  evQueue = (PLEventQueue*)mEventQTable->Get(&key);
  if (NULL != evQueue) {
    *aResult = evQueue;
    // XXX: For now only use the main eventQ...
    *aResult = PL_GetMainEventQueue();
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

// Entry point to create nsEventQueueService factory instances...

nsresult NS_NewEventQueueServiceFactory(nsIFactory** aResult)
{
  nsresult rv = NS_OK;
  nsIFactory* inst = new nsFactory<nsEventQueueServiceImpl>();
  if (NULL == inst) {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }
  else {
    NS_ADDREF(inst);
  }
  *aResult = inst;
  return rv;
}
