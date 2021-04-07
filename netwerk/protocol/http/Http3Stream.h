/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_Http3Stream_h
#define mozilla_net_Http3Stream_h

#include "nsAHttpTransaction.h"
#include "ARefBase.h"
#include "mozilla/WeakPtr.h"

namespace mozilla {
namespace net {

class Http3Session;

class Http3Stream final : public nsAHttpSegmentReader,
                          public nsAHttpSegmentWriter,
                          public SupportsWeakPtr,
                          public ARefBase {
 public:
  NS_DECL_NSAHTTPSEGMENTREADER
  NS_DECL_NSAHTTPSEGMENTWRITER
  // for RefPtr
  NS_INLINE_DECL_REFCOUNTING(Http3Stream, override)

  Http3Stream(nsAHttpTransaction* httpTransaction, Http3Session* session);

  bool HasStreamId() const { return mStreamId != UINT64_MAX; }
  uint64_t StreamId() const { return mStreamId; }

  nsresult TryActivating();

  // TODO priorities
  void TopBrowsingContextIdChanged(uint64_t id){};

  [[nodiscard]] nsresult ReadSegments(nsAHttpSegmentReader*);
  [[nodiscard]] nsresult WriteSegments(nsAHttpSegmentWriter*, uint32_t,
                                       uint32_t*);

  void SetQueued(bool aStatus) { mQueued = aStatus; }
  bool Queued() const { return mQueued; }

  bool Done() const { return mRecvState == RECV_DONE; }

  void Close(nsresult aResult);
  bool RecvdData() const { return mDataReceived; }

  nsAHttpTransaction* Transaction() { return mTransaction; }
  bool RecvdFin() const { return mFin; }
  bool RecvdReset() const { return mResetRecv; }
  void SetRecvdReset() { mResetRecv = true; }

  void StopSending();

  void SetResponseHeaders(nsTArray<uint8_t>& aResponseHeaders, bool fin);

  // Mirrors nsAHttpTransaction
  bool Do0RTT();
  nsresult Finish0RTT(bool aRestart);

 private:
  ~Http3Stream() = default;

  bool GetHeadersString(const char* buf, uint32_t avail, uint32_t* countUsed);
  nsresult StartRequest();
  void FindRequestContentLength();

  /**
   * SendStreamState:
   *  While sending request:
   *  - PREPARING_HEADERS:
   *      In this state we are collecting the headers and in some cases also
   *      waiting to be able to create a new stream.
   *      We need to read all headers into a buffer before calling
   *      Http3Session::TryActivating. Neqo may not have place for a new
   *      stream if it hits MAX_STREAMS limit. In that case the steam will be
   *      queued and dequeue when neqo can again create new stream
   *      (RequestsCreatable will be called).
   *      If transaction has data to send state changes to SENDING_BODY,
   *      otherwise the state transfers to READING_HEADERS.
   *  - SENDING_BODY:
   *      The stream will be in this state while the transaction is sending
   *      request body. Http3Session::SendRequestBody will be call to give
   *      the data to neqo.
   *      After SENDING_BODY, the state transfers to READING_HEADERS.
   *  - EARLY_RESPONSE:
   *      The server may send STOP_SENDING frame with error HTTP_NO_ERROR.
   *      That error means that the server is not interested in the request
   *      body. In this state the server will just ignore the request body.
   **/
  enum SendStreamState {
    PREPARING_HEADERS,
    WAITING_TO_ACTIVATE,
    SENDING_BODY,
    EARLY_RESPONSE,
    SEND_DONE
  } mSendState;

  /**
   * RecvStreamState:
   *  - BEFORE_HEADERS:
   *      The stream has not received headers yet.
   *  - READING_HEADERS:
   *      In this state Http3Session::ReadResponseHeaders will be called to read
   *      the response headers. All headers will be read at once into
   *      mFlatResponseHeaders. The stream will be in this state until all
   *      headers are given to the transaction.
   *      If the stream has been closed by the server after sending headers the
   *      stream will transit into RECEIVED_FIN state, otherwise it transits to
   *      READING_DATA state.
   *  - READING_DATA:
   *      In this state Http3Session::ReadResponseData will be called and the
   *      response body will be given to the transaction.
   *      This state may transfer to RECEIVED_FIN or DONE state.
   *  - DONE:
   *      The transaction is done.
   **/
  enum RecvStreamState {
    BEFORE_HEADERS,
    READING_HEADERS,
    READING_DATA,
    RECEIVED_FIN,
    RECV_DONE
  } mRecvState;

  uint64_t mStreamId;
  Http3Session* mSession;
  RefPtr<nsAHttpTransaction> mTransaction;
  nsCString mFlatHttpRequestHeaders;
  bool mQueued;
  bool mDataReceived;
  bool mResetRecv;
  nsTArray<uint8_t> mFlatResponseHeaders;
  uint32_t mRequestBodyLenRemaining;

  // For Progress Events
  uint64_t mTotalSent;
  uint64_t mTotalRead;

  bool mFin;

  bool mAttempting0RTT = false;

  uint32_t mSendingBlockedByFlowControlCount = 0;

  nsresult mSocketInCondition = NS_ERROR_NOT_INITIALIZED;
  nsresult mSocketOutCondition = NS_ERROR_NOT_INITIALIZED;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_Http3Stream_h
