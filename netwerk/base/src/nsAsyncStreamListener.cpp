/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
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

#include "nsAsyncStreamListener.h"
#include "nsIInputStream.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsIEventQueueService.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsIChannel.h"
#include "prlog.h"

static NS_DEFINE_CID(kEventQueueService, NS_EVENTQUEUESERVICE_CID);

#if defined(PR_LOGGING)
PRLogModuleInfo* gStreamEventLog = 0;
#endif

// prevent name conflicts
#define nsStreamListenerEvent  nsStreamListenerEvent0
#define nsOnStartRequestEvent  nsOnStartRequestEvent0
#define nsOnStopRequestEvent   nsOnStopRequestEvent0
#define nsOnDataAvailableEvent nsOnDataAvailableEvent0

////////////////////////////////////////////////////////////////////////////////

class nsStreamListenerEvent
{
public:
    nsStreamListenerEvent(nsAsyncStreamObserver* listener,
                          nsIRequest* request, nsISupports* context);
    virtual ~nsStreamListenerEvent();

    nsresult Fire(nsIEventQueue* aEventQ);

    NS_IMETHOD HandleEvent() = 0;

protected:
    static void PR_CALLBACK HandlePLEvent(PLEvent* aEvent);
    static void PR_CALLBACK DestroyPLEvent(PLEvent* aEvent);

    nsAsyncStreamObserver*      mListener;
    nsIRequest*                 mRequest;
    nsISupports*                mContext;
    PLEvent                     mEvent;
};

#define GET_STREAM_LISTENER_EVENT(_this) \
    ((nsStreamListenerEvent*)((char*)(_this) - offsetof(nsStreamListenerEvent, mEvent)))

////////////////////////////////////////////////////////////////////////////////

nsStreamListenerEvent::nsStreamListenerEvent(nsAsyncStreamObserver* listener,
                                             nsIRequest* request, nsISupports* context)
    : mListener(listener), mRequest(request), mContext(context)
{
    MOZ_COUNT_CTOR(nsStreamListenerEvent);

    NS_IF_ADDREF(mListener);
    NS_IF_ADDREF(mRequest);
    NS_IF_ADDREF(mContext);
}

nsStreamListenerEvent::~nsStreamListenerEvent()
{
    MOZ_COUNT_DTOR(nsStreamListenerEvent);

    NS_IF_RELEASE(mListener);
    NS_IF_RELEASE(mRequest);
    NS_IF_RELEASE(mContext);
}

void PR_CALLBACK nsStreamListenerEvent::HandlePLEvent(PLEvent* aEvent)
{
    nsStreamListenerEvent* ev = GET_STREAM_LISTENER_EVENT(aEvent);
    NS_ASSERTION(nsnull != ev,"null event.");

    nsresult rv = ev->HandleEvent();
    //
    // If the consumer fails, then cancel the transport.  This is necessary
    // in case where the socket transport is blocked waiting for room in the
    // pipe, but the consumer fails without consuming all the data.
    //
    // Unless the transport is cancelled, it will block forever, waiting for
    // the pipe to empty...
    //
    if (NS_FAILED(rv)) {
        nsresult cancelRv = ev->mRequest->Cancel(rv);
        NS_ASSERTION(NS_SUCCEEDED(cancelRv), "Cancel failed");
    }
}

void PR_CALLBACK nsStreamListenerEvent::DestroyPLEvent(PLEvent* aEvent)
{
    nsStreamListenerEvent* ev = GET_STREAM_LISTENER_EVENT(aEvent);
    NS_ASSERTION(nsnull != ev, "null event.");
    delete ev;
}

nsresult
nsStreamListenerEvent::Fire(nsIEventQueue* aEventQueue) 
{
    NS_PRECONDITION(nsnull != aEventQueue, "nsIEventQueue for thread is null");

    PL_InitEvent(&mEvent,
                 nsnull,
                 (PLHandleEventProc)  nsStreamListenerEvent::HandlePLEvent,
                 (PLDestroyEventProc) nsStreamListenerEvent::DestroyPLEvent);

    PRStatus status = aEventQueue->PostEvent(&mEvent);
    return status == PR_SUCCESS ? NS_OK : NS_ERROR_FAILURE;
}

////////////////////////////////////////////////////////////////////////////////

NS_IMPL_THREADSAFE_ISUPPORTS1(nsAsyncStreamObserver,
                              nsIRequestObserver)

NS_IMPL_ADDREF_INHERITED(nsAsyncStreamListener, nsAsyncStreamObserver);
NS_IMPL_RELEASE_INHERITED(nsAsyncStreamListener, nsAsyncStreamObserver);

NS_IMETHODIMP 
nsAsyncStreamListener::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (!aInstancePtr) return NS_ERROR_NULL_POINTER;
  if (aIID.Equals(NS_GET_IID(nsIAsyncStreamListener))) {
    *aInstancePtr = NS_STATIC_CAST(nsIAsyncStreamListener*, this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIStreamListener))) {
    *aInstancePtr = NS_STATIC_CAST(nsIStreamListener*, this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return nsAsyncStreamObserver::QueryInterface(aIID, aInstancePtr);
}

NS_IMETHODIMP
nsAsyncStreamObserver::Init(nsIRequestObserver* aObserver, nsIEventQueue* aEventQ)
{
    nsresult rv = NS_OK;
    NS_ASSERTION(aObserver, "null observer");
    mReceiver = aObserver;
        
    nsCOMPtr<nsIEventQueueService> eventQService = 
             do_GetService(kEventQueueService, &rv);
    if (NS_FAILED(rv)) 
    return rv;
        
    rv = eventQService->ResolveEventQueue(aEventQ, getter_AddRefs(mEventQueue));
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
//
// OnStartRequest...
//
////////////////////////////////////////////////////////////////////////////////

class nsOnStartRequestEvent : public nsStreamListenerEvent
{
public:
    nsOnStartRequestEvent(nsAsyncStreamObserver* listener, 
                          nsIRequest* request, nsISupports* context)
        : nsStreamListenerEvent(listener, request, context) {}
    virtual ~nsOnStartRequestEvent() {}

    NS_IMETHOD HandleEvent();
};

NS_IMETHODIMP
nsOnStartRequestEvent::HandleEvent()
{
#if defined(PR_LOGGING)
  if (!gStreamEventLog)
      gStreamEventLog = PR_NewLogModule("netlibStreamEvent");
  PR_LOG(gStreamEventLog, PR_LOG_DEBUG,
         ("netlibEvent: Handle Start [event=%x]", this));
#endif
  nsIRequestObserver* receiver = (nsIRequestObserver*)mListener->GetReceiver();
  if (receiver == nsnull) {
      // must have already called OnStopRequest (it clears the receiver)
      return NS_ERROR_FAILURE;
  }

  nsresult status;
  nsresult rv = mRequest->GetStatus(&status);
  NS_ASSERTION(NS_SUCCEEDED(rv), "GetStatus failed");
  rv = receiver->OnStartRequest(mRequest, mContext);

  return rv;
}

NS_IMETHODIMP 
nsAsyncStreamObserver::OnStartRequest(nsIRequest *request, nsISupports* context)
{
    nsresult rv;
    nsOnStartRequestEvent* event = 
        new nsOnStartRequestEvent(this, request, context);
    if (event == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

#if defined(PR_LOGGING)
    PLEventQueue *equeue;
    mEventQueue->GetPLEventQueue(&equeue);
    char ts[80];
    sprintf(ts, "nsAsyncStreamObserver: Start [this=%lx queue=%lx",
            (long)this, (long)equeue);
    if (!gStreamEventLog)
      gStreamEventLog = PR_NewLogModule("netlibStreamEvent");
    PR_LOG(gStreamEventLog, PR_LOG_DEBUG,
           ("nsAsyncStreamObserver: Start [this=%x queue=%x event=%x]",
            this, equeue, event));
#endif
    rv = event->Fire(mEventQueue);
    if (NS_FAILED(rv)) goto failed;
    return rv;

  failed:
    delete event;
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
//
// OnStopRequest
//
////////////////////////////////////////////////////////////////////////////////

class nsOnStopRequestEvent : public nsStreamListenerEvent
{
public:
    nsOnStopRequestEvent(nsAsyncStreamObserver* listener, 
                         nsISupports* context, nsIRequest* request)
        : nsStreamListenerEvent(listener, request, context),
          mStatus(NS_OK) {}
    virtual ~nsOnStopRequestEvent();

    nsresult Init(nsresult aStatus);
    NS_IMETHOD HandleEvent();

protected:
    nsresult    mStatus;
};

nsOnStopRequestEvent::~nsOnStopRequestEvent()
{
}

nsresult
nsOnStopRequestEvent::Init(nsresult aStatus)
{
    mStatus = aStatus;
    return NS_OK;
}

NS_IMETHODIMP
nsOnStopRequestEvent::HandleEvent()
{
#if defined(PR_LOGGING)
    if (!gStreamEventLog)
        gStreamEventLog = PR_NewLogModule("netlibStreamEvent");
    PR_LOG(gStreamEventLog, PR_LOG_DEBUG,
           ("netlibEvent: Handle Stop [event=%x]", this));
#endif
    nsIRequestObserver* receiver = (nsIRequestObserver*)mListener->GetReceiver();
    if (receiver == nsnull) {
        // must have already called OnStopRequest (it clears the receiver)
        return NS_ERROR_FAILURE;
    }

    nsresult status = NS_OK;
    nsresult rv = mRequest->GetStatus(&status);
    NS_ASSERTION(NS_SUCCEEDED(rv), "GetStatus failed");

    //
    // If the consumer returned a failure code, then pass it out in the
    // OnStopRequest(...) notification...
    //
    if (NS_SUCCEEDED(rv) && NS_FAILED(status)) {
        mStatus = status;
    }
    rv = receiver->OnStopRequest(mRequest, mContext, mStatus);
    // Call clear on the listener to make sure it's cleanup is done on the correct thread
    mListener->Clear();
    return rv;
}

NS_IMETHODIMP 
nsAsyncStreamObserver::OnStopRequest(nsIRequest* request, nsISupports* context,
                                     nsresult aStatus)
{
    nsresult rv;

    //
    // Fire the OnStopRequest(...) regardless of what the current
    // Status is...
    //
    nsOnStopRequestEvent* event = 
        new nsOnStopRequestEvent(this, context, request);
    if (event == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = event->Init(aStatus);
    if (NS_FAILED(rv)) goto failed;
#if defined(PR_LOGGING)
    PLEventQueue *equeue;
    mEventQueue->GetPLEventQueue(&equeue);
    if (!gStreamEventLog)
      gStreamEventLog = PR_NewLogModule("netlibStreamEvent");
    PR_LOG(gStreamEventLog, PR_LOG_DEBUG,
           ("nsAsyncStreamObserver: Stop [this=%x queue=%x event=%x]",
            this, equeue, event));
#endif
    rv = event->Fire(mEventQueue);
    if (NS_FAILED(rv)) goto failed;
    return rv;

  failed:
    delete event;
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
//
// OnDataAvailable
//
////////////////////////////////////////////////////////////////////////////////

class nsOnDataAvailableEvent : public nsStreamListenerEvent
{
public:
    nsOnDataAvailableEvent(nsAsyncStreamObserver* listener, 
                           nsIRequest* request, nsISupports* context)
        : nsStreamListenerEvent(listener, request, context),
          mIStream(nsnull), mLength(0) {}
    virtual ~nsOnDataAvailableEvent();

    nsresult Init(nsIInputStream* aIStream, PRUint32 aSourceOffset,
                  PRUint32 aLength);
    NS_IMETHOD HandleEvent();

protected:
    nsIInputStream*       mIStream;
    PRUint32                    mSourceOffset;
    PRUint32                    mLength;
};

nsOnDataAvailableEvent::~nsOnDataAvailableEvent()
{
    NS_RELEASE(mIStream);
}

nsresult
nsOnDataAvailableEvent::Init(nsIInputStream* aIStream, PRUint32 aSourceOffset,
                             PRUint32 aLength)
{
    mSourceOffset = aSourceOffset;
    mLength = aLength;
    mIStream = aIStream;
    NS_ADDREF(mIStream);
    return NS_OK;
}

NS_IMETHODIMP
nsOnDataAvailableEvent::HandleEvent()
{
#if defined(PR_LOGGING)
  if (!gStreamEventLog)
    gStreamEventLog = PR_NewLogModule("netlibStreamEvent");
  PR_LOG(gStreamEventLog, PR_LOG_DEBUG,
         ("netlibEvent: Handle Data [event=%x]", this));
#endif
  nsIStreamListener* receiver = (nsIStreamListener*)mListener->GetReceiver();
  if (receiver == nsnull) {
      // must have already called OnStopRequest (it clears the receiver)
      return NS_ERROR_FAILURE;
  }

  nsresult status;
  nsresult rv = mRequest->GetStatus(&status);
  NS_ASSERTION(NS_SUCCEEDED(rv), "GetStatus failed");

  //
  // Only send OnDataAvailable(... ) notifications if all previous calls
  // have succeeded...
  //
  if (NS_SUCCEEDED(rv) && NS_SUCCEEDED(status)) {
    rv = receiver->OnDataAvailable(mRequest, mContext,
                                   mIStream, mSourceOffset, mLength);
  }
  else {
	  NS_WARNING("not calling OnDataAvailable");
  }
  return rv;
}

NS_IMETHODIMP 
nsAsyncStreamListener::OnDataAvailable(nsIRequest* request, nsISupports* context,
                                       nsIInputStream *aIStream, 
                                       PRUint32 aSourceOffset,
                                       PRUint32 aLength)
{
    nsresult rv;
    nsOnDataAvailableEvent* event = 
        new nsOnDataAvailableEvent(this, request, context);
    if (event == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = event->Init(aIStream, aSourceOffset, aLength);
    if (NS_FAILED(rv)) goto failed;
#if defined(PR_LOGGING)
    PLEventQueue *equeue;
    mEventQueue->GetPLEventQueue(&equeue);
    if (!gStreamEventLog)
      gStreamEventLog = PR_NewLogModule("netlibStreamEvent");
    PR_LOG(gStreamEventLog, PR_LOG_DEBUG,
           ("nsAsyncStreamObserver: Data [this=%x queue=%x event=%x]",
            this, equeue, event));
#endif
    rv = event->Fire(mEventQueue);
    if (NS_FAILED(rv)) goto failed;
    return rv;

  failed:
    delete event;
    return rv;
}

////////////////////////////////////////////////////////////////////////////////

NS_METHOD
nsAsyncStreamObserver::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;
    nsAsyncStreamObserver* l = new nsAsyncStreamObserver();
    if (l == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(l);
    nsresult rv = l->QueryInterface(aIID, aResult);
    NS_RELEASE(l);
    return rv;
}

NS_METHOD
nsAsyncStreamListener::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;
    nsAsyncStreamListener* l = new nsAsyncStreamListener();
    if (l == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(l);
    nsresult rv = l->QueryInterface(aIID, aResult);
    NS_RELEASE(l);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
