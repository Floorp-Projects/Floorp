/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=4 sw=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Http3Session_H__
#define Http3Session_H__

#include "nsISupportsImpl.h"
#include "nsITimer.h"
#include "nsIUDPSocket.h"
#include "mozilla/net/NeqoHttp3Conn.h"
#include "nsAHttpConnection.h"
#include "nsRefPtrHashtable.h"
#include "nsWeakReference.h"
#include "HttpTrafficAnalyzer.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WeakPtr.h"
#include "nsDeque.h"

namespace mozilla::net {

class HttpConnectionUDP;
class Http3Stream;
class QuicSocketControl;

// IID for the Http3Session interface
#define NS_HTTP3SESSION_IID                          \
  {                                                  \
    0x8fc82aaf, 0xc4ef, 0x46ed, {                    \
      0x89, 0x41, 0x93, 0x95, 0x8f, 0xac, 0x4f, 0x21 \
    }                                                \
  }

class Http3Session final : public nsAHttpTransaction, public nsAHttpConnection {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_HTTP3SESSION_IID)

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSAHTTPTRANSACTION
  NS_DECL_NSAHTTPCONNECTION(mConnection)

  Http3Session();
  nsresult Init(const nsHttpConnectionInfo* aConnInfo, nsINetAddr* selfAddr,
                nsINetAddr* peerAddr, HttpConnectionUDP* udpConn,
                uint32_t controlFlags, nsIInterfaceRequestor* callbacks);

  bool IsConnected() const { return mState == CONNECTED; }
  bool CanSandData() const {
    return (mState == CONNECTED) || (mState == ZERORTT);
  }
  bool IsClosing() const { return (mState == CLOSING || mState == CLOSED); }
  bool IsClosed() const { return mState == CLOSED; }

  bool AddStream(nsAHttpTransaction* aHttpTransaction, int32_t aPriority,
                 nsIInterfaceRequestor* aCallbacks);

  bool CanReuse();

  // The folowing functions are used by Http3Stream:
  nsresult TryActivating(const nsACString& aMethod, const nsACString& aScheme,
                         const nsACString& aAuthorityHeader,
                         const nsACString& aPath, const nsACString& aHeaders,
                         uint64_t* aStreamId, Http3Stream* aStream);
  void CloseSendingSide(uint64_t aStreamId);
  nsresult SendRequestBody(uint64_t aStreamId, const char* buf, uint32_t count,
                           uint32_t* countRead);
  nsresult ReadResponseHeaders(uint64_t aStreamId,
                               nsTArray<uint8_t>& aResponseHeaders, bool* aFin);
  nsresult ReadResponseData(uint64_t aStreamId, char* aBuf, uint32_t aCount,
                            uint32_t* aCountWritten, bool* aFin);

  void CloseStream(Http3Stream* aStream, nsresult aResult);

  void SetCleanShutdown(bool aCleanShutdown) {
    mCleanShutdown = aCleanShutdown;
  }

  bool TestJoinConnection(const nsACString& hostname, int32_t port);
  bool JoinConnection(const nsACString& hostname, int32_t port);

  void TransactionHasDataToWrite(nsAHttpTransaction* caller) override;
  void TransactionHasDataToRecv(nsAHttpTransaction* caller) override;
  [[nodiscard]] nsresult GetTransactionTLSSocketControl(
      nsISSLSocketControl**) override;

  // This function will be called by QuicSocketControl when the certificate
  // verification is done.
  void Authenticated(int32_t aError);

  nsresult ProcessOutputAndEvents(nsIUDPSocket* socket);

  void ReportHttp3Connection();

  int64_t GetBytesWritten() { return mTotalBytesWritten; }
  int64_t BytesRead() { return mTotalBytesRead; }

  nsresult SendData(nsIUDPSocket* socket);
  nsresult RecvData(nsIUDPSocket* socket);

  void DoSetEchConfig(const nsACString& aEchConfig);

  nsresult SendPriorityUpdateFrame(uint64_t aStreamId, uint8_t aPriorityUrgency,
                                   bool aPriorityIncremental);

 private:
  ~Http3Session();

  void CloseInternal(bool aCallNeqoClose);
  void Shutdown();

  bool RealJoinConnection(const nsACString& hostname, int32_t port,
                          bool justKidding);

  nsresult ProcessOutput(nsIUDPSocket* socket);
  void ProcessInput(nsIUDPSocket* socket);
  nsresult ProcessEvents();

  nsresult ProcessTransactionRead(uint64_t stream_id, uint32_t* countWritten);
  nsresult ProcessTransactionRead(Http3Stream* stream, uint32_t* countWritten);
  nsresult ProcessSlowConsumers();
  void ConnectSlowConsumer(Http3Stream* stream);

  void SetupTimer(uint64_t aTimeout);

  void ResetRecvd(uint64_t aStreamId, uint64_t aError);

  void QueueStream(Http3Stream* stream);
  void RemoveStreamFromQueues(Http3Stream*);
  void ProcessPending();

  void CallCertVerification(Maybe<nsCString> aEchPublicName);
  void SetSecInfo();

  void StreamReadyToWrite(Http3Stream* aStream);
  void MaybeResumeSend();

  void CloseConnectionTelemetry(CloseError& aError, bool aClosing);
  void Finish0Rtt(bool aRestart);

  enum ZeroRttOutcome {
    NOT_USED,
    USED_SUCCEEDED,
    USED_REJECTED,
    USED_CONN_ERROR,
    USED_CONN_CLOSED_BY_NECKO
  };
  void ZeroRttTelemetry(ZeroRttOutcome aOutcome);

  RefPtr<NeqoHttp3Conn> mHttp3Connection;
  RefPtr<nsAHttpConnection> mConnection;
  nsRefPtrHashtable<nsUint64HashKey, Http3Stream> mStreamIdHash;
  nsRefPtrHashtable<nsPtrHashKey<nsAHttpTransaction>, Http3Stream>
      mStreamTransactionHash;

  nsDeque<Http3Stream> mReadyForWrite;
  nsTArray<RefPtr<Http3Stream>> mSlowConsumersReadyForRead;
  nsDeque<Http3Stream> mQueuedStreams;

  enum State {
    INITIALIZING,
    ZERORTT,
    CONNECTED,
    CLOSING,
    CLOSED
  } mState{INITIALIZING};

  bool mAuthenticationStarted{false};
  bool mCleanShutdown{false};
  bool mGoawayReceived{false};
  bool mShouldClose{false};
  bool mIsClosedByNeqo{false};
  bool mHttp3ConnectionReported = false;
  // mError is neqo error (a protocol error) and that may mean that we will
  // send some packets after that.
  nsresult mError{NS_OK};
  // This is a socket error, there is no poioint in sending anything on that
  // socket.
  nsresult mSocketError{NS_OK};
  bool mBeforeConnectedError{false};
  uint64_t mCurrentTopBrowsingContextId;

  // True if the mTimer is inited and waiting for firing.
  bool mTimerActive{false};

  RefPtr<HttpConnectionUDP> mUdpConn;

  // The underlying socket transport object is needed to propogate some events
  RefPtr<nsISocketTransport> mSocketTransport;

  nsCOMPtr<nsITimer> mTimer;

  nsTHashMap<nsCStringHashKey, bool> mJoinConnectionCache;

  RefPtr<QuicSocketControl> mSocketControl;

  uint64_t mTransactionCount = 0;

  // The stream(s) that we are getting 0RTT data from.
  nsTArray<WeakPtr<Http3Stream>> m0RTTStreams;
  // The stream(s) that are not able to send 0RTT data. We need to
  // remember them put them into mReadyForWrite queue when 0RTT finishes.
  nsTArray<WeakPtr<Http3Stream>> mCannotDo0RTTStreams;

  // The following variables are needed for telemetry.
  TimeStamp mConnectionIdleStart;
  TimeStamp mConnectionIdleEnd;
  Maybe<uint64_t> mFirstStreamIdReuseIdleConnection;
  TimeStamp mTimerShouldTrigger;
  TimeStamp mZeroRttStarted;
  uint64_t mBlockedByStreamLimitCount = 0;
  uint64_t mTransactionsBlockedByStreamLimitCount = 0;
  uint64_t mTransactionsSenderBlockedByFlowControlCount = 0;

  // NS_NET_STATUS_CONNECTED_TO event will be created by the Http3Session.
  // We want to  propagate it to the first transaction.
  RefPtr<nsHttpTransaction> mFirstHttpTransaction;

  RefPtr<nsHttpConnectionInfo> mConnInfo;

  bool mThroughCaptivePortal = false;
  int64_t mTotalBytesRead = 0;     // total data read
  int64_t mTotalBytesWritten = 0;  // total data read
  PRIntervalTime mLastWriteTime = 0;

  nsCOMPtr<nsINetAddr> mNetAddr;
};

NS_DEFINE_STATIC_IID_ACCESSOR(Http3Session, NS_HTTP3SESSION_IID);

}  // namespace mozilla::net

#endif  // Http3Session_H__
