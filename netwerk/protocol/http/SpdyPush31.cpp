/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include <algorithm>

#include "nsDependentString.h"
#include "SpdyPush31.h"

namespace mozilla {
namespace net {

//////////////////////////////////////////
// SpdyPushedStream31
//////////////////////////////////////////

SpdyPushedStream31::SpdyPushedStream31(SpdyPush31TransactionBuffer *aTransaction,
                                       SpdySession31 *aSession,
                                       SpdyStream31 *aAssociatedStream,
                                       uint32_t aID)
  :SpdyStream31(aTransaction, aSession,
                0 /* priority is only for sending, so ignore it on push */)
  , mConsumerStream(nullptr)
  , mBufferedPush(aTransaction)
  , mStatus(NS_OK)
  , mPushCompleted(false)
  , mDeferCleanupOnSuccess(true)
{
  LOG3(("SpdyPushedStream31 ctor this=%p id=0x%X\n", this, aID));
  mStreamID = aID;
  mBufferedPush->SetPushStream(this);
  mLoadGroupCI = aAssociatedStream->LoadGroupConnectionInfo();
  mLastRead = TimeStamp::Now();
}

bool
SpdyPushedStream31::GetPushComplete()
{
  return mPushCompleted;
}

nsresult
SpdyPushedStream31::WriteSegments(nsAHttpSegmentWriter *writer,
                                  uint32_t count,
                                  uint32_t *countWritten)
{
  nsresult rv = SpdyStream31::WriteSegments(writer, count, countWritten);
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

nsresult
SpdyPushedStream31::ReadSegments(nsAHttpSegmentReader *,  uint32_t, uint32_t *count)
{
  // The SYN_STREAM for this has been processed, so we need to verify
  // that :host, :scheme, and :path MUST be present
  nsDependentCSubstring host, scheme, path;
  nsresult rv;

  rv = SpdyStream31::FindHeader(NS_LITERAL_CSTRING(":host"), host);
  if (NS_FAILED(rv)) {
    LOG3(("SpdyPushedStream31::ReadSegments session=%p ID 0x%X "
          "push without required :host\n", mSession, mStreamID));
    return rv;
  }

  rv = SpdyStream31::FindHeader(NS_LITERAL_CSTRING(":scheme"), scheme);
  if (NS_FAILED(rv)) {
    LOG3(("SpdyPushedStream31::ReadSegments session=%p ID 0x%X "
          "push without required :scheme\n", mSession, mStreamID));
    return rv;
  }

  rv = SpdyStream31::FindHeader(NS_LITERAL_CSTRING(":path"), path);
  if (NS_FAILED(rv)) {
    LOG3(("SpdyPushedStream31::ReadSegments session=%p ID 0x%X "
          "push without required :host\n", mSession, mStreamID));
    return rv;
  }

  CreatePushHashKey(nsCString(scheme), nsCString(host),
                    mSession->Serial(), path,
                    mOrigin, mHashKey);

  LOG3(("SpdyPushStream31 0x%X hash key %s\n", mStreamID, mHashKey.get()));

  // the write side of a pushed transaction just involves manipulating a little state
  SpdyStream31::mSentFinOnData = 1;
  SpdyStream31::mSynFrameComplete = 1;
  SpdyStream31::ChangeState(UPSTREAM_COMPLETE);
  *count = 0;
  return NS_OK;
}

bool
SpdyPushedStream31::GetHashKey(nsCString &key)
{
  if (mHashKey.IsEmpty())
    return false;

  key = mHashKey;
  return true;
}

void
SpdyPushedStream31::ConnectPushedStream(SpdyStream31 *stream)
{
  mSession->ConnectPushedStream(stream);
}

bool
SpdyPushedStream31::IsOrphaned(TimeStamp now)
{
  MOZ_ASSERT(!now.IsNull());

  // if spdy is not transmitting, and is also not connected to a consumer
  // stream, and its been like that for too long then it is oprhaned

  if (mConsumerStream)
    return false;

  bool rv = ((now - mLastRead).ToSeconds() > 30.0);
  if (rv) {
    LOG3(("SpdyPushedStream31::IsOrphaned 0x%X IsOrphaned %3.2f\n",
          mStreamID, (now - mLastRead).ToSeconds()));
  }
  return rv;
}

nsresult
SpdyPushedStream31::GetBufferedData(char *buf,
                                    uint32_t count,
                                    uint32_t *countWritten)
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
// SpdyPush31TransactionBuffer
// This is the nsAHttpTransction owned by the stream when the pushed
// stream has not yet been matched with a pull request
//////////////////////////////////////////

NS_IMPL_ISUPPORTS0(SpdyPush31TransactionBuffer)

SpdyPush31TransactionBuffer::SpdyPush31TransactionBuffer()
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

SpdyPush31TransactionBuffer::~SpdyPush31TransactionBuffer()
{
  delete mRequestHead;
}

void
SpdyPush31TransactionBuffer::SetConnection(nsAHttpConnection *conn)
{
}

nsAHttpConnection *
SpdyPush31TransactionBuffer::Connection()
{
  return nullptr;
}

void
SpdyPush31TransactionBuffer::GetSecurityCallbacks(nsIInterfaceRequestor **outCB)
{
  *outCB = nullptr;
}

void
SpdyPush31TransactionBuffer::OnTransportStatus(nsITransport* transport,
                                               nsresult status, uint64_t progress)
{
}

bool
SpdyPush31TransactionBuffer::IsDone()
{
  return mIsDone;
}

nsresult
SpdyPush31TransactionBuffer::Status()
{
  return mStatus;
}

uint32_t
SpdyPush31TransactionBuffer::Caps()
{
  return 0;
}

uint64_t
SpdyPush31TransactionBuffer::Available()
{
  return mBufferedHTTP1Used - mBufferedHTTP1Consumed;
}

nsresult
SpdyPush31TransactionBuffer::ReadSegments(nsAHttpSegmentReader *reader,
                                            uint32_t count, uint32_t *countRead)
{
  *countRead = 0;
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
SpdyPush31TransactionBuffer::WriteSegments(nsAHttpSegmentWriter *writer,
                                           uint32_t count, uint32_t *countWritten)
{
  if ((mBufferedHTTP1Size - mBufferedHTTP1Used) < 20480) {
    SpdySession31::EnsureBuffer(mBufferedHTTP1,
                                mBufferedHTTP1Size + kDefaultBufferSize,
                                mBufferedHTTP1Used,
                                mBufferedHTTP1Size);
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

  if (Available()) {
    SpdyStream31 *consumer = mPushStream->GetConsumerStream();

    if (consumer) {
      LOG3(("SpdyPush31TransactionBuffer::WriteSegments notifying connection "
            "consumer data available 0x%X [%u]\n",
            mPushStream->StreamID(), Available()));
      mPushStream->ConnectPushedStream(consumer);
    }
  }

  return rv;
}

uint32_t
SpdyPush31TransactionBuffer::Http1xTransactionCount()
{
  return 0;
}

nsHttpRequestHead *
SpdyPush31TransactionBuffer::RequestHead()
{
  if (!mRequestHead)
    mRequestHead = new nsHttpRequestHead();
  return mRequestHead;
}

nsresult
SpdyPush31TransactionBuffer::TakeSubTransactions(
  nsTArray<nsRefPtr<nsAHttpTransaction> > &outTransactions)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

void
SpdyPush31TransactionBuffer::SetProxyConnectFailed()
{
}

void
SpdyPush31TransactionBuffer::Close(nsresult reason)
{
  mStatus = reason;
  mIsDone = true;
}

nsresult
SpdyPush31TransactionBuffer::AddTransaction(nsAHttpTransaction *trans)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

uint32_t
SpdyPush31TransactionBuffer::PipelineDepth()
{
  return 0;
}

nsresult
SpdyPush31TransactionBuffer::SetPipelinePosition(int32_t position)
{
  return NS_OK;
}

int32_t
SpdyPush31TransactionBuffer::PipelinePosition()
{
  return 1;
}

nsresult
SpdyPush31TransactionBuffer::GetBufferedData(char *buf,
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
