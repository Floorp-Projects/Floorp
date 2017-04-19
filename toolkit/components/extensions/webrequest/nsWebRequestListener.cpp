/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"
#include "nsWebRequestListener.h"

#ifdef DEBUG
#include "MainThreadUtils.h"
#endif

using namespace mozilla;

NS_IMPL_ISUPPORTS(nsWebRequestListener,
                  nsIWebRequestListener,
                  nsIStreamListener,
                  nsIRequestObserver,
                  nsIThreadRetargetableStreamListener)

NS_IMETHODIMP
nsWebRequestListener::Init(nsIStreamListener *aStreamListener, nsITraceableChannel *aTraceableChannel)
{
  MOZ_ASSERT(aStreamListener, "Should have aStreamListener");
  MOZ_ASSERT(aTraceableChannel, "Should have aTraceableChannel");
  mTargetStreamListener = aStreamListener;
  return aTraceableChannel->SetNewListener(this, getter_AddRefs(mOrigStreamListener));
}

NS_IMETHODIMP
nsWebRequestListener::OnStartRequest(nsIRequest *request, nsISupports * aCtxt)
{
  MOZ_ASSERT(mTargetStreamListener, "Should have mTargetStreamListener");
  MOZ_ASSERT(mOrigStreamListener, "Should have mOrigStreamListener");

  nsresult rv = mTargetStreamListener->OnStartRequest(request, aCtxt);
  NS_ENSURE_SUCCESS(rv, rv);

  return mOrigStreamListener->OnStartRequest(request, aCtxt);
}

NS_IMETHODIMP
nsWebRequestListener::OnStopRequest(nsIRequest *request, nsISupports *aCtxt,
                                           nsresult aStatus)
{
  MOZ_ASSERT(mOrigStreamListener, "Should have mOrigStreamListener");
  MOZ_ASSERT(mTargetStreamListener, "Should have mTargetStreamListener");

  nsresult rv = mOrigStreamListener->OnStopRequest(request, aCtxt, aStatus);
  NS_ENSURE_SUCCESS(rv, rv);

  return mTargetStreamListener->OnStopRequest(request, aCtxt, aStatus);
}

NS_IMETHODIMP
nsWebRequestListener::OnDataAvailable(nsIRequest *request, nsISupports * aCtxt,
                                             nsIInputStream * inStr,
                                             uint64_t sourceOffset, uint32_t count)
{
  MOZ_ASSERT(mOrigStreamListener, "Should have mOrigStreamListener");
  return mOrigStreamListener->OnDataAvailable(request, aCtxt, inStr, sourceOffset, count);
}

NS_IMETHODIMP
nsWebRequestListener::CheckListenerChain()
{
    MOZ_ASSERT(NS_IsMainThread(), "Should be on main thread!");
    nsresult rv;
    nsCOMPtr<nsIThreadRetargetableStreamListener> retargetableListener =
        do_QueryInterface(mOrigStreamListener, &rv);
    if (retargetableListener) {
        return retargetableListener->CheckListenerChain();
    }
    return rv;
}
