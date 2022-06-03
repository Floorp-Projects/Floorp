/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteStreamGetter.h"

#include "mozilla/net/NeckoChild.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ResultExtensions.h"
#include "nsContentUtils.h"
#include "nsIInputStreamPump.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(RemoteStreamGetter, nsICancelable)

RemoteStreamGetter::RemoteStreamGetter(nsIURI* aURI, nsILoadInfo* aLoadInfo)
    : mURI(aURI), mLoadInfo(aLoadInfo) {
  MOZ_ASSERT(aURI);
  MOZ_ASSERT(aLoadInfo);
}

// Request an input stream from the parent.
RequestOrReason RemoteStreamGetter::GetAsync(nsIStreamListener* aListener,
                                             nsIChannel* aChannel) {
  MOZ_ASSERT(IsNeckoChild());

  mListener = aListener;
  mChannel = aChannel;

  nsCOMPtr<nsICancelable> cancelableRequest(this);

  RefPtr<RemoteStreamGetter> self = this;

  // Request an input stream for this moz-page-thumb URI.
  gNeckoChild->SendGetPageThumbStream(mURI)->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [self](const RefPtr<nsIInputStream>& stream) {
        self->OnStream(do_AddRef(stream));
      },
      [self](const mozilla::ipc::ResponseRejectReason) {
        self->OnStream(nullptr);
      });
  return RequestOrCancelable(WrapNotNull(cancelableRequest));
}

// Called to cancel the ongoing async request.
NS_IMETHODIMP
RemoteStreamGetter::Cancel(nsresult aStatus) {
  if (mCanceled) {
    return NS_OK;
  }

  mCanceled = true;
  mStatus = aStatus;

  if (mPump) {
    mPump->Cancel(aStatus);
    mPump = nullptr;
  }

  return NS_OK;
}

// static
void RemoteStreamGetter::CancelRequest(nsIStreamListener* aListener,
                                       nsIChannel* aChannel, nsresult aResult) {
  MOZ_ASSERT(aListener);
  MOZ_ASSERT(aChannel);

  aListener->OnStartRequest(aChannel);
  aListener->OnStopRequest(aChannel, aResult);
  aChannel->Cancel(NS_BINDING_ABORTED);
}

// Handle an input stream sent from the parent.
void RemoteStreamGetter::OnStream(already_AddRefed<nsIInputStream> aStream) {
  MOZ_ASSERT(IsNeckoChild());
  MOZ_ASSERT(mChannel);
  MOZ_ASSERT(mListener);

  nsCOMPtr<nsIInputStream> stream = std::move(aStream);
  nsCOMPtr<nsIChannel> channel = std::move(mChannel);

  // We must keep an owning reference to the listener until we pass it on
  // to AsyncRead.
  nsCOMPtr<nsIStreamListener> listener = mListener.forget();

  if (mCanceled) {
    // The channel that has created this stream getter has been canceled.
    CancelRequest(listener, channel, mStatus);
    return;
  }

  if (!stream) {
    // The parent didn't send us back a stream.
    CancelRequest(listener, channel, NS_ERROR_FILE_ACCESS_DENIED);
    return;
  }

  nsCOMPtr<nsIInputStreamPump> pump;
  nsresult rv =
      NS_NewInputStreamPump(getter_AddRefs(pump), stream.forget(), 0, 0, false,
                            GetMainThreadSerialEventTarget());
  if (NS_FAILED(rv)) {
    CancelRequest(listener, channel, rv);
    return;
  }

  rv = pump->AsyncRead(listener);
  if (NS_FAILED(rv)) {
    CancelRequest(listener, channel, rv);
    return;
  }

  mPump = pump;
}

}  // namespace net
}  // namespace mozilla
