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

#include "mozilla/EndianUtils.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Preferences.h"
#include "nsHttp.h"
#include "nsHttpHandler.h"
#include "nsHttpConnection.h"
#include "nsIRequestContext.h"
#include "nsISSLSocketControl.h"
#include "nsISSLStatus.h"
#include "nsISSLStatusProvider.h"
#include "nsISupportsPriority.h"
#include "nsStandardURL.h"
#include "nsURLHelper.h"
#include "prnetdb.h"
#include "sslt.h"
#include "mozilla/Sprintf.h"
#include "nsSocketTransportService2.h"
#include "nsNetUtil.h"
#include "nsICacheEntry.h"
#include "nsICacheStorageService.h"
#include "nsICacheStorage.h"
#include "CacheControlParser.h"
#include "LoadContextInfo.h"

namespace mozilla {
namespace net {

// Http2Session has multiple inheritance of things that implement nsISupports
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

Http2Session::Http2Session(nsISocketTransport *aSocketTransport, uint32_t version, bool attemptingEarlyData)
  : mSocketTransport(aSocketTransport)
  , mSegmentReader(nullptr)
  , mSegmentWriter(nullptr)
  , mNextStreamID(3) // 1 is reserved for Updgrade handshakes
  , mLastPushedID(0)
  , mConcurrentHighWater(0)
  , mDownstreamState(BUFFERING_OPENING_SETTINGS)
  , mInputFrameBufferSize(kDefaultBufferSize)
  , mInputFrameBufferUsed(0)
  , mInputFrameDataSize(0)
  , mInputFrameDataRead(0)
  , mInputFrameFinal(false)
  , mInputFrameType(0)
  , mInputFrameFlags(0)
  , mInputFrameID(0)
  , mPaddingLength(0)
  , mInputFrameDataStream(nullptr)
  , mNeedsCleanup(nullptr)
  , mDownstreamRstReason(NO_HTTP_ERROR)
  , mExpectedHeaderID(0)
  , mExpectedPushPromiseID(0)
  , mContinuedPromiseStream(0)
  , mFlatHTTPResponseHeadersOut(0)
  , mShouldGoAway(false)
  , mClosed(false)
  , mCleanShutdown(false)
  , mReceivedSettings(false)
  , mTLSProfileConfirmed(false)
  , mGoAwayReason(NO_HTTP_ERROR)
  , mClientGoAwayReason(UNASSIGNED)
  , mPeerGoAwayReason(UNASSIGNED)
  , mGoAwayID(0)
  , mOutgoingGoAwayID(0)
  , mConcurrent(0)
  , mServerPushedResources(0)
  , mServerInitialStreamWindow(kDefaultRwin)
  , mLocalSessionWindow(kDefaultRwin)
  , mServerSessionWindow(kDefaultRwin)
  , mInitialRwin(ASpdySession::kInitialRwin)
  , mOutputQueueSize(kDefaultQueueSize)
  , mOutputQueueUsed(0)
  , mOutputQueueSent(0)
  , mLastReadEpoch(PR_IntervalNow())
  , mPingSentEpoch(0)
  , mPreviousUsed(false)
  , mWaitingForSettingsAck(false)
  , mGoAwayOnPush(false)
  , mUseH2Deps(false)
  , mAttemptingEarlyData(attemptingEarlyData)
  , mOriginFrameActivated(false)
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  static uint64_t sSerial;
  mSerial = ++sSerial;

  LOG3(("Http2Session::Http2Session %p serial=0x%" PRIX64 "\n", this, mSerial));

  mInputFrameBuffer = MakeUnique<char[]>(mInputFrameBufferSize);
  mOutputQueueBuffer = MakeUnique<char[]>(mOutputQueueSize);
  mDecompressBuffer.SetCapacity(kDefaultBufferSize);

  mPushAllowance = gHttpHandler->SpdyPushAllowance();
  mInitialRwin = std::max(gHttpHandler->SpdyPullAllowance(), mPushAllowance);
  mMaxConcurrent = gHttpHandler->DefaultSpdyConcurrent();
  mSendingChunkSize = gHttpHandler->SpdySendingChunkSize();
  SendHello();

  mLastDataReadEpoch = mLastReadEpoch;

  mPingThreshold = gHttpHandler->SpdyPingThreshold();
  mPreviousPingThreshold = mPingThreshold;
}

void
Http2Session::Shutdown()
{
  for (auto iter = mStreamTransactionHash.Iter(); !iter.Done(); iter.Next()) {
    nsAutoPtr<Http2Stream> &stream = iter.Data();

    // On a clean server hangup the server sets the GoAwayID to be the ID of
    // the last transaction it processed. If the ID of stream in the
    // local stream is greater than that it can safely be restarted because the
    // server guarantees it was not partially processed. Streams that have not
    // registered an ID haven't actually been sent yet so they can always be
    // restarted.
    if (mCleanShutdown &&
        (stream->StreamID() > mGoAwayID || !stream->HasRegisteredID())) {
      CloseStream(stream, NS_ERROR_NET_RESET);  // can be restarted
    } else if (stream->RecvdData()) {
      CloseStream(stream, NS_ERROR_NET_PARTIAL_TRANSFER);
    } else if (mGoAwayReason == INADEQUATE_SECURITY) {
      CloseStream(stream, NS_ERROR_NET_INADEQUATE_SECURITY);
    } else {
      CloseStream(stream, NS_ERROR_ABORT);
    }
  }
}

Http2Session::~Http2Session()
{
  LOG3(("Http2Session::~Http2Session %p mDownstreamState=%X",
        this, mDownstreamState));

  Shutdown();

  Telemetry::Accumulate(Telemetry::SPDY_PARALLEL_STREAMS, mConcurrentHighWater);
  Telemetry::Accumulate(Telemetry::SPDY_REQUEST_PER_CONN, (mNextStreamID - 1) / 2);
  Telemetry::Accumulate(Telemetry::SPDY_SERVER_INITIATED_STREAMS,
                        mServerPushedResources);
  Telemetry::Accumulate(Telemetry::SPDY_GOAWAY_LOCAL, mClientGoAwayReason);
  Telemetry::Accumulate(Telemetry::SPDY_GOAWAY_PEER, mPeerGoAwayReason);
}

void
Http2Session::LogIO(Http2Session *self, Http2Stream *stream,
                    const char *label,
                    const char *data, uint32_t datalen)
{
  if (!LOG5_ENABLED())
    return;

  LOG5(("Http2Session::LogIO %p stream=%p id=0x%X [%s]",
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
        LOG5(("%s", linebuf));
      }
      line = linebuf;
      snprintf(line, 128, "%08X: ", index);
      line += 10;
    }
    snprintf(line, 128 - (line - linebuf), "%02X ", (reinterpret_cast<const uint8_t *>(data))[index]);
    line += 3;
  }
  if (index) {
    *line = 0;
    LOG5(("%s", linebuf));
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
  Http2Session::RecvWindowUpdate,
  Http2Session::RecvContinuation,
  Http2Session::RecvAltSvc, // extension for type 0x0A
  Http2Session::RecvUnused, // 0x0B was BLOCKED still radioactive
  Http2Session::RecvOrigin  // extension for type 0x0C
};

bool
Http2Session::RoomForMoreConcurrent()
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
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
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  LOG3(("Http2Session::ReadTimeoutTick %p delta since last read %ds\n",
       this, PR_IntervalToSeconds(now - mLastReadEpoch)));

  if (!mPingThreshold)
    return UINT32_MAX;

  if ((now - mLastReadEpoch) < mPingThreshold) {
    // recent activity means ping is not an issue
    if (mPingSentEpoch) {
      mPingSentEpoch = 0;
      if (mPreviousUsed) {
        // restore the former value
        mPingThreshold = mPreviousPingThreshold;
        mPreviousUsed = false;
      }
    }

    return PR_IntervalToSeconds(mPingThreshold) -
      PR_IntervalToSeconds(now - mLastReadEpoch);
  }

  if (mPingSentEpoch) {
    LOG3(("Http2Session::ReadTimeoutTick %p handle outstanding ping\n", this));
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
  if (!mPingSentEpoch) {
    mPingSentEpoch = 1; // avoid the 0 sentinel value
  }
  GeneratePing(false);
  Unused << ResumeRecv(); // read the ping reply

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
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
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
                        int32_t aPriority,
                        bool aUseTunnel,
                        nsIInterfaceRequestor *aCallbacks)
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  // integrity check
  if (mStreamTransactionHash.Get(aHttpTransaction)) {
    LOG3(("   New transaction already present\n"));
    MOZ_ASSERT(false, "AddStream duplicate transaction pointer");
    return false;
  }

  if (!mConnection) {
    mConnection = aHttpTransaction->Connection();
  }

  if (mClosed || mShouldGoAway) {
    nsHttpTransaction *trans = aHttpTransaction->QueryHttpTransaction();
    if (trans && !trans->GetPushedStream()) {
      LOG3(("Http2Session::AddStream %p atrans=%p trans=%p session unusable - resched.\n",
            this, aHttpTransaction, trans));
      aHttpTransaction->SetConnection(nullptr);
      nsresult rv = gHttpHandler->InitiateTransaction(trans, trans->Priority());
      if (NS_FAILED(rv)) {
        LOG3(("Http2Session::AddStream %p atrans=%p trans=%p failed to initiate "
              "transaction (%08x).\n", this, aHttpTransaction, trans,
              static_cast<uint32_t>(rv)));
      }
      return true;
    }
  }

  aHttpTransaction->SetConnection(this);
  aHttpTransaction->OnActivated(true);

  if (aUseTunnel) {
    LOG3(("Http2Session::AddStream session=%p trans=%p OnTunnel",
          this, aHttpTransaction));
    DispatchOnTunnel(aHttpTransaction, aCallbacks);
    return true;
  }

  Http2Stream *stream = new Http2Stream(aHttpTransaction, this, aPriority);

  LOG3(("Http2Session::AddStream session=%p stream=%p serial=%" PRIu64 " "
        "NextID=0x%X (tentative)", this, stream, mSerial, mNextStreamID));

  mStreamTransactionHash.Put(aHttpTransaction, stream);

  mReadyForWrite.Push(stream);
  SetWriteCallbacks();

  // Kick off the SYN transmit without waiting for the poll loop
  // This won't work for the first stream because there is no segment reader
  // yet.
  if (mSegmentReader) {
    uint32_t countRead;
    Unused << ReadSegments(nullptr, kDefaultBufferSize, &countRead);
  }

  if (!(aHttpTransaction->Caps() & NS_HTTP_ALLOW_KEEPALIVE) &&
      !aHttpTransaction->IsNullTransaction()) {
    LOG3(("Http2Session::AddStream %p transaction %p forces keep-alive off.\n",
          this, aHttpTransaction));
    DontReuse();
  }

  return true;
}

void
Http2Session::QueueStream(Http2Stream *stream)
{
  // will be removed via processpending or a shutdown path
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  MOZ_ASSERT(!stream->CountAsActive());
  MOZ_ASSERT(!stream->Queued());

  LOG3(("Http2Session::QueueStream %p stream %p queued.", this, stream));

#ifdef DEBUG
  int32_t qsize = mQueuedStreams.GetSize();
  for (int32_t i = 0; i < qsize; i++) {
    Http2Stream *qStream = static_cast<Http2Stream *>(mQueuedStreams.ObjectAt(i));
    MOZ_ASSERT(qStream != stream);
    MOZ_ASSERT(qStream->Queued());
  }
#endif

  stream->SetQueued(true);
  mQueuedStreams.Push(stream);
}

void
Http2Session::ProcessPending()
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  Http2Stream*stream;
  while (RoomForMoreConcurrent() &&
         (stream = static_cast<Http2Stream *>(mQueuedStreams.PopFront()))) {

    LOG3(("Http2Session::ProcessPending %p stream %p woken from queue.",
          this, stream));
    MOZ_ASSERT(!stream->CountAsActive());
    MOZ_ASSERT(stream->Queued());
    stream->SetQueued(false);
    mReadyForWrite.Push(stream);
    SetWriteCallbacks();
  }
}

nsresult
Http2Session::NetworkRead(nsAHttpSegmentWriter *writer, char *buf,
                          uint32_t count, uint32_t *countWritten)
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

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
  if (mConnection &&
      (GetWriteQueueSize() || (mOutputQueueUsed > mOutputQueueSent))) {
    Unused << mConnection->ResumeSend();
  }
}

void
Http2Session::RealignOutputQueue()
{
  if (mAttemptingEarlyData) {
    // We can't realign right now, because we may need what's in there if early
    // data fails.
    return;
  }

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

  if (!avail && mAttemptingEarlyData) {
    // This is kind of a hack, but there are cases where we'll have already
    // written the data we want whlie doing early data, but we get called again
    // with a reader, and we need to avoid calling the reader when there's
    // nothing for it to read.
    return;
  }

  rv = mSegmentReader->
    OnReadSegment(mOutputQueueBuffer.get() + mOutputQueueSent, avail,
                  &countRead);
  LOG3(("Http2Session::FlushOutputQueue %p sz=%d rv=%" PRIx32 " actual=%d",
        this, avail, static_cast<uint32_t>(rv), countRead));

  // Dont worry about errors on write, we will pick this up as a read error too
  if (NS_FAILED(rv))
    return;

  mOutputQueueSent += countRead;

  if (mAttemptingEarlyData) {
    return;
  }

  if (countRead == avail) {
    mOutputQueueUsed = 0;
    mOutputQueueSent = 0;
    return;
  }

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
  LOG3(("Http2Session::DontReuse %p\n", this));
  mShouldGoAway = true;
  if (!mStreamTransactionHash.Count())
    Close(NS_OK);
}

uint32_t
Http2Session::SpdyVersion()
{
  return HTTP_VERSION_2;
}

uint32_t
Http2Session::GetWriteQueueSize()
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  return mReadyForWrite.GetSize();
}

void
Http2Session::ChangeDownstreamState(enum internalStateType newState)
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  LOG3(("Http2Session::ChangeDownstreamState() %p from %X to %X",
        this, mDownstreamState, newState));
  mDownstreamState = newState;
}

void
Http2Session::ResetDownstreamState()
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  LOG3(("Http2Session::ResetDownstreamState() %p", this));
  ChangeDownstreamState(BUFFERING_FRAME_HEADER);

  if (mInputFrameFinal && mInputFrameDataStream) {
    mInputFrameFinal = false;
    LOG3(("  SetRecvdFin id=0x%x\n", mInputFrameDataStream->StreamID()));
    mInputFrameDataStream->SetRecvdFin(true);
    MaybeDecrementConcurrent(mInputFrameDataStream);
  }
  mInputFrameFinal = false;
  mInputFrameBufferUsed = 0;
  mInputFrameDataStream = nullptr;
}

// return true if activated (and counted against max)
// otherwise return false and queue
bool
Http2Session::TryToActivate(Http2Stream *aStream)
{
  if (aStream->Queued()) {
    LOG3(("Http2Session::TryToActivate %p stream=%p already queued.\n", this, aStream));
    return false;
  }

  if (!RoomForMoreConcurrent()) {
    LOG3(("Http2Session::TryToActivate %p stream=%p no room for more concurrent "
          "streams\n", this, aStream));
    QueueStream(aStream);
    return false;
  }

  LOG3(("Http2Session::TryToActivate %p stream=%p\n", this, aStream));
  IncrementConcurrent(aStream);
  return true;
}

void
Http2Session::IncrementConcurrent(Http2Stream *stream)
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  MOZ_ASSERT(!stream->StreamID() || (stream->StreamID() & 1),
             "Do not activate pushed streams");

  nsAHttpTransaction *trans = stream->Transaction();
  if (!trans || !trans->IsNullTransaction() || trans->QuerySpdyConnectTransaction()) {

    MOZ_ASSERT(!stream->CountAsActive());
    stream->SetCountAsActive(true);
    ++mConcurrent;

    if (mConcurrent > mConcurrentHighWater) {
      mConcurrentHighWater = mConcurrent;
    }
    LOG3(("Http2Session::IncrementCounter %p counting stream %p Currently %d "
          "streams in session, high water mark is %d\n",
          this, stream, mConcurrent, mConcurrentHighWater));
  }
}

// call with data length (i.e. 0 for 0 data bytes - ignore 9 byte header)
// dest must have 9 bytes of allocated space
template<typename charType> void
Http2Session::CreateFrameHeader(charType dest, uint16_t frameLength,
                                uint8_t frameType, uint8_t frameFlags,
                                uint32_t streamID)
{
  MOZ_ASSERT(frameLength <= kMaxFrameData, "framelength too large");
  MOZ_ASSERT(!(streamID & 0x80000000));
  MOZ_ASSERT(!frameFlags ||
             (frameType != FRAME_TYPE_PRIORITY &&
              frameType != FRAME_TYPE_RST_STREAM &&
              frameType != FRAME_TYPE_GOAWAY &&
              frameType != FRAME_TYPE_WINDOW_UPDATE));

  dest[0] = 0x00;
  NetworkEndian::writeUint16(dest + 1, frameLength);
  dest[3] = frameType;
  dest[4] = frameFlags;
  NetworkEndian::writeUint32(dest + 5, streamID);
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
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
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
Http2Session::UncompressAndDiscard(bool isPush)
{
  nsresult rv;
  nsAutoCString trash;

  rv = mDecompressor.DecodeHeaderBlock(reinterpret_cast<const uint8_t *>(mDecompressBuffer.BeginReading()),
                                       mDecompressBuffer.Length(), trash, isPush);
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
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG3(("Http2Session::GeneratePing %p isAck=%d\n", this, isAck));

  char *packet = EnsureOutputBuffer(kFrameHeaderBytes + 8);
  mOutputQueueUsed += kFrameHeaderBytes + 8;

  if (isAck) {
    CreateFrameHeader(packet, 8, FRAME_TYPE_PING, kFlag_ACK, 0);
    memcpy(packet + kFrameHeaderBytes,
           mInputFrameBuffer.get() + kFrameHeaderBytes, 8);
  } else {
    CreateFrameHeader(packet, 8, FRAME_TYPE_PING, 0, 0);
    memset(packet + kFrameHeaderBytes, 0, 8);
  }

  LogIO(this, nullptr, "Generate Ping", packet, kFrameHeaderBytes + 8);
  FlushOutputQueue();
}

void
Http2Session::GenerateSettingsAck()
{
  // need to generate ack of this settings frame
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG3(("Http2Session::GenerateSettingsAck %p\n", this));

  char *packet = EnsureOutputBuffer(kFrameHeaderBytes);
  mOutputQueueUsed += kFrameHeaderBytes;
  CreateFrameHeader(packet, 0, FRAME_TYPE_SETTINGS, kFlag_ACK, 0);
  LogIO(this, nullptr, "Generate Settings ACK", packet, kFrameHeaderBytes);
  FlushOutputQueue();
}

void
Http2Session::GeneratePriority(uint32_t aID, uint8_t aPriorityWeight)
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG3(("Http2Session::GeneratePriority %p %X %X\n",
        this, aID, aPriorityWeight));

  uint32_t frameSize = kFrameHeaderBytes + 5;
  char *packet = EnsureOutputBuffer(frameSize);
  mOutputQueueUsed += frameSize;

  CreateFrameHeader(packet, 5, FRAME_TYPE_PRIORITY, 0, aID);
  NetworkEndian::writeUint32(packet + kFrameHeaderBytes, 0);
  memcpy(packet + frameSize - 1, &aPriorityWeight, 1);
  LogIO(this, nullptr, "Generate Priority", packet, frameSize);
  FlushOutputQueue();
}

void
Http2Session::GenerateRstStream(uint32_t aStatusCode, uint32_t aID)
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  // make sure we don't do this twice for the same stream (at least if we
  // have a stream entry for it)
  Http2Stream *stream = mStreamIDHash.Get(aID);
  if (stream) {
    if (stream->SentReset())
      return;
    stream->SetSentReset(true);
  }

  LOG3(("Http2Session::GenerateRst %p 0x%X %d\n", this, aID, aStatusCode));

  uint32_t frameSize = kFrameHeaderBytes + 4;
  char *packet = EnsureOutputBuffer(frameSize);
  mOutputQueueUsed += frameSize;
  CreateFrameHeader(packet, 4, FRAME_TYPE_RST_STREAM, 0, aID);

  NetworkEndian::writeUint32(packet + kFrameHeaderBytes, aStatusCode);

  LogIO(this, nullptr, "Generate Reset", packet, frameSize);
  FlushOutputQueue();
}

void
Http2Session::GenerateGoAway(uint32_t aStatusCode)
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG3(("Http2Session::GenerateGoAway %p code=%X\n", this, aStatusCode));

  mClientGoAwayReason = aStatusCode;
  uint32_t frameSize = kFrameHeaderBytes + 8;
  char *packet = EnsureOutputBuffer(frameSize);
  mOutputQueueUsed += frameSize;

  CreateFrameHeader(packet, 8, FRAME_TYPE_GOAWAY, 0, 0);

  // last-good-stream-id are bytes 9-12 reflecting pushes
  NetworkEndian::writeUint32(packet + kFrameHeaderBytes, mOutgoingGoAwayID);

  // bytes 13-16 are the status code.
  NetworkEndian::writeUint32(packet + frameSize - 4, aStatusCode);

  LogIO(this, nullptr, "Generate GoAway", packet, frameSize);
  FlushOutputQueue();
}

// The Hello is comprised of
// 1] 24 octets of magic, which are designed to
// flush out silent but broken intermediaries
// 2] a settings frame which sets a small flow control window for pushes
// 3] a window update frame which creates a large session flow control window
// 4] 5 priority frames for streams which will never be opened with headers
//    these streams (3, 5, 7, 9, b) build a dependency tree that all other
//    streams will be direct leaves of.
void
Http2Session::SendHello()
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG3(("Http2Session::SendHello %p\n", this));

  // sized for magic + 5 settings and a session window update and 5 priority frames
  // 24 magic, 33 for settings (9 header + 4 settings @6), 13 for window update,
  // 5 priority frames at 14 (9 + 5) each
  static const uint32_t maxSettings = 5;
  static const uint32_t prioritySize = 5 * (kFrameHeaderBytes + 5);
  static const uint32_t maxDataLen = 24 + kFrameHeaderBytes + maxSettings * 6 + 13 + prioritySize;
  char *packet = EnsureOutputBuffer(maxDataLen);
  memcpy(packet, kMagicHello, 24);
  mOutputQueueUsed += 24;
  LogIO(this, nullptr, "Magic Connection Header", packet, 24);

  packet = mOutputQueueBuffer.get() + mOutputQueueUsed;
  memset(packet, 0, maxDataLen - 24);

  // frame header will be filled in after we know how long the frame is
  uint8_t numberOfEntries = 0;

  // entries need to be listed in order by ID
  // 1st entry is bytes 9 to 14
  // 2nd entry is bytes 15 to 20
  // 3rd entry is bytes 21 to 26
  // 4th entry is bytes 27 to 32
  // 5th entry is bytes 33 to 38

  // Let the other endpoint know about our default HPACK decompress table size
  uint32_t maxHpackBufferSize = gHttpHandler->DefaultHpackBuffer();
  mDecompressor.SetInitialMaxBufferSize(maxHpackBufferSize);
  NetworkEndian::writeUint16(packet + kFrameHeaderBytes + (6 * numberOfEntries), SETTINGS_TYPE_HEADER_TABLE_SIZE);
  NetworkEndian::writeUint32(packet + kFrameHeaderBytes + (6 * numberOfEntries) + 2, maxHpackBufferSize);
  numberOfEntries++;

  if (!gHttpHandler->AllowPush()) {
    // If we don't support push then set MAX_CONCURRENT to 0 and also
    // set ENABLE_PUSH to 0
    NetworkEndian::writeUint16(packet + kFrameHeaderBytes + (6 * numberOfEntries), SETTINGS_TYPE_ENABLE_PUSH);
    // The value portion of the setting pair is already initialized to 0
    numberOfEntries++;

    NetworkEndian::writeUint16(packet + kFrameHeaderBytes + (6 * numberOfEntries), SETTINGS_TYPE_MAX_CONCURRENT);
    // The value portion of the setting pair is already initialized to 0
    numberOfEntries++;

    mWaitingForSettingsAck = true;
  }

  // Advertise the Push RWIN for the session, and on each new pull stream
  // send a window update
  NetworkEndian::writeUint16(packet + kFrameHeaderBytes + (6 * numberOfEntries), SETTINGS_TYPE_INITIAL_WINDOW);
  NetworkEndian::writeUint32(packet + kFrameHeaderBytes + (6 * numberOfEntries) + 2, mPushAllowance);
  numberOfEntries++;

  // Make sure the other endpoint knows that we're sticking to the default max
  // frame size
  NetworkEndian::writeUint16(packet + kFrameHeaderBytes + (6 * numberOfEntries), SETTINGS_TYPE_MAX_FRAME_SIZE);
  NetworkEndian::writeUint32(packet + kFrameHeaderBytes + (6 * numberOfEntries) + 2, kMaxFrameData);
  numberOfEntries++;

  MOZ_ASSERT(numberOfEntries <= maxSettings);
  uint32_t dataLen = 6 * numberOfEntries;
  CreateFrameHeader(packet, dataLen, FRAME_TYPE_SETTINGS, 0, 0);
  mOutputQueueUsed += kFrameHeaderBytes + dataLen;

  LogIO(this, nullptr, "Generate Settings", packet, kFrameHeaderBytes + dataLen);

  // now bump the local session window from 64KB
  uint32_t sessionWindowBump = mInitialRwin - kDefaultRwin;
  if (kDefaultRwin < mInitialRwin) {
    // send a window update for the session (Stream 0) for something large
    mLocalSessionWindow = mInitialRwin;

    packet = mOutputQueueBuffer.get() + mOutputQueueUsed;
    CreateFrameHeader(packet, 4, FRAME_TYPE_WINDOW_UPDATE, 0, 0);
    mOutputQueueUsed += kFrameHeaderBytes + 4;
    NetworkEndian::writeUint32(packet + kFrameHeaderBytes, sessionWindowBump);

    LOG3(("Session Window increase at start of session %p %u\n",
          this, sessionWindowBump));
    LogIO(this, nullptr, "Session Window Bump ", packet, kFrameHeaderBytes + 4);
  }

  if (gHttpHandler->UseH2Deps() && gHttpHandler->CriticalRequestPrioritization()) {
    mUseH2Deps = true;
    MOZ_ASSERT(mNextStreamID == kLeaderGroupID);
    CreatePriorityNode(kLeaderGroupID, 0, 200, "leader");
    mNextStreamID += 2;
    MOZ_ASSERT(mNextStreamID == kOtherGroupID);
    CreatePriorityNode(kOtherGroupID, 0, 100, "other");
    mNextStreamID += 2;
    MOZ_ASSERT(mNextStreamID == kBackgroundGroupID);
    CreatePriorityNode(kBackgroundGroupID, 0, 0, "background");
    mNextStreamID += 2;
    MOZ_ASSERT(mNextStreamID == kSpeculativeGroupID);
    CreatePriorityNode(kSpeculativeGroupID, kBackgroundGroupID, 0, "speculative");
    mNextStreamID += 2;
    MOZ_ASSERT(mNextStreamID == kFollowerGroupID);
    CreatePriorityNode(kFollowerGroupID, kLeaderGroupID, 0, "follower");
    mNextStreamID += 2;
    MOZ_ASSERT(mNextStreamID == kUrgentStartGroupID);
    CreatePriorityNode(kUrgentStartGroupID, 0, 240, "urgentStart");
    mNextStreamID += 2;
    // Hey, you! YES YOU! If you add/remove any groups here, you almost
    // certainly need to change the lookup of the stream/ID hash in
    // Http2Session::OnTransportStatus. Yeah, that's right. YOU!
  }

  FlushOutputQueue();
}

void
Http2Session::CreatePriorityNode(uint32_t streamID, uint32_t dependsOn, uint8_t weight,
                                 const char *label)
{
  char *packet = mOutputQueueBuffer.get() + mOutputQueueUsed;
  CreateFrameHeader(packet, 5, FRAME_TYPE_PRIORITY, 0, streamID);
  mOutputQueueUsed += kFrameHeaderBytes + 5;
  NetworkEndian::writeUint32(packet + kFrameHeaderBytes, dependsOn); // depends on
  packet[kFrameHeaderBytes + 4] = weight; // weight

  LOG3(("Http2Session %p generate Priority Frame 0x%X depends on 0x%X "
        "weight %d for %s class\n", this, streamID, dependsOn, weight, label));
  LogIO(this, nullptr, "Priority dep node", packet, kFrameHeaderBytes + 5);
}

// perform a bunch of integrity checks on the stream.
// returns true if passed, false (plus LOG and ABORT) if failed.
bool
Http2Session::VerifyStream(Http2Stream *aStream, uint32_t aOptionalID = 0)
{
  // This is annoying, but at least it is O(1)
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

#ifndef DEBUG
  // Only do the real verification in debug builds
  return true;
#else //DEBUG

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
#endif //DEBUG
}

void
Http2Session::CleanupStream(Http2Stream *aStream, nsresult aResult,
                            errorType aResetCode)
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG3(("Http2Session::CleanupStream %p %p 0x%X %" PRIX32 "\n",
        this, aStream, aStream ? aStream->StreamID() : 0, static_cast<uint32_t>(aResult)));
  if (!aStream) {
    return;
  }

  if (aStream->DeferCleanup(aResult)) {
    LOG3(("Http2Session::CleanupStream 0x%X deferred\n", aStream->StreamID()));
    return;
  }

  if (!VerifyStream(aStream)) {
    LOG3(("Http2Session::CleanupStream failed to verify stream\n"));
    return;
  }

  Http2PushedStream *pushSource = aStream->PushSource();
  if (pushSource) {
    // aStream is a synthetic  attached to an even push
    MOZ_ASSERT(pushSource->GetConsumerStream() == aStream);
    MOZ_ASSERT(!aStream->StreamID());
    MOZ_ASSERT(!(pushSource->StreamID() & 0x1));
    pushSource->SetConsumerStream(nullptr);
  }

  // don't reset a stream that has recevied a fin or rst
  if (!aStream->RecvdFin() && !aStream->RecvdReset() && aStream->StreamID() &&
      !(mInputFrameFinal && (aStream == mInputFrameDataStream))) { // !(recvdfin with mark pending)
    LOG3(("Stream 0x%X had not processed recv FIN, sending RST code %X\n", aStream->StreamID(), aResetCode));
    GenerateRstStream(aResetCode, aStream->StreamID());
  }

  CloseStream(aStream, aResult);

  // Remove the stream from the ID hash table and, if an even id, the pushed
  // table too.
  uint32_t id = aStream->StreamID();
  if (id > 0) {
    mStreamIDHash.Remove(id);
    if (!(id & 1)) {
      mPushedStreams.RemoveElement(aStream);
      Http2PushedStream *pushStream = static_cast<Http2PushedStream *>(aStream);
      nsAutoCString hashKey;
      DebugOnly<bool> rv = pushStream->GetHashKey(hashKey);
      MOZ_ASSERT(rv);
      nsIRequestContext *requestContext = aStream->RequestContext();
      if (requestContext) {
        SpdyPushCache *cache = nullptr;
        requestContext->GetSpdyPushCache(&cache);
        if (cache) {
          // Make sure the id of the stream in the push cache is the same
          // as the id of the stream we're cleaning up! See bug 1368080.
          Http2PushedStream *trash = cache->RemovePushedStreamHttp2ByID(hashKey, aStream->StreamID());
          LOG3(("Http2Session::CleanupStream %p aStream=%p pushStream=%p trash=%p",
                this, aStream, pushStream, trash));
        }
      }
    }
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

void
Http2Session::CleanupStream(uint32_t aID, nsresult aResult, errorType aResetCode)
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  Http2Stream *stream = mStreamIDHash.Get(aID);
  LOG3(("Http2Session::CleanupStream %p by ID 0x%X to stream %p\n",
        this, aID, stream));
  if (!stream) {
    return;
  }
  CleanupStream(stream, aResult, aResetCode);
}

static void RemoveStreamFromQueue(Http2Stream *aStream, nsDeque &queue)
{
  size_t size = queue.GetSize();
  for (size_t count = 0; count < size; ++count) {
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
  RemoveStreamFromQueue(aStream, mPushesReadyForRead);
  RemoveStreamFromQueue(aStream, mSlowConsumersReadyForRead);
}

void
Http2Session::CloseStream(Http2Stream *aStream, nsresult aResult)
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG3(("Http2Session::CloseStream %p %p 0x%x %" PRIX32 "\n",
        this, aStream, aStream->StreamID(), static_cast<uint32_t>(aResult)));

  MaybeDecrementConcurrent(aStream);

  // Check if partial frame reader
  if (aStream == mInputFrameDataStream) {
    LOG3(("Stream had active partial read frame on close"));
    ChangeDownstreamState(DISCARDING_DATA_FRAME);
    mInputFrameDataStream = nullptr;
  }

  RemoveStreamFromQueues(aStream);

  if (aStream->IsTunnel()) {
    UnRegisterTunnel(aStream);
  }

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
Http2Session::ParsePadding(uint8_t &paddingControlBytes, uint16_t &paddingLength)
{
  if (mInputFrameFlags & kFlag_PADDED) {
    paddingLength = *reinterpret_cast<uint8_t *>(&mInputFrameBuffer[kFrameHeaderBytes]);
    paddingControlBytes = 1;
  } else {
    paddingLength = 0;
    paddingControlBytes = 0;
  }

  if (static_cast<uint32_t>(paddingLength + paddingControlBytes) > mInputFrameDataSize) {
    // This is fatal to the session
    LOG3(("Http2Session::ParsePadding %p stream 0x%x PROTOCOL_ERROR "
          "paddingLength %d > frame size %d\n",
          this, mInputFrameID, paddingLength, mInputFrameDataSize));
    RETURN_SESSION_ERROR(this, PROTOCOL_ERROR);
  }

  return NS_OK;
}

nsresult
Http2Session::RecvHeaders(Http2Session *self)
{
  MOZ_ASSERT(self->mInputFrameType == FRAME_TYPE_HEADERS ||
             self->mInputFrameType == FRAME_TYPE_CONTINUATION);

  bool isContinuation = self->mExpectedHeaderID != 0;

  // If this doesn't have END_HEADERS set on it then require the next
  // frame to be HEADERS of the same ID
  bool endHeadersFlag = self->mInputFrameFlags & kFlag_END_HEADERS;

  if (endHeadersFlag)
    self->mExpectedHeaderID = 0;
  else
    self->mExpectedHeaderID = self->mInputFrameID;

  uint32_t priorityLen = 0;
  if (self->mInputFrameFlags & kFlag_PRIORITY) {
    priorityLen = 5;
  }
  nsresult rv = self->SetInputFrameDataStream(self->mInputFrameID);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  // Find out how much padding this frame has, so we can only extract the real
  // header data from the frame.
  uint16_t paddingLength = 0;
  uint8_t paddingControlBytes = 0;

  if (!isContinuation) {
    self->mDecompressBuffer.Truncate();
    rv = self->ParsePadding(paddingControlBytes, paddingLength);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  LOG3(("Http2Session::RecvHeaders %p stream 0x%X priorityLen=%d stream=%p "
        "end_stream=%d end_headers=%d priority_group=%d "
        "paddingLength=%d padded=%d\n",
        self, self->mInputFrameID, priorityLen, self->mInputFrameDataStream,
        self->mInputFrameFlags & kFlag_END_STREAM,
        self->mInputFrameFlags & kFlag_END_HEADERS,
        self->mInputFrameFlags & kFlag_PRIORITY,
        paddingLength,
        self->mInputFrameFlags & kFlag_PADDED));

  if ((paddingControlBytes + priorityLen + paddingLength) > self->mInputFrameDataSize) {
    // This is fatal to the session
    RETURN_SESSION_ERROR(self, PROTOCOL_ERROR);
  }

  if (!self->mInputFrameDataStream) {
    // Cannot find stream. We can continue the session, but we need to
    // uncompress the header block to maintain the correct compression context

    LOG3(("Http2Session::RecvHeaders %p lookup mInputFrameID stream "
          "0x%X failed. NextStreamID = 0x%X\n",
          self, self->mInputFrameID, self->mNextStreamID));

    if (self->mInputFrameID >= self->mNextStreamID)
      self->GenerateRstStream(PROTOCOL_ERROR, self->mInputFrameID);

    self->mDecompressBuffer.Append(&self->mInputFrameBuffer[kFrameHeaderBytes + paddingControlBytes + priorityLen],
                                   self->mInputFrameDataSize - paddingControlBytes - priorityLen - paddingLength);

    if (self->mInputFrameFlags & kFlag_END_HEADERS) {
      rv = self->UncompressAndDiscard(false);
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

  // make sure this is either the first headers or a trailer
  if (self->mInputFrameDataStream->AllHeadersReceived() &&
      !(self->mInputFrameFlags & kFlag_END_STREAM)) {
    // Any header block after the first that does *not* end the stream is
    // illegal.
    LOG3(("Http2Session::Illegal Extra HeaderBlock %p 0x%X\n", self, self->mInputFrameID));
    RETURN_SESSION_ERROR(self, PROTOCOL_ERROR);
  }

  // queue up any compression bytes
  self->mDecompressBuffer.Append(&self->mInputFrameBuffer[kFrameHeaderBytes + paddingControlBytes + priorityLen],
                                 self->mInputFrameDataSize - paddingControlBytes - priorityLen - paddingLength);

  self->mInputFrameDataStream->UpdateTransportReadEvents(self->mInputFrameDataSize);
  self->mLastDataReadEpoch = self->mLastReadEpoch;

  if (!isContinuation) {
    self->mAggregatedHeaderSize = self->mInputFrameDataSize - paddingControlBytes - priorityLen - paddingLength;
  } else {
    self->mAggregatedHeaderSize += self->mInputFrameDataSize - paddingControlBytes - priorityLen - paddingLength;
  }

  if (!endHeadersFlag) { // more are coming - don't process yet
    self->ResetDownstreamState();
    return NS_OK;
  }

  if (isContinuation) {
    Telemetry::Accumulate(Telemetry::SPDY_CONTINUED_HEADERS, self->mAggregatedHeaderSize);
  }

  rv = self->ResponseHeadersComplete();
  if (rv == NS_ERROR_ILLEGAL_VALUE) {
    LOG3(("Http2Session::RecvHeaders %p PROTOCOL_ERROR detected stream 0x%X\n",
          self, self->mInputFrameID));
    self->CleanupStream(self->mInputFrameDataStream, rv, PROTOCOL_ERROR);
    self->ResetDownstreamState();
    rv = NS_OK;
  } else if (NS_FAILED(rv)) {
    // This is fatal to the session.
    self->mGoAwayReason = COMPRESSION_ERROR;
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

  // only interpret headers once, afterwards ignore as trailers
  if (mInputFrameDataStream->AllHeadersReceived()) {
    LOG3(("Http2Session::ResponseHeadersComplete extra headers"));
    MOZ_ASSERT(mInputFrameFlags & kFlag_END_STREAM);
    nsresult rv = UncompressAndDiscard(false);
    if (NS_FAILED(rv)) {
      LOG3(("Http2Session::ResponseHeadersComplete extra uncompress failed\n"));
      return rv;
    }
    mFlatHTTPResponseHeadersOut = 0;
    mFlatHTTPResponseHeaders.Truncate();
    if (mInputFrameFinal) {
      // need to process the fin
      ChangeDownstreamState(PROCESSING_COMPLETE_HEADERS);
    } else {
      ResetDownstreamState();
    }

    return NS_OK;
  }

  // if this turns out to be a 1xx response code we have to
  // undo the headers received bit that we are setting here.
  bool didFirstSetAllRecvd = !mInputFrameDataStream->AllHeadersReceived();
  mInputFrameDataStream->SetAllHeadersReceived();

  // The stream needs to see flattened http headers
  // Uncompressed http/2 format headers currently live in
  // Http2Stream::mDecompressBuffer - convert that to HTTP format in
  // mFlatHTTPResponseHeaders via ConvertHeaders()

  nsresult rv;
  int32_t httpResponseCode; // out param to ConvertResponseHeaders
  mFlatHTTPResponseHeadersOut = 0;
  rv = mInputFrameDataStream->ConvertResponseHeaders(&mDecompressor,
                                                     mDecompressBuffer,
                                                     mFlatHTTPResponseHeaders,
                                                     httpResponseCode);
  if (rv == NS_ERROR_NET_RESET) {
    LOG(("Http2Session::ResponseHeadersComplete %p ConvertResponseHeaders reset\n", this));
    // This means the stream found connection-oriented auth. Treat this like we
    // got a reset with HTTP_1_1_REQUIRED.
    mInputFrameDataStream->Transaction()->DisableSpdy();
    CleanupStream(mInputFrameDataStream, NS_ERROR_NET_RESET, CANCEL_ERROR);
    ResetDownstreamState();
    return NS_OK;
  } else if (NS_FAILED(rv)) {
    return rv;
  }

  // allow more headers in the case of 1xx
  if (((httpResponseCode / 100) == 1) && didFirstSetAllRecvd) {
    mInputFrameDataStream->UnsetAllHeadersReceived();
  }

  ChangeDownstreamState(PROCESSING_COMPLETE_HEADERS);
  return NS_OK;
}

nsresult
Http2Session::RecvPriority(Http2Session *self)
{
  MOZ_ASSERT(self->mInputFrameType == FRAME_TYPE_PRIORITY);

  if (self->mInputFrameDataSize != 5) {
    LOG3(("Http2Session::RecvPriority %p wrong length data=%d\n",
          self, self->mInputFrameDataSize));
    RETURN_SESSION_ERROR(self, PROTOCOL_ERROR);
  }

  if (!self->mInputFrameID) {
    LOG3(("Http2Session::RecvPriority %p stream ID of 0.\n", self));
    RETURN_SESSION_ERROR(self, PROTOCOL_ERROR);
  }

  nsresult rv = self->SetInputFrameDataStream(self->mInputFrameID);
  if (NS_FAILED(rv))
    return rv;

  uint32_t newPriorityDependency = NetworkEndian::readUint32(
      self->mInputFrameBuffer.get() + kFrameHeaderBytes);
  bool exclusive = !!(newPriorityDependency & 0x80000000);
  newPriorityDependency &= 0x7fffffff;
  uint8_t newPriorityWeight = *(self->mInputFrameBuffer.get() + kFrameHeaderBytes + 4);
  if (self->mInputFrameDataStream) {
    self->mInputFrameDataStream->SetPriorityDependency(newPriorityDependency,
                                                       newPriorityWeight,
                                                       exclusive);
  }

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

  self->mDownstreamRstReason = NetworkEndian::readUint32(
      self->mInputFrameBuffer.get() + kFrameHeaderBytes);

  LOG3(("Http2Session::RecvRstStream %p RST_STREAM Reason Code %u ID %x\n",
        self, self->mDownstreamRstReason, self->mInputFrameID));

  DebugOnly<nsresult> rv = self->SetInputFrameDataStream(self->mInputFrameID);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
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

nsresult
Http2Session::RecvSettings(Http2Session *self)
{
  MOZ_ASSERT(self->mInputFrameType == FRAME_TYPE_SETTINGS);

  if (self->mInputFrameID) {
    LOG3(("Http2Session::RecvSettings %p needs stream ID of 0. 0x%X\n",
          self, self->mInputFrameID));
    RETURN_SESSION_ERROR(self, PROTOCOL_ERROR);
  }

  if (self->mInputFrameDataSize % 6) {
    // Number of Settings is determined by dividing by each 6 byte setting
    // entry. So the payload must be a multiple of 6.
    LOG3(("Http2Session::RecvSettings %p SETTINGS wrong length data=%d",
          self, self->mInputFrameDataSize));
    RETURN_SESSION_ERROR(self, PROTOCOL_ERROR);
  }

  self->mReceivedSettings = true;

  uint32_t numEntries = self->mInputFrameDataSize / 6;
  LOG3(("Http2Session::RecvSettings %p SETTINGS Control Frame "
        "with %d entries ack=%X", self, numEntries,
        self->mInputFrameFlags & kFlag_ACK));

  if ((self->mInputFrameFlags & kFlag_ACK) && self->mInputFrameDataSize) {
    LOG3(("Http2Session::RecvSettings %p ACK with non zero payload is err\n", self));
    RETURN_SESSION_ERROR(self, PROTOCOL_ERROR);
  }

  for (uint32_t index = 0; index < numEntries; ++index) {
    uint8_t *setting = reinterpret_cast<uint8_t *>
      (self->mInputFrameBuffer.get()) + kFrameHeaderBytes + index * 6;

    uint16_t id = NetworkEndian::readUint16(setting);
    uint32_t value = NetworkEndian::readUint32(setting + 2);
    LOG3(("Settings ID %u, Value %u", id, value));

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
      self->ProcessPending();
      break;

    case SETTINGS_TYPE_INITIAL_WINDOW:
      {
        Telemetry::Accumulate(Telemetry::SPDY_SETTINGS_IW, value >> 10);
        int32_t delta = value - self->mServerInitialStreamWindow;
        self->mServerInitialStreamWindow = value;

        // SETTINGS only adjusts stream windows. Leave the session window alone.
        // We need to add the delta to all open streams (delta can be negative)
        for (auto iter = self->mStreamTransactionHash.Iter();
             !iter.Done();
             iter.Next()) {
          iter.Data()->UpdateServerReceiveWindow(delta);
        }
      }
      break;

    case SETTINGS_TYPE_MAX_FRAME_SIZE:
      {
        if ((value < kMaxFrameData) || (value >= 0x01000000)) {
          LOG3(("Received invalid max frame size 0x%X", value));
          RETURN_SESSION_ERROR(self, PROTOCOL_ERROR);
        }
        // We stick to the default for simplicity's sake, so nothing to change
      }
      break;

    default:
      break;
    }
  }

  self->ResetDownstreamState();

  if (!(self->mInputFrameFlags & kFlag_ACK)) {
    self->GenerateSettingsAck();
  } else if (self->mWaitingForSettingsAck) {
    self->mGoAwayOnPush = true;
  }

  return NS_OK;
}

nsresult
Http2Session::RecvPushPromise(Http2Session *self)
{
  MOZ_ASSERT(self->mInputFrameType == FRAME_TYPE_PUSH_PROMISE ||
             self->mInputFrameType == FRAME_TYPE_CONTINUATION);

  // Find out how much padding this frame has, so we can only extract the real
  // header data from the frame.
  uint16_t paddingLength = 0;
  uint8_t paddingControlBytes = 0;

  // If this doesn't have END_PUSH_PROMISE set on it then require the next
  // frame to be PUSH_PROMISE of the same ID
  uint32_t promiseLen;
  uint32_t promisedID;

  if (self->mExpectedPushPromiseID) {
    promiseLen = 0; // really a continuation frame
    promisedID = self->mContinuedPromiseStream;
  } else {
    self->mDecompressBuffer.Truncate();
    nsresult rv = self->ParsePadding(paddingControlBytes, paddingLength);
    if (NS_FAILED(rv)) {
      return rv;
    }
    promiseLen = 4;
    promisedID = NetworkEndian::readUint32(
        self->mInputFrameBuffer.get() + kFrameHeaderBytes + paddingControlBytes);
    promisedID &= 0x7fffffff;
    if (promisedID <= self->mLastPushedID) {
      LOG3(("Http2Session::RecvPushPromise %p ID too low %u expected > %u.\n",
            self, promisedID, self->mLastPushedID));
      RETURN_SESSION_ERROR(self, PROTOCOL_ERROR);
    }
    self->mLastPushedID = promisedID;
  }

  uint32_t associatedID = self->mInputFrameID;

  if (self->mInputFrameFlags & kFlag_END_PUSH_PROMISE) {
    self->mExpectedPushPromiseID = 0;
    self->mContinuedPromiseStream = 0;
  } else {
    self->mExpectedPushPromiseID = self->mInputFrameID;
    self->mContinuedPromiseStream = promisedID;
  }

  if ((paddingControlBytes + promiseLen + paddingLength) > self->mInputFrameDataSize) {
    // This is fatal to the session
    LOG3(("Http2Session::RecvPushPromise %p ID 0x%X assoc ID 0x%X "
          "PROTOCOL_ERROR extra %d > frame size %d\n",
          self, promisedID, associatedID, (paddingControlBytes + promiseLen + paddingLength),
          self->mInputFrameDataSize));
    RETURN_SESSION_ERROR(self, PROTOCOL_ERROR);
  }

  LOG3(("Http2Session::RecvPushPromise %p ID 0x%X assoc ID 0x%X "
        "paddingLength %d padded %d\n",
        self, promisedID, associatedID, paddingLength,
        self->mInputFrameFlags & kFlag_PADDED));

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

  if (self->mShouldGoAway && !Http2PushedStream::TestOnPush(associatedStream)) {
    LOG3(("Http2Session::RecvPushPromise %p cache push while in GoAway "
          "mode refused.\n", self));
    self->GenerateRstStream(REFUSED_STREAM_ERROR, promisedID);
  } else if (!gHttpHandler->AllowPush()) {
    // ENABLE_PUSH and MAX_CONCURRENT_STREAMS of 0 in settings disabled push
    LOG3(("Http2Session::RecvPushPromise Push Recevied when Disabled\n"));
    if (self->mGoAwayOnPush) {
      LOG3(("Http2Session::RecvPushPromise sending GOAWAY"));
      RETURN_SESSION_ERROR(self, PROTOCOL_ERROR);
    }
    self->GenerateRstStream(REFUSED_STREAM_ERROR, promisedID);
  } else if (!(associatedID & 1)) {
    LOG3(("Http2Session::RecvPushPromise %p assocated=0x%X on pushed (even) stream not allowed\n",
          self, associatedID));
    self->GenerateRstStream(PROTOCOL_ERROR, promisedID);
  } else if (!associatedStream) {
    LOG3(("Http2Session::RecvPushPromise %p lookup associated ID failed.\n", self));
    self->GenerateRstStream(PROTOCOL_ERROR, promisedID);
  } else {
    nsIRequestContext *requestContext = associatedStream->RequestContext();
    if (requestContext) {
      requestContext->GetSpdyPushCache(&cache);
      if (!cache) {
        cache = new SpdyPushCache();
        if (!cache || NS_FAILED(requestContext->SetSpdyPushCache(cache))) {
          delete cache;
          cache = nullptr;
        }
      }
    }
    if (!cache) {
      // this is unexpected, but we can handle it just by refusing the push
      LOG3(("Http2Session::RecvPushPromise Push Recevied without push cache\n"));
      self->GenerateRstStream(REFUSED_STREAM_ERROR, promisedID);
    } else {
      resetStream = false;
    }
  }

  if (resetStream) {
    // Need to decompress the headers even though we aren't using them yet in
    // order to keep the compression context consistent for other frames
    self->mDecompressBuffer.Append(&self->mInputFrameBuffer[kFrameHeaderBytes + paddingControlBytes + promiseLen],
                                   self->mInputFrameDataSize - paddingControlBytes - promiseLen - paddingLength);
    if (self->mInputFrameFlags & kFlag_END_PUSH_PROMISE) {
      rv = self->UncompressAndDiscard(true);
      if (NS_FAILED(rv)) {
        LOG3(("Http2Session::RecvPushPromise uncompress failed\n"));
        self->mGoAwayReason = COMPRESSION_ERROR;
        return rv;
      }
    }
    self->ResetDownstreamState();
    return NS_OK;
  }

  self->mDecompressBuffer.Append(&self->mInputFrameBuffer[kFrameHeaderBytes + paddingControlBytes + promiseLen],
                                 self->mInputFrameDataSize - paddingControlBytes - promiseLen - paddingLength);

  if (self->mInputFrameType != FRAME_TYPE_CONTINUATION) {
    self->mAggregatedHeaderSize = self->mInputFrameDataSize - paddingControlBytes - promiseLen - paddingLength;
  } else {
    self->mAggregatedHeaderSize += self->mInputFrameDataSize - paddingControlBytes - promiseLen - paddingLength;
  }

  if (!(self->mInputFrameFlags & kFlag_END_PUSH_PROMISE)) {
    LOG3(("Http2Session::RecvPushPromise not finishing processing for multi-frame push\n"));
    self->ResetDownstreamState();
    return NS_OK;
  }

  if (self->mInputFrameType == FRAME_TYPE_CONTINUATION) {
    Telemetry::Accumulate(Telemetry::SPDY_CONTINUED_HEADERS, self->mAggregatedHeaderSize);
  }

  // Create the buffering transaction and push stream
  RefPtr<Http2PushTransactionBuffer> transactionBuffer =
    new Http2PushTransactionBuffer();
  transactionBuffer->SetConnection(self);
  Http2PushedStream *pushedStream =
    new Http2PushedStream(transactionBuffer, self, associatedStream, promisedID);

  rv = pushedStream->ConvertPushHeaders(&self->mDecompressor,
                                        self->mDecompressBuffer,
                                        pushedStream->GetRequestString());

  if (rv == NS_ERROR_NOT_IMPLEMENTED) {
    LOG3(("Http2Session::PushPromise Semantics not Implemented\n"));
    self->GenerateRstStream(REFUSED_STREAM_ERROR, promisedID);
    delete pushedStream;
    self->ResetDownstreamState();
    return NS_OK;
  }

  if (rv == NS_ERROR_ILLEGAL_VALUE) {
    // This means the decompression completed ok, but there was a problem with
    // the decoded headers. Reset the stream and go away.
    self->GenerateRstStream(PROTOCOL_ERROR, promisedID);
    delete pushedStream;
    self->ResetDownstreamState();
    return NS_OK;
  } else if (NS_FAILED(rv)) {
    // This is fatal to the session.
    self->mGoAwayReason = COMPRESSION_ERROR;
    return rv;
  }

  // Ownership of the pushed stream is by the transaction hash, just as it
  // is for a client initiated stream. Errors that aren't fatal to the
  // whole session must call cleanupStream() after this point in order
  // to remove the stream from that hash.
  self->mStreamTransactionHash.Put(transactionBuffer, pushedStream);
  self->mPushedStreams.AppendElement(pushedStream);

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
  Unused << pushedStream->ReadSegments(nullptr, 1, &notUsed);

  nsAutoCString key;
  if (!pushedStream->GetHashKey(key)) {
    LOG3(("Http2Session::RecvPushPromise one of :authority :scheme :path missing from push\n"));
    self->CleanupStream(pushedStream, NS_ERROR_FAILURE, PROTOCOL_ERROR);
    self->ResetDownstreamState();
    return NS_OK;
  }

  // does the pushed origin belong on this connection?
  LOG3(("Http2Session::RecvPushPromise %p origin check %s", self,
        pushedStream->Origin().get()));
  RefPtr<nsStandardURL> pushedOrigin;
  rv = Http2Stream::MakeOriginURL(pushedStream->Origin(), pushedOrigin);
  nsAutoCString pushedHostName;
  int32_t pushedPort = -1;
  if (NS_SUCCEEDED(rv)) {
    rv = pushedOrigin->GetHost(pushedHostName);
  }
  if (NS_SUCCEEDED(rv)) {
    rv = pushedOrigin->GetPort(&pushedPort);
  }
  if (NS_FAILED(rv) ||
      !self->TestJoinConnection(pushedHostName, pushedPort)) {
    LOG3(("Http2Session::RecvPushPromise %p pushed stream mismatched origin %s\n",
          self, pushedStream->Origin().get()));
    self->CleanupStream(pushedStream, NS_ERROR_FAILURE, REFUSED_STREAM_ERROR);
    self->ResetDownstreamState();
    return NS_OK;
  }

  if (pushedStream->TryOnPush()) {
    LOG3(("Http2Session::RecvPushPromise %p channel implements nsIHttpPushListener "
          "stream %p will not be placed into session cache.\n", self, pushedStream));
  } else {
    LOG3(("Http2Session::RecvPushPromise %p place stream into session cache\n", self));
    if (!cache->RegisterPushedStreamHttp2(key, pushedStream)) {
      // This only happens if they've already pushed us this item.
      LOG3(("Http2Session::RecvPushPromise registerPushedStream Failed\n"));
      self->CleanupStream(pushedStream, NS_ERROR_FAILURE, REFUSED_STREAM_ERROR);
      self->ResetDownstreamState();
      return NS_OK;
    }

    // Kick off a lookup into the HTTP cache so we can cancel the push if it's
    // unneeded (we already have it in our local regular cache). See bug 1367551.
    nsCOMPtr<nsICacheStorageService> css =
      do_GetService("@mozilla.org/netwerk/cache-storage-service;1");
    mozilla::OriginAttributes oa;
    pushedStream->GetOriginAttributes(&oa);
    RefPtr<LoadContextInfo> lci = GetLoadContextInfo(false, oa);
    nsCOMPtr<nsICacheStorage> ds;
    css->DiskCacheStorage(lci, false, getter_AddRefs(ds));
    // Build up our full URL for the cache lookup
    nsAutoCString spec;
    spec.Assign(pushedStream->Origin());
    spec.Append(pushedStream->Path());
    RefPtr<nsStandardURL> pushedURL;
    // Nifty trick: this doesn't actually do anything origin-specific, it's just
    // named that way. So by passing it the full spec here, we get a URL with
    // the full path.
    // Another nifty trick! Even though this is using nsIURIs (which are not
    // generally ok off the main thread), since we're not using the protocol
    // handler to create any URIs, this will work just fine here. Don't try this
    // at home, though, kids. I'm a trained professional.
    if (NS_SUCCEEDED(Http2Stream::MakeOriginURL(spec, pushedURL))) {
      LOG3(("Http2Session::RecvPushPromise %p check disk cache for entry", self));
      RefPtr<CachePushCheckCallback> cpcc = new CachePushCheckCallback(self, promisedID, pushedStream->GetRequestString());
      if (NS_FAILED(ds->AsyncOpenURI(pushedURL, EmptyCString(), nsICacheStorage::OPEN_READONLY|nsICacheStorage::OPEN_SECRETLY, cpcc))) {
        LOG3(("Http2Session::RecvPushPromise %p failed to open cache entry for push check", self));
      }
    }
  }

  pushedStream->SetHTTPState(Http2Stream::RESERVED_BY_REMOTE);
  static_assert(Http2Stream::kWorstPriority >= 0,
                "kWorstPriority out of range");
  uint8_t priorityWeight = (nsISupportsPriority::PRIORITY_LOWEST + 1) -
    (Http2Stream::kWorstPriority - Http2Stream::kNormalPriority);
  pushedStream->SetPriority(Http2Stream::kWorstPriority);
  self->GeneratePriority(promisedID, priorityWeight);
  self->ResetDownstreamState();
  return NS_OK;
}

NS_IMPL_ISUPPORTS(Http2Session::CachePushCheckCallback, nsICacheEntryOpenCallback);

Http2Session::CachePushCheckCallback::CachePushCheckCallback(Http2Session *session, uint32_t promisedID, const nsACString &requestString)
  :mPromisedID(promisedID)
{
  mSession = session;
  mRequestHead.ParseHeaderSet(requestString.BeginReading());
}

NS_IMETHODIMP
Http2Session::CachePushCheckCallback::OnCacheEntryCheck(nsICacheEntry *entry, nsIApplicationCache *appCache, uint32_t *result)
{
  MOZ_ASSERT(OnSocketThread(), "Not on socket thread?!");

  // We never care to fully open the entry, since we won't actually use it.
  // We just want to be able to do all our checks to see if a future channel can
  // use this entry, or if we need to accept the push.
  *result = nsICacheEntryOpenCallback::ENTRY_NOT_WANTED;

  bool isForcedValid = false;
  entry->GetIsForcedValid(&isForcedValid);

  nsHttpResponseHead cachedResponseHead;
  nsresult rv = nsHttp::GetHttpResponseHeadFromCacheEntry(entry, &cachedResponseHead);
  if (NS_FAILED(rv)) {
    // Couldn't make sense of what's in the cache entry, go ahead and accept
    // the push.
    return NS_OK;
  }

  if ((cachedResponseHead.Status() / 100) != 2) {
    // Assume the push is sending us a success, while we don't have one in the
    // cache, so we'll accept the push.
    return NS_OK;
  }

  // Get the method that was used to generate the cached response
  nsXPIDLCString buf;
  rv = entry->GetMetaDataElement("request-method", getter_Copies(buf));
  if (NS_FAILED(rv)) {
    // Can't check request method, accept the push
    return NS_OK;
  }
  nsAutoCString pushedMethod;
  mRequestHead.Method(pushedMethod);
  if (!buf.Equals(pushedMethod)) {
    // Methods don't match, accept the push
    return NS_OK;
  }

  int64_t size, contentLength;
  rv = nsHttp::CheckPartial(entry, &size, &contentLength, &cachedResponseHead);
  if (NS_FAILED(rv)) {
    // Couldn't figure out if this was partial or not, accept the push.
    return NS_OK;
  }

  if (size == int64_t(-1) || contentLength != size) {
    // This is partial content in the cache, accept the push.
    return NS_OK;
  }

  nsAutoCString requestedETag;
  if (NS_FAILED(mRequestHead.GetHeader(nsHttp::If_Match, requestedETag))) {
    // Can't check etag
    return NS_OK;
  }
  if (!requestedETag.IsEmpty()) {
    nsAutoCString cachedETag;
    if (NS_FAILED(cachedResponseHead.GetHeader(nsHttp::ETag, cachedETag))) {
      // Can't check etag
      return NS_OK;
    }
    if (!requestedETag.Equals(cachedETag)) {
      // ETags don't match, accept the push.
      return NS_OK;
    }
  }

  nsAutoCString imsString;
  Unused << mRequestHead.GetHeader(nsHttp::If_Modified_Since, imsString);
  if (!buf.IsEmpty()) {
    uint32_t ims = buf.ToInteger(&rv);
    uint32_t lm;
    rv = cachedResponseHead.GetLastModifiedValue(&lm);
    if (NS_SUCCEEDED(rv) && lm && lm < ims) {
      // The push appears to be newer than what's in our cache, accept it.
      return NS_OK;
    }
  }

  nsAutoCString cacheControlRequestHeader;
  Unused << mRequestHead.GetHeader(nsHttp::Cache_Control, cacheControlRequestHeader);
  CacheControlParser cacheControlRequest(cacheControlRequestHeader);
  if (cacheControlRequest.NoStore()) {
    // Don't use a no-store cache entry, accept the push.
    return NS_OK;
  }

  nsXPIDLCString cachedAuth;
  rv = entry->GetMetaDataElement("auth", getter_Copies(cachedAuth));
  if (NS_SUCCEEDED(rv)) {
    uint32_t lastModifiedTime;
    rv = entry->GetLastModified(&lastModifiedTime);
    if (NS_SUCCEEDED(rv)) {
      if ((gHttpHandler->SessionStartTime() > lastModifiedTime) && !cachedAuth.IsEmpty()) {
        // Need to revalidate this, as the auth is old. Accept the push.
        return NS_OK;
      }

      if (cachedAuth.IsEmpty() && mRequestHead.HasHeader(nsHttp::Authorization)) {
        // They're pushing us something with auth, but we didn't cache anything
        // with auth. Accept the push.
        return NS_OK;
      }
    }
  }

  bool weaklyFramed, isImmutable;
  nsHttp::DetermineFramingAndImmutability(entry, &cachedResponseHead, true,
                                          &weaklyFramed, &isImmutable);

  // We'll need this value in later computations...
  uint32_t lastModifiedTime;
  rv = entry->GetLastModified(&lastModifiedTime);
  if (NS_FAILED(rv)) {
    // Ugh, this really sucks. OK, accept the push.
    return NS_OK;
  }

  // Determine if this is the first time that this cache entry
  // has been accessed during this session.
  bool fromPreviousSession =
          (gHttpHandler->SessionStartTime() > lastModifiedTime);

  bool validationRequired = nsHttp::ValidationRequired(isForcedValid,
    &cachedResponseHead, 0/*NWGH: ??? - loadFlags*/, false, isImmutable, false, mRequestHead, entry,
    cacheControlRequest, fromPreviousSession);

  if (validationRequired) {
    // A real channel would most likely hit the net at this point, so let's
    // accept the push.
    return NS_OK;
  }

  // If we get here, then we would be able to use this cache entry. Cancel the
  // push so as not to waste any more bandwidth.
  mSession->CleanupStream(mPromisedID, NS_ERROR_FAILURE, Http2Session::REFUSED_STREAM_ERROR);

  return NS_OK;
}

NS_IMETHODIMP
Http2Session::CachePushCheckCallback::OnCacheEntryAvailable(
    nsICacheEntry *entry, bool isNew, nsIApplicationCache *appCache,
    nsresult result)
{
  // Nothing to do here, all the work is in OnCacheEntryCheck.
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
  self->mGoAwayID = NetworkEndian::readUint32(
      self->mInputFrameBuffer.get() + kFrameHeaderBytes);
  self->mGoAwayID &= 0x7fffffff;
  self->mCleanShutdown = true;
  self->mPeerGoAwayReason = NetworkEndian::readUint32(
      self->mInputFrameBuffer.get() + kFrameHeaderBytes + 4);

  // Find streams greater than the last-good ID and mark them for deletion
  // in the mGoAwayStreamsToRestart queue. The underlying transaction can be
  // restarted.
  for (auto iter = self->mStreamTransactionHash.Iter();
       !iter.Done();
       iter.Next()) {
    // these streams were not processed by the server and can be restarted.
    // Do that after the enumerator completes to avoid the risk of
    // a restart event re-entrantly modifying this hash. Be sure not to restart
    // a pushed (even numbered) stream
    nsAutoPtr<Http2Stream>& stream = iter.Data();
    if ((stream->StreamID() > self->mGoAwayID && (stream->StreamID() & 1)) ||
        !stream->HasRegisteredID()) {
      self->mGoAwayStreamsToRestart.Push(stream);
    }
  }

  // Process the streams marked for deletion and restart.
  size_t size = self->mGoAwayStreamsToRestart.GetSize();
  for (size_t count = 0; count < size; ++count) {
    Http2Stream *stream =
      static_cast<Http2Stream *>(self->mGoAwayStreamsToRestart.PopFront());

    if (self->mPeerGoAwayReason == HTTP_1_1_REQUIRED) {
      stream->Transaction()->DisableSpdy();
    }
    self->CloseStream(stream, NS_ERROR_NET_RESET);
    if (stream->HasRegisteredID())
      self->mStreamIDHash.Remove(stream->StreamID());
    self->mStreamTransactionHash.Remove(stream->Transaction());
  }

  // Queued streams can also be deleted from this session and restarted
  // in another one. (they were never sent on the network so they implicitly
  // are not covered by the last-good id.
  size = self->mQueuedStreams.GetSize();
  for (size_t count = 0; count < size; ++count) {
    Http2Stream *stream =
      static_cast<Http2Stream *>(self->mQueuedStreams.PopFront());
    MOZ_ASSERT(stream->Queued());
    stream->SetQueued(false);
    if (self->mPeerGoAwayReason == HTTP_1_1_REQUIRED) {
      stream->Transaction()->DisableSpdy();
    }
    self->CloseStream(stream, NS_ERROR_NET_RESET);
    self->mStreamTransactionHash.Remove(stream->Transaction());
  }

  LOG3(("Http2Session::RecvGoAway %p GOAWAY Last-Good-ID 0x%X status 0x%X "
        "live streams=%d\n", self, self->mGoAwayID, self->mPeerGoAwayReason,
        self->mStreamTransactionHash.Count()));

  self->ResetDownstreamState();
  return NS_OK;
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

  uint32_t delta = NetworkEndian::readUint32(
      self->mInputFrameBuffer.get() + kFrameHeaderBytes);
  delta &= 0x7fffffff;

  LOG3(("Http2Session::RecvWindowUpdate %p len=%d Stream 0x%X.\n",
        self, delta, self->mInputFrameID));

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

    if (delta == 0) {
      LOG3(("Http2Session::RecvWindowUpdate %p received 0 stream window update",
            self));
      self->CleanupStream(self->mInputFrameDataStream, NS_ERROR_ILLEGAL_VALUE,
                          PROTOCOL_ERROR);
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
          "%" PRId64 " increased by %" PRIu32 " now %" PRId64 ".\n",
          self, self->mInputFrameID, oldRemoteWindow, delta, oldRemoteWindow + delta));

  } else { // session window update
    if (delta == 0) {
      LOG3(("Http2Session::RecvWindowUpdate %p received 0 session window update",
            self));
      RETURN_SESSION_ERROR(self, PROTOCOL_ERROR);
    }

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
      for (auto iter = self->mStreamTransactionHash.Iter();
           !iter.Done();
           iter.Next()) {
        MOZ_ASSERT(self->mServerSessionWindow > 0);

        nsAutoPtr<Http2Stream>& stream = iter.Data();
        if (!stream->BlockedOnRwin() || stream->ServerReceiveWindow() <= 0) {
          continue;
        }

        self->mReadyForWrite.Push(stream);
        self->SetWriteCallbacks();
      }
    }
    LOG3(("Http2Session::RecvWindowUpdate %p session window "
          "%" PRId64 " increased by %d now %" PRId64 ".\n", self,
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

  DebugOnly<nsresult> rv = self->SetInputFrameDataStream(self->mInputFrameID);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

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

class UpdateAltSvcEvent : public Runnable
{
public:
  UpdateAltSvcEvent(const nsCString& header,
                    const nsCString& aOrigin,
                    nsHttpConnectionInfo* aCI,
                    nsIInterfaceRequestor* callbacks)
    : Runnable("net::UpdateAltSvcEvent")
    , mHeader(header)
    , mOrigin(aOrigin)
    , mCI(aCI)
    , mCallbacks(callbacks)
  {
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsCString originScheme;
    nsCString originHost;
    int32_t originPort = -1;

    nsCOMPtr<nsIURI> uri;
    if (NS_FAILED(NS_NewURI(getter_AddRefs(uri), mOrigin))) {
      LOG(("UpdateAltSvcEvent origin does not parse %s\n",
           mOrigin.get()));
      return NS_OK;
    }
    uri->GetScheme(originScheme);
    uri->GetHost(originHost);
    uri->GetPort(&originPort);

    AltSvcMapping::ProcessHeader(mHeader, originScheme, originHost, originPort,
                                 mCI->GetUsername(), mCI->GetPrivate(), mCallbacks,
                                 mCI->ProxyInfo(), 0, mCI->GetOriginAttributes());
    return NS_OK;
  }

private:
  nsCString mHeader;
  nsCString mOrigin;
  RefPtr<nsHttpConnectionInfo> mCI;
  nsCOMPtr<nsIInterfaceRequestor> mCallbacks;
};

// defined as an http2 extension - alt-svc
// defines receipt of frame type 0x0A.. See AlternateSevices.h at least draft -06 sec 4
// as this is an extension, never generate protocol error - just ignore problems
nsresult
Http2Session::RecvAltSvc(Http2Session *self)
{
  MOZ_ASSERT(self->mInputFrameType == FRAME_TYPE_ALTSVC);
  LOG3(("Http2Session::RecvAltSvc %p Flags 0x%X id 0x%X\n", self,
        self->mInputFrameFlags, self->mInputFrameID));

  if (self->mInputFrameDataSize < 2) {
    LOG3(("Http2Session::RecvAltSvc %p frame too small", self));
    self->ResetDownstreamState();
    return NS_OK;
  }

  uint16_t originLen = NetworkEndian::readUint16(
      self->mInputFrameBuffer.get() + kFrameHeaderBytes);
  if (originLen + 2U > self->mInputFrameDataSize) {
    LOG3(("Http2Session::RecvAltSvc %p origin len too big for frame", self));
    self->ResetDownstreamState();
    return NS_OK;
  }

  if (!gHttpHandler->AllowAltSvc()) {
    LOG3(("Http2Session::RecvAltSvc %p frame alt service pref'd off", self));
    self->ResetDownstreamState();
    return NS_OK;
  }

  uint16_t altSvcFieldValueLen = static_cast<uint16_t>(self->mInputFrameDataSize) - 2U - originLen;
  LOG3(("Http2Session::RecvAltSvc %p frame originLen=%u altSvcFieldValueLen=%u\n",
        self, originLen, altSvcFieldValueLen));

  if (self->mInputFrameDataSize > 2000) {
    LOG3(("Http2Session::RecvAltSvc %p frame too large to parse sensibly", self));
    self->ResetDownstreamState();
    return NS_OK;
  }

  nsAutoCString origin;
  bool impliedOrigin = true;
  if (originLen) {
    origin.Assign(self->mInputFrameBuffer.get() + kFrameHeaderBytes + 2, originLen);
    impliedOrigin = false;
  }

  nsAutoCString altSvcFieldValue;
  if (altSvcFieldValueLen) {
    altSvcFieldValue.Assign(self->mInputFrameBuffer.get() + kFrameHeaderBytes + 2 + originLen,
                            altSvcFieldValueLen);
  }

  if (altSvcFieldValue.IsEmpty() || !nsHttp::IsReasonableHeaderValue(altSvcFieldValue)) {
    LOG(("Http2Session %p Alt-Svc Response Header seems unreasonable - skipping\n", self));
    self->ResetDownstreamState();
    return NS_OK;
  }

  if (self->mInputFrameID & 1) {
    // pulled streams apply to the origin of the pulled stream.
    // If the origin field is filled in the frame, the frame should be ignored
    if (!origin.IsEmpty()) {
      LOG(("Http2Session %p Alt-Svc pulled stream has non empty origin\n", self));
      self->ResetDownstreamState();
      return NS_OK;
    }

    if (NS_FAILED(self->SetInputFrameDataStream(self->mInputFrameID)) ||
        !self->mInputFrameDataStream->Transaction() ||
        !self->mInputFrameDataStream->Transaction()->RequestHead()) {
      LOG3(("Http2Session::RecvAltSvc %p got frame w/o origin on invalid stream", self));
      self->ResetDownstreamState();
      return NS_OK;
    }

    self->mInputFrameDataStream->Transaction()->RequestHead()->Origin(origin);
  } else if (!self->mInputFrameID) {
    // ID 0 streams must supply their own origin
    if (origin.IsEmpty()) {
      LOG(("Http2Session %p Alt-Svc Stream 0 has empty origin\n", self));
      self->ResetDownstreamState();
      return NS_OK;
    }
  } else {
    // handling of push streams is not defined. Let's ignore it
    LOG(("Http2Session %p Alt-Svc received on pushed stream - ignoring\n", self));
    self->ResetDownstreamState();
    return NS_OK;
  }

  RefPtr<nsHttpConnectionInfo> ci(self->ConnectionInfo());
  if (!self->mConnection || !ci) {
    LOG3(("Http2Session::RecvAltSvc %p no connection or conninfo for %d", self,
          self->mInputFrameID));
    self->ResetDownstreamState();
    return NS_OK;
  }

  if (!impliedOrigin) {
    bool okToReroute = true;
    nsCOMPtr<nsISupports> securityInfo;
    self->mConnection->GetSecurityInfo(getter_AddRefs(securityInfo));
    nsCOMPtr<nsISSLSocketControl> ssl = do_QueryInterface(securityInfo);
    if (!ssl) {
      okToReroute = false;
    }

    // a little off main thread origin parser. This is a non critical function because
    // any alternate route created has to be verified anyhow
    nsAutoCString specifiedOriginHost;
    if (origin.EqualsIgnoreCase("https://", 8)) {
      specifiedOriginHost.Assign(origin.get() + 8, origin.Length() - 8);
    } else if (origin.EqualsIgnoreCase("http://", 7)) {
      specifiedOriginHost.Assign(origin.get() + 7, origin.Length() - 7);
    }

    int32_t colonOffset = specifiedOriginHost.FindCharInSet(":", 0);
    if (colonOffset != kNotFound) {
      specifiedOriginHost.Truncate(colonOffset);
    }

    if (okToReroute) {
      ssl->IsAcceptableForHost(specifiedOriginHost, &okToReroute);
    }

    if (!okToReroute) {
      LOG3(("Http2Session::RecvAltSvc %p can't reroute non-authoritative origin %s",
            self, origin.BeginReading()));
      self->ResetDownstreamState();
      return NS_OK;
    }
  }

  nsCOMPtr<nsISupports> callbacks;
  self->mConnection->GetSecurityInfo(getter_AddRefs(callbacks));
  nsCOMPtr<nsIInterfaceRequestor> irCallbacks = do_QueryInterface(callbacks);

  RefPtr<UpdateAltSvcEvent> event =
    new UpdateAltSvcEvent(altSvcFieldValue, origin, ci, irCallbacks);
  NS_DispatchToMainThread(event);
  self->ResetDownstreamState();
  return NS_OK;
}

void
Http2Session::Received421(nsHttpConnectionInfo *ci)
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG3(("Http2Session::Recevied421 %p %d\n", this, mOriginFrameActivated));
  if (!mOriginFrameActivated || !ci) {
    return;
  }

  nsAutoCString key(ci->GetOrigin());
  key.Append(':');
  key.AppendInt(ci->OriginPort());
  mOriginFrame.Remove(key);
  LOG3(("Http2Session::Received421 %p key %s removed\n", this, key.get()));
}

nsresult
Http2Session::RecvUnused(Http2Session *self)
{
  LOG3(("Http2Session %p unknown frame type %x ignored\n",
        self, self->mInputFrameType));
  self->ResetDownstreamState();
  return NS_OK;
}

// defined as an http2 extension - origin
// defines receipt of frame type 0x0b.. http://httpwg.org/http-extensions/origin-frame.html
// as this is an extension, never generate protocol error - just ignore problems
nsresult
Http2Session::RecvOrigin(Http2Session *self)
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  MOZ_ASSERT(self->mInputFrameType == FRAME_TYPE_ORIGIN);
  LOG3(("Http2Session::RecvOrigin %p Flags 0x%X id 0x%X\n", self,
        self->mInputFrameFlags, self->mInputFrameID));

  if (self->mInputFrameFlags & 0x0F) {
    LOG3(("Http2Session::RecvOrigin %p leading flags must be 0", self));
    self->ResetDownstreamState();
    return NS_OK;
  }

  if (self->mInputFrameID) {
    LOG3(("Http2Session::RecvOrigin %p not stream 0", self));
    self->ResetDownstreamState();
    return NS_OK;
  }

  if (self->ConnectionInfo()->UsingProxy()) {
    LOG3(("Http2Session::RecvOrigin %p must not use proxy", self));
    self->ResetDownstreamState();
    return NS_OK;
  }

  if (!gHttpHandler->AllowOriginExtension()) {
    LOG3(("Http2Session::RecvOrigin %p origin extension pref'd off", self));
    self->ResetDownstreamState();
    return NS_OK;
  }

  uint32_t offset = 0;
  self->mOriginFrameActivated = true;

  while (self->mInputFrameDataSize >= (offset + 2U)) {

    uint16_t originLen = NetworkEndian::readUint16(
      self->mInputFrameBuffer.get() + kFrameHeaderBytes + offset);
    LOG3(("Http2Session::RecvOrigin %p origin extension defined as %d bytes\n", self, originLen));
    if (originLen + 2U + offset > self->mInputFrameDataSize) {
      LOG3(("Http2Session::RecvOrigin %p origin len too big for frame", self));
      break;
    }

    nsAutoCString originString;
    RefPtr<nsStandardURL> originURL;
    originString.Assign(self->mInputFrameBuffer.get() + kFrameHeaderBytes + offset + 2, originLen);
    offset += originLen + 2;
    if (NS_FAILED(Http2Stream::MakeOriginURL(originString, originURL))){
      LOG3(("Http2Session::RecvOrigin %p origin frame string %s failed to parse\n", self, originString.get()));
      continue;
    }

    LOG3(("Http2Session::RecvOrigin %p origin frame string %s parsed OK\n", self, originString.get()));
    bool isHttps = false;
    if (NS_FAILED(originURL->SchemeIs("https", &isHttps)) || !isHttps) {
      LOG3(("Http2Session::RecvOrigin %p origin frame not https\n", self));
      continue;
    }

    int32_t port = -1;
    originURL->GetPort(&port);
    if (port == -1) {
      port = 443;
    }
    // dont use ->GetHostPort because we want explicit 443
    nsAutoCString host;
    originURL->GetHost(host);
    nsAutoCString key(host);
    key.Append(':');
    key.AppendInt(port);
    if (!self->mOriginFrame.Get(key)) {
      self->mOriginFrame.Put(key, true);
      RefPtr<nsHttpConnection> conn(self->HttpConnection());
      MOZ_ASSERT(conn.get());
      gHttpHandler->ConnMgr()->RegisterOriginCoalescingKey(conn, host, port);
    } else {
      LOG3(("Http2Session::RecvOrigin %p origin frame already in set\n", self));
    }
  }

  self->ResetDownstreamState();
  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsAHttpTransaction. It is expected that nsHttpConnection is the caller
// of these methods
//-----------------------------------------------------------------------------

void
Http2Session::OnTransportStatus(nsITransport* aTransport,
                                nsresult aStatus, int64_t aProgress)
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  switch (aStatus) {
    // These should appear only once, deliver to the first
    // transaction on the session.
  case NS_NET_STATUS_RESOLVING_HOST:
  case NS_NET_STATUS_RESOLVED_HOST:
  case NS_NET_STATUS_CONNECTING_TO:
  case NS_NET_STATUS_CONNECTED_TO:
  case NS_NET_STATUS_TLS_HANDSHAKE_STARTING:
  case NS_NET_STATUS_TLS_HANDSHAKE_ENDED:
  {
    Http2Stream *target = mStreamIDHash.Get(mUseH2Deps ? 0xF : 0x3);
    if (!target) {
      // any transaction will do if we can't find the low numbered one
      // generally this happens when the initial transaction hasn't been
      // assigned a stream id yet.
      auto iter = mStreamTransactionHash.Iter();
      if (!iter.Done()) {
        target = iter.Data();
      }
    }
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
Http2Session::ReadSegmentsAgain(nsAHttpSegmentReader *reader,
                                uint32_t count, uint32_t *countRead, bool *again)
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  MOZ_ASSERT(!mSegmentReader || !reader || (mSegmentReader == reader),
             "Inconsistent Write Function Callback");

  nsresult rv = ConfirmTLSProfile();
  if (NS_FAILED(rv)) {
    if (mGoAwayReason == INADEQUATE_SECURITY) {
      LOG3(("Http2Session::ReadSegments %p returning INADEQUATE_SECURITY %" PRIx32,
            this, static_cast<uint32_t>(NS_ERROR_NET_INADEQUATE_SECURITY)));
      rv = NS_ERROR_NET_INADEQUATE_SECURITY;
    }
    return rv;
  }

  if (reader)
    mSegmentReader = reader;

  *countRead = 0;

  LOG3(("Http2Session::ReadSegments %p", this));

  Http2Stream *stream = static_cast<Http2Stream *>(mReadyForWrite.PopFront());
  if (!stream) {
    LOG3(("Http2Session %p could not identify a stream to write; suspending.",
          this));
    uint32_t availBeforeFlush = mOutputQueueUsed - mOutputQueueSent;
    FlushOutputQueue();
    uint32_t availAfterFlush = mOutputQueueUsed - mOutputQueueSent;
    if (availBeforeFlush != availAfterFlush) {
      LOG3(("Http2Session %p ResumeRecv After early flush in ReadSegments", this));
      Unused << ResumeRecv();
    }
    SetWriteCallbacks();
    if (mAttemptingEarlyData) {
      // We can still try to send our preamble as early-data
      *countRead = mOutputQueueUsed - mOutputQueueSent;
    }
    return *countRead ? NS_OK : NS_BASE_STREAM_WOULD_BLOCK;
  }

  uint32_t earlyDataUsed = 0;
  if (mAttemptingEarlyData) {
    if (!stream->Do0RTT()) {
      LOG3(("Http2Session %p will not get early data from Http2Stream %p 0x%X",
            this, stream, stream->StreamID()));
      FlushOutputQueue();
      SetWriteCallbacks();
      // We can still send our preamble
      *countRead = mOutputQueueUsed - mOutputQueueSent;
      return *countRead ? NS_OK : NS_BASE_STREAM_WOULD_BLOCK;
    }

    // Need to adjust this to only take as much as we can fit in with the
    // preamble/settings/priority stuff
    count -= (mOutputQueueUsed - mOutputQueueSent);

    // Keep track of this to add it into countRead later, as
    // stream->ReadSegments will likely change the value of mOutputQueueUsed.
    earlyDataUsed = mOutputQueueUsed - mOutputQueueSent;
  }

  LOG3(("Http2Session %p will write from Http2Stream %p 0x%X "
        "block-input=%d block-output=%d\n", this, stream, stream->StreamID(),
        stream->RequestBlockedOnRead(), stream->BlockedOnRwin()));

  rv = stream->ReadSegments(this, count, countRead);

  if (earlyDataUsed) {
    // Do this here because countRead could get reset somewhere down the rabbit
    // hole of stream->ReadSegments, and we want to make sure we return the
    // proper value to our caller.
    *countRead += earlyDataUsed;
  }

  if (mAttemptingEarlyData && !m0RTTStreams.Contains(stream)) {
    LOG3(("Http2Session::ReadSegmentsAgain adding stream %d to m0RTTStreams\n",
          stream->StreamID()));
    m0RTTStreams.AppendElement(stream);
  }

  // Not every permutation of stream->ReadSegents produces data (and therefore
  // tries to flush the output queue) - SENDING_FIN_STREAM can be an example
  // of that. But we might still have old data buffered that would be good
  // to flush.
  FlushOutputQueue();

  // Allow new server reads - that might be data or control information
  // (e.g. window updates or http replies) that are responses to these writes
  Unused << ResumeRecv();

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
    LOG3(("Http2Session::ReadSegments %p may return FAIL code %" PRIX32,
          this, static_cast<uint32_t>(rv)));
    if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
      return rv;
    }

    CleanupStream(stream, rv, CANCEL_ERROR);
    if (SoftStreamError(rv)) {
      LOG3(("Http2Session::ReadSegments %p soft error override\n", this));
      *again = false;
      SetWriteCallbacks();
      rv = NS_OK;
    }
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

nsresult
Http2Session::ReadSegments(nsAHttpSegmentReader *reader,
                           uint32_t count, uint32_t *countRead)
{
  bool again = false;
  return ReadSegmentsAgain(reader, count, countRead, &again);
}

nsresult
Http2Session::ReadyToProcessDataFrame(enum internalStateType newState)
{
  MOZ_ASSERT(newState == PROCESSING_DATA_FRAME ||
             newState == DISCARDING_DATA_FRAME_PADDING);
  ChangeDownstreamState(newState);

  Telemetry::Accumulate(Telemetry::SPDY_CHUNK_RECVD,
                        mInputFrameDataSize >> 10);
  mLastDataReadEpoch = mLastReadEpoch;

  if (!mInputFrameID) {
    LOG3(("Http2Session::ReadyToProcessDataFrame %p data frame stream 0\n",
          this));
    RETURN_SESSION_ERROR(this, PROTOCOL_ERROR);
  }

  nsresult rv = SetInputFrameDataStream(mInputFrameID);
  if (NS_FAILED(rv)) {
    LOG3(("Http2Session::ReadyToProcessDataFrame %p lookup streamID 0x%X "
          "failed. probably due to verification.\n", this, mInputFrameID));
    return rv;
  }
  if (!mInputFrameDataStream) {
    LOG3(("Http2Session::ReadyToProcessDataFrame %p lookup streamID 0x%X "
          "failed. Next = 0x%X", this, mInputFrameID, mNextStreamID));
    if (mInputFrameID >= mNextStreamID)
      GenerateRstStream(PROTOCOL_ERROR, mInputFrameID);
    ChangeDownstreamState(DISCARDING_DATA_FRAME);
  } else if (mInputFrameDataStream->RecvdFin() ||
            mInputFrameDataStream->RecvdReset() ||
            mInputFrameDataStream->SentReset()) {
    LOG3(("Http2Session::ReadyToProcessDataFrame %p streamID 0x%X "
          "Data arrived for already server closed stream.\n",
          this, mInputFrameID));
    if (mInputFrameDataStream->RecvdFin() || mInputFrameDataStream->RecvdReset())
      GenerateRstStream(STREAM_CLOSED_ERROR, mInputFrameID);
    ChangeDownstreamState(DISCARDING_DATA_FRAME);
  } else if (mInputFrameDataSize == 0 && !mInputFrameFinal) {
    // Only if non-final because the stream properly handles final frames of any
    // size, and we want the stream to be able to notice its own end flag.
    LOG3(("Http2Session::ReadyToProcessDataFrame %p streamID 0x%X "
          "Ignoring 0-length non-terminal data frame.", this, mInputFrameID));
    ChangeDownstreamState(DISCARDING_DATA_FRAME);
  }

  LOG3(("Start Processing Data Frame. "
        "Session=%p Stream ID 0x%X Stream Ptr %p Fin=%d Len=%d",
        this, mInputFrameID, mInputFrameDataStream, mInputFrameFinal,
        mInputFrameDataSize));
  UpdateLocalRwin(mInputFrameDataStream, mInputFrameDataSize);

  if (mInputFrameDataStream) {
    mInputFrameDataStream->SetRecvdData(true);
  }

  return NS_OK;
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
Http2Session::WriteSegmentsAgain(nsAHttpSegmentWriter *writer,
                                 uint32_t count, uint32_t *countWritten,
                                 bool *again)
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

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
    static_cast<Http2Stream *>(mPushesReadyForRead.PopFront());
  if (pushConnectedStream) {
    return ProcessConnectedPush(pushConnectedStream, writer, count, countWritten);
  }

  // feed gecko channels that previously stopped consuming data
  // only take data from stored buffers
  Http2Stream *slowConsumer =
    static_cast<Http2Stream *>(mSlowConsumersReadyForRead.PopFront());
  if (slowConsumer) {
    internalStateType savedState = mDownstreamState;
    mDownstreamState = NOT_USING_NETWORK;
    rv = ProcessSlowConsumer(slowConsumer, writer, count, countWritten);
    mDownstreamState = savedState;
    return rv;
  }

  // The BUFFERING_OPENING_SETTINGS state is just like any BUFFERING_FRAME_HEADER
  // except the only frame type it will allow is SETTINGS

  // The session layer buffers the leading 8 byte header of every frame.
  // Non-Data frames are then buffered for their full length, but data
  // frames (type 0) are passed through to the http stack unprocessed

  if (mDownstreamState == BUFFERING_OPENING_SETTINGS ||
      mDownstreamState == BUFFERING_FRAME_HEADER) {
    // The first 9 bytes of every frame is header information that
    // we are going to want to strip before passing to http. That is
    // true of both control and data packets.

    MOZ_ASSERT(mInputFrameBufferUsed < kFrameHeaderBytes,
               "Frame Buffer Used Too Large for State");

    rv = NetworkRead(writer, &mInputFrameBuffer[mInputFrameBufferUsed],
                     kFrameHeaderBytes - mInputFrameBufferUsed, countWritten);

    if (NS_FAILED(rv)) {
      LOG3(("Http2Session %p buffering frame header read failure %" PRIx32 "\n",
            this, static_cast<uint32_t>(rv)));
      // maybe just blocked reading from network
      if (rv == NS_BASE_STREAM_WOULD_BLOCK)
        rv = NS_OK;
      return rv;
    }

    LogIO(this, nullptr, "Reading Frame Header",
          &mInputFrameBuffer[mInputFrameBufferUsed], *countWritten);

    mInputFrameBufferUsed += *countWritten;

    if (mInputFrameBufferUsed < kFrameHeaderBytes)
    {
      LOG3(("Http2Session::WriteSegments %p "
            "BUFFERING FRAME HEADER incomplete size=%d",
            this, mInputFrameBufferUsed));
      return rv;
    }

    // 3 bytes of length, 1 type byte, 1 flag byte, 1 unused bit, 31 bits of ID
    uint8_t totallyWastedByte = mInputFrameBuffer.get()[0];
    mInputFrameDataSize = NetworkEndian::readUint16(
        mInputFrameBuffer.get() + 1);
    if (totallyWastedByte || (mInputFrameDataSize > kMaxFrameData)) {
      LOG3(("Got frame too large 0x%02X%04X", totallyWastedByte, mInputFrameDataSize));
      RETURN_SESSION_ERROR(this, PROTOCOL_ERROR);
    }
    mInputFrameType = *reinterpret_cast<uint8_t *>(mInputFrameBuffer.get() + kFrameLengthBytes);
    mInputFrameFlags = *reinterpret_cast<uint8_t *>(mInputFrameBuffer.get() + kFrameLengthBytes + kFrameTypeBytes);
    mInputFrameID = NetworkEndian::readUint32(
        mInputFrameBuffer.get() + kFrameLengthBytes + kFrameTypeBytes + kFrameFlagBytes);
    mInputFrameID &= 0x7fffffff;
    mInputFrameDataRead = 0;

    if (mInputFrameType == FRAME_TYPE_DATA || mInputFrameType == FRAME_TYPE_HEADERS)  {
      mInputFrameFinal = mInputFrameFlags & kFlag_END_STREAM;
    } else {
      mInputFrameFinal = 0;
    }

    mPaddingLength = 0;

    LOG3(("Http2Session::WriteSegments[%p::%" PRIu64 "] Frame Header Read "
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
      EnsureBuffer(mInputFrameBuffer, mInputFrameDataSize + kFrameHeaderBytes,
                   kFrameHeaderBytes, mInputFrameBufferSize);
      ChangeDownstreamState(BUFFERING_CONTROL_FRAME);
    } else if (mInputFrameFlags & kFlag_PADDED) {
      ChangeDownstreamState(PROCESSING_DATA_FRAME_PADDING_CONTROL);
    } else {
      rv = ReadyToProcessDataFrame(PROCESSING_DATA_FRAME);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }

  if (mDownstreamState == PROCESSING_DATA_FRAME_PADDING_CONTROL) {
    MOZ_ASSERT(mInputFrameFlags & kFlag_PADDED,
               "Processing padding control on unpadded frame");

    MOZ_ASSERT(mInputFrameBufferUsed < (kFrameHeaderBytes + 1),
               "Frame buffer used too large for state");

    rv = NetworkRead(writer, &mInputFrameBuffer[mInputFrameBufferUsed],
                     (kFrameHeaderBytes + 1) - mInputFrameBufferUsed,
                     countWritten);

    if (NS_FAILED(rv)) {
      LOG3(("Http2Session %p buffering data frame padding control read failure %" PRIx32 "\n",
            this, static_cast<uint32_t>(rv)));
      // maybe just blocked reading from network
      if (rv == NS_BASE_STREAM_WOULD_BLOCK)
        rv = NS_OK;
      return rv;
    }

    LogIO(this, nullptr, "Reading Data Frame Padding Control",
          &mInputFrameBuffer[mInputFrameBufferUsed], *countWritten);

    mInputFrameBufferUsed += *countWritten;

    if (mInputFrameBufferUsed - kFrameHeaderBytes < 1) {
      LOG3(("Http2Session::WriteSegments %p "
            "BUFFERING DATA FRAME CONTROL PADDING incomplete size=%d",
            this, mInputFrameBufferUsed - 8));
      return rv;
    }

    ++mInputFrameDataRead;

    char *control = &mInputFrameBuffer[kFrameHeaderBytes];
    mPaddingLength = static_cast<uint8_t>(*control);

    LOG3(("Http2Session::WriteSegments %p stream 0x%X mPaddingLength=%d", this,
          mInputFrameID, mPaddingLength));

    if (1U + mPaddingLength > mInputFrameDataSize) {
      LOG3(("Http2Session::WriteSegments %p stream 0x%X padding too large for "
            "frame", this, mInputFrameID));
      RETURN_SESSION_ERROR(this, PROTOCOL_ERROR);
    } else if (1U + mPaddingLength == mInputFrameDataSize) {
      // This frame consists entirely of padding, we can just discard it
      LOG3(("Http2Session::WriteSegments %p stream 0x%X frame with only padding",
            this, mInputFrameID));
      rv = ReadyToProcessDataFrame(DISCARDING_DATA_FRAME_PADDING);
      if (NS_FAILED(rv)) {
        return rv;
      }
    } else {
      LOG3(("Http2Session::WriteSegments %p stream 0x%X ready to read HTTP data",
            this, mInputFrameID));
      rv = ReadyToProcessDataFrame(PROCESSING_DATA_FRAME);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }

  if (mDownstreamState == PROCESSING_CONTROL_RST_STREAM) {
    nsresult streamCleanupCode;

    // There is no bounds checking on the error code.. we provide special
    // handling for a couple of cases and all others (including unknown) are
    // equivalent to cancel.
    if (mDownstreamRstReason == REFUSED_STREAM_ERROR) {
      streamCleanupCode = NS_ERROR_NET_RESET;      // can retry this 100% safely
      mInputFrameDataStream->Transaction()->ReuseConnectionOnRestartOK(true);
    } else if (mDownstreamRstReason == HTTP_1_1_REQUIRED) {
      streamCleanupCode = NS_ERROR_NET_RESET;
      mInputFrameDataStream->Transaction()->ReuseConnectionOnRestartOK(true);
      mInputFrameDataStream->Transaction()->DisableSpdy();
    } else {
      streamCleanupCode = mInputFrameDataStream->RecvdData() ?
        NS_ERROR_NET_PARTIAL_TRANSFER :
        NS_ERROR_NET_INTERRUPT;
    }

    if (mDownstreamRstReason == COMPRESSION_ERROR) {
      mShouldGoAway = true;
    }

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

    uint32_t streamID = mInputFrameDataStream->StreamID();
    mSegmentWriter = writer;
    rv = mInputFrameDataStream->WriteSegments(this, count, countWritten);
    mSegmentWriter = nullptr;

    mLastDataReadEpoch = mLastReadEpoch;

    if (SoftStreamError(rv)) {
      // This will happen when the transaction figures out it is EOF, generally
      // due to a content-length match being made. Return OK from this function
      // otherwise the whole session would be torn down.

      // if we were doing PROCESSING_COMPLETE_HEADERS need to pop the state
      // back to PROCESSING_DATA_FRAME where we came from
      mDownstreamState = PROCESSING_DATA_FRAME;

      if (mInputFrameDataRead == mInputFrameDataSize)
        ResetDownstreamState();
      LOG3(("Http2Session::WriteSegments session=%p id 0x%X "
            "needscleanup=%p. cleanup stream based on "
            "stream->writeSegments returning code %" PRIx32 "\n",
            this, streamID, mNeedsCleanup, static_cast<uint32_t>(rv)));
      MOZ_ASSERT(!mNeedsCleanup || mNeedsCleanup->StreamID() == streamID);
      CleanupStream(streamID, NS_OK, CANCEL_ERROR);
      mNeedsCleanup = nullptr;
      *again = false;
      rv = ResumeRecv();
      if (NS_FAILED(rv)) {
        LOG3(("ResumeRecv returned code %x", static_cast<uint32_t>(rv)));
      }
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
      LOG3(("Http2Session %p data frame read failure %" PRIx32 "\n", this,
            static_cast<uint32_t>(rv)));
      // maybe just blocked reading from network
      if (rv == NS_BASE_STREAM_WOULD_BLOCK)
        rv = NS_OK;
    }

    return rv;
  }

  if (mDownstreamState == DISCARDING_DATA_FRAME ||
      mDownstreamState == DISCARDING_DATA_FRAME_PADDING) {
    char trash[4096];
    uint32_t discardCount = std::min(mInputFrameDataSize - mInputFrameDataRead,
                                     4096U);
    LOG3(("Http2Session::WriteSegments %p trying to discard %d bytes of data",
          this, discardCount));

    if (!discardCount) {
      ResetDownstreamState();
      Unused << ResumeRecv();
      return NS_BASE_STREAM_WOULD_BLOCK;
    }

    rv = NetworkRead(writer, trash, discardCount, countWritten);

    if (NS_FAILED(rv)) {
      LOG3(("Http2Session %p discard frame read failure %" PRIx32 "\n", this,
            static_cast<uint32_t>(rv)));
      // maybe just blocked reading from network
      if (rv == NS_BASE_STREAM_WOULD_BLOCK)
        rv = NS_OK;
      return rv;
    }

    LogIO(this, nullptr, "Discarding Frame", trash, *countWritten);

    mInputFrameDataRead += *countWritten;

    if (mInputFrameDataRead == mInputFrameDataSize) {
      Http2Stream *streamToCleanup = nullptr;
      if (mInputFrameFinal) {
        streamToCleanup = mInputFrameDataStream;
      }

      ResetDownstreamState();

      if (streamToCleanup) {
        CleanupStream(streamToCleanup, NS_OK, CANCEL_ERROR);
      }
    }
    return rv;
  }

  if (mDownstreamState != BUFFERING_CONTROL_FRAME) {
    MOZ_ASSERT(false); // this cannot happen
    return NS_ERROR_UNEXPECTED;
  }

  MOZ_ASSERT(mInputFrameBufferUsed == kFrameHeaderBytes, "Frame Buffer Header Not Present");
  MOZ_ASSERT(mInputFrameDataSize + kFrameHeaderBytes <= mInputFrameBufferSize,
             "allocation for control frame insufficient");

  rv = NetworkRead(writer, &mInputFrameBuffer[kFrameHeaderBytes + mInputFrameDataRead],
                   mInputFrameDataSize - mInputFrameDataRead, countWritten);

  if (NS_FAILED(rv)) {
    LOG3(("Http2Session %p buffering control frame read failure %" PRIx32 "\n",
          this, static_cast<uint32_t>(rv)));
    // maybe just blocked reading from network
    if (rv == NS_BASE_STREAM_WOULD_BLOCK)
      rv = NS_OK;
    return rv;
  }

  LogIO(this, nullptr, "Reading Control Frame",
        &mInputFrameBuffer[kFrameHeaderBytes + mInputFrameDataRead], *countWritten);

  mInputFrameDataRead += *countWritten;

  if (mInputFrameDataRead != mInputFrameDataSize)
    return NS_OK;

  MOZ_ASSERT(mInputFrameType != FRAME_TYPE_DATA);
  if (mInputFrameType < FRAME_TYPE_LAST) {
    rv = sControlFunctions[mInputFrameType](this);
  } else {
    // Section 4.1 requires this to be ignored; though protocol_error would
    // be better
    LOG3(("Http2Session %p unknown frame type %x ignored\n",
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

nsresult
Http2Session::WriteSegments(nsAHttpSegmentWriter *writer,
                            uint32_t count, uint32_t *countWritten)
{
  bool again = false;
  return WriteSegmentsAgain(writer, count, countWritten, &again);
}

nsresult
Http2Session::Finish0RTT(bool aRestart, bool aAlpnChanged)
{
  MOZ_ASSERT(mAttemptingEarlyData);
  LOG3(("Http2Session::Finish0RTT %p aRestart=%d aAlpnChanged=%d", this,
        aRestart, aAlpnChanged));

  for (size_t i = 0; i < m0RTTStreams.Length(); ++i) {
    if (m0RTTStreams[i]) {
      m0RTTStreams[i]->Finish0RTT(aRestart, aAlpnChanged);
    }
  }

  if (aRestart) {
    // 0RTT failed
    if (aAlpnChanged) {
      // This is a slightly more involved case - we need to get all our streams/
      // transactions back in the queue so they can restart as http/1

      // These must be set this way to ensure we gracefully restart all streams
      mGoAwayID = 0;
      mCleanShutdown = true;

      // Close takes care of the rest of our work for us. The reason code here
      // doesn't matter, as we aren't actually going to send a GOAWAY frame, but
      // we use NS_ERROR_NET_RESET as it's closest to the truth.
      Close(NS_ERROR_NET_RESET);
    } else {
      // This is the easy case - early data failed, but we're speaking h2, so
      // we just need to rewind to the beginning of the preamble and try again.
      mOutputQueueSent = 0;
    }
  } else {
    // 0RTT succeeded
    // Make sure we look for any incoming data in repsonse to our early data.
    Unused << ResumeRecv();
  }

  mAttemptingEarlyData = false;
  m0RTTStreams.Clear();
  RealignOutputQueue();

  return NS_OK;
}

void
Http2Session::SetFastOpenStatus(uint8_t aStatus)
{
  LOG3(("Http2Session::SetFastOpenStatus %d [this=%p]",
        aStatus, this));

  for (size_t i = 0; i < m0RTTStreams.Length(); ++i) {
    if (m0RTTStreams[i]) {
      m0RTTStreams[i]->Transaction()->SetFastOpenStatus(aStatus);
    }
  }
}

nsresult
Http2Session::ProcessConnectedPush(Http2Stream *pushConnectedStream,
                                   nsAHttpSegmentWriter * writer,
                                   uint32_t count, uint32_t *countWritten)
{
  LOG3(("Http2Session::ProcessConnectedPush %p 0x%X\n",
        this, pushConnectedStream->StreamID()));
  mSegmentWriter = writer;
  nsresult rv = pushConnectedStream->WriteSegments(this, count, countWritten);
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
    Unused << ResumeRecv();
  }
  return rv;
}

nsresult
Http2Session::ProcessSlowConsumer(Http2Stream *slowConsumer,
                                  nsAHttpSegmentWriter * writer,
                                  uint32_t count, uint32_t *countWritten)
{
  LOG3(("Http2Session::ProcessSlowConsumer %p 0x%X\n",
        this, slowConsumer->StreamID()));
  mSegmentWriter = writer;
  nsresult rv = slowConsumer->WriteSegments(this, count, countWritten);
  mSegmentWriter = nullptr;
  LOG3(("Http2Session::ProcessSlowConsumer Writesegments %p 0x%X rv %" PRIX32 " %d\n",
        this, slowConsumer->StreamID(), static_cast<uint32_t>(rv), *countWritten));
  if (NS_SUCCEEDED(rv) && !*countWritten && slowConsumer->RecvdFin()) {
    rv = NS_BASE_STREAM_CLOSED;
  }

  if (NS_SUCCEEDED(rv) && (*countWritten > 0)) {
    // There have been buffered bytes successfully fed into the
    // formerly blocked consumer. Repeat until buffer empty or
    // consumer is blocked again.
    UpdateLocalRwin(slowConsumer, 0);
    ConnectSlowConsumer(slowConsumer);
  }

  if (rv == NS_BASE_STREAM_CLOSED) {
    CleanupStream(slowConsumer, NS_OK, CANCEL_ERROR);
    rv = NS_OK;
  }

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
        "unacked=%" PRIu64 " localWindow=%" PRId64 "\n",
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
  if (toack == 0) {
    // Ensure we never send an illegal 0 window update
    return;
  }

  // room for this packet needs to be ensured before calling this function
  char *packet = mOutputQueueBuffer.get() + mOutputQueueUsed;
  mOutputQueueUsed += kFrameHeaderBytes + 4;
  MOZ_ASSERT(mOutputQueueUsed <= mOutputQueueSize);

  CreateFrameHeader(packet, 4, FRAME_TYPE_WINDOW_UPDATE, 0, stream->StreamID());
  NetworkEndian::writeUint32(packet + kFrameHeaderBytes, toack);

  LogIO(this, stream, "Stream Window Update", packet, kFrameHeaderBytes + 4);
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
        "localWindow=%" PRId64 "\n", this, bytes, mLocalSessionWindow));

  // Don't necessarily ack every data packet. Only do it
  // after a significant amount of data.
  if ((mLocalSessionWindow > (mInitialRwin - kMinimumToAck)) &&
      (mLocalSessionWindow > kEmergencyWindowThreshold))
    return;

  // Only send max  bits of window updates at a time.
  uint64_t toack64 = mInitialRwin - mLocalSessionWindow;
  uint32_t toack = (toack64 <= 0x7fffffffU) ? toack64 : 0x7fffffffU;

  LOG3(("Http2Session::UpdateLocalSessionWindow Ack this=%p acksize=%u\n",
        this, toack));
  mLocalSessionWindow += toack;

  if (toack == 0) {
    // Ensure we never send an illegal 0 window update
    return;
  }

  // room for this packet needs to be ensured before calling this function
  char *packet = mOutputQueueBuffer.get() + mOutputQueueUsed;
  mOutputQueueUsed += kFrameHeaderBytes + 4;
  MOZ_ASSERT(mOutputQueueUsed <= mOutputQueueSize);

  CreateFrameHeader(packet, 4, FRAME_TYPE_WINDOW_UPDATE, 0, 0);
  NetworkEndian::writeUint32(packet + kFrameHeaderBytes, toack);

  LogIO(this, nullptr, "Session Window Update", packet, kFrameHeaderBytes + 4);
  // dont flush here, this write can commonly be coalesced with others
}

void
Http2Session::UpdateLocalRwin(Http2Stream *stream, uint32_t bytes)
{
  // make sure there is room for 2 window updates even though
  // we may not generate any.
  EnsureOutputBuffer(2 * (kFrameHeaderBytes + 4));

  UpdateLocalStreamWindow(stream, bytes);
  UpdateLocalSessionWindow(bytes);
  FlushOutputQueue();
}

void
Http2Session::Close(nsresult aReason)
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  if (mClosed)
    return;

  LOG3(("Http2Session::Close %p %" PRIX32, this, static_cast<uint32_t>(aReason)));

  mClosed = true;

  Shutdown();

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
  if (!mAttemptingEarlyData) {
    GenerateGoAway(goAwayReason);
  }
  mConnection = nullptr;
  mSegmentReader = nullptr;
  mSegmentWriter = nullptr;
}

nsHttpConnectionInfo *
Http2Session::ConnectionInfo()
{
  RefPtr<nsHttpConnectionInfo> ci;
  GetConnectionInfo(getter_AddRefs(ci));
  return ci.get();
}

void
Http2Session::CloseTransaction(nsAHttpTransaction *aTransaction,
                               nsresult aResult)
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG3(("Http2Session::CloseTransaction %p %p %" PRIx32, this, aTransaction,
        static_cast<uint32_t>(aResult)));

  // Generally this arrives as a cancel event from the connection manager.

  // need to find the stream and call CleanupStream() on it.
  Http2Stream *stream = mStreamTransactionHash.Get(aTransaction);
  if (!stream) {
    LOG3(("Http2Session::CloseTransaction %p %p %" PRIx32 " - not found.",
          this, aTransaction, static_cast<uint32_t>(aResult)));
    return;
  }
  LOG3(("Http2Session::CloseTransaction probably a cancel. "
        "this=%p, trans=%p, result=%" PRIx32 ", streamID=0x%X stream=%p",
        this, aTransaction, static_cast<uint32_t>(aResult), stream->StreamID(), stream));
  CleanupStream(stream, aResult, CANCEL_ERROR);
  nsresult rv = ResumeRecv();
  if (NS_FAILED(rv)) {
    LOG3(("Http2Session::CloseTransaction %p %p %x ResumeRecv returned %x",
          this, aTransaction, static_cast<uint32_t>(aResult),
          static_cast<uint32_t>(rv)));
  }
}

//-----------------------------------------------------------------------------
// nsAHttpSegmentReader
//-----------------------------------------------------------------------------

nsresult
Http2Session::OnReadSegment(const char *buf,
                            uint32_t count, uint32_t *countRead)
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
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
  if (mOutputQueueUsed && !mAttemptingEarlyData)
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
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  nsresult rv;

  if (!mSegmentWriter) {
    // the only way this could happen would be if Close() were called on the
    // stack with WriteSegments()
    return NS_ERROR_FAILURE;
  }

  if (mDownstreamState == NOT_USING_NETWORK ||
      mDownstreamState == BUFFERING_FRAME_HEADER ||
      mDownstreamState == DISCARDING_DATA_FRAME_PADDING) {
    return NS_BASE_STREAM_WOULD_BLOCK;
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
    if (mPaddingLength && (mInputFrameDataSize - mInputFrameDataRead <= mPaddingLength)) {
      // We are crossing from real HTTP data into the realm of padding. If
      // we've actually crossed the line, we need to munge countWritten for the
      // sake of goodness and sanity. No matter what, any future calls to
      // WriteSegments need to just discard data until we reach the end of this
      // frame.
      if (mInputFrameDataSize != mInputFrameDataRead) {
        // Only change state if we still have padding to read. If we don't do
        // this, we can end up hanging on frames that combine real data,
        // padding, and END_STREAM (see bug 1019921)
        ChangeDownstreamState(DISCARDING_DATA_FRAME_PADDING);
      }
      uint32_t paddingRead = mPaddingLength - (mInputFrameDataSize - mInputFrameDataRead);
      LOG3(("Http2Session::OnWriteSegment %p stream 0x%X len=%d read=%d "
            "crossed from HTTP data into padding (%d of %d) countWritten=%d",
            this, mInputFrameID, mInputFrameDataSize, mInputFrameDataRead,
            paddingRead, mPaddingLength, *countWritten));
      *countWritten -= paddingRead;
      LOG3(("Http2Session::OnWriteSegment %p stream 0x%X new countWritten=%d",
            this, mInputFrameID, *countWritten));
    }

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

  MOZ_ASSERT(false);
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
  mInputFrameDataStream->SetResponseIsComplete();
  mNeedsCleanup = mInputFrameDataStream;
  ResetDownstreamState();
}

void
Http2Session::ConnectPushedStream(Http2Stream *stream)
{
  mPushesReadyForRead.Push(stream);
  Unused << ForceRecv();
}

void
Http2Session::ConnectSlowConsumer(Http2Stream *stream)
{
  LOG3(("Http2Session::ConnectSlowConsumer %p 0x%X\n",
        this, stream->StreamID()));
  mSlowConsumersReadyForRead.Push(stream);
  Unused << ForceRecv();
}

uint32_t
Http2Session::FindTunnelCount(nsHttpConnectionInfo *aConnInfo)
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  uint32_t rv = 0;
  mTunnelHash.Get(aConnInfo->HashKey(), &rv);
  return rv;
}

void
Http2Session::RegisterTunnel(Http2Stream *aTunnel)
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  nsHttpConnectionInfo *ci = aTunnel->Transaction()->ConnectionInfo();
  uint32_t newcount = FindTunnelCount(ci) + 1;
  mTunnelHash.Remove(ci->HashKey());
  mTunnelHash.Put(ci->HashKey(), newcount);
  LOG3(("Http2Stream::RegisterTunnel %p stream=%p tunnels=%d [%s]",
        this, aTunnel, newcount, ci->HashKey().get()));
}

void
Http2Session::UnRegisterTunnel(Http2Stream *aTunnel)
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  nsHttpConnectionInfo *ci = aTunnel->Transaction()->ConnectionInfo();
  MOZ_ASSERT(FindTunnelCount(ci));
  uint32_t newcount = FindTunnelCount(ci) - 1;
  mTunnelHash.Remove(ci->HashKey());
  if (newcount) {
    mTunnelHash.Put(ci->HashKey(), newcount);
  }
  LOG3(("Http2Session::UnRegisterTunnel %p stream=%p tunnels=%d [%s]",
        this, aTunnel, newcount, ci->HashKey().get()));
}

void
Http2Session::CreateTunnel(nsHttpTransaction *trans,
                           nsHttpConnectionInfo *ci,
                           nsIInterfaceRequestor *aCallbacks)
{
  LOG(("Http2Session::CreateTunnel %p %p make new tunnel\n", this, trans));
  // The connect transaction will hold onto the underlying http
  // transaction so that an auth created by the connect can be mappped
  // to the correct security callbacks

  RefPtr<SpdyConnectTransaction> connectTrans =
    new SpdyConnectTransaction(ci, aCallbacks, trans->Caps(), trans, this);
  DebugOnly<bool> rv = AddStream(connectTrans, nsISupportsPriority::PRIORITY_NORMAL, false, nullptr);
  MOZ_ASSERT(rv);
  Http2Stream *tunnel = mStreamTransactionHash.Get(connectTrans);
  MOZ_ASSERT(tunnel);
  RegisterTunnel(tunnel);
}

void
Http2Session::DispatchOnTunnel(nsAHttpTransaction *aHttpTransaction,
                               nsIInterfaceRequestor *aCallbacks)
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  nsHttpTransaction *trans = aHttpTransaction->QueryHttpTransaction();
  nsHttpConnectionInfo *ci = aHttpTransaction->ConnectionInfo();
  MOZ_ASSERT(trans);

  LOG3(("Http2Session::DispatchOnTunnel %p trans=%p", this, trans));

  aHttpTransaction->SetConnection(nullptr);

  // this transaction has done its work of setting up a tunnel, let
  // the connection manager queue it if necessary
  trans->SetTunnelProvider(this);
  trans->EnableKeepAlive();

  if (FindTunnelCount(ci) < gHttpHandler->MaxConnectionsPerOrigin()) {
    LOG3(("Http2Session::DispatchOnTunnel %p create on new tunnel %s",
          this, ci->HashKey().get()));
    CreateTunnel(trans, ci, aCallbacks);
  } else {
    // requeue it. The connection manager is responsible for actually putting
    // this on the tunnel connection with the specific ci. If that can't
    // happen the cmgr checks with us via MaybeReTunnel() to see if it should
    // make a new tunnel or just wait longer.
    LOG3(("Http2Session::DispatchOnTunnel %p trans=%p queue in connection manager",
          this, trans));
    nsresult rv = gHttpHandler->InitiateTransaction(trans, trans->Priority());
    if (NS_FAILED(rv)) {
      LOG3(("Http2Session::DispatchOnTunnel %p trans=%p failed to initiate "
            "transaction (%08x)", this, trans, static_cast<uint32_t>(rv)));
    }
  }
}

// From ASpdySession
bool
Http2Session::MaybeReTunnel(nsAHttpTransaction *aHttpTransaction)
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  nsHttpTransaction *trans = aHttpTransaction->QueryHttpTransaction();
  LOG(("Http2Session::MaybeReTunnel %p trans=%p\n", this, trans));
  if (!trans || trans->TunnelProvider() != this) {
    // this isn't really one of our transactions.
    return false;
  }

  if (mClosed || mShouldGoAway) {
    LOG(("Http2Session::MaybeReTunnel %p %p session closed - requeue\n", this, trans));
    trans->SetTunnelProvider(nullptr);
    nsresult rv = gHttpHandler->InitiateTransaction(trans, trans->Priority());
    if (NS_FAILED(rv)) {
      LOG3(("Http2Session::MaybeReTunnel %p trans=%p failed to initiate "
            "transaction (%08x)", this, trans, static_cast<uint32_t>(rv)));
    }
    return true;
  }

  nsHttpConnectionInfo *ci = aHttpTransaction->ConnectionInfo();
  LOG(("Http2Session:MaybeReTunnel %p %p count=%d limit %d\n",
       this, trans, FindTunnelCount(ci), gHttpHandler->MaxConnectionsPerOrigin()));
  if (FindTunnelCount(ci) >= gHttpHandler->MaxConnectionsPerOrigin()) {
    // patience - a tunnel will open up.
    return false;
  }

  LOG(("Http2Session::MaybeReTunnel %p %p make new tunnel\n", this, trans));
  CreateTunnel(trans, ci, trans->SecurityCallbacks());
  return true;
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

bool // static
Http2Session::ALPNCallback(nsISupports *securityInfo)
{
  if (!gHttpHandler->IsH2MandatorySuiteEnabled()) {
    LOG3(("Http2Session::ALPNCallback Mandatory Cipher Suite Unavailable\n"));
    return false;
  }

  nsCOMPtr<nsISSLSocketControl> ssl = do_QueryInterface(securityInfo);
  LOG3(("Http2Session::ALPNCallback sslsocketcontrol=%p\n", ssl.get()));
  if (ssl) {
    int16_t version = ssl->GetSSLVersionOffered();
    LOG3(("Http2Session::ALPNCallback version=%x\n", version));
    if (version >= nsISSLSocketControl::TLS_VERSION_1_2) {
      return true;
    }
  }
  return false;
}

nsresult
Http2Session::ConfirmTLSProfile()
{
  if (mTLSProfileConfirmed) {
    return NS_OK;
  }

  LOG3(("Http2Session::ConfirmTLSProfile %p mConnection=%p\n",
        this, mConnection.get()));

  if (mAttemptingEarlyData) {
    LOG3(("Http2Session::ConfirmTLSProfile %p temporarily passing due to early data\n", this));
    return NS_OK;
  }

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
  if (version < nsISSLSocketControl::TLS_VERSION_1_2) {
    LOG3(("Http2Session::ConfirmTLSProfile %p FAILED due to lack of TLS1.2\n", this));
    RETURN_SESSION_ERROR(this, INADEQUATE_SECURITY);
  }

  uint16_t kea = ssl->GetKEAUsed();
  if (kea != ssl_kea_dh && kea != ssl_kea_ecdh) {
    LOG3(("Http2Session::ConfirmTLSProfile %p FAILED due to invalid KEA %d\n",
          this, kea));
    RETURN_SESSION_ERROR(this, INADEQUATE_SECURITY);
  }

  uint32_t keybits = ssl->GetKEAKeyBits();
  if (kea == ssl_kea_dh && keybits < 2048) {
    LOG3(("Http2Session::ConfirmTLSProfile %p FAILED due to DH %d < 2048\n",
          this, keybits));
    RETURN_SESSION_ERROR(this, INADEQUATE_SECURITY);
  } else if (kea == ssl_kea_ecdh && keybits < 224) { // see rfc7540 9.2.1.
    LOG3(("Http2Session::ConfirmTLSProfile %p FAILED due to ECDH %d < 224\n",
          this, keybits));
    RETURN_SESSION_ERROR(this, INADEQUATE_SECURITY);
  }

  int16_t macAlgorithm = ssl->GetMACAlgorithmUsed();
  LOG3(("Http2Session::ConfirmTLSProfile %p MAC Algortihm (aead==6) %d\n",
        this, macAlgorithm));
  if (macAlgorithm != nsISSLSocketControl::SSL_MAC_AEAD) {
    LOG3(("Http2Session::ConfirmTLSProfile %p FAILED due to lack of AEAD\n", this));
    RETURN_SESSION_ERROR(this, INADEQUATE_SECURITY);
  }

  /* We are required to send SNI. We do that already, so no check is done
   * here to make sure we did. */

  /* We really should check to ensure TLS compression isn't enabled on
   * this connection. However, we never enable TLS compression on our end,
   * anyway, so it'll never be on. All the same, see https://bugzil.la/965881
   * for the possibility for an interface to ensure it never gets turned on. */

  mTLSProfileConfirmed = true;
  return NS_OK;
}


//-----------------------------------------------------------------------------
// Modified methods of nsAHttpConnection
//-----------------------------------------------------------------------------

void
Http2Session::TransactionHasDataToWrite(nsAHttpTransaction *caller)
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
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

  if (!mClosed) {
    mReadyForWrite.Push(stream);
    SetWriteCallbacks();
  } else {
    LOG3(("Http2Session::TransactionHasDataToWrite %p closed so not setting Ready4Write\n",
          this));
  }

  // NSPR poll will not poll the network if there are non system PR_FileDesc's
  // that are ready - so we can get into a deadlock waiting for the system IO
  // to come back here if we don't force the send loop manually.
  Unused << ForceSend();
}

void
Http2Session::TransactionHasDataToRecv(nsAHttpTransaction *caller)
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG3(("Http2Session::TransactionHasDataToRecv %p trans=%p", this, caller));

  // a signal from the http transaction to the connection that it will consume more
  Http2Stream *stream = mStreamTransactionHash.Get(caller);
  if (!stream || !VerifyStream(stream)) {
    LOG3(("Http2Session::TransactionHasDataToRecv %p caller %p not found",
          this, caller));
    return;
  }

  LOG3(("Http2Session::TransactionHasDataToRecv %p ID is 0x%X\n",
        this, stream->StreamID()));
  ConnectSlowConsumer(stream);
}

void
Http2Session::TransactionHasDataToWrite(Http2Stream *stream)
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG3(("Http2Session::TransactionHasDataToWrite %p stream=%p ID=0x%x",
        this, stream, stream->StreamID()));

  mReadyForWrite.Push(stream);
  SetWriteCallbacks();
  Unused << ForceSend();
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

already_AddRefed<nsHttpConnection>
Http2Session::TakeHttpConnection()
{
  MOZ_ASSERT(false, "TakeHttpConnection of Http2Session");
  return nullptr;
}

already_AddRefed<nsHttpConnection>
Http2Session::HttpConnection()
{
  if (mConnection) {
    return mConnection->HttpConnection();
  }
  return nullptr;
}

void
Http2Session::GetSecurityCallbacks(nsIInterfaceRequestor **aOut)
{
  *aOut = nullptr;
}

//-----------------------------------------------------------------------------
// unused methods of nsAHttpTransaction
// We can be sure of this because Http2Session is only constructed in
// nsHttpConnection and is never passed out of that object or a TLSFilterTransaction
// TLS tunnel
//-----------------------------------------------------------------------------

void
Http2Session::SetConnection(nsAHttpConnection *)
{
  // This is unexpected
  MOZ_ASSERT(false, "Http2Session::SetConnection()");
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

nsHttpRequestHead *
Http2Session::RequestHead()
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
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

nsresult
Http2Session::TakeSubTransactions(
  nsTArray<RefPtr<nsAHttpTransaction> > &outTransactions)
{
  // Generally this cannot be done with http/2 as transactions are
  // started right away.

  LOG3(("Http2Session::TakeSubTransactions %p\n", this));

  if (mConcurrentHighWater > 0)
    return NS_ERROR_ALREADY_OPENED;

  LOG3(("   taking %d\n", mStreamTransactionHash.Count()));

  for (auto iter = mStreamTransactionHash.Iter(); !iter.Done(); iter.Next()) {
    outTransactions.AppendElement(iter.Key());

    // Removing the stream from the hash will delete the stream and drop the
    // transaction reference the hash held.
    iter.Remove();
  }
  return NS_OK;
}

//-----------------------------------------------------------------------------
// Pass through methods of nsAHttpConnection
//-----------------------------------------------------------------------------

nsAHttpConnection *
Http2Session::Connection()
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
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

void
Http2Session::SendPing()
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  if (mPreviousUsed) {
    // alredy in progress, get out
    return;
  }

  mPingSentEpoch = PR_IntervalNow();
  if (!mPingSentEpoch) {
    mPingSentEpoch = 1; // avoid the 0 sentinel value
  }
  if (!mPingThreshold ||
      (mPingThreshold > gHttpHandler->NetworkChangedTimeout())) {
    mPreviousPingThreshold = mPingThreshold;
    mPreviousUsed = true;
    mPingThreshold = gHttpHandler->NetworkChangedTimeout();
  }
  GeneratePing(false);
  Unused << ResumeRecv();
}

bool
Http2Session::TestOriginFrame(const nsACString &hostname, int32_t port)
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  MOZ_ASSERT(mOriginFrameActivated);

  nsAutoCString key(hostname);
  key.Append (':');
  key.AppendInt(port);
  bool rv = mOriginFrame.Get(key);
  LOG3(("TestOriginFrame() hash.get %p %s %d\n", this, key.get(), rv));
  if (!rv && ConnectionInfo()) {
    // the SNI is also implicitly in this list, so consult that too
    nsHttpConnectionInfo *ci = ConnectionInfo();
    rv = nsCString(hostname).EqualsIgnoreCase(ci->Origin()) && (port == ci->OriginPort());
    LOG3(("TestOriginFrame() %p sni test %d\n", this, rv));
  }
  return rv;
}

bool
Http2Session::TestJoinConnection(const nsACString &hostname, int32_t port)
{
  return RealJoinConnection(hostname, port, true);
}

bool
Http2Session::JoinConnection(const nsACString &hostname, int32_t port)
{
  return RealJoinConnection(hostname, port, false);
}

bool
Http2Session::RealJoinConnection(const nsACString &hostname, int32_t port,
                                 bool justKidding)
{
  if (!mConnection || mClosed || mShouldGoAway) {
    return false;
  }

  nsHttpConnectionInfo *ci = ConnectionInfo();
  if (nsCString(hostname).EqualsIgnoreCase(ci->Origin()) && (port == ci->OriginPort())) {
    return true;
 }

  if (!mReceivedSettings) {
    return false;
  }

  if (mOriginFrameActivated) {
    bool originFrameResult = TestOriginFrame(hostname, port);
    if (!originFrameResult) {
      return false;
    }
  } else {
    LOG3(("JoinConnection %p no origin frame check used.\n", this));
  }

  nsAutoCString key(hostname);
  key.Append(':');
  key.Append(justKidding ? 'k' : '.');
  key.AppendInt(port);
  bool cachedResult;
  if (mJoinConnectionCache.Get(key, &cachedResult)) {
    LOG(("joinconnection [%p %s] %s result=%d cache\n",
         this, ConnectionInfo()->HashKey().get(), key.get(),
         cachedResult));
    return cachedResult;
  }

  nsresult rv;
  bool isJoined = false;

  nsCOMPtr<nsISupports> securityInfo;
  nsCOMPtr<nsISSLSocketControl> sslSocketControl;

  mConnection->GetSecurityInfo(getter_AddRefs(securityInfo));
  sslSocketControl = do_QueryInterface(securityInfo, &rv);
  if (NS_FAILED(rv) || !sslSocketControl) {
    return false;
  }

  // try all the coalescable versions we support.
  const SpdyInformation *info = gHttpHandler->SpdyInfo();
  static_assert(SpdyInformation::kCount == 1, "assume 1 alpn version");
  bool joinedReturn = false;
  if (info->ProtocolEnabled(0)) {
    if (justKidding) {
      rv = sslSocketControl->TestJoinConnection(info->VersionString[0],
                                                hostname, port, &isJoined);
    } else {
      rv = sslSocketControl->JoinConnection(info->VersionString[0],
                                            hostname, port, &isJoined);
    }
    if (NS_SUCCEEDED(rv) && isJoined) {
      joinedReturn = true;
    }
  }

  LOG(("joinconnection [%p %s] %s result=%d lookup\n",
       this, ConnectionInfo()->HashKey().get(), key.get(), joinedReturn));
  mJoinConnectionCache.Put(key, joinedReturn);
  if (!justKidding) {
    // cache a kidding entry too as this one is good for both
    nsAutoCString key2(hostname);
    key2.Append(':');
    key2.Append('k');
    key2.AppendInt(port);
    if (!mJoinConnectionCache.Get(key2)) {
      mJoinConnectionCache.Put(key2, joinedReturn);
    }
  }
  return joinedReturn;
}

} // namespace net
} // namespace mozilla
