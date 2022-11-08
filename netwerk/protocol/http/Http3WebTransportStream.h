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

namespace mozilla::net {

class Http3WebTransportSession;

class Http3WebTransportStream final : public Http3StreamBase,
                                      public nsAHttpSegmentWriter,
                                      public nsAHttpSegmentReader {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Http3WebTransportSession, override)
  NS_DECL_NSAHTTPSEGMENTWRITER
  NS_DECL_NSAHTTPSEGMENTREADER

  explicit Http3WebTransportStream(
      Http3Session* aSession, uint64_t aSessionId, WebTransportStreamType aType,
      std::function<void(Result<RefPtr<Http3WebTransportStream>, nsresult>&&)>&&
          aCallback);

  Http3WebTransportSession* GetHttp3WebTransportSession() override {
    return nullptr;
  }
  Http3WebTransportStream* GetHttp3WebTransportStream() override {
    return this;
  }
  Http3Stream* GetHttp3Stream() override { return nullptr; }

  [[nodiscard]] nsresult ReadSegments() override;
  [[nodiscard]] nsresult WriteSegments() override;

  bool Done() const override;
  void Close(nsresult aResult) override;

  void SetResponseHeaders(nsTArray<uint8_t>& aResponseHeaders, bool fin,
                          bool interim) override {}

  uint64_t SessionId() const { return mSessionId; }
  WebTransportStreamType StreamType() const { return mStreamType; }

  void SendFin();
  void Reset(uint8_t aErrorCode);

 private:
  virtual ~Http3WebTransportStream();

  nsresult TryActivating();
  static nsresult ReadRequestSegment(nsIInputStream*, void*, const char*,
                                     uint32_t, uint32_t, uint32_t*);
  uint64_t mSessionId{UINT64_MAX};
  WebTransportStreamType mStreamType{WebTransportStreamType::BiDi};

  enum SendStreamState {
    WAITING_TO_ACTIVATE,
    SENDING,
    SEND_DONE,
  } mSendState{WAITING_TO_ACTIVATE};

  nsresult mSocketOutCondition = NS_ERROR_NOT_INITIALIZED;

  std::function<void(Result<RefPtr<Http3WebTransportStream>, nsresult>&&)>
      mStreamReadyCallback;
};

}  // namespace mozilla::net

#endif  // mozilla_net_Http3WebTransportStream_h
