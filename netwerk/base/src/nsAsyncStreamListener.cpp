/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

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

////////////////////////////////////////////////////////////////////////////////

class nsStreamListenerEvent
{
public:
    nsStreamListenerEvent(nsAsyncStreamObserver* listener,
                          nsIChannel* channel, nsISupports* context);
    virtual ~nsStreamListenerEvent();

    nsresult Fire(nsIEventQueue* aEventQ);

    NS_IMETHOD HandleEvent() = 0;

protected:
    static void PR_CALLBACK HandlePLEvent(PLEvent* aEvent);
    static void PR_CALLBACK DestroyPLEvent(PLEvent* aEvent);

    nsAsyncStreamObserver*      mListener;
    nsIChannel*                 mChannel;
    nsISupports*                mContext;
    PLEvent *                   mEvent;
};

////////////////////////////////////////////////////////////////////////////////

nsStreamListenerEvent::nsStreamListenerEvent(nsAsyncStreamObserver* listener,
                                             nsIChannel* channel, nsISupports* context)
    : mListener(listener), mChannel(channel), mContext(context), mEvent(nsnull)
{
    MOZ_COUNT_CTOR(nsStreamListenerEvent);

    NS_IF_ADDREF(mListener);
    NS_IF_ADDREF(mChannel);
    NS_IF_ADDREF(mContext);
}

nsStreamListenerEvent::~nsStreamListenerEvent()
{
    MOZ_COUNT_DTOR(nsStreamListenerEvent);

    NS_IF_RELEASE(mListener);
    NS_IF_RELEASE(mChannel);
    NS_IF_RELEASE(mContext);

    if (nsnull != mEvent)
    {
        delete mEvent;
        mEvent = nsnull;
    }
}

void PR_CALLBACK nsStreamListenerEvent::HandlePLEvent(PLEvent* aEvent)
{
    nsStreamListenerEvent * ev = 
        (nsStreamListenerEvent *) PL_GetEventOwner(aEvent);

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
        nsresult cancelRv = ev->mChannel->Cancel(rv);
        NS_ASSERTION(NS_SUCCEEDED(cancelRv), "Cancel failed");
    }
}

void PR_CALLBACK nsStreamListenerEvent::DestroyPLEvent(PLEvent* aEvent)
{
    nsStreamListenerEvent * ev = 
        (nsStreamListenerEvent *) PL_GetEventOwner(aEvent);

    NS_ASSERTION(nsnull != ev,"null event.");

    delete ev;
}

nsresult
nsStreamListenerEvent::Fire(nsIEventQueue* aEventQueue) 
{
    NS_PRECONDITION(nsnull != aEventQueue, "nsIEventQueue for thread is null");

    NS_PRECONDITION(nsnull == mEvent, "Init plevent only once.");
    
    mEvent = new PLEvent;
    
    PL_InitEvent(mEvent, 
                 this,
                 (PLHandleEventProc)  nsStreamListenerEvent::HandlePLEvent,
                 (PLDestroyEventProc) nsStreamListenerEvent::DestroyPLEvent);

    PRStatus status = aEventQueue->PostEvent(mEvent);
    return status == PR_SUCCESS ? NS_OK : NS_ERROR_FAILURE;
}

////////////////////////////////////////////////////////////////////////////////

NS_IMPL_THREADSAFE_ISUPPORTS2(nsAsyncStreamObserver,
                              nsIAsyncStreamObserver,
                              nsIStreamObserver)

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
nsAsyncStreamObserver::Init(nsIStreamObserver* aObserver, nsIEventQueue* aEventQ)
{
    nsresult rv = NS_OK;
    mReceiver = aObserver;
        
    NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueService, &rv);
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
                          nsIChannel* channel, nsISupports* context)
        : nsStreamListenerEvent(listener, channel, context) {}
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
  nsIStreamObserver* receiver = (nsIStreamObserver*)mListener->GetReceiver();
  nsresult status;
  nsresult rv = mChannel->GetStatus(&status);
  NS_ASSERTION(NS_SUCCEEDED(rv), "GetStatus failed");
  rv = receiver->OnStartRequest(mChannel, mContext);

  return rv;
}

NS_IMETHODIMP 
nsAsyncStreamObserver::OnStartRequest(nsIChannel* channel, nsISupports* context)
{
    nsresult rv;
    nsOnStartRequestEvent* event = 
        new nsOnStartRequestEvent(this, channel, context);
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
                         nsISupports* context, nsIChannel* channel)
        : nsStreamListenerEvent(listener, channel, context),
          mStatus(NS_OK) {}
    virtual ~nsOnStopRequestEvent();

    nsresult Init(nsresult aStatus, const PRUnichar* aStatusArg);
    NS_IMETHOD HandleEvent();

protected:
    nsresult    mStatus;
    nsString    mStatusArg;
};

nsOnStopRequestEvent::~nsOnStopRequestEvent()
{
}

nsresult
nsOnStopRequestEvent::Init(nsresult aStatus, const PRUnichar* aStatusArg)
{
    mStatus = aStatus;
    mStatusArg = aStatusArg;
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
    nsIStreamObserver* receiver = (nsIStreamObserver*)mListener->GetReceiver();
    nsresult status = NS_OK;
    nsresult rv = mChannel->GetStatus(&status);
    NS_ASSERTION(NS_SUCCEEDED(rv), "GetStatus failed");

    //
    // If the consumer returned a failure code, then pass it out in the
    // OnStopRequest(...) notification...
    //
    if (NS_SUCCEEDED(rv) && NS_FAILED(status)) {
        mStatus = status;
    }
    return receiver->OnStopRequest(mChannel, mContext, mStatus, mStatusArg.GetUnicode());
}

NS_IMETHODIMP 
nsAsyncStreamObserver::OnStopRequest(nsIChannel* channel, nsISupports* context,
                                     nsresult aStatus, const PRUnichar* aStatusArg)
{
    nsresult rv;

    //
    // Fire the OnStopRequest(...) regardless of what the current
    // Status is...
    //
    nsOnStopRequestEvent* event = 
        new nsOnStopRequestEvent(this, context, channel);
    if (event == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = event->Init(aStatus, aStatusArg);
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
                           nsIChannel* channel, nsISupports* context)
        : nsStreamListenerEvent(listener, channel, context),
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
  nsresult status;
  nsresult rv = mChannel->GetStatus(&status);
  NS_ASSERTION(NS_SUCCEEDED(rv), "GetStatus failed");

  //
  // Only send OnDataAvailable(... ) notifications if all previous calls
  // have succeeded...
  //
  if (NS_SUCCEEDED(rv) && NS_SUCCEEDED(status)) {
    rv = receiver->OnDataAvailable(mChannel, mContext,
                                   mIStream, mSourceOffset, mLength);
  }
  else {
	  NS_WARNING("not calling OnDataAvailable");
  }
  return rv;
}

NS_IMETHODIMP 
nsAsyncStreamListener::OnDataAvailable(nsIChannel* channel, nsISupports* context,
                                       nsIInputStream *aIStream, 
                                       PRUint32 aSourceOffset,
                                       PRUint32 aLength)
{
    nsresult rv;
    nsOnDataAvailableEvent* event = 
        new nsOnDataAvailableEvent(this, channel, context);
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
