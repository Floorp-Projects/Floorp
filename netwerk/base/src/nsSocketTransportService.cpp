/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include <string.h>
#include "nsILoadGroup.h"
#include "nsSocketTransportService.h"
#include "nsSocketTransport.h"
#include "nsAutoLock.h"

#define MAX_OPEN_CONNECTIONS 50


nsSocketTransportService::nsSocketTransportService()
{
  NS_INIT_REFCNT();

  PR_INIT_CLIST(&mWorkQ);

  mThread      = nsnull;
#ifdef USE_POLLABLE_EVENT
  mThreadEvent = nsnull;
#endif /* USE_POLLABLE_EVENT */
  mThreadLock  = nsnull;

  mSelectFDSet      = nsnull;
  mSelectFDSetCount = 0;

  mActiveTransportList = nsnull;

  mThreadRunning = PR_FALSE;
}


nsSocketTransportService::~nsSocketTransportService()
{
  //
  // It is impossible for the nsSocketTransportService to be deleted while
  // the transport thread is running because it holds a reference to the
  // nsIRunnable (ie. the nsSocketTransportService instance)...
  //
  NS_ASSERTION(!mThread && !mThreadRunning, 
               "The socket transport thread is still running...");

  if (mSelectFDSet) {
    PR_Free(mSelectFDSet);
    mSelectFDSet = nsnull;
  }

  if (mActiveTransportList) {
    PR_Free(mActiveTransportList);
    mActiveTransportList = nsnull;
  }

#ifdef USE_POLLABLE_EVENT
  if (mThreadEvent) {
    PR_DestroyPollableEvent(mThreadEvent);
    mThreadEvent = nsnull;
  }
#endif /* USE_POLLABLE_EVENT */

  if (mThreadLock) {
    PR_DestroyLock(mThreadLock);
    mThreadLock = nsnull;
  }
}

NS_METHOD
nsSocketTransportService::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsSocketTransportService* trans = new nsSocketTransportService();
    if (trans == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(trans);
    nsresult rv = trans->Init();
    if (NS_SUCCEEDED(rv)) {
        rv = trans->QueryInterface(aIID, aResult);
    }
    NS_RELEASE(trans);
    return rv;
}

nsresult nsSocketTransportService::Init(void)
{
  nsresult rv = NS_OK;

  NS_ASSERTION(!mThread, "Socket transport thread has already been created!.");

  //
  // Create FDSET list used by PR_Poll(...)
  //
  if (!mSelectFDSet) {
    mSelectFDSet = (PRPollDesc*)PR_Malloc(sizeof(PRPollDesc)*MAX_OPEN_CONNECTIONS);
    if (mSelectFDSet) {
      memset(mSelectFDSet, 0, sizeof(PRPollDesc)*MAX_OPEN_CONNECTIONS);
    } else {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
  }

  //
  // Create the list of Active transport objects...  This list contains the 
  // nsSocketTransport corresponding to each PRFileDesc* in the mSelectFDSet
  //
  if (NS_SUCCEEDED(rv) && !mActiveTransportList) {
    mActiveTransportList = (nsSocketTransport**)PR_Malloc(sizeof(nsSocketTransport*)*MAX_OPEN_CONNECTIONS);
    if (mActiveTransportList) {
      memset(mActiveTransportList, 0, sizeof(nsSocketTransport*)*MAX_OPEN_CONNECTIONS);
    } else {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
  }

#ifdef USE_POLLABLE_EVENT
  //
  // Create the pollable event used to immediately wake up the transport 
  // thread when it is blocked in PR_Poll(...)
  //
  if (NS_SUCCEEDED(rv) && !mThreadEvent) {
    mThreadEvent = PR_NewPollableEvent();
    if (!mThreadEvent) {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
  }
#endif /* USE_POLLABLE_EVENT */

  //
  // Create the synchronization lock for the transport thread...
  //
  if (NS_SUCCEEDED(rv) && !mThreadLock) {
    mThreadLock = PR_NewLock();
    if (!mThreadLock) {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
  }

  //
  // Create the transport thread...
  //
  if (NS_SUCCEEDED(rv) && !mThread) {
    mThreadRunning = PR_TRUE;
    rv = NS_NewThread(&mThread, this);
  }

  return rv;
}


nsresult nsSocketTransportService::AddToWorkQ(nsSocketTransport* aTransport)
{
  PRStatus status;
  PRBool bFireEvent = PR_FALSE;
  nsresult rv = NS_OK;
  PRCList* qp;

  {
    nsAutoLock lock(mThreadLock);
    //
    // Only add the transport if it is *not* already on the list...
    //
    qp = aTransport->GetListNode();
    if (PR_CLIST_IS_EMPTY(qp)) {
      NS_ADDREF(aTransport);
      bFireEvent = PR_CLIST_IS_EMPTY(&mWorkQ);
      PR_APPEND_LINK(qp, &mWorkQ);
    }
  }
  //
  // Only fire an event if this is the first entry in the workQ.  Otherwise,
  // the event has already been fired and the transport thread will process
  // all of the entries at once...
  //
  if (bFireEvent) {
#ifdef USE_POLLABLE_EVENT
    status = PR_SetPollableEvent(mThreadEvent);
#else
  //
  // Need to break the socket transport thread out of the call to PR_Poll(...)
  // since a new transport needs to be processed...
  //
#endif /* USE_POLLABLE_EVENT */
    if (PR_FAILURE == status) {
      rv = NS_ERROR_FAILURE;
    }
  }
  return rv;
}


nsresult nsSocketTransportService::ProcessWorkQ(void)
{
  nsresult rv = NS_OK;
  PRCList* qp;

  //
  // Only process pending operations while there is space available in the
  // select list...
  //
  // XXX:  Need a way to restart the ProcessWorkQ(...) when space becomes
  //       available in the select set...
  //
  PR_Lock(mThreadLock);
  while (!PR_CLIST_IS_EMPTY(&mWorkQ) && 
         (MAX_OPEN_CONNECTIONS > mSelectFDSetCount)) {
    nsSocketTransport* transport;

    // Get the next item off of the workQ...
    qp = PR_LIST_HEAD(&mWorkQ);

    transport = nsSocketTransport::GetInstance(qp);
    PR_REMOVE_AND_INIT_LINK(qp);

    // Try to perform the operation...  
    //
    // Do not process the transport while holding the transport service 
    // lock...  A deadlock could occur if another thread is holding the 
    // transport lock and tries to add the transport to the service's WorkQ...
    //
    // Do not pass any select flags...
    PR_Unlock(mThreadLock);
    rv = transport->Process(0);
    PR_Lock(mThreadLock);
    //
    // If the operation would block, then add it to the select list for
    // later processing when the data arrives...
    //
    if (NS_BASE_STREAM_WOULD_BLOCK == rv) {
      rv = AddToSelectList(transport);
    }
    // Release the transport object (since it is no longer on the WorkQ).
    NS_RELEASE(transport);
  }
  PR_Unlock(mThreadLock);

  return rv;
}

nsresult nsSocketTransportService::AddToSelectList(nsSocketTransport* aTransport)
{
  nsresult rv = NS_OK;

  if (aTransport && (MAX_OPEN_CONNECTIONS > mSelectFDSetCount) ) {
    PRPollDesc* pfd;
    int i;
    //
    // Check to see if the transport is already in the list...
    //
    // If the first FD is the Pollable Event, it will be ignored since
    // its corresponding entry in the ActiveTransportList is nsnull.
    //
    for (i=0; i<mSelectFDSetCount; i++) {
      if (mActiveTransportList[i] == aTransport) {
        break;
      }
    }
    // Initialize/update the info in the entry...
    pfd = &mSelectFDSet[i];
    pfd->fd        = aTransport->GetSocket();;
    pfd->in_flags  = aTransport->GetSelectFlags();
    pfd->out_flags = 0;
    // Add the FileDesc to the PRPollDesc list...
    if (i == mSelectFDSetCount) {
      // Add the transport instance to the corresponding active transport list...
      NS_ADDREF(aTransport);
      mActiveTransportList[mSelectFDSetCount] = aTransport;
      mSelectFDSetCount += 1;
    }
  }

  return rv;
}


nsresult nsSocketTransportService::RemoveFromSelectList(nsSocketTransport* aTransport)
{
  int i;
  nsresult rv = NS_ERROR_FAILURE;

  if (!aTransport) return rv;

  //
  // Remove the transport from SelectFDSet and ActiveTransportList...
  //
  // If the first FD is the Pollable Event, it will be ignored since
  // its corresponding entry in the ActiveTransportList is nsnull.
  //
  for (i=0; i<mSelectFDSetCount; i++) {
    if (mActiveTransportList[i] == aTransport) {
      int last = mSelectFDSetCount-1;

      NS_RELEASE(mActiveTransportList[i]);

      // Move the last element in the array into the new empty slot...
      if (i != last) {
        memcpy(&mSelectFDSet[i], &mSelectFDSet[last], sizeof(mSelectFDSet[0]));
        mSelectFDSet[last].fd = nsnull;

        mActiveTransportList[i]    = mActiveTransportList[last];
        mActiveTransportList[last] = nsnull;
      } else {
        mSelectFDSet[i].fd      = nsnull;
        mActiveTransportList[i] = nsnull;
      }
      mSelectFDSetCount -= 1;
      rv = NS_OK;
      break;
    }
  }

  return rv;
}



//
// --------------------------------------------------------------------------
// nsISupports implementation...
// --------------------------------------------------------------------------
//
NS_IMPL_ADDREF(nsSocketTransportService);
NS_IMPL_RELEASE(nsSocketTransportService);

NS_IMETHODIMP
nsSocketTransportService::QueryInterface(const nsIID& aIID, void* *aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER; 
  } 
  if (aIID.Equals(NS_GET_IID(nsISocketTransportService)) ||
    aIID.Equals(NS_GET_IID(nsISupports))) {
    *aInstancePtr = NS_STATIC_CAST(nsISocketTransportService*, this); 
    NS_ADDREF_THIS(); 
    return NS_OK; 
  } 
  if (aIID.Equals(NS_GET_IID(nsIRunnable))) {
    *aInstancePtr = NS_STATIC_CAST(nsIRunnable*, this); 
    NS_ADDREF_THIS(); 
    return NS_OK; 
  } 
  return NS_NOINTERFACE; 
}


//
// --------------------------------------------------------------------------
// nsIRunnable implementation...
// --------------------------------------------------------------------------
//
NS_IMETHODIMP
nsSocketTransportService::Run(void)
{
  PRIntervalTime pollTimeout
    ;
#ifdef USE_POLLABLE_EVENT
  //
  // Initialize the FDSET used by PR_Poll(...).  The first item in the FDSet
  // is *always* the pollable event (ie. mThreadEvent).
  //
  mSelectFDSet[0].fd       = mThreadEvent;
  mSelectFDSet[0].in_flags = PR_POLL_READ;
  mSelectFDSetCount = 1;
  pollTimeout = PR_INTERVAL_NO_TIMEOUT;
#else
  //
  // For now, rather than breaking out of the call to PR_Poll(...) just set
  // the time out small enough...
  //
  // This means that new transports will only be processed once a timeout
  // occurs...
  //
  mSelectFDSetCount = 0;
  pollTimeout = PR_MillisecondsToInterval(250);
#endif /* USE_POLLABLE_EVENT */

  while (mThreadRunning) {
    nsresult rv;
    PRInt32 count;
    nsSocketTransport* transport;

    // XXX: PR_Poll(...) needs a timeout value...
    count = PR_Poll(mSelectFDSet, mSelectFDSetCount, pollTimeout);

    /* One or more sockets has data... */
    if (count > 0) {
      int i;

      /* Process any sockets with data first... */
      for (i=mSelectFDSetCount-1; i>=0; i--) {
        PRPollDesc* pfd;
        PRInt16 out_flags;

        pfd = &mSelectFDSet[i];
        if (pfd->out_flags) {
          // Clear the out_flags for next time...
          out_flags = pfd->out_flags;
          pfd->out_flags = 0;

          transport = mActiveTransportList[i];
          if (transport) {
            rv = transport->Process(out_flags);
            if (NS_BASE_STREAM_WOULD_BLOCK == rv) {
              // Update the select flags...
              pfd->in_flags = transport->GetSelectFlags();
            }
            //
            // If the operation completed, then remove the entry from the
            // select list...
            //
            else {
              rv = RemoveFromSelectList(transport);
            }
          }
          else {
#ifdef USE_POLLABLE_EVENT
            /* Process any pending operations on the mWorkQ... */
            NS_ASSERTION(0 == i, "Null transport in active list...");
            if (0 == i) {
              //
              // Clear the pollable event...  This call should *never* block since 
              // PR_Poll(...) said that it had been fired...
              //
              NS_ASSERTION(!(mSelectFDSet[0].out_flags & PR_POLL_EXCEPT), 
                           "Exception on Pollable event.");
              PR_WaitForPollableEvent(mThreadEvent);

              rv = ProcessWorkQ();
            }
#else
            //
            // The pollable event should be the *only* null transport
            // in the active transport list.
            //
            NS_ASSERTION(transport, "Null transport in active list...");
#endif /* USE_POLLABLE_EVENT */
          }
        }
      }
    } 
    /* PR_Poll(...) timeout... */
    else if (count == 0) {
    }
    /* PR_Poll(...) error.. */
    else {
    }

#ifndef USE_POLLABLE_EVENT
    /* Process any pending operations on the mWorkQ... */
    rv = ProcessWorkQ();
#endif /* !USE_POLLABLE_EVENT */
  }

  return NS_OK;
}


//
// --------------------------------------------------------------------------
// nsISocketTransportService implementation...
// --------------------------------------------------------------------------
//
NS_IMETHODIMP
nsSocketTransportService::CreateTransport(const char* aHost, 
                                          PRInt32 aPort,
                                          nsIChannel** aResult)
{
  return CreateTransport(aHost, aPort, nsnull, aResult);
}

NS_IMETHODIMP
nsSocketTransportService::CreateTransport(const char* aHost, 
                                          PRInt32 aPort,
                                          const char* aSocketType,
                                          nsIChannel** aResult)
{
  nsresult rv = NS_OK;
  nsSocketTransport* transport = nsnull;

  // Parameter validation...
  NS_ASSERTION(aResult, "aResult == nsnull.");
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }

  // Create and initialize a new connection object...
  NS_NEWXPCOM(transport, nsSocketTransport);
  if (transport) {
    rv = transport->Init(this, aHost, aPort, aSocketType);
    if (NS_FAILED(rv)) {
      delete transport;
      transport = nsnull;
    }
  } 
  else {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }

  // Set the reference count to one...
  if (NS_SUCCEEDED(rv)) {
    NS_ADDREF(transport);
  } 
  *aResult = transport;

  return rv;
}


NS_IMETHODIMP
nsSocketTransportService::Shutdown(void)
{
  PRStatus status;
  nsresult rv = NS_OK;

  if (mThread) {
    //
    // Clear the running flag and wake up the transport thread...
    //
    mThreadRunning = PR_FALSE;
#ifdef USE_POLLABLE_EVENT
    status = PR_SetPollableEvent(mThreadEvent);

    // XXX: what should happen if this fails?
    NS_ASSERTION(PR_SUCCESS == status, "Unable to wake up the transport thread.");
#else
    status = PR_SUCCESS;
#endif /* USE_POLLABLE_EVENT */
    
    // Wait for the transport thread to exit nsIRunnable::Run()
    if (PR_SUCCESS == status) {
      mThread->Join();
    } 

    NS_RELEASE(mThread);
  } else {
    rv = NS_ERROR_FAILURE;
  }

  return rv;
}
