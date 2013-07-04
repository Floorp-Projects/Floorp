/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "nsHttp.h"
#include "SpdySession2.h"
#include "SpdyStream2.h"
#include "nsAlgorithm.h"
#include "prnetdb.h"
#include "nsHttpRequestHead.h"
#include "mozilla/Telemetry.h"
#include "nsISocketTransport.h"
#include "nsISupportsPriority.h"
#include "nsHttpHandler.h"
#include <algorithm>

#ifdef DEBUG
// defined by the socket transport service while active
extern PRThread *gSocketThread;
#endif

namespace mozilla {
namespace net {

SpdyStream2::SpdyStream2(nsAHttpTransaction *httpTransaction,
                       SpdySession2 *spdySession,
                       nsISocketTransport *socketTransport,
                       uint32_t chunkSize,
                       z_stream *compressionContext,
                       int32_t priority)
  : mUpstreamState(GENERATING_SYN_STREAM),
    mTransaction(httpTransaction),
    mSession(spdySession),
    mSocketTransport(socketTransport),
    mSegmentReader(nullptr),
    mSegmentWriter(nullptr),
    mStreamID(0),
    mChunkSize(chunkSize),
    mSynFrameComplete(0),
    mRequestBlockedOnRead(0),
    mSentFinOnData(0),
    mRecvdFin(0),
    mFullyOpen(0),
    mSentWaitingFor(0),
    mSetTCPSocketBuffer(0),
    mTxInlineFrameSize(SpdySession2::kDefaultBufferSize),
    mTxInlineFrameUsed(0),
    mTxStreamFrameSize(0),
    mZlib(compressionContext),
    mRequestBodyLenRemaining(0),
    mPriority(priority),
    mTotalSent(0),
    mTotalRead(0)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  LOG3(("SpdyStream2::SpdyStream2 %p", this));

  mTxInlineFrame = new char[mTxInlineFrameSize];
}

SpdyStream2::~SpdyStream2()
{
  mStreamID = SpdySession2::kDeadStreamID;
}

// ReadSegments() is used to write data down the socket. Generally, HTTP
// request data is pulled from the approriate transaction and
// converted to SPDY data. Sometimes control data like a window-update is
// generated instead.

nsresult
SpdyStream2::ReadSegments(nsAHttpSegmentReader *reader,
                         uint32_t count,
                         uint32_t *countRead)
{
  LOG3(("SpdyStream2 %p ReadSegments reader=%p count=%d state=%x",
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

    if (!mTxInlineFrameUsed && NS_SUCCEEDED(rv) && (!*countRead)) {
      LOG3(("ReadSegments %p: Sending request data complete, mUpstreamState=%x",
            this, mUpstreamState));
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
    MOZ_ASSERT(false, "SpdyStream2::ReadSegments unknown state");
    break;
  }

  return rv;
}

// WriteSegments() is used to read data off the socket. Generally this is
// just the SPDY frame header and from there the appropriate SPDYStream
// is identified from the Stream-ID. The http transaction associated with
// that read then pulls in the data directly.

nsresult
SpdyStream2::WriteSegments(nsAHttpSegmentWriter *writer,
                          uint32_t count,
                          uint32_t *countWritten)
{
  LOG3(("SpdyStream2::WriteSegments %p count=%d state=%x",
        this, count, mUpstreamState));

  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  MOZ_ASSERT(!mSegmentWriter, "segment writer in progress");

  mSegmentWriter = writer;
  nsresult rv = mTransaction->WriteSegments(writer, count, countWritten);
  mSegmentWriter = nullptr;
  return rv;
}

PLDHashOperator
SpdyStream2::hdrHashEnumerate(const nsACString &key,
                             nsAutoPtr<nsCString> &value,
                             void *closure)
{
  SpdyStream2 *self = static_cast<SpdyStream2 *>(closure);

  self->CompressToFrame(key);
  self->CompressToFrame(value.get());
  return PL_DHASH_NEXT;
}

nsresult
SpdyStream2::ParseHttpRequestHeaders(const char *buf,
                                    uint32_t avail,
                                    uint32_t *countUsed)
{
  // Returns NS_OK even if the headers are incomplete
  // set mSynFrameComplete flag if they are complete

  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  MOZ_ASSERT(mUpstreamState == GENERATING_SYN_STREAM);

  LOG3(("SpdyStream2::ParseHttpRequestHeaders %p avail=%d state=%x",
        this, avail, mUpstreamState));

  mFlatHttpRequestHeaders.Append(buf, avail);

  // We can use the simple double crlf because firefox is the
  // only client we are parsing
  int32_t endHeader = mFlatHttpRequestHeaders.Find("\r\n\r\n");

  if (endHeader == kNotFound) {
    // We don't have all the headers yet
    LOG3(("SpdyStream2::ParseHttpRequestHeaders %p "
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

  // It is now OK to assign a streamID that we are assured will
  // be monotonically increasing amongst syn-streams on this
  // session
  mStreamID = mSession->RegisterStreamID(this);
  MOZ_ASSERT(mStreamID & 1,
             "Spdy Stream Channel ID must be odd");

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

  mTxInlineFrame[0] = SpdySession2::kFlag_Control;
  mTxInlineFrame[1] = 2;                          /* version */
  mTxInlineFrame[2] = 0;
  mTxInlineFrame[3] = SpdySession2::CONTROL_TYPE_SYN_STREAM;
  // 4 to 7 are length and flags, we'll fill that in later

  uint32_t networkOrderID = PR_htonl(mStreamID);
  memcpy(mTxInlineFrame + 8, &networkOrderID, 4);

  // this is the associated-to field, which is not used sending
  // from the client in the http binding
  memset (mTxInlineFrame + 12, 0, 4);

  // Priority flags are the C0 mask of byte 16.
  //
  // The other 6 bits of 16 are unused. Spdy/3 will expand
  // priority to 3 bits.
  //
  // When Spdy/3 implements WINDOW_UPDATE the lowest priority
  // streams over a threshold (32?) should be given tiny
  // receive windows, separate from their spdy priority
  //
  if (mPriority >= nsISupportsPriority::PRIORITY_LOW)
    mTxInlineFrame[16] = SpdySession2::kPri03;
  else if (mPriority >= nsISupportsPriority::PRIORITY_NORMAL)
    mTxInlineFrame[16] = SpdySession2::kPri02;
  else if (mPriority >= nsISupportsPriority::PRIORITY_HIGH)
    mTxInlineFrame[16] = SpdySession2::kPri01;
  else
    mTxInlineFrame[16] = SpdySession2::kPri00;

  mTxInlineFrame[17] = 0;                         /* unused */

  const char *methodHeader = mTransaction->RequestHead()->Method().get();

  nsCString hostHeader;
  mTransaction->RequestHead()->GetHeader(nsHttp::Host, hostHeader);

  nsCString versionHeader;
  if (mTransaction->RequestHead()->Version() == NS_HTTP_VERSION_1_1)
    versionHeader = NS_LITERAL_CSTRING("HTTP/1.1");
  else
    versionHeader = NS_LITERAL_CSTRING("HTTP/1.0");

  nsClassHashtable<nsCStringHashKey, nsCString> hdrHash;

  // use mRequestHead() to get a sense of how big to make the hash,
  // even though we are parsing the actual text stream because
  // it is legit to append headers.
  hdrHash.Init(1 + (mTransaction->RequestHead()->Headers().Count() * 2));

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

    if (name.Equals("method") ||
        name.Equals("version") ||
        name.Equals("scheme") ||
        name.Equals("keep-alive") ||
        name.Equals("accept-encoding") ||
        name.Equals("te") ||
        name.Equals("connection") ||
        name.Equals("url"))
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

  // The header block length
  uint16_t count = hdrHash.Count() + 4; /* method, scheme, url, version */
  CompressToFrame(count);

  // method, scheme, url, and version headers for request line

  CompressToFrame(NS_LITERAL_CSTRING("method"));
  CompressToFrame(methodHeader, strlen(methodHeader));
  CompressToFrame(NS_LITERAL_CSTRING("scheme"));
  CompressToFrame(NS_LITERAL_CSTRING("https"));
  CompressToFrame(NS_LITERAL_CSTRING("url"));
  CompressToFrame(mTransaction->RequestHead()->RequestURI());
  CompressToFrame(NS_LITERAL_CSTRING("version"));
  CompressToFrame(versionHeader);

  hdrHash.Enumerate(hdrHashEnumerate, this);
  CompressFlushFrame();

  // 4 to 7 are length and flags, which we can now fill in
  (reinterpret_cast<uint32_t *>(mTxInlineFrame.get()))[1] =
    PR_htonl(mTxInlineFrameUsed - 8);

  MOZ_ASSERT(!mTxInlineFrame[4],
             "Size greater than 24 bits");

  // Determine whether to put the fin bit on the syn stream frame or whether
  // to wait for a data packet to put it on.

  if (mTransaction->RequestHead()->Method() == nsHttp::Get ||
      mTransaction->RequestHead()->Method() == nsHttp::Connect ||
      mTransaction->RequestHead()->Method() == nsHttp::Head) {
    // for GET, CONNECT, and HEAD place the fin bit right on the
    // syn stream packet

    mSentFinOnData = 1;
    mTxInlineFrame[4] = SpdySession2::kFlag_Data_FIN;
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
    mTxInlineFrame[4] = SpdySession2::kFlag_Data_FIN;
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
SpdyStream2::UpdateTransportReadEvents(uint32_t count)
{
  mTotalRead += count;

  mTransaction->OnTransportStatus(mSocketTransport,
                                  NS_NET_STATUS_RECEIVING_FROM,
                                  mTotalRead);
}

void
SpdyStream2::UpdateTransportSendEvents(uint32_t count)
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
SpdyStream2::TransmitFrame(const char *buf,
                           uint32_t *countUsed,
                           bool forceCommitment)
{
  // If TransmitFrame returns SUCCESS than all the data is sent (or at least
  // buffered at the session level), if it returns WOULD_BLOCK then none of
  // the data is sent.

  // You can call this function with no data and no out parameter in order to
  // flush internal buffers that were previously blocked on writing. You can
  // of course feed new data to it as well.

  MOZ_ASSERT(mTxInlineFrameUsed, "empty stream frame in transmit");
  MOZ_ASSERT(mSegmentReader, "TransmitFrame with null mSegmentReader");
  MOZ_ASSERT((buf && countUsed) || (!buf && !countUsed),
             "TransmitFrame arguments inconsistent");

  uint32_t transmittedCount;
  nsresult rv;

  LOG3(("SpdyStream2::TransmitFrame %p inline=%d stream=%d",
        this, mTxInlineFrameUsed, mTxStreamFrameSize));
  if (countUsed)
    *countUsed = 0;

  // In the (relatively common) event that we have a small amount of data
  // split between the inlineframe and the streamframe, then move the stream
  // data into the inlineframe via copy in order to coalesce into one write.
  // Given the interaction with ssl this is worth the small copy cost.
  if (mTxStreamFrameSize && mTxInlineFrameUsed &&
      mTxStreamFrameSize < SpdySession2::kDefaultBufferSize &&
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
  // bytes through to the SpdySession2 and then the HttpConnection which calls
  // the socket write function. It will accept all of the inline and stream
  // data because of the above 'commitment' even if it has to buffer

  rv = mSegmentReader->OnReadSegment(mTxInlineFrame, mTxInlineFrameUsed,
                                     &transmittedCount);
  LOG3(("SpdyStream2::TransmitFrame for inline session=%p "
        "stream=%p result %x len=%d",
        mSession, this, rv, transmittedCount));

  MOZ_ASSERT(rv != NS_BASE_STREAM_WOULD_BLOCK,
             "inconsistent inline commitment result");

  if (NS_FAILED(rv))
    return rv;

  MOZ_ASSERT(transmittedCount == mTxInlineFrameUsed,
             "inconsistent inline commitment count");

  SpdySession2::LogIO(mSession, this, "Writing from Inline Buffer",
                     mTxInlineFrame, transmittedCount);

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

    LOG3(("SpdyStream2::TransmitFrame for regular session=%p "
          "stream=%p result %x len=%d",
          mSession, this, rv, transmittedCount));

    MOZ_ASSERT(rv != NS_BASE_STREAM_WOULD_BLOCK,
               "inconsistent stream commitment result");

    if (NS_FAILED(rv))
      return rv;

    MOZ_ASSERT(transmittedCount == mTxStreamFrameSize,
               "inconsistent stream commitment count");

    SpdySession2::LogIO(mSession, this, "Writing from Transaction Buffer",
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
SpdyStream2::ChangeState(enum stateType newState)
{
  LOG3(("SpdyStream2::ChangeState() %p from %X to %X",
        this, mUpstreamState, newState));
  mUpstreamState = newState;
  return;
}

void
SpdyStream2::GenerateDataFrameHeader(uint32_t dataLength, bool lastFrame)
{
  LOG3(("SpdyStream2::GenerateDataFrameHeader %p len=%d last=%d",
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
    mTxInlineFrame[4] |= SpdySession2::kFlag_Data_FIN;
    if (dataLength)
      mSentFinOnData = 1;
  }
}

void
SpdyStream2::CompressToFrame(const nsACString &str)
{
  CompressToFrame(str.BeginReading(), str.Length());
}

void
SpdyStream2::CompressToFrame(const nsACString *str)
{
  CompressToFrame(str->BeginReading(), str->Length());
}

// Dictionary taken from
// http://dev.chromium.org/spdy/spdy-protocol/spdy-protocol-draft2
// Name/Value Header Block Format
// spec indicates that the compression dictionary is not null terminated
// but in reality it is. see:
// https://groups.google.com/forum/#!topic/spdy-dev/2pWxxOZEIcs

const char *SpdyStream2::kDictionary =
  "optionsgetheadpostputdeletetraceacceptaccept-charsetaccept-encodingaccept-"
  "languageauthorizationexpectfromhostif-modified-sinceif-matchif-none-matchi"
  "f-rangeif-unmodifiedsincemax-forwardsproxy-authorizationrangerefererteuser"
  "-agent10010120020120220320420520630030130230330430530630740040140240340440"
  "5406407408409410411412413414415416417500501502503504505accept-rangesageeta"
  "glocationproxy-authenticatepublicretry-afterservervarywarningwww-authentic"
  "ateallowcontent-basecontent-encodingcache-controlconnectiondatetrailertran"
  "sfer-encodingupgradeviawarningcontent-languagecontent-lengthcontent-locati"
  "oncontent-md5content-rangecontent-typeetagexpireslast-modifiedset-cookieMo"
  "ndayTuesdayWednesdayThursdayFridaySaturdaySundayJanFebMarAprMayJunJulAugSe"
  "pOctNovDecchunkedtext/htmlimage/pngimage/jpgimage/gifapplication/xmlapplic"
  "ation/xhtmltext/plainpublicmax-agecharset=iso-8859-1utf-8gzipdeflateHTTP/1"
  ".1statusversionurl";

// use for zlib data types
void *
SpdyStream2::zlib_allocator(void *opaque, uInt items, uInt size)
{
  return moz_xmalloc(items * size);
}

// use for zlib data types
void
SpdyStream2::zlib_destructor(void *opaque, void *addr)
{
  moz_free(addr);
}

void
SpdyStream2::ExecuteCompress(uint32_t flushMode)
{
  // Expect mZlib->avail_in and mZlib->next_in to be set.
  // Append the compressed version of next_in to mTxInlineFrame

  do
  {
    uint32_t avail = mTxInlineFrameSize - mTxInlineFrameUsed;
    if (avail < 1) {
      SpdySession2::EnsureBuffer(mTxInlineFrame,
                                mTxInlineFrameSize + 2000,
                                mTxInlineFrameUsed,
                                mTxInlineFrameSize);
      avail = mTxInlineFrameSize - mTxInlineFrameUsed;
    }

    mZlib->next_out = reinterpret_cast<unsigned char *> (mTxInlineFrame.get()) +
      mTxInlineFrameUsed;
    mZlib->avail_out = avail;
    deflate(mZlib, flushMode);
    mTxInlineFrameUsed += avail - mZlib->avail_out;
  } while (mZlib->avail_in > 0 || !mZlib->avail_out);
}

void
SpdyStream2::CompressToFrame(uint16_t data)
{
  // convert the data to network byte order and write that
  // to the compressed stream

  data = PR_htons(data);

  mZlib->next_in = reinterpret_cast<unsigned char *> (&data);
  mZlib->avail_in = 2;
  ExecuteCompress(Z_NO_FLUSH);
}


void
SpdyStream2::CompressToFrame(const char *data, uint32_t len)
{
  // Format calls for a network ordered 16 bit length
  // followed by the utf8 string

  // for now, silently truncate headers greater than 64KB. Spdy/3 will
  // fix this by making the len a 32 bit quantity
  if (len > 0xffff)
    len = 0xffff;

  uint16_t networkLen = PR_htons(len);

  // write out the length
  mZlib->next_in = reinterpret_cast<unsigned char *> (&networkLen);
  mZlib->avail_in = 2;
  ExecuteCompress(Z_NO_FLUSH);

  // write out the data
  mZlib->next_in = (unsigned char *)data;
  mZlib->avail_in = len;
  ExecuteCompress(Z_NO_FLUSH);
}

void
SpdyStream2::CompressFlushFrame()
{
  mZlib->next_in = (unsigned char *) "";
  mZlib->avail_in = 0;
  ExecuteCompress(Z_SYNC_FLUSH);
}

void
SpdyStream2::Close(nsresult reason)
{
  mTransaction->Close(reason);
}

//-----------------------------------------------------------------------------
// nsAHttpSegmentReader
//-----------------------------------------------------------------------------

nsresult
SpdyStream2::OnReadSegment(const char *buf,
                          uint32_t count,
                          uint32_t *countRead)
{
  LOG3(("SpdyStream2::OnReadSegment %p count=%d state=%x",
        this, count, mUpstreamState));

  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  MOZ_ASSERT(mSegmentReader);

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
      MOZ_ASSERT(mTxInlineFrameUsed, "OnReadSegment SynFrameComplete 0b");
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
    dataLength = std::min(count, mChunkSize);
    LOG3(("SpdyStream2 %p id %x request len remaining %d, "
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
    MOZ_ASSERT(false, "SpdyStream2::OnReadSegment non-write state");
    break;
  }

  return rv;
}

//-----------------------------------------------------------------------------
// nsAHttpSegmentWriter
//-----------------------------------------------------------------------------

nsresult
SpdyStream2::OnWriteSegment(char *buf,
                           uint32_t count,
                           uint32_t *countWritten)
{
  LOG3(("SpdyStream2::OnWriteSegment %p count=%d state=%x",
        this, count, mUpstreamState));

  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  MOZ_ASSERT(mSegmentWriter, "OnWriteSegment with null mSegmentWriter");

  return mSegmentWriter->OnWriteSegment(buf, count, countWritten);
}

} // namespace mozilla::net
} // namespace mozilla

