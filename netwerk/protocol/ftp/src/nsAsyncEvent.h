/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef ___nsasyncevent_h____
#define ___nsasyncevent_h____

#include "nsCOMPtr.h"
#include "nsIStreamListener.h"
#include "nsIChannel.h"
#include "nsIInputStream.h"
#include "nsIEventQueue.h"
#include "plevent.h"

// Abstract async event class.
class nsAsyncEvent {
public:
    nsAsyncEvent(nsIChannel* channel, nsISupports* context);
    virtual ~nsAsyncEvent();

    nsresult Fire(nsIEventQueue *aEventQ);

    NS_IMETHOD HandleEvent() = 0;

protected:
    static void PR_CALLBACK HandlePLEvent(PLEvent* aEvent);
    static void PR_CALLBACK DestroyPLEvent(PLEvent* aEvent);

    nsCOMPtr<nsIChannel>        mChannel;
    nsCOMPtr<nsISupports>       mContext;
    PLEvent *                   mEvent;
};


// AsyncRead() event.
class nsFTPAsyncReadEvent : public nsAsyncEvent
{
public:
    nsFTPAsyncReadEvent(nsIStreamListener* listener, nsIChannel* channel, nsISupports* context);
    virtual ~nsFTPAsyncReadEvent() {}

    NS_IMETHOD HandleEvent();
protected:
    nsCOMPtr<nsIStreamListener> mListener;
};


// AsyncWrite() event.
class nsFTPAsyncWriteEvent : public nsAsyncEvent
{
public:
    nsFTPAsyncWriteEvent(nsIInputStream* inStream,
                         PRUint32 writeCount,
                         nsIStreamObserver* observer,
                         nsIChannel* channel, 
                         nsISupports* context);
    virtual ~nsFTPAsyncWriteEvent() {}

    NS_IMETHOD HandleEvent();
protected:
    nsCOMPtr<nsIStreamObserver> mObserver;
    nsCOMPtr<nsIInputStream>    mInStream;
    PRUint32                    mWriteCount;
};

#endif // ___nsasyncevent_h____
