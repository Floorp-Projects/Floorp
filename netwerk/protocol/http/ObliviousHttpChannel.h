/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_ObliviousHttpChannel_h
#define mozilla_net_ObliviousHttpChannel_h

#include "nsHashtablesFwd.h"
#include "nsIBinaryHttp.h"
#include "nsIHttpChannel.h"
#include "nsIObliviousHttp.h"
#include "nsIUploadChannel2.h"

namespace mozilla::net {

class ObliviousHttpChannel final : public nsIHttpChannel,
                                   public nsIStreamListener,
                                   public nsIUploadChannel2 {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICHANNEL
  NS_DECL_NSIHTTPCHANNEL
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIUPLOADCHANNEL2

  ObliviousHttpChannel(nsIURI* targetURI,
                       const nsTArray<uint8_t>& encodedConfig,
                       nsIHttpChannel* innerChannel);

  NS_FORWARD_NSIREQUEST(mInnerChannel->)
  NS_FORWARD_NSIIDENTCHANNEL(mInnerChannel->)

 protected:
  ~ObliviousHttpChannel();

  nsCOMPtr<nsIURI> mTargetURI;
  nsTArray<uint8_t> mEncodedConfig;

  nsCString mMethod{"GET"_ns};
  nsTHashMap<nsCStringHashKey, nsCString> mHeaders;
  nsTArray<uint8_t> mContent;

  nsCOMPtr<nsIHttpChannel> mInnerChannel;
  nsCOMPtr<nsIObliviousHttpClientRequest> mEncapsulatedRequest;
  nsTArray<uint8_t> mRawResponse;
  nsCOMPtr<nsIBinaryHttpResponse> mBinaryHttpResponse;

  nsCOMPtr<nsIStreamListener> mStreamListener;
};

}  // namespace mozilla::net

#endif  // mozilla_net_ObliviousHttpChannel_h
