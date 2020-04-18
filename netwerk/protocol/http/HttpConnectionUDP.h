/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HttpConnectionUDP_h__
#define HttpConnectionUDP_h__

#include "HttpConnectionBase.h"
#include "nsHttpConnectionInfo.h"
#include "nsHttpResponseHead.h"
#include "nsAHttpTransaction.h"
#include "nsCOMPtr.h"
#include "nsProxyRelease.h"
#include "prinrval.h"
#include "TunnelUtils.h"
#include "mozilla/Mutex.h"
#include "ARefBase.h"
#include "TimingStruct.h"
#include "HttpTrafficAnalyzer.h"

#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsIInterfaceRequestor.h"
#include "nsITimer.h"
#include "Http3Session.h"

class nsISocketTransport;
class nsISSLSocketControl;

namespace mozilla {
namespace net {

class nsHttpHandler;
class ASpdySession;

// 1dcc863e-db90-4652-a1fe-13fea0b54e46
#define HTTPCONNECTIONUDP_IID                        \
  {                                                  \
    0xb97d2036, 0xb441, 0x48be, {                    \
      0xb3, 0x1e, 0x25, 0x3e, 0xe8, 0x32, 0xdd, 0x67 \
    }                                                \
  }

//-----------------------------------------------------------------------------
// HttpConnectionUDP - represents a connection to a HTTP3 server
//
// NOTE: this objects lives on the socket thread only.  it should not be
// accessed from any other thread.
//-----------------------------------------------------------------------------

class HttpConnectionUDP final : public HttpConnectionBase,
                                public nsAHttpSegmentReader,
                                public nsAHttpSegmentWriter,
                                public nsIInputStreamCallback,
                                public nsIOutputStreamCallback,
                                public nsITransportEventSink,
                                public nsIInterfaceRequestor {
 private:
  virtual ~HttpConnectionUDP();

 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(HTTPCONNECTIONUDP_IID)
  NS_DECL_HTTPCONNECTIONBASE
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSAHTTPSEGMENTREADER
  NS_DECL_NSAHTTPSEGMENTWRITER
  NS_DECL_NSIINPUTSTREAMCALLBACK
  NS_DECL_NSIOUTPUTSTREAMCALLBACK
  NS_DECL_NSITRANSPORTEVENTSINK
  NS_DECL_NSIINTERFACEREQUESTOR

  HttpConnectionUDP();

  friend class HttpConnectionUDPForceIO;

  [[nodiscard]] static nsresult ReadFromStream(nsIInputStream*, void*,
                                               const char*, uint32_t, uint32_t,
                                               uint32_t*);

  bool UsingHttp3() override { return true; }

  static void OnQuicTimeout(nsITimer* aTimer, void* aClosure);
  void OnQuicTimeoutExpired();

 private:
  [[nodiscard]] nsresult OnTransactionDone(nsresult reason);
  [[nodiscard]] nsresult OnSocketWritable();
  [[nodiscard]] nsresult OnSocketReadable();

 private:
  nsCOMPtr<nsIAsyncInputStream> mSocketIn;
  nsCOMPtr<nsIAsyncOutputStream> mSocketOut;

  RefPtr<nsHttpHandler> mHttpHandler;  // keep gHttpHandler alive

  PRIntervalTime mLastReadTime;
  PRIntervalTime mLastWriteTime;
  int64_t mTotalBytesRead;       // total data read
  int64_t mContentBytesWritten;  // does not include CONNECT tunnel or TLS

  RefPtr<nsIAsyncInputStream> mInputOverflow;

  bool mConnectedTransport;
  bool mDontReuse;
  bool mIsReused;
  bool mLastTransactionExpectedNoContent;

  int32_t mPriority;

 private:
  // For ForceSend()
  static void ForceSendIO(nsITimer* aTimer, void* aClosure);
  [[nodiscard]] nsresult MaybeForceSendIO();
  bool mForceSendPending;
  nsCOMPtr<nsITimer> mForceSendTimer;

  PRIntervalTime mLastRequestBytesSentTime;

 private:
  bool mThroughCaptivePortal;

  // Http3
  RefPtr<Http3Session> mHttp3Session;
};

NS_DEFINE_STATIC_IID_ACCESSOR(HttpConnectionUDP, HTTPCONNECTIONUDP_IID)

}  // namespace net
}  // namespace mozilla

#endif  // HttpConnectionUDP_h__
