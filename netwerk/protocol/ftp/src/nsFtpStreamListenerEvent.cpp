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
#include "nsIBufferInputStream.h"
#include "nscore.h"
#include "nsIChannel.h"

nsFtpStreamListenerEvent::nsFtpStreamListenerEvent(nsIStreamListener* listener,
                                                   nsIChannel* channel,
                                                   nsISupports* context)
    : mListener(listener), mChannel(channel), mContext(context)
{
    NS_ADDREF(mListener);
    NS_ADDREF(mChannel);
    NS_IF_ADDREF(mContext);
}

nsFtpStreamListenerEvent::~nsFtpStreamListenerEvent()
{
    NS_RELEASE(mListener);
    NS_RELEASE(mChannel);
    NS_IF_RELEASE(mContext);
}

void PR_CALLBACK nsFtpStreamListenerEvent::HandlePLEvent(PLEvent* aEvent)
{
    // WARNING: This is a dangerous cast since it must adjust the pointer 
    // to compensate for the vtable...
    nsFtpStreamListenerEvent *ev = (nsFtpStreamListenerEvent*)aEvent;

    ev->HandleEvent();
}

void PR_CALLBACK nsFtpStreamListenerEvent::DestroyPLEvent(PLEvent* aEvent)
{
    // WARNING: This is a dangerous cast since it must adjust the pointer 
    // to compensate for the vtable...
    nsFtpStreamListenerEvent *ev = (nsFtpStreamListenerEvent*)aEvent;

    delete ev;
}

nsresult
nsFtpStreamListenerEvent::Fire(nsIEventQueue* aEventQueue) 
{
    NS_PRECONDITION(nsnull != aEventQueue, "nsIEventQueue for thread is null");

    PL_InitEvent(this, nsnull,
                 (PLHandleEventProc)  nsFtpStreamListenerEvent::HandlePLEvent,
                 (PLDestroyEventProc) nsFtpStreamListenerEvent::DestroyPLEvent);

    PRStatus status = aEventQueue->PostEvent(this);
    return status == PR_SUCCESS ? NS_OK : NS_ERROR_FAILURE;
}




////////////////////////////////////////////////////////////////////////////////
//
// OnStartRequest...
//
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFtpOnStartRequestEvent::HandleEvent()
{
  nsIStreamObserver* receiver = (nsIStreamObserver*)mListener;
  return receiver->OnStartRequest(mChannel, mContext);
}
/*
NS_IMETHODIMP 
nsMarshalingStreamObserver::OnStartRequest(nsISupports* context)
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
nsFtpOnDataAvailableEvent::Init(nsIInputStream* aIStream, 
                                PRUint32 aSourceOffset, PRUint32 aLength)
{
    mLength = aLength;
    mSourceOffset = aSourceOffset;
    mIStream = aIStream;
    NS_ADDREF(mIStream);
    return NS_OK;
}

NS_IMETHODIMP
nsFtpOnDataAvailableEvent::HandleEvent()
{
  nsIStreamListener* receiver = (nsIStreamListener*)mListener;
  return receiver->OnDataAvailable(mChannel, mContext, mIStream, mSourceOffset, mLength);
}
/*
NS_IMETHODIMP 
nsMarshalingStreamListener::OnDataAvailable(nsISupports* context,
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
*/
////////////////////////////////////////////////////////////////////////////////
//
// OnStopRequest
//
////////////////////////////////////////////////////////////////////////////////

nsFtpOnStopRequestEvent::~nsFtpOnStopRequestEvent()
{
}

nsresult
nsFtpOnStopRequestEvent::Init(nsresult status, PRUnichar* aMsg)
{
    mStatus = status;
    mMessage = aMsg;
    return NS_OK;
}

NS_IMETHODIMP
nsFtpOnStopRequestEvent::HandleEvent()
{
  nsIStreamObserver* receiver = (nsIStreamObserver*)mListener;
  return receiver->OnStopRequest(mChannel, mContext, mStatus, mMessage);
}
/*
NS_IMETHODIMP 
nsMarshalingStreamObserver::OnStopRequest(nsISupports* context,
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
*/
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
