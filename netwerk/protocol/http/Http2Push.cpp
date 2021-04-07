/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

// Log on level :5, instead of default :4.
#undef LOG
#define LOG(args) LOG5(args)
#undef LOG_ENABLED
#define LOG_ENABLED() LOG5_ENABLED()

#include <algorithm>

#include "Http2Push.h"
#include "nsHttpHandler.h"
#include "nsHttpTransaction.h"
#include "nsIHttpPushListener.h"
#include "nsISocketTransport.h"
#include "nsSocketTransportService2.h"
#include "nsString.h"

namespace mozilla {
namespace net {

// Because WeakPtr isn't thread-safe we must ensure that the object is destroyed
// on the socket thread, so any Release() called on a different thread is
// dispatched to the socket thread.
bool Http2PushedStreamWrapper::DispatchRelease() {
  if (OnSocketThread()) {
    return false;
  }

  gSocketTransportService->Dispatch(
      NewNonOwningRunnableMethod("net::Http2PushedStreamWrapper::Release", this,
                                 &Http2PushedStreamWrapper::Release),
      NS_DISPATCH_NORMAL);

  return true;
}

NS_IMPL_ADDREF(Http2PushedStreamWrapper)
NS_IMETHODIMP_(MozExternalRefCountType)
Http2PushedStreamWrapper::Release() {
  nsrefcnt count = mRefCnt - 1;
  if (DispatchRelease()) {
    // Redispatched to the socket thread.
    return count;
  }

  MOZ_ASSERT(0 != mRefCnt, "dup release");
  count = --mRefCnt;
  NS_LOG_RELEASE(this, count, "Http2PushedStreamWrapper");

  if (0 == count) {
    mRefCnt = 1;
    delete (this);
    return 0;
  }

  return count;
}

NS_INTERFACE_MAP_BEGIN(Http2PushedStreamWrapper)
NS_INTERFACE_MAP_END

Http2PushedStreamWrapper::Http2PushedStreamWrapper(
    Http2PushedStream* aPushStream) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  mStream = aPushStream;
  mRequestString = aPushStream->GetRequestString();
  mResourceUrl = aPushStream->GetResourceUrl();
  mStreamID = aPushStream->StreamID();
}

Http2PushedStreamWrapper::~Http2PushedStreamWrapper() {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
}

Http2PushedStream* Http2PushedStreamWrapper::GetStream() {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  if (mStream) {
    Http2Stream* stream = mStream;
    return static_cast<Http2PushedStream*>(stream);
  }
  return nullptr;
}

void Http2PushedStreamWrapper::OnPushFailed() {
  if (OnSocketThread()) {
    if (mStream) {
      Http2Stream* stream = mStream;
      static_cast<Http2PushedStream*>(stream)->OnPushFailed();
    }
  } else {
    gSocketTransportService->Dispatch(
        NewRunnableMethod("net::Http2PushedStreamWrapper::OnPushFailed", this,
                          &Http2PushedStreamWrapper::OnPushFailed),
        NS_DISPATCH_NORMAL);
  }
}

//////////////////////////////////////////
// Http2PushedStream
//////////////////////////////////////////

Http2PushedStream::Http2PushedStream(
    Http2PushTransactionBuffer* aTransaction, Http2Session* aSession,
    Http2Stream* aAssociatedStream, uint32_t aID,
    uint64_t aCurrentForegroundTabOuterContentWindowId)
    : Http2Stream(aTransaction, aSession, 0,
                  aCurrentForegroundTabOuterContentWindowId),
      mConsumerStream(nullptr),
      mAssociatedTransaction(aAssociatedStream->Transaction()),
      mBufferedPush(aTransaction),
      mStatus(NS_OK),
      mPushCompleted(false),
      mDeferCleanupOnSuccess(true),
      mDeferCleanupOnPush(false),
      mOnPushFailed(false) {
  LOG3(("Http2PushedStream ctor this=%p 0x%X\n", this, aID));
  mStreamID = aID;
  MOZ_ASSERT(!(aID & 1));  // must be even to be a pushed stream
  mBufferedPush->SetPushStream(this);
  mRequestContext = aAssociatedStream->RequestContext();
  mLastRead = TimeStamp::Now();
  mPriorityDependency = aAssociatedStream->PriorityDependency();
  if (mPriorityDependency == Http2Session::kUrgentStartGroupID ||
      mPriorityDependency == Http2Session::kLeaderGroupID) {
    mPriorityDependency = Http2Session::kFollowerGroupID;
  }
  // Cache this for later use in case of tab switch.
  mDefaultPriorityDependency = mPriorityDependency;
  SetPriorityDependency(aAssociatedStream->Priority() + 1, mPriorityDependency);
  // Assume we are on the same tab as our associated stream, for priority
  // purposes. It's possible this could change when we get paired with a sink,
  // but it's unlikely and doesn't much matter anyway.
  mTransactionTabId = aAssociatedStream->TransactionTabId();
}

bool Http2PushedStream::GetPushComplete() { return mPushCompleted; }

nsresult Http2PushedStream::WriteSegments(nsAHttpSegmentWriter* writer,
                                          uint32_t count,
                                          uint32_t* countWritten) {
  nsresult rv = Http2Stream::WriteSegments(writer, count, countWritten);
  if (NS_SUCCEEDED(rv) && *countWritten) {
    mLastRead = TimeStamp::Now();
  }

  if (rv == NS_BASE_STREAM_CLOSED) {
    mPushCompleted = true;
    rv = NS_OK;  // this is what a normal HTTP transaction would do
  }
  if (rv != NS_BASE_STREAM_WOULD_BLOCK && NS_FAILED(rv)) mStatus = rv;
  return rv;
}

bool Http2PushedStream::DeferCleanup(nsresult status) {
  LOG3(("Http2PushedStream::DeferCleanup Query %p %" PRIx32 "\n", this,
        static_cast<uint32_t>(status)));

  if (NS_SUCCEEDED(status) && mDeferCleanupOnSuccess) {
    LOG3(("Http2PushedStream::DeferCleanup %p %" PRIx32 " defer on success\n",
          this, static_cast<uint32_t>(status)));
    return true;
  }
  if (mDeferCleanupOnPush) {
    LOG3(("Http2PushedStream::DeferCleanup %p %" PRIx32 " defer onPush ref\n",
          this, static_cast<uint32_t>(status)));
    return true;
  }
  if (mConsumerStream) {
    LOG3(("Http2PushedStream::DeferCleanup %p %" PRIx32
          " defer active consumer\n",
          this, static_cast<uint32_t>(status)));
    return true;
  }
  LOG3(("Http2PushedStream::DeferCleanup Query %p %" PRIx32 " not deferred\n",
        this, static_cast<uint32_t>(status)));
  return false;
}

// return true if channel implements nsIHttpPushListener
bool Http2PushedStream::TryOnPush() {
  nsHttpTransaction* trans = mAssociatedTransaction->QueryHttpTransaction();
  if (!trans) {
    return false;
  }

  if (!(trans->Caps() & NS_HTTP_ONPUSH_LISTENER)) {
    return false;
  }

  mDeferCleanupOnPush = true;
  mResourceUrl = Origin() + Path();
  RefPtr<Http2PushedStreamWrapper> stream = new Http2PushedStreamWrapper(this);
  trans->OnPush(stream);
  return true;
}

// side effect free static method to determine if Http2Stream implements
// nsIHttpPushListener
bool Http2PushedStream::TestOnPush(Http2Stream* stream) {
  if (!stream) {
    return false;
  }
  nsAHttpTransaction* abstractTransaction = stream->Transaction();
  if (!abstractTransaction) {
    return false;
  }
  nsHttpTransaction* trans = abstractTransaction->QueryHttpTransaction();
  if (!trans) {
    return false;
  }
  return trans->Caps() & NS_HTTP_ONPUSH_LISTENER;
}

nsresult Http2PushedStream::ReadSegments(nsAHttpSegmentReader* reader, uint32_t,
                                         uint32_t* count) {
  nsresult rv = NS_OK;
  *count = 0;

  mozilla::OriginAttributes originAttributes;
  switch (mUpstreamState) {
    case GENERATING_HEADERS:
      // The request headers for this has been processed, so we need to verify
      // that :authority, :scheme, and :path MUST be present. :method MUST NOT
      // be present
      mSocketTransport->GetOriginAttributes(&originAttributes);
      CreatePushHashKey(mHeaderScheme, mHeaderHost, originAttributes,
                        mSession->Serial(), mHeaderPath, mOrigin, mHashKey);

      LOG3(("Http2PushStream 0x%X hash key %s\n", mStreamID, mHashKey.get()));

      // the write side of a pushed transaction just involves manipulating a
      // little state
      SetSentFin(true);
      Http2Stream::mRequestHeadersDone = 1;
      Http2Stream::mOpenGenerated = 1;
      Http2Stream::ChangeState(UPSTREAM_COMPLETE);
      break;

    case UPSTREAM_COMPLETE:
      // Let's just clear the stream's transmit buffer by pushing it into
      // the session. This is probably a window adjustment.
      LOG3(("Http2Push::ReadSegments 0x%X \n", mStreamID));
      mSegmentReader = reader;
      rv = TransmitFrame(nullptr, nullptr, true);
      mSegmentReader = nullptr;
      break;

    case GENERATING_BODY:
    case SENDING_BODY:
    case SENDING_FIN_STREAM:
    default:
      break;
  }

  return rv;
}

void Http2PushedStream::AdjustInitialWindow() {
  LOG3(("Http2PushStream %p 0x%X AdjustInitialWindow", this, mStreamID));
  if (mConsumerStream) {
    LOG3(
        ("Http2PushStream::AdjustInitialWindow %p 0x%X "
         "calling super consumer %p 0x%X\n",
         this, mStreamID, mConsumerStream, mConsumerStream->StreamID()));
    Http2Stream::AdjustInitialWindow();
    // Http2PushedStream::ReadSegments is needed to call TransmitFrame()
    // and actually get this information into the session bytestream
    mSession->TransactionHasDataToWrite(this);
  }
  // Otherwise, when we get hooked up, the initial window will get bumped
  // anyway, so we're good to go.
}

void Http2PushedStream::SetConsumerStream(Http2Stream* consumer) {
  LOG3(("Http2PushedStream::SetConsumerStream this=%p consumer=%p", this,
        consumer));

  mConsumerStream = consumer;
  mDeferCleanupOnPush = false;
}

bool Http2PushedStream::GetHashKey(nsCString& key) {
  if (mHashKey.IsEmpty()) return false;

  key = mHashKey;
  return true;
}

void Http2PushedStream::ConnectPushedStream(Http2Stream* stream) {
  mSession->ConnectPushedStream(stream);
}

bool Http2PushedStream::IsOrphaned(TimeStamp now) {
  MOZ_ASSERT(!now.IsNull());

  // if session is not transmitting, and is also not connected to a consumer
  // stream, and its been like that for too long then it is oprhaned

  if (mConsumerStream || mDeferCleanupOnPush) {
    return false;
  }

  if (mOnPushFailed) {
    return true;
  }

  bool rv = ((now - mLastRead).ToSeconds() > 30.0);
  if (rv) {
    LOG3(("Http2PushedStream:IsOrphaned 0x%X IsOrphaned %3.2f\n", mStreamID,
          (now - mLastRead).ToSeconds()));
  }
  return rv;
}

nsresult Http2PushedStream::GetBufferedData(char* buf, uint32_t count,
                                            uint32_t* countWritten) {
  if (NS_FAILED(mStatus)) return mStatus;

  nsresult rv = mBufferedPush->GetBufferedData(buf, count, countWritten);
  if (NS_FAILED(rv)) return rv;

  if (!*countWritten)
    rv = GetPushComplete() ? NS_BASE_STREAM_CLOSED : NS_BASE_STREAM_WOULD_BLOCK;

  return rv;
}

void Http2PushedStream::TopBrowsingContextIdChanged(uint64_t id) {
  if (mConsumerStream) {
    // Pass through to our sink, who will handle things appropriately.
    mConsumerStream->TopBrowsingContextIdChanged(id);
    return;
  }

  MOZ_ASSERT(gHttpHandler->ActiveTabPriority());

  mCurrentTopBrowsingContextId = id;

  if (!mSession->UseH2Deps()) {
    return;
  }

  uint32_t oldDependency = mPriorityDependency;
  if (mTransactionTabId != mCurrentTopBrowsingContextId) {
    mPriorityDependency = Http2Session::kBackgroundGroupID;
    nsHttp::NotifyActiveTabLoadOptimization();
  } else {
    mPriorityDependency = mDefaultPriorityDependency;
  }

  if (mPriorityDependency != oldDependency) {
    mSession->SendPriorityFrame(mStreamID, mPriorityDependency,
                                mPriorityWeight);
  }
}

//////////////////////////////////////////
// Http2PushTransactionBuffer
// This is the nsAHttpTransction owned by the stream when the pushed
// stream has not yet been matched with a pull request
//////////////////////////////////////////

NS_IMPL_ISUPPORTS0(Http2PushTransactionBuffer)

Http2PushTransactionBuffer::Http2PushTransactionBuffer()
    : mStatus(NS_OK),
      mRequestHead(nullptr),
      mPushStream(nullptr),
      mIsDone(false),
      mBufferedHTTP1Size(kDefaultBufferSize),
      mBufferedHTTP1Used(0),
      mBufferedHTTP1Consumed(0) {
  mBufferedHTTP1 = MakeUnique<char[]>(mBufferedHTTP1Size);
}

Http2PushTransactionBuffer::~Http2PushTransactionBuffer() {
  delete mRequestHead;
}

void Http2PushTransactionBuffer::SetConnection(nsAHttpConnection* conn) {}

nsAHttpConnection* Http2PushTransactionBuffer::Connection() { return nullptr; }

void Http2PushTransactionBuffer::GetSecurityCallbacks(
    nsIInterfaceRequestor** outCB) {
  *outCB = nullptr;
}

void Http2PushTransactionBuffer::OnTransportStatus(nsITransport* transport,
                                                   nsresult status,
                                                   int64_t progress) {}

nsHttpConnectionInfo* Http2PushTransactionBuffer::ConnectionInfo() {
  if (!mPushStream) {
    return nullptr;
  }
  if (!mPushStream->Transaction()) {
    return nullptr;
  }
  MOZ_ASSERT(mPushStream->Transaction() != this);
  return mPushStream->Transaction()->ConnectionInfo();
}

bool Http2PushTransactionBuffer::IsDone() { return mIsDone; }

nsresult Http2PushTransactionBuffer::Status() { return mStatus; }

uint32_t Http2PushTransactionBuffer::Caps() { return 0; }

uint64_t Http2PushTransactionBuffer::Available() {
  return mBufferedHTTP1Used - mBufferedHTTP1Consumed;
}

nsresult Http2PushTransactionBuffer::ReadSegments(nsAHttpSegmentReader* reader,
                                                  uint32_t count,
                                                  uint32_t* countRead) {
  *countRead = 0;
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult Http2PushTransactionBuffer::WriteSegments(nsAHttpSegmentWriter* writer,
                                                   uint32_t count,
                                                   uint32_t* countWritten) {
  if ((mBufferedHTTP1Size - mBufferedHTTP1Used) < 20480) {
    EnsureBuffer(mBufferedHTTP1, mBufferedHTTP1Size + kDefaultBufferSize,
                 mBufferedHTTP1Used, mBufferedHTTP1Size);
  }

  count = std::min(count, mBufferedHTTP1Size - mBufferedHTTP1Used);
  nsresult rv = writer->OnWriteSegment(&mBufferedHTTP1[mBufferedHTTP1Used],
                                       count, countWritten);
  if (NS_SUCCEEDED(rv)) {
    mBufferedHTTP1Used += *countWritten;
  } else if (rv == NS_BASE_STREAM_CLOSED) {
    mIsDone = true;
  }

  if (Available() || mIsDone) {
    Http2Stream* consumer = mPushStream->GetConsumerStream();

    if (consumer) {
      LOG3(
          ("Http2PushTransactionBuffer::WriteSegments notifying connection "
           "consumer data available 0x%X [%" PRIu64 "] done=%d\n",
           mPushStream->StreamID(), Available(), mIsDone));
      mPushStream->ConnectPushedStream(consumer);
    }
  }

  return rv;
}

uint32_t Http2PushTransactionBuffer::Http1xTransactionCount() { return 0; }

nsHttpRequestHead* Http2PushTransactionBuffer::RequestHead() {
  if (!mRequestHead) mRequestHead = new nsHttpRequestHead();
  return mRequestHead;
}

nsresult Http2PushTransactionBuffer::TakeSubTransactions(
    nsTArray<RefPtr<nsAHttpTransaction> >& outTransactions) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

void Http2PushTransactionBuffer::SetProxyConnectFailed() {}

void Http2PushTransactionBuffer::Close(nsresult reason) {
  mStatus = reason;
  mIsDone = true;
}

nsresult Http2PushTransactionBuffer::GetBufferedData(char* buf, uint32_t count,
                                                     uint32_t* countWritten) {
  *countWritten = std::min(count, static_cast<uint32_t>(Available()));
  if (*countWritten) {
    memcpy(buf, &mBufferedHTTP1[mBufferedHTTP1Consumed], *countWritten);
    mBufferedHTTP1Consumed += *countWritten;
  }

  // If all the data has been consumed then reset the buffer
  if (mBufferedHTTP1Consumed == mBufferedHTTP1Used) {
    mBufferedHTTP1Consumed = 0;
    mBufferedHTTP1Used = 0;
  }

  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
