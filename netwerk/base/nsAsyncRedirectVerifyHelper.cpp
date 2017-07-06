/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Logging.h"
#include "nsAsyncRedirectVerifyHelper.h"
#include "nsThreadUtils.h"
#include "nsNetUtil.h"

#include "nsIOService.h"
#include "nsIChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsILoadInfo.h"

namespace mozilla {
namespace net {

static LazyLogModule gRedirectLog("nsRedirect");
#undef LOG
#define LOG(args) MOZ_LOG(gRedirectLog, LogLevel::Debug, args)

NS_IMPL_ISUPPORTS(nsAsyncRedirectVerifyHelper,
                  nsIAsyncVerifyRedirectCallback,
                  nsIRunnable,
                  nsINamed)

class nsAsyncVerifyRedirectCallbackEvent : public Runnable {
public:
    nsAsyncVerifyRedirectCallbackEvent(nsIAsyncVerifyRedirectCallback *cb,
                                       nsresult result)
        : Runnable("nsAsyncVerifyRedirectCallbackEvent")
        , mCallback(cb)
        , mResult(result) {}

    NS_IMETHOD Run() override
    {
        LOG(("nsAsyncVerifyRedirectCallbackEvent::Run() "
             "callback to %p with result %" PRIx32,
             mCallback.get(), static_cast<uint32_t>(mResult)));
       (void) mCallback->OnRedirectVerifyCallback(mResult);
       return NS_OK;
    }
private:
    nsCOMPtr<nsIAsyncVerifyRedirectCallback> mCallback;
    nsresult mResult;
};

nsAsyncRedirectVerifyHelper::nsAsyncRedirectVerifyHelper()
    : mFlags(0),
      mWaitingForRedirectCallback(false),
      mCallbackInitiated(false),
      mExpectedCallbacks(0),
      mResult(NS_OK)
{
}

nsAsyncRedirectVerifyHelper::~nsAsyncRedirectVerifyHelper()
{
    NS_ASSERTION(NS_FAILED(mResult) || mExpectedCallbacks == 0,
                 "Did not receive all required callbacks!");
}

nsresult
nsAsyncRedirectVerifyHelper::Init(nsIChannel* oldChan,
                                  nsIChannel* newChan,
                                  uint32_t flags,
                                  nsIEventTarget* mainThreadEventTarget,
                                  bool synchronize)
{
    LOG(("nsAsyncRedirectVerifyHelper::Init() "
         "oldChan=%p newChan=%p", oldChan, newChan));
    mOldChan           = oldChan;
    mNewChan           = newChan;
    mFlags             = flags;
    mCallbackEventTarget = NS_IsMainThread() && mainThreadEventTarget
      ? mainThreadEventTarget
      : GetCurrentThreadEventTarget();

    if (!(flags & (nsIChannelEventSink::REDIRECT_INTERNAL |
                   nsIChannelEventSink::REDIRECT_STS_UPGRADE))) {
      nsCOMPtr<nsILoadInfo> loadInfo = oldChan->GetLoadInfo();
      if (loadInfo && loadInfo->GetDontFollowRedirects()) {
        ExplicitCallback(NS_BINDING_ABORTED);
        return NS_OK;
      }
    }

    if (synchronize)
      mWaitingForRedirectCallback = true;

    nsCOMPtr<nsIRunnable> runnable = this;
    nsresult rv;
    rv = mainThreadEventTarget
      ? mainThreadEventTarget->Dispatch(runnable.forget())
      : GetMainThreadEventTarget()->Dispatch(runnable.forget());
    NS_ENSURE_SUCCESS(rv, rv);

    if (synchronize) {
      if (!SpinEventLoopUntil([&]() { return !mWaitingForRedirectCallback; })) {
        return NS_ERROR_UNEXPECTED;
      }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsAsyncRedirectVerifyHelper::OnRedirectVerifyCallback(nsresult result)
{
    LOG(("nsAsyncRedirectVerifyHelper::OnRedirectVerifyCallback() "
         "result=%" PRIx32 " expectedCBs=%u mResult=%" PRIx32,
         static_cast<uint32_t>(result), mExpectedCallbacks,
         static_cast<uint32_t>(mResult)));

    MOZ_DIAGNOSTIC_ASSERT(mExpectedCallbacks > 0,
                          "OnRedirectVerifyCallback called more times than expected");
    if (mExpectedCallbacks <= 0) {
      return NS_ERROR_UNEXPECTED;
    }

    --mExpectedCallbacks;

    // If response indicates failure we may call back immediately
    if (NS_FAILED(result)) {
        // We chose to store the first failure-value (as opposed to the last)
        if (NS_SUCCEEDED(mResult))
            mResult = result;

        // If InitCallback() has been called, just invoke the callback and
        // return. Otherwise it will be invoked from InitCallback()
        if (mCallbackInitiated) {
            ExplicitCallback(mResult);
            return NS_OK;
        }
    }

    // If the expected-counter is in balance and InitCallback() was called, all
    // sinks have agreed that the redirect is ok and we can invoke our callback
    if (mCallbackInitiated && mExpectedCallbacks == 0) {
        ExplicitCallback(mResult);
    }

    return NS_OK;
}

nsresult
nsAsyncRedirectVerifyHelper::DelegateOnChannelRedirect(nsIChannelEventSink *sink,
                                                       nsIChannel *oldChannel,
                                                       nsIChannel *newChannel,
                                                       uint32_t flags)
{
    LOG(("nsAsyncRedirectVerifyHelper::DelegateOnChannelRedirect() "
         "sink=%p expectedCBs=%u mResult=%" PRIx32,
         sink, mExpectedCallbacks, static_cast<uint32_t>(mResult)));

    ++mExpectedCallbacks;

    if (IsOldChannelCanceled()) {
        LOG(("  old channel has been canceled, cancel the redirect by "
             "emulating OnRedirectVerifyCallback..."));
        (void) OnRedirectVerifyCallback(NS_BINDING_ABORTED);
        return NS_BINDING_ABORTED;
    }

    nsresult rv =
        sink->AsyncOnChannelRedirect(oldChannel, newChannel, flags, this);

    LOG(("  result=%" PRIx32 " expectedCBs=%u", static_cast<uint32_t>(rv), mExpectedCallbacks));

    // If the sink returns failure from this call the redirect is vetoed. We
    // emulate a callback from the sink in this case in order to perform all
    // the necessary logic.
    if (NS_FAILED(rv)) {
        LOG(("  emulating OnRedirectVerifyCallback..."));
        (void) OnRedirectVerifyCallback(rv);
    }

    return rv;  // Return the actual status since our caller may need it
}

void
nsAsyncRedirectVerifyHelper::ExplicitCallback(nsresult result)
{
    LOG(("nsAsyncRedirectVerifyHelper::ExplicitCallback() "
         "result=%" PRIx32 " expectedCBs=%u mCallbackInitiated=%u mResult=%"  PRIx32,
         static_cast<uint32_t>(result), mExpectedCallbacks, mCallbackInitiated,
         static_cast<uint32_t>(mResult)));

    nsCOMPtr<nsIAsyncVerifyRedirectCallback>
        callback(do_QueryInterface(mOldChan));

    if (!callback || !mCallbackEventTarget) {
        LOG(("nsAsyncRedirectVerifyHelper::ExplicitCallback() "
             "callback=%p mCallbackEventTarget=%p", callback.get(), mCallbackEventTarget.get()));
        return;
    }

    mCallbackInitiated = false;  // reset to ensure only one callback
    mWaitingForRedirectCallback = false;

    // Now, dispatch the callback on the event-target which called Init()
    nsCOMPtr<nsIRunnable> event =
        new nsAsyncVerifyRedirectCallbackEvent(callback, result);
    if (!event) {
        NS_WARNING("nsAsyncRedirectVerifyHelper::ExplicitCallback() "
                   "failed creating callback event!");
        return;
    }
    nsresult rv = mCallbackEventTarget->Dispatch(event, NS_DISPATCH_NORMAL);
    if (NS_FAILED(rv)) {
        NS_WARNING("nsAsyncRedirectVerifyHelper::ExplicitCallback() "
                   "failed dispatching callback event!");
    } else {
        LOG(("nsAsyncRedirectVerifyHelper::ExplicitCallback() "
             "dispatched callback event=%p", event.get()));
    }

}

void
nsAsyncRedirectVerifyHelper::InitCallback()
{
    LOG(("nsAsyncRedirectVerifyHelper::InitCallback() "
         "expectedCBs=%d mResult=%" PRIx32, mExpectedCallbacks,
         static_cast<uint32_t>(mResult)));

    mCallbackInitiated = true;

    // Invoke the callback if we are done
    if (mExpectedCallbacks == 0)
        ExplicitCallback(mResult);
}

NS_IMETHODIMP
nsAsyncRedirectVerifyHelper::GetName(nsACString& aName)
{
    aName.AssignASCII("nsAsyncRedirectVerifyHelper");
    return NS_OK;
}

NS_IMETHODIMP
nsAsyncRedirectVerifyHelper::SetName(const char* aName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsAsyncRedirectVerifyHelper::Run()
{
    /* If the channel got canceled after it fired AsyncOnChannelRedirect
     * and before we got here, mostly because docloader load has been canceled,
     * we must completely ignore this notification and prevent any further
     * notification.
     */
    if (IsOldChannelCanceled()) {
        ExplicitCallback(NS_BINDING_ABORTED);
        return NS_OK;
    }

    // First, the global observer
    NS_ASSERTION(gIOService, "Must have an IO service at this point");
    LOG(("nsAsyncRedirectVerifyHelper::Run() calling gIOService..."));
    nsresult rv = gIOService->AsyncOnChannelRedirect(mOldChan, mNewChan,
                                                     mFlags, this);
    if (NS_FAILED(rv)) {
        ExplicitCallback(rv);
        return NS_OK;
    }

    // Now, the per-channel observers
    nsCOMPtr<nsIChannelEventSink> sink;
    NS_QueryNotificationCallbacks(mOldChan, sink);
    if (sink) {
        LOG(("nsAsyncRedirectVerifyHelper::Run() calling sink..."));
        rv = DelegateOnChannelRedirect(sink, mOldChan, mNewChan, mFlags);
    }

    // All invocations to AsyncOnChannelRedirect has been done - call
    // InitCallback() to flag this
    InitCallback();
    return NS_OK;
}

bool
nsAsyncRedirectVerifyHelper::IsOldChannelCanceled()
{
    bool canceled;
    nsCOMPtr<nsIHttpChannelInternal> oldChannelInternal =
        do_QueryInterface(mOldChan);
    if (oldChannelInternal) {
        nsresult rv = oldChannelInternal->GetCanceled(&canceled);
        if (NS_SUCCEEDED(rv) && canceled) {
            return true;
        }
    } else if (mOldChan) {
        // For non-HTTP channels check on the status, failure
        // indicates the channel has probably been canceled.
        nsresult status = NS_ERROR_FAILURE;
        mOldChan->GetStatus(&status);
        if (NS_FAILED(status)) {
            return true;
        }
    }

    return false;
}

} // namespace net
} // namespace mozilla
