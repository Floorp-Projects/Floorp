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

#ifndef nsStreamObserverProxy_h__
#define nsStreamObserverProxy_h__

#include "nsIStreamObserver.h"
#include "nsIEventQueue.h"
#include "nsIChannel.h"
#include "nsCOMPtr.h"
#include "prlog.h"

#if defined(PR_LOGGING)
extern PRLogModuleInfo *gStreamProxyLog;
#endif

class nsStreamProxyBase : public nsIStreamObserver
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISTREAMOBSERVER

    nsStreamProxyBase() { NS_INIT_ISUPPORTS(); }
    virtual ~nsStreamProxyBase() {}

    nsIEventQueue *GetEventQueue() { return mEventQ.get(); }
    nsIStreamObserver *GetReceiver() { return mReceiver.get(); }

    nsresult SetEventQueue(nsIEventQueue *);

    nsresult SetReceiver(nsIStreamObserver *aReceiver) {
        mReceiver = aReceiver;
        return NS_OK;
    }

private:
    nsCOMPtr<nsIEventQueue>     mEventQ;
    nsCOMPtr<nsIStreamObserver> mReceiver;
};

class nsStreamObserverProxy : public nsStreamProxyBase
                            , public nsIStreamObserverProxy
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_FORWARD_NSISTREAMOBSERVER(nsStreamProxyBase::)
    NS_DECL_NSISTREAMOBSERVERPROXY
};

class nsStreamObserverEvent
{
public:
    nsStreamObserverEvent(nsStreamProxyBase *proxy,
                          nsIChannel *channel, nsISupports *context);
    virtual ~nsStreamObserverEvent();

    nsresult FireEvent(nsIEventQueue *);
    NS_IMETHOD HandleEvent() = 0;

protected:
    static void PR_CALLBACK HandlePLEvent(PLEvent *);
    static void PR_CALLBACK DestroyPLEvent(PLEvent *);

    PLEvent                mEvent;
    nsStreamProxyBase     *mProxy;
    nsCOMPtr<nsIChannel>   mChannel;
    nsCOMPtr<nsISupports>  mContext;
};

#define GET_STREAM_OBSERVER_EVENT(_mEvent_ptr) \
    ((nsStreamObserverEvent *) \
     ((char *)(_mEvent_ptr) - offsetof(nsStreamObserverEvent, mEvent)))

#endif /* !nsStreamObserverProxy_h__ */
