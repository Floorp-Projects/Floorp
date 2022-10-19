/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HttpConnectionBase_h__
#define HttpConnectionBase_h__

#include "nsHttpConnectionInfo.h"
#include "nsHttpResponseHead.h"
#include "nsAHttpTransaction.h"
#include "nsCOMPtr.h"
#include "nsProxyRelease.h"
#include "prinrval.h"
#include "mozilla/Mutex.h"
#include "ARefBase.h"
#include "TimingStruct.h"
#include "HttpTrafficAnalyzer.h"

#include "mozilla/net/DNS.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsIInterfaceRequestor.h"
#include "nsITimer.h"

class nsISocketTransport;
class nsISSLSocketControl;

namespace mozilla {
namespace net {

class nsHttpHandler;
class ASpdySession;

// 1dcc863e-db90-4652-a1fe-13fea0b54e46
#define HTTPCONNECTIONBASE_IID                       \
  {                                                  \
    0x437e7d26, 0xa2fd, 0x49f2, {                    \
      0xb3, 0x7c, 0x84, 0x23, 0xf0, 0x94, 0x72, 0x36 \
    }                                                \
  }

//-----------------------------------------------------------------------------
// nsHttpConnection - represents a connection to a HTTP server (or proxy)
//
// NOTE: this objects lives on the socket thread only.  it should not be
// accessed from any other thread.
//-----------------------------------------------------------------------------

class HttpConnectionBase : public nsSupportsWeakReference {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(HTTPCONNECTIONBASE_IID)

  HttpConnectionBase();

  // Activate causes the given transaction to be processed on this
  // connection.  It fails if there is already an existing transaction unless
  // a multiplexing protocol such as SPDY is being used
  [[nodiscard]] virtual nsresult Activate(nsAHttpTransaction*, uint32_t caps,
                                          int32_t pri) = 0;

  // Close the underlying socket transport.
  virtual void Close(nsresult reason, bool aIsShutdown = false) = 0;

  virtual bool CanReuse() = 0;  // can this connection be reused?
  virtual bool CanDirectlyActivate() = 0;

  virtual void DontReuse() = 0;

  virtual nsAHttpTransaction* Transaction() = 0;
  nsHttpConnectionInfo* ConnectionInfo() { return mConnInfo; }

  virtual void CloseTransaction(nsAHttpTransaction*, nsresult,
                                bool aIsShutdown = false) = 0;

  [[nodiscard]] virtual nsresult OnHeadersAvailable(nsAHttpTransaction*,
                                                    nsHttpRequestHead*,
                                                    nsHttpResponseHead*,
                                                    bool* reset) = 0;

  [[nodiscard]] virtual nsresult TakeTransport(nsISocketTransport**,
                                               nsIAsyncInputStream**,
                                               nsIAsyncOutputStream**) = 0;

  virtual bool UsingSpdy() { return false; }
  virtual bool UsingHttp3() { return false; }

  virtual void SetTransactionCaps(uint32_t aCaps) { mTransactionCaps = aCaps; }

  virtual void PrintDiagnostics(nsCString& log) = 0;

  // IsExperienced() returns true when the connection has started at least one
  // non null HTTP transaction of any version.
  bool IsExperienced() { return mExperienced; }

  virtual bool TestJoinConnection(const nsACString& hostname, int32_t port) = 0;
  virtual bool JoinConnection(const nsACString& hostname, int32_t port) = 0;

  // Return true when the socket this connection is using has not been
  // authenticated using a client certificate.  Before SSL negotiation
  // has finished this returns false.
  virtual bool NoClientCertAuth() const { return true; }

  // HTTP/2 websocket support
  virtual bool CanAcceptWebsocket() { return false; }

  void GetConnectionInfo(nsHttpConnectionInfo** ci) {
    *ci = do_AddRef(mConnInfo).take();
  }
  virtual void GetTLSSocketControl(nsISSLSocketControl** result) = 0;

  [[nodiscard]] virtual nsresult ResumeSend() = 0;
  [[nodiscard]] virtual nsresult ResumeRecv() = 0;
  [[nodiscard]] virtual nsresult ForceSend() = 0;
  [[nodiscard]] virtual nsresult ForceRecv() = 0;
  virtual HttpVersion Version() = 0;
  virtual bool IsProxyConnectInProgress() = 0;
  virtual bool LastTransactionExpectedNoContent() = 0;
  virtual void SetLastTransactionExpectedNoContent(bool) = 0;
  virtual int64_t BytesWritten() = 0;  // includes TLS
  void SetSecurityCallbacks(nsIInterfaceRequestor* aCallbacks);
  void SetTrafficCategory(HttpTrafficCategory);

  void BootstrapTimings(TimingStruct times);

  virtual bool IsPersistent() = 0;
  virtual bool IsReused() = 0;
  [[nodiscard]] virtual nsresult PushBack(const char* data,
                                          uint32_t length) = 0;
  PRIntervalTime Rtt() { return mRtt; }
  virtual void SetEvent(nsresult aStatus) = 0;

  virtual nsISocketTransport* Transport() { return nullptr; }

  virtual nsresult GetSelfAddr(NetAddr* addr) = 0;
  virtual nsresult GetPeerAddr(NetAddr* addr) = 0;
  virtual bool ResolvedByTRR() = 0;
  virtual bool GetEchConfigUsed() = 0;
  virtual PRIntervalTime LastWriteTime() = 0;

 protected:
  // The capabailities associated with the most recent transaction
  uint32_t mTransactionCaps{0};

  RefPtr<nsHttpConnectionInfo> mConnInfo;

  bool mExperienced{false};

  bool mBootstrappedTimingsSet{false};
  TimingStruct mBootstrappedTimings;

  Mutex mCallbacksLock MOZ_UNANNOTATED{"nsHttpConnection::mCallbacksLock"};
  nsMainThreadPtrHandle<nsIInterfaceRequestor> mCallbacks;

  nsTArray<HttpTrafficCategory> mTrafficCategory;
  PRIntervalTime mRtt{0};
  nsresult mErrorBeforeConnect = NS_OK;
};

NS_DEFINE_STATIC_IID_ACCESSOR(HttpConnectionBase, HTTPCONNECTIONBASE_IID)

#define NS_DECL_HTTPCONNECTIONBASE                                             \
  [[nodiscard]] nsresult Activate(nsAHttpTransaction*, uint32_t, int32_t)      \
      override;                                                                \
  [[nodiscard]] nsresult OnHeadersAvailable(                                   \
      nsAHttpTransaction*, nsHttpRequestHead*, nsHttpResponseHead*,            \
      bool* reset) override;                                                   \
  [[nodiscard]] nsresult TakeTransport(                                        \
      nsISocketTransport**, nsIAsyncInputStream**, nsIAsyncOutputStream**)     \
      override;                                                                \
  void Close(nsresult, bool aIsShutdown = false) override;                     \
  bool CanReuse() override;                                                    \
  bool CanDirectlyActivate() override;                                         \
  void DontReuse() override;                                                   \
  void CloseTransaction(nsAHttpTransaction*, nsresult,                         \
                        bool aIsShutdown = false) override;                    \
  void PrintDiagnostics(nsCString&) override;                                  \
  bool TestJoinConnection(const nsACString&, int32_t) override;                \
  bool JoinConnection(const nsACString&, int32_t) override;                    \
  void GetTLSSocketControl(nsISSLSocketControl** result) override;             \
  [[nodiscard]] nsresult ResumeSend() override;                                \
  [[nodiscard]] nsresult ResumeRecv() override;                                \
  [[nodiscard]] nsresult ForceSend() override;                                 \
  [[nodiscard]] nsresult ForceRecv() override;                                 \
  HttpVersion Version() override;                                              \
  bool IsProxyConnectInProgress() override;                                    \
  bool LastTransactionExpectedNoContent() override;                            \
  void SetLastTransactionExpectedNoContent(bool val) override;                 \
  bool IsPersistent() override;                                                \
  bool IsReused() override;                                                    \
  [[nodiscard]] nsresult PushBack(const char* data, uint32_t length) override; \
  void SetEvent(nsresult aStatus) override;                                    \
  virtual nsAHttpTransaction* Transaction() override;                          \
  PRIntervalTime LastWriteTime() override;

}  // namespace net
}  // namespace mozilla

#endif  // HttpConnectionBase_h__
