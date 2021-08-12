/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHtml5StreamListener.h"

NS_IMPL_ADDREF(nsHtml5StreamListener)
NS_IMPL_RELEASE(nsHtml5StreamListener)

NS_INTERFACE_MAP_BEGIN(nsHtml5StreamListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIRequestObserver)
  NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
  NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
  NS_INTERFACE_MAP_ENTRY(nsIThreadRetargetableStreamListener)
NS_INTERFACE_MAP_END

nsHtml5StreamListener::nsHtml5StreamListener(nsHtml5StreamParser* aDelegate)
    : mDelegate(aDelegate) {}

nsHtml5StreamListener::~nsHtml5StreamListener() {}

void nsHtml5StreamListener::DropDelegate() {
  MOZ_ASSERT(NS_IsMainThread(),
             "Must not call DropDelegate from non-main threads.");
  mDelegate = nullptr;
}

NS_IMETHODIMP
nsHtml5StreamListener::CheckListenerChain() {
  if (MOZ_UNLIKELY(!mDelegate)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHtml5StreamListener::OnStartRequest(nsIRequest* aRequest) {
  if (MOZ_UNLIKELY(!mDelegate)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return mDelegate->OnStartRequest(aRequest);
}

NS_IMETHODIMP
nsHtml5StreamListener::OnStopRequest(nsIRequest* aRequest, nsresult aStatus) {
  if (MOZ_UNLIKELY(!mDelegate)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return mDelegate->OnStopRequest(aRequest, aStatus);
}

NS_IMETHODIMP
nsHtml5StreamListener::OnDataAvailable(nsIRequest* aRequest,
                                       nsIInputStream* aInStream,
                                       uint64_t aSourceOffset,
                                       uint32_t aLength) {
  if (MOZ_UNLIKELY(!mDelegate)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return mDelegate->OnDataAvailable(aRequest, aInStream, aSourceOffset,
                                    aLength);
}
