/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_Http2ConnectTransaction_h
#define mozilla_net_Http2ConnectTransaction_h

#include "nsAHttpTransaction.h"
#include "nsHttpConnectionInfo.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsISocketTransport.h"
#include "NullHttpTransaction.h"
#include "mozilla/TimeStamp.h"
#include "prio.h"

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
The input path of http over a spdy CONNECT tunnel once it is established as a
stream

note the "real http transaction" can be either a http/1 transaction or another
spdy session inside the tunnel.

  nsHttpConnection::OnInputStreamReady (real socket)
  nsHttpConnection::OnSocketReadable()
  SpdySession::WriteSegment()
  SpdyStream::WriteSegment (tunnel stream)
  Http2ConnectTransaction::WriteSegment
  SpdyStream::OnWriteSegment(tunnel stream)
  SpdySession::OnWriteSegment()
  SpdySession::NetworkRead()
  nsHttpConnection::OnWriteSegment (real socket)
  realSocketIn->Read() return data from network

now pop the stack back up to Http2ConnectTransaction::WriteSegment, the data
that has been read is stored mInputData

  Http2ConnectTransaction.mTunneledConn::OnInputStreamReady(mTunnelStreamIn)
  Http2ConnectTransaction.mTunneledConn::OnSocketReadable()
  TLSFilterTransaction::WriteSegment()
  nsHttpTransaction::WriteSegment(real http transaction)
  TLSFilterTransaction::OnWriteSegment() removes tls on way back up stack
  Http2ConnectTransaction.mTunneledConn::OnWriteSegment()
  // gets data from mInputData
  Http2ConnectTransaction.mTunneledConn.mTunnelStreamIn->Read()

The output path works similarly:
  nsHttpConnection::OnOutputStreamReady (real socket)
  nsHttpConnection::OnSocketWritable()
  SpdySession::ReadSegments (locates tunnel)
  SpdyStream::ReadSegments (tunnel stream)
  Http2ConnectTransaction::ReadSegments()
  Http2ConnectTransaction.mTunneledConn::OnOutputStreamReady (tunnel connection)
  Http2ConnectTransaction.mTunneledConn::OnSocketWritable (tunnel connection)
  TLSFilterTransaction::ReadSegment()
  nsHttpTransaction::ReadSegment (real http transaction generates plaintext on
                                  way down)
  TLSFilterTransaction::OnReadSegment (BUF and LEN gets encrypted here on way
                                       down)
  Http2ConnectTransaction.mTunneledConn::OnReadSegment (BUF and LEN)
                                                      (tunnel connection)
  Http2ConnectTransaction.mTunneledConn.mTunnelStreamOut->Write(BUF, LEN) ..
                                                     get stored in mOutputData

Now pop the stack back up to Http2ConnectTransaction::ReadSegment(), where it
has the encrypted text available in mOutputData

  SpdyStream->OnReadSegment(BUF,LEN) from mOutputData. Tunnel stream
  SpdySession->OnReadSegment() // encrypted data gets put in a data frame
  nsHttpConnection->OnReadSegment()
  realSocketOut->write() writes data to network

**************************************************************************/

struct PRSocketOptionData;

namespace mozilla {
namespace net {

class SocketTransportShim;
class InputStreamShim;
class OutputStreamShim;
class nsHttpConnection;

class Http2ConnectTransaction final : public NullHttpTransaction {
 public:
  Http2ConnectTransaction(nsHttpConnectionInfo* ci,
                          nsIInterfaceRequestor* callbacks, uint32_t caps,
                          nsHttpTransaction* trans, nsAHttpConnection* session,
                          bool isWebsocket);
  ~Http2ConnectTransaction();

  Http2ConnectTransaction* QueryHttp2ConnectTransaction() override {
    return this;
  }

  // A transaction is forced into plaintext when it is intended to be used as a
  // CONNECT tunnel but the setup fails. The plaintext only carries the CONNECT
  // error.
  void ForcePlainText();
  // True if we successfully map stream to a nsHttpConnection. Currently we skip
  // 1xx response only.
  bool MapStreamToHttpConnection(nsISocketTransport* aTransport,
                                 nsHttpConnectionInfo* aConnInfo,
                                 const nsACString& aFlat407Headers,
                                 int32_t aHttpResponseCode);

  [[nodiscard]] nsresult ReadSegments(nsAHttpSegmentReader* reader,
                                      uint32_t count,
                                      uint32_t* countRead) final;
  [[nodiscard]] nsresult WriteSegments(nsAHttpSegmentWriter* writer,
                                       uint32_t count,
                                       uint32_t* countWritten) final;
  nsHttpRequestHead* RequestHead() final;
  void Close(nsresult code) final;

  // ConnectedReadyForInput() tests whether the spdy connect transaction is
  // attached to an nsHttpConnection that can properly deal with flow control,
  // etc..
  bool ConnectedReadyForInput();

  bool IsWebsocket() { return mIsWebsocket; }
  void SetConnRefTaken();

 private:
  friend class SocketTransportShim;
  friend class InputStreamShim;
  friend class OutputStreamShim;

  [[nodiscard]] nsresult Flush(uint32_t count, uint32_t* countRead);
  void CreateShimError(nsresult code);

  nsCString mConnectString;
  uint32_t mConnectStringOffset;

  nsAHttpConnection* mSession;
  nsAHttpSegmentReader* mSegmentReader;

  UniquePtr<char[]> mInputData;
  uint32_t mInputDataSize;
  uint32_t mInputDataUsed;
  uint32_t mInputDataOffset;

  UniquePtr<char[]> mOutputData;
  uint32_t mOutputDataSize;
  uint32_t mOutputDataUsed;
  uint32_t mOutputDataOffset;

  bool mForcePlainText;
  TimeStamp mTimestampSyn;

  // mTunneledConn, mTunnelTransport, mTunnelStreamIn, mTunnelStreamOut
  // are the connectors to the "real" http connection. They are created
  // together when the tunnel setup is complete and a static reference is held
  // for the lifetime of the tunnel.
  RefPtr<nsHttpConnection> mTunneledConn;
  RefPtr<SocketTransportShim> mTunnelTransport;
  RefPtr<InputStreamShim> mTunnelStreamIn;
  RefPtr<OutputStreamShim> mTunnelStreamOut;
  RefPtr<nsHttpTransaction> mDrivingTransaction;

  // This is all for websocket support
  bool mIsWebsocket;
  bool mConnRefTaken;
  nsCOMPtr<nsIAsyncOutputStream> mInputShimPipe;
  nsCOMPtr<nsIAsyncInputStream> mOutputShimPipe;
  nsresult WriteDataToBuffer(nsAHttpSegmentWriter* writer, uint32_t count,
                             uint32_t* countWritten);
  [[nodiscard]] nsresult WebsocketWriteSegments(nsAHttpSegmentWriter* writer,
                                                uint32_t count,
                                                uint32_t* countWritten);

  bool mCreateShimErrorCalled;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_Http2ConnectTransaction_h
