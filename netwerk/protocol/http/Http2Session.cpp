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

#include "Http2Session.h"
#include "Http2Stream.h"
#include "Http2Push.h"

#include "mozilla/Telemetry.h"
#include "mozilla/Preferences.h"
#include "nsHttp.h"
#include "nsHttpHandler.h"
#include "nsHttpConnection.h"
#include "nsILoadGroup.h"
#include "nsISSLSocketControl.h"
#include "prprf.h"
#include "prnetdb.h"

#ifdef DEBUG
// defined by the socket transport service while active
extern PRThread *gSocketThread;
#endif

namespace mozilla {
namespace net {

// Http2Session has multiple inheritance of things that implement
// nsISupports, so this magic is taken from nsHttpPipeline that
// implements some of the same abstract classes.
NS_IMPL_ADDREF(Http2Session)
NS_IMPL_RELEASE(Http2Session)
NS_INTERFACE_MAP_BEGIN(Http2Session)
NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsAHttpConnection)
NS_INTERFACE_MAP_END

// "magic" refers to the string that preceeds HTTP/2 on the wire
// to help find any intermediaries speaking an older version of HTTP
const uint8_t Http2Session::kMagicHello[] = {
  0x50, 0x52, 0x49, 0x20, 0x2a, 0x20, 0x48, 0x54,
  0x54, 0x50, 0x2f, 0x32, 0x2e, 0x30, 0x0d, 0x0a,
  0x0d, 0x0a, 0x53, 0x4d, 0x0d, 0x0a, 0x0d, 0x0a
};

#define RETURN_SESSION_ERROR(o,x)  \
do {                             \
  (o)->mGoAwayReason = (x);      \
  return NS_ERROR_ILLEGAL_VALUE; \
  } while (0)

Http2Session::Http2Session(nsAHttpTransaction *aHttpTransaction,
                           nsISocketTransport *aSocketTransport,
                           int32_t firstPriority)
  : mSocketTransport(aSocketTransport),
  mSegmentReader(nullptr),
  mSegmentWriter(nullptr),
  mNextStreamID(3), // 1 is reserved for Updgrade handshakes
  mConcurrentHighWater(0),
  mDownstreamState(BUFFERING_OPENING_SETTINGS),
  mInputFrameBufferSize(kDefaultBufferSize),
  mInputFrameBufferUsed(0),
  mInputFrameFinal(false),
  mInputFrameDataStream(nullptr),
  mNeedsCleanup(nullptr),
  mDownstreamRstReason(NO_HTTP_ERROR),
  mExpectedHeaderID(0),
  mExpectedPushPromiseID(0),
  mContinuedPromiseStream(0),
  mShouldGoAway(false),
  mClosed(false),
  mCleanShutdown(false),
  mServerUsesFlowControl(true),
  mTLSProfileConfirmed(false),
  mGoAwayReason(NO_HTTP_ERROR),
  mGoAwayID(0),
  mOutgoingGoAwayID(0),
  mMaxConcurrent(kDefaultMaxConcurrent),
  mConcurrent(0),
  mServerPushedResources(0),
  mServerInitialStreamWindow(kDefaultRwin),
  mLocalSessionWindow(kDefaultRwin),
  mServerSessionWindow(kDefaultRwin),
  mOutputQueueSize(kDefaultQueueSize),
  mOutputQueueUsed(0),
  mOutputQueueSent(0),
  mLastReadEpoch(PR_IntervalNow()),
  mPingSentEpoch(0)
{
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

    static uint64_t sSerial;
    mSerial = ++sSerial;

    LOG3(("Http2Session::Http2Session %p transaction 1 = %p serial=0x%X\n",
          this, aHttpTransaction, mSerial));

    mConnection = aHttpTransaction->Connection();
    mInputFrameBuffer = new char[mInputFrameBufferSize];
    mOutputQueueBuffer = new char[mOutputQueueSize];
    mDecompressBuffer.SetCapacity(kDefaultBufferSize);

    mPushAllowance = gHttpHandler->SpdyPushAllowance();

    mSendingChunkSize = gHttpHandler->SpdySendingChunkSize();
    SendHello();

    if (!aHttpTransaction->IsNullTransaction())
      AddStream(aHttpTransaction, firstPriority);
    mLastDataReadEpoch = mLastReadEpoch;

    mPingThreshold = gHttpHandler->SpdyPingThreshold();
}

PLDHashOperator
Http2Session::ShutdownEnumerator(nsAHttpTransaction *key,
                                 nsAutoPtr<Http2Stream> &stream,
                                 void *closure)
{
  Http2Session *self = static_cast<Http2Session *>(closure);

  // On a clean server hangup the server sets the GoAwayID to be the ID of
  // the last transaction it processed. If the ID of stream in the
  // local stream is greater than that it can safely be restarted because the
  // server guarantees it was not partially processed. Streams that have not
  // registered an ID haven't actually been sent yet so they can always be
  // restarted.
  if (self->mCleanShutdown &&
      (stream->StreamID() > self->mGoAwayID || !stream->HasRegisteredID())) {
    self->CloseStream(stream, NS_ERROR_NET_RESET); // can be restarted
  } else {
    self->CloseStream(stream, NS_ERROR_ABORT);
  }

  return PL_DHASH_NEXT;
}

PLDHashOperator
Http2Session::GoAwayEnumerator(nsAHttpTransaction *key,
                               nsAutoPtr<Http2Stream> &stream,
                               void *closure)
{
  Http2Session *self = static_cast<Http2Session *>(closure);

  // these streams were not processed by the server and can be restarted.
  // Do that after the enumerator completes to avoid the risk of
  // a restart event re-entrantly modifying this hash. Be sure not to restart
  // a pushed (even numbered) stream
  if ((stream->StreamID() > self->mGoAwayID && (stream->StreamID() & 1)) ||
      !stream->HasRegisteredID()) {
    self->mGoAwayStreamsToRestart.Push(stream);
  }

  return PL_DHASH_NEXT;
}

Http2Session::~Http2Session()
{
  LOG3(("Http2Session::~Http2Session %p mDownstreamState=%X",
        this, mDownstreamState));

  mStreamTransactionHash.Enumerate(ShutdownEnumerator, this);
  Telemetry::Accumulate(Telemetry::SPDY_PARALLEL_STREAMS, mConcurrentHighWater);
  Telemetry::Accumulate(Telemetry::SPDY_REQUEST_PER_CONN, (mNextStreamID - 1) / 2);
  Telemetry::Accumulate(Telemetry::SPDY_SERVER_INITIATED_STREAMS,
                        mServerPushedResources);
}

void
Http2Session::LogIO(Http2Session *self, Http2Stream *stream,
                    const char *label,
                    const char *data, uint32_t datalen)
{
  if (!LOG4_ENABLED())
    return;

  LOG4(("Http2Session::LogIO %p stream=%p id=0x%X [%s]",
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
                (reinterpret_cast<const uint8_t *>(data))[index]);
    line += 3;
  }
  if (index) {
    *line = 0;
    LOG4(("%s", linebuf));
  }
}

typedef nsresult (*Http2ControlFx) (Http2Session *self);
static Http2ControlFx sControlFunctions[] = {
  nullptr, // type 0 data is not a control function
  Http2Session::RecvHeaders,
  Http2Session::RecvPriority,
  Http2Session::RecvRstStream,
  Http2Session::RecvSettings,
  Http2Session::RecvPushPromise,
  Http2Session::RecvPing,
  Http2Session::RecvGoAway,
  Http2Session::RecvUnused1,
  Http2Session::RecvWindowUpdate,
  Http2Session::RecvContinuation
};

bool
Http2Session::RoomForMoreConcurrent()
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  return (mConcurrent < mMaxConcurrent);
}

bool
Http2Session::RoomForMoreStreams()
{
  if (mNextStreamID + mStreamTransactionHash.Count() * 2 > kMaxStreamID)
    return false;

  return !mShouldGoAway;
}

PRIntervalTime
Http2Session::IdleTime()
{
  return PR_IntervalNow() - mLastDataReadEpoch;
}

uint32_t
Http2Session::ReadTimeoutTick(PRIntervalTime now)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  LOG3(("Http2Session::ReadTimeoutTick %p delta since last read %ds\n",
       this, PR_IntervalToSeconds(now - mLastReadEpoch)));

  if (!mPingThreshold)
    return UINT32_MAX;

  if ((now - mLastReadEpoch) < mPingThreshold) {
    // recent activity means ping is not an issue
    if (mPingSentEpoch)
      mPingSentEpoch = 0;

    return PR_IntervalToSeconds(mPingThreshold) -
      PR_IntervalToSeconds(now - mLastReadEpoch);
  }

  if (mPingSentEpoch) {
    LOG3(("Http2Session::ReadTimeoutTick %p handle outstanding ping\n"));
    if ((now - mPingSentEpoch) >= gHttpHandler->SpdyPingTimeout()) {
      LOG3(("Http2Session::ReadTimeoutTick %p Ping Timer Exhaustion\n", this));
      mPingSentEpoch = 0;
      Close(NS_ERROR_NET_TIMEOUT);
      return UINT32_MAX;
    }
    return 1; // run the tick aggressively while ping is outstanding
  }

  LOG3(("Http2Session::ReadTimeoutTick %p generating ping\n", this));

  mPingSentEpoch = PR_IntervalNow();
  if (!mPingSentEpoch)
    mPingSentEpoch = 1; // avoid the 0 sentinel value
  GeneratePing(false);
  ResumeRecv(); // read the ping reply

  // Check for orphaned push streams. This looks expensive, but generally the
  // list is empty.
  Http2PushedStream *deleteMe;
  TimeStamp timestampNow;
  do {
    deleteMe = nullptr;

    for (uint32_t index = mPushedStreams.Length();
         index > 0 ; --index) {
      Http2PushedStream *pushedStream = mPushedStreams[index - 1];

      if (timestampNow.IsNull())
        timestampNow = TimeStamp::Now(); // lazy initializer

      // if stream finished, but is not connected, and its been like that for
      // long then cleanup the stream.
      if (pushedStream->IsOrphaned(timestampNow))
      {
        LOG3(("Http2Session Timeout Pushed Stream %p 0x%X\n",
              this, pushedStream->StreamID()));
        deleteMe = pushedStream;
        break; // don't CleanupStream() while iterating this vector
      }
    }
    if (deleteMe)
      CleanupStream(deleteMe, NS_ERROR_ABORT, CANCEL_ERROR);

  } while (deleteMe);

  return 1; // run the tick aggressively while ping is outstanding
}

uint32_t
Http2Session::RegisterStreamID(Http2Stream *stream, uint32_t aNewID)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  MOZ_ASSERT(mNextStreamID < 0xfffffff0,
             "should have stopped admitting streams");
  MOZ_ASSERT(!(aNewID & 1),
             "0 for autoassign pull, otherwise explicit even push assignment");

  if (!aNewID) {
    // auto generate a new pull stream ID
    aNewID = mNextStreamID;
    MOZ_ASSERT(aNewID & 1, "pull ID must be odd.");
    mNextStreamID += 2;
  }

  LOG3(("Http2Session::RegisterStreamID session=%p stream=%p id=0x%X "
        "concurrent=%d",this, stream, aNewID, mConcurrent));

  // We've used up plenty of ID's on this session. Start
  // moving to a new one before there is a crunch involving
  // server push streams or concurrent non-registered submits
  if (aNewID >= kMaxStreamID)
    mShouldGoAway = true;

  // integrity check
  if (mStreamIDHash.Get(aNewID)) {
    LOG3(("   New ID already present\n"));
    MOZ_ASSERT(false, "New ID already present in mStreamIDHash");
    mShouldGoAway = true;
    return kDeadStreamID;
  }

  mStreamIDHash.Put(aNewID, stream);
  return aNewID;
}

bool
Http2Session::AddStream(nsAHttpTransaction *aHttpTransaction,
                        int32_t aPriority)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  // integrity check
  if (mStreamTransactionHash.Get(aHttpTransaction)) {
    LOG3(("   New transaction already present\n"));
    MOZ_ASSERT(false, "AddStream duplicate transaction pointer");
    return false;
  }

  aHttpTransaction->SetConnection(this);
  Http2Stream *stream = new Http2Stream(aHttpTransaction, this, aPriority);

  LOG3(("Http2Session::AddStream session=%p stream=%p NextID=0x%X (tentative)",
        this, stream, mNextStreamID));

  mStreamTransactionHash.Put(aHttpTransaction, stream);

  if (RoomForMoreConcurrent()) {
    LOG3(("Http2Session::AddStream %p stream %p activated immediately.",
          this, stream));
    ActivateStream(stream);
  } else {
    LOG3(("Http2Session::AddStream %p stream %p queued.", this, stream));
    mQueuedStreams.Push(stream);
  }

  if (!(aHttpTransaction->Caps() & NS_HTTP_ALLOW_KEEPALIVE)) {
    LOG3(("Http2Session::AddStream %p transaction %p forces keep-alive off.\n",
          this, aHttpTransaction));
    DontReuse();
  }

  return true;
}

void
Http2Session::ActivateStream(Http2Stream *stream)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  MOZ_ASSERT(!stream->StreamID() || (stream->StreamID() & 1),
             "Do not activate pushed streams");

  MOZ_ASSERT(!stream->CountAsActive());
  stream->SetCountAsActive(true);
  ++mConcurrent;

  if (mConcurrent > mConcurrentHighWater)
    mConcurrentHighWater = mConcurrent;
  LOG3(("Http2Session::AddStream %p activating stream %p Currently %d "
        "streams in session, high water mark is %d",
        this, stream, mConcurrent, mConcurrentHighWater));

  mReadyForWrite.Push(stream);
  SetWriteCallbacks();

  // Kick off the headers transmit without waiting for the poll loop
  // This won't work for stream id=1 because there is no segment reader
  // yet.
  if (mSegmentReader) {
    uint32_t countRead;
    ReadSegments(nullptr, kDefaultBufferSize, &countRead);
  }
}

void
Http2Session::ProcessPending()
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  while (RoomForMoreConcurrent()) {
    Http2Stream *stream = static_cast<Http2Stream *>(mQueuedStreams.PopFront());
    if (!stream)
      return;
    LOG3(("Http2Session::ProcessPending %p stream %p activated from queue.",
          this, stream));
    ActivateStream(stream);
  }
}

nsresult
Http2Session::NetworkRead(nsAHttpSegmentWriter *writer, char *buf,
                          uint32_t count, uint32_t *countWritten)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

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
Http2Session::SetWriteCallbacks()
{
  if (mConnection && (GetWriteQueueSize() || mOutputQueueUsed))
    mConnection->ResumeSend();
}

void
Http2Session::RealignOutputQueue()
{
  mOutputQueueUsed -= mOutputQueueSent;
  memmove(mOutputQueueBuffer.get(),
          mOutputQueueBuffer.get() + mOutputQueueSent,
          mOutputQueueUsed);
  mOutputQueueSent = 0;
}

void
Http2Session::FlushOutputQueue()
{
  if (!mSegmentReader || !mOutputQueueUsed)
    return;

  nsresult rv;
  uint32_t countRead;
  uint32_t avail = mOutputQueueUsed - mOutputQueueSent;

  rv = mSegmentReader->
    OnReadSegment(mOutputQueueBuffer.get() + mOutputQueueSent, avail,
                  &countRead);
  LOG3(("Http2Session::FlushOutputQueue %p sz=%d rv=%x actual=%d",
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
Http2Session::DontReuse()
{
  mShouldGoAway = true;
  if (!mStreamTransactionHash.Count())
    Close(NS_OK);
}

uint32_t
Http2Session::GetWriteQueueSize()
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  return mReadyForWrite.GetSize();
}

void
Http2Session::ChangeDownstreamState(enum internalStateType newState)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  LOG3(("Http2Stream::ChangeDownstreamState() %p from %X to %X",
        this, mDownstreamState, newState));
  mDownstreamState = newState;
}

void
Http2Session::ResetDownstreamState()
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  LOG3(("Http2Stream::ResetDownstreamState() %p", this));
  ChangeDownstreamState(BUFFERING_FRAME_HEADER);

  if (mInputFrameFinal && mInputFrameDataStream) {
    mInputFrameFinal = false;
    LOG3(("  SetRecvdFin id=0x%x\n", mInputFrameDataStream->StreamID()));
    mInputFrameDataStream->SetRecvdFin(true);
    MaybeDecrementConcurrent(mInputFrameDataStream);
  }
  mInputFrameBufferUsed = 0;
  mInputFrameDataStream = nullptr;
}

template<typename T> void
Http2Session::EnsureBuffer(nsAutoArrayPtr<T> &buf, uint32_t newSize,
                           uint32_t preserve, uint32_t &objSize)
{
  if (objSize >= newSize)
    return;

  // Leave a little slop on the new allocation - add 2KB to
  // what we need and then round the result up to a 4KB (page)
  // boundary.

  objSize = (newSize + 2048 + 4095) & ~4095;

  static_assert(sizeof(T) == 1, "sizeof(T) must be 1");
  nsAutoArrayPtr<T> tmp(new T[objSize]);
  memcpy(tmp, buf, preserve);
  buf = tmp;
}

// Instantiate supported templates explicitly.
template void
Http2Session::EnsureBuffer(nsAutoArrayPtr<char> &buf, uint32_t newSize,
                                  uint32_t preserve, uint32_t &objSize);

template void
Http2Session::EnsureBuffer(nsAutoArrayPtr<uint8_t> &buf, uint32_t newSize,
                                  uint32_t preserve, uint32_t &objSize);

// call with data length (i.e. 0 for 0 data bytes - ignore 8 byte header)
// dest must have 8 bytes of allocated space
template<typename charType> void
Http2Session::CreateFrameHeader(charType dest, uint16_t frameLength,
                                uint8_t frameType, uint8_t frameFlags,
                                uint32_t streamID)
{
  MOZ_ASSERT(frameLength <= kMaxFrameData, "framelength too large");
  MOZ_ASSERT(!(streamID & 0x80000000));

  frameLength = PR_htons(frameLength);
  streamID = PR_htonl(streamID);

  memcpy(dest, &frameLength, 2);
  dest[2] = frameType;
  dest[3] = frameFlags;
  memcpy(dest + 4, &streamID, 4);
}

char *
Http2Session::EnsureOutputBuffer(uint32_t spaceNeeded)
{
  // this is an infallible allocation (if an allocation is
  // needed, which is probably isn't)
  EnsureBuffer(mOutputQueueBuffer, mOutputQueueUsed + spaceNeeded,
               mOutputQueueUsed, mOutputQueueSize);
  return mOutputQueueBuffer.get() + mOutputQueueUsed;
}

template void
Http2Session::CreateFrameHeader(char *dest, uint16_t frameLength,
                                uint8_t frameType, uint8_t frameFlags,
                                uint32_t streamID);

template void
Http2Session::CreateFrameHeader(uint8_t *dest, uint16_t frameLength,
                                uint8_t frameType, uint8_t frameFlags,
                                uint32_t streamID);

void
Http2Session::MaybeDecrementConcurrent(Http2Stream *aStream)
{
  LOG3(("MaybeDecrementConcurrent %p id=0x%X concurrent=%d active=%d\n",
        this, aStream->StreamID(), mConcurrent, aStream->CountAsActive()));

  if (!aStream->CountAsActive())
    return;

  MOZ_ASSERT(mConcurrent);
  aStream->SetCountAsActive(false);
  --mConcurrent;
  ProcessPending();
}

// Need to decompress some data in order to keep the compression
// context correct, but we really don't care what the result is
nsresult
Http2Session::UncompressAndDiscard()
{
  nsresult rv;
  nsAutoCString trash;

  rv = mDecompressor.DecodeHeaderBlock(reinterpret_cast<const uint8_t *>(mDecompressBuffer.BeginReading()),
                                       mDecompressBuffer.Length(), trash);
  mDecompressBuffer.Truncate();
  if (NS_FAILED(rv)) {
    LOG3(("Http2Session::UncompressAndDiscard %p Compression Error\n",
          this));
    mGoAwayReason = COMPRESSION_ERROR;
    return rv;
  }
  return NS_OK;
}

void
Http2Session::GeneratePing(bool isAck)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  LOG3(("Http2Session::GeneratePing %p isAck=%d\n", this, isAck));

  char *packet = EnsureOutputBuffer(16);
  mOutputQueueUsed += 16;

  if (isAck) {
    CreateFrameHeader(packet, 8, FRAME_TYPE_PING, kFlag_ACK, 0);
    memcpy(packet + 8, mInputFrameBuffer.get() + 8, 8);
  } else {
    CreateFrameHeader(packet, 8, FRAME_TYPE_PING, 0, 0);
    memset(packet + 8, 0, 8);
  }

  LogIO(this, nullptr, "Generate Ping", packet, 16);
  FlushOutputQueue();
}

void
Http2Session::GenerateSettingsAck()
{
  // need to generate ack of this settings frame
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  LOG3(("Http2Session::GenerateSettingsAck %p\n", this));

  char *packet = EnsureOutputBuffer(8);
  mOutputQueueUsed += 8;
  CreateFrameHeader(packet, 0, FRAME_TYPE_SETTINGS, kFlag_ACK, 0);
  LogIO(this, nullptr, "Generate Settings ACK", packet, 8);
  FlushOutputQueue();
}

void
Http2Session::GeneratePriority(uint32_t aID, uint32_t aPriority)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  LOG3(("Http2Session::GeneratePriority %p %X %X\n",
        this, aID, aPriority));

  char *packet = EnsureOutputBuffer(12);
  mOutputQueueUsed += 12;

  CreateFrameHeader(packet, 4, FRAME_TYPE_PRIORITY, 0, aID);
  aPriority = PR_htonl(aPriority);
  memcpy(packet + 8, &aPriority, 4);
  LogIO(this, nullptr, "Generate Priority", packet, 12);
  FlushOutputQueue();
}

void
Http2Session::GenerateRstStream(uint32_t aStatusCode, uint32_t aID)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  // make sure we don't do this twice for the same stream (at least if we
  // have a stream entry for it)
  Http2Stream *stream = mStreamIDHash.Get(aID);
  if (stream) {
    if (stream->SentReset())
      return;
    stream->SetSentReset(true);
  }

  LOG3(("Http2Session::GenerateRst %p 0x%X %d\n", this, aID, aStatusCode));

  char *packet = EnsureOutputBuffer(12);
  mOutputQueueUsed += 12;
  CreateFrameHeader(packet, 4, FRAME_TYPE_RST_STREAM, 0, aID);

  aStatusCode = PR_htonl(aStatusCode);
  memcpy(packet + 8, &aStatusCode, 4);

  LogIO(this, nullptr, "Generate Reset", packet, 12);
  FlushOutputQueue();
}

void
Http2Session::GenerateGoAway(uint32_t aStatusCode)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  LOG3(("Http2Session::GenerateGoAway %p code=%X\n", this, aStatusCode));

  char *packet = EnsureOutputBuffer(16);
  mOutputQueueUsed += 16;

  memset(packet + 8, 0, 8);
  CreateFrameHeader(packet, 8, FRAME_TYPE_GOAWAY, 0, 0);

  // last-good-stream-id are bytes 8-11 reflecting pushes
  uint32_t goAway = PR_htonl(mOutgoingGoAwayID);
  memcpy (packet + 7, &goAway, 4);

  // bytes 12-15 are the status code.
  aStatusCode = PR_htonl(aStatusCode);
  memcpy(packet + 12, &aStatusCode, 4);

  LogIO(this, nullptr, "Generate GoAway", packet, 16);
  FlushOutputQueue();
}

// The Hello is comprised of 24 octets of magic, which are designed to
// flush out silent but broken intermediaries, followed by a settings
// frame which sets a small flow control window for pushes and a
// window update frame which creates a large session flow control window
void
Http2Session::SendHello()
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  LOG3(("Http2Session::SendHello %p\n", this));

  // sized for magic + 2 settings and a session window update to follow
  // 24 magic, 32 for settings (8 header + 3 settings @8), 12 for window update
  static const uint32_t maxSettings = 3;
  static const uint32_t maxDataLen = 24 + 8 + maxSettings * 8 + 12;
  char *packet = EnsureOutputBuffer(maxDataLen);
  memcpy(packet, kMagicHello, 24);
  mOutputQueueUsed += 24;
  LogIO(this, nullptr, "Magic Connection Header", packet, 24);

  packet = mOutputQueueBuffer.get() + mOutputQueueUsed;
  memset(packet, 0, maxDataLen - 24);

  // frame header will be filled in after we know how long the frame is
  uint8_t numberOfEntries = 0;

  // entries need to be listed in order by ID
  // 1st entry is bytes 8 to 15
  // 2nd entry is bytes 16 to 23
  // 3rd entry is bytes 24 to 31

  if (!gHttpHandler->AllowPush()) {
    // If we don't support push then set MAX_CONCURRENT to 0 and also
    // set ENABLE_PUSH to 0
    packet[11 + 8 * numberOfEntries] = SETTINGS_TYPE_ENABLE_PUSH;
    // The value portion of the setting pair is already initialized to 0
    numberOfEntries++;

    packet[11 + 8 * numberOfEntries] = SETTINGS_TYPE_MAX_CONCURRENT;
    // The value portion of the setting pair is already initialized to 0
    numberOfEntries++;
  }

  // Advertise the Push RWIN for the session, and on each new pull stream
  // send a window update with END_FLOW_CONTROL
  packet[11 + 8 * numberOfEntries] = SETTINGS_TYPE_INITIAL_WINDOW;
  uint32_t rwin = PR_htonl(mPushAllowance);
  memcpy(packet + 12 + 8 * numberOfEntries, &rwin, 4);
  numberOfEntries++;

  MOZ_ASSERT(numberOfEntries <= maxSettings);
  uint32_t dataLen = 8 * numberOfEntries;
  CreateFrameHeader(packet, dataLen, FRAME_TYPE_SETTINGS, 0, 0);
  mOutputQueueUsed += 8 + dataLen;

  LogIO(this, nullptr, "Generate Settings", packet, 8 + dataLen);

  // now bump the local session window from 64KB
  uint32_t sessionWindowBump = ASpdySession::kInitialRwin - kDefaultRwin;
  if (kDefaultRwin >= ASpdySession::kInitialRwin)
    goto sendHello_complete;

  // send a window update for the session (Stream 0) for something large
  sessionWindowBump = PR_htonl(sessionWindowBump);
  mLocalSessionWindow = ASpdySession::kInitialRwin;

  packet = mOutputQueueBuffer.get() + mOutputQueueUsed;
  CreateFrameHeader(packet, 4, FRAME_TYPE_WINDOW_UPDATE, 0, 0);
  mOutputQueueUsed += 12;
  memcpy(packet + 8, &sessionWindowBump, 4);

  LOG3(("Session Window increase at start of session %p %u\n",
        this, PR_ntohl(sessionWindowBump)));
  LogIO(this, nullptr, "Session Window Bump ", packet, 12);

sendHello_complete:
  FlushOutputQueue();
}

// perform a bunch of integrity checks on the stream.
// returns true if passed, false (plus LOG and ABORT) if failed.
bool
Http2Session::VerifyStream(Http2Stream *aStream, uint32_t aOptionalID = 0)
{
  // This is annoying, but at least it is O(1)
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

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
      Http2Stream *idStream = mStreamIDHash.Get(aStream->StreamID());

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

  LOG3(("Http2Session %p VerifyStream Failure %p stream->id=0x%X "
       "optionalID=0x%X trans=%p test=%d\n",
       this, aStream, aStream->StreamID(),
       aOptionalID, aStream->Transaction(), test));

  MOZ_ASSERT(false, "VerifyStream");
  return false;
}

void
Http2Session::CleanupStream(Http2Stream *aStream, nsresult aResult,
                            errorType aResetCode)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  LOG3(("Http2Session::CleanupStream %p %p 0x%X %X\n",
        this, aStream, aStream ? aStream->StreamID() : 0, aResult));
  if (!aStream) {
    return;
  }

  Http2PushedStream *pushSource = nullptr;

  if (NS_SUCCEEDED(aResult) && aStream->DeferCleanupOnSuccess()) {
    LOG3(("Http2Session::CleanupStream 0x%X deferred\n", aStream->StreamID()));
    return;
  }

  if (!VerifyStream(aStream)) {
    LOG3(("Http2Session::CleanupStream failed to verify stream\n"));
    return;
  }

  pushSource = aStream->PushSource();

  if (!aStream->RecvdFin() && !aStream->RecvdReset() && aStream->StreamID()) {
    LOG3(("Stream had not processed recv FIN, sending RST code %X\n",
          aResetCode));
    GenerateRstStream(aResetCode, aStream->StreamID());
  }

  CloseStream(aStream, aResult);

  // Remove the stream from the ID hash table and, if an even id, the pushed
  // table too.
  uint32_t id = aStream->StreamID();
  if (id > 0) {
    mStreamIDHash.Remove(id);
    if (!(id & 1))
      mPushedStreams.RemoveElement(aStream);
  }

  RemoveStreamFromQueues(aStream);

  // removing from the stream transaction hash will
  // delete the Http2Stream and drop the reference to
  // its transaction
  mStreamTransactionHash.Remove(aStream->Transaction());

  if (mShouldGoAway && !mStreamTransactionHash.Count())
    Close(NS_OK);

  if (pushSource) {
    pushSource->SetDeferCleanupOnSuccess(false);
    CleanupStream(pushSource, aResult, aResetCode);
  }
}

static void RemoveStreamFromQueue(Http2Stream *aStream, nsDeque &queue)
{
  uint32_t size = queue.GetSize();
  for (uint32_t count = 0; count < size; ++count) {
    Http2Stream *stream = static_cast<Http2Stream *>(queue.PopFront());
    if (stream != aStream)
      queue.Push(stream);
  }
}

void
Http2Session::RemoveStreamFromQueues(Http2Stream *aStream)
{
  RemoveStreamFromQueue(aStream, mReadyForWrite);
  RemoveStreamFromQueue(aStream, mQueuedStreams);
  RemoveStreamFromQueue(aStream, mReadyForRead);
}

void
Http2Session::CloseStream(Http2Stream *aStream, nsresult aResult)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  LOG3(("Http2Session::CloseStream %p %p 0x%x %X\n",
        this, aStream, aStream->StreamID(), aResult));

  MaybeDecrementConcurrent(aStream);

  // Check if partial frame reader
  if (aStream == mInputFrameDataStream) {
    LOG3(("Stream had active partial read frame on close"));
    ChangeDownstreamState(DISCARDING_DATA_FRAME);
    mInputFrameDataStream = nullptr;
  }

  RemoveStreamFromQueues(aStream);

  // Send the stream the close() indication
  aStream->Close(aResult);
}

nsresult
Http2Session::SetInputFrameDataStream(uint32_t streamID)
{
  mInputFrameDataStream = mStreamIDHash.Get(streamID);
  if (VerifyStream(mInputFrameDataStream, streamID))
    return NS_OK;

  LOG3(("Http2Session::SetInputFrameDataStream failed to verify 0x%X\n",
       streamID));
  mInputFrameDataStream = nullptr;
  return NS_ERROR_UNEXPECTED;
}

nsresult
Http2Session::RecvHeaders(Http2Session *self)
{
  MOZ_ASSERT(self->mInputFrameType == FRAME_TYPE_HEADERS);

  // If this doesn't have END_HEADERS set on it then require the next
  // frame to be HEADERS of the same ID
  bool endHeadersFlag = self->mInputFrameFlags & kFlag_END_HEADERS;

  if (endHeadersFlag)
    self->mExpectedHeaderID = 0;
  else
    self->mExpectedHeaderID = self->mInputFrameID;

  uint32_t priorityLen = (self->mInputFrameFlags & kFlag_PRIORITY) ? 4 : 0;
  self->SetInputFrameDataStream(self->mInputFrameID);

  LOG3(("Http2Session::RecvHeaders %p stream 0x%X priorityLen=%d stream=%p "
        "end_stream=%d end_headers=%d priority_flag=%d\n",
        self, self->mInputFrameID, priorityLen, self->mInputFrameDataStream,
        self->mInputFrameFlags & kFlag_END_STREAM,
        self->mInputFrameFlags & kFlag_END_HEADERS,
        self->mInputFrameFlags & kFlag_PRIORITY));

  if (!self->mInputFrameDataStream) {
    // Cannot find stream. We can continue the session, but we need to
    // uncompress the header block to maintain the correct compression context

    LOG3(("Http2Session::RecvHeaders %p lookup mInputFrameID stream "
          "0x%X failed. NextStreamID = 0x%X\n",
          self, self->mInputFrameID, self->mNextStreamID));

    if (self->mInputFrameID >= self->mNextStreamID)
      self->GenerateRstStream(PROTOCOL_ERROR, self->mInputFrameID);

    self->mDecompressBuffer.Append(self->mInputFrameBuffer + 8 + priorityLen,
                                   self->mInputFrameDataSize - priorityLen);

    if (self->mInputFrameFlags & kFlag_END_HEADERS) {
      nsresult rv = self->UncompressAndDiscard();
      if (NS_FAILED(rv)) {
        LOG3(("Http2Session::RecvHeaders uncompress failed\n"));
        // this is fatal to the session
        self->mGoAwayReason = COMPRESSION_ERROR;
        return rv;
      }
    }

    self->ResetDownstreamState();
    return NS_OK;
  }

  // queue up any compression bytes
  self->mDecompressBuffer.Append(self->mInputFrameBuffer + 8 + priorityLen,
                                 self->mInputFrameDataSize - priorityLen);

  self->mInputFrameDataStream->UpdateTransportReadEvents(self->mInputFrameDataSize);
  self->mLastDataReadEpoch = self->mLastReadEpoch;

  if (!endHeadersFlag) { // more are coming - don't process yet
    self->ResetDownstreamState();
    return NS_OK;
  }

  nsresult rv = self->ResponseHeadersComplete();
  if (rv == NS_ERROR_ILLEGAL_VALUE) {
    LOG3(("Http2Session::RecvHeaders %p PROTOCOL_ERROR detected stream 0x%X\n",
          self, self->mInputFrameID));
    self->CleanupStream(self->mInputFrameDataStream, rv, PROTOCOL_ERROR);
    self->ResetDownstreamState();
    rv = NS_OK;
  }
  return rv;
}

// ResponseHeadersComplete() returns NS_ERROR_ILLEGAL_VALUE when the stream
// should be reset with a PROTOCOL_ERROR, NS_OK when the response headers were
// fine, and any other error is fatal to the session.
nsresult
Http2Session::ResponseHeadersComplete()
{
  LOG3(("Http2Session::ResponseHeadersComplete %p for 0x%X fin=%d",
        this, mInputFrameDataStream->StreamID(), mInputFrameFinal));

  // only do this once, afterwards ignore trailers
  if (mInputFrameDataStream->AllHeadersReceived())
    return NS_OK;
  mInputFrameDataStream->SetAllHeadersReceived(true);

  // The stream needs to see flattened http headers
  // Uncompressed http/2 format headers currently live in
  // Http2Stream::mDecompressBuffer - convert that to HTTP format in
  // mFlatHTTPResponseHeaders via ConvertHeaders()

  mFlatHTTPResponseHeadersOut = 0;
  nsresult rv = mInputFrameDataStream->ConvertResponseHeaders(&mDecompressor,
                                                              mDecompressBuffer,
                                                              mFlatHTTPResponseHeaders);
  if (NS_FAILED(rv))
    return rv;

  ChangeDownstreamState(PROCESSING_COMPLETE_HEADERS);
  return NS_OK;
}

nsresult
Http2Session::RecvPriority(Http2Session *self)
{
  MOZ_ASSERT(self->mInputFrameType == FRAME_TYPE_PRIORITY);

  if (self->mInputFrameDataSize != 4) {
    LOG3(("Http2Session::RecvPriority %p wrong length data=%d\n",
          self, self->mInputFrameDataSize));
    RETURN_SESSION_ERROR(self, PROTOCOL_ERROR);
  }

  if (!self->mInputFrameID) {
    LOG3(("Http2Session::RecvPriority %p stream ID of 0.\n", self));
    RETURN_SESSION_ERROR(self, PROTOCOL_ERROR);
  }

  uint32_t newPriority =
    PR_ntohl(reinterpret_cast<uint32_t *>(self->mInputFrameBuffer.get())[2]);
  newPriority &= 0x7fffffff;

  nsresult rv = self->SetInputFrameDataStream(self->mInputFrameID);
  if (NS_FAILED(rv))
    return rv;

  if (self->mInputFrameDataStream)
    self->mInputFrameDataStream->SetPriority(newPriority);
  self->ResetDownstreamState();
  return NS_OK;
}

nsresult
Http2Session::RecvRstStream(Http2Session *self)
{
  MOZ_ASSERT(self->mInputFrameType == FRAME_TYPE_RST_STREAM);

  if (self->mInputFrameDataSize != 4) {
    LOG3(("Http2Session::RecvRstStream %p RST_STREAM wrong length data=%d",
          self, self->mInputFrameDataSize));
    RETURN_SESSION_ERROR(self, PROTOCOL_ERROR);
  }

  if (!self->mInputFrameID) {
    LOG3(("Http2Session::RecvRstStream %p stream ID of 0.\n", self));
    RETURN_SESSION_ERROR(self, PROTOCOL_ERROR);
  }

  self->mDownstreamRstReason =
    PR_ntohl(reinterpret_cast<uint32_t *>(self->mInputFrameBuffer.get())[2]);

  LOG3(("Http2Session::RecvRstStream %p RST_STREAM Reason Code %u ID %x\n",
        self, self->mDownstreamRstReason, self->mInputFrameID));

  self->SetInputFrameDataStream(self->mInputFrameID);
  if (!self->mInputFrameDataStream) {
    // if we can't find the stream just ignore it (4.2 closed)
    self->ResetDownstreamState();
    return NS_OK;
  }

  self->mInputFrameDataStream->SetRecvdReset(true);
  self->MaybeDecrementConcurrent(self->mInputFrameDataStream);
  self->ChangeDownstreamState(PROCESSING_CONTROL_RST_STREAM);
  return NS_OK;
}

PLDHashOperator
Http2Session::UpdateServerRwinEnumerator(nsAHttpTransaction *key,
                                         nsAutoPtr<Http2Stream> &stream,
                                         void *closure)
{
  int32_t delta = *(static_cast<int32_t *>(closure));
  stream->UpdateServerReceiveWindow(delta);
  return PL_DHASH_NEXT;
}

nsresult
Http2Session::RecvSettings(Http2Session *self)
{
  MOZ_ASSERT(self->mInputFrameType == FRAME_TYPE_SETTINGS);

  if (self->mInputFrameID) {
    LOG3(("Http2Session::RecvSettings %p needs stream ID of 0. 0x%X\n",
          self, self->mInputFrameID));
    RETURN_SESSION_ERROR(self, PROTOCOL_ERROR);
  }

  if (self->mInputFrameDataSize & 7) {
    // Number of Setting is determined by dividing by each 8 byte setting
    // entry. So the payload must be a multiple of 8.
    LOG3(("Http2Session::RecvSettings %p SETTINGS wrong length data=%d",
          self, self->mInputFrameDataSize));
    RETURN_SESSION_ERROR(self, PROTOCOL_ERROR);
  }

  uint32_t numEntries = self->mInputFrameDataSize >> 3;
  LOG3(("Http2Session::RecvSettings %p SETTINGS Control Frame "
        "with %d entries ack=%X", self, numEntries,
        self->mInputFrameFlags & kFlag_ACK));

  if ((self->mInputFrameFlags & kFlag_ACK) && self->mInputFrameDataSize) {
    LOG3(("Http2Session::RecvSettings %p ACK with non zero payload is err\n"));
    RETURN_SESSION_ERROR(self, PROTOCOL_ERROR);
  }

  for (uint32_t index = 0; index < numEntries; ++index) {
    uint8_t *setting = reinterpret_cast<uint8_t *>
      (self->mInputFrameBuffer.get()) + 8 + index * 8;

    uint32_t id = PR_ntohl(reinterpret_cast<uint32_t *>(setting)[0]) & 0xffffff;
    uint32_t value = PR_ntohl(reinterpret_cast<uint32_t *>(setting)[1]);
    LOG3(("Settings ID %d, Value %d", id, value));

    switch (id)
    {
    case SETTINGS_TYPE_HEADER_TABLE_SIZE:
      LOG3(("Compression header table setting received: %d\n", value));
      self->mCompressor.SetMaxBufferSize(value);
      break;

    case SETTINGS_TYPE_ENABLE_PUSH:
      LOG3(("Client received an ENABLE Push SETTING. Odd.\n"));
      // nop
      break;

    case SETTINGS_TYPE_MAX_CONCURRENT:
      self->mMaxConcurrent = value;
      Telemetry::Accumulate(Telemetry::SPDY_SETTINGS_MAX_STREAMS, value);
      break;

    case SETTINGS_TYPE_INITIAL_WINDOW:
      {
        Telemetry::Accumulate(Telemetry::SPDY_SETTINGS_IW, value >> 10);
        int32_t delta = value - self->mServerInitialStreamWindow;
        self->mServerInitialStreamWindow = value;

        // SETTINGS only adjusts stream windows. Leave the sesison window alone.
        // we need to add the delta to all open streams (delta can be negative)
        self->mStreamTransactionHash.Enumerate(UpdateServerRwinEnumerator,
                                               &delta);
      }
      break;

    case SETTINGS_TYPE_FLOW_CONTROL:
      if (value & 1) {
        LOG3(("Http2Session::RecvSettings %p DISABLE FLOW CONTROL\n", self));
        self->mServerUsesFlowControl = false;

        // we need to touch all existing streams to unpause any that were
        // flow control restricted
        int32_t delta = 0;
        self->mStreamTransactionHash.Enumerate(UpdateServerRwinEnumerator,
                                               &delta);
      } else {
        if (!self->mServerUsesFlowControl) {
          // clearing this setting is a protocol error
          LOG3(("Http2Session::RecvSettings %p "
                "Cannot Clear FLOW_CONTROL SETTINGS bits\n",
                self));
          RETURN_SESSION_ERROR(self, FLOW_CONTROL_ERROR);
        }
      }
      break;

    default:
      break;
    }
  }

  self->ResetDownstreamState();

  if (!(self->mInputFrameFlags & kFlag_ACK))
    self->GenerateSettingsAck();

  return NS_OK;
}

nsresult
Http2Session::RecvPushPromise(Http2Session *self)
{
  MOZ_ASSERT(self->mInputFrameType == FRAME_TYPE_PUSH_PROMISE);

  // If this doesn't have END_PUSH_PROMISE set on it then require the next
  // frame to be PUSH_PROMISE of the same ID
  uint32_t promiseLen;
  uint32_t promisedID;

  if (self->mExpectedPushPromiseID) {
    promiseLen = 0; // really a continuation frame
    promisedID = self->mContinuedPromiseStream;
  } else {
    promiseLen = 4;
    promisedID =
      PR_ntohl(reinterpret_cast<uint32_t *>(self->mInputFrameBuffer.get())[2]);
    promisedID &= 0x7fffffff;
  }

  uint32_t associatedID = self->mInputFrameID;

  if (self->mInputFrameFlags & kFlag_END_PUSH_PROMISE) {
    self->mExpectedPushPromiseID = 0;
    self->mContinuedPromiseStream = 0;
  } else {
    self->mExpectedPushPromiseID = self->mInputFrameID;
    self->mContinuedPromiseStream = promisedID;
  }

  LOG3(("Http2Session::RecvPushPromise %p ID 0x%X assoc ID 0x%X.\n",
        self, promisedID, associatedID));

  if (!associatedID || !promisedID || (promisedID & 1)) {
    LOG3(("Http2Session::RecvPushPromise %p ID invalid.\n", self));
    RETURN_SESSION_ERROR(self, PROTOCOL_ERROR);
  }

  // confirm associated-to
  nsresult rv = self->SetInputFrameDataStream(associatedID);
  if (NS_FAILED(rv))
    return rv;

  Http2Stream *associatedStream = self->mInputFrameDataStream;
  ++(self->mServerPushedResources);

  // Anytime we start using the high bit of stream ID (either client or server)
  // begin to migrate to a new session.
  if (promisedID >= kMaxStreamID)
    self->mShouldGoAway = true;

  bool resetStream = true;
  SpdyPushCache *cache = nullptr;

  if (self->mShouldGoAway) {
    LOG3(("Http2Session::RecvPushPromise %p push while in GoAway "
          "mode refused.\n", self));
    self->GenerateRstStream(REFUSED_STREAM_ERROR, promisedID);
  } else if (!gHttpHandler->AllowPush()) {
    // MAX_CONCURRENT_STREAMS of 0 in settings disabled push
    LOG3(("Http2Session::RecvPushPromise Push Recevied when Disabled\n"));
    self->GenerateRstStream(REFUSED_STREAM_ERROR, promisedID);
  } else if (!(self->mInputFrameFlags & kFlag_END_PUSH_PROMISE)) {
    LOG3(("Http2Session::RecvPushPromise no support for multi frame push\n"));
    self->GenerateRstStream(REFUSED_STREAM_ERROR, promisedID);
  } else if (!associatedStream) {
    LOG3(("Http2Session::RecvPushPromise %p lookup associated ID failed.\n", self));
    self->GenerateRstStream(PROTOCOL_ERROR, promisedID);
  } else {
    nsILoadGroupConnectionInfo *loadGroupCI = associatedStream->LoadGroupConnectionInfo();
    if (loadGroupCI) {
      loadGroupCI->GetSpdyPushCache(&cache);
      if (!cache) {
        cache = new SpdyPushCache();
        if (!cache || NS_FAILED(loadGroupCI->SetSpdyPushCache(cache))) {
          delete cache;
          cache = nullptr;
        }
      }
    }
    if (!cache) {
      // this is unexpected, but we can handle it just by refusing the push
      LOG3(("Http2Session::RecvPushPromise Push Recevied without loadgroup cache\n"));
      self->GenerateRstStream(REFUSED_STREAM_ERROR, promisedID);
    } else {
      resetStream = false;
    }
  }

  if (resetStream) {
    // Need to decompress the headers even though we aren't using them yet in
    // order to keep the compression context consistent for other frames
    self->mDecompressBuffer.Append(self->mInputFrameBuffer + 8 + promiseLen,
                                   self->mInputFrameDataSize - promiseLen);
    if (self->mInputFrameFlags & kFlag_END_PUSH_PROMISE) {
      rv = self->UncompressAndDiscard();
      if (NS_FAILED(rv)) {
        LOG3(("Http2Session::RecvPushPromise uncompress failed\n"));
        self->mGoAwayReason = COMPRESSION_ERROR;
        return rv;
      }
    }
    self->ResetDownstreamState();
    return NS_OK;
  }

  // Create the buffering transaction and push stream
  nsRefPtr<Http2PushTransactionBuffer> transactionBuffer =
    new Http2PushTransactionBuffer();
  transactionBuffer->SetConnection(self);
  Http2PushedStream *pushedStream =
    new Http2PushedStream(transactionBuffer, self,
                                 associatedStream, promisedID);

  // Ownership of the pushed stream is by the transaction hash, just as it
  // is for a client initiated stream. Errors that aren't fatal to the
  // whole session must call cleanupStream() after this point in order
  // to remove the stream from that hash.
  self->mStreamTransactionHash.Put(transactionBuffer, pushedStream);
  self->mPushedStreams.AppendElement(pushedStream);

  self->mDecompressBuffer.Append(self->mInputFrameBuffer + 8 + promiseLen,
                                 self->mInputFrameDataSize - promiseLen);

  nsAutoCString requestHeaders;
  rv = pushedStream->ConvertPushHeaders(&self->mDecompressor,
                                        self->mDecompressBuffer, requestHeaders);

  if (rv == NS_ERROR_NOT_IMPLEMENTED) {
    LOG3(("Http2Session::PushPromise Semantics not Implemented\n"));
    self->GenerateRstStream(REFUSED_STREAM_ERROR, promisedID);
    return NS_OK;
  }

  if (NS_FAILED(rv))
    return rv;

  if (self->RegisterStreamID(pushedStream, promisedID) == kDeadStreamID) {
    LOG3(("Http2Session::RecvPushPromise registerstreamid failed\n"));
    self->mGoAwayReason = INTERNAL_ERROR;
    return NS_ERROR_FAILURE;
  }

  if (promisedID > self->mOutgoingGoAwayID)
    self->mOutgoingGoAwayID = promisedID;

  // Fake the request side of the pushed HTTP transaction. Sets up hash
  // key and origin
  uint32_t notUsed;
  pushedStream->ReadSegments(nullptr, 1, &notUsed);

  nsAutoCString key;
  if (!pushedStream->GetHashKey(key)) {
    LOG3(("Http2Session::RecvPushPromise one of :authority :scheme :path missing from push\n"));
    self->CleanupStream(pushedStream, NS_ERROR_FAILURE, PROTOCOL_ERROR);
    self->ResetDownstreamState();
    return NS_OK;
  }

  if (!associatedStream->Origin().Equals(pushedStream->Origin())) {
    LOG3(("Http2Session::RecvPushPromise pushed stream mismatched origin\n"));
    self->CleanupStream(pushedStream, NS_ERROR_FAILURE, REFUSED_STREAM_ERROR);
    self->ResetDownstreamState();
    return NS_OK;
  }

  if (!cache->RegisterPushedStreamHttp2(key, pushedStream)) {
    LOG3(("Http2Session::RecvPushPromise registerPushedStream Failed\n"));
    self->CleanupStream(pushedStream, NS_ERROR_FAILURE, INTERNAL_ERROR);
    self->ResetDownstreamState();
    return NS_OK;
  }

  static_assert(Http2Stream::kWorstPriority >= 0,
                "kWorstPriority out of range");
  uint32_t unsignedPriority = static_cast<uint32_t>(Http2Stream::kWorstPriority);
  pushedStream->SetPriority(unsignedPriority);
  self->GeneratePriority(promisedID, unsignedPriority);
  self->ResetDownstreamState();
  return NS_OK;
}

nsresult
Http2Session::RecvPing(Http2Session *self)
{
  MOZ_ASSERT(self->mInputFrameType == FRAME_TYPE_PING);

  LOG3(("Http2Session::RecvPing %p PING Flags 0x%X.", self,
        self->mInputFrameFlags));

  if (self->mInputFrameDataSize != 8) {
    LOG3(("Http2Session::RecvPing %p PING had wrong amount of data %d",
          self, self->mInputFrameDataSize));
    RETURN_SESSION_ERROR(self, FRAME_SIZE_ERROR);
  }

  if (self->mInputFrameID) {
    LOG3(("Http2Session::RecvPing %p PING needs stream ID of 0. 0x%X\n",
          self, self->mInputFrameID));
    RETURN_SESSION_ERROR(self, PROTOCOL_ERROR);
  }

  if (self->mInputFrameFlags & kFlag_ACK) {
    // presumably a reply to our timeout ping.. don't reply to it
    self->mPingSentEpoch = 0;
  } else {
    // reply with a ack'd ping
    self->GeneratePing(true);
  }

  self->ResetDownstreamState();
  return NS_OK;
}

nsresult
Http2Session::RecvGoAway(Http2Session *self)
{
  MOZ_ASSERT(self->mInputFrameType == FRAME_TYPE_GOAWAY);

  if (self->mInputFrameDataSize < 8) {
    // data > 8 is an opaque token that we can't interpret. NSPR Logs will
    // have the hex of all packets so there is no point in separately logging.
    LOG3(("Http2Session::RecvGoAway %p GOAWAY had wrong amount of data %d",
          self, self->mInputFrameDataSize));
    RETURN_SESSION_ERROR(self, PROTOCOL_ERROR);
  }

  if (self->mInputFrameID) {
    LOG3(("Http2Session::RecvGoAway %p GOAWAY had non zero stream ID 0x%X\n",
          self, self->mInputFrameID));
    RETURN_SESSION_ERROR(self, PROTOCOL_ERROR);
  }

  self->mShouldGoAway = true;
  self->mGoAwayID =
    PR_ntohl(reinterpret_cast<uint32_t *>(self->mInputFrameBuffer.get())[2]);
  self->mGoAwayID &= 0x7fffffff;
  self->mCleanShutdown = true;
  uint32_t statusCode =
    PR_ntohl(reinterpret_cast<uint32_t *>(self->mInputFrameBuffer.get())[3]);

  // Find streams greater than the last-good ID and mark them for deletion
  // in the mGoAwayStreamsToRestart queue with the GoAwayEnumerator. The
  // underlying transaction can be restarted.
  self->mStreamTransactionHash.Enumerate(GoAwayEnumerator, self);

  // Process the streams marked for deletion and restart.
  uint32_t size = self->mGoAwayStreamsToRestart.GetSize();
  for (uint32_t count = 0; count < size; ++count) {
    Http2Stream *stream =
      static_cast<Http2Stream *>(self->mGoAwayStreamsToRestart.PopFront());

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
    Http2Stream *stream =
      static_cast<Http2Stream *>(self->mQueuedStreams.PopFront());
    self->CloseStream(stream, NS_ERROR_NET_RESET);
    self->mStreamTransactionHash.Remove(stream->Transaction());
  }

  LOG3(("Http2Session::RecvGoAway %p GOAWAY Last-Good-ID 0x%X status 0x%X "
        "live streams=%d\n", self, self->mGoAwayID, statusCode,
        self->mStreamTransactionHash.Count()));

  self->ResetDownstreamState();
  return NS_OK;
}

nsresult
Http2Session::RecvUnused1(Http2Session *self)
{
  MOZ_ASSERT(self->mInputFrameType == FRAME_TYPE_UNUSED1);
  LOG3(("Http2Session::RecvUnused1 %p NOP.", self));
  // Section 4.1 says to ignore this
  self->ResetDownstreamState();
  return NS_OK;
}

PLDHashOperator
Http2Session::RestartBlockedOnRwinEnumerator(nsAHttpTransaction *key,
                                             nsAutoPtr<Http2Stream> &stream,
                                             void *closure)
{
  Http2Session *self = static_cast<Http2Session *>(closure);
  MOZ_ASSERT(self->mServerSessionWindow > 0);

  if (!stream->BlockedOnRwin() || stream->ServerReceiveWindow() <= 0)
    return PL_DHASH_NEXT;

  self->mReadyForWrite.Push(stream);
  self->SetWriteCallbacks();
  return PL_DHASH_NEXT;
}

nsresult
Http2Session::RecvWindowUpdate(Http2Session *self)
{
  MOZ_ASSERT(self->mInputFrameType == FRAME_TYPE_WINDOW_UPDATE);

  if (self->mInputFrameDataSize != 4) {
    LOG3(("Http2Session::RecvWindowUpdate %p Window Update wrong length %d\n",
          self, self->mInputFrameDataSize));
    RETURN_SESSION_ERROR(self, PROTOCOL_ERROR);
  }

  uint32_t delta =
    PR_ntohl(reinterpret_cast<uint32_t *>(self->mInputFrameBuffer.get())[2]);
  delta &= 0x7fffffff;

  LOG3(("Http2Session::RecvWindowUpdate %p len=%d Stream 0x%X.\n",
        self, delta, self->mInputFrameID));

  // if flow control is disabled then reset the stream
  if (!self->mServerUsesFlowControl) {
    LOG3(("Http2Session::RecvWindowUpdate %p stream window update "
          "when peer had disabled flow control\n", self));
    RETURN_SESSION_ERROR(self, FLOW_CONTROL_ERROR);
    return NS_OK;
  }

  if (self->mInputFrameID) { // stream window
    nsresult rv = self->SetInputFrameDataStream(self->mInputFrameID);
    if (NS_FAILED(rv))
      return rv;

    if (!self->mInputFrameDataStream) {
      LOG3(("Http2Session::RecvWindowUpdate %p lookup streamID 0x%X failed.\n",
            self, self->mInputFrameID));
      // only resest the session if the ID is one we haven't ever opened
      if (self->mInputFrameID >= self->mNextStreamID)
        self->GenerateRstStream(PROTOCOL_ERROR, self->mInputFrameID);
      self->ResetDownstreamState();
      return NS_OK;
    }

    int64_t oldRemoteWindow = self->mInputFrameDataStream->ServerReceiveWindow();
    self->mInputFrameDataStream->UpdateServerReceiveWindow(delta);
    if (self->mInputFrameDataStream->ServerReceiveWindow() >= 0x80000000) {
      // a window cannot reach 2^31 and be in compliance. Our calculations
      // are 64 bit safe though.
      LOG3(("Http2Session::RecvWindowUpdate %p stream window "
            "exceeds 2^31 - 1\n", self));
      self->CleanupStream(self->mInputFrameDataStream, NS_ERROR_ILLEGAL_VALUE,
                          FLOW_CONTROL_ERROR);
      self->ResetDownstreamState();
      return NS_OK;
    }

    LOG3(("Http2Session::RecvWindowUpdate %p stream 0x%X window "
          "%d increased by %d now %d.\n", self, self->mInputFrameID,
          oldRemoteWindow, delta, oldRemoteWindow + delta));

  } else { // session window update
    int64_t oldRemoteWindow = self->mServerSessionWindow;
    self->mServerSessionWindow += delta;

    if (self->mServerSessionWindow >= 0x80000000) {
      // a window cannot reach 2^31 and be in compliance. Our calculations
      // are 64 bit safe though.
      LOG3(("Http2Session::RecvWindowUpdate %p session window "
            "exceeds 2^31 - 1\n", self));
      RETURN_SESSION_ERROR(self, FLOW_CONTROL_ERROR);
    }

    if ((oldRemoteWindow <= 0) && (self->mServerSessionWindow > 0)) {
      LOG3(("Http2Session::RecvWindowUpdate %p restart session window\n",
            self));
      self->mStreamTransactionHash.Enumerate(RestartBlockedOnRwinEnumerator, self);
    }
    LOG3(("Http2Session::RecvWindowUpdate %p session window "
          "%d increased by %d now %d.\n", self,
          oldRemoteWindow, delta, oldRemoteWindow + delta));
  }

  self->ResetDownstreamState();
  return NS_OK;
}

nsresult
Http2Session::RecvContinuation(Http2Session *self)
{
  MOZ_ASSERT(self->mInputFrameType == FRAME_TYPE_CONTINUATION);
  MOZ_ASSERT(self->mInputFrameID);
  MOZ_ASSERT(self->mExpectedPushPromiseID || self->mExpectedHeaderID);
  MOZ_ASSERT(!(self->mExpectedPushPromiseID && self->mExpectedHeaderID));

  LOG3(("Http2Session::RecvContinuation %p Flags 0x%X id 0x%X "
        "promise id 0x%X header id 0x%X\n",
        self, self->mInputFrameFlags, self->mInputFrameID,
        self->mExpectedPushPromiseID, self->mExpectedHeaderID));

  self->SetInputFrameDataStream(self->mInputFrameID);

  if (!self->mInputFrameDataStream) {
    LOG3(("Http2Session::RecvContination stream ID 0x%X not found.",
          self->mInputFrameID));
    RETURN_SESSION_ERROR(self, PROTOCOL_ERROR);
  }

  // continued headers
  if (self->mExpectedHeaderID) {
    self->mInputFrameFlags &= ~kFlag_PRIORITY;
    return RecvHeaders(self);
  }

  // continued push promise
  if (self->mInputFrameFlags & kFlag_END_HEADERS) {
    self->mInputFrameFlags &= ~kFlag_END_HEADERS;
    self->mInputFrameFlags |= kFlag_END_PUSH_PROMISE;
  }
  return RecvPushPromise(self);
}

//-----------------------------------------------------------------------------
// nsAHttpTransaction. It is expected that nsHttpConnection is the caller
// of these methods
//-----------------------------------------------------------------------------

void
Http2Session::OnTransportStatus(nsITransport* aTransport,
                                nsresult aStatus, uint64_t aProgress)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  switch (aStatus) {
    // These should appear only once, deliver to the first
    // transaction on the session.
  case NS_NET_STATUS_RESOLVING_HOST:
  case NS_NET_STATUS_RESOLVED_HOST:
  case NS_NET_STATUS_CONNECTING_TO:
  case NS_NET_STATUS_CONNECTED_TO:
  {
    Http2Stream *target = mStreamIDHash.Get(1);
    nsAHttpTransaction *transaction = target ? target->Transaction() : nullptr;
    if (transaction)
      transaction->OnTransportStatus(aTransport, aStatus, aProgress);
    break;
  }

  default:
    // The other transport events are ignored here because there is no good
    // way to map them to the right transaction in http/2. Instead, the events
    // are generated again from the http/2 code and passed directly to the
    // correct transaction.

    // NS_NET_STATUS_SENDING_TO:
    // This is generated by the socket transport when (part) of
    // a transaction is written out
    //
    // There is no good way to map it to the right transaction in http/2,
    // so it is ignored here and generated separately when the request
    // is sent from Http2Stream::TransmitFrame

    // NS_NET_STATUS_WAITING_FOR:
    // Created by nsHttpConnection when the request has been totally sent.
    // There is no good way to map it to the right transaction in http/2,
    // so it is ignored here and generated separately when the same
    // condition is complete in Http2Stream when there is no more
    // request body left to be transmitted.

    // NS_NET_STATUS_RECEIVING_FROM
    // Generated in session whenever we read a data frame or a HEADERS
    // that can be attributed to a particular stream/transaction

    break;
  }
}

// ReadSegments() is used to write data to the network. Generally, HTTP
// request data is pulled from the approriate transaction and
// converted to http/2 data. Sometimes control data like window-update are
// generated instead.

nsresult
Http2Session::ReadSegments(nsAHttpSegmentReader *reader,
                           uint32_t count, uint32_t *countRead)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  MOZ_ASSERT(!mSegmentReader || !reader || (mSegmentReader == reader),
             "Inconsistent Write Function Callback");

  nsresult rv = ConfirmTLSProfile();
  if (NS_FAILED(rv))
    return rv;

  if (reader)
    mSegmentReader = reader;

  *countRead = 0;

  LOG3(("Http2Session::ReadSegments %p", this));

  Http2Stream *stream = static_cast<Http2Stream *>(mReadyForWrite.PopFront());
  if (!stream) {
    LOG3(("Http2Session %p could not identify a stream to write; suspending.",
          this));
    FlushOutputQueue();
    SetWriteCallbacks();
    return NS_BASE_STREAM_WOULD_BLOCK;
  }

  LOG3(("Http2Session %p will write from Http2Stream %p 0x%X "
        "block-input=%d block-output=%d\n", this, stream, stream->StreamID(),
        stream->RequestBlockedOnRead(), stream->BlockedOnRwin()));

  rv = stream->ReadSegments(this, count, countRead);

  // Not every permutation of stream->ReadSegents produces data (and therefore
  // tries to flush the output queue) - SENDING_FIN_STREAM can be an example
  // of that. But we might still have old data buffered that would be good
  // to flush.
  FlushOutputQueue();

  // Allow new server reads - that might be data or control information
  // (e.g. window updates or http replies) that are responses to these writes
  ResumeRecv();

  if (stream->RequestBlockedOnRead()) {

    // We are blocked waiting for input - either more http headers or
    // any request body data. When more data from the request stream
    // becomes available the httptransaction will call conn->ResumeSend().

    LOG3(("Http2Session::ReadSegments %p dealing with block on read", this));

    // call readsegments again if there are other streams ready
    // to run in this session
    if (GetWriteQueueSize()) {
      rv = NS_OK;
    } else {
      rv = NS_BASE_STREAM_WOULD_BLOCK;
    }
    SetWriteCallbacks();
    return rv;
  }

  if (NS_FAILED(rv)) {
    LOG3(("Http2Session::ReadSegments %p returning FAIL code %X",
          this, rv));
    if (rv != NS_BASE_STREAM_WOULD_BLOCK)
      CleanupStream(stream, rv, CANCEL_ERROR);
    return rv;
  }

  if (*countRead > 0) {
    LOG3(("Http2Session::ReadSegments %p stream=%p countread=%d",
          this, stream, *countRead));
    mReadyForWrite.Push(stream);
    SetWriteCallbacks();
    return rv;
  }

  if (stream->BlockedOnRwin()) {
    LOG3(("Http2Session %p will stream %p 0x%X suspended for flow control\n",
          this, stream, stream->StreamID()));
    return NS_BASE_STREAM_WOULD_BLOCK;
  }

  LOG3(("Http2Session::ReadSegments %p stream=%p stream send complete",
        this, stream));

  // call readsegments again if there are other streams ready
  // to go in this session
  SetWriteCallbacks();

  return rv;
}

// WriteSegments() is used to read data off the socket. Generally this is
// just the http2 frame header and from there the appropriate *Stream
// is identified from the Stream-ID. The http transaction associated with
// that read then pulls in the data directly, which it will feed to
// OnWriteSegment(). That function will gateway it into http and feed
// it to the appropriate transaction.

// we call writer->OnWriteSegment via NetworkRead() to get a http2 header..
// and decide if it is data or control.. if it is control, just deal with it.
// if it is data, identify the stream
// call stream->WriteSegments which can call this::OnWriteSegment to get the
// data. It always gets full frames if they are part of the stream

nsresult
Http2Session::WriteSegments(nsAHttpSegmentWriter *writer,
                            uint32_t count, uint32_t *countWritten)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  LOG3(("Http2Session::WriteSegments %p InternalState %X\n",
        this, mDownstreamState));

  *countWritten = 0;

  if (mClosed)
    return NS_ERROR_FAILURE;

  nsresult rv = ConfirmTLSProfile();
  if (NS_FAILED(rv))
    return rv;

  SetWriteCallbacks();

  // If there are http transactions attached to a push stream with filled buffers
  // trigger that data pump here. This only reads from buffers (not the network)
  // so mDownstreamState doesn't matter.
  Http2Stream *pushConnectedStream =
    static_cast<Http2Stream *>(mReadyForRead.PopFront());
  if (pushConnectedStream) {
    LOG3(("Http2Session::WriteSegments %p processing pushed stream 0x%X\n",
          this, pushConnectedStream->StreamID()));
    mSegmentWriter = writer;
    rv = pushConnectedStream->WriteSegments(this, count, countWritten);
    mSegmentWriter = nullptr;

    // The pipe in nsHttpTransaction rewrites CLOSED error codes into OK
    // so we need this check to determine the truth.
    if (NS_SUCCEEDED(rv) && !*countWritten &&
        pushConnectedStream->PushSource() &&
        pushConnectedStream->PushSource()->GetPushComplete()) {
      rv = NS_BASE_STREAM_CLOSED;
    }

    if (rv == NS_BASE_STREAM_CLOSED) {
      CleanupStream(pushConnectedStream, NS_OK, CANCEL_ERROR);
      rv = NS_OK;
    }

    // if we return OK to nsHttpConnection it will use mSocketInCondition
    // to determine whether to schedule more reads, incorrectly
    // assuming that nsHttpConnection::OnSocketWrite() was called.
    if (NS_SUCCEEDED(rv) || rv == NS_BASE_STREAM_WOULD_BLOCK) {
      rv = NS_BASE_STREAM_WOULD_BLOCK;
      ResumeRecv();
    }

    return rv;
  }

  // The BUFFERING_OPENING_SETTINGS state is just like any BUFFERING_FRAME_HEADER
  // except the only frame type it will allow is SETTINGS

  // The session layer buffers the leading 8 byte header of every frame.
  // Non-Data frames are then buffered for their full length, but data
  // frames (type 0) are passed through to the http stack unprocessed

  if (mDownstreamState == BUFFERING_OPENING_SETTINGS ||
      mDownstreamState == BUFFERING_FRAME_HEADER) {
    // The first 8 bytes of every frame is header information that
    // we are going to want to strip before passing to http. That is
    // true of both control and data packets.

    MOZ_ASSERT(mInputFrameBufferUsed < 8,
               "Frame Buffer Used Too Large for State");

    rv = NetworkRead(writer, mInputFrameBuffer + mInputFrameBufferUsed,
                     8 - mInputFrameBufferUsed, countWritten);

    if (NS_FAILED(rv)) {
      LOG3(("Http2Session %p buffering frame header read failure %x\n",
            this, rv));
      // maybe just blocked reading from network
      if (rv == NS_BASE_STREAM_WOULD_BLOCK)
        rv = NS_OK;
      return rv;
    }

    LogIO(this, nullptr, "Reading Frame Header",
          mInputFrameBuffer + mInputFrameBufferUsed, *countWritten);

    mInputFrameBufferUsed += *countWritten;

    if (mInputFrameBufferUsed < 8)
    {
      LOG3(("Http2Session::WriteSegments %p "
            "BUFFERING FRAME HEADER incomplete size=%d",
            this, mInputFrameBufferUsed));
      return rv;
    }

    // 2 bytes of length, 1 type byte, 1 flag byte, 1 unused bit, 31 bits of ID
    mInputFrameDataSize =
      PR_ntohs(reinterpret_cast<uint16_t *>(mInputFrameBuffer.get())[0]);
    mInputFrameType = reinterpret_cast<uint8_t *>(mInputFrameBuffer.get())[2];
    mInputFrameFlags = reinterpret_cast<uint8_t *>(mInputFrameBuffer.get())[3];
    mInputFrameID =
      PR_ntohl(reinterpret_cast<uint32_t *>(mInputFrameBuffer.get())[1]);
    mInputFrameID &= 0x7fffffff;
    mInputFrameDataRead = 0;

    if (mInputFrameType == FRAME_TYPE_DATA || mInputFrameType == FRAME_TYPE_HEADERS)  {
      mInputFrameFinal = mInputFrameFlags & kFlag_END_STREAM;
    } else {
      mInputFrameFinal = 0;
    }

    if (mInputFrameDataSize >= 0x4000) {
      // Section 9.1 HTTP frames cannot exceed 2^14 - 1 but receviers must ignore
      // those bits
      LOG3(("Http2Session::WriteSegments %p WARNING Frame Length bits past 14 are not 0 %08X\n",
            this, mInputFrameDataSize));
      mInputFrameDataSize &= 0x3fff;
    }

    LOG3(("Http2Session::WriteSegments[%p::%x] Frame Header Read "
          "type %X data len %u flags %x id 0x%X",
          this, mSerial, mInputFrameType, mInputFrameDataSize, mInputFrameFlags,
          mInputFrameID));

    // if mExpectedHeaderID is non 0, it means this frame must be a CONTINUATION of
    // a HEADERS frame with a matching ID (section 6.2)
    if (mExpectedHeaderID &&
        ((mInputFrameType != FRAME_TYPE_CONTINUATION) ||
         (mExpectedHeaderID != mInputFrameID))) {
      LOG3(("Expected CONINUATION OF HEADERS for ID 0x%X\n", mExpectedHeaderID));
      RETURN_SESSION_ERROR(this, PROTOCOL_ERROR);
    }

    // if mExpectedPushPromiseID is non 0, it means this frame must be a
    // CONTINUATION of a PUSH_PROMISE with a matching ID (section 6.2)
    if (mExpectedPushPromiseID &&
        ((mInputFrameType != FRAME_TYPE_CONTINUATION) ||
         (mExpectedPushPromiseID != mInputFrameID))) {
      LOG3(("Expected CONTINUATION of PUSH PROMISE for ID 0x%X\n",
            mExpectedPushPromiseID));
      RETURN_SESSION_ERROR(this, PROTOCOL_ERROR);
    }

    if (mDownstreamState == BUFFERING_OPENING_SETTINGS &&
        mInputFrameType != FRAME_TYPE_SETTINGS) {
      LOG3(("First Frame Type Must Be Settings\n"));
      RETURN_SESSION_ERROR(this, PROTOCOL_ERROR);
    }

    if (mInputFrameType != FRAME_TYPE_DATA) { // control frame
      EnsureBuffer(mInputFrameBuffer, mInputFrameDataSize + 8, 8,
                   mInputFrameBufferSize);
      ChangeDownstreamState(BUFFERING_CONTROL_FRAME);
    } else {
      ChangeDownstreamState(PROCESSING_DATA_FRAME);

      Telemetry::Accumulate(Telemetry::SPDY_CHUNK_RECVD,
                            mInputFrameDataSize >> 10);
      mLastDataReadEpoch = mLastReadEpoch;

      if (!mInputFrameID) {
        LOG3(("Http2Session::WriteSegments %p data frame stream 0\n", this));
        RETURN_SESSION_ERROR(this, PROTOCOL_ERROR);
      }

      rv = SetInputFrameDataStream(mInputFrameID);
      if (NS_FAILED(rv)) {
        LOG3(("Http2Session::WriteSegments %p lookup streamID 0x%X failed. "
              "probably due to verification.\n", this, mInputFrameID));
        return rv;
      }
      if (!mInputFrameDataStream) {
        LOG3(("Http2Session::WriteSegments %p lookup streamID 0x%X failed. "
              "Next = 0x%X", this, mInputFrameID, mNextStreamID));
        if (mInputFrameID >= mNextStreamID)
          GenerateRstStream(PROTOCOL_ERROR, mInputFrameID);
        ChangeDownstreamState(DISCARDING_DATA_FRAME);
      } else if (mInputFrameDataStream->RecvdFin() ||
               mInputFrameDataStream->RecvdReset() ||
               mInputFrameDataStream->SentReset()) {
        LOG3(("Http2Session::WriteSegments %p streamID 0x%X "
              "Data arrived for already server closed stream.\n",
              this, mInputFrameID));
        if (mInputFrameDataStream->RecvdFin() || mInputFrameDataStream->RecvdReset())
          GenerateRstStream(STREAM_CLOSED_ERROR, mInputFrameID);
        ChangeDownstreamState(DISCARDING_DATA_FRAME);
      }

      LOG3(("Start Processing Data Frame. "
            "Session=%p Stream ID 0x%X Stream Ptr %p Fin=%d Len=%d",
            this, mInputFrameID, mInputFrameDataStream, mInputFrameFinal,
            mInputFrameDataSize));
      UpdateLocalRwin(mInputFrameDataStream, mInputFrameDataSize);
    }
  }

  if (mDownstreamState == PROCESSING_CONTROL_RST_STREAM) {
    nsresult streamCleanupCode;

    // There is no bounds checking on the error code.. we provide special
    // handling for a couple of cases and all others (including unknown) are
    // equivalent to cancel.
    if (mDownstreamRstReason == REFUSED_STREAM_ERROR) {
      streamCleanupCode = NS_ERROR_NET_RESET;      // can retry this 100% safely
    } else {
      streamCleanupCode = NS_ERROR_NET_INTERRUPT;
    }

    if (mDownstreamRstReason == COMPRESSION_ERROR)
      mShouldGoAway = true;

    // mInputFrameDataStream is reset by ChangeDownstreamState
    Http2Stream *stream = mInputFrameDataStream;
    ResetDownstreamState();
    LOG3(("Http2Session::WriteSegments cleanup stream on recv of rst "
          "session=%p stream=%p 0x%X\n", this, stream,
          stream ? stream->StreamID() : 0));
    CleanupStream(stream, streamCleanupCode, CANCEL_ERROR);
    return NS_OK;
  }

  if (mDownstreamState == PROCESSING_DATA_FRAME ||
      mDownstreamState == PROCESSING_COMPLETE_HEADERS) {

    // The cleanup stream should only be set while stream->WriteSegments is
    // on the stack and then cleaned up in this code block afterwards.
    MOZ_ASSERT(!mNeedsCleanup, "cleanup stream set unexpectedly");
    mNeedsCleanup = nullptr;                     /* just in case */

    mSegmentWriter = writer;
    rv = mInputFrameDataStream->WriteSegments(this, count, countWritten);
    mSegmentWriter = nullptr;

    mLastDataReadEpoch = mLastReadEpoch;

    if (SoftStreamError(rv)) {
      // This will happen when the transaction figures out it is EOF, generally
      // due to a content-length match being made. Return OK from this function
      // otherwise the whole session would be torn down.
      Http2Stream *stream = mInputFrameDataStream;

      // if we were doing PROCESSING_COMPLETE_HEADERS need to pop the state
      // back to PROCESSING_DATA_FRAME where we came from
      mDownstreamState = PROCESSING_DATA_FRAME;

      if (mInputFrameDataRead == mInputFrameDataSize)
        ResetDownstreamState();
      LOG3(("Http2Session::WriteSegments session=%p stream=%p 0x%X "
            "needscleanup=%p. cleanup stream based on "
            "stream->writeSegments returning code %x\n",
            this, stream, stream ? stream->StreamID() : 0,
            mNeedsCleanup, rv));
      CleanupStream(stream, NS_OK, CANCEL_ERROR);
      MOZ_ASSERT(!mNeedsCleanup, "double cleanup out of data frame");
      mNeedsCleanup = nullptr;                     /* just in case */
      return NS_OK;
    }

    if (mNeedsCleanup) {
      LOG3(("Http2Session::WriteSegments session=%p stream=%p 0x%X "
            "cleanup stream based on mNeedsCleanup.\n",
            this, mNeedsCleanup, mNeedsCleanup ? mNeedsCleanup->StreamID() : 0));
      CleanupStream(mNeedsCleanup, NS_OK, CANCEL_ERROR);
      mNeedsCleanup = nullptr;
    }

    if (NS_FAILED(rv)) {
      LOG3(("Http2Session %p data frame read failure %x\n", this, rv));
      // maybe just blocked reading from network
      if (rv == NS_BASE_STREAM_WOULD_BLOCK)
        rv = NS_OK;
    }

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
      LOG3(("Http2Session %p discard frame read failure %x\n", this, rv));
      // maybe just blocked reading from network
      if (rv == NS_BASE_STREAM_WOULD_BLOCK)
        rv = NS_OK;
      return rv;
    }

    LogIO(this, nullptr, "Discarding Frame", trash, *countWritten);

    mInputFrameDataRead += *countWritten;

    if (mInputFrameDataRead == mInputFrameDataSize)
      ResetDownstreamState();
    return rv;
  }

  if (mDownstreamState != BUFFERING_CONTROL_FRAME) {
    MOZ_ASSERT(false); // this cannot happen
    return NS_ERROR_UNEXPECTED;
  }

  MOZ_ASSERT(mInputFrameBufferUsed == 8, "Frame Buffer Header Not Present");
  MOZ_ASSERT(mInputFrameDataSize + 8 <= mInputFrameBufferSize,
             "allocation for control frame insufficient");

  rv = NetworkRead(writer, mInputFrameBuffer + 8 + mInputFrameDataRead,
                   mInputFrameDataSize - mInputFrameDataRead, countWritten);

  if (NS_FAILED(rv)) {
    LOG3(("Http2Session %p buffering control frame read failure %x\n",
          this, rv));
    // maybe just blocked reading from network
    if (rv == NS_BASE_STREAM_WOULD_BLOCK)
      rv = NS_OK;
    return rv;
  }

  LogIO(this, nullptr, "Reading Control Frame",
        mInputFrameBuffer + 8 + mInputFrameDataRead, *countWritten);

  mInputFrameDataRead += *countWritten;

  if (mInputFrameDataRead != mInputFrameDataSize)
    return NS_OK;

  MOZ_ASSERT(mInputFrameType != FRAME_TYPE_DATA);
  if (mInputFrameType < FRAME_TYPE_LAST) {
    rv = sControlFunctions[mInputFrameType](this);
  } else {
    // Section 4.1 requires this to be ignored; though protocol_error would
    // be better
    LOG3(("Http2Session %p unknow frame type %x ignored\n",
          this, mInputFrameType));
    ResetDownstreamState();
    rv = NS_OK;
  }

  MOZ_ASSERT(NS_FAILED(rv) ||
             mDownstreamState != BUFFERING_CONTROL_FRAME,
             "Control Handler returned OK but did not change state");

  if (mShouldGoAway && !mStreamTransactionHash.Count())
    Close(NS_OK);
  return rv;
}

void
Http2Session::UpdateLocalStreamWindow(Http2Stream *stream, uint32_t bytes)
{
  if (!stream) // this is ok - it means there was a data frame for a rst stream
    return;

  // If this data packet was not for a valid or live stream then there
  // is no reason to mess with the flow control
  if (!stream || stream->RecvdFin() || stream->RecvdReset() ||
      mInputFrameFinal) {
    return;
  }

  stream->DecrementClientReceiveWindow(bytes);

  // Don't necessarily ack every data packet. Only do it
  // after a significant amount of data.
  uint64_t unacked = stream->LocalUnAcked();
  int64_t  localWindow = stream->ClientReceiveWindow();

  LOG3(("Http2Session::UpdateLocalStreamWindow this=%p id=0x%X newbytes=%u "
        "unacked=%llu localWindow=%lld\n",
        this, stream->StreamID(), bytes, unacked, localWindow));

  if (!unacked)
    return;

  if ((unacked < kMinimumToAck) && (localWindow > kEmergencyWindowThreshold))
    return;

  if (!stream->HasSink()) {
    LOG3(("Http2Session::UpdateLocalStreamWindow %p 0x%X Pushed Stream Has No Sink\n",
          this, stream->StreamID()));
    return;
  }

  // Generate window updates directly out of session instead of the stream
  // in order to avoid queue delays in getting the 'ACK' out.
  uint32_t toack = (unacked <= 0x7fffffffU) ? unacked : 0x7fffffffU;

  LOG3(("Http2Session::UpdateLocalStreamWindow Ack this=%p id=0x%X acksize=%d\n",
        this, stream->StreamID(), toack));
  stream->IncrementClientReceiveWindow(toack);

  // room for this packet needs to be ensured before calling this function
  char *packet = mOutputQueueBuffer.get() + mOutputQueueUsed;
  mOutputQueueUsed += 12;
  MOZ_ASSERT(mOutputQueueUsed <= mOutputQueueSize);

  CreateFrameHeader(packet, 4, FRAME_TYPE_WINDOW_UPDATE, 0, stream->StreamID());
  toack = PR_htonl(toack);
  memcpy(packet + 8, &toack, 4);

  LogIO(this, stream, "Stream Window Update", packet, 12);
  // dont flush here, this write can commonly be coalesced with a
  // session window update to immediately follow.
}

void
Http2Session::UpdateLocalSessionWindow(uint32_t bytes)
{
  if (!bytes)
    return;

  mLocalSessionWindow -= bytes;

  LOG3(("Http2Session::UpdateLocalSessionWindow this=%p newbytes=%u "
        "localWindow=%lld\n", this, bytes, mLocalSessionWindow));

  // Don't necessarily ack every data packet. Only do it
  // after a significant amount of data.
  if ((mLocalSessionWindow > (ASpdySession::kInitialRwin - kMinimumToAck)) &&
      (mLocalSessionWindow > kEmergencyWindowThreshold))
    return;

  // Only send max  bits of window updates at a time.
  uint64_t toack64 = ASpdySession::kInitialRwin - mLocalSessionWindow;
  uint32_t toack = (toack64 <= 0x7fffffffU) ? toack64 : 0x7fffffffU;

  LOG3(("Http2Session::UpdateLocalSessionWindow Ack this=%p acksize=%u\n",
        this, toack));
  mLocalSessionWindow += toack;

  // room for this packet needs to be ensured before calling this function
  char *packet = mOutputQueueBuffer.get() + mOutputQueueUsed;
  mOutputQueueUsed += 12;
  MOZ_ASSERT(mOutputQueueUsed <= mOutputQueueSize);

  CreateFrameHeader(packet, 4, FRAME_TYPE_WINDOW_UPDATE, 0, 0);
  toack = PR_htonl(toack);
  memcpy(packet + 8, &toack, 4);

  LogIO(this, nullptr, "Session Window Update", packet, 12);
  // dont flush here, this write can commonly be coalesced with others
}

void
Http2Session::UpdateLocalRwin(Http2Stream *stream, uint32_t bytes)
{
  // make sure there is room for 2 window updates even though
  // we may not generate any.
  EnsureOutputBuffer(16 * 2);

  UpdateLocalStreamWindow(stream, bytes);
  UpdateLocalSessionWindow(bytes);
  FlushOutputQueue();
}

void
Http2Session::Close(nsresult aReason)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  if (mClosed)
    return;

  LOG3(("Http2Session::Close %p %X", this, aReason));

  mClosed = true;

  mStreamTransactionHash.Enumerate(ShutdownEnumerator, this);
  mStreamIDHash.Clear();
  mStreamTransactionHash.Clear();

  uint32_t goAwayReason;
  if (mGoAwayReason != NO_HTTP_ERROR) {
    goAwayReason = mGoAwayReason;
  } else if (NS_SUCCEEDED(aReason)) {
    goAwayReason = NO_HTTP_ERROR;
  } else if (aReason == NS_ERROR_ILLEGAL_VALUE) {
    goAwayReason = PROTOCOL_ERROR;
  } else {
    goAwayReason = INTERNAL_ERROR;
  }
  GenerateGoAway(goAwayReason);
  mConnection = nullptr;
  mSegmentReader = nullptr;
  mSegmentWriter = nullptr;
}

void
Http2Session::CloseTransaction(nsAHttpTransaction *aTransaction,
                               nsresult aResult)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  LOG3(("Http2Session::CloseTransaction %p %p %x", this, aTransaction, aResult));

  // Generally this arrives as a cancel event from the connection manager.

  // need to find the stream and call CleanupStream() on it.
  Http2Stream *stream = mStreamTransactionHash.Get(aTransaction);
  if (!stream) {
    LOG3(("Http2Session::CloseTransaction %p %p %x - not found.",
          this, aTransaction, aResult));
    return;
  }
  LOG3(("Http2Session::CloseTranscation probably a cancel. "
        "this=%p, trans=%p, result=%x, streamID=0x%X stream=%p",
        this, aTransaction, aResult, stream->StreamID(), stream));
  CleanupStream(stream, aResult, CANCEL_ERROR);
  ResumeRecv();
}

//-----------------------------------------------------------------------------
// nsAHttpSegmentReader
//-----------------------------------------------------------------------------

nsresult
Http2Session::OnReadSegment(const char *buf,
                            uint32_t count, uint32_t *countRead)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  nsresult rv;

  // If we can release old queued data then we can try and write the new
  // data directly to the network without using the output queue at all
  if (mOutputQueueUsed)
    FlushOutputQueue();

  if (!mOutputQueueUsed && mSegmentReader) {
    // try and write directly without output queue
    rv = mSegmentReader->OnReadSegment(buf, count, countRead);

    if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
      *countRead = 0;
    } else if (NS_FAILED(rv)) {
      return rv;
    }

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
Http2Session::CommitToSegmentSize(uint32_t count, bool forceCommitment)
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
  EnsureOutputBuffer(count + kQueueReserved);

  MOZ_ASSERT((mOutputQueueUsed + count) <= (mOutputQueueSize - kQueueReserved),
             "buffer not as large as expected");

  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsAHttpSegmentWriter
//-----------------------------------------------------------------------------

nsresult
Http2Session::OnWriteSegment(char *buf,
                             uint32_t count, uint32_t *countWritten)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  nsresult rv;

  if (!mSegmentWriter) {
    // the only way this could happen would be if Close() were called on the
    // stack with WriteSegments()
    return NS_ERROR_FAILURE;
  }

  if (mDownstreamState == PROCESSING_DATA_FRAME) {

    if (mInputFrameFinal &&
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
    if ((mInputFrameDataRead == mInputFrameDataSize) && !mInputFrameFinal)
      ResetDownstreamState();

    return rv;
  }

  if (mDownstreamState == PROCESSING_COMPLETE_HEADERS) {

    if (mFlatHTTPResponseHeaders.Length() == mFlatHTTPResponseHeadersOut &&
        mInputFrameFinal) {
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

    if (mFlatHTTPResponseHeaders.Length() == mFlatHTTPResponseHeadersOut) {
      if (!mInputFrameFinal) {
        // If more frames are expected in this stream, then reset the state so they can be
        // handled. Otherwise (e.g. a 0 length response with the fin on the incoming headers)
        // stay in PROCESSING_COMPLETE_HEADERS state so the SetNeedsCleanup() code above can
        // cleanup the stream.
        ResetDownstreamState();
      }
    }

    return NS_OK;
  }

  return NS_ERROR_UNEXPECTED;
}

void
Http2Session::SetNeedsCleanup()
{
  LOG3(("Http2Session::SetNeedsCleanup %p - recorded downstream fin of "
        "stream %p 0x%X", this, mInputFrameDataStream,
        mInputFrameDataStream->StreamID()));

  // This will result in Close() being called
  MOZ_ASSERT(!mNeedsCleanup, "mNeedsCleanup unexpectedly set");
  mNeedsCleanup = mInputFrameDataStream;
  ResetDownstreamState();
}

void
Http2Session::ConnectPushedStream(Http2Stream *stream)
{
  mReadyForRead.Push(stream);
  ForceRecv();
}

nsresult
Http2Session::BufferOutput(const char *buf,
                           uint32_t count,
                           uint32_t *countRead)
{
  nsAHttpSegmentReader *old = mSegmentReader;
  mSegmentReader = nullptr;
  nsresult rv = OnReadSegment(buf, count, countRead);
  mSegmentReader = old;
  return rv;
}

nsresult
Http2Session::ConfirmTLSProfile()
{
  if (mTLSProfileConfirmed)
    return NS_OK;

  LOG3(("Http2Session::ConfirmTLSProfile %p mConnection=%p\n",
        this, mConnection.get()));

  if (!gHttpHandler->EnforceHttp2TlsProfile()) {
    LOG3(("Http2Session::ConfirmTLSProfile %p passed due to configuration bypass\n", this));
    mTLSProfileConfirmed = true;
    return NS_OK;
  }

  if (!mConnection)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsISupports> securityInfo;
  mConnection->GetSecurityInfo(getter_AddRefs(securityInfo));
  nsCOMPtr<nsISSLSocketControl> ssl = do_QueryInterface(securityInfo);
  LOG3(("Http2Session::ConfirmTLSProfile %p sslsocketcontrol=%p\n", this, ssl.get()));
  if (!ssl)
    return NS_ERROR_FAILURE;

  int16_t version = ssl->GetSSLVersionUsed();
  LOG3(("Http2Session::ConfirmTLSProfile %p version=%x\n", this, version));
  if (version < nsISSLSocketControl::TLS_VERSION_1_1) {
    LOG3(("Http2Session::ConfirmTLSProfile %p FAILED due to lack of TLS1.1\n", this));
    RETURN_SESSION_ERROR(this, PROTOCOL_ERROR);
  }

  mTLSProfileConfirmed = true;
  return NS_OK;
}


//-----------------------------------------------------------------------------
// Modified methods of nsAHttpConnection
//-----------------------------------------------------------------------------

void
Http2Session::TransactionHasDataToWrite(nsAHttpTransaction *caller)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  LOG3(("Http2Session::TransactionHasDataToWrite %p trans=%p", this, caller));

  // a trapped signal from the http transaction to the connection that
  // it is no longer blocked on read.

  Http2Stream *stream = mStreamTransactionHash.Get(caller);
  if (!stream || !VerifyStream(stream)) {
    LOG3(("Http2Session::TransactionHasDataToWrite %p caller %p not found",
          this, caller));
    return;
  }

  LOG3(("Http2Session::TransactionHasDataToWrite %p ID is 0x%X\n",
        this, stream->StreamID()));

  mReadyForWrite.Push(stream);
}

void
Http2Session::TransactionHasDataToWrite(Http2Stream *stream)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  LOG3(("Http2Session::TransactionHasDataToWrite %p stream=%p ID=0x%x",
        this, stream, stream->StreamID()));

  mReadyForWrite.Push(stream);
  SetWriteCallbacks();
}

bool
Http2Session::IsPersistent()
{
  return true;
}

nsresult
Http2Session::TakeTransport(nsISocketTransport **,
                            nsIAsyncInputStream **, nsIAsyncOutputStream **)
{
  MOZ_ASSERT(false, "TakeTransport of Http2Session");
  return NS_ERROR_UNEXPECTED;
}

nsHttpConnection *
Http2Session::TakeHttpConnection()
{
  MOZ_ASSERT(false, "TakeHttpConnection of Http2Session");
  return nullptr;
}

uint32_t
Http2Session::CancelPipeline(nsresult reason)
{
  // we don't pipeline inside http/2, so this isn't an issue
  return 0;
}

nsAHttpTransaction::Classifier
Http2Session::Classification()
{
  if (!mConnection)
    return nsAHttpTransaction::CLASS_GENERAL;
  return mConnection->Classification();
}

//-----------------------------------------------------------------------------
// unused methods of nsAHttpTransaction
// We can be sure of this because Http2Session is only constructed in
// nsHttpConnection and is never passed out of that object
//-----------------------------------------------------------------------------

void
Http2Session::SetConnection(nsAHttpConnection *)
{
  // This is unexpected
  MOZ_ASSERT(false, "Http2Session::SetConnection()");
}

void
Http2Session::GetSecurityCallbacks(nsIInterfaceRequestor **)
{
  // This is unexpected
  MOZ_ASSERT(false, "Http2Session::GetSecurityCallbacks()");
}

void
Http2Session::SetProxyConnectFailed()
{
  MOZ_ASSERT(false, "Http2Session::SetProxyConnectFailed()");
}

bool
Http2Session::IsDone()
{
  return !mStreamTransactionHash.Count();
}

nsresult
Http2Session::Status()
{
  MOZ_ASSERT(false, "Http2Session::Status()");
  return NS_ERROR_UNEXPECTED;
}

uint32_t
Http2Session::Caps()
{
  MOZ_ASSERT(false, "Http2Session::Caps()");
  return 0;
}

void
Http2Session::SetDNSWasRefreshed()
{
  MOZ_ASSERT(false, "Http2Session::SetDNSWasRefreshed()");
}

uint64_t
Http2Session::Available()
{
  MOZ_ASSERT(false, "Http2Session::Available()");
  return 0;
}

nsHttpRequestHead *
Http2Session::RequestHead()
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  MOZ_ASSERT(false,
             "Http2Session::RequestHead() "
             "should not be called after http/2 is setup");
  return NULL;
}

uint32_t
Http2Session::Http1xTransactionCount()
{
  return 0;
}

// used as an enumerator by TakeSubTransactions()
static PLDHashOperator
  TakeStream(nsAHttpTransaction *key,
             nsAutoPtr<Http2Stream> &stream,
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
Http2Session::TakeSubTransactions(
  nsTArray<nsRefPtr<nsAHttpTransaction> > &outTransactions)
{
  // Generally this cannot be done with http/2 as transactions are
  // started right away.

  LOG3(("Http2Session::TakeSubTransactions %p\n", this));

  if (mConcurrentHighWater > 0)
    return NS_ERROR_ALREADY_OPENED;

  LOG3(("   taking %d\n", mStreamTransactionHash.Count()));

  mStreamTransactionHash.Enumerate(TakeStream, &outTransactions);
  return NS_OK;
}

nsresult
Http2Session::AddTransaction(nsAHttpTransaction *)
{
  // This API is meant for pipelining, Http2Session's should be
  // extended with AddStream()

  MOZ_ASSERT(false,
             "Http2Session::AddTransaction() should not be called");

  return NS_ERROR_NOT_IMPLEMENTED;
}

uint32_t
Http2Session::PipelineDepth()
{
  return IsDone() ? 0 : 1;
}

nsresult
Http2Session::SetPipelinePosition(int32_t position)
{
  // This API is meant for pipelining, Http2Session's should be
  // extended with AddStream()

  MOZ_ASSERT(false,
             "Http2Session::SetPipelinePosition() should not be called");

  return NS_ERROR_NOT_IMPLEMENTED;
}

int32_t
Http2Session::PipelinePosition()
{
  return 0;
}

//-----------------------------------------------------------------------------
// Pass through methods of nsAHttpConnection
//-----------------------------------------------------------------------------

nsAHttpConnection *
Http2Session::Connection()
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  return mConnection;
}

nsresult
Http2Session::OnHeadersAvailable(nsAHttpTransaction *transaction,
                                 nsHttpRequestHead *requestHead,
                                 nsHttpResponseHead *responseHead, bool *reset)
{
  return mConnection->OnHeadersAvailable(transaction,
                                         requestHead,
                                         responseHead,
                                         reset);
}

bool
Http2Session::IsReused()
{
  return mConnection->IsReused();
}

nsresult
Http2Session::PushBack(const char *buf, uint32_t len)
{
  return mConnection->PushBack(buf, len);
}

} // namespace mozilla::net
} // namespace mozilla
