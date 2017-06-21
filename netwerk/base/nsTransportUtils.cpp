/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Mutex.h"
#include "nsTransportUtils.h"
#include "nsITransport.h"
#include "nsProxyRelease.h"
#include "nsThreadUtils.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"

using namespace mozilla;

//-----------------------------------------------------------------------------

class nsTransportStatusEvent;

class nsTransportEventSinkProxy : public nsITransportEventSink
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSITRANSPORTEVENTSINK

    nsTransportEventSinkProxy(nsITransportEventSink *sink,
                              nsIEventTarget *target)
        : mSink(sink)
        , mTarget(target)
        , mLock("nsTransportEventSinkProxy.mLock")
        , mLastEvent(nullptr)
    {
        NS_ADDREF(mSink);
    }

private:
    virtual ~nsTransportEventSinkProxy()
    {
        // our reference to mSink could be the last, so be sure to release
        // it on the target thread.  otherwise, we could get into trouble.
        NS_ProxyRelease(
          "nsTransportEventSinkProxy::mSink", mTarget, dont_AddRef(mSink));
    }

public:
    nsITransportEventSink           *mSink;
    nsCOMPtr<nsIEventTarget>         mTarget;
    Mutex                            mLock;
    nsTransportStatusEvent          *mLastEvent;
};

class nsTransportStatusEvent : public Runnable
{
public:
    nsTransportStatusEvent(nsTransportEventSinkProxy *proxy,
                           nsITransport *transport,
                           nsresult status,
                           int64_t progress,
                           int64_t progressMax)
        : Runnable("nsTransportStatusEvent")
        , mProxy(proxy)
        , mTransport(transport)
        , mStatus(status)
        , mProgress(progress)
        , mProgressMax(progressMax)
    {}

    ~nsTransportStatusEvent() {}

    NS_IMETHOD Run() override
    {
        // since this event is being handled, we need to clear the proxy's ref.
        // if not coalescing all, then last event may not equal self!
        {
            MutexAutoLock lock(mProxy->mLock);
            if (mProxy->mLastEvent == this)
                mProxy->mLastEvent = nullptr;
        }

        mProxy->mSink->OnTransportStatus(mTransport, mStatus, mProgress,
                                         mProgressMax);
        return NS_OK;
    }

    RefPtr<nsTransportEventSinkProxy> mProxy;

    // parameters to OnTransportStatus
    nsCOMPtr<nsITransport> mTransport;
    nsresult               mStatus;
    int64_t                mProgress;
    int64_t                mProgressMax;
};

NS_IMPL_ISUPPORTS(nsTransportEventSinkProxy, nsITransportEventSink)

NS_IMETHODIMP
nsTransportEventSinkProxy::OnTransportStatus(nsITransport *transport,
                                             nsresult status,
                                             int64_t progress,
                                             int64_t progressMax)
{
    nsresult rv = NS_OK;
    RefPtr<nsTransportStatusEvent> event;
    {
        MutexAutoLock lock(mLock);

        // try to coalesce events! ;-)
        if (mLastEvent && (mLastEvent->mStatus == status)) {
            mLastEvent->mStatus = status;
            mLastEvent->mProgress = progress;
            mLastEvent->mProgressMax = progressMax;
        }
        else {
            event = new nsTransportStatusEvent(this, transport, status,
                                               progress, progressMax);
            if (!event)
                rv = NS_ERROR_OUT_OF_MEMORY;
            mLastEvent = event;  // weak ref
        }
    }
    if (event) {
        rv = mTarget->Dispatch(event, NS_DISPATCH_NORMAL);
        if (NS_FAILED(rv)) {
            NS_WARNING("unable to post transport status event");

            MutexAutoLock lock(mLock); // cleanup.. don't reference anymore!
            mLastEvent = nullptr;
        }
    }
    return rv;
}

//-----------------------------------------------------------------------------

nsresult
net_NewTransportEventSinkProxy(nsITransportEventSink **result,
                               nsITransportEventSink *sink,
                               nsIEventTarget *target)
{
    *result = new nsTransportEventSinkProxy(sink, target);
    if (!*result)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(*result);
    return NS_OK;
}
