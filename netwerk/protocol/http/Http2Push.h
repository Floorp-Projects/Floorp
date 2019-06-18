/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_Http2Push_Internal_h
#define mozilla_net_Http2Push_Internal_h

// HTTP/2 - RFC 7540
// https://www.rfc-editor.org/rfc/rfc7540.txt

#include "Http2Session.h"
#include "Http2Stream.h"

#include "mozilla/Attributes.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "nsHttpRequestHead.h"
#include "nsILoadGroup.h"
#include "nsIRequestContext.h"
#include "nsString.h"
#include "PSpdyPush.h"

namespace mozilla {
namespace net {

class Http2PushTransactionBuffer;

class Http2PushedStream final : public Http2Stream {
 public:
  Http2PushedStream(Http2PushTransactionBuffer* aTransaction,
                    Http2Session* aSession, Http2Stream* aAssociatedStream,
                    uint32_t aID,
                    uint64_t aCurrentForegroundTabOuterContentWindowId);
  virtual ~Http2PushedStream() = default;

  bool GetPushComplete();

  // The consumer stream is the synthetic pull stream hooked up to this push
  virtual Http2Stream* GetConsumerStream() override { return mConsumerStream; };

  void SetConsumerStream(Http2Stream* aStream);
  MOZ_MUST_USE bool GetHashKey(nsCString& key);

  // override of Http2Stream
  MOZ_MUST_USE nsresult ReadSegments(nsAHttpSegmentReader*, uint32_t,
                                     uint32_t*) override;
  MOZ_MUST_USE nsresult WriteSegments(nsAHttpSegmentWriter*, uint32_t,
                                      uint32_t*) override;
  void AdjustInitialWindow() override;

  nsIRequestContext* RequestContext() override { return mRequestContext; };
  void ConnectPushedStream(Http2Stream* consumer);

  MOZ_MUST_USE bool TryOnPush();
  static MOZ_MUST_USE bool TestOnPush(Http2Stream* consumer);

  virtual bool DeferCleanup(nsresult status) override;
  void SetDeferCleanupOnSuccess(bool val) { mDeferCleanupOnSuccess = val; }

  bool IsOrphaned(TimeStamp now);
  void OnPushFailed() {
    mDeferCleanupOnPush = false;
    mOnPushFailed = true;
  }

  MOZ_MUST_USE nsresult GetBufferedData(char* buf, uint32_t count,
                                        uint32_t* countWritten);

  // overload of Http2Stream
  virtual bool HasSink() override { return !!mConsumerStream; }
  virtual void SetPushComplete() override { mPushCompleted = true; }
  virtual void TopLevelOuterContentWindowIdChanged(uint64_t) override;

  nsCString& GetRequestString() { return mRequestString; }

 private:
  Http2Stream*
      mConsumerStream;  // paired request stream that consumes from
                        // real http/2 one.. null until a match is made.

  nsCOMPtr<nsIRequestContext> mRequestContext;

  nsAHttpTransaction* mAssociatedTransaction;

  Http2PushTransactionBuffer* mBufferedPush;
  mozilla::TimeStamp mLastRead;

  nsCString mHashKey;
  nsresult mStatus;
  bool mPushCompleted;  // server push FIN received
  bool mDeferCleanupOnSuccess;

  // mDeferCleanupOnPush prevents Http2Session::CleanupStream() from
  // destroying the push stream on an error code during the period between
  // when we need to do OnPush() on another thread and the time it takes
  // for that event to create a synthetic pull stream attached to this
  // object. That synthetic pull will become mConsuemerStream.
  // Ths is essentially a delete protecting reference.
  bool mDeferCleanupOnPush;
  bool mOnPushFailed;
  nsCString mRequestString;

  uint32_t mDefaultPriorityDependency;
};

class Http2PushTransactionBuffer final : public nsAHttpTransaction {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSAHTTPTRANSACTION

  Http2PushTransactionBuffer();

  MOZ_MUST_USE nsresult GetBufferedData(char* buf, uint32_t count,
                                        uint32_t* countWritten);
  void SetPushStream(Http2PushedStream* stream) { mPushStream = stream; }

 private:
  virtual ~Http2PushTransactionBuffer();
  uint64_t Available();

  const static uint32_t kDefaultBufferSize = 4096;

  nsresult mStatus;
  nsHttpRequestHead* mRequestHead;
  Http2PushedStream* mPushStream;
  bool mIsDone;

  UniquePtr<char[]> mBufferedHTTP1;
  uint32_t mBufferedHTTP1Size;
  uint32_t mBufferedHTTP1Used;
  uint32_t mBufferedHTTP1Consumed;
};

class Http2PushedStreamWrapper : public nsISupports {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  bool DispatchRelease();

  explicit Http2PushedStreamWrapper(Http2PushedStream* aPushStream);

  nsCString& GetRequestString() { return mRequestString; }
  Http2PushedStream* GetStream();
  void OnPushFailed();

 private:
  virtual ~Http2PushedStreamWrapper();

  nsCString mRequestString;
  WeakPtr<Http2Stream> mStream;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_Http2Push_Internal_h
