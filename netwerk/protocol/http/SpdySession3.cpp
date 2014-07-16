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

#include "mozilla/Endian.h"
#include "mozilla/Telemetry.h"
#include "nsHttp.h"
#include "nsHttpHandler.h"
#include "nsILoadGroup.h"
#include "prprf.h"
#include "SpdyPush3.h"
#include "SpdySession3.h"
#include "SpdyStream3.h"
#include "PSpdyPush.h"
#include "SpdyZlibReporter.h"
#include "TunnelUtils.h"

#include <algorithm>

#ifdef DEBUG
// defined by the socket transport service while active
extern PRThread *gSocketThread;
#endif

namespace mozilla {
namespace net {

// SpdySession3 has multiple inheritance of things that implement
// nsISupports, so this magic is taken from nsHttpPipeline that
// implements some of the same abstract classes.
NS_IMPL_ADDREF(SpdySession3)
NS_IMPL_RELEASE(SpdySession3)
NS_INTERFACE_MAP_BEGIN(SpdySession3)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsAHttpConnection)
NS_INTERFACE_MAP_END

SpdySession3::SpdySession3(nsISocketTransport *aSocketTransport)
  : mSocketTransport(aSocketTransport)
  , mSegmentReader(nullptr)
  , mSegmentWriter(nullptr)
  , mNextStreamID(1)
  , mConcurrentHighWater(0)
  , mDownstreamState(BUFFERING_FRAME_HEADER)
  , mInputFrameBufferSize(kDefaultBufferSize)
  , mInputFrameBufferUsed(0)
  , mInputFrameDataLast(false)
  , mInputFrameDataStream(nullptr)
  , mNeedsCleanup(nullptr)
  , mShouldGoAway(false)
  , mClosed(false)
  , mCleanShutdown(false)
  , mDataPending(false)
  , mGoAwayID(0)
  , mMaxConcurrent(kDefaultMaxConcurrent)
  , mConcurrent(0)
  , mServerPushedResources(0)
  , mServerInitialWindow(kDefaultServerRwin)
  , mOutputQueueSize(kDefaultQueueSize)
  , mOutputQueueUsed(0)
  , mOutputQueueSent(0)
  , mLastReadEpoch(PR_IntervalNow())
  , mPingSentEpoch(0)
  , mNextPingID(1)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  static uint64_t sSerial;
  mSerial = ++sSerial;

  LOG3(("SpdySession3::SpdySession3 %p serial=0x%X\n", this, mSerial));

  mInputFrameBuffer = new char[mInputFrameBufferSize];
  mOutputQueueBuffer = new char[mOutputQueueSize];
  zlibInit();

  mPushAllowance = gHttpHandler->SpdyPushAllowance();
  mSendingChunkSize = gHttpHandler->SpdySendingChunkSize();
  GenerateSettings();

  mLastDataReadEpoch = mLastReadEpoch;

  mPingThreshold = gHttpHandler->SpdyPingThreshold();
}

PLDHashOperator
SpdySession3::ShutdownEnumerator(nsAHttpTransaction *key,
                                 nsAutoPtr<SpdyStream3> &stream,
                                 void *closure)
{
  SpdySession3 *self = static_cast<SpdySession3 *>(closure);

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
SpdySession3::GoAwayEnumerator(nsAHttpTransaction *key,
                               nsAutoPtr<SpdyStream3> &stream,
                               void *closure)
{
  SpdySession3 *self = static_cast<SpdySession3 *>(closure);

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

SpdySession3::~SpdySession3()
{
  LOG3(("SpdySession3::~SpdySession3 %p mDownstreamState=%X",
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
SpdySession3::LogIO(SpdySession3 *self, SpdyStream3 *stream, const char *label,
                    const char *data, uint32_t datalen)
{
  if (!LOG4_ENABLED())
    return;

  LOG4(("SpdySession3::LogIO %p stream=%p id=0x%X [%s]",
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

bool
SpdySession3::RoomForMoreConcurrent()
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  return (mConcurrent < mMaxConcurrent);
}

bool
SpdySession3::RoomForMoreStreams()
{
  if (mNextStreamID + mStreamTransactionHash.Count() * 2 > kMaxStreamID)
    return false;

  return !mShouldGoAway;
}

PRIntervalTime
SpdySession3::IdleTime()
{
  return PR_IntervalNow() - mLastDataReadEpoch;
}

uint32_t
SpdySession3::ReadTimeoutTick(PRIntervalTime now)
{
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
    MOZ_ASSERT(mNextPingID & 1, "Ping Counter Not Odd");

    LOG(("SpdySession3::ReadTimeoutTick %p delta since last read %ds\n",
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
      LOG(("SpdySession3::ReadTimeoutTick %p handle outstanding ping\n"));
      if ((now - mPingSentEpoch) >= gHttpHandler->SpdyPingTimeout()) {
        LOG(("SpdySession3::ReadTimeoutTick %p Ping Timer Exhaustion\n",
             this));
        mPingSentEpoch = 0;
        Close(NS_ERROR_NET_TIMEOUT);
        return UINT32_MAX;
      }
      return 1; // run the tick aggressively while ping is outstanding
    }

    LOG(("SpdySession3::ReadTimeoutTick %p generating ping 0x%X\n",
         this, mNextPingID));

    if (mNextPingID == 0xffffffff) {
      LOG(("SpdySession3::ReadTimeoutTick %p cannot form ping - ids exhausted\n",
           this));
      return UINT32_MAX;
    }

    mPingSentEpoch = PR_IntervalNow();
    if (!mPingSentEpoch)
      mPingSentEpoch = 1; // avoid the 0 sentinel value
    GeneratePing(mNextPingID);
    mNextPingID += 2;
    ResumeRecv(); // read the ping reply

    // Check for orphaned push streams. This looks expensive, but generally the
    // list is empty.
    SpdyPushedStream3 *deleteMe;
    TimeStamp timestampNow;
    do {
      deleteMe = nullptr;

      for (uint32_t index = mPushedStreams.Length();
           index > 0 ; --index) {
        SpdyPushedStream3 *pushedStream = mPushedStreams[index - 1];

        if (timestampNow.IsNull())
          timestampNow = TimeStamp::Now(); // lazy initializer

        // if spdy finished, but not connected, and its been like that for too long..
        // cleanup the stream..
        if (pushedStream->IsOrphaned(timestampNow))
        {
          LOG3(("SpdySession3 Timeout Pushed Stream %p 0x%X\n",
                this, pushedStream->StreamID()));
          deleteMe = pushedStream;
          break; // don't CleanupStream() while iterating this vector
        }
      }
      if (deleteMe)
        CleanupStream(deleteMe, NS_ERROR_ABORT, RST_CANCEL);

    } while (deleteMe);

    if (mNextPingID == 0xffffffff) {
      LOG(("SpdySession3::ReadTimeoutTick %p "
           "ping ids exhausted marking goaway\n", this));
      mShouldGoAway = true;
    }
    return 1; // run the tick aggressively while ping is outstanding
}

uint32_t
SpdySession3::RegisterStreamID(SpdyStream3 *stream, uint32_t aNewID)
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

  LOG3(("SpdySession3::RegisterStreamID session=%p stream=%p id=0x%X "
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
SpdySession3::AddStream(nsAHttpTransaction *aHttpTransaction,
                        int32_t aPriority,
                        bool aUseTunnel,
                        nsIInterfaceRequestor *aCallbacks)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  // integrity check
  if (mStreamTransactionHash.Get(aHttpTransaction)) {
    LOG3(("   New transaction already present\n"));
    MOZ_ASSERT(false, "AddStream duplicate transaction pointer");
    return false;
  }

  if (!mConnection) {
    mConnection = aHttpTransaction->Connection();
  }

  aHttpTransaction->SetConnection(this);

  if (aUseTunnel) {
    LOG3(("SpdySession3::AddStream session=%p trans=%p OnTunnel",
          this, aHttpTransaction));
    DispatchOnTunnel(aHttpTransaction, aCallbacks);
    return true;
  }

  SpdyStream3 *stream = new SpdyStream3(aHttpTransaction, this, aPriority);

  LOG3(("SpdySession3::AddStream session=%p stream=%p serial=%u "
        "NextID=0x%X (tentative)", this, stream, mSerial, mNextStreamID));

  mStreamTransactionHash.Put(aHttpTransaction, stream);

  if (RoomForMoreConcurrent()) {
    LOG3(("SpdySession3::AddStream %p stream %p activated immediately.",
          this, stream));
    ActivateStream(stream);
  }
  else {
    LOG3(("SpdySession3::AddStream %p stream %p queued.", this, stream));
    mQueuedStreams.Push(stream);
  }

  if (!(aHttpTransaction->Caps() & NS_HTTP_ALLOW_KEEPALIVE)) {
    LOG3(("SpdySession3::AddStream %p transaction %p forces keep-alive off.\n",
          this, aHttpTransaction));
    DontReuse();
  }
  return true;
}

void
SpdySession3::ActivateStream(SpdyStream3 *stream)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  MOZ_ASSERT(!stream->StreamID() || (stream->StreamID() & 1),
             "Do not activate pushed streams");

  ++mConcurrent;
  if (mConcurrent > mConcurrentHighWater)
    mConcurrentHighWater = mConcurrent;
  LOG3(("SpdySession3::AddStream %p activating stream %p Currently %d "
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
SpdySession3::ProcessPending()
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  while (RoomForMoreConcurrent()) {
    SpdyStream3 *stream = static_cast<SpdyStream3 *>(mQueuedStreams.PopFront());
    if (!stream)
      return;
    LOG3(("SpdySession3::ProcessPending %p stream %p activated from queue.",
          this, stream));
    ActivateStream(stream);
  }
}

nsresult
SpdySession3::NetworkRead(nsAHttpSegmentWriter *writer, char *buf,
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
SpdySession3::SetWriteCallbacks()
{
  if (mConnection && (GetWriteQueueSize() || mOutputQueueUsed))
      mConnection->ResumeSend();
}

void
SpdySession3::RealignOutputQueue()
{
  mOutputQueueUsed -= mOutputQueueSent;
  memmove(mOutputQueueBuffer.get(),
          mOutputQueueBuffer.get() + mOutputQueueSent,
          mOutputQueueUsed);
  mOutputQueueSent = 0;
}

void
SpdySession3::FlushOutputQueue()
{
  if (!mSegmentReader || !mOutputQueueUsed)
    return;

  nsresult rv;
  uint32_t countRead;
  uint32_t avail = mOutputQueueUsed - mOutputQueueSent;

  rv = mSegmentReader->
    OnReadSegment(mOutputQueueBuffer.get() + mOutputQueueSent, avail,
                                     &countRead);
  LOG3(("SpdySession3::FlushOutputQueue %p sz=%d rv=%x actual=%d",
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
SpdySession3::DontReuse()
{
  mShouldGoAway = true;
  if (!mStreamTransactionHash.Count())
    Close(NS_OK);
}

uint32_t
SpdySession3::GetWriteQueueSize()
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  return mReadyForWrite.GetSize();
}

void
SpdySession3::ChangeDownstreamState(enum stateType newState)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  LOG3(("SpdySession3::ChangeDownstreamState() %p from %X to %X",
        this, mDownstreamState, newState));
  mDownstreamState = newState;
}

void
SpdySession3::ResetDownstreamState()
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  LOG3(("SpdySession3::ResetDownstreamState() %p", this));
  ChangeDownstreamState(BUFFERING_FRAME_HEADER);

  if (mInputFrameDataLast && mInputFrameDataStream) {
    mInputFrameDataLast = false;
    if (!mInputFrameDataStream->RecvdFin()) {
      LOG3(("  SetRecvdFin id=0x%x\n", mInputFrameDataStream->StreamID()));
      mInputFrameDataStream->SetRecvdFin(true);
      DecrementConcurrent(mInputFrameDataStream);
    }
  }
  mInputFrameBufferUsed = 0;
  mInputFrameDataStream = nullptr;
}

void
SpdySession3::DecrementConcurrent(SpdyStream3 *aStream)
{
  uint32_t id = aStream->StreamID();

  if (id && !(id & 0x1))
    return; // pushed streams aren't counted in concurrent limit

  MOZ_ASSERT(mConcurrent);
  --mConcurrent;
  LOG3(("DecrementConcurrent %p id=0x%X concurrent=%d\n",
        this, id, mConcurrent));
  ProcessPending();
}

void
SpdySession3::zlibInit()
{
  mDownstreamZlib.zalloc = SpdyZlibReporter::Alloc;
  mDownstreamZlib.zfree = SpdyZlibReporter::Free;
  mDownstreamZlib.opaque = Z_NULL;

  inflateInit(&mDownstreamZlib);

  mUpstreamZlib.zalloc = SpdyZlibReporter::Alloc;
  mUpstreamZlib.zfree = SpdyZlibReporter::Free;
  mUpstreamZlib.opaque = Z_NULL;

  // mixing carte blanche compression with tls subjects us to traffic
  // analysis attacks
  deflateInit(&mUpstreamZlib, Z_NO_COMPRESSION);
  deflateSetDictionary(&mUpstreamZlib,
                       SpdyStream3::kDictionary,
                       sizeof(SpdyStream3::kDictionary));
}

// Need to decompress some data in order to keep the compression
// context correct, but we really don't care what the result is
nsresult
SpdySession3::UncompressAndDiscard(uint32_t offset,
                                   uint32_t blockLen)
{
  char *blockStart = mInputFrameBuffer + offset;
  unsigned char trash[2048];
  mDownstreamZlib.avail_in = blockLen;
  mDownstreamZlib.next_in = reinterpret_cast<unsigned char *>(blockStart);
  bool triedDictionary = false;

  do {
    mDownstreamZlib.next_out = trash;
    mDownstreamZlib.avail_out = sizeof(trash);
    int zlib_rv = inflate(&mDownstreamZlib, Z_NO_FLUSH);

    if (zlib_rv == Z_NEED_DICT) {
      if (triedDictionary) {
        LOG3(("SpdySession3::UncompressAndDiscard %p Dictionary Error\n", this));
        return NS_ERROR_ILLEGAL_VALUE;
      }

      triedDictionary = true;
      inflateSetDictionary(&mDownstreamZlib, SpdyStream3::kDictionary,
                           sizeof(SpdyStream3::kDictionary));
    }

    if (zlib_rv == Z_DATA_ERROR)
      return NS_ERROR_ILLEGAL_VALUE;

    if (zlib_rv == Z_MEM_ERROR)
      return NS_ERROR_FAILURE;
  }
  while (mDownstreamZlib.avail_in);
  return NS_OK;
}

void
SpdySession3::GeneratePing(uint32_t aID)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  LOG3(("SpdySession3::GeneratePing %p 0x%X\n", this, aID));

  EnsureBuffer(mOutputQueueBuffer, mOutputQueueUsed + 12,
               mOutputQueueUsed, mOutputQueueSize);
  char *packet = mOutputQueueBuffer.get() + mOutputQueueUsed;
  mOutputQueueUsed += 12;

  packet[0] = kFlag_Control;
  packet[1] = kVersion;
  packet[2] = 0;
  packet[3] = CONTROL_TYPE_PING;
  packet[4] = 0;                                  /* flags */
  packet[5] = 0;
  packet[6] = 0;
  packet[7] = 4;                                  /* length */

  NetworkEndian::writeUint32(packet + 8, aID);

  LogIO(this, nullptr, "Generate Ping", packet, 12);
  FlushOutputQueue();
}

void
SpdySession3::GenerateRstStream(uint32_t aStatusCode, uint32_t aID)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  LOG3(("SpdySession3::GenerateRst %p 0x%X %d\n", this, aID, aStatusCode));

  EnsureBuffer(mOutputQueueBuffer, mOutputQueueUsed + 16,
               mOutputQueueUsed, mOutputQueueSize);
  char *packet = mOutputQueueBuffer.get() + mOutputQueueUsed;
  mOutputQueueUsed += 16;

  packet[0] = kFlag_Control;
  packet[1] = kVersion;
  packet[2] = 0;
  packet[3] = CONTROL_TYPE_RST_STREAM;
  packet[4] = 0;                                  /* flags */
  packet[5] = 0;
  packet[6] = 0;
  packet[7] = 8;                                  /* length */

  NetworkEndian::writeUint32(packet + 8, aID);
  NetworkEndian::writeUint32(packet + 12, aStatusCode);

  LogIO(this, nullptr, "Generate Reset", packet, 16);
  FlushOutputQueue();
}

void
SpdySession3::GenerateGoAway(uint32_t aStatusCode)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  LOG3(("SpdySession3::GenerateGoAway %p code=%X\n", this, aStatusCode));

  EnsureBuffer(mOutputQueueBuffer, mOutputQueueUsed + 16,
               mOutputQueueUsed, mOutputQueueSize);
  char *packet = mOutputQueueBuffer.get() + mOutputQueueUsed;
  mOutputQueueUsed += 16;

  memset(packet, 0, 16);
  packet[0] = kFlag_Control;
  packet[1] = kVersion;
  packet[3] = CONTROL_TYPE_GOAWAY;
  packet[7] = 8;                                  /* data length */

  // last-good-stream-id are bytes 8-11, when we accept server push this will
  // need to be set non zero

  // bytes 12-15 are the status code.
  NetworkEndian::writeUint32(packet + 12, aStatusCode);

  LogIO(this, nullptr, "Generate GoAway", packet, 16);
  FlushOutputQueue();
}

void
SpdySession3::GenerateSettings()
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  LOG3(("SpdySession3::GenerateSettings %p\n", this));

  static const uint32_t maxDataLen = 4 + 3 * 8; // sized for 3 settings
  EnsureBuffer(mOutputQueueBuffer, mOutputQueueUsed + 8 + maxDataLen,
               mOutputQueueUsed, mOutputQueueSize);
  char *packet = mOutputQueueBuffer.get() + mOutputQueueUsed;

  memset(packet, 0, 8 + maxDataLen);
  packet[0] = kFlag_Control;
  packet[1] = kVersion;
  packet[3] = CONTROL_TYPE_SETTINGS;

  uint8_t numberOfEntries = 0;

  // entries need to be listed in order by ID
  // 1st entry is bytes 12 to 19
  // 2nd entry is bytes 20 to 27
  // 3rd entry is bytes 28 to 35

  if (!gHttpHandler->AllowPush()) {
  // announcing that we accept 0 incoming streams is done to
  // disable server push
    packet[15 + 8 * numberOfEntries] = SETTINGS_TYPE_MAX_CONCURRENT;
    // The value portion of the setting pair is already initialized to 0
    numberOfEntries++;
  }

  nsRefPtr<nsHttpConnectionInfo> ci;
  uint32_t cwnd = 0;
  GetConnectionInfo(getter_AddRefs(ci));
  if (ci)
    cwnd = gHttpHandler->ConnMgr()->GetSpdyCWNDSetting(ci);
  if (cwnd) {
    packet[12 + 8 * numberOfEntries] = PERSISTED_VALUE;
    packet[15 + 8 * numberOfEntries] = SETTINGS_TYPE_CWND;
    LOG(("SpdySession3::GenerateSettings %p sending CWND %u\n", this, cwnd));
    NetworkEndian::writeUint32(packet + 16 + 8 * numberOfEntries, cwnd);
    numberOfEntries++;
  }

  // Advertise the Push RWIN and on each client SYN_STREAM pipeline
  // a window update with it in order to use larger initial windows with pulled
  // streams.
  packet[15 + 8 * numberOfEntries] = SETTINGS_TYPE_INITIAL_WINDOW;
  NetworkEndian::writeUint32(packet + 16 + 8 * numberOfEntries, mPushAllowance);
  numberOfEntries++;

  uint32_t dataLen = 4 + 8 * numberOfEntries;
  mOutputQueueUsed += 8 + dataLen;
  packet[7] = dataLen;
  packet[11] = numberOfEntries;

  LogIO(this, nullptr, "Generate Settings", packet, 8 + dataLen);
  FlushOutputQueue();
}

// perform a bunch of integrity checks on the stream.
// returns true if passed, false (plus LOG and ABORT) if failed.
bool
SpdySession3::VerifyStream(SpdyStream3 *aStream, uint32_t aOptionalID = 0)
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
      SpdyStream3 *idStream = mStreamIDHash.Get(aStream->StreamID());

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

  LOG(("SpdySession3 %p VerifyStream Failure %p stream->id=0x%X "
       "optionalID=0x%X trans=%p test=%d\n",
       this, aStream, aStream->StreamID(),
       aOptionalID, aStream->Transaction(), test));

  MOZ_ASSERT(false, "VerifyStream");
  return false;
}

void
SpdySession3::CleanupStream(SpdyStream3 *aStream, nsresult aResult,
                            rstReason aResetCode)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  LOG3(("SpdySession3::CleanupStream %p %p 0x%X %X\n",
        this, aStream, aStream ? aStream->StreamID() : 0, aResult));
  if (!aStream) {
    return;
  }

  SpdyPushedStream3 *pushSource = nullptr;

  if (NS_SUCCEEDED(aResult) && aStream->DeferCleanupOnSuccess()) {
    LOG(("SpdySession3::CleanupStream 0x%X deferred\n", aStream->StreamID()));
    return;
  }

  if (!VerifyStream(aStream)) {
    LOG(("SpdySession3::CleanupStream failed to verify stream\n"));
    return;
  }

  pushSource = aStream->PushSource();

  if (!aStream->RecvdFin() && aStream->StreamID()) {
    LOG3(("Stream had not processed recv FIN, sending RST code %X\n",
          aResetCode));
    GenerateRstStream(aResetCode, aStream->StreamID());
    DecrementConcurrent(aStream);
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
  // delete the SpdyStream3 and drop the reference to
  // its transaction
  mStreamTransactionHash.Remove(aStream->Transaction());

  if (mShouldGoAway && !mStreamTransactionHash.Count())
    Close(NS_OK);

  if (pushSource) {
    pushSource->SetDeferCleanupOnSuccess(false);
    CleanupStream(pushSource, aResult, aResetCode);
  }
}

static void RemoveStreamFromQueue(SpdyStream3 *aStream, nsDeque &queue)
{
  uint32_t size = queue.GetSize();
  for (uint32_t count = 0; count < size; ++count) {
    SpdyStream3 *stream = static_cast<SpdyStream3 *>(queue.PopFront());
    if (stream != aStream)
      queue.Push(stream);
  }
}

void
SpdySession3::RemoveStreamFromQueues(SpdyStream3 *aStream)
{
  RemoveStreamFromQueue(aStream, mReadyForWrite);
  RemoveStreamFromQueue(aStream, mQueuedStreams);
  RemoveStreamFromQueue(aStream, mReadyForRead);
}

void
SpdySession3::CloseStream(SpdyStream3 *aStream, nsresult aResult)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  LOG3(("SpdySession3::CloseStream %p %p 0x%x %X\n",
        this, aStream, aStream->StreamID(), aResult));

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
SpdySession3::HandleSynStream(SpdySession3 *self)
{
  MOZ_ASSERT(self->mFrameControlType == CONTROL_TYPE_SYN_STREAM);

  if (self->mInputFrameDataSize < 18) {
    LOG3(("SpdySession3::HandleSynStream %p SYN_STREAM too short data=%d",
          self, self->mInputFrameDataSize));
    return NS_ERROR_ILLEGAL_VALUE;
  }

  uint32_t streamID =
    NetworkEndian::readUint32(self->mInputFrameBuffer + 2 * sizeof(uint32_t));
  uint32_t associatedID =
    NetworkEndian::readUint32(self->mInputFrameBuffer + 3 * sizeof(uint32_t));
  uint8_t flags = reinterpret_cast<uint8_t *>(self->mInputFrameBuffer.get())[4];

  LOG3(("SpdySession3::HandleSynStream %p recv SYN_STREAM (push) "
        "for ID 0x%X associated with 0x%X.\n",
        self, streamID, associatedID));

  if (streamID & 0x01) {                   // test for odd stream ID
    LOG3(("SpdySession3::HandleSynStream %p recvd SYN_STREAM id must be even.",
          self));
    return NS_ERROR_ILLEGAL_VALUE;
  }

  // confirm associated-to
  nsresult rv = self->SetInputFrameDataStream(associatedID);
  if (NS_FAILED(rv))
    return rv;
  SpdyStream3 *associatedStream = self->mInputFrameDataStream;

  ++(self->mServerPushedResources);

  // Anytime we start using the high bit of stream ID (either client or server)
  // begin to migrate to a new session.
  if (streamID >= kMaxStreamID)
    self->mShouldGoAway = true;

  bool resetStream = true;
  SpdyPushCache *cache = nullptr;

  if (!(flags & kFlag_Data_UNI)) {
    // pushed streams require UNIDIRECTIONAL flag
    LOG3(("SpdySession3::HandleSynStream %p ID %0x%X associated ID 0x%X failed.\n",
          self, streamID, associatedID));
    self->GenerateRstStream(RST_PROTOCOL_ERROR, streamID);

  } else if (!associatedID) {
    // associated stream 0 will never find a match, but the spec requires a
    // PROTOCOL_ERROR in this specific case
    LOG3(("SpdySession3::HandleSynStream %p associated ID of 0 failed.\n", self));
    self->GenerateRstStream(RST_PROTOCOL_ERROR, streamID);

  } else if (!gHttpHandler->AllowPush()) {
    // MAX_CONCURRENT_STREAMS of 0 in settings should have disabled push,
    // but some servers are buggy about that.. or the config could have
    // been updated after the settings frame was sent. In both cases just
    // reject the pushed stream as refused
    LOG3(("SpdySession3::HandleSynStream Push Recevied when Disabled\n"));
    self->GenerateRstStream(RST_REFUSED_STREAM, streamID);

  } else if (!associatedStream) {
    LOG3(("SpdySession3::HandleSynStream %p lookup associated ID failed.\n", self));
    self->GenerateRstStream(RST_INVALID_STREAM, streamID);

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
      // this is unexpected, but we can handle it just be refusing the push
      LOG3(("SpdySession3::HandleSynStream Push Recevied without loadgroup cache\n"));
      self->GenerateRstStream(RST_REFUSED_STREAM, streamID);
    }
    else {
      resetStream = false;
    }
  }

  if (resetStream) {
    // Need to decompress the headers even though we aren't using them yet in
    // order to keep the compression context consistent for other syn_reply frames
    rv = self->UncompressAndDiscard(18, self->mInputFrameDataSize - 10);
    if (NS_FAILED(rv)) {
      LOG(("SpdySession3::HandleSynStream uncompress failed\n"));
      return rv;
    }
    self->ResetDownstreamState();
    return NS_OK;
  }

  // Create the buffering transaction and push stream
  nsRefPtr<SpdyPush3TransactionBuffer> transactionBuffer =
    new SpdyPush3TransactionBuffer();
  transactionBuffer->SetConnection(self);
  SpdyPushedStream3 *pushedStream =
    new SpdyPushedStream3(transactionBuffer, self,
                          associatedStream, streamID);

  // ownership of the pushed stream is by the transaction hash, just as it
  // is for a client initiated stream. Errors that aren't fatal to the
  // whole session must call cleanupStream() after this point in order
  // to remove the stream from that hash.
  self->mStreamTransactionHash.Put(transactionBuffer, pushedStream);
  self->mPushedStreams.AppendElement(pushedStream);

  // The pushed stream is unidirectional so it is fully open immediately
  rv = pushedStream->SetFullyOpen();
  if (NS_FAILED(rv)) {
    LOG(("SpdySession3::HandleSynStream pushedstream fully open failed\n"));
    self->CleanupStream(pushedStream, rv, RST_CANCEL);
    self->ResetDownstreamState();
    return NS_OK;
  }

  // Uncompress the response headers into a stream specific buffer, leaving them
  // in spdy format for the time being.
  rv = pushedStream->Uncompress(&self->mDownstreamZlib,
                                self->mInputFrameBuffer + 18,
                                self->mInputFrameDataSize - 10);
  if (NS_FAILED(rv)) {
    LOG(("SpdySession3::HandleSynStream uncompress failed\n"));
    return rv;
  }

  if (self->RegisterStreamID(pushedStream, streamID) == kDeadStreamID) {
    LOG(("SpdySession3::HandleSynStream registerstreamid failed\n"));
    return NS_ERROR_FAILURE;
  }

  // Fake the request side of the pushed HTTP transaction. Sets up hash
  // key and origin
  uint32_t notUsed;
  pushedStream->ReadSegments(nullptr, 1, &notUsed);

  nsAutoCString key;
  if (!pushedStream->GetHashKey(key)) {
    LOG(("SpdySession3::HandleSynStream one of :host :scheme :path missing from push\n"));
    self->CleanupStream(pushedStream, NS_ERROR_FAILURE, RST_INVALID_STREAM);
    self->ResetDownstreamState();
    return NS_OK;
  }

  if (!associatedStream->Origin().Equals(pushedStream->Origin())) {
    LOG(("SpdySession3::HandleSynStream pushed stream mismatched origin\n"));
    self->CleanupStream(pushedStream, NS_ERROR_FAILURE, RST_INVALID_STREAM);
    self->ResetDownstreamState();
    return NS_OK;
  }

  if (!cache->RegisterPushedStreamSpdy3(key, pushedStream)) {
    LOG(("SpdySession3::HandleSynStream registerPushedStream Failed\n"));
    self->CleanupStream(pushedStream, NS_ERROR_FAILURE, RST_INVALID_STREAM);
    self->ResetDownstreamState();
    return NS_OK;
  }

  self->ResetDownstreamState();
  return NS_OK;
}

nsresult
SpdySession3::SetInputFrameDataStream(uint32_t streamID)
{
  mInputFrameDataStream = mStreamIDHash.Get(streamID);
  if (VerifyStream(mInputFrameDataStream, streamID))
    return NS_OK;

  LOG(("SpdySession3::SetInputFrameDataStream failed to verify 0x%X\n",
       streamID));
  mInputFrameDataStream = nullptr;
  return NS_ERROR_UNEXPECTED;
}

nsresult
SpdySession3::HandleSynReply(SpdySession3 *self)
{
  MOZ_ASSERT(self->mFrameControlType == CONTROL_TYPE_SYN_REPLY);

  if (self->mInputFrameDataSize < 4) {
    LOG3(("SpdySession3::HandleSynReply %p SYN REPLY too short data=%d",
          self, self->mInputFrameDataSize));
    // A framing error is a session wide error that cannot be recovered
    return NS_ERROR_ILLEGAL_VALUE;
  }

  uint32_t streamID =
    NetworkEndian::readUint32(self->mInputFrameBuffer + 2 * sizeof(uint32_t));
  LOG3(("SpdySession3::HandleSynReply %p lookup via streamID 0x%X in syn_reply.\n",
        self, streamID));
  nsresult rv = self->SetInputFrameDataStream(streamID);
  if (NS_FAILED(rv))
    return rv;

  if (!self->mInputFrameDataStream) {
    // Cannot find stream. We can continue the SPDY session, but we need to
    // uncompress the header block to maintain the correct compression context

    LOG3(("SpdySession3::HandleSynReply %p lookup streamID in syn_reply "
          "0x%X failed. NextStreamID = 0x%X\n",
          self, streamID, self->mNextStreamID));

    if (streamID >= self->mNextStreamID)
      self->GenerateRstStream(RST_INVALID_STREAM, streamID);

    rv = self->UncompressAndDiscard(12, self->mInputFrameDataSize - 4);
    if (NS_FAILED(rv)) {
      LOG(("SpdySession3::HandleSynReply uncompress failed\n"));
      // this is fatal to the session
      return rv;
    }

    self->ResetDownstreamState();
    return NS_OK;
  }

  // Uncompress the headers into a stream specific buffer, leaving them in
  // spdy format for the time being. Make certain to do this
  // step before any error handling that might abort the stream but not
  // the session becuase the session compression context will become
  // inconsistent if all of the compressed data is not processed.
  rv = self->mInputFrameDataStream->Uncompress(&self->mDownstreamZlib,
                                               self->mInputFrameBuffer + 12,
                                               self->mInputFrameDataSize - 4);

  if (NS_FAILED(rv)) {
    LOG(("SpdySession3::HandleSynReply uncompress failed\n"));
    return rv;
  }

  if (self->mInputFrameDataStream->GetFullyOpen()) {
    // "If an endpoint receives multiple SYN_REPLY frames for the same active
    // stream ID, it MUST issue a stream error (Section 2.4.2) with the error
    // code STREAM_IN_USE."
    //
    // "STREAM_ALREADY_CLOSED. The endpoint received a data or SYN_REPLY
    // frame for a stream which is half closed."
    //
    // If the stream is open then just RST_STREAM with STREAM_IN_USE
    // If the stream is half closed then RST_STREAM with STREAM_ALREADY_CLOSED
    // abort the session
    //
    LOG3(("SpdySession3::HandleSynReply %p dup SYN_REPLY for 0x%X"
          " recvdfin=%d", self, self->mInputFrameDataStream->StreamID(),
          self->mInputFrameDataStream->RecvdFin()));

    self->CleanupStream(self->mInputFrameDataStream, NS_ERROR_ALREADY_OPENED,
                        self->mInputFrameDataStream->RecvdFin() ?
                        RST_STREAM_ALREADY_CLOSED : RST_STREAM_IN_USE);
    self->ResetDownstreamState();
    return NS_OK;
  }

  rv = self->mInputFrameDataStream->SetFullyOpen();
  if (NS_FAILED(rv)) {
    LOG(("SpdySession3::HandleSynReply SetFullyOpen failed\n"));
    if (self->mInputFrameDataStream->IsTunnel()) {
      gHttpHandler->ConnMgr()->CancelTransactions(
        self->mInputFrameDataStream->Transaction()->ConnectionInfo(),
        NS_ERROR_CONNECTION_REFUSED);
    }
    self->CleanupStream(self->mInputFrameDataStream, rv, RST_CANCEL);
    self->ResetDownstreamState();
    return NS_OK;
  }

  self->mInputFrameDataLast = self->mInputFrameBuffer[4] & kFlag_Data_FIN;
  self->mInputFrameDataStream->UpdateTransportReadEvents(self->mInputFrameDataSize);
  self->mLastDataReadEpoch = self->mLastReadEpoch;

  if (self->mInputFrameBuffer[4] & ~kFlag_Data_FIN) {
    LOG3(("SynReply %p had undefined flag set 0x%X\n", self, streamID));
    self->CleanupStream(self->mInputFrameDataStream, NS_ERROR_ILLEGAL_VALUE,
                        RST_PROTOCOL_ERROR);
    self->ResetDownstreamState();
    return NS_OK;
  }

  if (!self->mInputFrameDataLast) {
    // don't process the headers yet as there could be more coming from HEADERS
    // frames
    self->ResetDownstreamState();
    return NS_OK;
  }

  rv = self->ResponseHeadersComplete();
  if (rv == NS_ERROR_ILLEGAL_VALUE) {
    LOG3(("SpdySession3::HandleSynReply %p PROTOCOL_ERROR detected 0x%X\n",
          self, streamID));
    self->CleanupStream(self->mInputFrameDataStream, rv, RST_PROTOCOL_ERROR);
    self->ResetDownstreamState();
    rv = NS_OK;
  }
  return rv;
}

// ResponseHeadersComplete() returns NS_ERROR_ILLEGAL_VALUE when the stream
// should be reset with a PROTOCOL_ERROR, NS_OK when the SYN_REPLY was
// fine, and any other error is fatal to the session.
nsresult
SpdySession3::ResponseHeadersComplete()
{
  LOG3(("SpdySession3::ResponseHeadersComplete %p for 0x%X fin=%d",
        this, mInputFrameDataStream->StreamID(), mInputFrameDataLast));

  // The spdystream needs to see flattened http headers
  // Uncompressed spdy format headers currently live in
  // SpdyStream3::mDecompressBuffer - convert that to HTTP format in
  // mFlatHTTPResponseHeaders via ConvertHeaders()

  mFlatHTTPResponseHeadersOut = 0;
  nsresult rv = mInputFrameDataStream->ConvertHeaders(mFlatHTTPResponseHeaders);
  if (NS_FAILED(rv))
    return rv;

  ChangeDownstreamState(PROCESSING_COMPLETE_HEADERS);
  return NS_OK;
}

nsresult
SpdySession3::HandleRstStream(SpdySession3 *self)
{
  MOZ_ASSERT(self->mFrameControlType == CONTROL_TYPE_RST_STREAM);

  if (self->mInputFrameDataSize != 8) {
    LOG3(("SpdySession3::HandleRstStream %p RST_STREAM wrong length data=%d",
          self, self->mInputFrameDataSize));
    return NS_ERROR_ILLEGAL_VALUE;
  }

  uint8_t flags = reinterpret_cast<uint8_t *>(self->mInputFrameBuffer.get())[4];

  uint32_t streamID =
    NetworkEndian::readUint32(self->mInputFrameBuffer + 2 * sizeof(uint32_t));

  self->mDownstreamRstReason =
    NetworkEndian::readUint32(self->mInputFrameBuffer + 3 * sizeof(uint32_t));

  LOG3(("SpdySession3::HandleRstStream %p RST_STREAM Reason Code %u ID %x "
        "flags %x", self, self->mDownstreamRstReason, streamID, flags));

  if (flags != 0) {
    LOG3(("SpdySession3::HandleRstStream %p RST_STREAM with flags is illegal",
          self));
    return NS_ERROR_ILLEGAL_VALUE;
  }

  if (self->mDownstreamRstReason == RST_INVALID_STREAM ||
      self->mDownstreamRstReason == RST_STREAM_IN_USE ||
      self->mDownstreamRstReason == RST_FLOW_CONTROL_ERROR) {
    // basically just ignore this
    LOG3(("SpdySession3::HandleRstStream %p No Reset Processing Needed.\n"));
    self->ResetDownstreamState();
    return NS_OK;
  }

  nsresult rv = self->SetInputFrameDataStream(streamID);

  if (!self->mInputFrameDataStream) {
    if (NS_FAILED(rv))
      LOG(("SpdySession3::HandleRstStream %p lookup streamID for RST Frame "
           "0x%X failed reason = %d :: VerifyStream Failed\n", self, streamID,
           self->mDownstreamRstReason));

    LOG3(("SpdySession3::HandleRstStream %p lookup streamID for RST Frame "
          "0x%X failed reason = %d", self, streamID,
          self->mDownstreamRstReason));
    return NS_ERROR_ILLEGAL_VALUE;
  }

  self->ChangeDownstreamState(PROCESSING_CONTROL_RST_STREAM);
  return NS_OK;
}

PLDHashOperator
SpdySession3::UpdateServerRwinEnumerator(nsAHttpTransaction *key,
                                         nsAutoPtr<SpdyStream3> &stream,
                                         void *closure)
{
  int32_t delta = *(static_cast<int32_t *>(closure));
  stream->UpdateRemoteWindow(delta);
  return PL_DHASH_NEXT;
}

nsresult
SpdySession3::HandleSettings(SpdySession3 *self)
{
  MOZ_ASSERT(self->mFrameControlType == CONTROL_TYPE_SETTINGS);

  if (self->mInputFrameDataSize < 4) {
    LOG3(("SpdySession3::HandleSettings %p SETTINGS wrong length data=%d",
          self, self->mInputFrameDataSize));
    return NS_ERROR_ILLEGAL_VALUE;
  }

  uint32_t numEntries =
    NetworkEndian::readUint32(self->mInputFrameBuffer + 2 * sizeof(uint32_t));

  // Ensure frame is large enough for supplied number of entries
  // Each entry is 8 bytes, frame data is reduced by 4 to account for
  // the NumEntries value.
  if ((self->mInputFrameDataSize - 4) < (numEntries * 8)) {
    LOG3(("SpdySession3::HandleSettings %p SETTINGS wrong length data=%d",
          self, self->mInputFrameDataSize));
    return NS_ERROR_ILLEGAL_VALUE;
  }

  LOG3(("SpdySession3::HandleSettings %p SETTINGS Control Frame with %d entries",
        self, numEntries));

  for (uint32_t index = 0; index < numEntries; ++index) {
    unsigned char *setting = reinterpret_cast<unsigned char *>
      (self->mInputFrameBuffer.get()) + 12 + index * 8;

    uint32_t flags = setting[0];
    uint32_t id = NetworkEndian::readUint32(setting) & 0xffffff;
    uint32_t value = NetworkEndian::readUint32(setting + 1 * sizeof(uint32_t));

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
      if (flags & PERSIST_VALUE)
      {
        nsRefPtr<nsHttpConnectionInfo> ci;
        self->GetConnectionInfo(getter_AddRefs(ci));
        if (ci)
          gHttpHandler->ConnMgr()->ReportSpdyCWNDSetting(ci, value);
      }
      Telemetry::Accumulate(Telemetry::SPDY_SETTINGS_CWND, value);
      break;

    case SETTINGS_TYPE_DOWNLOAD_RETRANS_RATE:
      Telemetry::Accumulate(Telemetry::SPDY_SETTINGS_RETRANS, value);
      break;

    case SETTINGS_TYPE_INITIAL_WINDOW:
      Telemetry::Accumulate(Telemetry::SPDY_SETTINGS_IW, value >> 10);
      {
        int32_t delta = value - self->mServerInitialWindow;
        self->mServerInitialWindow = value;

        // we need to add the delta to all open streams (delta can be negative)
        self->mStreamTransactionHash.Enumerate(UpdateServerRwinEnumerator,
                                               &delta);
      }
      break;

    default:
      break;
    }

  }

  self->ResetDownstreamState();
  return NS_OK;
}

nsresult
SpdySession3::HandleNoop(SpdySession3 *self)
{
  MOZ_ASSERT(self->mFrameControlType == CONTROL_TYPE_NOOP);

  // Should not be receiving noop frames in spdy/3, so we'll just
  // make a log and ignore it

  LOG3(("SpdySession3::HandleNoop %p NOP.", self));

  self->ResetDownstreamState();
  return NS_OK;
}

nsresult
SpdySession3::HandlePing(SpdySession3 *self)
{
  MOZ_ASSERT(self->mFrameControlType == CONTROL_TYPE_PING);

  if (self->mInputFrameDataSize != 4) {
    LOG3(("SpdySession3::HandlePing %p PING had wrong amount of data %d",
          self, self->mInputFrameDataSize));
    return NS_ERROR_ILLEGAL_VALUE;
  }

  uint32_t pingID =
    NetworkEndian::readUint32(self->mInputFrameBuffer + 2 * sizeof(uint32_t));

  LOG3(("SpdySession3::HandlePing %p PING ID 0x%X.", self, pingID));

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
SpdySession3::HandleGoAway(SpdySession3 *self)
{
  MOZ_ASSERT(self->mFrameControlType == CONTROL_TYPE_GOAWAY);

  if (self->mInputFrameDataSize != 8) {
    LOG3(("SpdySession3::HandleGoAway %p GOAWAY had wrong amount of data %d",
          self, self->mInputFrameDataSize));
    return NS_ERROR_ILLEGAL_VALUE;
  }

  self->mShouldGoAway = true;
  self->mGoAwayID =
    NetworkEndian::readUint32(self->mInputFrameBuffer + 2 * sizeof(uint32_t));
  self->mCleanShutdown = true;

  // Find streams greater than the last-good ID and mark them for deletion
  // in the mGoAwayStreamsToRestart queue with the GoAwayEnumerator. The
  // underlying transaction can be restarted.
  self->mStreamTransactionHash.Enumerate(GoAwayEnumerator, self);

  // Process the streams marked for deletion and restart.
  uint32_t size = self->mGoAwayStreamsToRestart.GetSize();
  for (uint32_t count = 0; count < size; ++count) {
    SpdyStream3 *stream =
      static_cast<SpdyStream3 *>(self->mGoAwayStreamsToRestart.PopFront());

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
    SpdyStream3 *stream =
      static_cast<SpdyStream3 *>(self->mQueuedStreams.PopFront());
    self->CloseStream(stream, NS_ERROR_NET_RESET);
    self->mStreamTransactionHash.Remove(stream->Transaction());
  }

  LOG3(("SpdySession3::HandleGoAway %p GOAWAY Last-Good-ID 0x%X status 0x%X "
        "live streams=%d\n", self, self->mGoAwayID,
        NetworkEndian::readUint32(self->mInputFrameBuffer +
                                  3 * sizeof(uint32_t)),
        self->mStreamTransactionHash.Count()));

  self->ResetDownstreamState();
  return NS_OK;
}

nsresult
SpdySession3::HandleHeaders(SpdySession3 *self)
{
  MOZ_ASSERT(self->mFrameControlType == CONTROL_TYPE_HEADERS);

  if (self->mInputFrameDataSize < 4) {
    LOG3(("SpdySession3::HandleHeaders %p HEADERS had wrong amount of data %d",
          self, self->mInputFrameDataSize));
    return NS_ERROR_ILLEGAL_VALUE;
  }

  uint32_t streamID =
    NetworkEndian::readUint32(self->mInputFrameBuffer + 2 * sizeof(uint32_t));
  LOG3(("SpdySession3::HandleHeaders %p HEADERS for Stream 0x%X.\n",
        self, streamID));
  nsresult rv = self->SetInputFrameDataStream(streamID);
  if (NS_FAILED(rv))
    return rv;

  if (!self->mInputFrameDataStream) {
    LOG3(("SpdySession3::HandleHeaders %p lookup streamID 0x%X failed.\n",
          self, streamID));
    if (streamID >= self->mNextStreamID)
      self->GenerateRstStream(RST_INVALID_STREAM, streamID);

    rv = self->UncompressAndDiscard(12, self->mInputFrameDataSize - 4);
    if (NS_FAILED(rv)) {
      LOG(("SpdySession3::HandleHeaders uncompress failed\n"));
      // this is fatal to the session
      return rv;
    }
    self->ResetDownstreamState();
    return NS_OK;
  }

  // Uncompress the headers into local buffers in the SpdyStream, leaving
  // them in spdy format for the time being. Make certain to do this
  // step before any error handling that might abort the stream but not
  // the session becuase the session compression context will become
  // inconsistent if all of the compressed data is not processed.
  rv = self->mInputFrameDataStream->Uncompress(&self->mDownstreamZlib,
                                               self->mInputFrameBuffer + 12,
                                               self->mInputFrameDataSize - 4);
  if (NS_FAILED(rv)) {
    LOG(("SpdySession3::HandleHeaders uncompress failed\n"));
    return rv;
  }

  self->mInputFrameDataLast = self->mInputFrameBuffer[4] & kFlag_Data_FIN;
  self->mInputFrameDataStream->
    UpdateTransportReadEvents(self->mInputFrameDataSize);
  self->mLastDataReadEpoch = self->mLastReadEpoch;

  if (self->mInputFrameBuffer[4] & ~kFlag_Data_FIN) {
    LOG3(("Headers %p had undefined flag set 0x%X\n", self, streamID));
    self->CleanupStream(self->mInputFrameDataStream, NS_ERROR_ILLEGAL_VALUE,
                        RST_PROTOCOL_ERROR);
    self->ResetDownstreamState();
    return NS_OK;
  }

  if (!self->mInputFrameDataLast) {
    // don't process the headers yet as there could be more HEADERS frames
    self->ResetDownstreamState();
    return NS_OK;
  }

  rv = self->ResponseHeadersComplete();
  if (rv == NS_ERROR_ILLEGAL_VALUE) {
    LOG3(("SpdySession3::HanndleHeaders %p PROTOCOL_ERROR detected 0x%X\n",
          self, streamID));
    self->CleanupStream(self->mInputFrameDataStream, rv, RST_PROTOCOL_ERROR);
    self->ResetDownstreamState();
    rv = NS_OK;
  }
  return rv;
}

nsresult
SpdySession3::HandleWindowUpdate(SpdySession3 *self)
{
  MOZ_ASSERT(self->mFrameControlType == CONTROL_TYPE_WINDOW_UPDATE);

  if (self->mInputFrameDataSize < 8) {
    LOG3(("SpdySession3::HandleWindowUpdate %p Window Update wrong length %d\n",
          self, self->mInputFrameDataSize));
    return NS_ERROR_ILLEGAL_VALUE;
  }

  uint32_t delta =
    NetworkEndian::readUint32(self->mInputFrameBuffer + 3 * sizeof(uint32_t));
  delta &= 0x7fffffff;
  uint32_t streamID =
    NetworkEndian::readUint32(self->mInputFrameBuffer + 2 * sizeof(uint32_t));
  streamID &= 0x7fffffff;

  LOG3(("SpdySession3::HandleWindowUpdate %p len=%d for Stream 0x%X.\n",
        self, delta, streamID));
  nsresult rv = self->SetInputFrameDataStream(streamID);
  if (NS_FAILED(rv))
    return rv;

  if (!self->mInputFrameDataStream) {
    LOG3(("SpdySession3::HandleWindowUpdate %p lookup streamID 0x%X failed.\n",
          self, streamID));
    if (streamID >= self->mNextStreamID)
      self->GenerateRstStream(RST_INVALID_STREAM, streamID);
    self->ResetDownstreamState();
    return NS_OK;
  }

  self->mInputFrameDataStream->UpdateRemoteWindow(delta);
  self->ResetDownstreamState();
  return NS_OK;
}

nsresult
SpdySession3::HandleCredential(SpdySession3 *self)
{
  MOZ_ASSERT(self->mFrameControlType == CONTROL_TYPE_CREDENTIAL);

  // These aren't used yet. Just ignore the frame.

  LOG3(("SpdySession3::HandleCredential %p NOP.", self));

  self->ResetDownstreamState();
  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsAHttpTransaction. It is expected that nsHttpConnection is the caller
// of these methods
//-----------------------------------------------------------------------------

void
SpdySession3::OnTransportStatus(nsITransport* aTransport,
                                nsresult aStatus,
                                uint64_t aProgress)
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
    SpdyStream3 *target = mStreamIDHash.Get(1);
    nsAHttpTransaction *transaction = target ? target->Transaction() : nullptr;
    if (transaction)
      transaction->OnTransportStatus(aTransport, aStatus, aProgress);
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
    // is sent from SpdyStream3::TransmitFrame

    // NS_NET_STATUS_WAITING_FOR:
    // Created by nsHttpConnection when the request has been totally sent.
    // There is no good way to map it to the right transaction in spdy,
    // so it is ignored here and generated separately when the same
    // condition is complete in SpdyStream3 when there is no more
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
SpdySession3::ReadSegments(nsAHttpSegmentReader *reader,
                           uint32_t count,
                           uint32_t *countRead)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  MOZ_ASSERT(!mSegmentReader || !reader || (mSegmentReader == reader),
             "Inconsistent Write Function Callback");

  if (reader)
    mSegmentReader = reader;

  nsresult rv;
  *countRead = 0;

  LOG3(("SpdySession3::ReadSegments %p", this));

  SpdyStream3 *stream = static_cast<SpdyStream3 *>(mReadyForWrite.PopFront());
  if (!stream) {
    LOG3(("SpdySession3 %p could not identify a stream to write; suspending.",
          this));
    FlushOutputQueue();
    SetWriteCallbacks();
    return NS_BASE_STREAM_WOULD_BLOCK;
  }

  LOG3(("SpdySession3 %p will write from SpdyStream3 %p 0x%X "
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

    LOG3(("SpdySession3::ReadSegments %p dealing with block on read", this));

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
    LOG3(("SpdySession3::ReadSegments %p returning FAIL code %X",
          this, rv));
    if (rv != NS_BASE_STREAM_WOULD_BLOCK)
      CleanupStream(stream, rv, RST_CANCEL);
    return rv;
  }

  if (*countRead > 0) {
    LOG3(("SpdySession3::ReadSegments %p stream=%p countread=%d",
          this, stream, *countRead));
    mReadyForWrite.Push(stream);
    SetWriteCallbacks();
    return rv;
  }

  if (stream->BlockedOnRwin()) {
    LOG3(("SpdySession3 %p will stream %p 0x%X suspended for flow control\n",
          this, stream, stream->StreamID()));
    return NS_BASE_STREAM_WOULD_BLOCK;
  }

  LOG3(("SpdySession3::ReadSegments %p stream=%p stream send complete",
        this, stream));

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
// call stream->WriteSegments which can call this::OnWriteSegment to get the
// data. It always gets full frames if they are part of the stream

nsresult
SpdySession3::WriteSegments(nsAHttpSegmentWriter *writer,
                            uint32_t count,
                            uint32_t *countWritten)
{
  typedef nsresult  (*Control_FX) (SpdySession3 *self);
  static const Control_FX sControlFunctions[] =
  {
    nullptr,
    SpdySession3::HandleSynStream,
    SpdySession3::HandleSynReply,
    SpdySession3::HandleRstStream,
    SpdySession3::HandleSettings,
    SpdySession3::HandleNoop,
    SpdySession3::HandlePing,
    SpdySession3::HandleGoAway,
    SpdySession3::HandleHeaders,
    SpdySession3::HandleWindowUpdate,
    SpdySession3::HandleCredential
  };

  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  nsresult rv;
  *countWritten = 0;

  if (mClosed)
    return NS_ERROR_FAILURE;

  SetWriteCallbacks();

  // If there are http transactions attached to a push stream with filled buffers
  // trigger that data pump here. This only reads from buffers (not the network)
  // so mDownstreamState doesn't matter.
  SpdyStream3 *pushConnectedStream =
    static_cast<SpdyStream3 *>(mReadyForRead.PopFront());
  if (pushConnectedStream) {
    LOG3(("SpdySession3::WriteSegments %p processing pushed stream 0x%X\n",
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
      CleanupStream(pushConnectedStream, NS_OK, RST_CANCEL);
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

  // We buffer all control frames and act on them in this layer.
  // We buffer the first 8 bytes of data frames (the header) but
  // the actual data is passed through unprocessed.

  if (mDownstreamState == BUFFERING_FRAME_HEADER) {
    // The first 8 bytes of every frame is header information that
    // we are going to want to strip before passing to http. That is
    // true of both control and data packets.

    MOZ_ASSERT(mInputFrameBufferUsed < 8,
               "Frame Buffer Used Too Large for State");

    rv = NetworkRead(writer, mInputFrameBuffer + mInputFrameBufferUsed,
                     8 - mInputFrameBufferUsed, countWritten);

    if (NS_FAILED(rv)) {
      LOG3(("SpdySession3 %p buffering frame header read failure %x\n",
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
      LOG3(("SpdySession3::WriteSegments %p "
            "BUFFERING FRAME HEADER incomplete size=%d",
            this, mInputFrameBufferUsed));
      return rv;
    }

    // For both control and data frames the second 32 bit word of the header
    // is 8-flags, 24-length. (network byte order)
    mInputFrameDataSize =
      NetworkEndian::readUint32(mInputFrameBuffer + 1 * sizeof(uint32_t));
    mInputFrameDataSize &= 0x00ffffff;
    mInputFrameDataRead = 0;

    if (mInputFrameBuffer[0] & kFlag_Control) {
      EnsureBuffer(mInputFrameBuffer, mInputFrameDataSize + 8, 8,
                   mInputFrameBufferSize);
      ChangeDownstreamState(BUFFERING_CONTROL_FRAME);

      // The first 32 bit word of the header is
      // 1 ctrl - 15 version - 16 type
      uint16_t version = NetworkEndian::readUint16(mInputFrameBuffer);
      version &= 0x7fff;

      mFrameControlType =
        NetworkEndian::readUint16(mInputFrameBuffer + sizeof(uint16_t));

      LOG3(("SpdySession3::WriteSegments %p - Control Frame Identified "
            "type %d version %d data len %d",
            this, mFrameControlType, version, mInputFrameDataSize));

      if (mFrameControlType >= CONTROL_TYPE_LAST ||
          mFrameControlType <= CONTROL_TYPE_FIRST)
        return NS_ERROR_ILLEGAL_VALUE;

      if (version != kVersion)
        return NS_ERROR_ILLEGAL_VALUE;
    }
    else {
      ChangeDownstreamState(PROCESSING_DATA_FRAME);

      Telemetry::Accumulate(Telemetry::SPDY_CHUNK_RECVD,
                            mInputFrameDataSize >> 10);
      mLastDataReadEpoch = mLastReadEpoch;

      uint32_t streamID = NetworkEndian::readUint32(mInputFrameBuffer);
      rv = SetInputFrameDataStream(streamID);
      if (NS_FAILED(rv)) {
        LOG(("SpdySession3::WriteSegments %p lookup streamID 0x%X failed. "
              "probably due to verification.\n", this, streamID));
        return rv;
      }
      if (!mInputFrameDataStream) {
        LOG3(("SpdySession3::WriteSegments %p lookup streamID 0x%X failed. "
              "Next = 0x%X", this, streamID, mNextStreamID));
        if (streamID >= mNextStreamID)
          GenerateRstStream(RST_INVALID_STREAM, streamID);
        ChangeDownstreamState(DISCARDING_DATA_FRAME);
      }
      else if (mInputFrameDataStream->RecvdFin()) {
        LOG3(("SpdySession3::WriteSegments %p streamID 0x%X "
              "Data arrived for already server closed stream.\n",
              this, streamID));
        GenerateRstStream(RST_STREAM_ALREADY_CLOSED, streamID);
        ChangeDownstreamState(DISCARDING_DATA_FRAME);
      }
      else if (!mInputFrameDataStream->RecvdData()) {
        LOG3(("SpdySession3 %p First Data Frame Flushes Headers stream 0x%X\n",
              this, streamID));

        mInputFrameDataStream->SetRecvdData(true);
        rv = ResponseHeadersComplete();
        if (rv == NS_ERROR_ILLEGAL_VALUE) {
          LOG3(("SpdySession3 %p PROTOCOL_ERROR detected 0x%X\n",
                this, streamID));
          CleanupStream(mInputFrameDataStream, rv, RST_PROTOCOL_ERROR);
          ChangeDownstreamState(DISCARDING_DATA_FRAME);
        }
        else {
          mDataPending = true;
        }
      }

      mInputFrameDataLast = (mInputFrameBuffer[4] & kFlag_Data_FIN);
      LOG3(("Start Processing Data Frame. "
            "Session=%p Stream ID 0x%X Stream Ptr %p Fin=%d Len=%d",
            this, streamID, mInputFrameDataStream, mInputFrameDataLast,
            mInputFrameDataSize));
      UpdateLocalRwin(mInputFrameDataStream, mInputFrameDataSize);
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
    else if (mDownstreamRstReason == RST_FRAME_TOO_LARGE)
      rv = NS_ERROR_FILE_TOO_BIG;
    else
      rv = NS_ERROR_ILLEGAL_VALUE;

    if (mDownstreamRstReason != RST_REFUSED_STREAM &&
        mDownstreamRstReason != RST_CANCEL)
      mShouldGoAway = true;

    // mInputFrameDataStream is reset by ChangeDownstreamState
    SpdyStream3 *stream = mInputFrameDataStream;
    ResetDownstreamState();
    LOG3(("SpdySession3::WriteSegments cleanup stream on recv of rst "
          "session=%p stream=%p 0x%X\n", this, stream,
          stream ? stream->StreamID() : 0));
    CleanupStream(stream, rv, RST_CANCEL);
    return NS_OK;
  }

  if (mDownstreamState == PROCESSING_DATA_FRAME ||
      mDownstreamState == PROCESSING_COMPLETE_HEADERS) {

    // The cleanup stream should only be set while stream->WriteSegments is
    // on the stack and then cleaned up in this code block afterwards.
    MOZ_ASSERT(!mNeedsCleanup, "cleanup stream set unexpectedly");
    mNeedsCleanup = nullptr;                     /* just in case */

    SpdyStream3 *stream = mInputFrameDataStream;
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
      LOG3(("SpdySession3::WriteSegments session=%p stream=%p 0x%X "
            "needscleanup=%p. cleanup stream based on "
            "stream->writeSegments returning code %X\n",
            this, stream, stream ? stream->StreamID() : 0,
            mNeedsCleanup, rv));
      CleanupStream(stream, NS_OK, RST_CANCEL);
      MOZ_ASSERT(!mNeedsCleanup || mNeedsCleanup == stream);
      mNeedsCleanup = nullptr;
      return NS_OK;
    }

    if (mNeedsCleanup) {
      LOG3(("SpdySession3::WriteSegments session=%p stream=%p 0x%X "
            "cleanup stream based on mNeedsCleanup.\n",
            this, mNeedsCleanup, mNeedsCleanup ? mNeedsCleanup->StreamID() : 0));
      CleanupStream(mNeedsCleanup, NS_OK, RST_CANCEL);
      mNeedsCleanup = nullptr;
    }

    if (NS_FAILED(rv)) {
      LOG3(("SpdySession3 %p data frame read failure %x\n", this, rv));
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
      LOG3(("SpdySession3 %p discard frame read failure %x\n", this, rv));
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

  MOZ_ASSERT(mDownstreamState == BUFFERING_CONTROL_FRAME);
  if (mDownstreamState != BUFFERING_CONTROL_FRAME) {
    // this cannot happen
    return NS_ERROR_UNEXPECTED;
  }

  MOZ_ASSERT(mInputFrameBufferUsed == 8,
             "Frame Buffer Header Not Present");

  rv = NetworkRead(writer, mInputFrameBuffer + 8 + mInputFrameDataRead,
                   mInputFrameDataSize - mInputFrameDataRead, countWritten);

  if (NS_FAILED(rv)) {
    LOG3(("SpdySession3 %p buffering control frame read failure %x\n",
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

  // This check is actually redundant, the control type was previously
  // checked to make sure it was in range, but we will check it again
  // at time of use to make sure a regression doesn't creep in.
  if (mFrameControlType >= CONTROL_TYPE_LAST ||
      mFrameControlType <= CONTROL_TYPE_FIRST)
  {
    MOZ_ASSERT(false, "control type out of range");
    return NS_ERROR_ILLEGAL_VALUE;
  }
  rv = sControlFunctions[mFrameControlType](this);

  MOZ_ASSERT(NS_FAILED(rv) ||
             mDownstreamState != BUFFERING_CONTROL_FRAME,
             "Control Handler returned OK but did not change state");

  if (mShouldGoAway && !mStreamTransactionHash.Count())
    Close(NS_OK);
  return rv;
}

void
SpdySession3::UpdateLocalRwin(SpdyStream3 *stream,
                              uint32_t bytes)
{
  // If this data packet was not for a valid or live stream then there
  // is no reason to mess with the flow control
  if (!stream || stream->RecvdFin())
    return;

  stream->DecrementLocalWindow(bytes);

  // Don't necessarily ack every data packet. Only do it
  // after a significant amount of data.
  uint64_t unacked = stream->LocalUnAcked();
  int64_t  localWindow = stream->LocalWindow();

  LOG3(("SpdySession3::UpdateLocalRwin this=%p id=0x%X newbytes=%u "
        "unacked=%llu localWindow=%lld\n",
        this, stream->StreamID(), bytes, unacked, localWindow));

  if (!unacked)
    return;

  if ((unacked < kMinimumToAck) && (localWindow > kEmergencyWindowThreshold))
    return;

  if (!stream->HasSink()) {
    LOG3(("SpdySession3::UpdateLocalRwin %p 0x%X Pushed Stream Has No Sink\n",
          this, stream->StreamID()));
    return;
  }

  // Generate window updates directly out of spdysession instead of the stream
  // in order to avoid queue delays in getting the 'ACK' out.
  uint32_t toack = (unacked <= 0x7fffffffU) ? unacked : 0x7fffffffU;

  LOG3(("SpdySession3::UpdateLocalRwin Ack this=%p id=0x%X acksize=%d\n",
        this, stream->StreamID(), toack));
  stream->IncrementLocalWindow(toack);

  static const uint32_t dataLen = 8;
  EnsureBuffer(mOutputQueueBuffer, mOutputQueueUsed + 8 + dataLen,
               mOutputQueueUsed, mOutputQueueSize);
  char *packet = mOutputQueueBuffer.get() + mOutputQueueUsed;
  mOutputQueueUsed += 8 + dataLen;

  memset(packet, 0, 8 + dataLen);
  packet[0] = kFlag_Control;
  packet[1] = kVersion;
  packet[3] = CONTROL_TYPE_WINDOW_UPDATE;
  packet[7] = dataLen;

  NetworkEndian::writeUint32(packet + 8, stream->StreamID());
  NetworkEndian::writeUint32(packet + 12, toack);

  LogIO(this, stream, "Window Update", packet, 8 + dataLen);
  FlushOutputQueue();
}

void
SpdySession3::Close(nsresult aReason)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  if (mClosed)
    return;

  LOG3(("SpdySession3::Close %p %X", this, aReason));

  mClosed = true;

  mStreamTransactionHash.Enumerate(ShutdownEnumerator, this);
  mStreamIDHash.Clear();
  mStreamTransactionHash.Clear();

  uint32_t goAwayReason;
  if (NS_SUCCEEDED(aReason)) {
    goAwayReason = OK;
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

nsHttpConnectionInfo *
SpdySession3::ConnectionInfo()
{
  nsRefPtr<nsHttpConnectionInfo> ci;
  GetConnectionInfo(getter_AddRefs(ci));
  return ci.get();
}

void
SpdySession3::CloseTransaction(nsAHttpTransaction *aTransaction,
                               nsresult aResult)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  LOG3(("SpdySession3::CloseTransaction %p %p %x", this, aTransaction, aResult));

  // Generally this arrives as a cancel event from the connection manager.

  // need to find the stream and call CleanupStream() on it.
  SpdyStream3 *stream = mStreamTransactionHash.Get(aTransaction);
  if (!stream) {
    LOG3(("SpdySession3::CloseTransaction %p %p %x - not found.",
          this, aTransaction, aResult));
    return;
  }
  LOG3(("SpdySession3::CloseTranscation probably a cancel. "
        "this=%p, trans=%p, result=%x, streamID=0x%X stream=%p",
        this, aTransaction, aResult, stream->StreamID(), stream));
  CleanupStream(stream, aResult, RST_CANCEL);
  ResumeRecv();
}


//-----------------------------------------------------------------------------
// nsAHttpSegmentReader
//-----------------------------------------------------------------------------

nsresult
SpdySession3::OnReadSegment(const char *buf,
                            uint32_t count,
                            uint32_t *countRead)
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
SpdySession3::CommitToSegmentSize(uint32_t count, bool forceCommitment)
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

  MOZ_ASSERT((mOutputQueueUsed + count) <= (mOutputQueueSize - kQueueReserved),
             "buffer not as large as expected");

  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsAHttpSegmentWriter
//-----------------------------------------------------------------------------

nsresult
SpdySession3::OnWriteSegment(char *buf,
                             uint32_t count,
                             uint32_t *countWritten)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
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

  if (mDownstreamState == PROCESSING_COMPLETE_HEADERS) {

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

    if (mFlatHTTPResponseHeaders.Length() == mFlatHTTPResponseHeadersOut) {
      if (mDataPending) {
        // Now ready to process data frames - pop PROCESING_DATA_FRAME back onto
        // the stack because receipt of that first data frame triggered the
        // response header processing
        mDataPending = false;
        ChangeDownstreamState(PROCESSING_DATA_FRAME);
      }
      else if (!mInputFrameDataLast) {
        // If more frames are expected in this stream, then reset the state so they can be
        // handled. Otherwise (e.g. a 0 length response with the fin on the SYN_REPLY)
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
SpdySession3::SetNeedsCleanup()
{
  LOG3(("SpdySession3::SetNeedsCleanup %p - recorded downstream fin of "
        "stream %p 0x%X", this, mInputFrameDataStream,
        mInputFrameDataStream->StreamID()));

  // This will result in Close() being called
  MOZ_ASSERT(!mNeedsCleanup, "mNeedsCleanup unexpectedly set");
  mNeedsCleanup = mInputFrameDataStream;
  ResetDownstreamState();
}

void
SpdySession3::ConnectPushedStream(SpdyStream3 *stream)
{
  mReadyForRead.Push(stream);
  ForceRecv();
}

uint32_t
SpdySession3::FindTunnelCount(nsHttpConnectionInfo *aConnInfo)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  uint32_t rv = 0;
  mTunnelHash.Get(aConnInfo->HashKey(), &rv);
  return rv;
}

void
SpdySession3::RegisterTunnel(SpdyStream3 *aTunnel)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  nsHttpConnectionInfo *ci = aTunnel->Transaction()->ConnectionInfo();
  uint32_t newcount = FindTunnelCount(ci) + 1;
  mTunnelHash.Remove(ci->HashKey());
  mTunnelHash.Put(ci->HashKey(), newcount);
  LOG3(("SpdySession3::RegisterTunnel %p stream=%p tunnels=%d [%s]",
        this, aTunnel, newcount, ci->HashKey().get()));
}

void
SpdySession3::UnRegisterTunnel(SpdyStream3 *aTunnel)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  nsHttpConnectionInfo *ci = aTunnel->Transaction()->ConnectionInfo();
  MOZ_ASSERT(FindTunnelCount(ci));
  uint32_t newcount = FindTunnelCount(ci) - 1;
  mTunnelHash.Remove(ci->HashKey());
  if (newcount) {
    mTunnelHash.Put(ci->HashKey(), newcount);
  }
  LOG3(("SpdySession3::UnRegisterTunnel %p stream=%p tunnels=%d [%s]",
        this, aTunnel, newcount, ci->HashKey().get()));
}

void
SpdySession3::DispatchOnTunnel(nsAHttpTransaction *aHttpTransaction,
                               nsIInterfaceRequestor *aCallbacks)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  nsHttpTransaction *trans = aHttpTransaction->QueryHttpTransaction();
  nsHttpConnectionInfo *ci = aHttpTransaction->ConnectionInfo();
  MOZ_ASSERT(trans);

  LOG3(("SpdySession3::DispatchOnTunnel %p trans=%p", this, trans));

  aHttpTransaction->SetConnection(nullptr);

  // this transaction has done its work of setting up a tunnel, let
  // the connection manager queue it if necessary
  trans->SetDontRouteViaWildCard(true);
  trans->EnableKeepAlive();

  if (FindTunnelCount(ci) < gHttpHandler->MaxConnectionsPerOrigin()) {
    LOG3(("SpdySession3::DispatchOnTunnel %p create on new tunnel %s",
          this, ci->HashKey().get()));
    // The connect transaction will hold onto the underlying http
    // transaction so that an auth created by the connect can be mappped
    // to the correct security callbacks
    nsRefPtr<SpdyConnectTransaction> connectTrans =
      new SpdyConnectTransaction(ci, aCallbacks,
                                 trans->Caps(), trans, this);
    AddStream(connectTrans, trans->Priority(),
              false, nullptr);
    SpdyStream3 *tunnel = mStreamTransactionHash.Get(connectTrans);
    MOZ_ASSERT(tunnel);
    RegisterTunnel(tunnel);
  } else {
    // requeue it. The connection manager is responsible for actually putting
    // this on the tunnel connection with the specific ci now that it
    // has DontRouteViaWildCard set.
    LOG3(("SpdySession3::DispatchOnTunnel %p trans=%p queue in connection manager",
          this, trans));
    gHttpHandler->InitiateTransaction(trans, trans->Priority());
  }
}

//-----------------------------------------------------------------------------
// Modified methods of nsAHttpConnection
//-----------------------------------------------------------------------------

void
SpdySession3::TransactionHasDataToWrite(nsAHttpTransaction *caller)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  LOG3(("SpdySession3::TransactionHasDataToWrite %p trans=%p", this, caller));

  // a trapped signal from the http transaction to the connection that
  // it is no longer blocked on read.

  SpdyStream3 *stream = mStreamTransactionHash.Get(caller);
  if (!stream || !VerifyStream(stream)) {
    LOG3(("SpdySession3::TransactionHasDataToWrite %p caller %p not found",
          this, caller));
    return;
  }

  LOG3(("SpdySession3::TransactionHasDataToWrite %p ID is 0x%X\n",
        this, stream->StreamID()));

  mReadyForWrite.Push(stream);
  SetWriteCallbacks();

  // NSPR poll will not poll the network if there are non system PR_FileDesc's
  // that are ready - so we can get into a deadlock waiting for the system IO
  // to come back here if we don't force the send loop manually.
  ForceSend();
}

void
SpdySession3::TransactionHasDataToWrite(SpdyStream3 *stream)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  LOG3(("SpdySession3::TransactionHasDataToWrite %p stream=%p ID=%x",
        this, stream, stream->StreamID()));

  mReadyForWrite.Push(stream);
  SetWriteCallbacks();
  ForceSend();
}

bool
SpdySession3::IsPersistent()
{
  return true;
}

nsresult
SpdySession3::TakeTransport(nsISocketTransport **,
                            nsIAsyncInputStream **,
                            nsIAsyncOutputStream **)
{
  MOZ_ASSERT(false, "TakeTransport of SpdySession3");
  return NS_ERROR_UNEXPECTED;
}

nsHttpConnection *
SpdySession3::TakeHttpConnection()
{
  MOZ_ASSERT(false, "TakeHttpConnection of SpdySession3");
  return nullptr;
}

uint32_t
SpdySession3::CancelPipeline(nsresult reason)
{
  // we don't pipeline inside spdy, so this isn't an issue
  return 0;
}

nsAHttpTransaction::Classifier
SpdySession3::Classification()
{
  if (!mConnection)
    return nsAHttpTransaction::CLASS_GENERAL;
  return mConnection->Classification();
}

void
SpdySession3::GetSecurityCallbacks(nsIInterfaceRequestor **aOut)
{
  *aOut = nullptr;
}

//-----------------------------------------------------------------------------
// unused methods of nsAHttpTransaction
// We can be sure of this because SpdySession3 is only constructed in
// nsHttpConnection and is never passed out of that object or a TLSFilterTransaction
// TLS tunnel
//-----------------------------------------------------------------------------

void
SpdySession3::SetConnection(nsAHttpConnection *)
{
  // This is unexpected
  MOZ_ASSERT(false, "SpdySession3::SetConnection()");
}

void
SpdySession3::SetProxyConnectFailed()
{
  MOZ_ASSERT(false, "SpdySession3::SetProxyConnectFailed()");
}

bool
SpdySession3::IsDone()
{
  return !mStreamTransactionHash.Count();
}

nsresult
SpdySession3::Status()
{
  MOZ_ASSERT(false, "SpdySession3::Status()");
  return NS_ERROR_UNEXPECTED;
}

uint32_t
SpdySession3::Caps()
{
  MOZ_ASSERT(false, "SpdySession3::Caps()");
  return 0;
}

void
SpdySession3::SetDNSWasRefreshed()
{
}

uint64_t
SpdySession3::Available()
{
  MOZ_ASSERT(false, "SpdySession3::Available()");
  return 0;
}

nsHttpRequestHead *
SpdySession3::RequestHead()
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  MOZ_ASSERT(false,
             "SpdySession3::RequestHead() "
             "should not be called after SPDY is setup");
  return nullptr;
}

uint32_t
SpdySession3::Http1xTransactionCount()
{
  return 0;
}

// used as an enumerator by TakeSubTransactions()
static PLDHashOperator
TakeStream(nsAHttpTransaction *key,
           nsAutoPtr<SpdyStream3> &stream,
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
SpdySession3::TakeSubTransactions(
    nsTArray<nsRefPtr<nsAHttpTransaction> > &outTransactions)
{
  // Generally this cannot be done with spdy as transactions are
  // started right away.

  LOG3(("SpdySession3::TakeSubTransactions %p\n", this));

  if (mConcurrentHighWater > 0)
    return NS_ERROR_ALREADY_OPENED;

  LOG3(("   taking %d\n", mStreamTransactionHash.Count()));

  mStreamTransactionHash.Enumerate(TakeStream, &outTransactions);
  return NS_OK;
}

nsresult
SpdySession3::AddTransaction(nsAHttpTransaction *)
{
  // This API is meant for pipelining, SpdySession3's should be
  // extended with AddStream()

  MOZ_ASSERT(false,
             "SpdySession3::AddTransaction() should not be called");

  return NS_ERROR_NOT_IMPLEMENTED;
}

uint32_t
SpdySession3::PipelineDepth()
{
  return IsDone() ? 0 : 1;
}

nsresult
SpdySession3::SetPipelinePosition(int32_t position)
{
  // This API is meant for pipelining, SpdySession3's should be
  // extended with AddStream()

  MOZ_ASSERT(false,
             "SpdySession3::SetPipelinePosition() should not be called");

  return NS_ERROR_NOT_IMPLEMENTED;
}

int32_t
SpdySession3::PipelinePosition()
{
    return 0;
}

//-----------------------------------------------------------------------------
// Pass through methods of nsAHttpConnection
//-----------------------------------------------------------------------------

nsAHttpConnection *
SpdySession3::Connection()
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  return mConnection;
}

nsresult
SpdySession3::OnHeadersAvailable(nsAHttpTransaction *transaction,
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
SpdySession3::IsReused()
{
  return mConnection->IsReused();
}

nsresult
SpdySession3::PushBack(const char *buf, uint32_t len)
{
  return mConnection->PushBack(buf, len);
}

} // namespace mozilla::net
} // namespace mozilla
