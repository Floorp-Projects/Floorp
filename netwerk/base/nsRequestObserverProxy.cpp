/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "nscore.h"
#include "nsRequestObserverProxy.h"
#include "nsIRequest.h"
#include "nsAutoPtr.h"
#include "mozilla/Logging.h"
#include "mozilla/IntegerPrintfMacros.h"

namespace mozilla {
namespace net {

static LazyLogModule gRequestObserverProxyLog("nsRequestObserverProxy");

#undef LOG
#define LOG(args) MOZ_LOG(gRequestObserverProxyLog, LogLevel::Debug, args)

//-----------------------------------------------------------------------------
// nsARequestObserverEvent internal class...
//-----------------------------------------------------------------------------

nsARequestObserverEvent::nsARequestObserverEvent(nsIRequest* request)
  : Runnable("net::nsARequestObserverEvent")
  , mRequest(request)
{
    NS_PRECONDITION(mRequest, "null pointer");
}

//-----------------------------------------------------------------------------
// nsOnStartRequestEvent internal class...
//-----------------------------------------------------------------------------

class nsOnStartRequestEvent : public nsARequestObserverEvent
{
    RefPtr<nsRequestObserverProxy> mProxy;
public:
    nsOnStartRequestEvent(nsRequestObserverProxy *proxy,
                          nsIRequest *request)
        : nsARequestObserverEvent(request)
        , mProxy(proxy)
    {
        NS_PRECONDITION(mProxy, "null pointer");
    }

    virtual ~nsOnStartRequestEvent() {}

    NS_IMETHOD Run() override
    {
        LOG(("nsOnStartRequestEvent::HandleEvent [req=%p]\n", mRequest.get()));

        if (!mProxy->mObserver) {
            NS_NOTREACHED("already handled onStopRequest event (observer is null)");
            return NS_OK;
        }

        LOG(("handle startevent=%p\n", this));
        nsresult rv = mProxy->mObserver->OnStartRequest(mRequest, mProxy->mContext);
        if (NS_FAILED(rv)) {
            LOG(("OnStartRequest failed [rv=%" PRIx32 "] canceling request!\n",
                 static_cast<uint32_t>(rv)));
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
    RefPtr<nsRequestObserverProxy> mProxy;
public:
    nsOnStopRequestEvent(nsRequestObserverProxy *proxy,
                         nsIRequest *request)
        : nsARequestObserverEvent(request)
        , mProxy(proxy)
    {
        NS_PRECONDITION(mProxy, "null pointer");
    }

    virtual ~nsOnStopRequestEvent() {}

    NS_IMETHOD Run() override
    {
        LOG(("nsOnStopRequestEvent::HandleEvent [req=%p]\n", mRequest.get()));

        nsMainThreadPtrHandle<nsIRequestObserver> observer = mProxy->mObserver;
        if (!observer) {
            NS_NOTREACHED("already handled onStopRequest event (observer is null)");
            return NS_OK;
        }
        // Do not allow any more events to be handled after OnStopRequest
        mProxy->mObserver = 0;

        nsresult status = NS_OK;
        DebugOnly<nsresult> rv = mRequest->GetStatus(&status);
        NS_ASSERTION(NS_SUCCEEDED(rv), "GetStatus failed for request!");

        LOG(("handle stopevent=%p\n", this));
        (void) observer->OnStopRequest(mRequest, mProxy->mContext, status);

        return NS_OK;
    }
};

//-----------------------------------------------------------------------------
// nsRequestObserverProxy::nsISupports implementation...
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(nsRequestObserverProxy,
                  nsIRequestObserver,
                  nsIRequestObserverProxy)

//-----------------------------------------------------------------------------
// nsRequestObserverProxy::nsIRequestObserver implementation...
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsRequestObserverProxy::OnStartRequest(nsIRequest *request,
                                       nsISupports *context)
{
    MOZ_ASSERT(!context || context == mContext);
    LOG(("nsRequestObserverProxy::OnStartRequest [this=%p req=%p]\n", this, request));

    nsOnStartRequestEvent *ev =
        new nsOnStartRequestEvent(this, request);
    if (!ev)
        return NS_ERROR_OUT_OF_MEMORY;

    LOG(("post startevent=%p\n", ev));
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
    MOZ_ASSERT(!context || context == mContext);
    LOG(("nsRequestObserverProxy: OnStopRequest [this=%p req=%p status=%" PRIx32 "]\n",
         this, request, static_cast<uint32_t>(status)));

    // The status argument is ignored because, by the time the OnStopRequestEvent
    // is actually processed, the status of the request may have changed :-(
    // To make sure that an accurate status code is always used, GetStatus() is
    // called when the OnStopRequestEvent is actually processed (see above).

    nsOnStopRequestEvent *ev =
        new nsOnStopRequestEvent(this, request);
    if (!ev)
        return NS_ERROR_OUT_OF_MEMORY;

    LOG(("post stopevent=%p\n", ev));
    nsresult rv = FireEvent(ev);
    if (NS_FAILED(rv))
        delete ev;
    return rv;
}

//-----------------------------------------------------------------------------
// nsRequestObserverProxy::nsIRequestObserverProxy implementation...
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsRequestObserverProxy::Init(nsIRequestObserver *observer, nsISupports *context)
{
    NS_ENSURE_ARG_POINTER(observer);
    mObserver = new nsMainThreadPtrHolder<nsIRequestObserver>(
      "nsRequestObserverProxy::mObserver", observer);
    mContext = new nsMainThreadPtrHolder<nsISupports>(
      "nsRequestObserverProxy::mContext", context);

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsRequestObserverProxy implementation...
//-----------------------------------------------------------------------------

nsresult
nsRequestObserverProxy::FireEvent(nsARequestObserverEvent *event)
{
    nsCOMPtr<nsIEventTarget> mainThread(GetMainThreadEventTarget());
    return mainThread->Dispatch(event, NS_DISPATCH_NORMAL);
}

} // namespace net
} // namespace mozilla
