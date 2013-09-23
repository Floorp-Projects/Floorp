/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// SPDY Server Push as defined by
// http://dev.chromium.org/spdy/spdy-protocol/spdy-protocol-draft3

#ifndef mozilla_net_SpdyPush3_Internal_h
#define mozilla_net_SpdyPush3_Internal_h

#include "mozilla/Attributes.h"
#include "mozilla/TimeStamp.h"
#include "nsILoadGroup.h"
#include "nsString.h"
#include "SpdyStream3.h"

namespace mozilla {
namespace net {

class SpdySession3;

class SpdyPush3TransactionBuffer;

class SpdyPushedStream3 MOZ_FINAL : public SpdyStream3
{
public:
  SpdyPushedStream3(SpdyPush3TransactionBuffer *aTransaction,
                    SpdySession3 *aSession,
                    SpdyStream3 *aAssociatedStream,
                    uint32_t aID);
  virtual ~SpdyPushedStream3() {}

  bool GetPushComplete();
  SpdyStream3 *GetConsumerStream() { return mConsumerStream; };
  void SetConsumerStream(SpdyStream3 *aStream) { mConsumerStream = aStream; }
  bool GetHashKey(nsCString &key);

  // override of SpdyStream3
  nsresult ReadSegments(nsAHttpSegmentReader *,  uint32_t, uint32_t *);
  nsresult WriteSegments(nsAHttpSegmentWriter *, uint32_t, uint32_t *);

  nsILoadGroupConnectionInfo *LoadGroupConnectionInfo() { return mLoadGroupCI; };
  void ConnectPushedStream(SpdyStream3 *consumer);

  bool DeferCleanupOnSuccess() { return mDeferCleanupOnSuccess; }
  void SetDeferCleanupOnSuccess(bool val) { mDeferCleanupOnSuccess = val; }

  bool IsOrphaned(TimeStamp now);

  nsresult GetBufferedData(char *buf, uint32_t count, uint32_t *countWritten);

  // overload of SpdyStream3
  virtual bool HasSink() { return !!mConsumerStream; }

private:

  SpdyStream3 *mConsumerStream; // paired request stream that consumes from
                                // real spdy one.. null until a match is made.

  nsCOMPtr<nsILoadGroupConnectionInfo> mLoadGroupCI;

  SpdyPush3TransactionBuffer *mBufferedPush;
  mozilla::TimeStamp          mLastRead;

  nsCString mHashKey;
  nsresult mStatus;
  bool mPushCompleted; // server push FIN received
  bool mDeferCleanupOnSuccess;
};

class SpdyPush3TransactionBuffer MOZ_FINAL : public nsAHttpTransaction
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSAHTTPTRANSACTION

  SpdyPush3TransactionBuffer();
  virtual ~SpdyPush3TransactionBuffer();

  nsresult GetBufferedData(char *buf, uint32_t count, uint32_t *countWritten);
  void SetPushStream(SpdyPushedStream3 *stream) { mPushStream = stream; }

private:
  const static uint32_t kDefaultBufferSize = 4096;

  nsresult mStatus;
  nsHttpRequestHead *mRequestHead;
  SpdyPushedStream3 *mPushStream;
  bool mIsDone;

  nsAutoArrayPtr<char> mBufferedHTTP1;
  uint32_t mBufferedHTTP1Size;
  uint32_t mBufferedHTTP1Used;
  uint32_t mBufferedHTTP1Consumed;
};

} // namespace mozilla::net
} // namespace mozilla

#endif // mozilla_net_SpdyPush3_Internal_h
