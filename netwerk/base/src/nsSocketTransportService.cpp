/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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


#include "nsSocketTransportService.h"
#include "nsSocketTransport.h"


#define MAX_OPEN_CONNECTIONS 10


static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);


nsSocketTransportService::nsSocketTransportService()
{
  NS_INIT_REFCNT();

  PR_INIT_CLIST(&mWorkQ);

  mThread      = nsnull;
  mThreadEvent = nsnull;
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

  if (mThreadEvent) {
    PR_DestroyPollableEvent(mThreadEvent);
    mThreadEvent = nsnull;
  }

  if (mThreadLock) {
    PR_DestroyLock(mThreadLock);
    mThreadLock = nsnull;
  }
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
  PRBool bFireEvent;
  nsresult rv = NS_OK;

  NS_ADDREF(aTransport);
  Lock();
  bFireEvent = PR_CLIST_IS_EMPTY(&mWorkQ);
  PR_APPEND_LINK(aTransport, &mWorkQ);
  Unlock();

  //
  // Only fire an event if this is the first entry in the workQ.  Otherwise,
  // the event has already been fired and the transport thread will process
  // all of the entries at once...
  //
  if (bFireEvent) {
    status = PR_SetPollableEvent(mThreadEvent);
    if (PR_FAILURE == status) {
      rv = NS_ERROR_FAILURE;
    }
  }
  return rv;
}


nsresult nsSocketTransportService::ProcessWorkQ(void)
{
  nsresult rv = NS_OK;

  //
  // Only process pending operations while there is space available in the
  // select list...
  //
  // XXX:  Need a way to restart the ProcessWorkQ(...) when space becomes
  //       available in the select set...
  //
  Lock();
  while (!PR_CLIST_IS_EMPTY(&mWorkQ) && 
         (MAX_OPEN_CONNECTIONS > mSelectFDSetCount)) {
    nsSocketTransport* transport;

    // Get the next item off of the workQ...
    transport = (nsSocketTransport*)PR_LIST_HEAD(&mWorkQ);
    PR_REMOVE_AND_INIT_LINK(transport);

    // Try to perform the operation...
    //
    // Do not pass any select flags...
    rv = transport->Process(0);
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
  Unlock();

  return rv;
}

nsresult nsSocketTransportService::AddToSelectList(nsSocketTransport* aTransport)
{
  nsresult rv = NS_OK;

  if (aTransport && (MAX_OPEN_CONNECTIONS > mSelectFDSetCount) ) {
    PRPollDesc* pfd;
    // Add the FileDesc to the PRPollDesc list...
    pfd = &mSelectFDSet[mSelectFDSetCount];
    pfd->fd        = aTransport->GetSocket();;
    pfd->in_flags  = aTransport->GetSelectFlags();
    pfd->out_flags = 0;
    // Add the transport instance to the corresponding active transport list...
    NS_ADDREF(aTransport);
    mActiveTransportList[mSelectFDSetCount] = aTransport;
    mSelectFDSetCount += 1;
  }
  else {
    rv = NS_ERROR_FAILURE;
  }

  return rv;
}


nsresult nsSocketTransportService::RemoveFromSelectList(nsSocketTransport* aTransport)
{
  int i;
  nsresult rv = NS_ERROR_FAILURE;

  for (i=1; i<mSelectFDSetCount; i++) {
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
  if (aIID.Equals(nsISocketTransportService::GetIID()) ||
    aIID.Equals(kISupportsIID)) {
    *aInstancePtr = NS_STATIC_CAST(nsISocketTransportService*, this); 
    NS_ADDREF_THIS(); 
    return NS_OK; 
  } 
  if (aIID.Equals(nsIRunnable::GetIID())) {
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
  //
  // Initialize the FDSET used by PR_Poll(...).  The first item in the FDSet
  // is *always* the pollable event (ie. mThreadEvent).
  //
  mSelectFDSet[0].fd       = mThreadEvent;
  mSelectFDSet[0].in_flags = PR_POLL_READ;
  mSelectFDSetCount = 1;

  while (mThreadRunning) {
    PRInt32 count;
    nsSocketTransport* transport;

    // XXX: PR_Poll(...) needs a timeout value...
    count = PR_Poll(mSelectFDSet, mSelectFDSetCount, PR_INTERVAL_NO_TIMEOUT);

    /* One or more sockets has data... */
    if (count > 0) {
      nsresult rv;
      int i;

      /* Process any sockets with data first... */
      for (i=mSelectFDSetCount-1; i>=1; i--) {
        PRPollDesc* pfd;
        PRInt16 out_flags;

        pfd = &mSelectFDSet[i];
        if (pfd->out_flags) {
          // Clear the out_flags for next time...
          out_flags = pfd->out_flags;
          pfd->out_flags = 0;

          transport = mActiveTransportList[i];
          NS_ASSERTION(transport, "Null transport in active list...");
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
        }
      }
      
      /* Process any pending operations on the mWorkQ... */
      if (mSelectFDSet[0].out_flags) {
        //
        // Clear the pollable event...  This call should *never* block since 
        // PR_Poll(...) said that it had been fired...
        //
        NS_ASSERTION(!(mSelectFDSet[0].out_flags & PR_POLL_EXCEPT), 
                     "Exception on Pollable event.");
        PR_WaitForPollableEvent(mThreadEvent);

        rv = ProcessWorkQ();
      }
    } 
    /* PR_Poll(...) timeout... */
    else if (count == 0) {
    }
    /* PR_Poll(...) error.. */
    else {
    }
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
                                          nsITransport** aResult)
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
    rv = transport->Init(this, aHost, aPort);
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
    status = PR_SetPollableEvent(mThreadEvent);

    // XXX: what should happen if this fails?
    NS_ASSERTION(PR_SUCCESS == status, "Unable to wake up the transport thread.");
    
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