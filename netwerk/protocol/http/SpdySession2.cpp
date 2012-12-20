/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHttp.h"
#include "SpdySession2.h"
#include "SpdyStream2.h"
#include "nsHttpConnection.h"
#include "nsHttpHandler.h"
#include "prnetdb.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Preferences.h"
#include "prprf.h"
#include <algorithm>

#ifdef DEBUG
// defined by the socket transport service while active
extern PRThread *gSocketThread;
#endif

namespace mozilla {
namespace net {

// SpdySession2 has multiple inheritance of things that implement
// nsISupports, so this magic is taken from nsHttpPipeline that
// implements some of the same abstract classes.
NS_IMPL_THREADSAFE_ADDREF(SpdySession2)
NS_IMPL_THREADSAFE_RELEASE(SpdySession2)
NS_INTERFACE_MAP_BEGIN(SpdySession2)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsAHttpConnection)
NS_INTERFACE_MAP_END

SpdySession2::SpdySession2(nsAHttpTransaction *aHttpTransaction,
                         nsISocketTransport *aSocketTransport,
                         int32_t firstPriority)
  : mSocketTransport(aSocketTransport),
    mSegmentReader(nullptr),
    mSegmentWriter(nullptr),
    mSendingChunkSize(ASpdySession::kSendingChunkSize),
    mNextStreamID(1),
    mConcurrentHighWater(0),
    mDownstreamState(BUFFERING_FRAME_HEADER),
    mInputFrameBufferSize(kDefaultBufferSize),
    mInputFrameBufferUsed(0),
    mInputFrameDataLast(false),
    mInputFrameDataStream(nullptr),
    mNeedsCleanup(nullptr),
    mDecompressBufferSize(kDefaultBufferSize),
    mDecompressBufferUsed(0),
    mShouldGoAway(false),
    mClosed(false),
    mCleanShutdown(false),
    mGoAwayID(0),
    mMaxConcurrent(kDefaultMaxConcurrent),
    mConcurrent(0),
    mServerPushedResources(0),
    mOutputQueueSize(kDefaultQueueSize),
    mOutputQueueUsed(0),
    mOutputQueueSent(0),
    mLastReadEpoch(PR_IntervalNow()),
    mPingSentEpoch(0),
    mNextPingID(1)
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");

  LOG3(("SpdySession2::SpdySession2 %p transaction 1 = %p",
        this, aHttpTransaction));
  
  mStreamIDHash.Init();
  mStreamTransactionHash.Init();
  mConnection = aHttpTransaction->Connection();
  mInputFrameBuffer = new char[mInputFrameBufferSize];
  mDecompressBuffer = new char[mDecompressBufferSize];
  mOutputQueueBuffer = new char[mOutputQueueSize];
  zlibInit();
  
  mSendingChunkSize = gHttpHandler->SpdySendingChunkSize();
  if (!aHttpTransaction->IsNullTransaction())
    AddStream(aHttpTransaction, firstPriority);
  mLastDataReadEpoch = mLastReadEpoch;
  
  mPingThreshold = gHttpHandler->SpdyPingThreshold();
}

PLDHashOperator
SpdySession2::ShutdownEnumerator(nsAHttpTransaction *key,
                                nsAutoPtr<SpdyStream2> &stream,
                                void *closure)
{
  SpdySession2 *self = static_cast<SpdySession2 *>(closure);
 
  // On a clean server hangup the server sets the GoAwayID to be the ID of
  // the last transaction it processed. If the ID of stream in the
  // local stream is greater than that it can safely be restarted because the
  // server guarantees it was not partially processed. Streams that have not
  // registered an ID haven't actually been sent yet so they can always be
  // restarted.
  if (self->mCleanShutdown &&
      (stream->StreamID() > self->mGoAwayID || !stream->HasRegisteredID()))
    self->CloseStream(stream, NS_ERROR_NET_RESET); // can be restarted
  else
    self->CloseStream(stream, NS_ERROR_ABORT);

  return PL_DHASH_NEXT;
}

PLDHashOperator
SpdySession2::GoAwayEnumerator(nsAHttpTransaction *key,
                               nsAutoPtr<SpdyStream2> &stream,
                               void *closure)
{
  SpdySession2 *self = static_cast<SpdySession2 *>(closure);

  // these streams were not processed by the server and can be restarted.
  // Do that after the enumerator completes to avoid the risk of
  // a restart event re-entrantly modifying this hash.
  if (stream->StreamID() > self->mGoAwayID || !stream->HasRegisteredID())
    self->mGoAwayStreamsToRestart.Push(stream);

  return PL_DHASH_NEXT;
}

SpdySession2::~SpdySession2()
{
  LOG3(("SpdySession2::~SpdySession2 %p mDownstreamState=%X",
        this, mDownstreamState));

  inflateEnd(&mDownstreamZlib);
  deflateEnd(&mUpstreamZlib);
  
  mStreamTransactionHash.Enumerate(ShutdownEnumerator, this);
  Telemetry::Accumulate(Telemetry::SPDY_PARALLEL_STREAMS, mConcurrentHighWater);
  Telemetry::Accumulate(Telemetry::SPDY_REQUEST_PER_CONN, (mNextStreamID - 1) / 2);
  Telemetry::Accumulate(Telemetry::SPDY_SERVER_INITIATED_STREAMS,
                        mServerPushedResources);
}

void
SpdySession2::LogIO(SpdySession2 *self, SpdyStream2 *stream, const char *label,
                   const char *data, uint32_t datalen)
{
  if (!LOG4_ENABLED())
    return;
  
  LOG4(("SpdySession2::LogIO %p stream=%p id=0x%X [%s]",
        self, stream, stream ? stream->StreamID() : 0, label));

  // Max line is (16 * 3) + 10(prefix) + newline + null
  char linebuf[128];
  uint32_t index;
  char *line = linebuf;

  linebuf[127] = 0;

  for (index = 0; index < datalen; ++index) {
    if (!(index % 16)) {
      if (index) {
        *line = 0;
        LOG4(("%s", linebuf));
      }
      line = linebuf;
      PR_snprintf(line, 128, "%08X: ", index);
      line += 10;
    }
    PR_snprintf(line, 128 - (line - linebuf), "%02X ",
                ((unsigned char *)data)[index]);
    line += 3;
  }
  if (index) {
    *line = 0;
    LOG4(("%s", linebuf));
  }
}

typedef nsresult  (*Control_FX) (SpdySession2 *self);
static Control_FX sControlFunctions[] = 
{
  nullptr,
  SpdySession2::HandleSynStream,
  SpdySession2::HandleSynReply,
  SpdySession2::HandleRstStream,
  SpdySession2::HandleSettings,
  SpdySession2::HandleNoop,
  SpdySession2::HandlePing,
  SpdySession2::HandleGoAway,
  SpdySession2::HandleHeaders,
  SpdySession2::HandleWindowUpdate
};

bool
SpdySession2::RoomForMoreConcurrent()
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");

  return (mConcurrent < mMaxConcurrent);
}

bool
SpdySession2::RoomForMoreStreams()
{
  if (mNextStreamID + mStreamTransactionHash.Count() * 2 > kMaxStreamID)
    return false;

  return !mShouldGoAway;
}

PRIntervalTime
SpdySession2::IdleTime()
{
  return PR_IntervalNow() - mLastDataReadEpoch;
}

void
SpdySession2::ReadTimeoutTick(PRIntervalTime now)
{
    NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");
    NS_ABORT_IF_FALSE(mNextPingID & 1, "Ping Counter Not Odd");

    if (!mPingThreshold)
      return;

    LOG(("SpdySession2::ReadTimeoutTick %p delta since last read %ds\n",
         this, PR_IntervalToSeconds(now - mLastReadEpoch)));

    if ((now - mLastReadEpoch) < mPingThreshold) {
      // recent activity means ping is not an issue
      if (mPingSentEpoch)
        mPingSentEpoch = 0;
      return;
    }

    if (mPingSentEpoch) {
      LOG(("SpdySession2::ReadTimeoutTick %p handle outstanding ping\n"));
      if ((now - mPingSentEpoch) >= gHttpHandler->SpdyPingTimeout()) {
        LOG(("SpdySession2::ReadTimeoutTick %p Ping Timer Exhaustion\n",
             this));
        mPingSentEpoch = 0;
        Close(NS_ERROR_NET_TIMEOUT);
      }
      return;
    }
    
    LOG(("SpdySession2::ReadTimeoutTick %p generating ping 0x%x\n",
         this, mNextPingID));

    if (mNextPingID == 0xffffffff) {
      LOG(("SpdySession2::ReadTimeoutTick %p cannot form ping - ids exhausted\n",
           this));
      return;
    }

    mPingSentEpoch = PR_IntervalNow();
    if (!mPingSentEpoch)
      mPingSentEpoch = 1; // avoid the 0 sentinel value
    GeneratePing(mNextPingID);
    mNextPingID += 2;
    ResumeRecv(); // read the ping reply

    if (mNextPingID == 0xffffffff) {
      LOG(("SpdySession2::ReadTimeoutTick %p "
           "ping ids exhausted marking goaway\n", this));
      mShouldGoAway = true;
    }
}

uint32_t
SpdySession2::RegisterStreamID(SpdyStream2 *stream)
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");

  LOG3(("SpdySession2::RegisterStreamID session=%p stream=%p id=0x%X "
        "concurrent=%d",this, stream, mNextStreamID, mConcurrent));

  NS_ABORT_IF_FALSE(mNextStreamID < 0xfffffff0,
                    "should have stopped admitting streams");
  
  uint32_t result = mNextStreamID;
  mNextStreamID += 2;

  // We've used up plenty of ID's on this session. Start
  // moving to a new one before there is a crunch involving
  // server push streams or concurrent non-registered submits
  if (mNextStreamID >= kMaxStreamID)
    mShouldGoAway = true;

  // integrity check
  if (mStreamIDHash.Get(result)) {
    LOG3(("   New ID already present\n"));
    NS_ABORT_IF_FALSE(false, "New ID already present in mStreamIDHash");
    mShouldGoAway = true;
    return kDeadStreamID;
  }

  mStreamIDHash.Put(result, stream);
  return result;
}

bool
SpdySession2::AddStream(nsAHttpTransaction *aHttpTransaction,
                       int32_t aPriority)
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");

  // integrity check
  if (mStreamTransactionHash.Get(aHttpTransaction)) {
    LOG3(("   New transaction already present\n"));
    NS_ABORT_IF_FALSE(false, "AddStream duplicate transaction pointer");
    return false;
  }

  aHttpTransaction->SetConnection(this);
  SpdyStream2 *stream = new SpdyStream2(aHttpTransaction,
                                      this,
                                      mSocketTransport,
                                      mSendingChunkSize,
                                      &mUpstreamZlib,
                                      aPriority);

  
  LOG3(("SpdySession2::AddStream session=%p stream=%p NextID=0x%X (tentative)",
        this, stream, mNextStreamID));

  mStreamTransactionHash.Put(aHttpTransaction, stream);

  if (RoomForMoreConcurrent()) {
    LOG3(("SpdySession2::AddStream %p stream %p activated immediately.",
          this, stream));
    ActivateStream(stream);
  }
  else {
    LOG3(("SpdySession2::AddStream %p stream %p queued.",
          this, stream));
    mQueuedStreams.Push(stream);
  }
  
  return true;
}

void
SpdySession2::ActivateStream(SpdyStream2 *stream)
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");

  mConcurrent++;
  if (mConcurrent > mConcurrentHighWater)
    mConcurrentHighWater = mConcurrent;
  LOG3(("SpdySession2::AddStream %p activating stream %p Currently %d "
        "streams in session, high water mark is %d",
        this, stream, mConcurrent, mConcurrentHighWater));

  mReadyForWrite.Push(stream);
  SetWriteCallbacks();

  // Kick off the SYN transmit without waiting for the poll loop
  // This won't work for stream id=1 because there is no segment reader
  // yet.
  if (mSegmentReader) {
    uint32_t countRead;
    ReadSegments(nullptr, kDefaultBufferSize, &countRead);
  }
}

void
SpdySession2::ProcessPending()
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");

  while (RoomForMoreConcurrent()) {
    SpdyStream2 *stream = static_cast<SpdyStream2 *>(mQueuedStreams.PopFront());
    if (!stream)
      return;
    LOG3(("SpdySession2::ProcessPending %p stream %p activated from queue.",
          this, stream));
    ActivateStream(stream);
  }
}

nsresult
SpdySession2::NetworkRead(nsAHttpSegmentWriter *writer, char *buf,
                         uint32_t count, uint32_t *countWritten)
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");

  if (!count) {
    *countWritten = 0;
    return NS_OK;
  }

  nsresult rv = writer->OnWriteSegment(buf, count, countWritten);
  if (NS_SUCCEEDED(rv) && *countWritten > 0)
    mLastReadEpoch = PR_IntervalNow();
  return rv;
}

void
SpdySession2::SetWriteCallbacks()
{
  if (mConnection && (GetWriteQueueSize() || mOutputQueueUsed))
      mConnection->ResumeSend();
}

void
SpdySession2::RealignOutputQueue()
{
  mOutputQueueUsed -= mOutputQueueSent;
  memmove(mOutputQueueBuffer.get(),
          mOutputQueueBuffer.get() + mOutputQueueSent,
          mOutputQueueUsed);
  mOutputQueueSent = 0;
}

void
SpdySession2::FlushOutputQueue()
{
  if (!mSegmentReader || !mOutputQueueUsed)
    return;
  
  nsresult rv;
  uint32_t countRead;
  uint32_t avail = mOutputQueueUsed - mOutputQueueSent;

  rv = mSegmentReader->
    OnReadSegment(mOutputQueueBuffer.get() + mOutputQueueSent, avail,
                                     &countRead);
  LOG3(("SpdySession2::FlushOutputQueue %p sz=%d rv=%x actual=%d",
        this, avail, rv, countRead));
  
  // Dont worry about errors on write, we will pick this up as a read error too
  if (NS_FAILED(rv))
    return;
  
  if (countRead == avail) {
    mOutputQueueUsed = 0;
    mOutputQueueSent = 0;
    return;
  }

  mOutputQueueSent += countRead;

  // If the output queue is close to filling up and we have sent out a good
  // chunk of data from the beginning then realign it.
  
  if ((mOutputQueueSent >= kQueueMinimumCleanup) &&
      ((mOutputQueueSize - mOutputQueueUsed) < kQueueTailRoom)) {
    RealignOutputQueue();
  }
}

void
SpdySession2::DontReuse()
{
  mShouldGoAway = true;
  if (!mStreamTransactionHash.Count())
    Close(NS_OK);
}

uint32_t
SpdySession2::GetWriteQueueSize()
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");

  return mUrgentForWrite.GetSize() + mReadyForWrite.GetSize();
}

void
SpdySession2::ChangeDownstreamState(enum stateType newState)
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");

  LOG3(("SpdyStream2::ChangeDownstreamState() %p from %X to %X",
        this, mDownstreamState, newState));
  mDownstreamState = newState;
}

void
SpdySession2::ResetDownstreamState()
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");

  LOG3(("SpdyStream2::ResetDownstreamState() %p", this));
  ChangeDownstreamState(BUFFERING_FRAME_HEADER);

  if (mInputFrameDataLast && mInputFrameDataStream) {
    mInputFrameDataLast = false;
    if (!mInputFrameDataStream->RecvdFin()) {
      mInputFrameDataStream->SetRecvdFin(true);
      --mConcurrent;
      ProcessPending();
    }
  }
  mInputFrameBufferUsed = 0;
  mInputFrameDataStream = nullptr;
}

void
SpdySession2::EnsureBuffer(nsAutoArrayPtr<char> &buf,
                          uint32_t newSize,
                          uint32_t preserve,
                          uint32_t &objSize)
{
  if (objSize >= newSize)
      return;
  
  // Leave a little slop on the new allocation - add 2KB to
  // what we need and then round the result up to a 4KB (page)
  // boundary.

  objSize = (newSize + 2048 + 4095) & ~4095;
  
  nsAutoArrayPtr<char> tmp(new char[objSize]);
  memcpy(tmp, buf, preserve);
  buf = tmp;
}

void
SpdySession2::zlibInit()
{
  mDownstreamZlib.zalloc = SpdyStream2::zlib_allocator;
  mDownstreamZlib.zfree = SpdyStream2::zlib_destructor;
  mDownstreamZlib.opaque = Z_NULL;

  inflateInit(&mDownstreamZlib);

  mUpstreamZlib.zalloc = SpdyStream2::zlib_allocator;
  mUpstreamZlib.zfree = SpdyStream2::zlib_destructor;
  mUpstreamZlib.opaque = Z_NULL;

  // mixing carte blanche compression with tls subjects us to traffic
  // analysis attacks
  deflateInit(&mUpstreamZlib, Z_NO_COMPRESSION);
  deflateSetDictionary(&mUpstreamZlib,
                       reinterpret_cast<const unsigned char *>
                       (SpdyStream2::kDictionary),
                       strlen(SpdyStream2::kDictionary) + 1);

}

nsresult
SpdySession2::DownstreamUncompress(char *blockStart, uint32_t blockLen)
{
  mDecompressBufferUsed = 0;

  mDownstreamZlib.avail_in = blockLen;
  mDownstreamZlib.next_in = reinterpret_cast<unsigned char *>(blockStart);
  bool triedDictionary = false;

  do {
    mDownstreamZlib.next_out =
      reinterpret_cast<unsigned char *>(mDecompressBuffer.get()) +
      mDecompressBufferUsed;
    mDownstreamZlib.avail_out = mDecompressBufferSize - mDecompressBufferUsed;
    int zlib_rv = inflate(&mDownstreamZlib, Z_NO_FLUSH);

    if (zlib_rv == Z_NEED_DICT) {
      if (triedDictionary) {
        LOG3(("SpdySession2::DownstreamUncompress %p Dictionary Error\n", this));
        return NS_ERROR_FAILURE;
      }
      
      triedDictionary = true;
      inflateSetDictionary(&mDownstreamZlib,
                           reinterpret_cast<const unsigned char *>
                           (SpdyStream2::kDictionary),
                           strlen(SpdyStream2::kDictionary) + 1);
    }
    
    if (zlib_rv == Z_DATA_ERROR || zlib_rv == Z_MEM_ERROR)
      return NS_ERROR_FAILURE;

    mDecompressBufferUsed += mDecompressBufferSize - mDecompressBufferUsed -
      mDownstreamZlib.avail_out;
    
    // When there is no more output room, but input still available then
    // increase the output space
    if (zlib_rv == Z_OK &&
        !mDownstreamZlib.avail_out && mDownstreamZlib.avail_in) {
      LOG3(("SpdySession2::DownstreamUncompress %p Large Headers - so far %d",
            this, mDecompressBufferSize));
      EnsureBuffer(mDecompressBuffer,
                   mDecompressBufferSize + 4096,
                   mDecompressBufferUsed,
                   mDecompressBufferSize);
    }
  }
  while (mDownstreamZlib.avail_in);
  return NS_OK;
}

nsresult
SpdySession2::FindHeader(nsCString name,
                        nsDependentCSubstring &value)
{
  const unsigned char *nvpair = reinterpret_cast<unsigned char *>
    (mDecompressBuffer.get()) + 2;
  const unsigned char *lastHeaderByte = reinterpret_cast<unsigned char *>
    (mDecompressBuffer.get()) + mDecompressBufferUsed;
  if (lastHeaderByte < nvpair)
    return NS_ERROR_ILLEGAL_VALUE;
  uint16_t numPairs =
    PR_ntohs(reinterpret_cast<uint16_t *>(mDecompressBuffer.get())[0]);
  for (uint16_t index = 0; index < numPairs; ++index) {
    if (lastHeaderByte < nvpair + 2)
      return NS_ERROR_ILLEGAL_VALUE;
    uint32_t nameLen = (nvpair[0] << 8) + nvpair[1];
    if (lastHeaderByte < nvpair + 2 + nameLen)
      return NS_ERROR_ILLEGAL_VALUE;
    nsDependentCSubstring nameString =
      Substring(reinterpret_cast<const char *>(nvpair) + 2,
                reinterpret_cast<const char *>(nvpair) + 2 + nameLen);
    if (lastHeaderByte < nvpair + 4 + nameLen)
      return NS_ERROR_ILLEGAL_VALUE;
    uint16_t valueLen = (nvpair[2 + nameLen] << 8) + nvpair[3 + nameLen];
    if (lastHeaderByte < nvpair + 4 + nameLen + valueLen)
      return NS_ERROR_ILLEGAL_VALUE;
    if (nameString.Equals(name)) {
      value.Assign(((char *)nvpair) + 4 + nameLen, valueLen);
      return NS_OK;
    }
    nvpair += 4 + nameLen + valueLen;
  }
  return NS_ERROR_NOT_AVAILABLE;
}

nsresult
SpdySession2::ConvertHeaders(nsDependentCSubstring &status,
                            nsDependentCSubstring &version)
{
  mFlatHTTPResponseHeaders.Truncate();
  mFlatHTTPResponseHeadersOut = 0;
  mFlatHTTPResponseHeaders.SetCapacity(mDecompressBufferUsed + 64);

  // Connection, Keep-Alive and chunked transfer encodings are to be
  // removed.

  // Content-Length is 'advisory'.. we will not strip it because it can
  // create UI feedback.
  
  mFlatHTTPResponseHeaders.Append(version);
  mFlatHTTPResponseHeaders.Append(NS_LITERAL_CSTRING(" "));
  mFlatHTTPResponseHeaders.Append(status);
  mFlatHTTPResponseHeaders.Append(NS_LITERAL_CSTRING("\r\n"));

  const unsigned char *nvpair = reinterpret_cast<unsigned char *>
    (mDecompressBuffer.get()) + 2;
  const unsigned char *lastHeaderByte = reinterpret_cast<unsigned char *>
    (mDecompressBuffer.get()) + mDecompressBufferUsed;

  if (lastHeaderByte < nvpair)
    return NS_ERROR_ILLEGAL_VALUE;

  uint16_t numPairs =
    PR_ntohs(reinterpret_cast<uint16_t *>(mDecompressBuffer.get())[0]);

  for (uint16_t index = 0; index < numPairs; ++index) {
    if (lastHeaderByte < nvpair + 2)
      return NS_ERROR_ILLEGAL_VALUE;

    uint32_t nameLen = (nvpair[0] << 8) + nvpair[1];
    if (lastHeaderByte < nvpair + 2 + nameLen)
      return NS_ERROR_ILLEGAL_VALUE;

    nsDependentCSubstring nameString =
      Substring(reinterpret_cast<const char *>(nvpair) + 2,
                reinterpret_cast<const char *>(nvpair) + 2 + nameLen);

    if (lastHeaderByte < nvpair + 4 + nameLen)
      return NS_ERROR_ILLEGAL_VALUE;

    // Look for illegal characters in the nameString.
    // This includes upper case characters and nulls (as they will
    // break the fixup-nulls-in-value-string algorithm)
    // Look for upper case characters in the name. They are illegal.
    for (char *cPtr = nameString.BeginWriting();
         cPtr && cPtr < nameString.EndWriting();
         ++cPtr) {
      if (*cPtr <= 'Z' && *cPtr >= 'A') {
        nsCString toLog(nameString);

        LOG3(("SpdySession2::ConvertHeaders session=%p stream=%p "
              "upper case response header found. [%s]\n",
              this, mInputFrameDataStream, toLog.get()));

        return NS_ERROR_ILLEGAL_VALUE;
      }

      // check for null characters
      if (*cPtr == '\0')
        return NS_ERROR_ILLEGAL_VALUE;
    }

    // HTTP Chunked responses are not legal over spdy. We do not need
    // to look for chunked specifically because it is the only HTTP
    // allowed default encoding and we did not negotiate further encodings
    // via TE
    if (nameString.Equals(NS_LITERAL_CSTRING("transfer-encoding"))) {
      LOG3(("SpdySession2::ConvertHeaders session=%p stream=%p "
            "transfer-encoding found. Chunked is invalid and no TE sent.",
            this, mInputFrameDataStream));

      return NS_ERROR_ILLEGAL_VALUE;
    }

    uint16_t valueLen = (nvpair[2 + nameLen] << 8) + nvpair[3 + nameLen];
    if (lastHeaderByte < nvpair + 4 + nameLen + valueLen)
      return NS_ERROR_ILLEGAL_VALUE;

    if (!nameString.Equals(NS_LITERAL_CSTRING("version")) &&
        !nameString.Equals(NS_LITERAL_CSTRING("status")) &&
        !nameString.Equals(NS_LITERAL_CSTRING("connection")) &&
        !nameString.Equals(NS_LITERAL_CSTRING("keep-alive"))) {
      nsDependentCSubstring valueString =
        Substring(reinterpret_cast<const char *>(nvpair) + 4 + nameLen,
                  reinterpret_cast<const char *>(nvpair) + 4 + nameLen +
                  valueLen);
      
      mFlatHTTPResponseHeaders.Append(nameString);
      mFlatHTTPResponseHeaders.Append(NS_LITERAL_CSTRING(": "));

      // expand NULL bytes in the value string
      for (char *cPtr = valueString.BeginWriting();
           cPtr && cPtr < valueString.EndWriting();
           ++cPtr) {
        if (*cPtr != 0) {
          mFlatHTTPResponseHeaders.Append(*cPtr);
          continue;
        }

        // NULLs are really "\r\nhdr: "
        mFlatHTTPResponseHeaders.Append(NS_LITERAL_CSTRING("\r\n"));
        mFlatHTTPResponseHeaders.Append(nameString);
        mFlatHTTPResponseHeaders.Append(NS_LITERAL_CSTRING(": "));
      }

      mFlatHTTPResponseHeaders.Append(NS_LITERAL_CSTRING("\r\n"));
    }
    nvpair += 4 + nameLen + valueLen;
  }

  mFlatHTTPResponseHeaders.Append(
    NS_LITERAL_CSTRING("X-Firefox-Spdy: 2\r\n\r\n"));
  LOG (("decoded response headers are:\n%s",
        mFlatHTTPResponseHeaders.get()));
  
  return NS_OK;
}

void
SpdySession2::GeneratePing(uint32_t aID)
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");
  LOG3(("SpdySession2::GeneratePing %p 0x%X\n", this, aID));

  EnsureBuffer(mOutputQueueBuffer, mOutputQueueUsed + 12,
               mOutputQueueUsed, mOutputQueueSize);
  char *packet = mOutputQueueBuffer.get() + mOutputQueueUsed;
  mOutputQueueUsed += 12;

  packet[0] = kFlag_Control;
  packet[1] = 2;                                  /* version 2 */
  packet[2] = 0;
  packet[3] = CONTROL_TYPE_PING;
  packet[4] = 0;                                  /* flags */
  packet[5] = 0;
  packet[6] = 0;
  packet[7] = 4;                                  /* length */
  
  aID = PR_htonl(aID);
  memcpy(packet + 8, &aID, 4);

  FlushOutputQueue();
}

void
SpdySession2::GenerateRstStream(uint32_t aStatusCode, uint32_t aID)
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");
  LOG3(("SpdySession2::GenerateRst %p 0x%X %d\n", this, aID, aStatusCode));

  EnsureBuffer(mOutputQueueBuffer, mOutputQueueUsed + 16,
               mOutputQueueUsed, mOutputQueueSize);
  char *packet = mOutputQueueBuffer.get() + mOutputQueueUsed;
  mOutputQueueUsed += 16;

  packet[0] = kFlag_Control;
  packet[1] = 2;                                  /* version 2 */
  packet[2] = 0;
  packet[3] = CONTROL_TYPE_RST_STREAM;
  packet[4] = 0;                                  /* flags */
  packet[5] = 0;
  packet[6] = 0;
  packet[7] = 8;                                  /* length */
  
  aID = PR_htonl(aID);
  memcpy(packet + 8, &aID, 4);
  aStatusCode = PR_htonl(aStatusCode);
  memcpy(packet + 12, &aStatusCode, 4);

  FlushOutputQueue();
}

void
SpdySession2::GenerateGoAway()
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");
  LOG3(("SpdySession2::GenerateGoAway %p\n", this));

  EnsureBuffer(mOutputQueueBuffer, mOutputQueueUsed + 12,
               mOutputQueueUsed, mOutputQueueSize);
  char *packet = mOutputQueueBuffer.get() + mOutputQueueUsed;
  mOutputQueueUsed += 12;

  memset(packet, 0, 12);
  packet[0] = kFlag_Control;
  packet[1] = 2;                                  /* version 2 */
  packet[3] = CONTROL_TYPE_GOAWAY;
  packet[7] = 4;                                  /* data length */
  
  // last-good-stream-id are bytes 8-11, when we accept server push this will
  // need to be set non zero

  FlushOutputQueue();
}

// perform a bunch of integrity checks on the stream.
// returns true if passed, false (plus LOG and ABORT) if failed.
bool
SpdySession2::VerifyStream(SpdyStream2 *aStream, uint32_t aOptionalID = 0)
{
  // This is annoying, but at least it is O(1)
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");

#ifndef DEBUG
  // Only do the real verification in debug builds
  return true;
#endif

  if (!aStream)
    return true;

  uint32_t test = 0;
  
  do {
    if (aStream->StreamID() == kDeadStreamID)
      break;

    nsAHttpTransaction *trans = aStream->Transaction();

    test++;  
    if (!trans)
      break;

    test++;
    if (mStreamTransactionHash.Get(trans) != aStream)
      break;
    
    if (aStream->StreamID()) {
      SpdyStream2 *idStream = mStreamIDHash.Get(aStream->StreamID());

      test++;
      if (idStream != aStream)
        break;

      if (aOptionalID) {
        test++;
        if (idStream->StreamID() != aOptionalID)
          break;
      }
    }

    // tests passed
    return true;
  } while (0);

  LOG(("SpdySession %p VerifyStream Failure %p stream->id=0x%x "
       "optionalID=0x%x trans=%p test=%d\n",
       this, aStream, aStream->StreamID(),
       aOptionalID, aStream->Transaction(), test));
  NS_ABORT_IF_FALSE(false, "VerifyStream");
  return false;
}

void
SpdySession2::CleanupStream(SpdyStream2 *aStream, nsresult aResult,
                           rstReason aResetCode)
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");
  LOG3(("SpdySession2::CleanupStream %p %p 0x%x %X\n",
        this, aStream, aStream->StreamID(), aResult));

  if (!VerifyStream(aStream)) {
    LOG(("SpdySession2::CleanupStream failed to verify stream\n"));
    return;
  }

  if (!aStream->RecvdFin() && aStream->StreamID()) {
    LOG3(("Stream had not processed recv FIN, sending RST code %X\n",
          aResetCode));
    GenerateRstStream(aResetCode, aStream->StreamID());
    --mConcurrent;
    ProcessPending();
  }
  
  CloseStream(aStream, aResult);

  // Remove the stream from the ID hash table. (this one isn't short, which is
  // why it is hashed.)
  mStreamIDHash.Remove(aStream->StreamID());

  // removing from the stream transaction hash will
  // delete the SpdyStream2 and drop the reference to
  // its transaction
  mStreamTransactionHash.Remove(aStream->Transaction());

  if (mShouldGoAway && !mStreamTransactionHash.Count())
    Close(NS_OK);
}

void
SpdySession2::CloseStream(SpdyStream2 *aStream, nsresult aResult)
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");
  LOG3(("SpdySession2::CloseStream %p %p 0x%x %X\n",
        this, aStream, aStream->StreamID(), aResult));

  // Check if partial frame reader
  if (aStream == mInputFrameDataStream) {
    LOG3(("Stream had active partial read frame on close"));
    ChangeDownstreamState(DISCARDING_DATA_FRAME);
    mInputFrameDataStream = nullptr;
  }

  // check the streams blocked on write, this is linear but the list
  // should be pretty short.
  uint32_t size = mReadyForWrite.GetSize();
  for (uint32_t count = 0; count < size; ++count) {
    SpdyStream2 *stream = static_cast<SpdyStream2 *>(mReadyForWrite.PopFront());
    if (stream != aStream)
      mReadyForWrite.Push(stream);
  }

  // Check the streams blocked on urgent (i.e. window update) writing.
  // This should also be short.
  size = mUrgentForWrite.GetSize();
  for (uint32_t count = 0; count < size; ++count) {
    SpdyStream2 *stream = static_cast<SpdyStream2 *>(mUrgentForWrite.PopFront());
    if (stream != aStream)
      mUrgentForWrite.Push(stream);
  }

  // Check the streams queued for activation. Because we normally accept a high
  // level of parallelization this should also be short.
  size = mQueuedStreams.GetSize();
  for (uint32_t count = 0; count < size; ++count) {
    SpdyStream2 *stream = static_cast<SpdyStream2 *>(mQueuedStreams.PopFront());
    if (stream != aStream)
      mQueuedStreams.Push(stream);
  }

  // Send the stream the close() indication
  aStream->Close(aResult);
}

nsresult
SpdySession2::HandleSynStream(SpdySession2 *self)
{
  NS_ABORT_IF_FALSE(self->mFrameControlType == CONTROL_TYPE_SYN_STREAM,
                    "wrong control type");
  
  if (self->mInputFrameDataSize < 18) {
    LOG3(("SpdySession2::HandleSynStream %p SYN_STREAM too short data=%d",
          self, self->mInputFrameDataSize));
    return NS_ERROR_ILLEGAL_VALUE;
  }

  uint32_t streamID =
    PR_ntohl(reinterpret_cast<uint32_t *>(self->mInputFrameBuffer.get())[2]);
  uint32_t associatedID =
    PR_ntohl(reinterpret_cast<uint32_t *>(self->mInputFrameBuffer.get())[3]);

  LOG3(("SpdySession2::HandleSynStream %p recv SYN_STREAM (push) "
        "for ID 0x%X associated with 0x%X.",
        self, streamID, associatedID));
    
  if (streamID & 0x01) {                   // test for odd stream ID
    LOG3(("SpdySession2::HandleSynStream %p recvd SYN_STREAM id must be even.",
          self));
    return NS_ERROR_ILLEGAL_VALUE;
  }

  ++(self->mServerPushedResources);

  // Anytime we start using the high bit of stream ID (either client or server)
  // begin to migrate to a new session.
  if (streamID >= kMaxStreamID)
    self->mShouldGoAway = true;

  // Need to decompress the headers even though we aren't using them yet in
  // order to keep the compression context consistent for other syn_reply frames
  nsresult rv = self->DownstreamUncompress(self->mInputFrameBuffer + 18,
                                           self->mInputFrameDataSize - 10);
  if (NS_FAILED(rv)) {
    LOG(("SpdySession2::HandleSynStream uncompress failed\n"));
    return rv;
  }

  // todo populate cache. For now, just reject server push p3
  self->GenerateRstStream(RST_REFUSED_STREAM, streamID);
  self->ResetDownstreamState();
  return NS_OK;
}

nsresult
SpdySession2::SetInputFrameDataStream(uint32_t streamID)
{
  mInputFrameDataStream = mStreamIDHash.Get(streamID);
  if (VerifyStream(mInputFrameDataStream, streamID))
    return NS_OK;

  LOG(("SpdySession2::SetInputFrameDataStream failed to verify 0x%X\n",
       streamID));
  mInputFrameDataStream = nullptr;
  return NS_ERROR_UNEXPECTED;
}

nsresult
SpdySession2::HandleSynReply(SpdySession2 *self)
{
  NS_ABORT_IF_FALSE(self->mFrameControlType == CONTROL_TYPE_SYN_REPLY,
                    "wrong control type");

  if (self->mInputFrameDataSize < 8) {
    LOG3(("SpdySession2::HandleSynReply %p SYN REPLY too short data=%d",
          self, self->mInputFrameDataSize));
    // A framing error is a session wide error that cannot be recovered
    return NS_ERROR_ILLEGAL_VALUE;
  }
  
  // Uncompress the headers into mDecompressBuffer, leaving them in
  // spdy format for the time being. Make certain to do this
  // step before any error handling that might abort the stream but not
  // the session becuase the session compression context will become
  // inconsistent if all of the compressed data is not processed.
  if (NS_FAILED(self->DownstreamUncompress(self->mInputFrameBuffer + 14,
                                           self->mInputFrameDataSize - 6))) {
    LOG(("SpdySession2::HandleSynReply uncompress failed\n"));
    return NS_ERROR_FAILURE;
  }

  LOG3(("SpdySession2::HandleSynReply %p lookup via streamID in syn_reply.\n",
        self));
  uint32_t streamID =
    PR_ntohl(reinterpret_cast<uint32_t *>(self->mInputFrameBuffer.get())[2]);
  nsresult rv = self->SetInputFrameDataStream(streamID);
  if (NS_FAILED(rv))
    return rv;

  if (!self->mInputFrameDataStream) {
    LOG3(("SpdySession2::HandleSynReply %p lookup streamID in syn_reply "
          "0x%X failed. NextStreamID = 0x%x", self, streamID,
          self->mNextStreamID));
    if (streamID >= self->mNextStreamID)
      self->GenerateRstStream(RST_INVALID_STREAM, streamID);

    self->ResetDownstreamState();
    return NS_OK;
  }

  rv = self->HandleSynReplyForValidStream();
  if (rv == NS_ERROR_ILLEGAL_VALUE) {
    LOG3(("SpdySession2::HandleSynReply %p PROTOCOL_ERROR detected 0x%X\n",
          self, streamID));
    self->CleanupStream(self->mInputFrameDataStream, rv, RST_PROTOCOL_ERROR);
    self->ResetDownstreamState();
    rv = NS_OK;
  }

  return rv;
}

// HandleSynReplyForValidStream() returns NS_ERROR_ILLEGAL_VALUE when the stream
// should be reset with a PROTOCOL_ERROR, NS_OK when the SYN_REPLY was
// fine, and any other error is fatal to the session.
nsresult
SpdySession2::HandleSynReplyForValidStream()
{
  if (mInputFrameDataStream->GetFullyOpen()) {
    // "If an endpoint receives multiple SYN_REPLY frames for the same active
    // stream ID, it must drop the stream, and send a RST_STREAM for the
    // stream with the error PROTOCOL_ERROR."
    //
    // If the stream is open then just RST_STREAM and move on, otherwise
    // abort the session
    return mInputFrameDataStream->RecvdFin() ?
      NS_ERROR_ALREADY_OPENED : NS_ERROR_ILLEGAL_VALUE;
  }
  mInputFrameDataStream->SetFullyOpen();

  mInputFrameDataLast = mInputFrameBuffer[4] & kFlag_Data_FIN;

  if (mInputFrameBuffer[4] & kFlag_Data_UNI) {
    LOG3(("SynReply had unidirectional flag set on it - nonsensical"));
    return NS_ERROR_ILLEGAL_VALUE;
  }

  LOG3(("SpdySession2::HandleSynReplyForValidStream %p SYN_REPLY for 0x%X "
        "fin=%d",
        this, mInputFrameDataStream->StreamID(), mInputFrameDataLast));
  
  Telemetry::Accumulate(Telemetry::SPDY_SYN_REPLY_SIZE,
                        mInputFrameDataSize - 6);
  if (mDecompressBufferUsed) {
    uint32_t ratio =
      (mInputFrameDataSize - 6) * 100 / mDecompressBufferUsed;
    Telemetry::Accumulate(Telemetry::SPDY_SYN_REPLY_RATIO, ratio);
  }

  // status and version are required.
  nsDependentCSubstring status, version;
  nsresult rv = FindHeader(NS_LITERAL_CSTRING("status"), status);
  if (NS_FAILED(rv))
    return (rv == NS_ERROR_NOT_AVAILABLE) ? NS_ERROR_ILLEGAL_VALUE : rv;

  rv = FindHeader(NS_LITERAL_CSTRING("version"), version);
  if (NS_FAILED(rv))
    return (rv == NS_ERROR_NOT_AVAILABLE) ? NS_ERROR_ILLEGAL_VALUE : rv;

  // The spdystream needs to see flattened http headers
  // Uncompressed spdy format headers currently live in
  // mDeccompressBuffer - convert that to HTTP format in
  // mFlatHTTPResponseHeaders in ConvertHeaders()

  rv = ConvertHeaders(status, version);
  if (NS_FAILED(rv))
    return rv;

  mInputFrameDataStream->UpdateTransportReadEvents(mInputFrameDataSize);
  mLastDataReadEpoch = mLastReadEpoch;
  ChangeDownstreamState(PROCESSING_CONTROL_SYN_REPLY);
  return NS_OK;
}

nsresult
SpdySession2::HandleRstStream(SpdySession2 *self)
{
  NS_ABORT_IF_FALSE(self->mFrameControlType == CONTROL_TYPE_RST_STREAM,
                    "wrong control type");

  if (self->mInputFrameDataSize != 8) {
    LOG3(("SpdySession2::HandleRstStream %p RST_STREAM wrong length data=%d",
          self, self->mInputFrameDataSize));
    return NS_ERROR_ILLEGAL_VALUE;
  }

  uint8_t flags = reinterpret_cast<uint8_t *>(self->mInputFrameBuffer.get())[4];

  uint32_t streamID =
    PR_ntohl(reinterpret_cast<uint32_t *>(self->mInputFrameBuffer.get())[2]);

  self->mDownstreamRstReason =
    PR_ntohl(reinterpret_cast<uint32_t *>(self->mInputFrameBuffer.get())[3]);

  LOG3(("SpdySession2::HandleRstStream %p RST_STREAM Reason Code %u ID %x "
        "flags %x", self, self->mDownstreamRstReason, streamID, flags));

  if (flags != 0) {
    LOG3(("SpdySession2::HandleRstStream %p RST_STREAM with flags is illegal",
          self));
    return NS_ERROR_ILLEGAL_VALUE;
  }
  
  if (self->mDownstreamRstReason == RST_INVALID_STREAM ||
      self->mDownstreamRstReason == RST_FLOW_CONTROL_ERROR) {
    // basically just ignore this
    self->ResetDownstreamState();
    return NS_OK;
  }

  nsresult rv = self->SetInputFrameDataStream(streamID);
  
  if (!self->mInputFrameDataStream) {
    if (NS_FAILED(rv))
      LOG(("SpdySession2::HandleRstStream %p lookup streamID for RST Frame "
           "0x%X failed reason = %d :: VerifyStream Failed\n", self, streamID,
           self->mDownstreamRstReason));

    LOG3(("SpdySession2::HandleRstStream %p lookup streamID for RST Frame "
          "0x%X failed reason = %d", self, streamID,
          self->mDownstreamRstReason));

    return NS_ERROR_ILLEGAL_VALUE;
  }

  self->ChangeDownstreamState(PROCESSING_CONTROL_RST_STREAM);
  return NS_OK;
}

nsresult
SpdySession2::HandleSettings(SpdySession2 *self)
{
  NS_ABORT_IF_FALSE(self->mFrameControlType == CONTROL_TYPE_SETTINGS,
                    "wrong control type");

  if (self->mInputFrameDataSize < 4) {
    LOG3(("SpdySession2::HandleSettings %p SETTINGS wrong length data=%d",
          self, self->mInputFrameDataSize));
    return NS_ERROR_ILLEGAL_VALUE;
  }

  uint32_t numEntries =
    PR_ntohl(reinterpret_cast<uint32_t *>(self->mInputFrameBuffer.get())[2]);

  // Ensure frame is large enough for supplied number of entries
  // Each entry is 8 bytes, frame data is reduced by 4 to account for
  // the NumEntries value.
  if ((self->mInputFrameDataSize - 4) < (numEntries * 8)) {
    LOG3(("SpdySession2::HandleSettings %p SETTINGS wrong length data=%d",
          self, self->mInputFrameDataSize));
    return NS_ERROR_ILLEGAL_VALUE;
  }

  LOG3(("SpdySession2::HandleSettings %p SETTINGS Control Frame with %d entries",
        self, numEntries));

  for (uint32_t index = 0; index < numEntries; ++index) {
    // To clarify the v2 spec:
    // Each entry is a 24 bits of a little endian id
    // followed by 8 bits of flags
    // followed by a 32 bit big endian value
    
    unsigned char *setting = reinterpret_cast<unsigned char *>
      (self->mInputFrameBuffer.get()) + 12 + index * 8;

    uint32_t id = (setting[2] << 16) + (setting[1] << 8) + setting[0];
    uint32_t flags = setting[3];
    uint32_t value =  PR_ntohl(reinterpret_cast<uint32_t *>(setting)[1]);

    LOG3(("Settings ID %d, Flags %X, Value %d", id, flags, value));

    switch (id)
    {
    case SETTINGS_TYPE_UPLOAD_BW:
      Telemetry::Accumulate(Telemetry::SPDY_SETTINGS_UL_BW, value);
      break;
      
    case SETTINGS_TYPE_DOWNLOAD_BW:
      Telemetry::Accumulate(Telemetry::SPDY_SETTINGS_DL_BW, value);
      break;
      
    case SETTINGS_TYPE_RTT:
      Telemetry::Accumulate(Telemetry::SPDY_SETTINGS_RTT, value);
      break;
      
    case SETTINGS_TYPE_MAX_CONCURRENT:
      self->mMaxConcurrent = value;
      Telemetry::Accumulate(Telemetry::SPDY_SETTINGS_MAX_STREAMS, value);
      break;
      
    case SETTINGS_TYPE_CWND:
      Telemetry::Accumulate(Telemetry::SPDY_SETTINGS_CWND, value);
      break;
      
    case SETTINGS_TYPE_DOWNLOAD_RETRANS_RATE:
      Telemetry::Accumulate(Telemetry::SPDY_SETTINGS_RETRANS, value);
      break;
      
    case SETTINGS_TYPE_INITIAL_WINDOW:
      Telemetry::Accumulate(Telemetry::SPDY_SETTINGS_IW, value >> 10);
      break;
      
    default:
      break;
    }
    
  }
  
  self->ResetDownstreamState();
  return NS_OK;
}

nsresult
SpdySession2::HandleNoop(SpdySession2 *self)
{
  NS_ABORT_IF_FALSE(self->mFrameControlType == CONTROL_TYPE_NOOP,
                    "wrong control type");

  if (self->mInputFrameDataSize != 0) {
    LOG3(("SpdySession2::HandleNoop %p NOP had data %d",
          self, self->mInputFrameDataSize));
    return NS_ERROR_ILLEGAL_VALUE;
  }

  LOG3(("SpdySession2::HandleNoop %p NOP.", self));

  self->ResetDownstreamState();
  return NS_OK;
}

nsresult
SpdySession2::HandlePing(SpdySession2 *self)
{
  NS_ABORT_IF_FALSE(self->mFrameControlType == CONTROL_TYPE_PING,
                    "wrong control type");

  if (self->mInputFrameDataSize != 4) {
    LOG3(("SpdySession2::HandlePing %p PING had wrong amount of data %d",
          self, self->mInputFrameDataSize));
    return NS_ERROR_ILLEGAL_VALUE;
  }

  uint32_t pingID =
    PR_ntohl(reinterpret_cast<uint32_t *>(self->mInputFrameBuffer.get())[2]);

  LOG3(("SpdySession2::HandlePing %p PING ID 0x%X.", self, pingID));

  if (pingID & 0x01) {
    // presumably a reply to our timeout ping
    self->mPingSentEpoch = 0;
  }
  else {
    // Servers initiate even numbered pings, go ahead and echo it back
    self->GeneratePing(pingID);
  }
    
  self->ResetDownstreamState();
  return NS_OK;
}

nsresult
SpdySession2::HandleGoAway(SpdySession2 *self)
{
  NS_ABORT_IF_FALSE(self->mFrameControlType == CONTROL_TYPE_GOAWAY,
                    "wrong control type");

  if (self->mInputFrameDataSize != 4) {
    LOG3(("SpdySession2::HandleGoAway %p GOAWAY had wrong amount of data %d",
          self, self->mInputFrameDataSize));
    return NS_ERROR_ILLEGAL_VALUE;
  }

  self->mShouldGoAway = true;
  self->mGoAwayID =
    PR_ntohl(reinterpret_cast<uint32_t *>(self->mInputFrameBuffer.get())[2]);
  self->mCleanShutdown = true;

  // Find streams greater than the last-good ID and mark them for deletion 
  // in the mGoAwayStreamsToRestart queue with the GoAwayEnumerator. They can
  // be restarted.
  self->mStreamTransactionHash.Enumerate(GoAwayEnumerator, self);

  // Process the streams marked for deletion and restart.
  uint32_t size = self->mGoAwayStreamsToRestart.GetSize();
  for (uint32_t count = 0; count < size; ++count) {
    SpdyStream2 *stream =
      static_cast<SpdyStream2 *>(self->mGoAwayStreamsToRestart.PopFront());

    self->CloseStream(stream, NS_ERROR_NET_RESET);
    if (stream->HasRegisteredID())
      self->mStreamIDHash.Remove(stream->StreamID());
    self->mStreamTransactionHash.Remove(stream->Transaction());
  }

  // Queued streams can also be deleted from this session and restarted
  // in another one. (they were never sent on the network so they implicitly
  // are not covered by the last-good id.
  size = self->mQueuedStreams.GetSize();
  for (uint32_t count = 0; count < size; ++count) {
    SpdyStream2 *stream =
      static_cast<SpdyStream2 *>(self->mQueuedStreams.PopFront());
    self->CloseStream(stream, NS_ERROR_NET_RESET);
    self->mStreamTransactionHash.Remove(stream->Transaction());
  }

  LOG3(("SpdySession2::HandleGoAway %p GOAWAY Last-Good-ID 0x%X."
        "live streams=%d\n", self, self->mGoAwayID,
        self->mStreamTransactionHash.Count()));
  self->ResumeRecv();
  self->ResetDownstreamState();
  return NS_OK;
}

nsresult
SpdySession2::HandleHeaders(SpdySession2 *self)
{
  NS_ABORT_IF_FALSE(self->mFrameControlType == CONTROL_TYPE_HEADERS,
                    "wrong control type");

  if (self->mInputFrameDataSize < 10) {
    LOG3(("SpdySession2::HandleHeaders %p HEADERS had wrong amount of data %d",
          self, self->mInputFrameDataSize));
    return NS_ERROR_ILLEGAL_VALUE;
  }

  uint32_t streamID =
    PR_ntohl(reinterpret_cast<uint32_t *>(self->mInputFrameBuffer.get())[2]);

  // this is actually not legal in the HTTP mapping of SPDY. All
  // headers are in the syn or syn reply. Log and ignore it.

  // in v3 this will be legal and we must remember to note
  // NS_NET_STATUS_RECEIVING_FROM from it

  LOG3(("SpdySession2::HandleHeaders %p HEADERS for Stream 0x%X. "
        "They are ignored in the HTTP/SPDY mapping.",
        self, streamID));
  self->mLastDataReadEpoch = self->mLastReadEpoch;
  self->ResetDownstreamState();
  return NS_OK;
}

nsresult
SpdySession2::HandleWindowUpdate(SpdySession2 *self)
{
  NS_ABORT_IF_FALSE(self->mFrameControlType == CONTROL_TYPE_WINDOW_UPDATE,
                    "wrong control type");
  LOG3(("SpdySession2::HandleWindowUpdate %p WINDOW UPDATE was "
        "received. WINDOW UPDATE is no longer defined in v2. Ignoring.",
        self));

  self->ResetDownstreamState();
  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsAHttpTransaction. It is expected that nsHttpConnection is the caller
// of these methods
//-----------------------------------------------------------------------------

void
SpdySession2::OnTransportStatus(nsITransport* aTransport,
                               nsresult aStatus,
                               uint64_t aProgress)
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");

  switch (aStatus) {
    // These should appear only once, deliver to the first
    // transaction on the session.
  case NS_NET_STATUS_RESOLVING_HOST:
  case NS_NET_STATUS_RESOLVED_HOST:
  case NS_NET_STATUS_CONNECTING_TO:
  case NS_NET_STATUS_CONNECTED_TO:
  {
    SpdyStream2 *target = mStreamIDHash.Get(1);
    if (target)
      target->Transaction()->OnTransportStatus(aTransport, aStatus, aProgress);
    break;
  }

  default:
    // The other transport events are ignored here because there is no good
    // way to map them to the right transaction in spdy. Instead, the events
    // are generated again from the spdy code and passed directly to the
    // correct transaction.

    // NS_NET_STATUS_SENDING_TO:
    // This is generated by the socket transport when (part) of
    // a transaction is written out
    //
    // There is no good way to map it to the right transaction in spdy,
    // so it is ignored here and generated separately when the SYN_STREAM
    // is sent from SpdyStream2::TransmitFrame

    // NS_NET_STATUS_WAITING_FOR:
    // Created by nsHttpConnection when the request has been totally sent.
    // There is no good way to map it to the right transaction in spdy,
    // so it is ignored here and generated separately when the same
    // condition is complete in SpdyStream2 when there is no more
    // request body left to be transmitted.

    // NS_NET_STATUS_RECEIVING_FROM
    // Generated in spdysession whenever we read a data frame or a syn_reply
    // that can be attributed to a particular stream/transaction

    break;
  }
}

// ReadSegments() is used to write data to the network. Generally, HTTP
// request data is pulled from the approriate transaction and
// converted to SPDY data. Sometimes control data like window-update are
// generated instead.

nsresult
SpdySession2::ReadSegments(nsAHttpSegmentReader *reader,
                          uint32_t count,
                          uint32_t *countRead)
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");
  
  NS_ABORT_IF_FALSE(!mSegmentReader || !reader || (mSegmentReader == reader),
                    "Inconsistent Write Function Callback");

  if (reader)
    mSegmentReader = reader;

  nsresult rv;
  *countRead = 0;

  // First priority goes to frames that were writing to the network but were
  // blocked part way through. Then to frames that have no streams (e.g ping
  // reply) and then third to streams marked urgent (generally they have
  // window updates), and finally to streams generally
  // ready to send data frames (http requests).

  LOG3(("SpdySession2::ReadSegments %p", this));

  SpdyStream2 *stream;
  
  stream = static_cast<SpdyStream2 *>(mUrgentForWrite.PopFront());
  if (!stream)
    stream = static_cast<SpdyStream2 *>(mReadyForWrite.PopFront());
  if (!stream) {
    LOG3(("SpdySession2 %p could not identify a stream to write; suspending.",
          this));
    FlushOutputQueue();
    SetWriteCallbacks();
    return NS_BASE_STREAM_WOULD_BLOCK;
  }
  
  LOG3(("SpdySession2 %p will write from SpdyStream2 %p", this, stream));

  rv = stream->ReadSegments(this, count, countRead);

  // Not every permutation of stream->ReadSegents produces data (and therefore
  // tries to flush the output queue) - SENDING_FIN_STREAM can be an example
  // of that. But we might still have old data buffered that would be good
  // to flush.
  FlushOutputQueue();

  if (stream->RequestBlockedOnRead()) {
    
    // We are blocked waiting for input - either more http headers or
    // any request body data. When more data from the request stream
    // becomes available the httptransaction will call conn->ResumeSend().
    
    LOG3(("SpdySession2::ReadSegments %p dealing with block on read", this));

    // call readsegments again if there are other streams ready
    // to run in this session
    if (GetWriteQueueSize())
      rv = NS_OK;
    else
      rv = NS_BASE_STREAM_WOULD_BLOCK;
    SetWriteCallbacks();
    return rv;
  }
  
  if (NS_FAILED(rv)) {
    LOG3(("SpdySession2::ReadSegments %p returning FAIL code %X",
          this, rv));
    if (rv != NS_BASE_STREAM_WOULD_BLOCK)
      CleanupStream(stream, rv, RST_CANCEL);
    return rv;
  }
  
  if (*countRead > 0) {
    LOG3(("SpdySession2::ReadSegments %p stream=%p generated end of frame %d",
          this, stream, *countRead));
    mReadyForWrite.Push(stream);
    SetWriteCallbacks();
    return rv;
  }
  
  LOG3(("SpdySession2::ReadSegments %p stream=%p stream send complete",
        this, stream));
  
  /* we now want to recv data */
  ResumeRecv();

  // call readsegments again if there are other streams ready
  // to go in this session
  SetWriteCallbacks();

  return rv;
}

// WriteSegments() is used to read data off the socket. Generally this is
// just the SPDY frame header and from there the appropriate SPDYStream
// is identified from the Stream-ID. The http transaction associated with
// that read then pulls in the data directly, which it will feed to
// OnWriteSegment(). That function will gateway it into http and feed
// it to the appropriate transaction.

// we call writer->OnWriteSegment via NetworkRead() to get a spdy header.. 
// and decide if it is data or control.. if it is control, just deal with it.
// if it is data, identify the spdy stream
// call stream->WriteSegemnts which can call this::OnWriteSegment to get the
// data. It always gets full frames if they are part of the stream

nsresult
SpdySession2::WriteSegments(nsAHttpSegmentWriter *writer,
                           uint32_t count,
                           uint32_t *countWritten)
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");
  
  nsresult rv;
  *countWritten = 0;

  if (mClosed)
    return NS_ERROR_FAILURE;

  SetWriteCallbacks();
  
  // We buffer all control frames and act on them in this layer.
  // We buffer the first 8 bytes of data frames (the header) but
  // the actual data is passed through unprocessed.
  
  if (mDownstreamState == BUFFERING_FRAME_HEADER) {
    // The first 8 bytes of every frame is header information that
    // we are going to want to strip before passing to http. That is
    // true of both control and data packets.
    
    NS_ABORT_IF_FALSE(mInputFrameBufferUsed < 8,
                      "Frame Buffer Used Too Large for State");

    rv = NetworkRead(writer, mInputFrameBuffer + mInputFrameBufferUsed,
                     8 - mInputFrameBufferUsed, countWritten);

    if (NS_FAILED(rv)) {
      LOG3(("SpdySession2 %p buffering frame header read failure %x\n",
            this, rv));
      // maybe just blocked reading from network
      if (rv == NS_BASE_STREAM_WOULD_BLOCK)
        ResumeRecv();
      return rv;
    }

    LogIO(this, nullptr, "Reading Frame Header",
          mInputFrameBuffer + mInputFrameBufferUsed, *countWritten);

    mInputFrameBufferUsed += *countWritten;

    if (mInputFrameBufferUsed < 8)
    {
      LOG3(("SpdySession2::WriteSegments %p "
            "BUFFERING FRAME HEADER incomplete size=%d",
            this, mInputFrameBufferUsed));
      return rv;
    }

    // For both control and data frames the second 32 bit word of the header
    // is 8-flags, 24-length. (network byte order)
    mInputFrameDataSize =
      PR_ntohl(reinterpret_cast<uint32_t *>(mInputFrameBuffer.get())[1]);
    mInputFrameDataSize &= 0x00ffffff;
    mInputFrameDataRead = 0;
    
    if (mInputFrameBuffer[0] & kFlag_Control) {
      EnsureBuffer(mInputFrameBuffer, mInputFrameDataSize + 8, 8,
                   mInputFrameBufferSize);
      ChangeDownstreamState(BUFFERING_CONTROL_FRAME);
      
      // The first 32 bit word of the header is
      // 1 ctrl - 15 version - 16 type
      uint16_t version =
        PR_ntohs(reinterpret_cast<uint16_t *>(mInputFrameBuffer.get())[0]);
      version &= 0x7fff;
      
      mFrameControlType =
        PR_ntohs(reinterpret_cast<uint16_t *>(mInputFrameBuffer.get())[1]);
      
      LOG3(("SpdySession2::WriteSegments %p - Control Frame Identified "
            "type %d version %d data len %d",
            this, mFrameControlType, version, mInputFrameDataSize));

      if (mFrameControlType >= CONTROL_TYPE_LAST ||
          mFrameControlType <= CONTROL_TYPE_FIRST)
        return NS_ERROR_ILLEGAL_VALUE;

      // The protocol document says this value must be 1 even though this
      // is known as version 2.. Testing interop indicates that is a typo
      // in the protocol document
      if (version != 2) {
        return NS_ERROR_ILLEGAL_VALUE;
      }
    }
    else {
      ChangeDownstreamState(PROCESSING_DATA_FRAME);

      uint32_t streamID =
        PR_ntohl(reinterpret_cast<uint32_t *>(mInputFrameBuffer.get())[0]);
      rv = SetInputFrameDataStream(streamID);
      if (NS_FAILED(rv)) {
        LOG(("SpdySession2::WriteSegments %p lookup streamID 0x%X failed. "
              "probably due to verification.\n", this, streamID));
        return rv;
      }
      if (!mInputFrameDataStream) {
        LOG3(("SpdySession2::WriteSegments %p lookup streamID 0x%X failed. "
              "Next = 0x%x", this, streamID, mNextStreamID));
        if (streamID >= mNextStreamID)
          GenerateRstStream(RST_INVALID_STREAM, streamID);
        ChangeDownstreamState(DISCARDING_DATA_FRAME);
      }
      mInputFrameDataLast = (mInputFrameBuffer[4] & kFlag_Data_FIN);
      Telemetry::Accumulate(Telemetry::SPDY_CHUNK_RECVD,
                            mInputFrameDataSize >> 10);
      LOG3(("Start Processing Data Frame. "
            "Session=%p Stream ID 0x%x Stream Ptr %p Fin=%d Len=%d",
            this, streamID, mInputFrameDataStream, mInputFrameDataLast,
            mInputFrameDataSize));
      mLastDataReadEpoch = mLastReadEpoch;

      if (mInputFrameBuffer[4] & kFlag_Data_ZLIB) {
        LOG3(("Data flag has ZLIB flag set which is not valid >=2 spdy"));
        return NS_ERROR_ILLEGAL_VALUE;
      }
    }
  }

  if (mDownstreamState == PROCESSING_CONTROL_RST_STREAM) {
    if (mDownstreamRstReason == RST_REFUSED_STREAM)
      rv = NS_ERROR_NET_RESET;            //we can retry this 100% safely
    else if (mDownstreamRstReason == RST_CANCEL ||
             mDownstreamRstReason == RST_PROTOCOL_ERROR ||
             mDownstreamRstReason == RST_INTERNAL_ERROR ||
             mDownstreamRstReason == RST_UNSUPPORTED_VERSION)
      rv = NS_ERROR_NET_INTERRUPT;
    else
      rv = NS_ERROR_ILLEGAL_VALUE;

    if (mDownstreamRstReason != RST_REFUSED_STREAM &&
        mDownstreamRstReason != RST_CANCEL)
      mShouldGoAway = true;

    // mInputFrameDataStream is reset by ChangeDownstreamState
    SpdyStream2 *stream = mInputFrameDataStream;
    ResetDownstreamState();
    LOG3(("SpdySession2::WriteSegments cleanup stream on recv of rst "
          "session=%p stream=%p 0x%X\n", this, stream,
          stream ? stream->StreamID() : 0));
    CleanupStream(stream, rv, RST_CANCEL);
    return NS_OK;
  }

  if (mDownstreamState == PROCESSING_DATA_FRAME ||
      mDownstreamState == PROCESSING_CONTROL_SYN_REPLY) {

    // The cleanup stream should only be set while stream->WriteSegments is
    // on the stack and then cleaned up in this code block afterwards.
    NS_ABORT_IF_FALSE(!mNeedsCleanup, "cleanup stream set unexpectedly");
    mNeedsCleanup = nullptr;                     /* just in case */

    mSegmentWriter = writer;
    rv = mInputFrameDataStream->WriteSegments(this, count, countWritten);
    mSegmentWriter = nullptr;

    mLastDataReadEpoch = mLastReadEpoch;

    if (rv == NS_BASE_STREAM_CLOSED) {
      // This will happen when the transaction figures out it is EOF, generally
      // due to a content-length match being made
      SpdyStream2 *stream = mInputFrameDataStream;
      if (mInputFrameDataRead == mInputFrameDataSize)
        ResetDownstreamState();
      LOG3(("SpdySession2::WriteSegments session=%p stream=%p 0x%X "
            "needscleanup=%p. cleanup stream based on "
            "stream->writeSegments returning BASE_STREAM_CLOSED\n",
            this, stream, stream ? stream->StreamID() : 0,
            mNeedsCleanup));
      CleanupStream(stream, NS_OK, RST_CANCEL);
      NS_ABORT_IF_FALSE(!mNeedsCleanup, "double cleanup out of data frame");
      mNeedsCleanup = nullptr;                     /* just in case */
      return NS_OK;
    }
    
    if (mNeedsCleanup) {
      LOG3(("SpdySession2::WriteSegments session=%p stream=%p 0x%X "
            "cleanup stream based on mNeedsCleanup.\n",
            this, mNeedsCleanup, mNeedsCleanup ? mNeedsCleanup->StreamID() : 0));
      CleanupStream(mNeedsCleanup, NS_OK, RST_CANCEL);
      mNeedsCleanup = nullptr;
    }

    // In v3 this is where we would generate a window update

    return rv;
  }

  if (mDownstreamState == DISCARDING_DATA_FRAME) {
    char trash[4096];
    uint32_t count = std::min(4096U, mInputFrameDataSize - mInputFrameDataRead);

    if (!count) {
      ResetDownstreamState();
      ResumeRecv();
      return NS_BASE_STREAM_WOULD_BLOCK;
    }

    rv = NetworkRead(writer, trash, count, countWritten);

    if (NS_FAILED(rv)) {
      LOG3(("SpdySession2 %p discard frame read failure %x\n", this, rv));
      // maybe just blocked reading from network
      if (rv == NS_BASE_STREAM_WOULD_BLOCK)
        ResumeRecv();
      return rv;
    }

    LogIO(this, nullptr, "Discarding Frame", trash, *countWritten);

    mInputFrameDataRead += *countWritten;

    if (mInputFrameDataRead == mInputFrameDataSize)
      ResetDownstreamState();
    return rv;
  }
  
  if (mDownstreamState != BUFFERING_CONTROL_FRAME) {
    // this cannot happen
    NS_ABORT_IF_FALSE(false, "Not in Bufering Control Frame State");
    return NS_ERROR_UNEXPECTED;
  }

  NS_ABORT_IF_FALSE(mInputFrameBufferUsed == 8,
                    "Frame Buffer Header Not Present");

  rv = NetworkRead(writer, mInputFrameBuffer + 8 + mInputFrameDataRead,
                   mInputFrameDataSize - mInputFrameDataRead, countWritten);

  if (NS_FAILED(rv)) {
    LOG3(("SpdySession2 %p buffering control frame read failure %x\n",
          this, rv));
    // maybe just blocked reading from network
    if (rv == NS_BASE_STREAM_WOULD_BLOCK)
      ResumeRecv();
    return rv;
  }

  LogIO(this, nullptr, "Reading Control Frame",
        mInputFrameBuffer + 8 + mInputFrameDataRead, *countWritten);

  mInputFrameDataRead += *countWritten;

  if (mInputFrameDataRead != mInputFrameDataSize)
    return NS_OK;

  // This check is actually redundant, the control type was previously
  // checked to make sure it was in range, but we will check it again
  // at time of use to make sure a regression doesn't creep in.
  if (mFrameControlType >= CONTROL_TYPE_LAST ||
      mFrameControlType <= CONTROL_TYPE_FIRST) 
  {
    NS_ABORT_IF_FALSE(false, "control type out of range");
    return NS_ERROR_ILLEGAL_VALUE;
  }
  rv = sControlFunctions[mFrameControlType](this);

  NS_ABORT_IF_FALSE(NS_FAILED(rv) ||
                    mDownstreamState != BUFFERING_CONTROL_FRAME,
                    "Control Handler returned OK but did not change state");

  if (mShouldGoAway && !mStreamTransactionHash.Count())
    Close(NS_OK);
  return rv;
}

void
SpdySession2::Close(nsresult aReason)
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");

  if (mClosed)
    return;

  LOG3(("SpdySession2::Close %p %X", this, aReason));

  mClosed = true;

  mStreamTransactionHash.Enumerate(ShutdownEnumerator, this);
  mStreamIDHash.Clear();
  mStreamTransactionHash.Clear();

  if (NS_SUCCEEDED(aReason))
    GenerateGoAway();
  mConnection = nullptr;
  mSegmentReader = nullptr;
  mSegmentWriter = nullptr;
}

void
SpdySession2::CloseTransaction(nsAHttpTransaction *aTransaction,
                              nsresult aResult)
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");
  LOG3(("SpdySession2::CloseTransaction %p %p %x", this, aTransaction, aResult));

  // Generally this arrives as a cancel event from the connection manager.

  // need to find the stream and call CleanupStream() on it.
  SpdyStream2 *stream = mStreamTransactionHash.Get(aTransaction);
  if (!stream) {
    LOG3(("SpdySession2::CloseTransaction %p %p %x - not found.",
          this, aTransaction, aResult));
    return;
  }
  LOG3(("SpdySession2::CloseTranscation probably a cancel. "
        "this=%p, trans=%p, result=%x, streamID=0x%X stream=%p",
        this, aTransaction, aResult, stream->StreamID(), stream));
  CleanupStream(stream, aResult, RST_CANCEL);
  ResumeRecv();
}


//-----------------------------------------------------------------------------
// nsAHttpSegmentReader
//-----------------------------------------------------------------------------

nsresult
SpdySession2::OnReadSegment(const char *buf,
                           uint32_t count,
                           uint32_t *countRead)
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");
  
  nsresult rv;
  
  // If we can release old queued data then we can try and write the new
  // data directly to the network without using the output queue at all
  if (mOutputQueueUsed)
    FlushOutputQueue();

  if (!mOutputQueueUsed && mSegmentReader) {
    // try and write directly without output queue
    rv = mSegmentReader->OnReadSegment(buf, count, countRead);

    if (rv == NS_BASE_STREAM_WOULD_BLOCK)
      *countRead = 0;
    else if (NS_FAILED(rv))
      return rv;
    
    if (*countRead < count) {
      uint32_t required = count - *countRead;
      // assuming a commitment() happened, this ensurebuffer is a nop
      // but just in case the queuesize is too small for the required data
      // call ensurebuffer().
      EnsureBuffer(mOutputQueueBuffer, required, 0, mOutputQueueSize);
      memcpy(mOutputQueueBuffer.get(), buf + *countRead, required);
      mOutputQueueUsed = required;
    }
    
    *countRead = count;
    return NS_OK;
  }

  // At this point we are going to buffer the new data in the output
  // queue if it fits. By coalescing multiple small submissions into one larger
  // buffer we can get larger writes out to the network later on.

  // This routine should not be allowed to fill up the output queue
  // all on its own - at least kQueueReserved bytes are always left
  // for other routines to use - but this is an all-or-nothing function,
  // so if it will not all fit just return WOULD_BLOCK

  if ((mOutputQueueUsed + count) > (mOutputQueueSize - kQueueReserved))
    return NS_BASE_STREAM_WOULD_BLOCK;
  
  memcpy(mOutputQueueBuffer.get() + mOutputQueueUsed, buf, count);
  mOutputQueueUsed += count;
  *countRead = count;

  FlushOutputQueue();

  return NS_OK;
}

nsresult
SpdySession2::CommitToSegmentSize(uint32_t count, bool forceCommitment)
{
  if (mOutputQueueUsed)
    FlushOutputQueue();

  // would there be enough room to buffer this if needed?
  if ((mOutputQueueUsed + count) <= (mOutputQueueSize - kQueueReserved))
    return NS_OK;

  // if we are using part of our buffers already, try again later unless
  // forceCommitment is set.
  if (mOutputQueueUsed && !forceCommitment)
    return NS_BASE_STREAM_WOULD_BLOCK;

  if (mOutputQueueUsed) {
    // normally we avoid the memmove of RealignOutputQueue, but we'll try
    // it if forceCommitment is set before growing the buffer.
    RealignOutputQueue();

    // is there enough room now?
    if ((mOutputQueueUsed + count) <= (mOutputQueueSize - kQueueReserved))
      return NS_OK;
  }

  // resize the buffers as needed
  EnsureBuffer(mOutputQueueBuffer, mOutputQueueUsed + count + kQueueReserved,
               mOutputQueueUsed, mOutputQueueSize);

  NS_ABORT_IF_FALSE((mOutputQueueUsed + count) <=
                    (mOutputQueueSize - kQueueReserved),
                    "buffer not as large as expected");

  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsAHttpSegmentWriter
//-----------------------------------------------------------------------------

nsresult
SpdySession2::OnWriteSegment(char *buf,
                            uint32_t count,
                            uint32_t *countWritten)
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");
  nsresult rv;

  if (!mSegmentWriter) {
    // the only way this could happen would be if Close() were called on the
    // stack with WriteSegments()
    return NS_ERROR_FAILURE;
  }
  
  if (mDownstreamState == PROCESSING_DATA_FRAME) {

    if (mInputFrameDataLast &&
        mInputFrameDataRead == mInputFrameDataSize) {
      *countWritten = 0;
      SetNeedsCleanup();
      return NS_BASE_STREAM_CLOSED;
    }
    
    count = std::min(count, mInputFrameDataSize - mInputFrameDataRead);
    rv = NetworkRead(mSegmentWriter, buf, count, countWritten);
    if (NS_FAILED(rv))
      return rv;

    LogIO(this, mInputFrameDataStream, "Reading Data Frame",
          buf, *countWritten);

    mInputFrameDataRead += *countWritten;
    
    mInputFrameDataStream->UpdateTransportReadEvents(*countWritten);
    if ((mInputFrameDataRead == mInputFrameDataSize) && !mInputFrameDataLast)
      ResetDownstreamState();

    return rv;
  }
  
  if (mDownstreamState == PROCESSING_CONTROL_SYN_REPLY) {
    
    if (mFlatHTTPResponseHeaders.Length() == mFlatHTTPResponseHeadersOut &&
        mInputFrameDataLast) {
      *countWritten = 0;
      SetNeedsCleanup();
      return NS_BASE_STREAM_CLOSED;
    }
      
    count = std::min(count,
                   mFlatHTTPResponseHeaders.Length() -
                   mFlatHTTPResponseHeadersOut);
    memcpy(buf,
           mFlatHTTPResponseHeaders.get() + mFlatHTTPResponseHeadersOut,
           count);
    mFlatHTTPResponseHeadersOut += count;
    *countWritten = count;

    if (mFlatHTTPResponseHeaders.Length() == mFlatHTTPResponseHeadersOut &&
        !mInputFrameDataLast)
      ResetDownstreamState();
    return NS_OK;
  }

  return NS_ERROR_UNEXPECTED;
}

void
SpdySession2::SetNeedsCleanup()
{
  LOG3(("SpdySession2::SetNeedsCleanup %p - recorded downstream fin of "
        "stream %p 0x%X", this, mInputFrameDataStream,
        mInputFrameDataStream->StreamID()));

  // This will result in Close() being called
  NS_ABORT_IF_FALSE(!mNeedsCleanup, "mNeedsCleanup unexpectedly set");
  mNeedsCleanup = mInputFrameDataStream;
  ResetDownstreamState();
}

//-----------------------------------------------------------------------------
// Modified methods of nsAHttpConnection
//-----------------------------------------------------------------------------

void
SpdySession2::TransactionHasDataToWrite(nsAHttpTransaction *caller)
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");
  LOG3(("SpdySession2::TransactionHasDataToWrite %p trans=%p", this, caller));

  // a trapped signal from the http transaction to the connection that
  // it is no longer blocked on read.

  SpdyStream2 *stream = mStreamTransactionHash.Get(caller);
  if (!stream || !VerifyStream(stream)) {
    LOG3(("SpdySession2::TransactionHasDataToWrite %p caller %p not found",
          this, caller));
    return;
  }
  
  LOG3(("SpdySession2::TransactionHasDataToWrite %p ID is %x",
        this, stream->StreamID()));

  mReadyForWrite.Push(stream);
}

void
SpdySession2::TransactionHasDataToWrite(SpdyStream2 *stream)
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");
  LOG3(("SpdySession2::TransactionHasDataToWrite %p stream=%p ID=%x",
        this, stream, stream->StreamID()));

  mReadyForWrite.Push(stream);
  SetWriteCallbacks();
}

bool
SpdySession2::IsPersistent()
{
  return true;
}

nsresult
SpdySession2::TakeTransport(nsISocketTransport **,
                           nsIAsyncInputStream **,
                           nsIAsyncOutputStream **)
{
  NS_ABORT_IF_FALSE(false, "TakeTransport of SpdySession2");
  return NS_ERROR_UNEXPECTED;
}

nsHttpConnection *
SpdySession2::TakeHttpConnection()
{
  NS_ABORT_IF_FALSE(false, "TakeHttpConnection of SpdySession2");
  return nullptr;
}

uint32_t
SpdySession2::CancelPipeline(nsresult reason)
{
  // we don't pipeline inside spdy, so this isn't an issue
  return 0;
}

nsAHttpTransaction::Classifier
SpdySession2::Classification()
{
  if (!mConnection)
    return nsAHttpTransaction::CLASS_GENERAL;
  return mConnection->Classification();
}

//-----------------------------------------------------------------------------
// unused methods of nsAHttpTransaction
// We can be sure of this because SpdySession2 is only constructed in
// nsHttpConnection and is never passed out of that object
//-----------------------------------------------------------------------------

void
SpdySession2::SetConnection(nsAHttpConnection *)
{
  // This is unexpected
  NS_ABORT_IF_FALSE(false, "SpdySession2::SetConnection()");
}

void
SpdySession2::GetSecurityCallbacks(nsIInterfaceRequestor **)
{
  // This is unexpected
  NS_ABORT_IF_FALSE(false, "SpdySession2::GetSecurityCallbacks()");
}

void
SpdySession2::SetProxyConnectFailed()
{
  NS_ABORT_IF_FALSE(false, "SpdySession2::SetProxyConnectFailed()");
}

bool
SpdySession2::IsDone()
{
  return !mStreamTransactionHash.Count();
}

nsresult
SpdySession2::Status()
{
  NS_ABORT_IF_FALSE(false, "SpdySession2::Status()");
  return NS_ERROR_UNEXPECTED;
}

uint32_t
SpdySession2::Caps()
{
  NS_ABORT_IF_FALSE(false, "SpdySession2::Caps()");
  return 0;
}

uint64_t
SpdySession2::Available()
{
  NS_ABORT_IF_FALSE(false, "SpdySession2::Available()");
  return 0;
}

nsHttpRequestHead *
SpdySession2::RequestHead()
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");
  NS_ABORT_IF_FALSE(false,
                    "SpdySession2::RequestHead() "
                    "should not be called after SPDY is setup");
  return NULL;
}

uint32_t
SpdySession2::Http1xTransactionCount()
{
  return 0;
}

// used as an enumerator by TakeSubTransactions()
static PLDHashOperator
TakeStream(nsAHttpTransaction *key,
           nsAutoPtr<SpdyStream2> &stream,
           void *closure)
{
  nsTArray<nsRefPtr<nsAHttpTransaction> > *list =
    static_cast<nsTArray<nsRefPtr<nsAHttpTransaction> > *>(closure);

  list->AppendElement(key);

  // removing the stream from the hash will delete the stream
  // and drop the transaction reference the hash held
  return PL_DHASH_REMOVE;
}

nsresult
SpdySession2::TakeSubTransactions(
    nsTArray<nsRefPtr<nsAHttpTransaction> > &outTransactions)
{
  // Generally this cannot be done with spdy as transactions are
  // started right away.

  LOG3(("SpdySession2::TakeSubTransactions %p\n", this));

  if (mConcurrentHighWater > 0)
    return NS_ERROR_ALREADY_OPENED;

  LOG3(("   taking %d\n", mStreamTransactionHash.Count()));

  mStreamTransactionHash.Enumerate(TakeStream, &outTransactions);
  return NS_OK;
}

nsresult
SpdySession2::AddTransaction(nsAHttpTransaction *)
{
  // This API is meant for pipelining, SpdySession2's should be
  // extended with AddStream()

  NS_ABORT_IF_FALSE(false,
                    "SpdySession2::AddTransaction() should not be called");

  return NS_ERROR_NOT_IMPLEMENTED;
}

uint32_t
SpdySession2::PipelineDepth()
{
  return IsDone() ? 0 : 1;
}

nsresult
SpdySession2::SetPipelinePosition(int32_t position)
{
  // This API is meant for pipelining, SpdySession2's should be
  // extended with AddStream()

  NS_ABORT_IF_FALSE(false,
                    "SpdySession2::SetPipelinePosition() should not be called");

  return NS_ERROR_NOT_IMPLEMENTED;
}

int32_t
SpdySession2::PipelinePosition()
{
    return 0;
}

//-----------------------------------------------------------------------------
// Pass through methods of nsAHttpConnection
//-----------------------------------------------------------------------------

nsAHttpConnection *
SpdySession2::Connection()
{
  NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");
  return mConnection;
}

nsresult
SpdySession2::OnHeadersAvailable(nsAHttpTransaction *transaction,
                                nsHttpRequestHead *requestHead,
                                nsHttpResponseHead *responseHead,
                                bool *reset)
{
  return mConnection->OnHeadersAvailable(transaction,
                                         requestHead,
                                         responseHead,
                                         reset);
}

bool
SpdySession2::IsReused()
{
  return mConnection->IsReused();
}

nsresult
SpdySession2::PushBack(const char *buf, uint32_t len)
{
  return mConnection->PushBack(buf, len);
}

} // namespace mozilla::net
} // namespace mozilla

