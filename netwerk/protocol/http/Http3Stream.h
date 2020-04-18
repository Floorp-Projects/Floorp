/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_Http3Stream_h
#define mozilla_net_Http3Stream_h

#include "nsAHttpTransaction.h"
#include "ARefBase.h"

namespace mozilla {
namespace net {

class Http3Session;

class Http3Stream final : public nsAHttpSegmentReader,
                          public nsAHttpSegmentWriter,
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
  void TopLevelOuterContentWindowIdChanged(uint64_t windowId){};

  [[nodiscard]] nsresult ReadSegments(nsAHttpSegmentReader*, uint32_t,
                                      uint32_t*);
  [[nodiscard]] nsresult WriteSegments(nsAHttpSegmentWriter*, uint32_t,
                                       uint32_t*);

  bool RequestBlockedOnRead() const { return mRequestBlockedOnRead; }

  void SetQueued(bool aStatus) { mQueued = aStatus; }
  bool Queued() const { return mQueued; }

  bool Done() const { return mState == DONE; }

  void Close(nsresult aResult);
  bool RecvdData() const { return mDataReceived; }

  nsAHttpTransaction* Transaction() { return mTransaction; }
  bool RecvdFin() const { return mState == RECEIVED_FIN; }
  bool RecvdReset() const { return mState == RECEIVED_RESET; }
  void SetRecvdReset() { mState = RECEIVED_RESET; }

 private:
  ~Http3Stream() = default;

  void GetHeadersString(const char* buf, uint32_t avail, uint32_t* countUsed);
  nsresult StartRequest();
  void FindRequestContentLength();

  /**
   * StreamState:
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
   *      The server may send STOP_SENDING frame with error HTTP_EARLY_RESPONSE.
   *      That error means that the server is not interested in the request
   *      body. In this state the server will just ignore the request body.
   *  After sending a request, the transaction reads data:
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
   *  - RECEIVED_FIN:
   *      The stream is in this state when the receiving side is closed by the
   *      server and this information is not given to the transaction yet.
   *      (example 1: ReadResponseData(Http3Stream is in READING_DATA state)
   *      returns 10 bytes and fin set to true (the server has closed the
   *      stream). The transaction will get this 10 bytes, but it cannot get
   *      the information about the fin at the same time. The Http3Stream
   *      transit into RECEIVED_FIN state. The transaction will need to call
   *      OnWriteSegment again and the Http3Stream::OnWriteSegment will return
   *      NS_BASE_STREAM_CLOSED which means that the stream has been closed.
   *      example 2: if ReadResponseData(Http3Stream is in READING_DATA state)
   *      returns 0 bytes and a fin set to true. Http3Stream::OnWriteSegment
   *      will return NS_BASE_STREAM_CLOSED to the transaction and transfer to
   *      state DONE.)
   *  - RECEIVED_RESET:
   *      The stream has been reset by the server.
   *  - DONE:
   *      The transaction is done.
   **/
  enum StreamState {
    PREPARING_HEADERS,
    SENDING_BODY,
    EARLY_RESPONSE,
    READING_HEADERS,
    READING_DATA,
    RECEIVED_FIN,
    RECEIVED_RESET,
    DONE
  } mState;

  uint64_t mStreamId;
  Http3Session* mSession;
  RefPtr<nsAHttpTransaction> mTransaction;
  nsCString mFlatHttpRequestHeaders;
  bool mRequestHeadersDone;
  bool mRequestStarted;
  bool mQueued;
  bool mRequestBlockedOnRead;
  bool mDataReceived;
  nsTArray<uint8_t> mFlatResponseHeaders;
  uint32_t mRequestBodyLenRemaining;

  // The underlying socket transport object is needed to propogate some events
  RefPtr<nsISocketTransport> mSocketTransport;

  // True if TryActivating() failed and the stream was queued. In this case we
  // return fake count of bytes read by OnReadSegment() to ensure that
  // OnReadSegment() is called again. Otherwise we wouldn't call TryActivating()
  // again and the stream would hang.
  bool mActivatingFailed;

  // For Progress Events
  uint64_t mTotalSent;
  uint64_t mTotalRead;

  bool mFin;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_Http3Stream_h
