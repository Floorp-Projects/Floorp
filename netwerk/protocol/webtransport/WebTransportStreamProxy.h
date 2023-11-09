/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_WebTransportStreamProxy_h
#define mozilla_net_WebTransportStreamProxy_h

#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsIWebTransportStream.h"
#include "nsCOMPtr.h"

namespace mozilla::net {

class Http3WebTransportStream;

class WebTransportStreamProxy final
    : public nsIWebTransportReceiveStream,
      public nsIWebTransportSendStream,
      public nsIWebTransportBidirectionalStream {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  explicit WebTransportStreamProxy(Http3WebTransportStream* aStream);

  NS_IMETHOD SendStopSending(uint8_t aError) override;
  NS_IMETHOD SendFin() override;
  NS_IMETHOD Reset(uint8_t aErrorCode) override;
  NS_IMETHOD GetSendStreamStats(
      nsIWebTransportStreamStatsCallback* aCallback) override;
  NS_IMETHOD GetReceiveStreamStats(
      nsIWebTransportStreamStatsCallback* aCallback) override;

  NS_IMETHOD GetHasReceivedFIN(bool* aHasReceivedFIN) override;

  NS_IMETHOD GetInputStream(nsIAsyncInputStream** aOut) override;
  NS_IMETHOD GetOutputStream(nsIAsyncOutputStream** aOut) override;

  NS_IMETHOD GetStreamId(uint64_t* aId) override;
  NS_IMETHOD SetSendOrder(Maybe<int64_t> aSendOrder) override;

 private:
  virtual ~WebTransportStreamProxy();

  class AsyncInputStreamWrapper : public nsIAsyncInputStream {
   public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIINPUTSTREAM
    NS_DECL_NSIASYNCINPUTSTREAM

    AsyncInputStreamWrapper(nsIAsyncInputStream* aStream,
                            Http3WebTransportStream* aWebTransportStream);

   private:
    virtual ~AsyncInputStreamWrapper();
    void MaybeCloseStream();

    nsCOMPtr<nsIAsyncInputStream> mStream;
    RefPtr<Http3WebTransportStream> mWebTransportStream;
  };

  class AsyncOutputStreamWrapper : public nsIAsyncOutputStream {
   public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIOUTPUTSTREAM
    NS_DECL_NSIASYNCOUTPUTSTREAM

    explicit AsyncOutputStreamWrapper(nsIAsyncOutputStream* aStream);

   private:
    virtual ~AsyncOutputStreamWrapper();

    nsCOMPtr<nsIAsyncOutputStream> mStream;
  };

  RefPtr<Http3WebTransportStream> mWebTransportStream;
  RefPtr<AsyncOutputStreamWrapper> mWriter;
  RefPtr<AsyncInputStreamWrapper> mReader;
};

}  // namespace mozilla::net

#endif
