/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_Http3WebTransportStream_h
#define mozilla_net_Http3WebTransportStream_h

#include <functional>
#include "Http3StreamBase.h"
#include "mozilla/net/NeqoHttp3Conn.h"
#include "mozilla/Maybe.h"
#include "mozilla/Result.h"
#include "mozilla/ResultVariant.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"

class nsIWebTransportSendStreamStats;
class nsIWebTransportReceiveStreamStats;

namespace mozilla::net {

class Http3WebTransportSession;

class Http3WebTransportStream final : public Http3StreamBase,
                                      public nsAHttpSegmentWriter,
                                      public nsAHttpSegmentReader,
                                      public nsIInputStreamCallback {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSAHTTPSEGMENTWRITER
  NS_DECL_NSAHTTPSEGMENTREADER
  NS_DECL_NSIINPUTSTREAMCALLBACK

  explicit Http3WebTransportStream(
      Http3Session* aSession, uint64_t aSessionId, WebTransportStreamType aType,
      std::function<void(Result<RefPtr<Http3WebTransportStream>, nsresult>&&)>&&
          aCallback);
  explicit Http3WebTransportStream(Http3Session* aSession, uint64_t aSessionId,
                                   WebTransportStreamType aType,
                                   uint64_t aStreamId);

  Http3WebTransportSession* GetHttp3WebTransportSession() override {
    return nullptr;
  }
  Http3WebTransportStream* GetHttp3WebTransportStream() override {
    return this;
  }
  Http3Stream* GetHttp3Stream() override { return nullptr; }

  void SetSendOrder(Maybe<int64_t> aSendOrder);

  [[nodiscard]] nsresult ReadSegments() override;
  [[nodiscard]] nsresult WriteSegments() override;

  bool Done() const override;
  void Close(nsresult aResult) override;

  void SetResponseHeaders(nsTArray<uint8_t>& aResponseHeaders, bool fin,
                          bool interim) override {}

  uint64_t SessionId() const { return mSessionId; }
  WebTransportStreamType StreamType() const { return mStreamType; }

  void SendFin();
  void Reset(uint64_t aErrorCode);
  void SendStopSending(uint8_t aErrorCode);

  already_AddRefed<nsIWebTransportSendStreamStats> GetSendStreamStats();
  already_AddRefed<nsIWebTransportReceiveStreamStats> GetReceiveStreamStats();
  void GetWriterAndReader(nsIAsyncOutputStream** aOutOutputStream,
                          nsIAsyncInputStream** aOutInputStream);

  // When mRecvState is RECV_DONE, this means we already received the FIN.
  bool RecvDone() const { return mRecvState == RECV_DONE; }

 private:
  friend class Http3WebTransportSession;
  virtual ~Http3WebTransportStream();

  nsresult TryActivating();
  static nsresult ReadRequestSegment(nsIInputStream*, void*, const char*,
                                     uint32_t, uint32_t, uint32_t*);
  static nsresult WritePipeSegment(nsIOutputStream*, void*, char*, uint32_t,
                                   uint32_t, uint32_t*);
  nsresult InitOutputPipe();
  nsresult InitInputPipe();

  uint64_t mSessionId{UINT64_MAX};
  WebTransportStreamType mStreamType{WebTransportStreamType::BiDi};

  enum StreamRole {
    INCOMING,
    OUTGOING,
  } mStreamRole{INCOMING};

  enum SendStreamState {
    WAITING_TO_ACTIVATE,
    WAITING_DATA,
    SENDING,
    SEND_DONE,
  } mSendState{WAITING_TO_ACTIVATE};

  enum RecvStreamState { BEFORE_READING, READING, RECEIVED_FIN, RECV_DONE };
  Atomic<RecvStreamState> mRecvState{BEFORE_READING};

  nsresult mSocketOutCondition = NS_ERROR_NOT_INITIALIZED;
  nsresult mSocketInCondition = NS_ERROR_NOT_INITIALIZED;

  std::function<void(Result<RefPtr<Http3WebTransportStream>, nsresult>&&)>
      mStreamReadyCallback;

  Mutex mMutex{"Http3WebTransportStream::mMutex"};
  nsCOMPtr<nsIAsyncInputStream> mSendStreamPipeIn;
  nsCOMPtr<nsIAsyncOutputStream> mSendStreamPipeOut MOZ_GUARDED_BY(mMutex);

  nsCOMPtr<nsIAsyncInputStream> mReceiveStreamPipeIn MOZ_GUARDED_BY(mMutex);
  nsCOMPtr<nsIAsyncOutputStream> mReceiveStreamPipeOut;

  uint64_t mTotalSent = 0;
  uint64_t mTotalReceived = 0;
  // TODO: neqo doesn't expose this information for now.
  uint64_t mTotalAcknowledged = 0;
  bool mSendFin{false};
  // The error code used to reset the stream. Should be only set once.
  Maybe<uint64_t> mResetError;
  // The error code used for STOP_SENDING. Should be only set once.
  Maybe<uint8_t> mStopSendingError;

  // This is used when SendFin or Reset is called when mSendState is SENDING.
  nsTArray<std::function<void()>> mPendingTasks;
};

}  // namespace mozilla::net

#endif  // mozilla_net_Http3WebTransportStream_h
