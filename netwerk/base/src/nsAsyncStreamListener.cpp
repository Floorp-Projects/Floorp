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

#include "nsIStreamObserver.h"
#include "nsIStreamListener.h"
#include "nsIBufferInputStream.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsIEventQueue.h"
#include "nsIChannel.h"

class nsAsyncStreamObserver : public nsIStreamObserver
{
public:
    NS_DECL_ISUPPORTS

    // nsIStreamObserver methods:
    NS_IMETHOD OnStartRequest(nsIChannel* channel, 
                              nsISupports* context);
    NS_IMETHOD OnStopRequest(nsIChannel* channel,
                             nsISupports* context, 
                             nsresult aStatus,
                             const PRUnichar* aMsg);

    // nsAsyncStreamObserver methods:
    nsAsyncStreamObserver(nsIEventQueue* aEventQ) 
        : mReceiver(nsnull), mStatus(NS_OK) 
    { 
        NS_INIT_REFCNT();
        mEventQueue = aEventQ;
        NS_IF_ADDREF(mEventQueue);
    }
    
    virtual ~nsAsyncStreamObserver();

    void Init(nsIStreamObserver* aListener) {
        mReceiver = aListener;
        NS_ADDREF(mReceiver);
    }

    nsISupports* GetReceiver()      { return mReceiver; }
    nsresult GetStatus()            { return mStatus; }
    void SetStatus(nsresult value)  { mStatus = value; }

protected:
    nsIEventQueue*      mEventQueue;
    nsIStreamObserver*  mReceiver;
    nsresult            mStatus;
};


class nsAsyncStreamListener : public nsAsyncStreamObserver,
                              public nsIStreamListener
{
public:
    NS_DECL_ISUPPORTS_INHERITED

    // nsIStreamListener methods:
    NS_IMETHOD OnStartRequest(nsIChannel* channel,
                              nsISupports* context) 
    { 
        return nsAsyncStreamObserver::OnStartRequest(channel, context); 
    }

    NS_IMETHOD OnStopRequest(nsIChannel* channel,
                             nsISupports* context, 
                             nsresult aStatus,
                             const PRUnichar* aMsg) 
    { 
        return nsAsyncStreamObserver::OnStopRequest(channel, context, aStatus, aMsg); 
    }

    NS_IMETHOD OnDataAvailable(nsIChannel* channel, nsISupports* context,
                               nsIInputStream *aIStream, 
                               PRUint32 aSourceOffset,
                               PRUint32 aLength);

    // nsAsyncStreamListener methods:
    nsAsyncStreamListener(nsIEventQueue* aEventQ) 
        : nsAsyncStreamObserver(aEventQ) {}

    void Init(nsIStreamListener* aListener) {
        mReceiver = aListener;
        NS_ADDREF(mReceiver);
    }
};

////////////////////////////////////////////////////////////////////////////////

class nsStreamListenerEvent : public PLEvent 
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
};

////////////////////////////////////////////////////////////////////////////////

nsStreamListenerEvent::nsStreamListenerEvent(nsAsyncStreamObserver* listener,
                                             nsIChannel* channel, nsISupports* context)
    : mListener(listener), mChannel(channel), mContext(context)
{
    NS_ADDREF(mListener);
    NS_ADDREF(mChannel);
    NS_IF_ADDREF(mContext);
}

nsStreamListenerEvent::~nsStreamListenerEvent()
{
    NS_RELEASE(mListener);
    NS_RELEASE(mChannel);
    NS_IF_RELEASE(mContext);
}

void PR_CALLBACK nsStreamListenerEvent::HandlePLEvent(PLEvent* aEvent)
{
    // WARNING: This is a dangerous cast since it must adjust the pointer 
    // to compensate for the vtable...
    nsStreamListenerEvent *ev = (nsStreamListenerEvent*)aEvent;

    nsresult rv = ev->HandleEvent();
    ev->mListener->SetStatus(rv);
}

void PR_CALLBACK nsStreamListenerEvent::DestroyPLEvent(PLEvent* aEvent)
{
    // WARNING: This is a dangerous cast since it must adjust the pointer 
    // to compensate for the vtable...
    nsStreamListenerEvent *ev = (nsStreamListenerEvent*)aEvent;

    delete ev;
}

nsresult
nsStreamListenerEvent::Fire(nsIEventQueue* aEventQueue) 
{
    NS_PRECONDITION(nsnull != aEventQueue, "nsIEventQueue for thread is null");

    PL_InitEvent(this, nsnull,
                 (PLHandleEventProc)  nsStreamListenerEvent::HandlePLEvent,
                 (PLDestroyEventProc) nsStreamListenerEvent::DestroyPLEvent);

    PRStatus status = aEventQueue->PostEvent(this);
    return status == PR_SUCCESS ? NS_OK : NS_ERROR_FAILURE;
}

////////////////////////////////////////////////////////////////////////////////

nsAsyncStreamObserver::~nsAsyncStreamObserver()
{
  NS_RELEASE(mReceiver);
  NS_IF_RELEASE(mEventQueue);
}

NS_IMPL_THREADSAFE_ISUPPORTS(nsAsyncStreamObserver, 
                             nsCOMTypeInfo<nsIStreamObserver>::GetIID());

NS_IMPL_ISUPPORTS_INHERITED(nsAsyncStreamListener, 
                            nsAsyncStreamObserver, 
                            nsIStreamListener);

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
  nsIStreamObserver* receiver = (nsIStreamObserver*)mListener->GetReceiver();
  return receiver->OnStartRequest(mChannel, mContext);
}

NS_IMETHODIMP 
nsAsyncStreamObserver::OnStartRequest(nsIChannel* channel, nsISupports* context)
{
    nsresult rv = GetStatus();
    if (NS_FAILED(rv)) return rv;

    nsOnStartRequestEvent* event = 
        new nsOnStartRequestEvent(this, channel, context);
    if (event == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

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
          mStatus(NS_OK), mMessage(nsnull) {}
    virtual ~nsOnStopRequestEvent();

    nsresult Init(nsresult status, const PRUnichar* aMsg);
    NS_IMETHOD HandleEvent();

protected:
    nsresult    mStatus;
    PRUnichar*  mMessage;
};

nsOnStopRequestEvent::~nsOnStopRequestEvent()
{
}

nsresult
nsOnStopRequestEvent::Init(nsresult status, const PRUnichar* aMsg)
{
    mStatus = status;
    mMessage = (PRUnichar*)aMsg;
    return NS_OK;
}

NS_IMETHODIMP
nsOnStopRequestEvent::HandleEvent()
{
  nsIStreamObserver* receiver = (nsIStreamObserver*)mListener->GetReceiver();
  return receiver->OnStopRequest(mChannel, mContext, mStatus, mMessage);
}

NS_IMETHODIMP 
nsAsyncStreamObserver::OnStopRequest(nsIChannel* channel, nsISupports* context,
                                     nsresult aStatus,
                                     const PRUnichar* aMsg)
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

    rv = event->Init(aStatus, aMsg);
    if (NS_FAILED(rv)) goto failed;
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
  nsIStreamListener* receiver = (nsIStreamListener*)mListener->GetReceiver();
  return receiver->OnDataAvailable(mChannel, mContext,
                                   mIStream, mSourceOffset, mLength);
}

NS_IMETHODIMP 
nsAsyncStreamListener::OnDataAvailable(nsIChannel* channel, nsISupports* context,
                                       nsIInputStream *aIStream, 
                                       PRUint32 aSourceOffset,
                                       PRUint32 aLength)
{
    nsresult rv = GetStatus();
    if (NS_FAILED(rv)) return rv;

    nsOnDataAvailableEvent* event = 
        new nsOnDataAvailableEvent(this, channel, context);
    if (event == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = event->Init(aIStream, aSourceOffset, aLength);
    if (NS_FAILED(rv)) goto failed;
    rv = event->Fire(mEventQueue);
    if (NS_FAILED(rv)) goto failed;
    return rv;

  failed:
    delete event;
    return rv;
}

////////////////////////////////////////////////////////////////////////////////

NS_NET nsresult
NS_NewAsyncStreamObserver(nsIStreamObserver* *result,
                          nsIEventQueue* eventQueue,
                          nsIStreamObserver* receiver)
{
    nsAsyncStreamObserver* l =
        new nsAsyncStreamObserver(eventQueue);
    if (l == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    l->Init(receiver);
    NS_ADDREF(l);
    *result = l;
    return NS_OK;
}

NS_NET nsresult
NS_NewAsyncStreamListener(nsIStreamListener* *result,
                          nsIEventQueue* eventQueue,
                          nsIStreamListener* receiver)
{
    nsAsyncStreamListener* l =
        new nsAsyncStreamListener(eventQueue);
    if (l == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    l->Init(receiver);
    NS_ADDREF(l);
    *result = l;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
