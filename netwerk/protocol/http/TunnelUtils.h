/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_TLSFilterTransaction_h
#define mozilla_net_TLSFilterTransaction_h

#include "mozilla/Attributes.h"
#include "nsAHttpTransaction.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsISocketTransport.h"
#include "nsITimer.h"
#include "NullHttpTransaction.h"

// a TLSFilterTransaction wraps another nsAHttpTransaction but
// applies a encode/decode filter of TLS onto the ReadSegments
// and WriteSegments data. It is not used for basic https://
// but it is used for supplemental TLS tunnels - such as those
// needed by CONNECT tunnels in HTTP/2 or even CONNECT tunnels when
// the underlying proxy connection is already running TLS
//
// HTTP/2 CONNECT tunnels cannot use pushed IO layers because of
// the multiplexing involved on the base stream. i.e. the base stream
// once it is decrypted may have parts that are encrypted with a
// variety of keys, or none at all

/* ************************************************************************
The input path of http over a spdy CONNECT tunnel once it is established as a stream

note the "real http transaction" can be either a http/1 transaction or another spdy session
inside the tunnel.

  nsHttpConnection::OnInputStreamReady (real socket)
  nsHttpConnection::OnSocketReadable()
  SpdySession::WriteSegment()
  SpdyStream::WriteSegment (tunnel stream)
  SpdyConnectTransaction::WriteSegment
  SpdyStream::OnWriteSegment(tunnel stream)
  SpdySession::OnWriteSegment()
  SpdySession::NetworkRead()
  nsHttpConnection::OnWriteSegment (real socket)
  realSocketIn->Read() return data from network

now pop the stack back up to SpdyConnectTransaction::WriteSegment, the data
that has been read is stored mInputData

  SpdyConnectTransaction.mTunneledConn::OnInputStreamReady(mTunnelStreamIn)
  SpdyConnectTransaction.mTunneledConn::OnSocketReadable()
  TLSFilterTransaction::WriteSegment()
  nsHttpTransaction::WriteSegment(real http transaction)
  TLSFilterTransaction::OnWriteSegment() removes tls on way back up stack
  SpdyConnectTransaction.mTunneledConn::OnWriteSegment()
  SpdyConnectTransaction.mTunneledConn.mTunnelStreamIn->Read() // gets data from mInputData

The output path works similarly:
  nsHttpConnection::OnOutputStreamReady (real socket)
  nsHttpConnection::OnSocketWritable()
  SpdySession::ReadSegments (locates tunnel)
  SpdyStream::ReadSegments (tunnel stream)
  SpdyConnectTransaction::ReadSegments()
  SpdyConnectTransaction.mTunneledConn::OnOutputStreamReady (tunnel connection)
  SpdyConnectTransaction.mTunneledConn::OnSocketWritable (tunnel connection)
  TLSFilterTransaction::ReadSegment()
  nsHttpTransaction::ReadSegment (real http transaction generates plaintext on way down)
  TLSFilterTransaction::OnReadSegment (BUF and LEN gets encrypted here on way down)
  SpdyConnectTransaction.mTunneledConn::OnReadSegment (BUF and LEN) (tunnel connection)
  SpdyConnectTransaction.mTunneledConn.mTunnelStreamOut->Write(BUF, LEN) .. get stored in mOutputData

Now pop the stack back up to SpdyConnectTransaction::ReadSegment(), where it has
the encrypted text available in mOutputData

  SpdyStream->OnReadSegment(BUF,LEN) from mOutputData. Tunnel stream
  SpdySession->OnReadSegment() // encrypted data gets put in a data frame
  nsHttpConnection->OnReadSegment()
  realSocketOut->write() writes data to network

**************************************************************************/

struct PRSocketOptionData;

namespace mozilla { namespace net {

class nsHttpRequestHead;
class TLSFilterTransaction;

class NudgeTunnelCallback : public nsISupports
{
public:
  virtual void OnTunnelNudged(TLSFilterTransaction *) = 0;
};

#define NS_DECL_NUDGETUNNELCALLBACK void OnTunnelNudged(TLSFilterTransaction *);

class TLSFilterTransaction MOZ_FINAL
  : public nsAHttpTransaction
  , public nsAHttpSegmentReader
  , public nsAHttpSegmentWriter
  , public nsITimerCallback
{
  ~TLSFilterTransaction();
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSAHTTPTRANSACTION
  NS_DECL_NSAHTTPSEGMENTREADER
  NS_DECL_NSAHTTPSEGMENTWRITER
  NS_DECL_NSITIMERCALLBACK

  TLSFilterTransaction(nsAHttpTransaction *aWrappedTransaction,
                       const char *tlsHost, int32_t tlsPort,
                       nsAHttpSegmentReader *reader,
                       nsAHttpSegmentWriter *writer);

  const nsAHttpTransaction *Transaction() const { return mTransaction.get(); }
  nsresult CommitToSegmentSize(uint32_t size, bool forceCommitment);
  nsresult GetTransactionSecurityInfo(nsISupports **);
  nsresult NudgeTunnel(NudgeTunnelCallback *callback);
  nsresult SetProxiedTransaction(nsAHttpTransaction *aTrans);
  void     newIODriver(nsIAsyncInputStream *aSocketIn,
                       nsIAsyncOutputStream *aSocketOut,
                       nsIAsyncInputStream **outSocketIn,
                       nsIAsyncOutputStream **outSocketOut);

  // nsAHttpTransaction overloads
  nsHttpPipeline *QueryPipeline() MOZ_OVERRIDE;
  bool IsNullTransaction() MOZ_OVERRIDE;
  nsHttpTransaction *QueryHttpTransaction() MOZ_OVERRIDE;
  SpdyConnectTransaction *QuerySpdyConnectTransaction() MOZ_OVERRIDE;

private:
  nsresult StartTimerCallback();
  void Cleanup();
  int32_t FilterOutput(const char *aBuf, int32_t aAmount);
  int32_t FilterInput(char *aBuf, int32_t aAmount);

  static PRStatus GetPeerName(PRFileDesc *fd, PRNetAddr*addr);
  static PRStatus GetSocketOption(PRFileDesc *fd, PRSocketOptionData *data);
  static PRStatus SetSocketOption(PRFileDesc *fd, const PRSocketOptionData *data);
  static int32_t FilterWrite(PRFileDesc *fd, const void *buf, int32_t amount);
  static int32_t FilterRead(PRFileDesc *fd, void *buf, int32_t amount);
  static int32_t FilterSend(PRFileDesc *fd, const void *buf, int32_t amount, int flags,
                             PRIntervalTime timeout);
  static int32_t FilterRecv(PRFileDesc *fd, void *buf, int32_t amount, int flags,
                             PRIntervalTime timeout);
  static PRStatus FilterClose(PRFileDesc *fd);

private:
  nsRefPtr<nsAHttpTransaction> mTransaction;
  nsCOMPtr<nsISupports> mSecInfo;
  nsCOMPtr<nsITimer> mTimer;
  nsRefPtr<NudgeTunnelCallback> mNudgeCallback;

  // buffered network output, after encryption
  nsAutoArrayPtr<char> mEncryptedText;
  uint32_t mEncryptedTextUsed;
  uint32_t mEncryptedTextSize;

  PRFileDesc *mFD;
  nsAHttpSegmentReader *mSegmentReader;
  nsAHttpSegmentWriter *mSegmentWriter;

  nsresult mFilterReadCode;
  bool mForce;
  bool mReadSegmentBlocked;
  uint32_t mNudgeCounter;
};

class SocketTransportShim;
class InputStreamShim;
class OutputStreamShim;
class nsHttpConnection;
class ASpdySession;

class SpdyConnectTransaction MOZ_FINAL : public NullHttpTransaction
{
public:
  SpdyConnectTransaction(nsHttpConnectionInfo *ci,
                         nsIInterfaceRequestor *callbacks,
                         uint32_t caps,
                         nsHttpTransaction *trans,
                         nsAHttpConnection *session);
  ~SpdyConnectTransaction();

  SpdyConnectTransaction *QuerySpdyConnectTransaction() { return this; }

  // A transaction is forced into plaintext when it is intended to be used as a CONNECT
  // tunnel but the setup fails. The plaintext only carries the CONNECT error.
  void ForcePlainText();
  void MapStreamToHttpConnection(nsISocketTransport *aTransport,
                                 nsHttpConnectionInfo *aConnInfo);

  nsresult ReadSegments(nsAHttpSegmentReader *reader,
                        uint32_t count, uint32_t *countRead) MOZ_OVERRIDE MOZ_FINAL;
  nsresult WriteSegments(nsAHttpSegmentWriter *writer,
                         uint32_t count, uint32_t *countWritten) MOZ_OVERRIDE MOZ_FINAL;
  nsHttpRequestHead *RequestHead() MOZ_OVERRIDE MOZ_FINAL;
  void Close(nsresult reason) MOZ_OVERRIDE MOZ_FINAL;

private:
  friend class InputStreamShim;
  friend class OutputStreamShim;

  nsresult Flush(uint32_t count, uint32_t *countRead);
  void CreateShimError(nsresult code);

  nsCString             mConnectString;
  uint32_t              mConnectStringOffset;
  nsHttpRequestHead     *mRequestHead;

  nsAHttpConnection    *mSession;
  nsAHttpSegmentReader *mSegmentReader;

  nsAutoArrayPtr<char> mInputData;
  uint32_t             mInputDataSize;
  uint32_t             mInputDataUsed;
  uint32_t             mInputDataOffset;

  nsAutoArrayPtr<char> mOutputData;
  uint32_t             mOutputDataSize;
  uint32_t             mOutputDataUsed;
  uint32_t             mOutputDataOffset;

  bool                           mForcePlainText;
  TimeStamp                      mTimestampSyn;
  nsRefPtr<nsHttpConnectionInfo> mConnInfo;

  // mTunneledConn, mTunnelTransport, mTunnelStreamIn, mTunnelStreamOut
  // are the connectors to the "real" http connection. They are created
  // together when the tunnel setup is complete and a static reference is held
  // for the lifetime of the tunnel.
  nsRefPtr<nsHttpConnection>     mTunneledConn;
  nsRefPtr<SocketTransportShim>  mTunnelTransport;
  nsRefPtr<InputStreamShim>      mTunnelStreamIn;
  nsRefPtr<OutputStreamShim>     mTunnelStreamOut;
  nsRefPtr<nsHttpTransaction>    mDrivingTransaction;
};

}} // namespace mozilla::net

#endif // mozilla_net_TLSFilterTransaction_h
