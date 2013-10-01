/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "mozilla/Telemetry.h"
#include "nsAlgorithm.h"
#include "nsHttp.h"
#include "nsHttpHandler.h"
#include "nsHttpRequestHead.h"
#include "nsISocketTransport.h"
#include "nsISupportsPriority.h"
#include "prnetdb.h"
#include "SpdyPush3.h"
#include "SpdySession3.h"
#include "SpdyStream3.h"

#include <algorithm>

#ifdef DEBUG
// defined by the socket transport service while active
extern PRThread *gSocketThread;
#endif

namespace mozilla {
namespace net {

SpdyStream3::SpdyStream3(nsAHttpTransaction *httpTransaction,
                       SpdySession3 *spdySession,
                       int32_t priority)
  : mStreamID(0),
    mSession(spdySession),
    mUpstreamState(GENERATING_SYN_STREAM),
    mSynFrameComplete(0),
    mSentFinOnData(0),
    mTransaction(httpTransaction),
    mSocketTransport(spdySession->SocketTransport()),
    mSegmentReader(nullptr),
    mSegmentWriter(nullptr),
    mChunkSize(spdySession->SendingChunkSize()),
    mRequestBlockedOnRead(0),
    mRecvdFin(0),
    mFullyOpen(0),
    mSentWaitingFor(0),
    mReceivedData(0),
    mSetTCPSocketBuffer(0),
    mTxInlineFrameSize(SpdySession3::kDefaultBufferSize),
    mTxInlineFrameUsed(0),
    mTxStreamFrameSize(0),
    mZlib(spdySession->UpstreamZlib()),
    mDecompressBufferSize(SpdySession3::kDefaultBufferSize),
    mDecompressBufferUsed(0),
    mDecompressedBytes(0),
    mRequestBodyLenRemaining(0),
    mPriority(priority),
    mLocalUnacked(0),
    mBlockedOnRwin(false),
    mTotalSent(0),
    mTotalRead(0),
    mPushSource(nullptr)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  LOG3(("SpdyStream3::SpdyStream3 %p", this));

  mRemoteWindow = spdySession->GetServerInitialWindow();
  mLocalWindow = spdySession->PushAllowance();

  mTxInlineFrame = new uint8_t[mTxInlineFrameSize];
  mDecompressBuffer = new char[mDecompressBufferSize];
}

SpdyStream3::~SpdyStream3()
{
  mStreamID = SpdySession3::kDeadStreamID;
}

// ReadSegments() is used to write data down the socket. Generally, HTTP
// request data is pulled from the approriate transaction and
// converted to SPDY data. Sometimes control data like a window-update is
// generated instead.

nsresult
SpdyStream3::ReadSegments(nsAHttpSegmentReader *reader,
                         uint32_t count,
                         uint32_t *countRead)
{
  LOG3(("SpdyStream3 %p ReadSegments reader=%p count=%d state=%x",
        this, reader, count, mUpstreamState));

  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  nsresult rv = NS_ERROR_UNEXPECTED;
  mRequestBlockedOnRead = 0;

  switch (mUpstreamState) {
  case GENERATING_SYN_STREAM:
  case GENERATING_REQUEST_BODY:
  case SENDING_REQUEST_BODY:
    // Call into the HTTP Transaction to generate the HTTP request
    // stream. That stream will show up in OnReadSegment().
    mSegmentReader = reader;
    rv = mTransaction->ReadSegments(this, count, countRead);
    mSegmentReader = nullptr;

    // Check to see if the transaction's request could be written out now.
    // If not, mark the stream for callback when writing can proceed.
    if (NS_SUCCEEDED(rv) &&
        mUpstreamState == GENERATING_SYN_STREAM &&
        !mSynFrameComplete)
      mSession->TransactionHasDataToWrite(this);

    // mTxinlineFrameUsed represents any queued un-sent frame. It might
    // be 0 if there is no such frame, which is not a gurantee that we
    // don't have more request body to send - just that any data that was
    // sent comprised a complete SPDY frame. Likewise, a non 0 value is
    // a queued, but complete, spdy frame length.

    // Mark that we are blocked on read if the http transaction needs to
    // provide more of the request message body and there is nothing queued
    // for writing
    if (rv == NS_BASE_STREAM_WOULD_BLOCK && !mTxInlineFrameUsed)
      mRequestBlockedOnRead = 1;

    // If the sending flow control window is open (!mBlockedOnRwin) then
    // continue sending the request
    if (!mBlockedOnRwin &&
        !mTxInlineFrameUsed && NS_SUCCEEDED(rv) && (!*countRead)) {
      LOG3(("SpdyStream3::ReadSegments %p 0x%X: Sending request data complete, "
            "mUpstreamState=%x",this, mStreamID, mUpstreamState));
      if (mSentFinOnData) {
        ChangeState(UPSTREAM_COMPLETE);
      }
      else {
        GenerateDataFrameHeader(0, true);
        ChangeState(SENDING_FIN_STREAM);
        mSession->TransactionHasDataToWrite(this);
        rv = NS_BASE_STREAM_WOULD_BLOCK;
      }
    }
    break;

  case SENDING_FIN_STREAM:
    // We were trying to send the FIN-STREAM but were blocked from
    // sending it out - try again.
    if (!mSentFinOnData) {
      mSegmentReader = reader;
      rv = TransmitFrame(nullptr, nullptr, false);
      mSegmentReader = nullptr;
      MOZ_ASSERT(NS_FAILED(rv) || !mTxInlineFrameUsed,
                 "Transmit Frame should be all or nothing");
      if (NS_SUCCEEDED(rv))
        ChangeState(UPSTREAM_COMPLETE);
    }
    else {
      rv = NS_OK;
      mTxInlineFrameUsed = 0;         // cancel fin data packet
      ChangeState(UPSTREAM_COMPLETE);
    }

    *countRead = 0;

    // don't change OK to WOULD BLOCK. we are really done sending if OK
    break;

  case UPSTREAM_COMPLETE:
    *countRead = 0;
    rv = NS_OK;
    break;

  default:
    MOZ_ASSERT(false, "SpdyStream3::ReadSegments unknown state");
    break;
  }

  return rv;
}

// WriteSegments() is used to read data off the socket. Generally this is
// just the SPDY frame header and from there the appropriate SPDYStream
// is identified from the Stream-ID. The http transaction associated with
// that read then pulls in the data directly.

nsresult
SpdyStream3::WriteSegments(nsAHttpSegmentWriter *writer,
                          uint32_t count,
                          uint32_t *countWritten)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  MOZ_ASSERT(!mSegmentWriter, "segment writer in progress");

  LOG3(("SpdyStream3::WriteSegments %p count=%d state=%x",
        this, count, mUpstreamState));

  mSegmentWriter = writer;
  nsresult rv = mTransaction->WriteSegments(this, count, countWritten);
  mSegmentWriter = nullptr;

  return rv;
}

PLDHashOperator
SpdyStream3::hdrHashEnumerate(const nsACString &key,
                             nsAutoPtr<nsCString> &value,
                             void *closure)
{
  SpdyStream3 *self = static_cast<SpdyStream3 *>(closure);

  self->CompressToFrame(key);
  self->CompressToFrame(value.get());
  return PL_DHASH_NEXT;
}

void
SpdyStream3::CreatePushHashKey(const nsCString &scheme,
                               const nsCString &hostHeader,
                               uint64_t serial,
                               const nsCSubstring &pathInfo,
                               nsCString &outOrigin,
                               nsCString &outKey)
{
  outOrigin = scheme;
  outOrigin.Append(NS_LITERAL_CSTRING("://"));
  outOrigin.Append(hostHeader);

  outKey = outOrigin;
  outKey.Append(NS_LITERAL_CSTRING("/[spdy3."));
  outKey.AppendInt(serial);
  outKey.Append(NS_LITERAL_CSTRING("]"));
  outKey.Append(pathInfo);
}


nsresult
SpdyStream3::ParseHttpRequestHeaders(const char *buf,
                                    uint32_t avail,
                                    uint32_t *countUsed)
{
  // Returns NS_OK even if the headers are incomplete
  // set mSynFrameComplete flag if they are complete

  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  MOZ_ASSERT(mUpstreamState == GENERATING_SYN_STREAM);

  LOG3(("SpdyStream3::ParseHttpRequestHeaders %p avail=%d state=%x",
        this, avail, mUpstreamState));

  mFlatHttpRequestHeaders.Append(buf, avail);

  // We can use the simple double crlf because firefox is the
  // only client we are parsing
  int32_t endHeader = mFlatHttpRequestHeaders.Find("\r\n\r\n");

  if (endHeader == kNotFound) {
    // We don't have all the headers yet
    LOG3(("SpdyStream3::ParseHttpRequestHeaders %p "
          "Need more header bytes. Len = %d",
          this, mFlatHttpRequestHeaders.Length()));
    *countUsed = avail;
    return NS_OK;
  }

  // We have recvd all the headers, trim the local
  // buffer of the final empty line, and set countUsed to reflect
  // the whole header has been consumed.
  uint32_t oldLen = mFlatHttpRequestHeaders.Length();
  mFlatHttpRequestHeaders.SetLength(endHeader + 2);
  *countUsed = avail - (oldLen - endHeader) + 4;
  mSynFrameComplete = 1;

  nsCString hostHeader;
  nsCString hashkey;
  mTransaction->RequestHead()->GetHeader(nsHttp::Host, hostHeader);

  CreatePushHashKey(NS_LITERAL_CSTRING("https"),
                    hostHeader, mSession->Serial(),
                    mTransaction->RequestHead()->RequestURI(),
                    mOrigin, hashkey);

  // check the push cache for GET
  if (mTransaction->RequestHead()->Method() == nsHttp::Get) {
    // from :scheme, :host, :path
    nsILoadGroupConnectionInfo *loadGroupCI = mTransaction->LoadGroupConnectionInfo();
    SpdyPushCache3 *cache = nullptr;
    if (loadGroupCI)
      loadGroupCI->GetSpdyPushCache3(&cache);

    SpdyPushedStream3 *pushedStream = nullptr;
    // we remove the pushedstream from the push cache so that
    // it will not be used for another GET. This does not destroy the
    // stream itself - that is done when the transactionhash is done with it.
    if (cache)
      pushedStream = cache->RemovePushedStream(hashkey);

    if (pushedStream) {
      LOG3(("Pushed Stream Match located id=0x%X key=%s\n",
            pushedStream->StreamID(), hashkey.get()));
      pushedStream->SetConsumerStream(this);
      mPushSource = pushedStream;
      mSentFinOnData = 1;

      // There is probably pushed data buffered so trigger a read manually
      // as we can't rely on future network events to do it
      mSession->ConnectPushedStream(this);
      return NS_OK;
    }
  }

  // It is now OK to assign a streamID that we are assured will
  // be monotonically increasing amongst syn-streams on this
  // session
  mStreamID = mSession->RegisterStreamID(this);
  MOZ_ASSERT(mStreamID & 1, "Spdy Stream Channel ID must be odd");

  if (mStreamID >= 0x80000000) {
    // streamID must fit in 31 bits. This is theoretically possible
    // because stream ID assignment is asynchronous to stream creation
    // because of the protocol requirement that the ID in syn-stream
    // be monotonically increasing. In reality this is really not possible
    // because new streams stop being added to a session with 0x10000000 / 2
    // IDs still available and no race condition is going to bridge that gap,
    // so we can be comfortable on just erroring out for correctness in that
    // case.
    LOG3(("Stream assigned out of range ID: 0x%X", mStreamID));
    return NS_ERROR_UNEXPECTED;
  }

  // Now we need to convert the flat http headers into a set
  // of SPDY headers..  writing to mTxInlineFrame{sz}

  mTxInlineFrame[0] = SpdySession3::kFlag_Control;
  mTxInlineFrame[1] = SpdySession3::kVersion;
  mTxInlineFrame[2] = 0;
  mTxInlineFrame[3] = SpdySession3::CONTROL_TYPE_SYN_STREAM;
  // 4 to 7 are length and flags, we'll fill that in later

  uint32_t networkOrderID = PR_htonl(mStreamID);
  memcpy(mTxInlineFrame + 8, &networkOrderID, 4);

  // this is the associated-to field, which is not used sending
  // from the client in the http binding
  memset (mTxInlineFrame + 12, 0, 4);

  // Priority flags are the E0 mask of byte 16.
  // 0 is highest priority, 7 is lowest.
  // The other 5 bits of byte 16 are unused.

  if (mPriority >= nsISupportsPriority::PRIORITY_LOWEST)
    mTxInlineFrame[16] = 7 << 5;
  else if (mPriority <= nsISupportsPriority::PRIORITY_HIGHEST)
    mTxInlineFrame[16] = 0 << 5;
  else {
    // The priority mapping relies on the unfiltered ranged to be
    // between -20 .. +20
    PR_STATIC_ASSERT(nsISupportsPriority::PRIORITY_LOWEST == 20);
    PR_STATIC_ASSERT(nsISupportsPriority::PRIORITY_HIGHEST == -20);

    // Add one to the priority so that values such as -10 and -11
    // get different spdy priorities - this appears to be an important
    // breaking line in the priorities content assigns to
    // transactions.
    uint8_t calculatedPriority = 3 + ((mPriority + 1) / 5);
    MOZ_ASSERT (!(calculatedPriority & 0xf8),
                "Calculated Priority Out Of Range");
    mTxInlineFrame[16] = calculatedPriority << 5;
  }

  // The client cert "slot". Right now we don't send client certs
  mTxInlineFrame[17] = 0;

  nsCString versionHeader;
  if (mTransaction->RequestHead()->Version() == NS_HTTP_VERSION_1_1)
    versionHeader = NS_LITERAL_CSTRING("HTTP/1.1");
  else
    versionHeader = NS_LITERAL_CSTRING("HTTP/1.0");

  // use mRequestHead() to get a sense of how big to make the hash,
  // even though we are parsing the actual text stream because
  // it is legit to append headers.
  nsClassHashtable<nsCStringHashKey, nsCString>
    hdrHash(1 + (mTransaction->RequestHead()->Headers().Count() * 2));

  const char *beginBuffer = mFlatHttpRequestHeaders.BeginReading();

  // need to hash all the headers together to remove duplicates, special
  // headers, etc..

  int32_t crlfIndex = mFlatHttpRequestHeaders.Find("\r\n");
  while (true) {
    int32_t startIndex = crlfIndex + 2;

    crlfIndex = mFlatHttpRequestHeaders.Find("\r\n", false, startIndex);
    if (crlfIndex == -1)
      break;

    int32_t colonIndex = mFlatHttpRequestHeaders.Find(":", false, startIndex,
                                                      crlfIndex - startIndex);
    if (colonIndex == -1)
      break;

    nsDependentCSubstring name = Substring(beginBuffer + startIndex,
                                           beginBuffer + colonIndex);
    // all header names are lower case in spdy
    ToLowerCase(name);

    // exclusions.. mostly from 3.2.1
    if (name.Equals("connection") ||
        name.Equals("keep-alive") ||
        name.Equals("host") ||
        name.Equals("accept-encoding") ||
        name.Equals("te") ||
        name.Equals("transfer-encoding"))
      continue;

    nsCString *val = hdrHash.Get(name);
    if (!val) {
      val = new nsCString();
      hdrHash.Put(name, val);
    }

    int32_t valueIndex = colonIndex + 1;
    while (valueIndex < crlfIndex && beginBuffer[valueIndex] == ' ')
      ++valueIndex;

    nsDependentCSubstring v = Substring(beginBuffer + valueIndex,
                                        beginBuffer + crlfIndex);
    if (!val->IsEmpty())
      val->Append(static_cast<char>(0));
    val->Append(v);

    if (name.Equals("content-length")) {
      int64_t len;
      if (nsHttp::ParseInt64(val->get(), nullptr, &len))
        mRequestBodyLenRemaining = len;
    }
  }

  mTxInlineFrameUsed = 18;

  // Do not naively log the request headers here beacuse they might
  // contain auth. The http transaction already logs the sanitized request
  // headers at this same level so it is not necessary to do so here.

  const char *methodHeader = mTransaction->RequestHead()->Method().get();

  // The header block length
  uint16_t count = hdrHash.Count() + 5; /* method, path, version, host, scheme */
  CompressToFrame(count);

  // :method, :path, :version comprise a HTTP/1 request line, so send those first
  // to make life easy for any gateways
  CompressToFrame(NS_LITERAL_CSTRING(":method"));
  CompressToFrame(methodHeader, strlen(methodHeader));
  CompressToFrame(NS_LITERAL_CSTRING(":path"));
  CompressToFrame(mTransaction->RequestHead()->RequestURI());
  CompressToFrame(NS_LITERAL_CSTRING(":version"));
  CompressToFrame(versionHeader);

  CompressToFrame(NS_LITERAL_CSTRING(":host"));
  CompressToFrame(hostHeader);
  CompressToFrame(NS_LITERAL_CSTRING(":scheme"));
  CompressToFrame(NS_LITERAL_CSTRING("https"));

  hdrHash.Enumerate(hdrHashEnumerate, this);
  CompressFlushFrame();

  // 4 to 7 are length and flags, which we can now fill in
  (reinterpret_cast<uint32_t *>(mTxInlineFrame.get()))[1] =
    PR_htonl(mTxInlineFrameUsed - 8);

  MOZ_ASSERT(!mTxInlineFrame[4], "Size greater than 24 bits");

  // Determine whether to put the fin bit on the syn stream frame or whether
  // to wait for a data packet to put it on.

  if (mTransaction->RequestHead()->Method() == nsHttp::Get ||
      mTransaction->RequestHead()->Method() == nsHttp::Connect ||
      mTransaction->RequestHead()->Method() == nsHttp::Head) {
    // for GET, CONNECT, and HEAD place the fin bit right on the
    // syn stream packet

    mSentFinOnData = 1;
    mTxInlineFrame[4] = SpdySession3::kFlag_Data_FIN;
  }
  else if (mTransaction->RequestHead()->Method() == nsHttp::Post ||
           mTransaction->RequestHead()->Method() == nsHttp::Put ||
           mTransaction->RequestHead()->Method() == nsHttp::Options) {
    // place fin in a data frame even for 0 length messages, I've seen
    // the google gateway be unhappy with fin-on-syn for 0 length POST
  }
  else if (!mRequestBodyLenRemaining) {
    // for other HTTP extension methods, rely on the content-length
    // to determine whether or not to put fin on syn
    mSentFinOnData = 1;
    mTxInlineFrame[4] = SpdySession3::kFlag_Data_FIN;
  }

  Telemetry::Accumulate(Telemetry::SPDY_SYN_SIZE, mTxInlineFrameUsed - 18);

  // The size of the input headers is approximate
  uint32_t ratio =
    (mTxInlineFrameUsed - 18) * 100 /
    (11 + mTransaction->RequestHead()->RequestURI().Length() +
     mFlatHttpRequestHeaders.Length());

  Telemetry::Accumulate(Telemetry::SPDY_SYN_RATIO, ratio);
  return NS_OK;
}

void
SpdyStream3::AdjustInitialWindow()
{
  MOZ_ASSERT(mSession->PushAllowance() <= ASpdySession::kInitialRwin);

  // The session initial_window is sized for serverpushed streams. When we
  // generate a client pulled stream we want to adjust the initial window
  // to a huge value in a pipeline with that SYN_STREAM.

  // >0 even numbered IDs are pushed streams.
  // odd numbered IDs are pulled streams.
  // 0 is the sink for a pushed stream.
  SpdyStream3 *stream = this;
  if (!mStreamID) {
    MOZ_ASSERT(mPushSource);
    if (!mPushSource)
      return;
    stream = mPushSource;
    MOZ_ASSERT(stream->mStreamID);
    MOZ_ASSERT(!(stream->mStreamID & 1)); // is a push stream

    // If the pushed stream has sent a FIN, there is no reason to update
    // the window
    if (stream->RecvdFin())
      return;
  }

  // For server pushes we also want to include in the ack any data that has been
  // buffered but unacknowledged.

  // mLocalUnacked is technically 64 bits, but because it can never grow larger than
  // our window size (which is closer to 29bits max) we know it fits comfortably in 32.
  // However we don't really enforce that, and track it as a 64 so that broken senders
  // can still interoperate. That means we have to be careful with this calculation.
  uint64_t toack64 = (ASpdySession::kInitialRwin - mSession->PushAllowance()) +
    stream->mLocalUnacked;
  stream->mLocalUnacked = 0;
  if (toack64 > 0x7fffffff) {
    stream->mLocalUnacked = toack64 - 0x7fffffff;
    toack64 = 0x7fffffff;
  }
  uint32_t toack = static_cast<uint32_t>(toack64);
  if (!toack)
    return;
  toack = PR_htonl(toack);

  SpdySession3::EnsureBuffer(mTxInlineFrame,
                             mTxInlineFrameUsed + 16,
                             mTxInlineFrameUsed,
                             mTxInlineFrameSize);

  unsigned char *packet = mTxInlineFrame.get() + mTxInlineFrameUsed;
  mTxInlineFrameUsed += 16;

  memset(packet, 0, 8);
  packet[0] = SpdySession3::kFlag_Control;
  packet[1] = SpdySession3::kVersion;
  packet[3] = SpdySession3::CONTROL_TYPE_WINDOW_UPDATE;
  packet[7] = 8; // 8 data bytes after 8 byte header

  uint32_t id = PR_htonl(stream->mStreamID);
  memcpy(packet + 8, &id, 4);
  memcpy(packet + 12, &toack, 4);

  stream->mLocalWindow += PR_ntohl(toack);
  LOG3(("AdjustInitialwindow %p 0x%X %u\n",
        this, stream->mStreamID, PR_ntohl(toack)));
}

void
SpdyStream3::UpdateTransportReadEvents(uint32_t count)
{
  mTotalRead += count;

  mTransaction->OnTransportStatus(mSocketTransport,
                                  NS_NET_STATUS_RECEIVING_FROM,
                                  mTotalRead);
}

void
SpdyStream3::UpdateTransportSendEvents(uint32_t count)
{
  mTotalSent += count;

  // normally on non-windows platform we use TCP autotuning for
  // the socket buffers, and this works well (managing enough
  // buffers for BDP while conserving memory) for HTTP even when
  // it creates really deep queues. However this 'buffer bloat' is
  // a problem for spdy because it ruins the low latency properties
  // necessary for PING and cancel to work meaningfully.
  //
  // If this stream represents a large upload, disable autotuning for
  // the session and cap the send buffers by default at 128KB.
  // (10Mbit/sec @ 100ms)
  //
  uint32_t bufferSize = gHttpHandler->SpdySendBufferSize();
  if ((mTotalSent > bufferSize) && !mSetTCPSocketBuffer) {
    mSetTCPSocketBuffer = 1;
    mSocketTransport->SetSendBufferSize(bufferSize);
  }

  if (mUpstreamState != SENDING_FIN_STREAM)
    mTransaction->OnTransportStatus(mSocketTransport,
                                    NS_NET_STATUS_SENDING_TO,
                                    mTotalSent);

  if (!mSentWaitingFor && !mRequestBodyLenRemaining) {
    mSentWaitingFor = 1;
    mTransaction->OnTransportStatus(mSocketTransport,
                                    NS_NET_STATUS_WAITING_FOR,
                                    0);
  }
}

nsresult
SpdyStream3::TransmitFrame(const char *buf,
                           uint32_t *countUsed,
                           bool forceCommitment)
{
  // If TransmitFrame returns SUCCESS than all the data is sent (or at least
  // buffered at the session level), if it returns WOULD_BLOCK then none of
  // the data is sent.

  // You can call this function with no data and no out parameter in order to
  // flush internal buffers that were previously blocked on writing. You can
  // of course feed new data to it as well.

  LOG3(("SpdyStream3::TransmitFrame %p inline=%d stream=%d",
        this, mTxInlineFrameUsed, mTxStreamFrameSize));
  if (countUsed)
    *countUsed = 0;

  if (!mTxInlineFrameUsed) {
    MOZ_ASSERT(!buf);
    return NS_OK;
  }

  MOZ_ASSERT(mTxInlineFrameUsed, "empty stream frame in transmit");
  MOZ_ASSERT(mSegmentReader, "TransmitFrame with null mSegmentReader");
  MOZ_ASSERT((buf && countUsed) || (!buf && !countUsed),
             "TransmitFrame arguments inconsistent");

  uint32_t transmittedCount;
  nsresult rv;

  // In the (relatively common) event that we have a small amount of data
  // split between the inlineframe and the streamframe, then move the stream
  // data into the inlineframe via copy in order to coalesce into one write.
  // Given the interaction with ssl this is worth the small copy cost.
  if (mTxStreamFrameSize && mTxInlineFrameUsed &&
      mTxStreamFrameSize < SpdySession3::kDefaultBufferSize &&
      mTxInlineFrameUsed + mTxStreamFrameSize < mTxInlineFrameSize) {
    LOG3(("Coalesce Transmit"));
    memcpy (mTxInlineFrame + mTxInlineFrameUsed,
            buf, mTxStreamFrameSize);
    if (countUsed)
      *countUsed += mTxStreamFrameSize;
    mTxInlineFrameUsed += mTxStreamFrameSize;
    mTxStreamFrameSize = 0;
  }

  rv =
    mSegmentReader->CommitToSegmentSize(mTxStreamFrameSize + mTxInlineFrameUsed,
                                        forceCommitment);

  if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
    MOZ_ASSERT(!forceCommitment, "forceCommitment with WOULD_BLOCK");
    mSession->TransactionHasDataToWrite(this);
  }
  if (NS_FAILED(rv))     // this will include WOULD_BLOCK
    return rv;

  // This function calls mSegmentReader->OnReadSegment to report the actual SPDY
  // bytes through to the SpdySession3 and then the HttpConnection which calls
  // the socket write function. It will accept all of the inline and stream
  // data because of the above 'commitment' even if it has to buffer

  rv = mSegmentReader->OnReadSegment(reinterpret_cast<char*>(mTxInlineFrame.get()),
                                     mTxInlineFrameUsed,
                                     &transmittedCount);
  LOG3(("SpdyStream3::TransmitFrame for inline session=%p "
        "stream=%p result %x len=%d",
        mSession, this, rv, transmittedCount));

  MOZ_ASSERT(rv != NS_BASE_STREAM_WOULD_BLOCK,
             "inconsistent inline commitment result");

  if (NS_FAILED(rv))
    return rv;

  MOZ_ASSERT(transmittedCount == mTxInlineFrameUsed,
             "inconsistent inline commitment count");

  SpdySession3::LogIO(mSession, this, "Writing from Inline Buffer",
                      reinterpret_cast<char*>(mTxInlineFrame.get()),
                      transmittedCount);

  if (mTxStreamFrameSize) {
    if (!buf) {
      // this cannot happen
      MOZ_ASSERT(false, "Stream transmit with null buf argument to "
                 "TransmitFrame()");
      LOG(("Stream transmit with null buf argument to TransmitFrame()\n"));
      return NS_ERROR_UNEXPECTED;
    }

    rv = mSegmentReader->OnReadSegment(buf, mTxStreamFrameSize,
                                       &transmittedCount);

    LOG3(("SpdyStream3::TransmitFrame for regular session=%p "
          "stream=%p result %x len=%d",
          mSession, this, rv, transmittedCount));

    MOZ_ASSERT(rv != NS_BASE_STREAM_WOULD_BLOCK,
               "inconsistent stream commitment result");

    if (NS_FAILED(rv))
      return rv;

    MOZ_ASSERT(transmittedCount == mTxStreamFrameSize,
               "inconsistent stream commitment count");

    SpdySession3::LogIO(mSession, this, "Writing from Transaction Buffer",
                       buf, transmittedCount);

    *countUsed += mTxStreamFrameSize;
  }

  // calling this will trigger waiting_for if mRequestBodyLenRemaining is 0
  UpdateTransportSendEvents(mTxInlineFrameUsed + mTxStreamFrameSize);

  mTxInlineFrameUsed = 0;
  mTxStreamFrameSize = 0;

  return NS_OK;
}

void
SpdyStream3::ChangeState(enum stateType newState)
{
  LOG3(("SpdyStream3::ChangeState() %p from %X to %X",
        this, mUpstreamState, newState));
  mUpstreamState = newState;
  return;
}

void
SpdyStream3::GenerateDataFrameHeader(uint32_t dataLength, bool lastFrame)
{
  LOG3(("SpdyStream3::GenerateDataFrameHeader %p len=%d last=%d",
        this, dataLength, lastFrame));

  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  MOZ_ASSERT(!mTxInlineFrameUsed, "inline frame not empty");
  MOZ_ASSERT(!mTxStreamFrameSize, "stream frame not empty");
  MOZ_ASSERT(!(dataLength & 0xff000000), "datalength > 24 bits");

  (reinterpret_cast<uint32_t *>(mTxInlineFrame.get()))[0] = PR_htonl(mStreamID);
  (reinterpret_cast<uint32_t *>(mTxInlineFrame.get()))[1] =
    PR_htonl(dataLength);

  MOZ_ASSERT(!(mTxInlineFrame[0] & 0x80), "control bit set unexpectedly");
  MOZ_ASSERT(!mTxInlineFrame[4], "flag bits set unexpectedly");

  mTxInlineFrameUsed = 8;
  mTxStreamFrameSize = dataLength;

  if (lastFrame) {
    mTxInlineFrame[4] |= SpdySession3::kFlag_Data_FIN;
    if (dataLength)
      mSentFinOnData = 1;
  }
}

void
SpdyStream3::CompressToFrame(const nsACString &str)
{
  CompressToFrame(str.BeginReading(), str.Length());
}

void
SpdyStream3::CompressToFrame(const nsACString *str)
{
  CompressToFrame(str->BeginReading(), str->Length());
}

// Dictionary taken from
// http://dev.chromium.org/spdy/spdy-protocol/spdy-protocol-draft3

const unsigned char SpdyStream3::kDictionary[] = {
	0x00, 0x00, 0x00, 0x07, 0x6f, 0x70, 0x74, 0x69,   // - - - - o p t i
	0x6f, 0x6e, 0x73, 0x00, 0x00, 0x00, 0x04, 0x68,   // o n s - - - - h
	0x65, 0x61, 0x64, 0x00, 0x00, 0x00, 0x04, 0x70,   // e a d - - - - p
	0x6f, 0x73, 0x74, 0x00, 0x00, 0x00, 0x03, 0x70,   // o s t - - - - p
	0x75, 0x74, 0x00, 0x00, 0x00, 0x06, 0x64, 0x65,   // u t - - - - d e
	0x6c, 0x65, 0x74, 0x65, 0x00, 0x00, 0x00, 0x05,   // l e t e - - - -
	0x74, 0x72, 0x61, 0x63, 0x65, 0x00, 0x00, 0x00,   // t r a c e - - -
	0x06, 0x61, 0x63, 0x63, 0x65, 0x70, 0x74, 0x00,   // - a c c e p t -
	0x00, 0x00, 0x0e, 0x61, 0x63, 0x63, 0x65, 0x70,   // - - - a c c e p
	0x74, 0x2d, 0x63, 0x68, 0x61, 0x72, 0x73, 0x65,   // t - c h a r s e
	0x74, 0x00, 0x00, 0x00, 0x0f, 0x61, 0x63, 0x63,   // t - - - - a c c
	0x65, 0x70, 0x74, 0x2d, 0x65, 0x6e, 0x63, 0x6f,   // e p t - e n c o
	0x64, 0x69, 0x6e, 0x67, 0x00, 0x00, 0x00, 0x0f,   // d i n g - - - -
	0x61, 0x63, 0x63, 0x65, 0x70, 0x74, 0x2d, 0x6c,   // a c c e p t - l
	0x61, 0x6e, 0x67, 0x75, 0x61, 0x67, 0x65, 0x00,   // a n g u a g e -
	0x00, 0x00, 0x0d, 0x61, 0x63, 0x63, 0x65, 0x70,   // - - - a c c e p
	0x74, 0x2d, 0x72, 0x61, 0x6e, 0x67, 0x65, 0x73,   // t - r a n g e s
	0x00, 0x00, 0x00, 0x03, 0x61, 0x67, 0x65, 0x00,   // - - - - a g e -
	0x00, 0x00, 0x05, 0x61, 0x6c, 0x6c, 0x6f, 0x77,   // - - - a l l o w
	0x00, 0x00, 0x00, 0x0d, 0x61, 0x75, 0x74, 0x68,   // - - - - a u t h
	0x6f, 0x72, 0x69, 0x7a, 0x61, 0x74, 0x69, 0x6f,   // o r i z a t i o
	0x6e, 0x00, 0x00, 0x00, 0x0d, 0x63, 0x61, 0x63,   // n - - - - c a c
	0x68, 0x65, 0x2d, 0x63, 0x6f, 0x6e, 0x74, 0x72,   // h e - c o n t r
	0x6f, 0x6c, 0x00, 0x00, 0x00, 0x0a, 0x63, 0x6f,   // o l - - - - c o
	0x6e, 0x6e, 0x65, 0x63, 0x74, 0x69, 0x6f, 0x6e,   // n n e c t i o n
	0x00, 0x00, 0x00, 0x0c, 0x63, 0x6f, 0x6e, 0x74,   // - - - - c o n t
	0x65, 0x6e, 0x74, 0x2d, 0x62, 0x61, 0x73, 0x65,   // e n t - b a s e
	0x00, 0x00, 0x00, 0x10, 0x63, 0x6f, 0x6e, 0x74,   // - - - - c o n t
	0x65, 0x6e, 0x74, 0x2d, 0x65, 0x6e, 0x63, 0x6f,   // e n t - e n c o
	0x64, 0x69, 0x6e, 0x67, 0x00, 0x00, 0x00, 0x10,   // d i n g - - - -
	0x63, 0x6f, 0x6e, 0x74, 0x65, 0x6e, 0x74, 0x2d,   // c o n t e n t -
	0x6c, 0x61, 0x6e, 0x67, 0x75, 0x61, 0x67, 0x65,   // l a n g u a g e
	0x00, 0x00, 0x00, 0x0e, 0x63, 0x6f, 0x6e, 0x74,   // - - - - c o n t
	0x65, 0x6e, 0x74, 0x2d, 0x6c, 0x65, 0x6e, 0x67,   // e n t - l e n g
	0x74, 0x68, 0x00, 0x00, 0x00, 0x10, 0x63, 0x6f,   // t h - - - - c o
	0x6e, 0x74, 0x65, 0x6e, 0x74, 0x2d, 0x6c, 0x6f,   // n t e n t - l o
	0x63, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x00, 0x00,   // c a t i o n - -
	0x00, 0x0b, 0x63, 0x6f, 0x6e, 0x74, 0x65, 0x6e,   // - - c o n t e n
	0x74, 0x2d, 0x6d, 0x64, 0x35, 0x00, 0x00, 0x00,   // t - m d 5 - - -
	0x0d, 0x63, 0x6f, 0x6e, 0x74, 0x65, 0x6e, 0x74,   // - c o n t e n t
	0x2d, 0x72, 0x61, 0x6e, 0x67, 0x65, 0x00, 0x00,   // - r a n g e - -
	0x00, 0x0c, 0x63, 0x6f, 0x6e, 0x74, 0x65, 0x6e,   // - - c o n t e n
	0x74, 0x2d, 0x74, 0x79, 0x70, 0x65, 0x00, 0x00,   // t - t y p e - -
	0x00, 0x04, 0x64, 0x61, 0x74, 0x65, 0x00, 0x00,   // - - d a t e - -
	0x00, 0x04, 0x65, 0x74, 0x61, 0x67, 0x00, 0x00,   // - - e t a g - -
	0x00, 0x06, 0x65, 0x78, 0x70, 0x65, 0x63, 0x74,   // - - e x p e c t
	0x00, 0x00, 0x00, 0x07, 0x65, 0x78, 0x70, 0x69,   // - - - - e x p i
	0x72, 0x65, 0x73, 0x00, 0x00, 0x00, 0x04, 0x66,   // r e s - - - - f
	0x72, 0x6f, 0x6d, 0x00, 0x00, 0x00, 0x04, 0x68,   // r o m - - - - h
	0x6f, 0x73, 0x74, 0x00, 0x00, 0x00, 0x08, 0x69,   // o s t - - - - i
	0x66, 0x2d, 0x6d, 0x61, 0x74, 0x63, 0x68, 0x00,   // f - m a t c h -
	0x00, 0x00, 0x11, 0x69, 0x66, 0x2d, 0x6d, 0x6f,   // - - - i f - m o
	0x64, 0x69, 0x66, 0x69, 0x65, 0x64, 0x2d, 0x73,   // d i f i e d - s
	0x69, 0x6e, 0x63, 0x65, 0x00, 0x00, 0x00, 0x0d,   // i n c e - - - -
	0x69, 0x66, 0x2d, 0x6e, 0x6f, 0x6e, 0x65, 0x2d,   // i f - n o n e -
	0x6d, 0x61, 0x74, 0x63, 0x68, 0x00, 0x00, 0x00,   // m a t c h - - -
	0x08, 0x69, 0x66, 0x2d, 0x72, 0x61, 0x6e, 0x67,   // - i f - r a n g
	0x65, 0x00, 0x00, 0x00, 0x13, 0x69, 0x66, 0x2d,   // e - - - - i f -
	0x75, 0x6e, 0x6d, 0x6f, 0x64, 0x69, 0x66, 0x69,   // u n m o d i f i
	0x65, 0x64, 0x2d, 0x73, 0x69, 0x6e, 0x63, 0x65,   // e d - s i n c e
	0x00, 0x00, 0x00, 0x0d, 0x6c, 0x61, 0x73, 0x74,   // - - - - l a s t
	0x2d, 0x6d, 0x6f, 0x64, 0x69, 0x66, 0x69, 0x65,   // - m o d i f i e
	0x64, 0x00, 0x00, 0x00, 0x08, 0x6c, 0x6f, 0x63,   // d - - - - l o c
	0x61, 0x74, 0x69, 0x6f, 0x6e, 0x00, 0x00, 0x00,   // a t i o n - - -
	0x0c, 0x6d, 0x61, 0x78, 0x2d, 0x66, 0x6f, 0x72,   // - m a x - f o r
	0x77, 0x61, 0x72, 0x64, 0x73, 0x00, 0x00, 0x00,   // w a r d s - - -
	0x06, 0x70, 0x72, 0x61, 0x67, 0x6d, 0x61, 0x00,   // - p r a g m a -
	0x00, 0x00, 0x12, 0x70, 0x72, 0x6f, 0x78, 0x79,   // - - - p r o x y
	0x2d, 0x61, 0x75, 0x74, 0x68, 0x65, 0x6e, 0x74,   // - a u t h e n t
	0x69, 0x63, 0x61, 0x74, 0x65, 0x00, 0x00, 0x00,   // i c a t e - - -
	0x13, 0x70, 0x72, 0x6f, 0x78, 0x79, 0x2d, 0x61,   // - p r o x y - a
	0x75, 0x74, 0x68, 0x6f, 0x72, 0x69, 0x7a, 0x61,   // u t h o r i z a
	0x74, 0x69, 0x6f, 0x6e, 0x00, 0x00, 0x00, 0x05,   // t i o n - - - -
	0x72, 0x61, 0x6e, 0x67, 0x65, 0x00, 0x00, 0x00,   // r a n g e - - -
	0x07, 0x72, 0x65, 0x66, 0x65, 0x72, 0x65, 0x72,   // - r e f e r e r
	0x00, 0x00, 0x00, 0x0b, 0x72, 0x65, 0x74, 0x72,   // - - - - r e t r
	0x79, 0x2d, 0x61, 0x66, 0x74, 0x65, 0x72, 0x00,   // y - a f t e r -
	0x00, 0x00, 0x06, 0x73, 0x65, 0x72, 0x76, 0x65,   // - - - s e r v e
	0x72, 0x00, 0x00, 0x00, 0x02, 0x74, 0x65, 0x00,   // r - - - - t e -
	0x00, 0x00, 0x07, 0x74, 0x72, 0x61, 0x69, 0x6c,   // - - - t r a i l
	0x65, 0x72, 0x00, 0x00, 0x00, 0x11, 0x74, 0x72,   // e r - - - - t r
	0x61, 0x6e, 0x73, 0x66, 0x65, 0x72, 0x2d, 0x65,   // a n s f e r - e
	0x6e, 0x63, 0x6f, 0x64, 0x69, 0x6e, 0x67, 0x00,   // n c o d i n g -
	0x00, 0x00, 0x07, 0x75, 0x70, 0x67, 0x72, 0x61,   // - - - u p g r a
	0x64, 0x65, 0x00, 0x00, 0x00, 0x0a, 0x75, 0x73,   // d e - - - - u s
	0x65, 0x72, 0x2d, 0x61, 0x67, 0x65, 0x6e, 0x74,   // e r - a g e n t
	0x00, 0x00, 0x00, 0x04, 0x76, 0x61, 0x72, 0x79,   // - - - - v a r y
	0x00, 0x00, 0x00, 0x03, 0x76, 0x69, 0x61, 0x00,   // - - - - v i a -
	0x00, 0x00, 0x07, 0x77, 0x61, 0x72, 0x6e, 0x69,   // - - - w a r n i
	0x6e, 0x67, 0x00, 0x00, 0x00, 0x10, 0x77, 0x77,   // n g - - - - w w
	0x77, 0x2d, 0x61, 0x75, 0x74, 0x68, 0x65, 0x6e,   // w - a u t h e n
	0x74, 0x69, 0x63, 0x61, 0x74, 0x65, 0x00, 0x00,   // t i c a t e - -
	0x00, 0x06, 0x6d, 0x65, 0x74, 0x68, 0x6f, 0x64,   // - - m e t h o d
	0x00, 0x00, 0x00, 0x03, 0x67, 0x65, 0x74, 0x00,   // - - - - g e t -
	0x00, 0x00, 0x06, 0x73, 0x74, 0x61, 0x74, 0x75,   // - - - s t a t u
	0x73, 0x00, 0x00, 0x00, 0x06, 0x32, 0x30, 0x30,   // s - - - - 2 0 0
	0x20, 0x4f, 0x4b, 0x00, 0x00, 0x00, 0x07, 0x76,   // - O K - - - - v
	0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x00, 0x00,   // e r s i o n - -
	0x00, 0x08, 0x48, 0x54, 0x54, 0x50, 0x2f, 0x31,   // - - H T T P - 1
	0x2e, 0x31, 0x00, 0x00, 0x00, 0x03, 0x75, 0x72,   // - 1 - - - - u r
	0x6c, 0x00, 0x00, 0x00, 0x06, 0x70, 0x75, 0x62,   // l - - - - p u b
	0x6c, 0x69, 0x63, 0x00, 0x00, 0x00, 0x0a, 0x73,   // l i c - - - - s
	0x65, 0x74, 0x2d, 0x63, 0x6f, 0x6f, 0x6b, 0x69,   // e t - c o o k i
	0x65, 0x00, 0x00, 0x00, 0x0a, 0x6b, 0x65, 0x65,   // e - - - - k e e
	0x70, 0x2d, 0x61, 0x6c, 0x69, 0x76, 0x65, 0x00,   // p - a l i v e -
	0x00, 0x00, 0x06, 0x6f, 0x72, 0x69, 0x67, 0x69,   // - - - o r i g i
	0x6e, 0x31, 0x30, 0x30, 0x31, 0x30, 0x31, 0x32,   // n 1 0 0 1 0 1 2
	0x30, 0x31, 0x32, 0x30, 0x32, 0x32, 0x30, 0x35,   // 0 1 2 0 2 2 0 5
	0x32, 0x30, 0x36, 0x33, 0x30, 0x30, 0x33, 0x30,   // 2 0 6 3 0 0 3 0
	0x32, 0x33, 0x30, 0x33, 0x33, 0x30, 0x34, 0x33,   // 2 3 0 3 3 0 4 3
	0x30, 0x35, 0x33, 0x30, 0x36, 0x33, 0x30, 0x37,   // 0 5 3 0 6 3 0 7
	0x34, 0x30, 0x32, 0x34, 0x30, 0x35, 0x34, 0x30,   // 4 0 2 4 0 5 4 0
	0x36, 0x34, 0x30, 0x37, 0x34, 0x30, 0x38, 0x34,   // 6 4 0 7 4 0 8 4
	0x30, 0x39, 0x34, 0x31, 0x30, 0x34, 0x31, 0x31,   // 0 9 4 1 0 4 1 1
	0x34, 0x31, 0x32, 0x34, 0x31, 0x33, 0x34, 0x31,   // 4 1 2 4 1 3 4 1
	0x34, 0x34, 0x31, 0x35, 0x34, 0x31, 0x36, 0x34,   // 4 4 1 5 4 1 6 4
	0x31, 0x37, 0x35, 0x30, 0x32, 0x35, 0x30, 0x34,   // 1 7 5 0 2 5 0 4
	0x35, 0x30, 0x35, 0x32, 0x30, 0x33, 0x20, 0x4e,   // 5 0 5 2 0 3 - N
	0x6f, 0x6e, 0x2d, 0x41, 0x75, 0x74, 0x68, 0x6f,   // o n - A u t h o
	0x72, 0x69, 0x74, 0x61, 0x74, 0x69, 0x76, 0x65,   // r i t a t i v e
	0x20, 0x49, 0x6e, 0x66, 0x6f, 0x72, 0x6d, 0x61,   // - I n f o r m a
	0x74, 0x69, 0x6f, 0x6e, 0x32, 0x30, 0x34, 0x20,   // t i o n 2 0 4 -
	0x4e, 0x6f, 0x20, 0x43, 0x6f, 0x6e, 0x74, 0x65,   // N o - C o n t e
	0x6e, 0x74, 0x33, 0x30, 0x31, 0x20, 0x4d, 0x6f,   // n t 3 0 1 - M o
	0x76, 0x65, 0x64, 0x20, 0x50, 0x65, 0x72, 0x6d,   // v e d - P e r m
	0x61, 0x6e, 0x65, 0x6e, 0x74, 0x6c, 0x79, 0x34,   // a n e n t l y 4
	0x30, 0x30, 0x20, 0x42, 0x61, 0x64, 0x20, 0x52,   // 0 0 - B a d - R
	0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x34, 0x30,   // e q u e s t 4 0
	0x31, 0x20, 0x55, 0x6e, 0x61, 0x75, 0x74, 0x68,   // 1 - U n a u t h
	0x6f, 0x72, 0x69, 0x7a, 0x65, 0x64, 0x34, 0x30,   // o r i z e d 4 0
	0x33, 0x20, 0x46, 0x6f, 0x72, 0x62, 0x69, 0x64,   // 3 - F o r b i d
	0x64, 0x65, 0x6e, 0x34, 0x30, 0x34, 0x20, 0x4e,   // d e n 4 0 4 - N
	0x6f, 0x74, 0x20, 0x46, 0x6f, 0x75, 0x6e, 0x64,   // o t - F o u n d
	0x35, 0x30, 0x30, 0x20, 0x49, 0x6e, 0x74, 0x65,   // 5 0 0 - I n t e
	0x72, 0x6e, 0x61, 0x6c, 0x20, 0x53, 0x65, 0x72,   // r n a l - S e r
	0x76, 0x65, 0x72, 0x20, 0x45, 0x72, 0x72, 0x6f,   // v e r - E r r o
	0x72, 0x35, 0x30, 0x31, 0x20, 0x4e, 0x6f, 0x74,   // r 5 0 1 - N o t
	0x20, 0x49, 0x6d, 0x70, 0x6c, 0x65, 0x6d, 0x65,   // - I m p l e m e
	0x6e, 0x74, 0x65, 0x64, 0x35, 0x30, 0x33, 0x20,   // n t e d 5 0 3 -
	0x53, 0x65, 0x72, 0x76, 0x69, 0x63, 0x65, 0x20,   // S e r v i c e -
	0x55, 0x6e, 0x61, 0x76, 0x61, 0x69, 0x6c, 0x61,   // U n a v a i l a
	0x62, 0x6c, 0x65, 0x4a, 0x61, 0x6e, 0x20, 0x46,   // b l e J a n - F
	0x65, 0x62, 0x20, 0x4d, 0x61, 0x72, 0x20, 0x41,   // e b - M a r - A
	0x70, 0x72, 0x20, 0x4d, 0x61, 0x79, 0x20, 0x4a,   // p r - M a y - J
	0x75, 0x6e, 0x20, 0x4a, 0x75, 0x6c, 0x20, 0x41,   // u n - J u l - A
	0x75, 0x67, 0x20, 0x53, 0x65, 0x70, 0x74, 0x20,   // u g - S e p t -
	0x4f, 0x63, 0x74, 0x20, 0x4e, 0x6f, 0x76, 0x20,   // O c t - N o v -
	0x44, 0x65, 0x63, 0x20, 0x30, 0x30, 0x3a, 0x30,   // D e c - 0 0 - 0
	0x30, 0x3a, 0x30, 0x30, 0x20, 0x4d, 0x6f, 0x6e,   // 0 - 0 0 - M o n
	0x2c, 0x20, 0x54, 0x75, 0x65, 0x2c, 0x20, 0x57,   // - - T u e - - W
	0x65, 0x64, 0x2c, 0x20, 0x54, 0x68, 0x75, 0x2c,   // e d - - T h u -
	0x20, 0x46, 0x72, 0x69, 0x2c, 0x20, 0x53, 0x61,   // - F r i - - S a
	0x74, 0x2c, 0x20, 0x53, 0x75, 0x6e, 0x2c, 0x20,   // t - - S u n - -
	0x47, 0x4d, 0x54, 0x63, 0x68, 0x75, 0x6e, 0x6b,   // G M T c h u n k
	0x65, 0x64, 0x2c, 0x74, 0x65, 0x78, 0x74, 0x2f,   // e d - t e x t -
	0x68, 0x74, 0x6d, 0x6c, 0x2c, 0x69, 0x6d, 0x61,   // h t m l - i m a
	0x67, 0x65, 0x2f, 0x70, 0x6e, 0x67, 0x2c, 0x69,   // g e - p n g - i
	0x6d, 0x61, 0x67, 0x65, 0x2f, 0x6a, 0x70, 0x67,   // m a g e - j p g
	0x2c, 0x69, 0x6d, 0x61, 0x67, 0x65, 0x2f, 0x67,   // - i m a g e - g
	0x69, 0x66, 0x2c, 0x61, 0x70, 0x70, 0x6c, 0x69,   // i f - a p p l i
	0x63, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x2f, 0x78,   // c a t i o n - x
	0x6d, 0x6c, 0x2c, 0x61, 0x70, 0x70, 0x6c, 0x69,   // m l - a p p l i
	0x63, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x2f, 0x78,   // c a t i o n - x
	0x68, 0x74, 0x6d, 0x6c, 0x2b, 0x78, 0x6d, 0x6c,   // h t m l - x m l
	0x2c, 0x74, 0x65, 0x78, 0x74, 0x2f, 0x70, 0x6c,   // - t e x t - p l
	0x61, 0x69, 0x6e, 0x2c, 0x74, 0x65, 0x78, 0x74,   // a i n - t e x t
	0x2f, 0x6a, 0x61, 0x76, 0x61, 0x73, 0x63, 0x72,   // - j a v a s c r
	0x69, 0x70, 0x74, 0x2c, 0x70, 0x75, 0x62, 0x6c,   // i p t - p u b l
	0x69, 0x63, 0x70, 0x72, 0x69, 0x76, 0x61, 0x74,   // i c p r i v a t
	0x65, 0x6d, 0x61, 0x78, 0x2d, 0x61, 0x67, 0x65,   // e m a x - a g e
	0x3d, 0x67, 0x7a, 0x69, 0x70, 0x2c, 0x64, 0x65,   // - g z i p - d e
	0x66, 0x6c, 0x61, 0x74, 0x65, 0x2c, 0x73, 0x64,   // f l a t e - s d
	0x63, 0x68, 0x63, 0x68, 0x61, 0x72, 0x73, 0x65,   // c h c h a r s e
	0x74, 0x3d, 0x75, 0x74, 0x66, 0x2d, 0x38, 0x63,   // t - u t f - 8 c
	0x68, 0x61, 0x72, 0x73, 0x65, 0x74, 0x3d, 0x69,   // h a r s e t - i
	0x73, 0x6f, 0x2d, 0x38, 0x38, 0x35, 0x39, 0x2d,   // s o - 8 8 5 9 -
	0x31, 0x2c, 0x75, 0x74, 0x66, 0x2d, 0x2c, 0x2a,   // 1 - u t f - - -
	0x2c, 0x65, 0x6e, 0x71, 0x3d, 0x30, 0x2e          // - e n q - 0 -
};

// use for zlib data types
void *
SpdyStream3::zlib_allocator(void *opaque, uInt items, uInt size)
{
  return moz_xmalloc(items * size);
}

// use for zlib data types
void
SpdyStream3::zlib_destructor(void *opaque, void *addr)
{
  moz_free(addr);
}

// This can be called N times.. 1 for syn_reply and 0->N for headers
nsresult
SpdyStream3::Uncompress(z_stream *context,
                        char *blockStart,
                        uint32_t blockLen)
{
  mDecompressedBytes += blockLen;

  context->avail_in = blockLen;
  context->next_in = reinterpret_cast<unsigned char *>(blockStart);
  bool triedDictionary = false;

  do {
    context->next_out =
      reinterpret_cast<unsigned char *>(mDecompressBuffer.get()) +
      mDecompressBufferUsed;
    context->avail_out = mDecompressBufferSize - mDecompressBufferUsed;
    int zlib_rv = inflate(context, Z_NO_FLUSH);

    if (zlib_rv == Z_NEED_DICT) {
      if (triedDictionary) {
        LOG3(("SpdySession3::Uncompress %p Dictionary Error\n", this));
        return NS_ERROR_ILLEGAL_VALUE;
      }

      triedDictionary = true;
      inflateSetDictionary(context, kDictionary, sizeof(kDictionary));
    }

    if (zlib_rv == Z_DATA_ERROR)
      return NS_ERROR_ILLEGAL_VALUE;

    if (zlib_rv == Z_MEM_ERROR)
      return NS_ERROR_FAILURE;

    // zlib's inflate() decreases context->avail_out by the amount it places
    // in the output buffer

    mDecompressBufferUsed += mDecompressBufferSize - mDecompressBufferUsed -
      context->avail_out;

    // When there is no more output room, but input still available then
    // increase the output space
    if (zlib_rv == Z_OK &&
        !context->avail_out && context->avail_in) {
      LOG3(("SpdyStream3::Uncompress %p Large Headers - so far %d",
            this, mDecompressBufferSize));
      SpdySession3::EnsureBuffer(mDecompressBuffer,
                                 mDecompressBufferSize + 4096,
                                 mDecompressBufferUsed,
                                 mDecompressBufferSize);
    }
  }
  while (context->avail_in);
  return NS_OK;
}

// mDecompressBuffer contains 0 to N uncompressed Name/Value Header blocks
nsresult
SpdyStream3::FindHeader(nsCString name,
                        nsDependentCSubstring &value)
{
  const unsigned char *nvpair = reinterpret_cast<unsigned char *>
    (mDecompressBuffer.get()) + 4;
  const unsigned char *lastHeaderByte = reinterpret_cast<unsigned char *>
    (mDecompressBuffer.get()) + mDecompressBufferUsed;
  if (lastHeaderByte < nvpair)
    return NS_ERROR_ILLEGAL_VALUE;

  do {
    uint32_t numPairs = PR_ntohl(reinterpret_cast<const uint32_t *>(nvpair)[-1]);

    for (uint32_t index = 0; index < numPairs; ++index) {
      if (lastHeaderByte < nvpair + 4)
        return NS_ERROR_ILLEGAL_VALUE;
      uint32_t nameLen = (nvpair[0] << 24) + (nvpair[1] << 16) +
        (nvpair[2] << 8) + nvpair[3];
      if (lastHeaderByte < nvpair + 4 + nameLen)
        return NS_ERROR_ILLEGAL_VALUE;
      nsDependentCSubstring nameString =
        Substring(reinterpret_cast<const char *>(nvpair) + 4,
                  reinterpret_cast<const char *>(nvpair) + 4 + nameLen);
      if (lastHeaderByte < nvpair + 8 + nameLen)
        return NS_ERROR_ILLEGAL_VALUE;
      uint32_t valueLen = (nvpair[4 + nameLen] << 24) + (nvpair[5 + nameLen] << 16) +
        (nvpair[6 + nameLen] << 8) + nvpair[7 + nameLen];
      if (lastHeaderByte < nvpair + 8 + nameLen + valueLen)
        return NS_ERROR_ILLEGAL_VALUE;
      if (nameString.Equals(name)) {
        value.Assign(((char *)nvpair) + 8 + nameLen, valueLen);
        return NS_OK;
      }

      // that pair didn't match - try the next one in this block
      nvpair += 8 + nameLen + valueLen;
    }

    // move to the next name/value header block (if there is one) - the
    // first pair is offset 4 bytes into it
    nvpair += 4;
  } while (lastHeaderByte >= nvpair);

  return NS_ERROR_NOT_AVAILABLE;
}

// ConvertHeaders is used to convert the response headers
// in a syn_reply or in 0..N headers frames that follow it into
// HTTP/1 format
nsresult
SpdyStream3::ConvertHeaders(nsACString &aHeadersOut)
{
  // :status and :version are required.
  nsDependentCSubstring status, version;
  nsresult rv = FindHeader(NS_LITERAL_CSTRING(":status"),
                           status);
  if (NS_FAILED(rv))
    return (rv == NS_ERROR_NOT_AVAILABLE) ? NS_ERROR_ILLEGAL_VALUE : rv;

  rv = FindHeader(NS_LITERAL_CSTRING(":version"),
                  version);
  if (NS_FAILED(rv))
    return (rv == NS_ERROR_NOT_AVAILABLE) ? NS_ERROR_ILLEGAL_VALUE : rv;

  if (mDecompressedBytes && mDecompressBufferUsed) {
    Telemetry::Accumulate(Telemetry::SPDY_SYN_REPLY_SIZE, mDecompressedBytes);
    uint32_t ratio =
      mDecompressedBytes * 100 / mDecompressBufferUsed;
    Telemetry::Accumulate(Telemetry::SPDY_SYN_REPLY_RATIO, ratio);
  }

  aHeadersOut.Truncate();
  aHeadersOut.SetCapacity(mDecompressBufferUsed + 64);

  // Connection, Keep-Alive and chunked transfer encodings are to be
  // removed.

  // Content-Length is 'advisory'.. we will not strip it because it can
  // create UI feedback.

  aHeadersOut.Append(version);
  aHeadersOut.Append(NS_LITERAL_CSTRING(" "));
  aHeadersOut.Append(status);
  aHeadersOut.Append(NS_LITERAL_CSTRING("\r\n"));

  const unsigned char *nvpair = reinterpret_cast<unsigned char *>
    (mDecompressBuffer.get()) + 4;
  const unsigned char *lastHeaderByte = reinterpret_cast<unsigned char *>
    (mDecompressBuffer.get()) + mDecompressBufferUsed;
  if (lastHeaderByte < nvpair)
    return NS_ERROR_ILLEGAL_VALUE;

  do {
    uint32_t numPairs = PR_ntohl(reinterpret_cast<const uint32_t *>(nvpair)[-1]);

    for (uint32_t index = 0; index < numPairs; ++index) {
      if (lastHeaderByte < nvpair + 4)
        return NS_ERROR_ILLEGAL_VALUE;

      uint32_t nameLen = (nvpair[0] << 24) + (nvpair[1] << 16) +
        (nvpair[2] << 8) + nvpair[3];
      if (lastHeaderByte < nvpair + 4 + nameLen)
        return NS_ERROR_ILLEGAL_VALUE;

      nsDependentCSubstring nameString =
        Substring(reinterpret_cast<const char *>(nvpair) + 4,
                  reinterpret_cast<const char *>(nvpair) + 4 + nameLen);

      if (lastHeaderByte < nvpair + 8 + nameLen)
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

          LOG3(("SpdyStream3::ConvertHeaders session=%p stream=%p "
                "upper case response header found. [%s]\n",
                mSession, this, toLog.get()));

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
        LOG3(("SpdyStream3::ConvertHeaders session=%p stream=%p "
              "transfer-encoding found. Chunked is invalid and no TE sent.",
              mSession, this));

        return NS_ERROR_ILLEGAL_VALUE;
      }

      uint32_t valueLen =
        (nvpair[4 + nameLen] << 24) + (nvpair[5 + nameLen] << 16) +
        (nvpair[6 + nameLen] << 8)  +   nvpair[7 + nameLen];

      if (lastHeaderByte < nvpair + 8 + nameLen + valueLen)
        return NS_ERROR_ILLEGAL_VALUE;

      // spdy transport level headers shouldn't be gatewayed into http/1
      if (!nameString.IsEmpty() && nameString[0] != ':' &&
          !nameString.Equals(NS_LITERAL_CSTRING("connection")) &&
          !nameString.Equals(NS_LITERAL_CSTRING("keep-alive"))) {
        nsDependentCSubstring valueString =
          Substring(reinterpret_cast<const char *>(nvpair) + 8 + nameLen,
                    reinterpret_cast<const char *>(nvpair) + 8 + nameLen +
                    valueLen);

        aHeadersOut.Append(nameString);
        aHeadersOut.Append(NS_LITERAL_CSTRING(": "));

        // expand NULL bytes in the value string
        for (char *cPtr = valueString.BeginWriting();
             cPtr && cPtr < valueString.EndWriting();
             ++cPtr) {
          if (*cPtr != 0) {
            aHeadersOut.Append(*cPtr);
            continue;
          }

          // NULLs are really "\r\nhdr: "
          aHeadersOut.Append(NS_LITERAL_CSTRING("\r\n"));
          aHeadersOut.Append(nameString);
          aHeadersOut.Append(NS_LITERAL_CSTRING(": "));
        }

        aHeadersOut.Append(NS_LITERAL_CSTRING("\r\n"));
      }
      // move to the next name/value pair in this block
      nvpair += 8 + nameLen + valueLen;
    }

    // move to the next name/value header block (if there is one) - the
    // first pair is offset 4 bytes into it
    nvpair += 4;
  } while (lastHeaderByte >= nvpair);

  aHeadersOut.Append(NS_LITERAL_CSTRING("X-Firefox-Spdy: 3\r\n\r\n"));
  LOG (("decoded response headers are:\n%s",
        aHeadersOut.BeginReading()));

  // The spdy formatted buffer isnt needed anymore - free it up
  mDecompressBuffer = nullptr;
  mDecompressBufferSize = 0;
  mDecompressBufferUsed = 0;

  return NS_OK;
}

void
SpdyStream3::ExecuteCompress(uint32_t flushMode)
{
  // Expect mZlib->avail_in and mZlib->next_in to be set.
  // Append the compressed version of next_in to mTxInlineFrame

  do
  {
    uint32_t avail = mTxInlineFrameSize - mTxInlineFrameUsed;
    if (avail < 1) {
      SpdySession3::EnsureBuffer(mTxInlineFrame,
                                mTxInlineFrameSize + 2000,
                                mTxInlineFrameUsed,
                                mTxInlineFrameSize);
      avail = mTxInlineFrameSize - mTxInlineFrameUsed;
    }

    mZlib->next_out = mTxInlineFrame + mTxInlineFrameUsed;
    mZlib->avail_out = avail;
    deflate(mZlib, flushMode);
    mTxInlineFrameUsed += avail - mZlib->avail_out;
  } while (mZlib->avail_in > 0 || !mZlib->avail_out);
}

void
SpdyStream3::CompressToFrame(uint32_t data)
{
  // convert the data to 4 byte network byte order and write that
  // to the compressed stream
  data = PR_htonl(data);

  mZlib->next_in = reinterpret_cast<unsigned char *> (&data);
  mZlib->avail_in = 4;
  ExecuteCompress(Z_NO_FLUSH);
}


void
SpdyStream3::CompressToFrame(const char *data, uint32_t len)
{
  // Format calls for a network ordered 32 bit length
  // followed by the utf8 string

  uint32_t networkLen = PR_htonl(len);

  // write out the length
  mZlib->next_in = reinterpret_cast<unsigned char *> (&networkLen);
  mZlib->avail_in = 4;
  ExecuteCompress(Z_NO_FLUSH);

  // write out the data
  mZlib->next_in = (unsigned char *)data;
  mZlib->avail_in = len;
  ExecuteCompress(Z_NO_FLUSH);
}

void
SpdyStream3::CompressFlushFrame()
{
  mZlib->next_in = (unsigned char *) "";
  mZlib->avail_in = 0;
  ExecuteCompress(Z_SYNC_FLUSH);
}

void
SpdyStream3::Close(nsresult reason)
{
  mTransaction->Close(reason);
}

void
SpdyStream3::UpdateRemoteWindow(int32_t delta)
{
  int64_t oldRemoteWindow = mRemoteWindow;
  mRemoteWindow += delta;
  if (oldRemoteWindow <= 0 && mRemoteWindow > 0) {
    // the window has been opened :)
    mSession->TransactionHasDataToWrite(this);
  }
}

//-----------------------------------------------------------------------------
// nsAHttpSegmentReader
//-----------------------------------------------------------------------------

nsresult
SpdyStream3::OnReadSegment(const char *buf,
                          uint32_t count,
                          uint32_t *countRead)
{
  LOG3(("SpdyStream3::OnReadSegment %p count=%d state=%x",
        this, count, mUpstreamState));

  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  MOZ_ASSERT(mSegmentReader, "OnReadSegment with null mSegmentReader");

  nsresult rv = NS_ERROR_UNEXPECTED;
  uint32_t dataLength;

  switch (mUpstreamState) {
  case GENERATING_SYN_STREAM:
    // The buffer is the HTTP request stream, including at least part of the
    // HTTP request header. This state's job is to build a SYN_STREAM frame
    // from the header information. count is the number of http bytes available
    // (which may include more than the header), and in countRead we return
    // the number of those bytes that we consume (i.e. the portion that are
    // header bytes)

    rv = ParseHttpRequestHeaders(buf, count, countRead);
    if (NS_FAILED(rv))
      return rv;
    LOG3(("ParseHttpRequestHeaders %p used %d of %d. complete = %d",
          this, *countRead, count, mSynFrameComplete));
    if (mSynFrameComplete) {
      AdjustInitialWindow();
      rv = TransmitFrame(nullptr, nullptr, true);
      if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
        // this can't happen
        MOZ_ASSERT(false, "Transmit Frame SYN_FRAME must at least buffer data");
        rv = NS_ERROR_UNEXPECTED;
      }

      ChangeState(GENERATING_REQUEST_BODY);
      break;
    }
    MOZ_ASSERT(*countRead == count, "Header parsing not complete but unused data");
    break;

  case GENERATING_REQUEST_BODY:
    if (mRemoteWindow <= 0) {
      *countRead = 0;
      LOG3(("SpdyStream3 this=%p, id 0x%X request body suspended because "
            "remote window is %d.\n", this, mStreamID, mRemoteWindow));
      mBlockedOnRwin = true;
      return NS_BASE_STREAM_WOULD_BLOCK;
    }
    mBlockedOnRwin = false;

    dataLength = std::min(count, mChunkSize);

    if (dataLength > mRemoteWindow)
      dataLength = static_cast<uint32_t>(mRemoteWindow);

    LOG3(("SpdyStream3 this=%p id 0x%X remote window is %d. Chunk is %d\n",
          this, mStreamID, mRemoteWindow, dataLength));
    mRemoteWindow -= dataLength;

    LOG3(("SpdyStream3 %p id %x request len remaining %d, "
          "count avail %d, chunk used %d",
          this, mStreamID, mRequestBodyLenRemaining, count, dataLength));
    if (dataLength > mRequestBodyLenRemaining)
      return NS_ERROR_UNEXPECTED;
    mRequestBodyLenRemaining -= dataLength;
    GenerateDataFrameHeader(dataLength, !mRequestBodyLenRemaining);
    ChangeState(SENDING_REQUEST_BODY);
    // NO BREAK

  case SENDING_REQUEST_BODY:
    MOZ_ASSERT(mTxInlineFrameUsed, "OnReadSegment Send Data Header 0b");
    rv = TransmitFrame(buf, countRead, false);
    MOZ_ASSERT(NS_FAILED(rv) || !mTxInlineFrameUsed,
               "Transmit Frame should be all or nothing");

    LOG3(("TransmitFrame() rv=%x returning %d data bytes. "
          "Header is %d Body is %d.",
          rv, *countRead, mTxInlineFrameUsed, mTxStreamFrameSize));

    // normalize a partial write with a WOULD_BLOCK into just a partial write
    // as some code will take WOULD_BLOCK to mean an error with nothing
    // written (e.g. nsHttpTransaction::ReadRequestSegment()
    if (rv == NS_BASE_STREAM_WOULD_BLOCK && *countRead)
      rv = NS_OK;

    // If that frame was all sent, look for another one
    if (!mTxInlineFrameUsed)
        ChangeState(GENERATING_REQUEST_BODY);
    break;

  case SENDING_FIN_STREAM:
    MOZ_ASSERT(false, "resuming partial fin stream out of OnReadSegment");
    break;

  default:
    MOZ_ASSERT(false, "SpdyStream3::OnReadSegment non-write state");
    break;
  }

  return rv;
}

//-----------------------------------------------------------------------------
// nsAHttpSegmentWriter
//-----------------------------------------------------------------------------

nsresult
SpdyStream3::OnWriteSegment(char *buf,
                            uint32_t count,
                            uint32_t *countWritten)
{
  LOG3(("SpdyStream3::OnWriteSegment %p count=%d state=%x 0x%X\n",
        this, count, mUpstreamState, mStreamID));

  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  MOZ_ASSERT(mSegmentWriter);

  if (!mPushSource)
    return mSegmentWriter->OnWriteSegment(buf, count, countWritten);

  nsresult rv;
  rv = mPushSource->GetBufferedData(buf, count, countWritten);
  if (NS_FAILED(rv))
    return rv;

  mSession->ConnectPushedStream(this);
  return NS_OK;
}

} // namespace mozilla::net
} // namespace mozilla

