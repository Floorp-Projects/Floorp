/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nscore.h"
#include "nsRequestObserverProxy.h"
#include "nsIRequest.h"
#include "nsIServiceManager.h"
#include "nsProxyRelease.h"
#include "nsAutoPtr.h"
#include "nsString.h"
#include "prlog.h"

#if defined(PR_LOGGING)
static PRLogModuleInfo *gRequestObserverProxyLog;
#endif

#define LOG(args) PR_LOG(gRequestObserverProxyLog, PR_LOG_DEBUG, args)

//-----------------------------------------------------------------------------
// nsARequestObserverEvent internal class...
//-----------------------------------------------------------------------------

nsARequestObserverEvent::nsARequestObserverEvent(nsIRequest *request,
                                                 nsISupports *context)
    : mRequest(request)
    , mContext(context)
{
    NS_PRECONDITION(mRequest, "null pointer");
}

//-----------------------------------------------------------------------------
// nsOnStartRequestEvent internal class...
//-----------------------------------------------------------------------------

class nsOnStartRequestEvent : public nsARequestObserverEvent
{
    nsRefPtr<nsRequestObserverProxy> mProxy;
public:
    nsOnStartRequestEvent(nsRequestObserverProxy *proxy,
                          nsIRequest *request,
                          nsISupports *context)
        : nsARequestObserverEvent(request, context)
        , mProxy(proxy)
    {
        NS_PRECONDITION(mProxy, "null pointer");
    }

    virtual ~nsOnStartRequestEvent() {}

    NS_IMETHOD Run()
    {
        LOG(("nsOnStartRequestEvent::HandleEvent [req=%x]\n", mRequest.get()));

        if (!mProxy->mObserver) {
            NS_NOTREACHED("already handled onStopRequest event (observer is null)");
            return NS_OK;
        }

        LOG(("handle startevent=%p\n", this));
        nsresult rv = mProxy->mObserver->OnStartRequest(mRequest, mContext);
        if (NS_FAILED(rv)) {
            LOG(("OnStartRequest failed [rv=%x] canceling request!\n", rv));
            rv = mRequest->Cancel(rv);
            NS_ASSERTION(NS_SUCCEEDED(rv), "Cancel failed for request!");
        }

        return NS_OK;
    }
};

//-----------------------------------------------------------------------------
// nsOnStopRequestEvent internal class...
//-----------------------------------------------------------------------------

class nsOnStopRequestEvent : public nsARequestObserverEvent
{
    nsRefPtr<nsRequestObserverProxy> mProxy;
public:
    nsOnStopRequestEvent(nsRequestObserverProxy *proxy,
                         nsIRequest *request, nsISupports *context)
        : nsARequestObserverEvent(request, context)
        , mProxy(proxy)
    {
        NS_PRECONDITION(mProxy, "null pointer");
    }

    virtual ~nsOnStopRequestEvent() {}

    NS_IMETHOD Run()
    {
        nsresult rv, status = NS_OK;

        LOG(("nsOnStopRequestEvent::HandleEvent [req=%x]\n", mRequest.get()));

        nsCOMPtr<nsIRequestObserver> observer = mProxy->mObserver;
        if (!observer) {
            NS_NOTREACHED("already handled onStopRequest event (observer is null)");
            return NS_OK;
        }
        // Do not allow any more events to be handled after OnStopRequest
        mProxy->mObserver = 0;

        rv = mRequest->GetStatus(&status);
        NS_ASSERTION(NS_SUCCEEDED(rv), "GetStatus failed for request!");

        LOG(("handle stopevent=%p\n", this));
        (void) observer->OnStopRequest(mRequest, mContext, status);

        return NS_OK;
    }
};

//-----------------------------------------------------------------------------
// nsRequestObserverProxy <public>
//-----------------------------------------------------------------------------

nsRequestObserverProxy::~nsRequestObserverProxy()
{
    if (mObserver) {
        // order is crucial here... we must be careful to clear mObserver
        // before posting the proxy release event.  otherwise, we'd risk
        // releasing the object on this thread.
        nsIRequestObserver *obs = nsnull;
        mObserver.swap(obs);
        NS_ProxyRelease(mTarget, obs);
    }
}

//-----------------------------------------------------------------------------
// nsRequestObserverProxy::nsISupports implementation...
//-----------------------------------------------------------------------------

NS_IMPL_THREADSAFE_ISUPPORTS2(nsRequestObserverProxy,
                              nsIRequestObserver,
                              nsIRequestObserverProxy)

//-----------------------------------------------------------------------------
// nsRequestObserverProxy::nsIRequestObserver implementation...
//-----------------------------------------------------------------------------

NS_IMETHODIMP 
nsRequestObserverProxy::OnStartRequest(nsIRequest *request,
                                       nsISupports *context)
{
    LOG(("nsRequestObserverProxy::OnStartRequest [this=%x req=%x]\n", this, request));

    nsOnStartRequestEvent *ev = 
        new nsOnStartRequestEvent(this, request, context);
    if (!ev)
        return NS_ERROR_OUT_OF_MEMORY;

    LOG(("post startevent=%p queue=%p\n", ev, mTarget.get()));
    nsresult rv = FireEvent(ev);
    if (NS_FAILED(rv))
        delete ev;
    return rv;
}

NS_IMETHODIMP 
nsRequestObserverProxy::OnStopRequest(nsIRequest *request,
                                      nsISupports *context,
                                      nsresult status)
{
    LOG(("nsRequestObserverProxy: OnStopRequest [this=%x req=%x status=%x]\n",
        this, request, status));

    // The status argument is ignored because, by the time the OnStopRequestEvent
    // is actually processed, the status of the request may have changed :-( 
    // To make sure that an accurate status code is always used, GetStatus() is
    // called when the OnStopRequestEvent is actually processed (see above).

    nsOnStopRequestEvent *ev = 
        new nsOnStopRequestEvent(this, request, context);
    if (!ev)
        return NS_ERROR_OUT_OF_MEMORY;

    LOG(("post stopevent=%p queue=%p\n", ev, mTarget.get()));
    nsresult rv = FireEvent(ev);
    if (NS_FAILED(rv))
        delete ev;
    return rv;
}

//-----------------------------------------------------------------------------
// nsRequestObserverProxy::nsIRequestObserverProxy implementation...
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsRequestObserverProxy::Init(nsIRequestObserver *observer,
                             nsIEventTarget *target)
{
    NS_ENSURE_ARG_POINTER(observer);

#if defined(PR_LOGGING)
    if (!gRequestObserverProxyLog)
        gRequestObserverProxyLog = PR_NewLogModule("nsRequestObserverProxy");
#endif

    mObserver = observer;

    SetTarget(target ? target : NS_GetCurrentThread());
    NS_ENSURE_STATE(mTarget);

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsRequestObserverProxy implementation...
//-----------------------------------------------------------------------------

nsresult
nsRequestObserverProxy::FireEvent(nsARequestObserverEvent *event)
{
    NS_ENSURE_TRUE(mTarget, NS_ERROR_NOT_INITIALIZED);
    return mTarget->Dispatch(event, NS_DISPATCH_NORMAL);
}
