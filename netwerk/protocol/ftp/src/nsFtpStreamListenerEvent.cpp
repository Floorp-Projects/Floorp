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

#include "nsFtpStreamListenerEvent.h"
#include "nsIInputStream.h"

#include "nscore.h"
#include "nsIString.h"


nsFtpStreamListenerEvent::nsFtpStreamListenerEvent(nsIStreamListener* listener,
                                             nsISupports* context)
    : mListener(listener), mContext(context)
{
    NS_ADDREF(mListener);
    NS_IF_ADDREF(mContext);
}

nsFtpStreamListenerEvent::~nsFtpStreamListenerEvent()
{
    NS_RELEASE(mListener);
    NS_IF_RELEASE(mContext);
}

void PR_CALLBACK nsFtpStreamListenerEvent::HandlePLEvent(PLEvent* aEvent)
{
    // WARNING: This is a dangerous cast since it must adjust the pointer 
    // to compensate for the vtable...
    nsFtpStreamListenerEvent *ev = (nsFtpStreamListenerEvent*)aEvent;

    nsresult rv = ev->HandleEvent();
    //ev->mListener->SetStatus(rv);
}

void PR_CALLBACK nsFtpStreamListenerEvent::DestroyPLEvent(PLEvent* aEvent)
{
    // WARNING: This is a dangerous cast since it must adjust the pointer 
    // to compensate for the vtable...
    nsFtpStreamListenerEvent *ev = (nsFtpStreamListenerEvent*)aEvent;

    delete ev;
}

nsresult
nsFtpStreamListenerEvent::Fire(PLEventQueue* aEventQueue) 
{
    NS_PRECONDITION(nsnull != aEventQueue, "PLEventQueue for thread is null");

    PL_InitEvent(this, nsnull,
                 (PLHandleEventProc)  nsFtpStreamListenerEvent::HandlePLEvent,
                 (PLDestroyEventProc) nsFtpStreamListenerEvent::DestroyPLEvent);

    PRStatus status = PL_PostEvent(aEventQueue, this);
    return status == PR_SUCCESS ? NS_OK : NS_ERROR_FAILURE;
}




////////////////////////////////////////////////////////////////////////////////
//
// OnStartBinding...
//
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFtpOnStartBindingEvent::HandleEvent()
{
  nsIStreamObserver* receiver = (nsIStreamObserver*)mListener;
  return receiver->OnStartBinding(mContext);
}
/*
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
*/
////////////////////////////////////////////////////////////////////////////////
//
// OnDataAvailable
//
////////////////////////////////////////////////////////////////////////////////

nsFtpOnDataAvailableEvent::~nsFtpOnDataAvailableEvent()
{
    NS_RELEASE(mIStream);
}

nsresult
nsFtpOnDataAvailableEvent::Init(nsIInputStream* aIStream, PRUint32 aLength)
{
    mLength = aLength;
    mIStream = aIStream;
    NS_ADDREF(mIStream);
    return NS_OK;
}

NS_IMETHODIMP
nsFtpOnDataAvailableEvent::HandleEvent()
{
  nsIStreamListener* receiver = (nsIStreamListener*)mListener;
  return receiver->OnDataAvailable(mContext, mIStream, mLength);
}
/*
NS_IMETHODIMP 
nsMarshalingStreamListener::OnDataAvailable(nsISupports* context,
                                            nsIInputStream *aIStream, 
                                            PRUint32 aLength)
{
    nsresult rv = GetStatus();
    if (NS_FAILED(rv)) return rv;

    nsOnDataAvailableEvent* event = 
        new nsOnDataAvailableEvent(this, context);
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
*/
////////////////////////////////////////////////////////////////////////////////
//
// OnStopBinding
//
////////////////////////////////////////////////////////////////////////////////

nsFtpOnStopBindingEvent::~nsFtpOnStopBindingEvent()
{
    NS_IF_RELEASE(mMessage);
}

nsresult
nsFtpOnStopBindingEvent::Init(nsresult status, nsIString* aMsg)
{
    mStatus = status;
    mMessage = aMsg;
    NS_IF_ADDREF(mMessage);
    return NS_OK;
}

NS_IMETHODIMP
nsFtpOnStopBindingEvent::HandleEvent()
{
  nsIStreamObserver* receiver = (nsIStreamObserver*)mListener;
  return receiver->OnStopBinding(mContext, mStatus, mMessage);
}
/*
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
*/
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
