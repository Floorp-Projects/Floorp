/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prlog.h"
#include "nsAsyncRedirectVerifyHelper.h"
#include "nsThreadUtils.h"
#include "nsNetUtil.h"

#include "nsIOService.h"
#include "nsIChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIAsyncVerifyRedirectCallback.h"

#undef LOG
static PRLogModuleInfo *
GetRedirectLog()
{
    static PRLogModuleInfo *sLog;
    if (!sLog)
        sLog = PR_NewLogModule("nsRedirect");
    return sLog;
}
#define LOG(args) PR_LOG(GetRedirectLog(), PR_LOG_DEBUG, args)

NS_IMPL_ISUPPORTS(nsAsyncRedirectVerifyHelper,
                  nsIAsyncVerifyRedirectCallback,
                  nsIRunnable)

class nsAsyncVerifyRedirectCallbackEvent : public nsRunnable {
public:
    nsAsyncVerifyRedirectCallbackEvent(nsIAsyncVerifyRedirectCallback *cb,
                                       nsresult result)
        : mCallback(cb), mResult(result) {
    }

    NS_IMETHOD Run()
    {
        LOG(("nsAsyncVerifyRedirectCallbackEvent::Run() "
             "callback to %p with result %x",
             mCallback.get(), mResult));
       (void) mCallback->OnRedirectVerifyCallback(mResult);
       return NS_OK;
    }
private:
    nsCOMPtr<nsIAsyncVerifyRedirectCallback> mCallback;
    nsresult mResult;
};

nsAsyncRedirectVerifyHelper::nsAsyncRedirectVerifyHelper()
    : mCallbackInitiated(false),
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
nsAsyncRedirectVerifyHelper::Init(nsIChannel* oldChan, nsIChannel* newChan,
                                  uint32_t flags, bool synchronize)
{
    LOG(("nsAsyncRedirectVerifyHelper::Init() "
         "oldChan=%p newChan=%p", oldChan, newChan));
    mOldChan           = oldChan;
    mNewChan           = newChan;
    mFlags             = flags;
    mCallbackThread    = do_GetCurrentThread();

    if (synchronize)
      mWaitingForRedirectCallback = true;

    nsresult rv;
    rv = NS_DispatchToMainThread(this);
    NS_ENSURE_SUCCESS(rv, rv);

    if (synchronize) {
      nsIThread *thread = NS_GetCurrentThread();
      while (mWaitingForRedirectCallback) {
        if (!NS_ProcessNextEvent(thread)) {
          return NS_ERROR_UNEXPECTED;
        }
      }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsAsyncRedirectVerifyHelper::OnRedirectVerifyCallback(nsresult result)
{
    LOG(("nsAsyncRedirectVerifyHelper::OnRedirectVerifyCallback() "
         "result=%x expectedCBs=%u mResult=%x",
         result, mExpectedCallbacks, mResult));

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
         "sink=%p expectedCBs=%u mResult=%x",
         sink, mExpectedCallbacks, mResult));

    ++mExpectedCallbacks;

    if (IsOldChannelCanceled()) {
        LOG(("  old channel has been canceled, cancel the redirect by "
             "emulating OnRedirectVerifyCallback..."));
        (void) OnRedirectVerifyCallback(NS_BINDING_ABORTED);
        return NS_BINDING_ABORTED;
    }

    nsresult rv =
        sink->AsyncOnChannelRedirect(oldChannel, newChannel, flags, this);

    LOG(("  result=%x expectedCBs=%u", rv, mExpectedCallbacks));

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
         "result=%x expectedCBs=%u mCallbackInitiated=%u mResult=%x",
         result, mExpectedCallbacks, mCallbackInitiated, mResult));

    nsCOMPtr<nsIAsyncVerifyRedirectCallback>
        callback(do_QueryInterface(mOldChan));

    if (!callback || !mCallbackThread) {
        LOG(("nsAsyncRedirectVerifyHelper::ExplicitCallback() "
             "callback=%p mCallbackThread=%p", callback.get(), mCallbackThread.get()));
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
    nsresult rv = mCallbackThread->Dispatch(event, NS_DISPATCH_NORMAL);
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
         "expectedCBs=%d mResult=%x", mExpectedCallbacks, mResult));

    mCallbackInitiated = true;

    // Invoke the callback if we are done
    if (mExpectedCallbacks == 0)
        ExplicitCallback(mResult);
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
        oldChannelInternal->GetCanceled(&canceled);
        if (canceled) {
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
