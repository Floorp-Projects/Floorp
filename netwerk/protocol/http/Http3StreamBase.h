/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_Http3StreamBase_h
#define mozilla_net_Http3StreamBase_h

#include "nsAHttpTransaction.h"
#include "ARefBase.h"
#include "mozilla/WeakPtr.h"
#include "nsIClassOfService.h"

namespace mozilla::net {

class Http3Session;
class Http3Stream;
class Http3WebTransportSession;
class Http3WebTransportStream;

class Http3StreamBase : public SupportsWeakPtr, public ARefBase {
 public:
  Http3StreamBase(nsAHttpTransaction* trans, Http3Session* session);

  virtual Http3WebTransportSession* GetHttp3WebTransportSession() = 0;
  virtual Http3WebTransportStream* GetHttp3WebTransportStream() = 0;
  virtual Http3Stream* GetHttp3Stream() = 0;

  bool HasStreamId() const { return mStreamId != UINT64_MAX; }
  uint64_t StreamId() const { return mStreamId; }

  [[nodiscard]] virtual nsresult ReadSegments() = 0;
  [[nodiscard]] virtual nsresult WriteSegments() = 0;

  virtual bool Done() const = 0;

  virtual void SetResponseHeaders(nsTArray<uint8_t>& aResponseHeaders, bool fin,
                                  bool interim) = 0;

  void SetQueued(bool aStatus) { mQueued = aStatus; }
  bool Queued() const { return mQueued; }

  virtual void Close(nsresult aResult) = 0;

  nsAHttpTransaction* Transaction() { return mTransaction; }

  // Mirrors nsAHttpTransaction
  virtual bool Do0RTT() { return false; }
  virtual nsresult Finish0RTT(bool aRestart) { return NS_OK; }

  virtual bool RecvdFin() const { return mFin; }
  virtual bool RecvdReset() const { return mResetRecv; }
  virtual void SetRecvdReset() { mResetRecv = true; }

 protected:
  ~Http3StreamBase();

  uint64_t mStreamId{UINT64_MAX};
  int64_t mSendOrder{0};
  bool mSendOrderIsSet{false};
  RefPtr<nsAHttpTransaction> mTransaction;
  RefPtr<Http3Session> mSession;
  bool mQueued{false};
  bool mFin{false};
  bool mResetRecv{false};
};

}  // namespace mozilla::net

#endif  // mozilla_net_Http3StreamBase_h
