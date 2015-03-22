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
#include "nsHttpChannel.h"
#include "nsIHttpPushListener.h"
#include "nsString.h"

namespace mozilla {
namespace net {

class CallChannelOnPush final : public nsRunnable {
  public:
  CallChannelOnPush(nsIHttpChannelInternal *associatedChannel,
                    const nsACString &pushedURI,
                    Http2PushedStream *pushStream)
    : mAssociatedChannel(associatedChannel)
    , mPushedURI(pushedURI)
    , mPushedStream(pushStream)
  {
  }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());
    nsRefPtr<nsHttpChannel> channel;
    CallQueryInterface(mAssociatedChannel, channel.StartAssignment());
    MOZ_ASSERT(channel);
    if (channel && NS_SUCCEEDED(channel->OnPush(mPushedURI, mPushedStream))) {
      return NS_OK;
    }

    LOG3(("Http2PushedStream Orphan %p failed OnPush\n", this));
    mPushedStream->OnPushFailed();
    return NS_OK;
  }

private:
  nsCOMPtr<nsIHttpChannelInternal> mAssociatedChannel;
  const nsCString mPushedURI;
  Http2PushedStream *mPushedStream;
};

//////////////////////////////////////////
// Http2PushedStream
//////////////////////////////////////////

Http2PushedStream::Http2PushedStream(Http2PushTransactionBuffer *aTransaction,
                                     Http2Session *aSession,
                                     Http2Stream *aAssociatedStream,
                                     uint32_t aID)
  :Http2Stream(aTransaction, aSession, 0)
  , mConsumerStream(nullptr)
  , mAssociatedTransaction(aAssociatedStream->Transaction())
  , mBufferedPush(aTransaction)
  , mStatus(NS_OK)
  , mPushCompleted(false)
  , mDeferCleanupOnSuccess(true)
  , mDeferCleanupOnPush(false)
  , mOnPushFailed(false)
{
  LOG3(("Http2PushedStream ctor this=%p 0x%X\n", this, aID));
  mStreamID = aID;
  MOZ_ASSERT(!(aID & 1)); // must be even to be a pushed stream
  mBufferedPush->SetPushStream(this);
  mLoadGroupCI = aAssociatedStream->LoadGroupConnectionInfo();
  mLastRead = TimeStamp::Now();
  SetPriority(aAssociatedStream->Priority() + 1);
}

bool
Http2PushedStream::GetPushComplete()
{
  return mPushCompleted;
}

nsresult
Http2PushedStream::WriteSegments(nsAHttpSegmentWriter *writer,
                                 uint32_t count, uint32_t *countWritten)
{
  nsresult rv = Http2Stream::WriteSegments(writer, count, countWritten);
  if (NS_SUCCEEDED(rv) && *countWritten) {
    mLastRead = TimeStamp::Now();
  }

  if (rv == NS_BASE_STREAM_CLOSED) {
    mPushCompleted = true;
    rv = NS_OK; // this is what a normal HTTP transaction would do
  }
  if (rv != NS_BASE_STREAM_WOULD_BLOCK && NS_FAILED(rv))
    mStatus = rv;
  return rv;
}

bool
Http2PushedStream::DeferCleanup(nsresult status)
{
  LOG3(("Http2PushedStream::DeferCleanup Query %p %x\n", this, status));

  if (NS_SUCCEEDED(status) && mDeferCleanupOnSuccess) {
    LOG3(("Http2PushedStream::DeferCleanup %p %x defer on success\n", this, status));
    return true;
  }
  if (mDeferCleanupOnPush) {
    LOG3(("Http2PushedStream::DeferCleanup %p %x defer onPush ref\n", this, status));
    return true;
  }
  if (mConsumerStream) {
    LOG3(("Http2PushedStream::DeferCleanup %p %x defer active consumer\n", this, status));
    return true;
  }
  LOG3(("Http2PushedStream::DeferCleanup Query %p %x not deferred\n", this, status));
  return false;
}

// return true if channel implements nsIHttpPushListener
bool
Http2PushedStream::TryOnPush()
{
  nsHttpTransaction *trans = mAssociatedTransaction->QueryHttpTransaction();
  if (!trans) {
    return false;
  }

  nsCOMPtr<nsIHttpChannelInternal> associatedChannel = do_QueryInterface(trans->HttpChannel());
  if (!associatedChannel) {
    return false;
  }

  if (!(trans->Caps() & NS_HTTP_ONPUSH_LISTENER)) {
    return false;
  }

  mDeferCleanupOnPush = true;
  nsCString uri = Origin() + Path();
  NS_DispatchToMainThread(new CallChannelOnPush(associatedChannel, uri, this));
  return true;
}

nsresult
Http2PushedStream::ReadSegments(nsAHttpSegmentReader *,
                                uint32_t, uint32_t *count)
{
  // The request headers for this has been processed, so we need to verify
  // that :authority, :scheme, and :path MUST be present. :method MUST NOT be
  // present
  CreatePushHashKey(mHeaderScheme, mHeaderHost,
                    mSession->Serial(), mHeaderPath,
                    mOrigin, mHashKey);

  LOG3(("Http2PushStream 0x%X hash key %s\n", mStreamID, mHashKey.get()));

  // the write side of a pushed transaction just involves manipulating a little state
  SetSentFin(true);
  Http2Stream::mRequestHeadersDone = 1;
  Http2Stream::mOpenGenerated = 1;
  Http2Stream::ChangeState(UPSTREAM_COMPLETE);
  *count = 0;
  return NS_OK;
}

void
Http2PushedStream::SetConsumerStream(Http2Stream *consumer)
{
  mConsumerStream = consumer;
  mDeferCleanupOnPush = false;
}

bool
Http2PushedStream::GetHashKey(nsCString &key)
{
  if (mHashKey.IsEmpty())
    return false;

  key = mHashKey;
  return true;
}

void
Http2PushedStream::ConnectPushedStream(Http2Stream *stream)
{
  mSession->ConnectPushedStream(stream);
}

bool
Http2PushedStream::IsOrphaned(TimeStamp now)
{
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
    LOG3(("Http2PushedStream:IsOrphaned 0x%X IsOrphaned %3.2f\n",
          mStreamID, (now - mLastRead).ToSeconds()));
  }
  return rv;
}

nsresult
Http2PushedStream::GetBufferedData(char *buf,
                                   uint32_t count, uint32_t *countWritten)
{
  if (NS_FAILED(mStatus))
    return mStatus;

  nsresult rv = mBufferedPush->GetBufferedData(buf, count, countWritten);
  if (NS_FAILED(rv))
    return rv;

  if (!*countWritten)
    rv = GetPushComplete() ? NS_BASE_STREAM_CLOSED : NS_BASE_STREAM_WOULD_BLOCK;

  return rv;
}

//////////////////////////////////////////
// Http2PushTransactionBuffer
// This is the nsAHttpTransction owned by the stream when the pushed
// stream has not yet been matched with a pull request
//////////////////////////////////////////

NS_IMPL_ISUPPORTS0(Http2PushTransactionBuffer)

Http2PushTransactionBuffer::Http2PushTransactionBuffer()
  : mStatus(NS_OK)
  , mRequestHead(nullptr)
  , mPushStream(nullptr)
  , mIsDone(false)
  , mBufferedHTTP1Size(kDefaultBufferSize)
  , mBufferedHTTP1Used(0)
  , mBufferedHTTP1Consumed(0)
{
  mBufferedHTTP1 = new char[mBufferedHTTP1Size];
}

Http2PushTransactionBuffer::~Http2PushTransactionBuffer()
{
  delete mRequestHead;
}

void
Http2PushTransactionBuffer::SetConnection(nsAHttpConnection *conn)
{
}

nsAHttpConnection *
Http2PushTransactionBuffer::Connection()
{
  return nullptr;
}

void
Http2PushTransactionBuffer::GetSecurityCallbacks(nsIInterfaceRequestor **outCB)
{
  *outCB = nullptr;
}

void
Http2PushTransactionBuffer::OnTransportStatus(nsITransport* transport,
                                              nsresult status, int64_t progress)
{
}

nsHttpConnectionInfo *
Http2PushTransactionBuffer::ConnectionInfo()
{
  if (!mPushStream) {
    return nullptr;
  }
  if (!mPushStream->Transaction()) {
    return nullptr;
  }
  MOZ_ASSERT(mPushStream->Transaction() != this);
  return mPushStream->Transaction()->ConnectionInfo();
}

bool
Http2PushTransactionBuffer::IsDone()
{
  return mIsDone;
}

nsresult
Http2PushTransactionBuffer::Status()
{
  return mStatus;
}

uint32_t
Http2PushTransactionBuffer::Caps()
{
  return 0;
}

void
Http2PushTransactionBuffer::SetDNSWasRefreshed()
{
}

uint64_t
Http2PushTransactionBuffer::Available()
{
  return mBufferedHTTP1Used - mBufferedHTTP1Consumed;
}

nsresult
Http2PushTransactionBuffer::ReadSegments(nsAHttpSegmentReader *reader,
                                         uint32_t count, uint32_t *countRead)
{
  *countRead = 0;
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
Http2PushTransactionBuffer::WriteSegments(nsAHttpSegmentWriter *writer,
                                          uint32_t count, uint32_t *countWritten)
{
  if ((mBufferedHTTP1Size - mBufferedHTTP1Used) < 20480) {
    EnsureBuffer(mBufferedHTTP1,mBufferedHTTP1Size + kDefaultBufferSize,
                 mBufferedHTTP1Used, mBufferedHTTP1Size);
  }

  count = std::min(count, mBufferedHTTP1Size - mBufferedHTTP1Used);
  nsresult rv = writer->OnWriteSegment(mBufferedHTTP1 + mBufferedHTTP1Used,
                                       count, countWritten);
  if (NS_SUCCEEDED(rv)) {
    mBufferedHTTP1Used += *countWritten;
  }
  else if (rv == NS_BASE_STREAM_CLOSED) {
    mIsDone = true;
  }

  if (Available() || mIsDone) {
    Http2Stream *consumer = mPushStream->GetConsumerStream();

    if (consumer) {
      LOG3(("Http2PushTransactionBuffer::WriteSegments notifying connection "
            "consumer data available 0x%X [%u] done=%d\n",
            mPushStream->StreamID(), Available(), mIsDone));
      mPushStream->ConnectPushedStream(consumer);
    }
  }

  return rv;
}

uint32_t
Http2PushTransactionBuffer::Http1xTransactionCount()
{
  return 0;
}

nsHttpRequestHead *
Http2PushTransactionBuffer::RequestHead()
{
  if (!mRequestHead)
    mRequestHead = new nsHttpRequestHead();
  return mRequestHead;
}

nsresult
Http2PushTransactionBuffer::TakeSubTransactions(
  nsTArray<nsRefPtr<nsAHttpTransaction> > &outTransactions)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

void
Http2PushTransactionBuffer::SetProxyConnectFailed()
{
}

void
Http2PushTransactionBuffer::Close(nsresult reason)
{
  mStatus = reason;
  mIsDone = true;
}

nsresult
Http2PushTransactionBuffer::AddTransaction(nsAHttpTransaction *trans)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

uint32_t
Http2PushTransactionBuffer::PipelineDepth()
{
  return 0;
}

nsresult
Http2PushTransactionBuffer::SetPipelinePosition(int32_t position)
{
  return NS_OK;
}

int32_t
Http2PushTransactionBuffer::PipelinePosition()
{
  return 1;
}

nsresult
Http2PushTransactionBuffer::GetBufferedData(char *buf,
                                            uint32_t count,
                                            uint32_t *countWritten)
{
  *countWritten = std::min(count, static_cast<uint32_t>(Available()));
  if (*countWritten) {
    memcpy(buf, mBufferedHTTP1 + mBufferedHTTP1Consumed, *countWritten);
    mBufferedHTTP1Consumed += *countWritten;
  }

  // If all the data has been consumed then reset the buffer
  if (mBufferedHTTP1Consumed == mBufferedHTTP1Used) {
    mBufferedHTTP1Consumed = 0;
    mBufferedHTTP1Used = 0;
  }

  return NS_OK;
}

} // namespace mozilla::net
} // namespace mozilla
