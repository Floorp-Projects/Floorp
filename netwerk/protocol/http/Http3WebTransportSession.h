/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_Http3WebTransportSession_h
#define mozilla_net_Http3WebTransportSession_h

#include "ARefBase.h"
#include "Http3StreamBase.h"
#include "nsIWebTransport.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/net/NeqoHttp3Conn.h"

namespace mozilla::net {

class Http3Session;

// TODO Http3WebTransportSession is very similar to Http3Stream. It should
// be built on top of it with a couple of small changes. The docs will be added
// when this is implemented.

class Http3WebTransportSession final : public Http3StreamBase,
                                       public nsAHttpSegmentWriter,
                                       public nsAHttpSegmentReader {
 public:
  NS_DECL_NSAHTTPSEGMENTWRITER
  NS_DECL_NSAHTTPSEGMENTREADER
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Http3WebTransportSession, override)

  Http3WebTransportSession(nsAHttpTransaction*, Http3Session*);

  Http3WebTransportSession* GetHttp3WebTransportSession() override {
    return this;
  }
  Http3WebTransportStream* GetHttp3WebTransportStream() override {
    return nullptr;
  }
  Http3Stream* GetHttp3Stream() override { return nullptr; }

  [[nodiscard]] nsresult ReadSegments() override;
  [[nodiscard]] nsresult WriteSegments() override;

  bool Done() const override { return mRecvState == RECV_DONE; }

  void Close(nsresult aResult) override;

  void SetResponseHeaders(nsTArray<uint8_t>& aResponseHeaders, bool fin,
                          bool interim) override;
  void SetWebTransportSessionEventListener(
      WebTransportSessionEventListener* listener) {
    mListener = listener;
  }

  nsresult TryActivating();
  void TransactionIsDone(nsresult aResult);
  void CloseSession(uint32_t aStatus, const nsACString& aReason);
  void OnSessionClosed(bool aCleanly, uint32_t aStatus,
                       const nsACString& aReason);

  void CreateOutgoingBidirectionalStream(
      std::function<void(Result<RefPtr<Http3WebTransportStream>, nsresult>&&)>&&
          aCallback);
  void CreateOutgoingUnidirectionalStream(
      std::function<void(Result<RefPtr<Http3WebTransportStream>, nsresult>&&)>&&
          aCallback);
  void RemoveWebTransportStream(Http3WebTransportStream* aStream);

  already_AddRefed<Http3WebTransportStream> OnIncomingWebTransportStream(
      WebTransportStreamType aType, uint64_t aId);

  void SendDatagram(nsTArray<uint8_t>&& aData, uint64_t aTrackingId);

  void OnDatagramReceived(nsTArray<uint8_t>&& aData);

  void GetMaxDatagramSize();

  void OnOutgoingDatagramOutCome(
      uint64_t aId, WebTransportSessionEventListener::DatagramOutcome aOutCome);

  void OnStreamStopSending(uint64_t aId, nsresult aError);
  void OnStreamReset(uint64_t aId, nsresult aError);

 private:
  virtual ~Http3WebTransportSession();

  bool ConsumeHeaders(const char* buf, uint32_t avail, uint32_t* countUsed);

  void CreateStreamInternal(
      bool aBidi,
      std::function<void(Result<RefPtr<Http3WebTransportStream>, nsresult>&&)>&&
          aCallback);

  enum RecvStreamState {
    BEFORE_HEADERS,
    READING_HEADERS,
    READING_INTERIM_HEADERS,
    ACTIVE,
    CLOSE_PENDING,
    RECV_DONE
  } mRecvState{BEFORE_HEADERS};

  enum SendStreamState {
    PREPARING_HEADERS,
    WAITING_TO_ACTIVATE,
    PROCESSING_DATAGRAM,
    SEND_DONE,
  } mSendState{PREPARING_HEADERS};

  nsCString mFlatHttpRequestHeaders;
  nsTArray<uint8_t> mFlatResponseHeaders;
  nsTArray<RefPtr<Http3WebTransportStream>> mStreams;

  nsresult mSocketInCondition = NS_ERROR_NOT_INITIALIZED;
  nsresult mSocketOutCondition = NS_ERROR_NOT_INITIALIZED;

  RefPtr<WebTransportSessionEventListener> mListener;
  uint32_t mStatus{0};
  nsCString mReason;
};

}  // namespace mozilla::net
#endif  // mozilla_net_Http3WebTransportSession_h
