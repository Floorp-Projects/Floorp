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
#ifndef ___nsftpstreamlistener_h__
#define ___nsftpstreamlistener_h__

#include "nsIStreamListener.h"

#include "plevent.h"
#include "nscore.h"
#include "nsString.h"


class nsFtpStreamListenerEvent : public PLEvent {
public:
    nsFtpStreamListenerEvent(nsIStreamListener* listener, nsISupports* context);
    virtual ~nsFtpStreamListenerEvent();

    nsresult Fire(PLEventQueue* aEventQ);

    NS_IMETHOD HandleEvent() = 0;

protected:
    static void PR_CALLBACK HandlePLEvent(PLEvent* aEvent);
    static void PR_CALLBACK DestroyPLEvent(PLEvent* aEvent);

    nsIStreamListener* mListener;
    nsISupports*                mContext;
};

class nsFtpOnStartBindingEvent : public nsFtpStreamListenerEvent
{
public:
    nsFtpOnStartBindingEvent(nsIStreamListener* listener, nsISupports* context)
        : nsFtpStreamListenerEvent(listener, context) {}
    virtual ~nsFtpOnStartBindingEvent() {}

    NS_IMETHOD HandleEvent();
};


class nsFtpOnDataAvailableEvent : public nsFtpStreamListenerEvent
{
public:
    nsFtpOnDataAvailableEvent(nsIStreamListener* listener, nsISupports* context)
        : nsFtpStreamListenerEvent(listener, context),
          mIStream(nsnull), mLength(0) {}
    virtual ~nsFtpOnDataAvailableEvent();

    nsresult Init(nsIInputStream* aIStream, PRUint32 aSourceOffset, PRUint32 aLength);
    NS_IMETHOD HandleEvent();

protected:
    nsIInputStream*     mIStream;
    PRUint32            mSourceOffset;
    PRUint32            mLength;
};


class nsFtpOnStopBindingEvent : public nsFtpStreamListenerEvent
{
public:
    nsFtpOnStopBindingEvent(nsIStreamListener* listener, nsISupports* context)
        : nsFtpStreamListenerEvent(listener, context),
          mStatus(NS_OK), mMessage(nsnull) {}
    virtual ~nsFtpOnStopBindingEvent();

    nsresult Init(nsresult status, nsIString* aMsg);
    NS_IMETHOD HandleEvent();

protected:
    nsresult    mStatus;
    nsIString*  mMessage;
};

#endif // ___nsftpstreamlistener_h__
