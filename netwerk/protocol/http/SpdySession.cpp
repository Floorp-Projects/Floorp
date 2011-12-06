/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Patrick McManus <mcmanus@ducksong.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsHttp.h"
#include "SpdySession.h"
#include "SpdyStream.h"
#include "nsHttpConnection.h"
#include "prnetdb.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Preferences.h"
#include "prprf.h"

#ifdef DEBUG
// defined by the socket transport service while active
extern PRThread *gSocketThread;
#endif

namespace mozilla {
namespace net {

// SpdySession has multiple inheritance of things that implement
// nsISupports, so this magic is taken from nsHttpPipeline that
// implements some of the same abstract classes.
NS_IMPL_THREADSAFE_ADDREF(SpdySession)
NS_IMPL_THREADSAFE_RELEASE(SpdySession)
NS_INTERFACE_MAP_BEGIN(SpdySession)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsAHttpConnection)
NS_INTERFACE_MAP_END

SpdySession::SpdySession(nsAHttpTransaction *aHttpTransaction,
                         nsISocketTransport *aSocketTransport,
                         PRInt32 firstPriority)
  : mSocketTransport(aSocketTransport),
    mSegmentReader(nsnull),
    mSegmentWriter(nsnull),
    mSendingChunkSize(kSendingChunkSize),
    mNextStreamID(1),
    mConcurrentHighWater(0),
    mDownstreamState(BUFFERING_FRAME_HEADER),
    mPartialFrame(nsnull),
    mFrameBufferSize(kDefaultBufferSize),
    mFrameBufferUsed(0),
    mFrameDataLast(false),
    mFrameDataStream(nsnull),
    mNeedsCleanup(nsnull),
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
    mOutputQueueSent(0)
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");

  LOG3(("SpdySession::SpdySession %p transaction 1 = %p",
        this, aHttpTransaction));
  
  mStreamIDHash.Init();
  mStreamTransactionHash.Init();
  mConnection = aHttpTransaction->Connection();
  mFrameBuffer = new char[mFrameBufferSize];
  mDecompressBuffer = new char[mDecompressBufferSize];
  mOutputQueueBuffer = new char[mOutputQueueSize];
  zlibInit();
  
  mSendingChunkSize =
    Preferences::GetInt("network.http.spdy.chunk-size", kSendingChunkSize);
  AddStream(aHttpTransaction, firstPriority);
}

PLDHashOperator
SpdySession::Shutdown(nsAHttpTransaction *key,
                      nsAutoPtr<SpdyStream> &stream,
                      void *closure)
{
  SpdySession *self = static_cast<SpdySession *>(closure);
  
  if (self->mCleanShutdown &&
      self->mGoAwayID < stream->StreamID())
    stream->Close(NS_ERROR_NET_RESET); // can be restarted
  else
    stream->Close(NS_ERROR_ABORT);

  return PL_DHASH_NEXT;
}

SpdySession::~SpdySession()
{
  LOG3(("SpdySession::~SpdySession %p", this));

  inflateEnd(&mDownstreamZlib);
  deflateEnd(&mUpstreamZlib);
  
  mStreamTransactionHash.Enumerate(Shutdown, this);
  Telemetry::Accumulate(Telemetry::SPDY_PARALLEL_STREAMS, mConcurrentHighWater);
  Telemetry::Accumulate(Telemetry::SPDY_TOTAL_STREAMS, (mNextStreamID - 1) / 2);
  Telemetry::Accumulate(Telemetry::SPDY_SERVER_INITIATED_STREAMS,
                        mServerPushedResources);
}

void
SpdySession::LogIO(SpdySession *self, SpdyStream *stream, const char *label,
                   const char *data, PRUint32 datalen)
{
  if (!LOG4_ENABLED())
    return;
  
  LOG4(("SpdySession::LogIO %p stream=%p id=0x%X [%s]",
        self, stream, stream ? stream->StreamID() : 0, label));

  // Max line is (16 * 3) + 10(prefix) + newline + null
  char linebuf[128];
  PRUint32 index;
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

typedef nsresult  (*Control_FX) (SpdySession *self);
static Control_FX sControlFunctions[] = 
{
  nsnull,
  SpdySession::HandleSynStream,
  SpdySession::HandleSynReply,
  SpdySession::HandleRstStream,
  SpdySession::HandleSettings,
  SpdySession::HandleNoop,
  SpdySession::HandlePing,
  SpdySession::HandleGoAway,
  SpdySession::HandleHeaders,
  SpdySession::HandleWindowUpdate
};

bool
SpdySession::RoomForMoreConcurrent()
{
  return (mConcurrent < mMaxConcurrent);
}

bool
SpdySession::RoomForMoreStreams()
{
  if (mNextStreamID + mStreamTransactionHash.Count() * 2 > kMaxStreamID)
    return false;

  return !mShouldGoAway;
}

PRUint32
SpdySession::RegisterStreamID(SpdyStream *stream)
{
  LOG3(("SpdySession::RegisterStreamID session=%p stream=%p id=0x%X "
        "concurrent=%d",this, stream, mNextStreamID, mConcurrent));

  NS_ABORT_IF_FALSE(mNextStreamID < 0xfffffff0,
                    "should have stopped admitting streams");
  
  PRUint32 result = mNextStreamID;
  mNextStreamID += 2;

  // We've used up plenty of ID's on this session. Start
  // moving to a new one before there is a crunch involving
  // server push streams or concurrent non-registered submits
  if (mNextStreamID >= kMaxStreamID)
    mShouldGoAway = true;

  mStreamIDHash.Put(result, stream);
  return result;
}

bool
SpdySession::AddStream(nsAHttpTransaction *aHttpTransaction,
                       PRInt32 aPriority)
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");
  NS_ABORT_IF_FALSE(!mStreamTransactionHash.Get(aHttpTransaction),
                    "AddStream duplicate transaction pointer");

  aHttpTransaction->SetConnection(this);
  SpdyStream *stream = new SpdyStream(aHttpTransaction,
                                      this,
                                      mSocketTransport,
                                      mSendingChunkSize,
                                      &mUpstreamZlib,
                                      aPriority);

  
  LOG3(("SpdySession::AddStream session=%p stream=%p NextID=0x%X (tentative)",
        this, stream, mNextStreamID));

  mStreamTransactionHash.Put(aHttpTransaction, stream);

  if (RoomForMoreConcurrent()) {
    LOG3(("SpdySession::AddStream %p stream %p activated immediately.",
          this, stream));
    ActivateStream(stream);
  }
  else {
    LOG3(("SpdySession::AddStream %p stream %p queued.",
          this, stream));
    mQueuedStreams.Push(stream);
  }
  
  return true;
}

void
SpdySession::ActivateStream(SpdyStream *stream)
{
  mConcurrent++;
  if (mConcurrent > mConcurrentHighWater)
    mConcurrentHighWater = mConcurrent;
  LOG3(("SpdySession::AddStream %p activating stream %p Currently %d"
        "streams in session, high water mark is %d",
        this, stream, mConcurrent, mConcurrentHighWater));

  mReadyForWrite.Push(stream);
  SetWriteCallbacks(stream->Transaction());

  // Kick off the SYN transmit without waiting for the poll loop
  PRUint32 countRead;
  ReadSegments(nsnull, kDefaultBufferSize, &countRead);
}

void
SpdySession::ProcessPending()
{
  while (RoomForMoreConcurrent()) {
    SpdyStream *stream = static_cast<SpdyStream *>(mQueuedStreams.PopFront());
    if (!stream)
      return;
    LOG3(("SpdySession::ProcessPending %p stream %p activated from queue.",
          this, stream));
    ActivateStream(stream);
  }
}

void
SpdySession::SetWriteCallbacks(nsAHttpTransaction *aTrans)
{
  if (mConnection && (WriteQueueSize() || mOutputQueueUsed))
      mConnection->ResumeSend(aTrans);
}

void
SpdySession::FlushOutputQueue()
{
  if (!mSegmentReader || !mOutputQueueUsed)
    return;
  
  nsresult rv;
  PRUint32 countRead;
  PRUint32 avail = mOutputQueueUsed - mOutputQueueSent;

  rv = mSegmentReader->
    OnReadSegment(mOutputQueueBuffer.get() + mOutputQueueSent, avail,
                                     &countRead);
  LOG3(("SpdySession::FlushOutputQueue %p sz=%d rv=%x actual=%d",
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
  if (mOutputQueueSize - mOutputQueueUsed < kQueueTailRoom) {
    // The output queue is filling up and we just sent some data out, so
    // this is a good time to rearrange the output queue.

    mOutputQueueUsed -= mOutputQueueSent;
    memmove(mOutputQueueBuffer.get(),
            mOutputQueueBuffer.get() + mOutputQueueSent,
            mOutputQueueUsed);
    mOutputQueueSent = 0;
  }
}

void
SpdySession::DontReuse()
{
  mShouldGoAway = true;
  if(!mStreamTransactionHash.Count())
    Close(NS_OK);
}

PRUint32
SpdySession::WriteQueueSize()
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");

  PRUint32 count = mUrgentForWrite.GetSize() + mReadyForWrite.GetSize();

  if (mPartialFrame)
    ++count;
  return count;
}

void
SpdySession::ChangeDownstreamState(enum stateType newState)
{
  LOG3(("SpdyStream::ChangeDownstreamState() %p from %X to %X",
        this, mDownstreamState, newState));
  mDownstreamState = newState;

  if (mDownstreamState == BUFFERING_FRAME_HEADER) {
    if (mFrameDataLast && mFrameDataStream) {
      mFrameDataLast = 0;
      if (!mFrameDataStream->RecvdFin()) {
        mFrameDataStream->SetRecvdFin(true);
        --mConcurrent;
        ProcessPending();
      }
    }
    mFrameBufferUsed = 0;
    mFrameDataStream = nsnull;
  }
  
  return;
}

void
SpdySession::EnsureBuffer(nsAutoArrayPtr<char> &buf,
                          PRUint32 newSize,
                          PRUint32 preserve,
                          PRUint32 &objSize)
{
  if (objSize >= newSize)
    return;
  
  objSize = newSize;
  nsAutoArrayPtr<char> tmp(new char[objSize]);
  memcpy (tmp, buf, preserve);
  buf = tmp;
}

void
SpdySession::zlibInit()
{
  mDownstreamZlib.zalloc = SpdyStream::zlib_allocator;
  mDownstreamZlib.zfree = SpdyStream::zlib_destructor;
  mDownstreamZlib.opaque = Z_NULL;

  inflateInit(&mDownstreamZlib);

  mUpstreamZlib.zalloc = SpdyStream::zlib_allocator;
  mUpstreamZlib.zfree = SpdyStream::zlib_destructor;
  mUpstreamZlib.opaque = Z_NULL;

  deflateInit(&mUpstreamZlib, Z_DEFAULT_COMPRESSION);
  deflateSetDictionary(&mUpstreamZlib,
                       reinterpret_cast<const unsigned char *>
                       (SpdyStream::kDictionary),
                       strlen(SpdyStream::kDictionary) + 1);

}

nsresult
SpdySession::DownstreamUncompress(char *blockStart, PRUint32 blockLen)
{
  mDecompressBufferUsed = 0;

  mDownstreamZlib.avail_in = blockLen;
  mDownstreamZlib.next_in = reinterpret_cast<unsigned char *>(blockStart);

  do {
    mDownstreamZlib.next_out =
      reinterpret_cast<unsigned char *>(mDecompressBuffer.get()) +
      mDecompressBufferUsed;
    mDownstreamZlib.avail_out = mDecompressBufferSize - mDecompressBufferUsed;
    int zlib_rv = inflate(&mDownstreamZlib, Z_NO_FLUSH);

    if (zlib_rv == Z_NEED_DICT)
      inflateSetDictionary(&mDownstreamZlib,
                           reinterpret_cast<const unsigned char *>
                           (SpdyStream::kDictionary),
                           strlen(SpdyStream::kDictionary) + 1);
    
    if (zlib_rv == Z_DATA_ERROR || zlib_rv == Z_MEM_ERROR)
      return NS_ERROR_FAILURE;

    mDecompressBufferUsed += mDecompressBufferSize - mDecompressBufferUsed -
      mDownstreamZlib.avail_out;
    
    // When there is no more output room, but input still available then
    // increase the output space
    if (zlib_rv == Z_OK &&
        !mDownstreamZlib.avail_out && mDownstreamZlib.avail_in) {
      LOG3(("SpdySession::DownstreamUncompress %p Large Headers - so far %d",
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
SpdySession::FindHeader(nsCString name,
                        nsDependentCSubstring &value)
{
  const unsigned char *nvpair = reinterpret_cast<unsigned char *>
    (mDecompressBuffer.get()) + 2;
  const unsigned char *lastHeaderByte = reinterpret_cast<unsigned char *>
    (mDecompressBuffer.get()) + mDecompressBufferUsed;
  if (lastHeaderByte < nvpair)
    return NS_ERROR_ILLEGAL_VALUE;
  PRUint16 numPairs =
    PR_ntohs(reinterpret_cast<PRUint16 *>(mDecompressBuffer.get())[0]);
  for (PRUint16 index = 0; index < numPairs; ++index) {
    if (lastHeaderByte < nvpair + 2)
      return NS_ERROR_ILLEGAL_VALUE;
    PRUint32 nameLen = (nvpair[0] << 8) + nvpair[1];
    if (lastHeaderByte < nvpair + 2 + nameLen)
      return NS_ERROR_ILLEGAL_VALUE;
    nsDependentCSubstring nameString =
      Substring (reinterpret_cast<const char *>(nvpair) + 2,
                 reinterpret_cast<const char *>(nvpair) + 2 + nameLen);
    if (lastHeaderByte < nvpair + 4 + nameLen)
      return NS_ERROR_ILLEGAL_VALUE;
    PRUint16 valueLen = (nvpair[2 + nameLen] << 8) + nvpair[3 + nameLen];
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
SpdySession::ConvertHeaders(nsDependentCSubstring &status,
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

  PRUint16 numPairs =
    PR_ntohs(reinterpret_cast<PRUint16 *>(mDecompressBuffer.get())[0]);

  for (PRUint16 index = 0; index < numPairs; ++index) {
    if (lastHeaderByte < nvpair + 2)
      return NS_ERROR_ILLEGAL_VALUE;

    PRUint32 nameLen = (nvpair[0] << 8) + nvpair[1];
    if (lastHeaderByte < nvpair + 2 + nameLen)
      return NS_ERROR_ILLEGAL_VALUE;

    nsDependentCSubstring nameString =
      Substring (reinterpret_cast<const char *>(nvpair) + 2,
                 reinterpret_cast<const char *>(nvpair) + 2 + nameLen);

    // a null in the name string is particularly wrong because it will
    // break the fix-up-nulls-in-value-string algorithm.
    if (nameString.FindChar(0) != -1)
      return NS_ERROR_ILLEGAL_VALUE;

    if (lastHeaderByte < nvpair + 4 + nameLen)
      return NS_ERROR_ILLEGAL_VALUE;
    PRUint16 valueLen = (nvpair[2 + nameLen] << 8) + nvpair[3 + nameLen];
    if (lastHeaderByte < nvpair + 4 + nameLen + valueLen)
      return NS_ERROR_ILLEGAL_VALUE;
    
    // Look for upper case characters in the name. They are illegal.
    for (char *cPtr = nameString.BeginWriting();
         cPtr && cPtr < nameString.EndWriting();
         ++cPtr) {
      if (*cPtr <= 'Z' && *cPtr >= 'A') {
        nsCString toLog(nameString);

        LOG3(("SpdySession::ConvertHeaders session=%p stream=%p "
              "upper case response header found. [%s]\n",
              this, mFrameDataStream, toLog.get()));

        return NS_ERROR_ILLEGAL_VALUE;
      }
    }

    // HTTP Chunked responses are not legal over spdy. We do not need
    // to look for chunked specifically because it is the only HTTP
    // allowed default encoding and we did not negotiate further encodings
    // via TE
    if (nameString.Equals(NS_LITERAL_CSTRING("transfer-encoding"))) {
      LOG3(("SpdySession::ConvertHeaders session=%p stream=%p "
            "transfer-encoding found. Chunked is invalid and no TE sent.",
            this, mFrameDataStream));

      return NS_ERROR_ILLEGAL_VALUE;
    }

    if (!nameString.Equals(NS_LITERAL_CSTRING("version")) &&
        !nameString.Equals(NS_LITERAL_CSTRING("status")) &&
        !nameString.Equals(NS_LITERAL_CSTRING("connection")) &&
        !nameString.Equals(NS_LITERAL_CSTRING("keep-alive"))) {
      nsDependentCSubstring valueString =
        Substring (reinterpret_cast<const char *>(nvpair) + 4 + nameLen,
                   reinterpret_cast<const char *>(nvpair) + 4 + nameLen +
                   valueLen);
      
      mFlatHTTPResponseHeaders.Append(nameString);
      mFlatHTTPResponseHeaders.Append(NS_LITERAL_CSTRING(": "));

      PRInt32 valueIndex;
      // NULLs are really "\r\nhdr: "
      while ((valueIndex = valueString.FindChar(0)) != -1) {
        nsCString replacement = NS_LITERAL_CSTRING("\r\n");
        replacement.Append(nameString);
        replacement.Append(NS_LITERAL_CSTRING(": "));
        valueString.Replace(valueIndex, 1, replacement);
      }

      mFlatHTTPResponseHeaders.Append(valueString);
      mFlatHTTPResponseHeaders.Append(NS_LITERAL_CSTRING("\r\n"));
    }
    nvpair += 4 + nameLen + valueLen;
  }

  mFlatHTTPResponseHeaders.Append(
    NS_LITERAL_CSTRING("X-Firefox-Spdy: 1\r\n\r\n"));
  LOG (("decoded response headers are:\n%s",
        mFlatHTTPResponseHeaders.get()));
  
  return NS_OK;
}

void
SpdySession::GeneratePing(PRUint32 aID)
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");
  LOG3(("SpdySession::GeneratePing %p 0x%X\n", this, aID));

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
  memcpy (packet + 8, &aID, 4);

  FlushOutputQueue();
}

void
SpdySession::GenerateRstStream(PRUint32 aStatusCode, PRUint32 aID)
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");
  LOG3(("SpdySession::GenerateRst %p 0x%X %d\n", this, aID, aStatusCode));

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
  memcpy (packet + 8, &aID, 4);
  aStatusCode = PR_htonl(aStatusCode);
  memcpy (packet + 12, &aStatusCode, 4);

  FlushOutputQueue();
}

void
SpdySession::GenerateGoAway()
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");
  LOG3(("SpdySession::GenerateGoAway %p\n", this));

  EnsureBuffer(mOutputQueueBuffer, mOutputQueueUsed + 12,
               mOutputQueueUsed, mOutputQueueSize);
  char *packet = mOutputQueueBuffer.get() + mOutputQueueUsed;
  mOutputQueueUsed += 12;

  memset (packet, 0, 12);
  packet[0] = kFlag_Control;
  packet[1] = 2;                                  /* version 2 */
  packet[3] = CONTROL_TYPE_GOAWAY;
  packet[7] = 4;                                  /* data length */
  
  // last-good-stream-id are bytes 8-11, when we accept server push this will
  // need to be set non zero

  FlushOutputQueue();
}

void
SpdySession::CleanupStream(SpdyStream *aStream, nsresult aResult)
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");
  LOG3(("SpdySession::CleanupStream %p %p 0x%x %X\n",
        this, aStream, aStream->StreamID(), aResult));

  nsresult abortCode = NS_OK;

  if (!aStream->RecvdFin() && aStream->StreamID()) {
    LOG3(("Stream had not processed recv FIN, sending RST"));
    GenerateRstStream(RST_CANCEL, aStream->StreamID());
    --mConcurrent;
    ProcessPending();
  }
  
  // Check if partial frame writer
  if (mPartialFrame == aStream) {
    LOG3(("Stream had active partial write frame - need to abort session"));
    abortCode = aResult;
    if (NS_SUCCEEDED(abortCode))
      abortCode = NS_ERROR_ABORT;
    
    mPartialFrame = nsnull;
  }
  
  // Check if partial frame reader
  if (aStream == mFrameDataStream) {
    LOG3(("Stream had active partial read frame on close"));
    ChangeDownstreamState(DISCARD_DATA_FRAME);
    mFrameDataStream = nsnull;
  }

  // check the streams blocked on write, this is linear but the list
  // should be pretty short.
  PRUint32 size = mReadyForWrite.GetSize();
  for (PRUint32 count = 0; count < size; ++count) {
    SpdyStream *stream = static_cast<SpdyStream *>(mReadyForWrite.PopFront());
    if (stream != aStream)
      mReadyForWrite.Push(stream);
  }

  // Check the streams blocked on urgent (i.e. window update) writing.
  // This should also be short.
  size = mUrgentForWrite.GetSize();
  for (PRUint32 count = 0; count < size; ++count) {
    SpdyStream *stream = static_cast<SpdyStream *>(mUrgentForWrite.PopFront());
    if (stream != aStream)
      mUrgentForWrite.Push(stream);
  }

  // Remove the stream from the ID hash table. (this one isn't short, which is
  // why it is hashed.)
  mStreamIDHash.Remove(aStream->StreamID());

  // Send the stream the close() indication
  aStream->Close(aResult);

  // removing from the stream transaction hash will
  // delete the SpdyStream and drop the reference to
  // its transaction
  mStreamTransactionHash.Remove(aStream->Transaction());

  if (NS_FAILED(abortCode))
    Close(abortCode);
  else if (mShouldGoAway && !mStreamTransactionHash.Count())
    Close(NS_OK);
}

nsresult
SpdySession::HandleSynStream(SpdySession *self)
{
  NS_ABORT_IF_FALSE(self->mFrameControlType == CONTROL_TYPE_SYN_STREAM,
                    "wrong control type");
  
  if (self->mFrameDataSize < 12) {
    LOG3(("SpdySession::HandleSynStream %p SYN_STREAM too short data=%d",
          self, self->mFrameDataSize));
    return NS_ERROR_ILLEGAL_VALUE;
  }

  PRUint32 streamID =
    PR_ntohl(reinterpret_cast<PRUint32 *>(self->mFrameBuffer.get())[2]);

  LOG3(("SpdySession::HandleSynStream %p recv SYN_STREAM (push) for ID 0x%X.",
        self, streamID));
    
  if (streamID & 0x01) {                   // test for odd stream ID
    LOG3(("SpdySession::HandleSynStream %p recvd SYN_STREAM id must be even.",
          self));
    return NS_ERROR_ILLEGAL_VALUE;
  }

  ++(self->mServerPushedResources);

  // Anytime we start using the high bit of stream ID (either client or server)
  // begin to migrate to a new session.
  if (streamID >= kMaxStreamID)
    self->mShouldGoAway = true;

  // todo populate cache. For now, just reject server push p3
  self->GenerateRstStream(RST_REFUSED_STREAM, streamID);
  self->ChangeDownstreamState(BUFFERING_FRAME_HEADER);
  return NS_OK;
}

nsresult
SpdySession::HandleSynReply(SpdySession *self)
{
  NS_ABORT_IF_FALSE(self->mFrameControlType == CONTROL_TYPE_SYN_REPLY,
                    "wrong control type");

  if (self->mFrameDataSize < 8) {
    LOG3(("SpdySession::HandleSynReply %p SYN REPLY too short data=%d",
          self, self->mFrameDataSize));
    return NS_ERROR_ILLEGAL_VALUE;
  }
  
  PRUint32 streamID =
    PR_ntohl(reinterpret_cast<PRUint32 *>(self->mFrameBuffer.get())[2]);
  self->mFrameDataStream = self->mStreamIDHash.Get(streamID);
  if (!self->mFrameDataStream) {
    LOG3(("SpdySession::HandleSynReply %p lookup streamID in syn_reply "
          "0x%X failed. NextStreamID = 0x%x", self, streamID,
          self->mNextStreamID));
    if (streamID >= self->mNextStreamID)
      self->GenerateRstStream(RST_INVALID_STREAM, streamID);
    
    // It is likely that this is a reply to a stream ID that has been canceled.
    // For the most part we would like to ignore it, but the header needs to be
    // be parsed to keep the compression context synchronized
    self->DownstreamUncompress(self->mFrameBuffer + 14,
                               self->mFrameDataSize - 6);
    self->ChangeDownstreamState(BUFFERING_FRAME_HEADER);
    return NS_OK;
  }
  
  if (!self->mFrameDataStream->SetFullyOpen()) {
    // "If an endpoint receives multiple SYN_REPLY frames for the same active
    // stream ID, it must drop the stream, and send a RST_STREAM for the
    // stream with the error PROTOCOL_ERROR."
    //
    // In addition to that we abort the session - this is a serious protocol
    // violation.

    self->GenerateRstStream(RST_PROTOCOL_ERROR, streamID);
    return NS_ERROR_ILLEGAL_VALUE;
  }

  self->mFrameDataLast = self->mFrameBuffer[4] & kFlag_Data_FIN;

  if (self->mFrameBuffer[4] & kFlag_Data_UNI) {
    LOG3(("SynReply had unidirectional flag set on it - nonsensical"));
    return NS_ERROR_ILLEGAL_VALUE;
  }

  LOG3(("SpdySession::HandleSynReply %p SYN_REPLY for 0x%X fin=%d",
        self, streamID, self->mFrameDataLast));
  
  // The spdystream needs to see flattened http headers
  // The Frame Buffer currently holds the complete SYN_REPLY
  // frame. The interesting data is at offset 14, where the
  // compressed name/value header block lives.
  // We unpack that into the mDecompressBuffer - we can't do
  // it streamed because the version and status information
  // is not guaranteed to be first. This is then finally
  // converted to HTTP format in mFlatHTTPResponseHeaders

  nsresult rv = self->DownstreamUncompress(self->mFrameBuffer + 14,
                                           self->mFrameDataSize - 6);
  if (NS_FAILED(rv))
    return rv;
  
  Telemetry::Accumulate(Telemetry::SPDY_SYN_REPLY_SIZE,
                        self->mFrameDataSize - 6);
  PRUint32 ratio =
    (self->mFrameDataSize - 6) * 100 / self->mDecompressBufferUsed;
  Telemetry::Accumulate(Telemetry::SPDY_SYN_REPLY_RATIO, ratio);

  // status and version are required.
  nsDependentCSubstring status, version;
  rv = self->FindHeader(NS_LITERAL_CSTRING("status"), status);
  if (NS_FAILED(rv))
    return rv;

  rv = self->FindHeader(NS_LITERAL_CSTRING("version"), version);
  if (NS_FAILED(rv))
    return rv;

  rv = self->ConvertHeaders(status, version);
  if (NS_FAILED(rv))
    return rv;

  self->ChangeDownstreamState(PROCESSING_CONTROL_SYN_REPLY);
  return NS_OK;
}

nsresult
SpdySession::HandleRstStream(SpdySession *self)
{
  NS_ABORT_IF_FALSE(self->mFrameControlType == CONTROL_TYPE_RST_STREAM,
                    "wrong control type");

  if (self->mFrameDataSize != 8) {
    LOG3(("SpdySession::HandleRstStream %p RST_STREAM wrong length data=%d",
          self, self->mFrameDataSize));
    return NS_ERROR_ILLEGAL_VALUE;
  }

  PRUint32 streamID =
    PR_ntohl(reinterpret_cast<PRUint32 *>(self->mFrameBuffer.get())[2]);

  self->mDownstreamRstReason =
    PR_ntohl(reinterpret_cast<PRUint32 *>(self->mFrameBuffer.get())[3]);

  LOG3(("SpdySession::HandleRstStream %p RST_STREAM Reason Code %u ID %x",
        self, self->mDownstreamRstReason, streamID));

  if (self->mDownstreamRstReason == RST_INVALID_STREAM ||
      self->mDownstreamRstReason == RST_FLOW_CONTROL_ERROR) {
    // basically just ignore this
    self->ChangeDownstreamState(BUFFERING_FRAME_HEADER);
    return NS_OK;
  }

  self->mFrameDataStream = self->mStreamIDHash.Get(streamID);
  if (!self->mFrameDataStream) {
    LOG3(("SpdySession::HandleRstStream %p lookup streamID for RST Frame "
          "0x%X failed", self, streamID));
    return NS_ERROR_ILLEGAL_VALUE;
  }
    
  self->ChangeDownstreamState(PROCESSING_CONTROL_RST_STREAM);
  return NS_OK;
}

nsresult
SpdySession::HandleSettings(SpdySession *self)
{
  NS_ABORT_IF_FALSE(self->mFrameControlType == CONTROL_TYPE_SETTINGS,
                    "wrong control type");

  if (self->mFrameDataSize < 4) {
    LOG3(("SpdySession::HandleSettings %p SETTINGS wrong length data=%d",
          self, self->mFrameDataSize));
    return NS_ERROR_ILLEGAL_VALUE;
  }

  PRUint32 numEntries =
    PR_ntohl(reinterpret_cast<PRUint32 *>(self->mFrameBuffer.get())[2]);

  // Ensure frame is large enough for supplied number of entries
  // Each entry is 8 bytes, frame data is reduced by 4 to account for
  // the NumEntries value.
  if ((self->mFrameDataSize - 4) < (numEntries * 8)) {
    LOG3(("SpdySession::HandleSettings %p SETTINGS wrong length data=%d",
          self, self->mFrameDataSize));
    return NS_ERROR_ILLEGAL_VALUE;
  }

  LOG3(("SpdySession::HandleSettings %p SETTINGS Control Frame with %d entries",
        self, numEntries));

  for (PRUint32 index = 0; index < numEntries; ++index) {
    // To clarify the v2 spec:
    // Each entry is a 24 bits of a little endian id
    // followed by 8 bits of flags
    // followed by a 32 bit big endian value
    
    unsigned char *setting = reinterpret_cast<unsigned char *>
      (self->mFrameBuffer.get()) + 12 + index * 8;

    PRUint32 id = (setting[2] << 16) + (setting[1] << 8) + setting[0];
    PRUint32 flags = setting[3];
    PRUint32 value =  PR_ntohl(reinterpret_cast<PRUint32 *>(setting)[1]);

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
  
  self->ChangeDownstreamState(BUFFERING_FRAME_HEADER);
  return NS_OK;
}

nsresult
SpdySession::HandleNoop(SpdySession *self)
{
  NS_ABORT_IF_FALSE(self->mFrameControlType == CONTROL_TYPE_NOOP,
                    "wrong control type");

  if (self->mFrameDataSize != 0) {
    LOG3(("SpdySession::HandleNoop %p NOP had data %d",
          self, self->mFrameDataSize));
    return NS_ERROR_ILLEGAL_VALUE;
  }

  LOG3(("SpdySession::HandleNoop %p NOP.", self));

  self->ChangeDownstreamState(BUFFERING_FRAME_HEADER);
  return NS_OK;
}

nsresult
SpdySession::HandlePing(SpdySession *self)
{
  NS_ABORT_IF_FALSE(self->mFrameControlType == CONTROL_TYPE_PING,
                    "wrong control type");

  if (self->mFrameDataSize != 4) {
    LOG3(("SpdySession::HandlePing %p PING had wrong amount of data %d",
          self, self->mFrameDataSize));
    return NS_ERROR_ILLEGAL_VALUE;
  }

  PRUint32 pingID =
    PR_ntohl(reinterpret_cast<PRUint32 *>(self->mFrameBuffer.get())[2]);

  LOG3(("SpdySession::HandlePing %p PING ID 0x%X.", self, pingID));

  if (pingID & 0x01) {
    // We never expect to see an odd PING beacuse we never generate PING.
      // The spec mandates ignoring this
    LOG3(("SpdySession::HandlePing %p PING ID from server was odd.",
          self));
  }
  else {
    self->GeneratePing(pingID);
  }
    
  self->ChangeDownstreamState(BUFFERING_FRAME_HEADER);
  return NS_OK;
}

nsresult
SpdySession::HandleGoAway(SpdySession *self)
{
  NS_ABORT_IF_FALSE(self->mFrameControlType == CONTROL_TYPE_GOAWAY,
                    "wrong control type");

  if (self->mFrameDataSize != 4) {
    LOG3(("SpdySession::HandleGoAway %p GOAWAY had wrong amount of data %d",
          self, self->mFrameDataSize));
    return NS_ERROR_ILLEGAL_VALUE;
  }

  self->mShouldGoAway = true;
  self->mGoAwayID =
    PR_ntohl(reinterpret_cast<PRUint32 *>(self->mFrameBuffer.get())[2]);
  self->mCleanShutdown = true;
  
  LOG3(("SpdySession::HandleGoAway %p GOAWAY Last-Good-ID 0x%X.",
        self, self->mGoAwayID));
  self->ResumeRecv(self);
  self->ChangeDownstreamState(BUFFERING_FRAME_HEADER);
  return NS_OK;
}

nsresult
SpdySession::HandleHeaders(SpdySession *self)
{
  NS_ABORT_IF_FALSE(self->mFrameControlType == CONTROL_TYPE_HEADERS,
                    "wrong control type");

  if (self->mFrameDataSize < 10) {
    LOG3(("SpdySession::HandleHeaders %p HEADERS had wrong amount of data %d",
          self, self->mFrameDataSize));
    return NS_ERROR_ILLEGAL_VALUE;
  }

  PRUint32 streamID =
    PR_ntohl(reinterpret_cast<PRUint32 *>(self->mFrameBuffer.get())[2]);

  // this is actually not legal in the HTTP mapping of SPDY. All
  // headers are in the syn or syn reply. Log and ignore it.

  LOG3(("SpdySession::HandleHeaders %p HEADERS for Stream 0x%X. "
        "They are ignored in the HTTP/SPDY mapping.",
        self, streamID));
  self->ChangeDownstreamState(BUFFERING_FRAME_HEADER);
  return NS_OK;
}

nsresult
SpdySession::HandleWindowUpdate(SpdySession *self)
{
  NS_ABORT_IF_FALSE(self->mFrameControlType == CONTROL_TYPE_WINDOW_UPDATE,
                    "wrong control type");
  LOG3(("SpdySession::HandleWindowUpdate %p WINDOW UPDATE was "
        "received. WINDOW UPDATE is no longer defined in v2. Ignoring.",
        self));

  self->ChangeDownstreamState(BUFFERING_FRAME_HEADER);
  return NS_OK;
}

// Used for the hashtable enumeration to propogate OnTransportStatus events
struct transportStatus
{
  nsITransport *transport;
  nsresult status;
  PRUint64 progress;
};

static PLDHashOperator
StreamTransportStatus(nsAHttpTransaction *key,
                      nsAutoPtr<SpdyStream> &stream,
                      void *closure)
{
  struct transportStatus *status =
    static_cast<struct transportStatus *>(closure);

  stream->Transaction()->OnTransportStatus(status->transport,
                                           status->status,
                                           status->progress);
  return PL_DHASH_NEXT;
}


//-----------------------------------------------------------------------------
// nsAHttpTransaction. It is expected that nsHttpConnection is the caller
// of these methods
//-----------------------------------------------------------------------------

void
SpdySession::OnTransportStatus(nsITransport* aTransport,
                               nsresult aStatus,
                               PRUint64 aProgress)
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");

  // nsHttpChannel synthesizes progress events in OnDataAvailable
  if (aStatus == nsISocketTransport::STATUS_RECEIVING_FROM)
    return;

  // STATUS_SENDING_TO is handled by SpdyStream
  if (aStatus == nsISocketTransport::STATUS_SENDING_TO)
    return;

  struct transportStatus status;
  
  status.transport = aTransport;
  status.status = aStatus;
  status.progress = aProgress;

  mStreamTransactionHash.Enumerate(StreamTransportStatus, &status);
}

// ReadSegments() is used to write data to the network. Generally, HTTP
// request data is pulled from the approriate transaction and
// converted to SPDY data. Sometimes control data like window-update are
// generated instead.

nsresult
SpdySession::ReadSegments(nsAHttpSegmentReader *reader,
                          PRUint32 count,
                          PRUint32 *countRead)
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");
  
  nsresult rv;
  *countRead = 0;

  // First priority goes to frames that were writing to the network but were
  // blocked part way through. Then to frames that have no streams (e.g ping
  // reply) and then third to streams marked urgent (generally they have
  // window updates), and finally to streams generally
  // ready to send data frames (http requests).

  LOG3(("SpdySession::ReadSegments %p partial frame stream=%p",
        this, mPartialFrame));

  SpdyStream *stream = mPartialFrame;
  mPartialFrame = nsnull;

  if (!stream)
    stream = static_cast<SpdyStream *>(mUrgentForWrite.PopFront());
  if (!stream)
    stream = static_cast<SpdyStream *>(mReadyForWrite.PopFront());
  if (!stream) {
    LOG3(("SpdySession %p could not identify a stream to write; suspending.",
          this));
    FlushOutputQueue();
    SetWriteCallbacks(nsnull);
    return NS_BASE_STREAM_WOULD_BLOCK;
  }
  
  LOG3(("SpdySession %p will write from SpdyStream %p", this, stream));

  NS_ABORT_IF_FALSE(!mSegmentReader || !reader || (mSegmentReader == reader),
                    "Inconsistent Write Function Callback");

  if (reader)
    mSegmentReader = reader;
  rv = stream->ReadSegments(this, count, countRead);

  FlushOutputQueue();

  if (stream->BlockedOnWrite()) {

    // We are writing a frame out, but it is blocked on the output stream.
    // Make sure to service that stream next write because we can only
    // multiplex between complete frames.

    LOG3(("SpdySession::ReadSegments %p dealing with block on write", this));

    NS_ABORT_IF_FALSE(!mPartialFrame, "partial frame should be empty");

    mPartialFrame = stream;
    SetWriteCallbacks(stream->Transaction());
    return rv;
  }

  if (stream->RequestBlockedOnRead()) {
    
    // We are blocked waiting for input - either more http headers or
    // any request body data. When more data from the request stream
    // becomes available the httptransaction will call conn->ResumeSend().
    
    LOG3(("SpdySession::ReadSegments %p dealing with block on read", this));

    // call readsegments again if there are other streams ready
    // to run in this session
    if (WriteQueueSize())
      rv = NS_OK;
    else
      rv = NS_BASE_STREAM_WOULD_BLOCK;
    SetWriteCallbacks(stream->Transaction());
    return rv;
  }
  
  NS_ABORT_IF_FALSE(rv != NS_BASE_STREAM_WOULD_BLOCK,
                    "Stream Would Block inconsistency");
  
  if (NS_FAILED(rv)) {
    LOG3(("SpdySession::ReadSegments %p returning FAIL code %X",
          this, rv));
    return rv;
  }
  
  if (*countRead > 0) {
    LOG3(("SpdySession::ReadSegments %p stream=%p generated end of frame %d",
          this, stream, *countRead));
    mReadyForWrite.Push(stream);
    SetWriteCallbacks(stream->Transaction());
    return rv;
  }
  
  LOG3(("SpdySession::ReadSegments %p stream=%p stream send complete",
        this, stream));
  
  // in normal http this is done by nshttpconnection, but that class does not
  // know which http transaction has made this state transition.
  stream->Transaction()->
    OnTransportStatus(mSocketTransport, nsISocketTransport::STATUS_WAITING_FOR,
                      LL_ZERO);
  /* we now want to recv data */
  mConnection->ResumeRecv(stream->Transaction());

  // call readsegments again if there are other streams ready
  // to go in this session
  SetWriteCallbacks(stream->Transaction());

  return rv;
}

// WriteSegments() is used to read data off the socket. Generally this is
// just the SPDY frame header and from there the appropriate SPDYStream
// is identified from the Stream-ID. The http transaction associated with
// that read then pulls in the data directly, which it will feed to
// OnWriteSegment(). That function will gateway it into http and feed
// it to the appropriate transaction.

// we call writer->OnWriteSegment to get a spdy header.. and decide if it is
// data or control.. if it is control, just deal with it.
// if it is data, identify the spdy stream
// call stream->WriteSegemnts which can call this::OnWriteSegment to get the
// data. It always gets full frames if they are part of the stream

nsresult
SpdySession::WriteSegments(nsAHttpSegmentWriter *writer,
                           PRUint32 count,
                           PRUint32 *countWritten)
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");
  
  nsresult rv;
  *countWritten = 0;

  if (mClosed)
    return NS_ERROR_FAILURE;

  SetWriteCallbacks(nsnull);
  
  // We buffer all control frames and act on them in this layer.
  // We buffer the first 8 bytes of data frames (the header) but
  // the actual data is passed through unprocessed.
  
  if (mDownstreamState == BUFFERING_FRAME_HEADER) {
    // The first 8 bytes of every frame is header information that
    // we are going to want to strip before passing to http. That is
    // true of both control and data packets.
    
    NS_ABORT_IF_FALSE(mFrameBufferUsed < 8,
                      "Frame Buffer Used Too Large for State");

    rv = writer->OnWriteSegment(mFrameBuffer + mFrameBufferUsed,
                                8 - mFrameBufferUsed,
                                countWritten);
    if (NS_FAILED(rv)) {
      if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
        ResumeRecv(nsnull);
      }
      return rv;
    }

    LogIO(this, nsnull, "Reading Frame Header",
          mFrameBuffer + mFrameBufferUsed, *countWritten);

    mFrameBufferUsed += *countWritten;

    if (mFrameBufferUsed < 8)
    {
      LOG3(("SpdySession::WriteSegments %p "
            "BUFFERING FRAME HEADER incomplete size=%d",
            this, mFrameBufferUsed));
      return rv;
    }

    // For both control and data frames the second 32 bit word of the header
    // is 8-flags, 24-length. (network byte order)
    mFrameDataSize =
      PR_ntohl(reinterpret_cast<PRUint32 *>(mFrameBuffer.get())[1]);
    mFrameDataSize &= 0x00ffffff;
    mFrameDataRead = 0;
    
    if (mFrameBuffer[0] & kFlag_Control) {
      EnsureBuffer(mFrameBuffer, mFrameDataSize + 8, 8, mFrameBufferSize);
      ChangeDownstreamState(BUFFERING_CONTROL_FRAME);
      
      // The first 32 bit word of the header is
      // 1 ctrl - 15 version - 16 type
      PRUint16 version =
        PR_ntohs(reinterpret_cast<PRUint16 *>(mFrameBuffer.get())[0]);
      version &= 0x7fff;
      
      mFrameControlType =
        PR_ntohs(reinterpret_cast<PRUint16 *>(mFrameBuffer.get())[1]);
      
      LOG3(("SpdySession::WriteSegments %p - Control Frame Identified "
            "type %d version %d data len %d",
            this, mFrameControlType, version, mFrameDataSize));

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

      PRUint32 streamID =
        PR_ntohl(reinterpret_cast<PRUint32 *>(mFrameBuffer.get())[0]);
      mFrameDataStream = mStreamIDHash.Get(streamID);
      if (!mFrameDataStream) {
        LOG3(("SpdySession::WriteSegments %p lookup streamID 0x%X failed. "
              "Next = 0x%x", this, streamID, mNextStreamID));
        if (streamID >= mNextStreamID)
          GenerateRstStream(RST_INVALID_STREAM, streamID);
          ChangeDownstreamState(DISCARD_DATA_FRAME);
      }
      mFrameDataLast = (mFrameBuffer[4] & kFlag_Data_FIN);
      Telemetry::Accumulate(Telemetry::SPDY_CHUNK_RECVD, mFrameDataSize >> 10);
      LOG3(("Start Processing Data Frame. "
            "Session=%p Stream ID 0x%x Stream Ptr %p Fin=%d Len=%d",
            this, streamID, mFrameDataStream, mFrameDataLast, mFrameDataSize));

      if (mFrameBuffer[4] & kFlag_Data_ZLIB) {
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

    // mFrameDataStream is reset by ChangeDownstreamState
    SpdyStream *stream = mFrameDataStream;
    ChangeDownstreamState(BUFFERING_FRAME_HEADER);
    CleanupStream(stream, rv);
    return NS_OK;
  }

  if (mDownstreamState == PROCESSING_DATA_FRAME ||
      mDownstreamState == PROCESSING_CONTROL_SYN_REPLY) {

    mSegmentWriter = writer;
    rv = mFrameDataStream->WriteSegments(this, count, countWritten);
    mSegmentWriter = nsnull;

    if (rv == NS_BASE_STREAM_CLOSED) {
      // This will happen when the transaction figures out it is EOF, generally
      // due to a content-length match being made
      SpdyStream *stream = mFrameDataStream;
      if (mFrameDataRead == mFrameDataSize)
        ChangeDownstreamState(BUFFERING_FRAME_HEADER);
      CleanupStream(stream, NS_OK);
      NS_ABORT_IF_FALSE(!mNeedsCleanup, "double cleanup out of data frame");
      return NS_OK;
    }
    
    if (mNeedsCleanup) {
      CleanupStream(mNeedsCleanup, NS_OK);
      mNeedsCleanup = nsnull;
    }

    // In v3 this is where we would generate a window update

    return rv;
  }

  if (mDownstreamState == DISCARD_DATA_FRAME) {
    char trash[4096];
    PRUint32 count = NS_MIN(4096U, mFrameDataSize - mFrameDataRead);

    if (!count) {
      ChangeDownstreamState(BUFFERING_FRAME_HEADER);
      *countWritten = 1;
      return NS_OK;
    }

    rv = writer->OnWriteSegment(trash, count, countWritten);

    if (NS_FAILED(rv)) {
      // maybe just blocked reading from network
      ResumeRecv(nsnull);
      return rv;
    }

    LogIO(this, nsnull, "Discarding Frame", trash, *countWritten);

    mFrameDataRead += *countWritten;

    if (mFrameDataRead == mFrameDataSize)
      ChangeDownstreamState(BUFFERING_FRAME_HEADER);
    return rv;
  }
  
  NS_ABORT_IF_FALSE(mDownstreamState == BUFFERING_CONTROL_FRAME,
                    "Not in Bufering Control Frame State");
  NS_ABORT_IF_FALSE(mFrameBufferUsed == 8,
                    "Frame Buffer Header Not Present");

  rv = writer->OnWriteSegment(mFrameBuffer + 8 + mFrameDataRead,
                              mFrameDataSize - mFrameDataRead,
                              countWritten);
  if (NS_FAILED(rv)) {
    // maybe just blocked reading from network
    ResumeRecv(nsnull);
    return rv;
  }

  LogIO(this, nsnull, "Reading Control Frame",
        mFrameBuffer + 8 + mFrameDataRead, *countWritten);

  mFrameDataRead += *countWritten;

  if (mFrameDataRead != mFrameDataSize)
    return NS_OK;

  rv = sControlFunctions[mFrameControlType](this);

  NS_ABORT_IF_FALSE(NS_FAILED(rv) ||
                    mDownstreamState != BUFFERING_CONTROL_FRAME,
                    "Control Handler returned OK but did not change state");

  if (mShouldGoAway && !mStreamTransactionHash.Count())
    Close(NS_OK);
  return rv;
}

void
SpdySession::Close(nsresult aReason)
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");

  if (mClosed)
    return;

  LOG3(("SpdySession::Close %p %X", this, aReason));

  mClosed = true;
  mStreamTransactionHash.Enumerate(Shutdown, this);
  GenerateGoAway();
  mConnection = nsnull;
}

void
SpdySession::CloseTransaction(nsAHttpTransaction *aTransaction,
                              nsresult aResult)
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");
  LOG3(("SpdySession::CloseTransaction %p %p %x", this, aTransaction, aResult));

  // Generally this arrives as a cancel event from the connection manager.

  // need to find the stream and call CleanupStream() on it.
  SpdyStream *stream = mStreamTransactionHash.Get(aTransaction);
  if (!stream) {
    LOG3(("SpdySession::CloseTransaction %p %p %x - not found.",
          this, aTransaction, aResult));
    return;
  }
  LOG3(("SpdySession::CloseTranscation probably a cancel. "
        "this=%p, trans=%p, result=%x, streamID=0x%X stream=%p",
        this, aTransaction, aResult, stream->StreamID(), stream));
  CleanupStream(stream, aResult);
  ResumeRecv(this);
}


//-----------------------------------------------------------------------------
// nsAHttpSegmentReader
//-----------------------------------------------------------------------------

nsresult
SpdySession::OnReadSegment(const char *buf,
                           PRUint32 count,
                           PRUint32 *countRead)
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");
  
  nsresult rv;
  
  if (!mOutputQueueUsed && mSegmentReader) {

    // try and write directly without output queue
    rv = mSegmentReader->OnReadSegment(buf, count, countRead);
    if (NS_SUCCEEDED(rv) || (rv != NS_BASE_STREAM_WOULD_BLOCK))
      return rv;
  }
  
  if (mOutputQueueUsed + count > mOutputQueueSize)
    FlushOutputQueue();

  if (mOutputQueueUsed + count > mOutputQueueSize)
    count = mOutputQueueSize - mOutputQueueUsed;

  if (!count)
    return NS_BASE_STREAM_WOULD_BLOCK;
  
  memcpy(mOutputQueueBuffer.get() + mOutputQueueUsed, buf, count);
  mOutputQueueUsed += count;
  *countRead = count;

  FlushOutputQueue();
    
  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsAHttpSegmentWriter
//-----------------------------------------------------------------------------

nsresult
SpdySession::OnWriteSegment(char *buf,
                            PRUint32 count,
                            PRUint32 *countWritten)
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");
  NS_ABORT_IF_FALSE(mSegmentWriter, "OnWriteSegment with null mSegmentWriter");
  nsresult rv;

  if (mDownstreamState == PROCESSING_DATA_FRAME) {

    if (mFrameDataLast &&
        mFrameDataRead == mFrameDataSize) {
      // This will result in Close() being called
      mNeedsCleanup = mFrameDataStream;

      LOG3(("SpdySession::OnWriteSegment %p - recorded downstream fin of "
            "stream %p 0x%X", this, mFrameDataStream,
            mFrameDataStream->StreamID()));
      *countWritten = 0;
      ChangeDownstreamState(BUFFERING_FRAME_HEADER);
      return NS_BASE_STREAM_CLOSED;
    }
    
    count = NS_MIN(count, mFrameDataSize - mFrameDataRead);
    rv = mSegmentWriter->OnWriteSegment(buf, count, countWritten);
    if (NS_FAILED(rv))
      return rv;

    LogIO(this, mFrameDataStream, "Reading Data Frame", buf, *countWritten);

    mFrameDataRead += *countWritten;
    
    if ((mFrameDataRead == mFrameDataSize) && !mFrameDataLast)
      ChangeDownstreamState(BUFFERING_FRAME_HEADER);

    return rv;
  }
  
  if (mDownstreamState == PROCESSING_CONTROL_SYN_REPLY) {
    
    if (mFlatHTTPResponseHeaders.Length() == mFlatHTTPResponseHeadersOut &&
        mFrameDataLast) {
      *countWritten = 0;
      ChangeDownstreamState(BUFFERING_FRAME_HEADER);
      return NS_BASE_STREAM_CLOSED;
    }
      
    count = NS_MIN(count,
                   mFlatHTTPResponseHeaders.Length() -
                   mFlatHTTPResponseHeadersOut);
    memcpy(buf,
           mFlatHTTPResponseHeaders.get() + mFlatHTTPResponseHeadersOut,
           count);
    mFlatHTTPResponseHeadersOut += count;
    *countWritten = count;

    if (mFlatHTTPResponseHeaders.Length() == mFlatHTTPResponseHeadersOut &&
        !mFrameDataLast)
      ChangeDownstreamState(BUFFERING_FRAME_HEADER);
    return NS_OK;
  }

  return NS_ERROR_UNEXPECTED;
}

//-----------------------------------------------------------------------------
// Modified methods of nsAHttpConnection
//-----------------------------------------------------------------------------

nsresult
SpdySession::ResumeSend(nsAHttpTransaction *caller)
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");
  LOG3(("SpdySession::ResumeSend %p caller=%p", this, caller));

  // a trapped signal from the http transaction to the connection that
  // it is no longer blocked on read.

  if (!mConnection)
    return NS_ERROR_FAILURE;

  SpdyStream *stream = mStreamTransactionHash.Get(caller);
  if (stream)
    mReadyForWrite.Push(stream);
  else
    LOG3(("SpdySession::ResumeSend %p caller %p not found", this, caller));
  
  return mConnection->ResumeSend(caller);
}

nsresult
SpdySession::ResumeRecv(nsAHttpTransaction *caller)
{
  if (!mConnection)
    return NS_ERROR_FAILURE;

  return mConnection->ResumeRecv(caller);
}

bool
SpdySession::IsPersistent()
{
  return PR_TRUE;
}

nsresult
SpdySession::TakeTransport(nsISocketTransport **,
                           nsIAsyncInputStream **,
                           nsIAsyncOutputStream **)
{
  NS_ABORT_IF_FALSE(false, "TakeTransport of SpdySession");
  return NS_ERROR_UNEXPECTED;
}

nsHttpConnection *
SpdySession::TakeHttpConnection()
{
  NS_ABORT_IF_FALSE(false, "TakeHttpConnection of SpdySession");
  return nsnull;
}

nsISocketTransport *
SpdySession::Transport()
{
    if (!mConnection)
        return nsnull;
    return mConnection->Transport();
}

//-----------------------------------------------------------------------------
// unused methods of nsAHttpTransaction
// We can be sure of this because SpdySession is only constructed in
// nsHttpConnection and is never passed out of that object
//-----------------------------------------------------------------------------

void
SpdySession::SetConnection(nsAHttpConnection *)
{
  // This is unexpected
  NS_ABORT_IF_FALSE(false, "SpdySession::SetConnection()");
}

void
SpdySession::GetSecurityCallbacks(nsIInterfaceRequestor **,
                                  nsIEventTarget **)
{
  // This is unexpected
  NS_ABORT_IF_FALSE(false, "SpdySession::GetSecurityCallbacks()");
}

void
SpdySession::SetSSLConnectFailed()
{
  NS_ABORT_IF_FALSE(false, "SpdySession::SetSSLConnectFailed()");
}

bool
SpdySession::IsDone()
{
  NS_ABORT_IF_FALSE(false, "SpdySession::IsDone()");
  return PR_FALSE;
}

nsresult
SpdySession::Status()
{
  NS_ABORT_IF_FALSE(false, "SpdySession::Status()");
  return NS_ERROR_UNEXPECTED;
}

PRUint32
SpdySession::Available()
{
  NS_ABORT_IF_FALSE(false, "SpdySession::Available()");
  return 0;
}

nsHttpRequestHead *
SpdySession::RequestHead()
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");
  NS_ABORT_IF_FALSE(false,
                    "SpdySession::RequestHead() "
                    "should not be called after SPDY is setup");
  return NULL;
}

//-----------------------------------------------------------------------------
// Pass through methods of nsAHttpConnection
//-----------------------------------------------------------------------------

nsAHttpConnection *
SpdySession::Connection()
{
    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");
    return mConnection;
}

nsresult
SpdySession::OnHeadersAvailable(nsAHttpTransaction *transaction,
                                nsHttpRequestHead *requestHead,
                                nsHttpResponseHead *responseHead,
                                bool *reset)
{
  return mConnection->OnHeadersAvailable(transaction,
                                         requestHead,
                                         responseHead,
                                         reset);
}

void
SpdySession::GetConnectionInfo(nsHttpConnectionInfo **connInfo)
{
  mConnection->GetConnectionInfo(connInfo);
}

void
SpdySession::GetSecurityInfo(nsISupports **supports)
{
  mConnection->GetSecurityInfo(supports);
}

bool
SpdySession::IsReused()
{
  return mConnection->IsReused();
}

nsresult
SpdySession::PushBack(const char *buf, PRUint32 len)
{
  return mConnection->PushBack(buf, len);
}

bool
SpdySession::LastTransactionExpectedNoContent()
{
  return mConnection->LastTransactionExpectedNoContent();
}

void
SpdySession::SetLastTransactionExpectedNoContent(bool val)
{
  mConnection->SetLastTransactionExpectedNoContent(val);
}

} // namespace mozilla::net
} // namespace mozilla

