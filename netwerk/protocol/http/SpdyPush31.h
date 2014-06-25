/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// spdy/3.1

#ifndef mozilla_net_SpdyPush31_Internal_h
#define mozilla_net_SpdyPush31_Internal_h

#include "mozilla/Attributes.h"
#include "mozilla/TimeStamp.h"
#include "nsHttpRequestHead.h"
#include "nsILoadGroup.h"
#include "nsString.h"
#include "PSpdyPush.h"
#include "SpdySession31.h"
#include "SpdyStream31.h"

namespace mozilla {
namespace net {

class SpdyPush31TransactionBuffer;

class SpdyPushedStream31 MOZ_FINAL : public SpdyStream31
{
public:
  SpdyPushedStream31(SpdyPush31TransactionBuffer *aTransaction,
                     SpdySession31 *aSession,
                     SpdyStream31 *aAssociatedStream,
                     uint32_t aID);
  virtual ~SpdyPushedStream31() {}

  bool GetPushComplete();
  SpdyStream31 *GetConsumerStream() { return mConsumerStream; };
  void SetConsumerStream(SpdyStream31 *aStream) { mConsumerStream = aStream; }
  bool GetHashKey(nsCString &key);

  // override of SpdyStream31
  nsresult ReadSegments(nsAHttpSegmentReader *,  uint32_t, uint32_t *);
  nsresult WriteSegments(nsAHttpSegmentWriter *, uint32_t, uint32_t *);

  nsILoadGroupConnectionInfo *LoadGroupConnectionInfo() { return mLoadGroupCI; };
  void ConnectPushedStream(SpdyStream31 *consumer);

  bool DeferCleanupOnSuccess() { return mDeferCleanupOnSuccess; }
  void SetDeferCleanupOnSuccess(bool val) { mDeferCleanupOnSuccess = val; }

  bool IsOrphaned(TimeStamp now);

  nsresult GetBufferedData(char *buf, uint32_t count, uint32_t *countWritten);

  // overload of SpdyStream31
  virtual bool HasSink() { return !!mConsumerStream; }

private:

  SpdyStream31 *mConsumerStream; // paired request stream that consumes from
  // real spdy one.. null until a match is made.

  nsCOMPtr<nsILoadGroupConnectionInfo> mLoadGroupCI;

  SpdyPush31TransactionBuffer *mBufferedPush;
  TimeStamp          mLastRead;

  nsCString mHashKey;
  nsresult mStatus;
  bool mPushCompleted; // server push FIN received
  bool mDeferCleanupOnSuccess;
};

class SpdyPush31TransactionBuffer MOZ_FINAL : public nsAHttpTransaction
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSAHTTPTRANSACTION

  SpdyPush31TransactionBuffer();

  nsresult GetBufferedData(char *buf, uint32_t count, uint32_t *countWritten);
  void SetPushStream(SpdyPushedStream31 *stream) { mPushStream = stream; }

private:
  virtual ~SpdyPush31TransactionBuffer();

  const static uint32_t kDefaultBufferSize = 4096;

  nsresult mStatus;
  nsHttpRequestHead *mRequestHead;
  SpdyPushedStream31 *mPushStream;
  bool mIsDone;

  nsAutoArrayPtr<char> mBufferedHTTP1;
  uint32_t mBufferedHTTP1Size;
  uint32_t mBufferedHTTP1Used;
  uint32_t mBufferedHTTP1Consumed;
};

} // namespace mozilla::net
} // namespace mozilla

#endif // mozilla_net_SpdyPush3_Internal_h
