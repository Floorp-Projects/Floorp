/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RemoteStreamGetter_h___
#define RemoteStreamGetter_h___

#include "nsIChannel.h"
#include "nsIInputStreamPump.h"
#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsICancelable.h"
#include "SimpleChannel.h"
#include "mozilla/net/NeckoChannelParams.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/Maybe.h"

class nsILoadInfo;

namespace mozilla {
namespace net {

using RemoteStreamPromise =
    mozilla::MozPromise<RemoteStreamInfo, nsresult, false>;
using Method = RefPtr<
    MozPromise<Maybe<RemoteStreamInfo>, ipc::ResponseRejectReason, true>> (
    PNeckoChild::*)(nsIURI*, const LoadInfoArgs&);

/**
 * Helper class used with SimpleChannel to asynchronously obtain an input
 * stream and metadata from the parent for a remote protocol load from the
 * child.
 */
class RemoteStreamGetter final : public nsICancelable {
  NS_DECL_ISUPPORTS
  NS_DECL_NSICANCELABLE

 public:
  RemoteStreamGetter(nsIURI* aURI, nsILoadInfo* aLoadInfo);

  // Get an input stream from the parent asynchronously.
  RequestOrReason GetAsync(nsIStreamListener* aListener, nsIChannel* aChannel,
                           Method aMethod);

  // Handle an input stream being returned from the parent
  void OnStream(const Maybe<RemoteStreamInfo>& aStreamInfo);

  static void CancelRequest(nsIStreamListener* aListener, nsIChannel* aChannel,
                            nsresult aResult);

 private:
  ~RemoteStreamGetter() = default;

  nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsILoadInfo> mLoadInfo;
  nsCOMPtr<nsIStreamListener> mListener;
  nsCOMPtr<nsIChannel> mChannel;
  nsCOMPtr<nsIInputStreamPump> mPump;
  bool mCanceled{false};
  nsresult mStatus{NS_OK};
};

}  // namespace net
}  // namespace mozilla

#endif /* RemoteStreamGetter_h___ */
