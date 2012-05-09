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
    mRequestBlockedOnRead(0),
    mSentFinOnData(0),
    mRecvdFin(0),
    mFullyOpen(0),
    mSentWaitingFor(0),
    mTxInlineFrameSize(SpdySession::kDefaultBufferSize),
    mTxInlineFrameUsed(0),
    mTxStreamFrameSize(0),
    mZlib(compressionContext),
    mRequestBodyLenRemaining(0),
    mPriority(priority),
    mTotalSent(0),
    mTotalRead(0)
{
  NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");

  LOG3(("SpdyStream::SpdyStream %p", this));

  mTxInlineFrame = new char[mTxInlineFrameSize];
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

  case SENDING_SYN_STREAM:
    // We were trying to send the SYN-STREAM but were blocked from trying
    // to transmit it the first time(s).
    mSegmentReader = reader;
    rv = TransmitFrame(nsnull, nsnull);
    mSegmentReader = nsnull;
    *countRead = 0;
    if (NS_SUCCEEDED(rv)) {
      NS_ABORT_IF_FALSE(!mTxInlineFrameUsed,
                        "Transmit Frame should be all or nothing");
    
      if (mSentFinOnData) {
        ChangeState(UPSTREAM_COMPLETE);
        rv = NS_OK;
      }
      else {
        rv = NS_BASE_STREAM_WOULD_BLOCK;
        ChangeState(GENERATING_REQUEST_BODY);
        mSession->TransactionHasDataToWrite(this);
      }
    }
    break;

  case SENDING_FIN_STREAM:
    // We were trying to send the FIN-STREAM but were blocked from
    // sending it out - try again.
    if (!mSentFinOnData) {
      mSegmentReader = reader;
      rv = TransmitFrame(nsnull, nsnull);
      mSegmentReader = nsnull;
      NS_ABORT_IF_FALSE(NS_FAILED(rv) || !mTxInlineFrameUsed,
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
  
  if (endHeader == kNotFound) {
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
  //
  // The other 6 bits of 16 are unused. Spdy/3 will expand
  // priority to 4 bits.
  //
  // When Spdy/3 implements WINDOW_UPDATE the lowest priority
  // streams over a threshold (32?) should be given tiny
  // receive windows, separate from their spdy priority
  //
  if (mPriority >= nsISupportsPriority::PRIORITY_LOW)
    mTxInlineFrame[16] = SpdySession::kPri03;
  else if (mPriority >= nsISupportsPriority::PRIORITY_NORMAL)
    mTxInlineFrame[16] = SpdySession::kPri02;
  else if (mPriority >= nsISupportsPriority::PRIORITY_HIGH)
    mTxInlineFrame[16] = SpdySession::kPri01;
  else
    mTxInlineFrame[16] = SpdySession::kPri00;

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
        mRequestBodyLenRemaining = len;
    }
  }
  
  mTxInlineFrameUsed = 18;

  // Do not naively log the request headers here beacuse they might
  // contain auth. The http transaction already logs the sanitized request
  // headers at this same level so it is not necessary to do so here.

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
    PR_htonl(mTxInlineFrameUsed - 8);

  NS_ABORT_IF_FALSE(!mTxInlineFrame[4],
                    "Size greater than 24 bits");
  
  // Determine whether to put the fin bit on the syn stream frame or whether
  // to wait for a data packet to put it on.

  if (mTransaction->RequestHead()->Method() == nsHttp::Get ||
      mTransaction->RequestHead()->Method() == nsHttp::Connect ||
      mTransaction->RequestHead()->Method() == nsHttp::Head) {
    // for GET, CONNECT, and HEAD place the fin bit right on the
    // syn stream packet

    mSentFinOnData = 1;
    mTxInlineFrame[4] = SpdySession::kFlag_Data_FIN;
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
    mTxInlineFrame[4] = SpdySession::kFlag_Data_FIN;
  }
  
  Telemetry::Accumulate(Telemetry::SPDY_SYN_SIZE, mTxInlineFrameUsed - 18);

  // The size of the input headers is approximate
  PRUint32 ratio =
    (mTxInlineFrameUsed - 18) * 100 /
    (11 + mTransaction->RequestHead()->RequestURI().Length() +
     mFlatHttpRequestHeaders.Length());
  
  Telemetry::Accumulate(Telemetry::SPDY_SYN_RATIO, ratio);
  return NS_OK;
}

void
SpdyStream::UpdateTransportReadEvents(PRUint32 count)
{
  mTotalRead += count;

  mTransaction->OnTransportStatus(mSocketTransport,
                                  NS_NET_STATUS_RECEIVING_FROM,
                                  mTotalRead);
}

void
SpdyStream::UpdateTransportSendEvents(PRUint32 count)
{
  mTotalSent += count;

  if (mUpstreamState != SENDING_FIN_STREAM)
    mTransaction->OnTransportStatus(mSocketTransport,
                                    NS_NET_STATUS_SENDING_TO,
                                    mTotalSent);

  if (!mSentWaitingFor && !mRequestBodyLenRemaining) {
    mSentWaitingFor = 1;
    mTransaction->OnTransportStatus(mSocketTransport,
                                    NS_NET_STATUS_WAITING_FOR,
                                    LL_ZERO);
  }
}

nsresult
SpdyStream::TransmitFrame(const char *buf,
                          PRUint32 *countUsed)
{
  // If TransmitFrame returns SUCCESS than all the data is sent (or at least
  // buffered at the session level), if it returns WOULD_BLOCK then none of
  // the data is sent.

  // You can call this function with no data and no out parameter in order to
  // flush internal buffers that were previously blocked on writing. You can
  // of course feed new data to it as well.

  NS_ABORT_IF_FALSE(mTxInlineFrameUsed, "empty stream frame in transmit");
  NS_ABORT_IF_FALSE(mSegmentReader, "TransmitFrame with null mSegmentReader");
  NS_ABORT_IF_FALSE((buf && countUsed) || (!buf && !countUsed),
                    "TransmitFrame arguments inconsistent");

  PRUint32 transmittedCount;
  nsresult rv;
  
  LOG3(("SpdyStream::TransmitFrame %p inline=%d stream=%d",
        this, mTxInlineFrameUsed, mTxStreamFrameSize));
  if (countUsed)
    *countUsed = 0;

  // In the (relatively common) event that we have a small amount of data
  // split between the inlineframe and the streamframe, then move the stream
  // data into the inlineframe via copy in order to coalesce into one write.
  // Given the interaction with ssl this is worth the small copy cost.
  if (mTxStreamFrameSize && mTxInlineFrameUsed &&
      mTxStreamFrameSize < SpdySession::kDefaultBufferSize &&
      mTxInlineFrameUsed + mTxStreamFrameSize < mTxInlineFrameSize) {
    LOG3(("Coalesce Transmit"));
    memcpy (mTxInlineFrame + mTxInlineFrameUsed,
            buf, mTxStreamFrameSize);
    if (countUsed)
      *countUsed += mTxStreamFrameSize;
    mTxInlineFrameUsed += mTxStreamFrameSize;
    mTxStreamFrameSize = 0;
  }

  rv = mSegmentReader->CommitToSegmentSize(mTxStreamFrameSize +
                                           mTxInlineFrameUsed);
  if (rv == NS_BASE_STREAM_WOULD_BLOCK)
    mSession->TransactionHasDataToWrite(this);
  if (NS_FAILED(rv))     // this will include WOULD_BLOCK
    return rv;

  // This function calls mSegmentReader->OnReadSegment to report the actual SPDY
  // bytes through to the SpdySession and then the HttpConnection which calls
  // the socket write function. It will accept all of the inline and stream
  // data because of the above 'commitment' even if it has to buffer
  
  rv = mSegmentReader->OnReadSegment(mTxInlineFrame, mTxInlineFrameUsed,
                                     &transmittedCount);
  LOG3(("SpdyStream::TransmitFrame for inline session=%p "
        "stream=%p result %x len=%d",
        mSession, this, rv, transmittedCount));

  NS_ABORT_IF_FALSE(rv != NS_BASE_STREAM_WOULD_BLOCK,
                    "inconsistent inline commitment result");

  if (NS_FAILED(rv))
    return rv;

  NS_ABORT_IF_FALSE(transmittedCount == mTxInlineFrameUsed,
                    "inconsistent inline commitment count");
    
  SpdySession::LogIO(mSession, this, "Writing from Inline Buffer",
                     mTxInlineFrame, transmittedCount);

  if (mTxStreamFrameSize) {
    if (!buf) {
      // this cannot happen
      NS_ABORT_IF_FALSE(false, "Stream transmit with null buf argument to "
                        "TransmitFrame()");
      LOG(("Stream transmit with null buf argument to TransmitFrame()\n"));
      return NS_ERROR_UNEXPECTED;
    }

    rv = mSegmentReader->OnReadSegment(buf, mTxStreamFrameSize,
                                       &transmittedCount);

    LOG3(("SpdyStream::TransmitFrame for regular session=%p "
          "stream=%p result %x len=%d",
          mSession, this, rv, transmittedCount));
  
    NS_ABORT_IF_FALSE(rv != NS_BASE_STREAM_WOULD_BLOCK,
                      "inconsistent stream commitment result");

    if (NS_FAILED(rv))
      return rv;

    NS_ABORT_IF_FALSE(transmittedCount == mTxStreamFrameSize,
                      "inconsistent stream commitment count");
    
    SpdySession::LogIO(mSession, this, "Writing from Transaction Buffer",
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
  NS_ABORT_IF_FALSE(!mTxInlineFrameUsed, "inline frame not empty");
  NS_ABORT_IF_FALSE(!mTxStreamFrameSize, "stream frame not empty");
  NS_ABORT_IF_FALSE(!(dataLength & 0xff000000), "datalength > 24 bits");
  
  (reinterpret_cast<PRUint32 *>(mTxInlineFrame.get()))[0] = PR_htonl(mStreamID);
  (reinterpret_cast<PRUint32 *>(mTxInlineFrame.get()))[1] =
    PR_htonl(dataLength);
  
  NS_ABORT_IF_FALSE(!(mTxInlineFrame[0] & 0x80),
                    "control bit set unexpectedly");
  NS_ABORT_IF_FALSE(!mTxInlineFrame[4], "flag bits set unexpectedly");
  
  mTxInlineFrameUsed = 8;
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
    PRUint32 avail = mTxInlineFrameSize - mTxInlineFrameUsed;
    if (avail < 1) {
      SpdySession::EnsureBuffer(mTxInlineFrame,
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

  PRUint16 networkLen = PR_htons(len);
  
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
    LOG3(("ParseHttpRequestHeaders %p used %d of %d. complete = %d",
          this, *countRead, count, mSynFrameComplete));
    if (mSynFrameComplete) {
      NS_ABORT_IF_FALSE(mTxInlineFrameUsed,
                        "OnReadSegment SynFrameComplete 0b");
      rv = TransmitFrame(nsnull, nsnull);
      NS_ABORT_IF_FALSE(NS_FAILED(rv) || !mTxInlineFrameUsed,
                        "Transmit Frame should be all or nothing");

      // normalize a blocked write into an ok one if we have consumed the data
      // while parsing headers as some code will take WOULD_BLOCK to mean an
      // error with nothing processed.
      // (e.g. nsHttpTransaction::ReadRequestSegment())
      if (rv == NS_BASE_STREAM_WOULD_BLOCK && *countRead)
        rv = NS_OK;

      // mTxInlineFrameUsed > 0 means the current frame is in progress
      // of sending.  mTxInlineFrameUsed is dropped to 0 after both the frame
      // and its payload (if any) are completely sent out.  Here during
      // GENERATING_SYN_STREAM state we are sending just the http headers.
      // Only when the frame is completely sent out do we proceed to
      // GENERATING_REQUEST_BODY state.

      if (mTxInlineFrameUsed)
        ChangeState(SENDING_SYN_STREAM);
      else
        ChangeState(GENERATING_REQUEST_BODY);
      break;
    }
    NS_ABORT_IF_FALSE(*countRead == count,
                      "Header parsing not complete but unused data");
    break;

  case GENERATING_REQUEST_BODY:
    dataLength = NS_MIN(count, mChunkSize);
    LOG3(("SpdyStream %p id %x request len remaining %d, "
          "count avail %d, chunk used %d",
          this, mStreamID, mRequestBodyLenRemaining, count, dataLength));
    if (dataLength > mRequestBodyLenRemaining)
      return NS_ERROR_UNEXPECTED;
    mRequestBodyLenRemaining -= dataLength;
    GenerateDataFrameHeader(dataLength, !mRequestBodyLenRemaining);
    ChangeState(SENDING_REQUEST_BODY);
    // NO BREAK

  case SENDING_REQUEST_BODY:
    NS_ABORT_IF_FALSE(mTxInlineFrameUsed, "OnReadSegment Send Data Header 0b");
    rv = TransmitFrame(buf, countRead);
    NS_ABORT_IF_FALSE(NS_FAILED(rv) || !mTxInlineFrameUsed,
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

