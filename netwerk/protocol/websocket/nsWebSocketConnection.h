/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_nsWebSocketConnection_h
#define mozilla_net_nsWebSocketConnection_h

#include <list>

#include "nsISupports.h"
#include "nsIStreamListener.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsIWebSocketConnection.h"

class nsISocketTransport;

namespace mozilla {
namespace net {

class nsWebSocketConnection : public nsIWebSocketConnection,
                              public nsIInputStreamCallback,
                              public nsIOutputStreamCallback {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIWEBSOCKETCONNECTION
  NS_DECL_NSIINPUTSTREAMCALLBACK
  NS_DECL_NSIOUTPUTSTREAMCALLBACK

  explicit nsWebSocketConnection(nsISocketTransport* aTransport,
                                 nsIAsyncInputStream* aInputStream,
                                 nsIAsyncOutputStream* aOutputStream);

  nsresult EnqueueOutputData(nsTArray<uint8_t>&& aHeader,
                             nsTArray<uint8_t>&& aPayload);

 private:
  virtual ~nsWebSocketConnection() = default;

  class OutputData {
   public:
    explicit OutputData(nsTArray<uint8_t>&& aData) : mData(std::move(aData)) {
      MOZ_COUNT_CTOR(OutputData);
    }

    ~OutputData() { MOZ_COUNT_DTOR(OutputData); }

    const nsTArray<uint8_t>& GetData() const { return mData; }

   private:
    nsTArray<uint8_t> mData;
  };

  nsCOMPtr<nsIWebSocketConnectionListener> mListener;
  nsCOMPtr<nsISocketTransport> mTransport;
  nsCOMPtr<nsIAsyncInputStream> mSocketIn;
  nsCOMPtr<nsIAsyncOutputStream> mSocketOut;
  nsCOMPtr<nsIEventTarget> mEventTarget;
  size_t mWriteOffset;
  std::list<OutputData> mOutputQueue;
  bool mStartReadingCalled;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_nsWebSocketConnection_h
