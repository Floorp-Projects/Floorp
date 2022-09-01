/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_WebSocketConnection_h
#define mozilla_net_WebSocketConnection_h

#include <list>

#include "nsIStreamListener.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "mozilla/net/WebSocketConnectionBase.h"
#include "nsTArray.h"
#include "nsISocketTransport.h"

class nsISocketTransport;

namespace mozilla {
namespace net {

class WebSocketConnectionListener;

class WebSocketConnection : public nsIInputStreamCallback,
                            public nsIOutputStreamCallback,
                            public WebSocketConnectionBase {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAMCALLBACK
  NS_DECL_NSIOUTPUTSTREAMCALLBACK

  explicit WebSocketConnection(nsISocketTransport* aTransport,
                               nsIAsyncInputStream* aInputStream,
                               nsIAsyncOutputStream* aOutputStream);

  nsresult Init(WebSocketConnectionListener* aListener) override;
  void GetIoTarget(nsIEventTarget** aTarget) override;
  void Close() override;
  nsresult WriteOutputData(const uint8_t* aHdrBuf, uint32_t aHdrBufLength,
                           const uint8_t* aPayloadBuf,
                           uint32_t aPayloadBufLength) override;
  nsresult WriteOutputData(nsTArray<uint8_t>&& aData);
  nsresult StartReading() override;
  void DrainSocketData() override;
  nsresult GetSecurityInfo(nsITransportSecurityInfo** aSecurityInfo) override;

 private:
  virtual ~WebSocketConnection();

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

  RefPtr<WebSocketConnectionListener> mListener;
  nsCOMPtr<nsISocketTransport> mTransport;
  nsCOMPtr<nsIAsyncInputStream> mSocketIn;
  nsCOMPtr<nsIAsyncOutputStream> mSocketOut;
  nsCOMPtr<nsIEventTarget> mSocketThread;
  size_t mWriteOffset{0};
  std::list<OutputData> mOutputQueue;
  bool mStartReadingCalled{false};
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_WebSocketConnection_h
