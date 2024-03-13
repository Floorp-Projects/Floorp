/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_Http3Stream_h
#define mozilla_net_Http3Stream_h

#include "nsAHttpTransaction.h"
#include "ARefBase.h"
#include "Http3StreamBase.h"
#include "mozilla/WeakPtr.h"
#include "nsIClassOfService.h"

namespace mozilla {
namespace net {

class Http3Session;

class Http3Stream final : public nsAHttpSegmentReader,
                          public nsAHttpSegmentWriter,
                          public Http3StreamBase {
 public:
  NS_DECL_NSAHTTPSEGMENTREADER
  NS_DECL_NSAHTTPSEGMENTWRITER
  // for RefPtr
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Http3Stream, override)

  Http3Stream(nsAHttpTransaction*, Http3Session*, const ClassOfService&,
              uint64_t);

  Http3WebTransportSession* GetHttp3WebTransportSession() override {
    return nullptr;
  }
  Http3WebTransportStream* GetHttp3WebTransportStream() override {
    return nullptr;
  }
  Http3Stream* GetHttp3Stream() override { return this; }

  nsresult TryActivating();

  void CurrentBrowserIdChanged(uint64_t id);

  [[nodiscard]] nsresult ReadSegments() override;
  [[nodiscard]] nsresult WriteSegments() override;

  bool Done() const override { return mRecvState == RECV_DONE; }

  void Close(nsresult aResult) override;
  bool RecvdData() const { return mDataReceived; }

  void StopSending();

  void SetResponseHeaders(nsTArray<uint8_t>& aResponseHeaders, bool fin,
                          bool interim) override;

  // Mirrors nsAHttpTransaction
  bool Do0RTT() override;
  nsresult Finish0RTT(bool aRestart) override;

  uint8_t PriorityUrgency();
  bool PriorityIncremental();

 private:
  ~Http3Stream() = default;

  bool GetHeadersString(const char* buf, uint32_t avail, uint32_t* countUsed);
  nsresult StartRequest();

  void SetPriority(uint32_t aCos);
  void SetIncremental(bool incremental);

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
  } mSendState{PREPARING_HEADERS};

  /**
   * RecvStreamState:
   *  - BEFORE_HEADERS:
   *      The stream has not received headers yet.
   *  - READING_HEADERS and READING_INTERIM_HEADERS:
   *      In this state Http3Session::ReadResponseHeaders will be called to
   *      read the response headers. All headers will be read at once into
   *      mFlatResponseHeaders. The stream will be in this state until all
   *      headers are given to the transaction.
   *      If the steam was in the READING_INTERIM_HEADERS state it will
   *      change back to the  BEFORE_HEADERS  state. If the stream has been
   *      in the READING_HEADERS state it will  change to the READING_DATA
   *      state. If the stream was closed by the server after sending headers
   *      the stream will transit into RECEIVED_FIN state. neqo makes sure
   *      that response headers and data are received in the right order,
   *      e.g. 1xx cannot be received after a non-1xx response, fin cannot
   *      follow 1xx response, etc.
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
    READING_INTERIM_HEADERS,
    READING_DATA,
    RECEIVED_FIN,
    RECV_DONE
  } mRecvState{BEFORE_HEADERS};

  nsCString mFlatHttpRequestHeaders;
  bool mDataReceived{false};
  nsTArray<uint8_t> mFlatResponseHeaders;
  uint64_t mTransactionBrowserId{0};
  uint64_t mCurrentBrowserId;
  uint8_t mPriorityUrgency{3};  // urgency field of http priority
  bool mPriorityIncremental{false};

  // For Progress Events
  uint64_t mTotalSent{0};
  uint64_t mTotalRead{0};

  bool mAttempting0RTT = false;

  uint32_t mSendingBlockedByFlowControlCount = 0;

  nsresult mSocketInCondition = NS_ERROR_NOT_INITIALIZED;
  nsresult mSocketOutCondition = NS_ERROR_NOT_INITIALIZED;

#ifdef DEBUG
  uint32_t mRequestBodyLenExpected{0};
  uint32_t mRequestBodyLenSent{0};
#endif
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_Http3Stream_h
