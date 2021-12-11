/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "nscore.h"
#include "nsRequestObserverProxy.h"
#include "nsIRequest.h"
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
    : Runnable("net::nsARequestObserverEvent"), mRequest(request) {
  MOZ_ASSERT(mRequest, "null pointer");
}

//-----------------------------------------------------------------------------
// nsOnStartRequestEvent internal class...
//-----------------------------------------------------------------------------

class nsOnStartRequestEvent : public nsARequestObserverEvent {
  RefPtr<nsRequestObserverProxy> mProxy;

 public:
  nsOnStartRequestEvent(nsRequestObserverProxy* proxy, nsIRequest* request)
      : nsARequestObserverEvent(request), mProxy(proxy) {
    MOZ_ASSERT(mProxy, "null pointer");
  }

  NS_IMETHOD Run() override {
    LOG(("nsOnStartRequestEvent::HandleEvent [req=%p]\n", mRequest.get()));

    if (!mProxy->mObserver) {
      MOZ_ASSERT_UNREACHABLE(
          "already handled onStopRequest event "
          "(observer is null)");
      return NS_OK;
    }

    LOG(("handle startevent=%p\n", this));
    nsresult rv = mProxy->mObserver->OnStartRequest(mRequest);
    if (NS_FAILED(rv)) {
      LOG(("OnStartRequest failed [rv=%" PRIx32 "] canceling request!\n",
           static_cast<uint32_t>(rv)));
      rv = mRequest->Cancel(rv);
      NS_ASSERTION(NS_SUCCEEDED(rv), "Cancel failed for request!");
    }

    return NS_OK;
  }

 private:
  virtual ~nsOnStartRequestEvent() = default;
};

//-----------------------------------------------------------------------------
// nsOnStopRequestEvent internal class...
//-----------------------------------------------------------------------------

class nsOnStopRequestEvent : public nsARequestObserverEvent {
  RefPtr<nsRequestObserverProxy> mProxy;

 public:
  nsOnStopRequestEvent(nsRequestObserverProxy* proxy, nsIRequest* request)
      : nsARequestObserverEvent(request), mProxy(proxy) {
    MOZ_ASSERT(mProxy, "null pointer");
  }

  NS_IMETHOD Run() override {
    LOG(("nsOnStopRequestEvent::HandleEvent [req=%p]\n", mRequest.get()));

    nsMainThreadPtrHandle<nsIRequestObserver> observer = mProxy->mObserver;
    if (!observer) {
      MOZ_ASSERT_UNREACHABLE(
          "already handled onStopRequest event "
          "(observer is null)");
      return NS_OK;
    }
    // Do not allow any more events to be handled after OnStopRequest
    mProxy->mObserver = nullptr;

    nsresult status = NS_OK;
    DebugOnly<nsresult> rv = mRequest->GetStatus(&status);
    NS_ASSERTION(NS_SUCCEEDED(rv), "GetStatus failed for request!");

    LOG(("handle stopevent=%p\n", this));
    (void)observer->OnStopRequest(mRequest, status);

    return NS_OK;
  }

 private:
  virtual ~nsOnStopRequestEvent() = default;
};

//-----------------------------------------------------------------------------
// nsRequestObserverProxy::nsISupports implementation...
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(nsRequestObserverProxy, nsIRequestObserver,
                  nsIRequestObserverProxy)

//-----------------------------------------------------------------------------
// nsRequestObserverProxy::nsIRequestObserver implementation...
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsRequestObserverProxy::OnStartRequest(nsIRequest* request) {
  LOG(("nsRequestObserverProxy::OnStartRequest [this=%p req=%p]\n", this,
       request));

  RefPtr<nsOnStartRequestEvent> ev = new nsOnStartRequestEvent(this, request);

  LOG(("post startevent=%p\n", ev.get()));
  return FireEvent(ev);
}

NS_IMETHODIMP
nsRequestObserverProxy::OnStopRequest(nsIRequest* request, nsresult status) {
  LOG(("nsRequestObserverProxy: OnStopRequest [this=%p req=%p status=%" PRIx32
       "]\n",
       this, request, static_cast<uint32_t>(status)));

  // The status argument is ignored because, by the time the OnStopRequestEvent
  // is actually processed, the status of the request may have changed :-(
  // To make sure that an accurate status code is always used, GetStatus() is
  // called when the OnStopRequestEvent is actually processed (see above).

  RefPtr<nsOnStopRequestEvent> ev = new nsOnStopRequestEvent(this, request);

  LOG(("post stopevent=%p\n", ev.get()));
  return FireEvent(ev);
}

//-----------------------------------------------------------------------------
// nsRequestObserverProxy::nsIRequestObserverProxy implementation...
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsRequestObserverProxy::Init(nsIRequestObserver* observer,
                             nsISupports* context) {
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

nsresult nsRequestObserverProxy::FireEvent(nsARequestObserverEvent* event) {
  nsCOMPtr<nsIEventTarget> mainThread(GetMainThreadEventTarget());
  return mainThread->Dispatch(event, NS_DISPATCH_NORMAL);
}

}  // namespace net
}  // namespace mozilla
