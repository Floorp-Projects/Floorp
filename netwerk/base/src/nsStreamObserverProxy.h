/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
        PRBool same = ::SameCOMIdentity(aReceiver, this);
        NS_ASSERTION((!same), "aReceiver is self"); 
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
                          nsIRequest *request, nsISupports *context);
    virtual ~nsStreamObserverEvent();

    nsresult FireEvent(nsIEventQueue *);
    NS_IMETHOD HandleEvent() = 0;

protected:
    static void PR_CALLBACK HandlePLEvent(PLEvent *);
    static void PR_CALLBACK DestroyPLEvent(PLEvent *);

    PLEvent                mEvent;
    nsStreamProxyBase     *mProxy;
    nsCOMPtr<nsIRequest>   mRequest;
    nsCOMPtr<nsISupports>  mContext;
};

#define GET_STREAM_OBSERVER_EVENT(_mEvent_ptr) \
    ((nsStreamObserverEvent *) \
     ((char *)(_mEvent_ptr) - offsetof(nsStreamObserverEvent, mEvent)))

#endif /* !nsStreamObserverProxy_h__ */
