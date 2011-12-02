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

#ifndef mozilla_net_SpdyStream_h
#define mozilla_net_SpdyStream_h

#include "nsAHttpTransaction.h"

namespace mozilla { namespace net {

class SpdyStream : public nsAHttpSegmentReader
                 , public nsAHttpSegmentWriter
{
public:
  NS_DECL_NSAHTTPSEGMENTREADER
  NS_DECL_NSAHTTPSEGMENTWRITER

  SpdyStream(nsAHttpTransaction *,
             SpdySession *, nsISocketTransport *,
             PRUint32, z_stream *, PRInt32);
  ~SpdyStream();

  PRUint32 StreamID() { return mStreamID; }

  nsresult ReadSegments(nsAHttpSegmentReader *,  PRUint32, PRUint32 *);
  nsresult WriteSegments(nsAHttpSegmentWriter *, PRUint32, PRUint32 *);

  bool BlockedOnWrite()
  {
    return static_cast<bool>(mBlockedOnWrite);
  }

  bool RequestBlockedOnRead()
  {
    return static_cast<bool>(mRequestBlockedOnRead);
  }

  // returns false if called more than once
  bool SetFullyOpen()
  {
    bool result = !mFullyOpen;
    mFullyOpen = 1;
    return result;
  }

  nsAHttpTransaction *Transaction()
  {
    return mTransaction;
  }

  void Close(nsresult reason);

  void SetRecvdFin(bool aStatus) { mRecvdFin = aStatus ? 1 : 0; }
  bool RecvdFin() { return mRecvdFin; }

  // The zlib header compression dictionary defined by SPDY,
  // and hooks to the mozilla allocator for zlib to use.
  static const char *kDictionary;
  static void *zlib_allocator(void *, uInt, uInt);
  static void zlib_destructor(void *, void *);

private:

  enum stateType {
    GENERATING_SYN_STREAM,
    SENDING_SYN_STREAM,
    GENERATING_REQUEST_BODY,
    SENDING_REQUEST_BODY,
    SENDING_FIN_STREAM,
    UPSTREAM_COMPLETE
  };

  static PLDHashOperator hdrHashEnumerate(const nsACString &,
                                          nsAutoPtr<nsCString> &,
                                          void *);

  void     ChangeState(enum stateType );
  nsresult ParseHttpRequestHeaders(const char *, PRUint32, PRUint32 *);
  nsresult TransmitFrame(const char *, PRUint32 *);
  void     GenerateDataFrameHeader(PRUint32, bool);

  void     CompressToFrame(const nsACString &);
  void     CompressToFrame(const nsACString *);
  void     CompressToFrame(const char *, PRUint32);
  void     CompressToFrame(PRUint16);
  void     CompressFlushFrame();
  void     ExecuteCompress(PRUint32);
  
  // Each stream goes from syn_stream to upstream_complete, perhaps
  // looping on multiple instances of generating_request_body and
  // sending_request_body for each SPDY chunk in the upload.
  enum stateType mUpstreamState;

  // The underlying HTTP transaction
  nsRefPtr<nsAHttpTransaction> mTransaction;

  // The session that this stream is a subset of
  SpdySession                *mSession;

  // The underlying socket transport object is needed to propogate some events
  nsISocketTransport         *mSocketTransport;

  // These are temporary state variables to hold the argument to
  // Read/WriteSegments so it can be accessed by On(read/write)segment
  // further up the stack.
  nsAHttpSegmentReader        *mSegmentReader;
  nsAHttpSegmentWriter        *mSegmentWriter;

  // The 24 bit SPDY stream ID
  PRUint32                    mStreamID;

  // The quanta upstream data frames are chopped into
  PRUint32                    mChunkSize;

  // Flag is set when all http request headers have been read
  PRUint32                     mSynFrameComplete     : 1;

  // Flag is set when there is more request data to send and the
  // stream needs to be rescheduled for writing. Sometimes this
  // is done as a matter of fairness, not really due to blocking
  PRUint32                     mBlockedOnWrite       : 1;

  // Flag is set when the HTTP processor has more data to send
  // but has blocked in doing so.
  PRUint32                     mRequestBlockedOnRead : 1;

  // Flag is set when a FIN has been placed on a data or syn packet
  // (i.e after the client has closed)
  PRUint32                     mSentFinOnData        : 1;

  // Flag is set after the response frame bearing the fin bit has
  // been processed. (i.e. after the server has closed).
  PRUint32                     mRecvdFin             : 1;

  // Flag is set after syn reply received
  PRUint32                     mFullyOpen            : 1;

  // The InlineFrame and associated data is used for composing control
  // frames and data frame headers.
  nsAutoArrayPtr<char>         mTxInlineFrame;
  PRUint32                     mTxInlineFrameAllocation;
  PRUint32                     mTxInlineFrameSize;
  PRUint32                     mTxInlineFrameSent;

  // mTxStreamFrameSize and mTxStreamFrameSent track the progress of
  // transmitting a request body data frame. The data frame itself
  // is never copied into the spdy layer.
  PRUint32                     mTxStreamFrameSize;
  PRUint32                     mTxStreamFrameSent;

  // Compression context and buffer for request header compression.
  // This is a copy of SpdySession::mUpstreamZlib because it needs
  //  to remain the same in all streams of a session.
  z_stream                     *mZlib;
  nsCString                    mFlatHttpRequestHeaders;

  // Track the content-length of a request body so that we can
  // place the fin flag on the last data packet instead of waiting
  // for a stream closed indication. Relying on stream close results
  // in an extra 0-length runt packet and seems to have some interop
  // problems with the google servers.
  PRInt64                      mRequestBodyLen;

  // based on nsISupportsPriority definitions
  PRInt32                      mPriority;

};

}} // namespace mozilla::net

#endif // mozilla_net_SpdyStream_h
