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

class nsAsyncStreamObserver : public nsIStreamObserver
{
public:
    NS_DECL_ISUPPORTS

    // nsIStreamObserver methods:
    NS_IMETHOD OnStartBinding(nsISupports* context);
    NS_IMETHOD OnStopBinding(nsISupports* context,
                             nsresult aStatus,
                             const PRUnichar* aMsg);
    NS_IMETHOD OnStartRequest(nsISupports* context);
    NS_IMETHOD OnStopRequest(nsISupports* context,
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
    NS_IMETHOD OnStartBinding(nsISupports* context) 
    { 
      return nsAsyncStreamObserver::OnStartBinding(context); 
    }

    NS_IMETHOD OnStopBinding(nsISupports* context,
                             nsresult aStatus,
                             const PRUnichar* aMsg) 
    { 
      return nsAsyncStreamObserver::OnStopBinding(context, aStatus, aMsg); 
    }

    NS_IMETHOD OnStartRequest(nsISupports* context) 
    { 
      return nsAsyncStreamObserver::OnStartRequest(context); 
    }

    NS_IMETHOD OnStopRequest(nsISupports* context,
                             nsresult aStatus,
                             const PRUnichar* aMsg) 
    { 
      return nsAsyncStreamObserver::OnStopRequest(context, aStatus, aMsg); 
    }

    NS_IMETHOD OnDataAvailable(nsISupports* context,
                               nsIBufferInputStream *aIStream, 
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
                          nsISupports* context);
    virtual ~nsStreamListenerEvent();

    nsresult Fire(nsIEventQueue* aEventQ);

    NS_IMETHOD HandleEvent() = 0;

protected:
    static void PR_CALLBACK HandlePLEvent(PLEvent* aEvent);
    static void PR_CALLBACK DestroyPLEvent(PLEvent* aEvent);

    nsAsyncStreamObserver* mListener;
    nsISupports*                mContext;
};

////////////////////////////////////////////////////////////////////////////////

nsStreamListenerEvent::nsStreamListenerEvent(nsAsyncStreamObserver* listener,
                                             nsISupports* context)
    : mListener(listener), mContext(context)
{
    NS_ADDREF(mListener);
    NS_IF_ADDREF(mContext);
}

nsStreamListenerEvent::~nsStreamListenerEvent()
{
    NS_RELEASE(mListener);
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

NS_IMPL_ISUPPORTS(nsAsyncStreamObserver, nsIStreamObserver::GetIID());

NS_IMPL_ISUPPORTS_INHERITED(nsAsyncStreamListener, 
                            nsAsyncStreamObserver, 
                            nsIStreamListener);

////////////////////////////////////////////////////////////////////////////////
//
// OnStartBinding...
//
////////////////////////////////////////////////////////////////////////////////

class nsOnStartBindingEvent : public nsStreamListenerEvent
{
public:
    nsOnStartBindingEvent(nsAsyncStreamObserver* listener, 
                          nsISupports* context)
        : nsStreamListenerEvent(listener, context), mContentType(nsnull) {}
    virtual ~nsOnStartBindingEvent();

    NS_IMETHOD HandleEvent();

protected:
    char*       mContentType;
};

nsOnStartBindingEvent::~nsOnStartBindingEvent()
{
    if (mContentType)
        delete[] mContentType;
}

NS_IMETHODIMP
nsOnStartBindingEvent::HandleEvent()
{
  nsIStreamObserver* receiver = (nsIStreamObserver*)mListener->GetReceiver();
  return receiver->OnStartBinding(mContext);
}

NS_IMETHODIMP 
nsAsyncStreamObserver::OnStartBinding(nsISupports* context)
{
    nsresult rv = GetStatus();
    if (NS_FAILED(rv)) return rv;

    nsOnStartBindingEvent* event = 
        new nsOnStartBindingEvent(this, context);
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
// OnStopBinding
//
////////////////////////////////////////////////////////////////////////////////

class nsOnStopBindingEvent : public nsStreamListenerEvent
{
public:
    nsOnStopBindingEvent(nsAsyncStreamObserver* listener, 
                         nsISupports* context)
        : nsStreamListenerEvent(listener, context),
          mStatus(NS_OK), mMessage(nsnull) {}
    virtual ~nsOnStopBindingEvent();

    nsresult Init(nsresult status, const PRUnichar* aMsg);
    NS_IMETHOD HandleEvent();

protected:
    nsresult    mStatus;
    PRUnichar*  mMessage;
};

nsOnStopBindingEvent::~nsOnStopBindingEvent()
{
}

nsresult
nsOnStopBindingEvent::Init(nsresult status, const PRUnichar* aMsg)
{
    mStatus = status;
    mMessage = (PRUnichar*)aMsg;
    return NS_OK;
}

NS_IMETHODIMP
nsOnStopBindingEvent::HandleEvent()
{
  nsIStreamObserver* receiver = (nsIStreamObserver*)mListener->GetReceiver();
  return receiver->OnStopBinding(mContext, mStatus, mMessage);
}

NS_IMETHODIMP 
nsAsyncStreamObserver::OnStopBinding(nsISupports* context,
                                          nsresult aStatus,
                                          const PRUnichar* aMsg)
{
    nsresult rv = GetStatus();
    if (NS_FAILED(rv)) return rv;

    nsOnStopBindingEvent* event = 
        new nsOnStopBindingEvent(this, context);
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
// OnStartRequest...
//
////////////////////////////////////////////////////////////////////////////////

class nsOnStartRequestEvent : public nsStreamListenerEvent
{
public:
    nsOnStartRequestEvent(nsAsyncStreamObserver* listener, 
                          nsISupports* context)
        : nsStreamListenerEvent(listener, context), mContentType(nsnull) {}
    virtual ~nsOnStartRequestEvent();

    NS_IMETHOD HandleEvent();

protected:
    char*       mContentType;
};

nsOnStartRequestEvent::~nsOnStartRequestEvent()
{
    if (mContentType)
        delete[] mContentType;
}

NS_IMETHODIMP
nsOnStartRequestEvent::HandleEvent()
{
  nsIStreamObserver* receiver = (nsIStreamObserver*)mListener->GetReceiver();
  return receiver->OnStartRequest(mContext);
}

NS_IMETHODIMP 
nsAsyncStreamObserver::OnStartRequest(nsISupports* context)
{
    nsresult rv = GetStatus();
    if (NS_FAILED(rv)) return rv;

    nsOnStartRequestEvent* event = 
        new nsOnStartRequestEvent(this, context);
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
                         nsISupports* context)
        : nsStreamListenerEvent(listener, context),
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
  return receiver->OnStopRequest(mContext, mStatus, mMessage);
}

NS_IMETHODIMP 
nsAsyncStreamObserver::OnStopRequest(nsISupports* context,
                                          nsresult aStatus,
                                          const PRUnichar* aMsg)
{
    nsresult rv = GetStatus();
    if (NS_FAILED(rv)) return rv;

    nsOnStopRequestEvent* event = 
        new nsOnStopRequestEvent(this, context);
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
                           nsISupports* context)
        : nsStreamListenerEvent(listener, context),
          mIStream(nsnull), mLength(0) {}
    virtual ~nsOnDataAvailableEvent();

    nsresult Init(nsIBufferInputStream* aIStream, PRUint32 aSourceOffset,
                  PRUint32 aLength);
    NS_IMETHOD HandleEvent();

protected:
    nsIBufferInputStream*       mIStream;
    PRUint32                    mSourceOffset;
    PRUint32                    mLength;
};

nsOnDataAvailableEvent::~nsOnDataAvailableEvent()
{
    NS_RELEASE(mIStream);
}

nsresult
nsOnDataAvailableEvent::Init(nsIBufferInputStream* aIStream, PRUint32 aSourceOffset,
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
  return receiver->OnDataAvailable(mContext, mIStream, mSourceOffset, mLength);
}

NS_IMETHODIMP 
nsAsyncStreamListener::OnDataAvailable(nsISupports* context,
                                       nsIBufferInputStream *aIStream, 
                                       PRUint32 aSourceOffset,
                                       PRUint32 aLength)
{
    nsresult rv = GetStatus();
    if (NS_FAILED(rv)) return rv;

    nsOnDataAvailableEvent* event = 
        new nsOnDataAvailableEvent(this, context);
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

nsresult
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

nsresult
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
