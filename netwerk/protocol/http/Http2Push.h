/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_Http2Push_Internal_h
#define mozilla_net_Http2Push_Internal_h

// HTTP/2 - RFC 7540
// https://www.rfc-editor.org/rfc/rfc7540.txt

#include "Http2Session.h"
#include "Http2StreamBase.h"

#include "mozilla/Attributes.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "nsHttpRequestHead.h"
#include "nsIRequestContext.h"
#include "nsString.h"
#include "PSpdyPush.h"

namespace mozilla {
namespace net {

class Http2PushTransactionBuffer;

class Http2PushedStream final : public Http2StreamBase {
 public:
  Http2PushedStream(Http2PushTransactionBuffer* aTransaction,
                    Http2Session* aSession, Http2StreamBase* aAssociatedStream,
                    uint32_t aID,
                    uint64_t aCurrentForegroundTabOuterContentWindowId);

  bool GetPushComplete();

  // The consumer stream is the synthetic pull stream hooked up to this push
  virtual Http2StreamBase* GetConsumerStream() override {
    return mConsumerStream;
  };

  void SetConsumerStream(Http2StreamBase* consumer);
  [[nodiscard]] bool GetHashKey(nsCString& key);

  // override of Http2StreamBase
  [[nodiscard]] nsresult ReadSegments(nsAHttpSegmentReader*, uint32_t,
                                      uint32_t*) override;
  [[nodiscard]] nsresult WriteSegments(nsAHttpSegmentWriter*, uint32_t,
                                       uint32_t*) override;
  void AdjustInitialWindow() override;

  nsIRequestContext* RequestContext() override { return mRequestContext; };
  void ConnectPushedStream(Http2StreamBase* stream);

  [[nodiscard]] bool TryOnPush();
  [[nodiscard]] static bool TestOnPush(Http2StreamBase* stream);

  virtual bool DeferCleanup(nsresult status) override;
  void SetDeferCleanupOnSuccess(bool val) { mDeferCleanupOnSuccess = val; }

  bool IsOrphaned(TimeStamp now);
  void OnPushFailed() {
    mDeferCleanupOnPush = false;
    mOnPushFailed = true;
  }

  [[nodiscard]] nsresult GetBufferedData(char* buf, uint32_t count,
                                         uint32_t* countWritten);

  // overload of Http2StreamBase
  virtual bool HasSink() override { return !!mConsumerStream; }
  virtual void SetPushComplete() override { mPushCompleted = true; }
  virtual void TopBrowsingContextIdChanged(uint64_t) override;

  nsCString& GetRequestString() { return mRequestString; }
  nsCString& GetResourceUrl() { return mResourceUrl; }

 private:
  virtual ~Http2PushedStream() = default;
  // paired request stream that consumes from real http/2 one.. null until a
  // match is made.
  Http2StreamBase* mConsumerStream{nullptr};

  nsCOMPtr<nsIRequestContext> mRequestContext;

  nsAHttpTransaction* mAssociatedTransaction;

  Http2PushTransactionBuffer* mBufferedPush;
  mozilla::TimeStamp mLastRead;

  nsCString mHashKey;
  nsresult mStatus{NS_OK};
  bool mPushCompleted{false};  // server push FIN received
  bool mDeferCleanupOnSuccess{true};

  // mDeferCleanupOnPush prevents Http2Session::CleanupStream() from
  // destroying the push stream on an error code during the period between
  // when we need to do OnPush() on another thread and the time it takes
  // for that event to create a synthetic pull stream attached to this
  // object. That synthetic pull will become mConsuemerStream.
  // Ths is essentially a delete protecting reference.
  bool mDeferCleanupOnPush{false};
  bool mOnPushFailed{false};
  nsCString mRequestString;
  nsCString mResourceUrl;

  uint32_t mDefaultPriorityDependency;
};

class Http2PushTransactionBuffer final : public nsAHttpTransaction {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSAHTTPTRANSACTION

  Http2PushTransactionBuffer();

  [[nodiscard]] nsresult GetBufferedData(char* buf, uint32_t count,
                                         uint32_t* countWritten);
  void SetPushStream(Http2PushedStream* stream) { mPushStream = stream; }

 private:
  virtual ~Http2PushTransactionBuffer();
  uint64_t Available();

  const static uint32_t kDefaultBufferSize = 4096;

  nsresult mStatus{NS_OK};
  nsHttpRequestHead* mRequestHead{nullptr};
  Http2PushedStream* mPushStream{nullptr};
  bool mIsDone{false};

  UniquePtr<char[]> mBufferedHTTP1;
  uint32_t mBufferedHTTP1Size{kDefaultBufferSize};
  uint32_t mBufferedHTTP1Used{0};
  uint32_t mBufferedHTTP1Consumed{0};
};

class Http2PushedStreamWrapper : public nsISupports {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  bool DispatchRelease();

  explicit Http2PushedStreamWrapper(Http2PushedStream* aPushStream);

  nsCString& GetRequestString() { return mRequestString; }
  nsCString& GetResourceUrl() { return mResourceUrl; }
  Http2PushedStream* GetStream();
  void OnPushFailed();
  uint32_t StreamID() { return mStreamID; }

 private:
  virtual ~Http2PushedStreamWrapper();

  nsCString mRequestString;
  nsCString mResourceUrl;
  uint32_t mStreamID;
  WeakPtr<Http2StreamBase> mStream;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_Http2Push_Internal_h
