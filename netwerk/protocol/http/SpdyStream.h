/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

  PRUint32 StreamID() { return mStreamID; }

  nsresult ReadSegments(nsAHttpSegmentReader *,  PRUint32, PRUint32 *);
  nsresult WriteSegments(nsAHttpSegmentWriter *, PRUint32, PRUint32 *);

  bool RequestBlockedOnRead()
  {
    return static_cast<bool>(mRequestBlockedOnRead);
  }

  // returns false if called more than once
  bool GetFullyOpen() {return mFullyOpen;}
  void SetFullyOpen() 
  {
    NS_ABORT_IF_FALSE(!mFullyOpen, "SetFullyOpen already open");
    mFullyOpen = 1;
  }

  nsAHttpTransaction *Transaction()
  {
    return mTransaction;
  }

  void Close(nsresult reason);

  void SetRecvdFin(bool aStatus) { mRecvdFin = aStatus ? 1 : 0; }
  bool RecvdFin() { return mRecvdFin; }

  void UpdateTransportSendEvents(PRUint32 count);
  void UpdateTransportReadEvents(PRUint32 count);

  // The zlib header compression dictionary defined by SPDY,
  // and hooks to the mozilla allocator for zlib to use.
  static const char *kDictionary;
  static void *zlib_allocator(void *, uInt, uInt);
  static void zlib_destructor(void *, void *);

private:

  // a SpdyStream object is only destroyed by being removed from the
  // SpdySession mStreamTransactionHash - make the dtor private to
  // just the AutoPtr implementation needed for that hash.
  friend class nsAutoPtr<SpdyStream>;
  ~SpdyStream();

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

  void     ChangeState(enum stateType);
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

  // The underlying HTTP transaction. This pointer is used as the key
  // in the SpdySession mStreamTransactionHash so it is important to
  // keep a reference to it as long as this stream is a member of that hash.
  // (i.e. don't change it or release it after it is set in the ctor).
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

  // Flag is set after the WAITING_FOR Transport event has been generated
  PRUint32                     mSentWaitingFor       : 1;

  // The InlineFrame and associated data is used for composing control
  // frames and data frame headers.
  nsAutoArrayPtr<char>         mTxInlineFrame;
  PRUint32                     mTxInlineFrameSize;
  PRUint32                     mTxInlineFrameUsed;

  // mTxStreamFrameSize tracks the progress of
  // transmitting a request body data frame. The data frame itself
  // is never copied into the spdy layer.
  PRUint32                     mTxStreamFrameSize;

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
  PRInt64                      mRequestBodyLenRemaining;

  // based on nsISupportsPriority definitions
  PRInt32                      mPriority;

  // For Progress Events
  PRUint64                     mTotalSent;
  PRUint64                     mTotalRead;
};

}} // namespace mozilla::net

#endif // mozilla_net_SpdyStream_h
