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
#include "nsIInputStream.h"
#include "nsIString.h"
#include "nsCRT.h"

class nsMarshalingStreamObserver : public nsIStreamObserver
{
public:
    NS_DECL_ISUPPORTS

    // nsIStreamObserver methods:
    NS_IMETHOD OnStartBinding(nsISupports* context);
    NS_IMETHOD OnStopBinding(nsISupports* context,
                             nsresult aStatus,
                             nsIString* aMsg);

    // nsMarshalingStreamObserver methods:
    nsMarshalingStreamObserver(PLEventQueue* aEventQ) 
      : mEventQueue(aEventQ), mReceiver(nsnull), mStatus(NS_OK) 
    { 
      NS_INIT_REFCNT();
    }
    
    virtual ~nsMarshalingStreamObserver();

    void Init(nsIStreamObserver* aListener) {
      mReceiver = aListener;
      NS_ADDREF(mReceiver);
    }

    nsISupports* GetReceiver()      { return mReceiver; }
    nsresult GetStatus()            { return mStatus; }
    void SetStatus(nsresult value)  { mStatus = value; }

protected:
    PLEventQueue*       mEventQueue;
    nsIStreamObserver*  mReceiver;
    nsresult            mStatus;
};


class nsMarshalingStreamListener : public nsMarshalingStreamObserver,
                                   public nsIStreamListener
{
public:
    NS_DECL_ISUPPORTS_INHERITED

    // nsIStreamListener methods:
    NS_IMETHOD OnStartBinding(nsISupports* context) 
    { 
      return nsMarshalingStreamObserver::OnStartBinding(context); 
    }

    NS_IMETHOD OnStopBinding(nsISupports* context,
                             nsresult aStatus,
                             nsIString* aMsg) 
    { 
      return nsMarshalingStreamObserver::OnStopBinding(context, aStatus, aMsg); 
    }

    NS_IMETHOD OnDataAvailable(nsISupports* context,
                               nsIInputStream *aIStream, 
                               PRUint32 aSourceOffset,
                               PRUint32 aLength);

    // nsMarshalingStreamListener methods:
    nsMarshalingStreamListener(PLEventQueue* aEventQ) 
      : nsMarshalingStreamObserver(aEventQ) {}

    void Init(nsIStreamListener* aListener) {
      mReceiver = aListener;
      NS_ADDREF(mReceiver);
    }
};

////////////////////////////////////////////////////////////////////////////////

class nsStreamListenerEvent : public PLEvent 
{
public:
    nsStreamListenerEvent(nsMarshalingStreamObserver* listener,
                          nsISupports* context);
    virtual ~nsStreamListenerEvent();

    nsresult Fire(PLEventQueue* aEventQ);

    NS_IMETHOD HandleEvent() = 0;

protected:
    static void PR_CALLBACK HandlePLEvent(PLEvent* aEvent);
    static void PR_CALLBACK DestroyPLEvent(PLEvent* aEvent);

    nsMarshalingStreamObserver* mListener;
    nsISupports*                mContext;
};

////////////////////////////////////////////////////////////////////////////////

nsStreamListenerEvent::nsStreamListenerEvent(nsMarshalingStreamObserver* listener,
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
nsStreamListenerEvent::Fire(PLEventQueue* aEventQueue) 
{
    NS_PRECONDITION(nsnull != aEventQueue, "PLEventQueue for thread is null");

    PL_InitEvent(this, nsnull,
                 (PLHandleEventProc)  nsStreamListenerEvent::HandlePLEvent,
                 (PLDestroyEventProc) nsStreamListenerEvent::DestroyPLEvent);

    PRStatus status = PL_PostEvent(aEventQueue, this);
    return status == PR_SUCCESS ? NS_OK : NS_ERROR_FAILURE;
}

////////////////////////////////////////////////////////////////////////////////

nsMarshalingStreamObserver::~nsMarshalingStreamObserver()
{
  NS_RELEASE(mReceiver);
}

NS_IMPL_ISUPPORTS(nsMarshalingStreamObserver, nsIStreamObserver::GetIID());

NS_IMPL_ISUPPORTS_INHERITED(nsMarshalingStreamListener, 
                            nsMarshalingStreamObserver, 
                            nsIStreamListener);

////////////////////////////////////////////////////////////////////////////////
//
// OnStartBinding...
//
////////////////////////////////////////////////////////////////////////////////
class nsOnStartBindingEvent : public nsStreamListenerEvent
{
public:
    nsOnStartBindingEvent(nsMarshalingStreamObserver* listener, 
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
nsMarshalingStreamObserver::OnStartBinding(nsISupports* context)
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
// OnDataAvailable
//
////////////////////////////////////////////////////////////////////////////////

class nsOnDataAvailableEvent : public nsStreamListenerEvent
{
public:
    nsOnDataAvailableEvent(nsMarshalingStreamObserver* listener, 
                           nsISupports* context)
        : nsStreamListenerEvent(listener, context),
          mIStream(nsnull), mLength(0) {}
    virtual ~nsOnDataAvailableEvent();

    nsresult Init(nsIInputStream* aIStream, PRUint32 aSourceOffset,
                  PRUint32 aLength);
    NS_IMETHOD HandleEvent();

protected:
    nsIInputStream*     mIStream;
    PRUint32            mSourceOffset;
    PRUint32            mLength;
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
  return receiver->OnDataAvailable(mContext, mIStream, mSourceOffset, mLength);
}

NS_IMETHODIMP 
nsMarshalingStreamListener::OnDataAvailable(nsISupports* context,
                                            nsIInputStream *aIStream, 
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
//
// OnStopBinding
//
////////////////////////////////////////////////////////////////////////////////

class nsOnStopBindingEvent : public nsStreamListenerEvent
{
public:
    nsOnStopBindingEvent(nsMarshalingStreamObserver* listener, 
                         nsISupports* context)
        : nsStreamListenerEvent(listener, context),
          mStatus(NS_OK), mMessage(nsnull) {}
    virtual ~nsOnStopBindingEvent();

    nsresult Init(nsresult status, nsIString* aMsg);
    NS_IMETHOD HandleEvent();

protected:
    nsresult    mStatus;
    nsIString*  mMessage;
};

nsOnStopBindingEvent::~nsOnStopBindingEvent()
{
    NS_IF_RELEASE(mMessage);
}

nsresult
nsOnStopBindingEvent::Init(nsresult status, nsIString* aMsg)
{
    mStatus = status;
    mMessage = aMsg;
    NS_IF_ADDREF(mMessage);
    return NS_OK;
}

NS_IMETHODIMP
nsOnStopBindingEvent::HandleEvent()
{
  nsIStreamObserver* receiver = (nsIStreamObserver*)mListener->GetReceiver();
  return receiver->OnStopBinding(mContext, mStatus, mMessage);
}

NS_IMETHODIMP 
nsMarshalingStreamObserver::OnStopBinding(nsISupports* context,
                                          nsresult aStatus,
                                          nsIString* aMsg)
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

nsresult
NS_NewAsyncStreamObserver(nsIStreamObserver* *result,
                          PLEventQueue* eventQueue,
                          nsIStreamObserver* receiver)
{
    nsMarshalingStreamObserver* l =
        new nsMarshalingStreamObserver(eventQueue);
    if (l == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    l->Init(receiver);
    NS_ADDREF(l);
    *result = l;
    return NS_OK;
}

nsresult
NS_NewAsyncStreamListener(nsIStreamListener* *result,
                          PLEventQueue* eventQueue,
                          nsIStreamListener* receiver)
{
    nsMarshalingStreamListener* l =
        new nsMarshalingStreamListener(eventQueue);
    if (l == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    l->Init(receiver);
    NS_ADDREF(l);
    *result = l;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
