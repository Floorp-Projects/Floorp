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

#include "nsIStreamListener.h"
#include "nsIUrl.h"
#include "nsIInputStream.h"
#include "nsCRT.h"

class nsMarshalingStreamListener : public nsIStreamListener
{
public:
    NS_DECL_ISUPPORTS

    // nsIStreamListener methods:
    NS_IMETHOD OnStartBinding(nsIUrl* aUrl, const char *aContentType);
    NS_IMETHOD OnDataAvailable(nsIUrl* aUrl, nsIInputStream *aIStream, 
                               PRUint32 aLength);
    NS_IMETHOD OnStopBinding(nsIUrl* aUrl, nsresult aStatus, const PRUnichar* aMsg);
    NS_IMETHOD GetBindInfo(nsIUrl* aUrl, nsStreamBindingInfo* aInfo);

    // nsMarshalingStreamListener methods:
    nsMarshalingStreamListener(PLEventQueue* eventQueue,
                               nsIStreamListener* receiver) 
        : mEventQueue(eventQueue), mReceiver(receiver) {
        NS_INIT_REFCNT();
    }
    virtual ~nsMarshalingStreamListener();

    nsIStreamListener* GetReceiver() { return mReceiver; }
    nsresult GetStatus() { return mStatus; }
    void SetStatus(nsresult value) { mStatus = value; }

protected:
    PLEventQueue*       mEventQueue;
    nsIStreamListener*  mReceiver;
    nsresult            mStatus;
};

////////////////////////////////////////////////////////////////////////////////

class nsStreamListenerEvent : public PLEvent 
{
public:
    nsStreamListenerEvent(nsMarshalingStreamListener* listener, nsIUrl* url);
    virtual ~nsStreamListenerEvent();

    nsresult Fire(PLEventQueue* aEventQ);

    NS_IMETHOD HandleEvent() = 0;

protected:
    static void PR_CALLBACK HandlePLEvent(PLEvent* aEvent);
    static void PR_CALLBACK DestroyPLEvent(PLEvent* aEvent);

    nsMarshalingStreamListener* mListener;
    nsIUrl*                     mURL;
};

////////////////////////////////////////////////////////////////////////////////

nsStreamListenerEvent::nsStreamListenerEvent(nsMarshalingStreamListener* listener, nsIUrl* url)
    : mListener(listener), mURL(url)
{
    NS_ADDREF(mListener);
    NS_ADDREF(mURL);
}

nsStreamListenerEvent::~nsStreamListenerEvent()
{
    NS_RELEASE(mListener);
    NS_RELEASE(mURL);
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

nsMarshalingStreamListener::~nsMarshalingStreamListener()
{
}

NS_IMPL_ISUPPORTS(nsMarshalingStreamListener, nsIStreamListener::GetIID());

////////////////////////////////////////////////////////////////////////////////

class nsOnStartBindingEvent : public nsStreamListenerEvent
{
public:
    nsOnStartBindingEvent(nsMarshalingStreamListener* listener, nsIUrl* url)
        : nsStreamListenerEvent(listener, url), mContentType(nsnull) {}
    virtual ~nsOnStartBindingEvent();

    nsresult Init(const char* aContentType);
    NS_IMETHOD HandleEvent();

protected:
    char*       mContentType;
};

nsOnStartBindingEvent::~nsOnStartBindingEvent()
{
    if (mContentType)
        delete[] mContentType;
}

nsresult
nsOnStartBindingEvent::Init(const char* aContentType)
{
    mContentType = nsCRT::strdup(aContentType);
    if (mContentType == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsOnStartBindingEvent::HandleEvent()
{
    return mListener->GetReceiver()->OnStartBinding(mURL, mContentType);
}

NS_IMETHODIMP 
nsMarshalingStreamListener::OnStartBinding(nsIUrl* aUrl, const char *aContentType)
{
    nsresult rv = GetStatus();
    if (NS_FAILED(rv)) return rv;

    nsOnStartBindingEvent* event = 
        new nsOnStartBindingEvent(this, aUrl);
    if (event == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = event->Init(aContentType);
    if (NS_FAILED(rv)) goto failed;
    rv = event->Fire(mEventQueue);
    if (NS_FAILED(rv)) goto failed;
    return rv;

  failed:
    delete event;
    return rv;
}

////////////////////////////////////////////////////////////////////////////////

class nsOnDataAvailableEvent : public nsStreamListenerEvent
{
public:
    nsOnDataAvailableEvent(nsMarshalingStreamListener* listener, nsIUrl* url)
        : nsStreamListenerEvent(listener, url), mIStream(nsnull), mLength(0) {}
    virtual ~nsOnDataAvailableEvent();

    nsresult Init(nsIInputStream* aIStream, PRUint32 aLength);
    NS_IMETHOD HandleEvent();

protected:
    nsIInputStream*     mIStream;
    PRUint32            mLength;
};

nsOnDataAvailableEvent::~nsOnDataAvailableEvent()
{
    NS_RELEASE(mIStream);
}

nsresult
nsOnDataAvailableEvent::Init(nsIInputStream* aIStream, PRUint32 aLength)
{
    mLength = aLength;
    mIStream = aIStream;
    NS_ADDREF(mIStream);
    return NS_OK;
}

NS_IMETHODIMP
nsOnDataAvailableEvent::HandleEvent()
{
    return mListener->GetReceiver()->OnDataAvailable(mURL, mIStream, mLength);
}

NS_IMETHODIMP 
nsMarshalingStreamListener::OnDataAvailable(nsIUrl* aUrl, nsIInputStream *aIStream, 
                                            PRUint32 aLength)
{
    nsresult rv = GetStatus();
    if (NS_FAILED(rv)) return rv;

    nsOnDataAvailableEvent* event = 
        new nsOnDataAvailableEvent(this, aUrl);
    if (event == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = event->Init(aIStream, aLength);
    if (NS_FAILED(rv)) goto failed;
    rv = event->Fire(mEventQueue);
    if (NS_FAILED(rv)) goto failed;
    return rv;

  failed:
    delete event;
    return rv;
}

////////////////////////////////////////////////////////////////////////////////

class nsOnStopBindingEvent : public nsStreamListenerEvent
{
public:
    nsOnStopBindingEvent(nsMarshalingStreamListener* listener, nsIUrl* url)
        : nsStreamListenerEvent(listener, url), mStatus(NS_OK), mMessage(nsnull) {}
    virtual ~nsOnStopBindingEvent();

    nsresult Init(nsresult status, const PRUnichar* aMsg);
    NS_IMETHOD HandleEvent();

protected:
    nsresult    mStatus;
    PRUnichar*  mMessage;
};

nsOnStopBindingEvent::~nsOnStopBindingEvent()
{
    if (mMessage)
        delete[] mMessage;
}

nsresult
nsOnStopBindingEvent::Init(nsresult status, const PRUnichar* aMsg)
{
    mStatus = status;
    mMessage = nsCRT::strdup(aMsg);
    if (mMessage == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsOnStopBindingEvent::HandleEvent()
{
    return mListener->GetReceiver()->OnStopBinding(mURL, mStatus, mMessage);
}

NS_IMETHODIMP 
nsMarshalingStreamListener::OnStopBinding(nsIUrl* aUrl, nsresult aStatus,
                                          const PRUnichar* aMsg)
{
    nsresult rv = GetStatus();
    if (NS_FAILED(rv)) return rv;

    nsOnStopBindingEvent* event = 
        new nsOnStopBindingEvent(this, aUrl);
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

#if 0
class nsGetBindInfoEvent : public nsStreamListenerEvent
{
public:
    nsGetBindInfoEvent(nsMarshalingStreamListener* listener, nsIUrl* url)
        : nsStreamListenerEvent(listener, url), mContentType(nsnull) {}
    virtual ~nsGetBindInfoEvent();

    nsresult Init(const char* aContentType);
    NS_IMETHOD HandleEvent();

protected:
    char*       mContentType;
};

nsGetBindInfoEvent::~nsGetBindInfoEvent()
{
    if (mContentType)
        delete[] mContentType;
}

nsresult
nsGetBindInfoEvent::Init(const char* aContentType)
{
    mContentType = nsCRT::strdup(aContentType);
    if (mContentType == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsGetBindInfoEvent::HandleEvent()
{
    return mListener->GetReceiver()->GetBindInfo(mURL, mContentType);
}
#endif

NS_IMETHODIMP 
nsMarshalingStreamListener::GetBindInfo(nsIUrl* aUrl, nsStreamBindingInfo* aInfo)
{
#if 0
    nsresult rv = GetStatus();
    if (NS_FAILED(rv)) return rv;

    nsGetBindInfoEvent* event = 
        new nsGetBindInfoEvent(this, aUrl);
    if (event == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = event->Init(aContentType);
    if (NS_FAILED(rv)) goto failed;
    rv = event->Fire(mEventQueue);
    if (NS_FAILED(rv)) goto failed;
    return rv;

  failed:
    delete event;
    return rv;
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

////////////////////////////////////////////////////////////////////////////////

nsresult
NS_NewMarshalingStreamListener(PLEventQueue* eventQueue,
                               nsIStreamListener* receiver,
                               nsIStreamListener* *result)
{
    nsMarshalingStreamListener* l =
        new nsMarshalingStreamListener(eventQueue, receiver);
    if (l == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(l);
    *result = l;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
