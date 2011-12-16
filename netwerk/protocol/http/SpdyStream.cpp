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
#include "nsAlgorithm.h"
#include "prnetdb.h"
#include "nsHttpRequestHead.h"
#include "mozilla/Telemetry.h"
#include "nsISocketTransport.h"
#include "nsISupportsPriority.h"

#ifdef DEBUG
// defined by the socket transport service while active
extern PRThread *gSocketThread;
#endif

namespace mozilla {
namespace net {

SpdyStream::SpdyStream(nsAHttpTransaction *httpTransaction,
                       SpdySession *spdySession,
                       nsISocketTransport *socketTransport,
                       PRUint32 chunkSize,
                       z_stream *compressionContext,
                       PRInt32 priority)
  : mUpstreamState(GENERATING_SYN_STREAM),
    mTransaction(httpTransaction),
    mSession(spdySession),
    mSocketTransport(socketTransport),
    mSegmentReader(nsnull),
    mSegmentWriter(nsnull),
    mStreamID(0),
    mChunkSize(chunkSize),
    mSynFrameComplete(0),
    mBlockedOnWrite(0),
    mRequestBlockedOnRead(0),
    mSentFinOnData(0),
    mRecvdFin(0),
    mFullyOpen(0),
    mTxInlineFrameAllocation(SpdySession::kDefaultBufferSize),
    mTxInlineFrameSize(0),
    mTxInlineFrameSent(0),
    mTxStreamFrameSize(0),
    mTxStreamFrameSent(0),
    mZlib(compressionContext),
    mRequestBodyLen(0),
    mPriority(priority)
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");

  LOG3(("SpdyStream::SpdyStream %p", this));

  mTxInlineFrame = new char[mTxInlineFrameAllocation];
}

SpdyStream::~SpdyStream()
{
}

// ReadSegments() is used to write data down the socket. Generally, HTTP
// request data is pulled from the approriate transaction and
// converted to SPDY data. Sometimes control data like a window-update is
// generated instead.

nsresult
SpdyStream::ReadSegments(nsAHttpSegmentReader *reader,
                         PRUint32 count,
                         PRUint32 *countRead)
{
  LOG3(("SpdyStream %p ReadSegments reader=%p count=%d state=%x",
        this, reader, count, mUpstreamState));

  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");
  
  nsresult rv = NS_ERROR_UNEXPECTED;
  mBlockedOnWrite = 0;
  mRequestBlockedOnRead = 0;

  switch (mUpstreamState) {
  case GENERATING_SYN_STREAM:
  case GENERATING_REQUEST_BODY:
  case SENDING_REQUEST_BODY:
    // Call into the HTTP Transaction to generate the HTTP request
    // stream. That stream will show up in OnReadSegment().
    mSegmentReader = reader;
    rv = mTransaction->ReadSegments(this, count, countRead);
    mSegmentReader = nsnull;

    if (NS_SUCCEEDED(rv) &&
        mUpstreamState == GENERATING_SYN_STREAM &&
        !mSynFrameComplete)
      mBlockedOnWrite = 1;
    
    // Mark that we are blocked on read if we the http transaction
    // is going to get us going again.
    if (rv == NS_BASE_STREAM_WOULD_BLOCK && !mBlockedOnWrite)
      mRequestBlockedOnRead = 1;

    if (!mBlockedOnWrite && NS_SUCCEEDED(rv) && (!*countRead)) {
      LOG3(("ReadSegments %p Send Request data complete from %x",
            this, mUpstreamState));
      if (mSentFinOnData) {
        ChangeState(UPSTREAM_COMPLETE);
      }
      else {
        GenerateDataFrameHeader(0, true);
        ChangeState(SENDING_FIN_STREAM);
        mBlockedOnWrite = 1;
        rv = NS_BASE_STREAM_WOULD_BLOCK;
      }
    }

    break;

  case SENDING_SYN_STREAM:
    // We were trying to send the SYN-STREAM but only got part of it out
    // before being blocked. Try and send more.
    mSegmentReader = reader;
    rv = TransmitFrame(nsnull, nsnull);
    mSegmentReader = nsnull;
    *countRead = 0;
    if (NS_SUCCEEDED(rv))
      rv = NS_BASE_STREAM_WOULD_BLOCK;

    if (!mTxInlineFrameSize) {
      if (mSentFinOnData) {
        ChangeState(UPSTREAM_COMPLETE);
        rv = NS_OK;
      }
      else {
        ChangeState(GENERATING_REQUEST_BODY);
        mBlockedOnWrite = 1;
      }
    }
    break;

  case SENDING_FIN_STREAM:
    // We were trying to send the SYN-STREAM but only got part of it out
    // before being blocked. Try and send more.
    if (!mSentFinOnData) {
      mSegmentReader = reader;
      rv = TransmitFrame(nsnull, nsnull);
      mSegmentReader = nsnull;
      if (!mTxInlineFrameSize)
        ChangeState(UPSTREAM_COMPLETE);
    }
    else {
      rv = NS_OK;
      mTxInlineFrameSize = 0;         // cancel fin data packet
      ChangeState(UPSTREAM_COMPLETE);
    }
    
    *countRead = 0;

    // don't change OK to WOULD BLOCK. we are really done sending if OK
    break;

  default:
    NS_ABORT_IF_FALSE(false, "SpdyStream::ReadSegments unknown state");
    break;
  }

  return rv;
}

// WriteSegments() is used to read data off the socket. Generally this is
// just the SPDY frame header and from there the appropriate SPDYStream
// is identified from the Stream-ID. The http transaction associated with
// that read then pulls in the data directly.

nsresult
SpdyStream::WriteSegments(nsAHttpSegmentWriter *writer,
                          PRUint32 count,
                          PRUint32 *countWritten)
{
  LOG3(("SpdyStream::WriteSegments %p count=%d state=%x",
        this, count, mUpstreamState));
  
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");
  NS_ABORT_IF_FALSE(!mSegmentWriter, "segment writer in progress");

  mSegmentWriter = writer;
  nsresult rv = mTransaction->WriteSegments(writer, count, countWritten);
  mSegmentWriter = nsnull;
  return rv;
}

PLDHashOperator
SpdyStream::hdrHashEnumerate(const nsACString &key,
                             nsAutoPtr<nsCString> &value,
                             void *closure)
{
  SpdyStream *self = static_cast<SpdyStream *>(closure);

  self->CompressToFrame(key);
  self->CompressToFrame(value.get());
  return PL_DHASH_NEXT;
}

nsresult
SpdyStream::ParseHttpRequestHeaders(const char *buf,
                                    PRUint32 avail,
                                    PRUint32 *countUsed)
{
  // Returns NS_OK even if the headers are incomplete
  // set mSynFrameComplete flag if they are complete

  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");
  NS_ABORT_IF_FALSE(mUpstreamState == GENERATING_SYN_STREAM, "wrong state");

  LOG3(("SpdyStream::ParseHttpRequestHeaders %p avail=%d state=%x",
        this, avail, mUpstreamState));

  mFlatHttpRequestHeaders.Append(buf, avail);

  // We can use the simple double crlf because firefox is the
  // only client we are parsing
  PRInt32 endHeader = mFlatHttpRequestHeaders.Find("\r\n\r\n");
  
  if (endHeader == -1) {
    // We don't have all the headers yet
    LOG3(("SpdyStream::ParseHttpRequestHeaders %p "
          "Need more header bytes. Len = %d",
          this, mFlatHttpRequestHeaders.Length()));
    *countUsed = avail;
    return NS_OK;
  }
           
  // We have recvd all the headers, trim the local
  // buffer of the final empty line, and set countUsed to reflect
  // the whole header has been consumed.
  PRUint32 oldLen = mFlatHttpRequestHeaders.Length();
  mFlatHttpRequestHeaders.SetLength(endHeader + 2);
  *countUsed = avail - (oldLen - endHeader) + 4;
  mSynFrameComplete = 1;

  // It is now OK to assign a streamID that we are assured will
  // be monotonically increasing amongst syn-streams on this
  // session
  mStreamID = mSession->RegisterStreamID(this);
  NS_ABORT_IF_FALSE(mStreamID & 1,
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

  mTxInlineFrame[0] = SpdySession::kFlag_Control;
  mTxInlineFrame[1] = 2;                          /* version */
  mTxInlineFrame[2] = 0;
  mTxInlineFrame[3] = SpdySession::CONTROL_TYPE_SYN_STREAM;
  // 4 to 7 are length and flags, we'll fill that in later
  
  PRUint32 networkOrderID = PR_htonl(mStreamID);
  memcpy(mTxInlineFrame + 8, &networkOrderID, 4);
  
  // this is the associated-to field, which is not used sending
  // from the client in the http binding
  memset (mTxInlineFrame + 12, 0, 4);

  // Priority flags are the C0 mask of byte 16.
  // From low to high: 00 40 80 C0
  // higher raw priority values are actually less important
  //
  // The other 6 bits of 16 are unused. Spdy/3 will expand
  // priority to 4 bits.
  //
  // When Spdy/3 implements WINDOW_UPDATE the lowest priority
  // streams over a threshold (32?) should be given tiny
  // receive windows, separate from their spdy priority
  //
  if (mPriority >= nsISupportsPriority::PRIORITY_LOW)
    mTxInlineFrame[16] = SpdySession::kPri00;
  else if (mPriority >= nsISupportsPriority::PRIORITY_NORMAL)
    mTxInlineFrame[16] = SpdySession::kPri01;
  else if (mPriority >= nsISupportsPriority::PRIORITY_HIGH)
    mTxInlineFrame[16] = SpdySession::kPri02;
  else
    mTxInlineFrame[16] = SpdySession::kPri03;

  mTxInlineFrame[17] = 0;                         /* unused */
  
//  nsCString methodHeader;
//  mTransaction->RequestHead()->Method()->ToUTF8String(methodHeader);
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

  PRInt32 crlfIndex = mFlatHttpRequestHeaders.Find("\r\n");
  while (true) {
    PRInt32 startIndex = crlfIndex + 2;

    crlfIndex = mFlatHttpRequestHeaders.Find("\r\n", false, startIndex);
    if (crlfIndex == -1)
      break;
    
    PRInt32 colonIndex = mFlatHttpRequestHeaders.Find(":", false, startIndex,
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
        name.Equals("proxy-connection") ||
        name.Equals("url"))
      continue;
    
    nsCString *val = hdrHash.Get(name);
    if (!val) {
      val = new nsCString();
      hdrHash.Put(name, val);
    }

    PRInt32 valueIndex = colonIndex + 1;
    while (valueIndex < crlfIndex && beginBuffer[valueIndex] == ' ')
      ++valueIndex;
    
    nsDependentCSubstring v = Substring(beginBuffer + valueIndex,
                                        beginBuffer + crlfIndex);
    if (!val->IsEmpty())
      val->Append(static_cast<char>(0));
    val->Append(v);

    if (name.Equals("content-length")) {
      PRInt64 len;
      if (nsHttp::ParseInt64(val->get(), nsnull, &len))
        mRequestBodyLen = len;
    }
  }
  
  mTxInlineFrameSize = 18;

  LOG3(("http request headers to encode are: \n%s",
        mFlatHttpRequestHeaders.get()));

  // The header block length
  PRUint16 count = hdrHash.Count() + 4; /* method, scheme, url, version */
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
  (reinterpret_cast<PRUint32 *>(mTxInlineFrame.get()))[1] =
    PR_htonl(mTxInlineFrameSize - 8);

  NS_ABORT_IF_FALSE(!mTxInlineFrame[4],
                    "Size greater than 24 bits");
  
  // For methods other than POST and PUT, we will set the fin bit
  // right on the syn stream packet.

  if (mTransaction->RequestHead()->Method() != nsHttp::Post &&
      mTransaction->RequestHead()->Method() != nsHttp::Put) {
    mSentFinOnData = 1;
    mTxInlineFrame[4] = SpdySession::kFlag_Data_FIN;
  }

  Telemetry::Accumulate(Telemetry::SPDY_SYN_SIZE, mTxInlineFrameSize - 18);

  // The size of the input headers is approximate
  PRUint32 ratio =
    (mTxInlineFrameSize - 18) * 100 /
    (11 + mTransaction->RequestHead()->RequestURI().Length() +
     mFlatHttpRequestHeaders.Length());
  
  Telemetry::Accumulate(Telemetry::SPDY_SYN_RATIO, ratio);
  return NS_OK;
}

nsresult
SpdyStream::TransmitFrame(const char *buf,
                          PRUint32 *countUsed)
{
  NS_ABORT_IF_FALSE(mTxInlineFrameSize, "empty stream frame in transmit");
  NS_ABORT_IF_FALSE(mSegmentReader, "TransmitFrame with null mSegmentReader");
  
  PRUint32 transmittedCount;
  nsresult rv;
  
  LOG3(("SpdyStream::TransmitFrame %p inline=%d of %d stream=%d of %d",
        this, mTxInlineFrameSent, mTxInlineFrameSize,
        mTxStreamFrameSent, mTxStreamFrameSize));
  if (countUsed)
    *countUsed = 0;
  mBlockedOnWrite = 0;

  // In the (relatively common) event that we have a small amount of data
  // split between the inlineframe and the streamframe, then move the stream
  // data into the inlineframe via copy in order to coalesce into one write.
  // Given the interaction with ssl this is worth the small copy cost.
  if (mTxStreamFrameSize && mTxInlineFrameSize &&
      !mTxInlineFrameSent && !mTxStreamFrameSent &&
      mTxStreamFrameSize < SpdySession::kDefaultBufferSize &&
      mTxInlineFrameSize + mTxStreamFrameSize < mTxInlineFrameAllocation) {
    LOG3(("Coalesce Transmit"));
    memcpy (mTxInlineFrame + mTxInlineFrameSize,
            buf, mTxStreamFrameSize);
    mTxInlineFrameSize += mTxStreamFrameSize;
    mTxStreamFrameSent = 0;
    mTxStreamFrameSize = 0;
  }

  // This function calls mSegmentReader->OnReadSegment to report the actual SPDY
  // bytes through to the SpdySession and then the HttpConnection which calls
  // the socket write function.
  
  while (mTxInlineFrameSent < mTxInlineFrameSize) {
    rv = mSegmentReader->OnReadSegment(mTxInlineFrame + mTxInlineFrameSent,
                                       mTxInlineFrameSize - mTxInlineFrameSent,
                                       &transmittedCount);
    LOG3(("SpdyStream::TransmitFrame for inline session=%p "
          "stream=%p result %x len=%d",
          mSession, this, rv, transmittedCount));
    SpdySession::LogIO(mSession, this, "Writing from Inline Buffer",
                       mTxInlineFrame + mTxInlineFrameSent,
                       transmittedCount);

    if (rv == NS_BASE_STREAM_WOULD_BLOCK)
      mBlockedOnWrite = 1;

    if (NS_FAILED(rv))     // this will include WOULD_BLOCK
      return rv;
    
    mTxInlineFrameSent += transmittedCount;
  }

  PRUint32 offset = 0;
  NS_ABORT_IF_FALSE(mTxStreamFrameSize >= mTxStreamFrameSent,
                    "negative unsent");
  PRUint32 avail =  mTxStreamFrameSize - mTxStreamFrameSent;

  while (avail) {
    NS_ABORT_IF_FALSE(countUsed, "null countused pointer in a stream context");
    rv = mSegmentReader->OnReadSegment(buf + offset, avail, &transmittedCount);

    LOG3(("SpdyStream::TransmitFrame for regular session=%p "
          "stream=%p result %x len=%d",
          mSession, this, rv, transmittedCount));
    SpdySession::LogIO(mSession, this, "Writing from Transaction Buffer",
                       buf + offset, transmittedCount);

    if (rv == NS_BASE_STREAM_WOULD_BLOCK)
      mBlockedOnWrite = 1;

    if (NS_FAILED(rv))     // this will include WOULD_BLOCK
      return rv;
    
    if (mUpstreamState == SENDING_REQUEST_BODY) {
      mTransaction->OnTransportStatus(mSocketTransport,
                                      nsISocketTransport::STATUS_SENDING_TO,
                                      transmittedCount);
    }
    
    *countUsed += transmittedCount;
    avail -= transmittedCount;
    offset += transmittedCount;
    mTxStreamFrameSent += transmittedCount;
  }

  if (!avail) {
    mTxInlineFrameSent = 0;
    mTxInlineFrameSize = 0;
    mTxStreamFrameSent = 0;
    mTxStreamFrameSize = 0;
  }
    
  return NS_OK;
}

void
SpdyStream::ChangeState(enum stateType newState)
{
  LOG3(("SpdyStream::ChangeState() %p from %X to %X",
        this, mUpstreamState, newState));
  mUpstreamState = newState;
  return;
}

void
SpdyStream::GenerateDataFrameHeader(PRUint32 dataLength, bool lastFrame)
{
  LOG3(("SpdyStream::GenerateDataFrameHeader %p len=%d last=%d",
        this, dataLength, lastFrame));

  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");
  NS_ABORT_IF_FALSE(!mTxInlineFrameSize, "inline frame not empty");
  NS_ABORT_IF_FALSE(!mTxInlineFrameSent, "inline partial send not 0");
  NS_ABORT_IF_FALSE(!mTxStreamFrameSize, "stream frame not empty");
  NS_ABORT_IF_FALSE(!mTxStreamFrameSent, "stream partial send not 0");
  NS_ABORT_IF_FALSE(!(dataLength & 0xff000000), "datalength > 24 bits");
  
  (reinterpret_cast<PRUint32 *>(mTxInlineFrame.get()))[0] = PR_htonl(mStreamID);
  (reinterpret_cast<PRUint32 *>(mTxInlineFrame.get()))[1] =
    PR_htonl(dataLength);
  
  NS_ABORT_IF_FALSE(!(mTxInlineFrame[0] & 0x80),
                    "control bit set unexpectedly");
  NS_ABORT_IF_FALSE(!mTxInlineFrame[4], "flag bits set unexpectedly");
  
  mTxInlineFrameSize = 8;
  mTxStreamFrameSize = dataLength;

  if (lastFrame) {
    mTxInlineFrame[4] |= SpdySession::kFlag_Data_FIN;
    if (dataLength)
      mSentFinOnData = 1;
  }
}

void
SpdyStream::CompressToFrame(const nsACString &str)
{
  CompressToFrame(str.BeginReading(), str.Length());
}

void
SpdyStream::CompressToFrame(const nsACString *str)
{
  CompressToFrame(str->BeginReading(), str->Length());
}

// Dictionary taken from
// http://dev.chromium.org/spdy/spdy-protocol/spdy-protocol-draft2 
// Name/Value Header Block Format
// spec indicates that the compression dictionary is not null terminated
// but in reality it is. see:
// https://groups.google.com/forum/#!topic/spdy-dev/2pWxxOZEIcs

const char *SpdyStream::kDictionary =
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
SpdyStream::zlib_allocator(void *opaque, uInt items, uInt size)
{
  return moz_xmalloc(items * size);
}

// use for zlib data types
void
SpdyStream::zlib_destructor(void *opaque, void *addr)
{
  moz_free(addr);
}

void
SpdyStream::ExecuteCompress(PRUint32 flushMode)
{
  // Expect mZlib->avail_in and mZlib->next_in to be set.
  // Append the compressed version of next_in to mTxInlineFrame

  do
  {
    PRUint32 avail = mTxInlineFrameAllocation - mTxInlineFrameSize;
    if (avail < 1) {
      SpdySession::EnsureBuffer(mTxInlineFrame,
                                mTxInlineFrameAllocation + 2000,
                                mTxInlineFrameSize,
                                mTxInlineFrameAllocation);
      avail = mTxInlineFrameAllocation - mTxInlineFrameSize;
    }

    mZlib->next_out = reinterpret_cast<unsigned char *> (mTxInlineFrame.get()) +
      mTxInlineFrameSize;
    mZlib->avail_out = avail;
    deflate(mZlib, flushMode);
    mTxInlineFrameSize += avail - mZlib->avail_out;
  } while (mZlib->avail_in > 0 || !mZlib->avail_out);
}

void
SpdyStream::CompressToFrame(PRUint16 data)
{
  // convert the data to network byte order and write that
  // to the compressed stream
  
  data = PR_htons(data);

  mZlib->next_in = reinterpret_cast<unsigned char *> (&data);
  mZlib->avail_in = 2;
  ExecuteCompress(Z_NO_FLUSH);
}


void
SpdyStream::CompressToFrame(const char *data, PRUint32 len)
{
  // Format calls for a network ordered 16 bit length
  // followed by the utf8 string

  // for now, silently truncate headers greater than 64KB. Spdy/3 will
  // fix this by making the len a 32 bit quantity
  if (len > 0xffff)
    len = 0xffff;

  PRUint16 networkLen = len;
  networkLen = PR_htons(len);
  
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
SpdyStream::CompressFlushFrame()
{
  mZlib->next_in = (unsigned char *) "";
  mZlib->avail_in = 0;
  ExecuteCompress(Z_SYNC_FLUSH);
}

void
SpdyStream::Close(nsresult reason)
{
  mTransaction->Close(reason);
}

//-----------------------------------------------------------------------------
// nsAHttpSegmentReader
//-----------------------------------------------------------------------------

nsresult
SpdyStream::OnReadSegment(const char *buf,
                          PRUint32 count,
                          PRUint32 *countRead)
{
  LOG3(("SpdyStream::OnReadSegment %p count=%d state=%x",
        this, count, mUpstreamState));

  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");
  NS_ABORT_IF_FALSE(mSegmentReader, "OnReadSegment with null mSegmentReader");
  
  nsresult rv = NS_ERROR_UNEXPECTED;
  PRUint32 dataLength;

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
    LOG3(("ParseHttpRequestHeaders %p used %d of %d.",
          this, *countRead, count));
    if (mSynFrameComplete) {
      NS_ABORT_IF_FALSE(mTxInlineFrameSize,
                        "OnReadSegment SynFrameComplete 0b");
      rv = TransmitFrame(nsnull, nsnull);
      if (rv == NS_BASE_STREAM_WOULD_BLOCK && *countRead)
        rv = NS_OK;
      if (mTxInlineFrameSize)
        ChangeState(SENDING_SYN_STREAM);
      else
        ChangeState(GENERATING_REQUEST_BODY);
      break;
    }
    NS_ABORT_IF_FALSE(*countRead == count,
                      "Header parsing not complete but unused data");
    break;

  case GENERATING_REQUEST_BODY:
    NS_ABORT_IF_FALSE(!mTxInlineFrameSent,
                      "OnReadSegment in generating_request_body with "
                      "frame in progress");
    if (count < mChunkSize && count < mRequestBodyLen) {
      LOG3(("SpdyStream %p id %x has %d to write out of a bodylen %d"
            " with a chunk size of %d. Waiting for more.",
            this, mStreamID, count, mChunkSize, mRequestBodyLen));
      rv = NS_BASE_STREAM_WOULD_BLOCK;
      break;
    }
    
    dataLength = NS_MIN(count, mChunkSize);
    if (dataLength > mRequestBodyLen)
      return NS_ERROR_UNEXPECTED;
    mRequestBodyLen -= dataLength;
    GenerateDataFrameHeader(dataLength, !mRequestBodyLen);
    ChangeState(SENDING_REQUEST_BODY);
    // NO BREAK

  case SENDING_REQUEST_BODY:
    NS_ABORT_IF_FALSE(mTxInlineFrameSize, "OnReadSegment Send Data Header 0b");
    rv = TransmitFrame(buf, countRead);
    LOG3(("TransmitFrame() rv=%x returning %d data bytes. "
          "Header is %d/%d Body is %d/%d.",
          rv, *countRead,
          mTxInlineFrameSent, mTxInlineFrameSize,
          mTxStreamFrameSent, mTxStreamFrameSize));

    if (rv == NS_BASE_STREAM_WOULD_BLOCK && *countRead)
      rv = NS_OK;

    // If that frame was all sent, look for another one
    if (!mTxInlineFrameSize)
        ChangeState(GENERATING_REQUEST_BODY);
    break;

  case SENDING_SYN_STREAM:
    rv = NS_BASE_STREAM_WOULD_BLOCK;
    break;

  case SENDING_FIN_STREAM:
    NS_ABORT_IF_FALSE(false,
                      "resuming partial fin stream out of OnReadSegment");
    break;
    
  default:
    NS_ABORT_IF_FALSE(false, "SpdyStream::OnReadSegment non-write state");
    break;
  }
  
  return rv;
}

//-----------------------------------------------------------------------------
// nsAHttpSegmentWriter
//-----------------------------------------------------------------------------

nsresult
SpdyStream::OnWriteSegment(char *buf,
                           PRUint32 count,
                           PRUint32 *countWritten)
{
  LOG3(("SpdyStream::OnWriteSegment %p count=%d state=%x",
        this, count, mUpstreamState));

  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");
  NS_ABORT_IF_FALSE(mSegmentWriter, "OnWriteSegment with null mSegmentWriter");

  return mSegmentWriter->OnWriteSegment(buf, count, countWritten);
}

} // namespace mozilla::net
} // namespace mozilla

