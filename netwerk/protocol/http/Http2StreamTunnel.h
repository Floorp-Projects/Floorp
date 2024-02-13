/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_Http2StreamTunnel_h
#define mozilla_net_Http2StreamTunnel_h

#include "Http2StreamBase.h"
#include "nsHttpConnection.h"
#include "nsWeakReference.h"

namespace mozilla::net {

class OutputStreamTunnel;
class InputStreamTunnel;

// c881f764-a183-45cb-9dec-d9872b2f47b2
#define NS_HTTP2STREAMTUNNEL_IID                     \
  {                                                  \
    0xc881f764, 0xa183, 0x45cb, {                    \
      0x9d, 0xec, 0xd9, 0x87, 0x2b, 0x2f, 0x47, 0xb2 \
    }                                                \
  }

class Http2StreamTunnel : public Http2StreamBase,
                          public nsISocketTransport,
                          public nsSupportsWeakReference {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_HTTP2STREAMTUNNEL_IID)
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITRANSPORT
  NS_DECL_NSISOCKETTRANSPORT

  Http2StreamTunnel(Http2Session* session, int32_t priority, uint64_t bcId,
                    nsHttpConnectionInfo* aConnectionInfo);
  bool IsTunnel() override { return true; }

  already_AddRefed<nsHttpConnection> CreateHttpConnection(
      nsAHttpTransaction* httpTransaction, nsIInterfaceRequestor* aCallbacks,
      PRIntervalTime aRtt, bool aIsWebSocket);

  nsHttpConnectionInfo* ConnectionInfo() override { return mConnectionInfo; }

  void SetRequestDone() { mSendClosed = true; }
  nsresult Condition() override { return mCondition; }
  void CloseStream(nsresult reason) override;
  void DisableSpdy() override {
    if (mTransaction) {
      mTransaction->DisableHttp2ForProxy();
    }
  }
  void ReuseConnectionOnRestartOK(bool aReuse) override {
    // Do nothing here. We'll never reuse the connection.
  }
  void MakeNonSticky() override {}

 protected:
  ~Http2StreamTunnel();
  nsresult CallToReadData(uint32_t count, uint32_t* countRead) override;
  nsresult CallToWriteData(uint32_t count, uint32_t* countWritten) override;
  void HandleResponseHeaders(nsACString& aHeadersOut,
                             int32_t httpResponseCode) override;
  nsresult GenerateHeaders(nsCString& aCompressedData,
                           uint8_t& firstFrameFlags) override;
  bool CloseSendStreamWhenDone() override { return mSendClosed; }

  RefPtr<nsAHttpTransaction> mTransaction;

  void ClearTransactionsBlockedOnTunnel();
  bool DispatchRelease();

  RefPtr<OutputStreamTunnel> mOutput;
  RefPtr<InputStreamTunnel> mInput;
  RefPtr<nsHttpConnectionInfo> mConnectionInfo;
  bool mSendClosed{false};
  nsresult mCondition{NS_OK};
};

// f9d10060-f5d4-443e-ba59-f84ea975c5f0
#define NS_OUTPUTSTREAMTUNNEL_IID                    \
  {                                                  \
    0xf9d10060, 0xf5d4, 0x443e, {                    \
      0xba, 0x59, 0xf8, 0x4e, 0xa9, 0x75, 0xc5, 0xf0 \
    }                                                \
  }

class OutputStreamTunnel : public nsIAsyncOutputStream {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_OUTPUTSTREAMTUNNEL_IID)
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOUTPUTSTREAM
  NS_DECL_NSIASYNCOUTPUTSTREAM

  explicit OutputStreamTunnel(Http2StreamTunnel* aStream);

  nsresult OnSocketReady(nsresult condition);
  void MaybeSetRequestDone(nsIOutputStreamCallback* aCallback);

 private:
  virtual ~OutputStreamTunnel();

  nsresult GetStream(Http2StreamTunnel** aStream);
  nsresult GetSession(Http2Session** aSession);

  nsWeakPtr mWeakStream;
  nsCOMPtr<nsIOutputStreamCallback> mCallback;
  nsresult mCondition{NS_OK};
};

class InputStreamTunnel : public nsIAsyncInputStream {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSIASYNCINPUTSTREAM

  explicit InputStreamTunnel(Http2StreamTunnel* aStream);

  nsresult OnSocketReady(nsresult condition);
  bool HasCallback() { return !!mCallback; }

 private:
  virtual ~InputStreamTunnel();

  nsresult GetStream(Http2StreamTunnel** aStream);
  nsresult GetSession(Http2Session** aSession);

  nsWeakPtr mWeakStream;
  nsCOMPtr<nsIInputStreamCallback> mCallback;
  nsresult mCondition{NS_OK};
};

NS_DEFINE_STATIC_IID_ACCESSOR(Http2StreamTunnel, NS_HTTP2STREAMTUNNEL_IID)
NS_DEFINE_STATIC_IID_ACCESSOR(OutputStreamTunnel, NS_OUTPUTSTREAMTUNNEL_IID)

class Http2StreamWebSocket : public Http2StreamTunnel {
 public:
  Http2StreamWebSocket(Http2Session* session, int32_t priority, uint64_t bcId,
                       nsHttpConnectionInfo* aConnectionInfo);
  void CloseStream(nsresult reason) override;

 protected:
  virtual ~Http2StreamWebSocket();
  nsresult GenerateHeaders(nsCString& aCompressedData,
                           uint8_t& firstFrameFlags) override;
};

}  // namespace mozilla::net

#endif  // mozilla_net_Http2StreamTunnel_h
