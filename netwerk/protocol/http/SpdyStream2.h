/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_SpdyStream2_h
#define mozilla_net_SpdyStream2_h

#include "nsAHttpTransaction.h"
#include "mozilla/Attributes.h"

namespace mozilla { namespace net {

class SpdyStream2 MOZ_FINAL : public nsAHttpSegmentReader
                            , public nsAHttpSegmentWriter
{
public:
  NS_DECL_NSAHTTPSEGMENTREADER
  NS_DECL_NSAHTTPSEGMENTWRITER

  SpdyStream2(nsAHttpTransaction *,
             SpdySession2 *, nsISocketTransport *,
             uint32_t, z_stream *, int32_t);

  uint32_t StreamID() { return mStreamID; }

  nsresult ReadSegments(nsAHttpSegmentReader *,  uint32_t, uint32_t *);
  nsresult WriteSegments(nsAHttpSegmentWriter *, uint32_t, uint32_t *);

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

  bool HasRegisteredID() { return mStreamID != 0; }

  nsAHttpTransaction *Transaction()
  {
    return mTransaction;
  }

  void Close(nsresult reason);

  void SetRecvdFin(bool aStatus) { mRecvdFin = aStatus ? 1 : 0; }
  bool RecvdFin() { return mRecvdFin; }

  void UpdateTransportSendEvents(uint32_t count);
  void UpdateTransportReadEvents(uint32_t count);

  // The zlib header compression dictionary defined by SPDY,
  // and hooks to the mozilla allocator for zlib to use.
  static const char *kDictionary;
  static void *zlib_allocator(void *, uInt, uInt);
  static void zlib_destructor(void *, void *);

private:

  // a SpdyStream2 object is only destroyed by being removed from the
  // SpdySession2 mStreamTransactionHash - make the dtor private to
  // just the AutoPtr implementation needed for that hash.
  friend class nsAutoPtr<SpdyStream2>;
  ~SpdyStream2();

  enum stateType {
    GENERATING_SYN_STREAM,
    GENERATING_REQUEST_BODY,
    SENDING_REQUEST_BODY,
    SENDING_FIN_STREAM,
    UPSTREAM_COMPLETE
  };

  static PLDHashOperator hdrHashEnumerate(const nsACString &,
                                          nsAutoPtr<nsCString> &,
                                          void *);

  void     ChangeState(enum stateType);
  nsresult ParseHttpRequestHeaders(const char *, uint32_t, uint32_t *);
  nsresult TransmitFrame(const char *, uint32_t *, bool forceCommitment);
  void     GenerateDataFrameHeader(uint32_t, bool);

  void     CompressToFrame(const nsACString &);
  void     CompressToFrame(const nsACString *);
  void     CompressToFrame(const char *, uint32_t);
  void     CompressToFrame(uint16_t);
  void     CompressFlushFrame();
  void     ExecuteCompress(uint32_t);
  
  // Each stream goes from syn_stream to upstream_complete, perhaps
  // looping on multiple instances of generating_request_body and
  // sending_request_body for each SPDY chunk in the upload.
  enum stateType mUpstreamState;

  // The underlying HTTP transaction. This pointer is used as the key
  // in the SpdySession2 mStreamTransactionHash so it is important to
  // keep a reference to it as long as this stream is a member of that hash.
  // (i.e. don't change it or release it after it is set in the ctor).
  nsRefPtr<nsAHttpTransaction> mTransaction;

  // The session that this stream is a subset of
  SpdySession2                *mSession;

  // The underlying socket transport object is needed to propogate some events
  nsISocketTransport         *mSocketTransport;

  // These are temporary state variables to hold the argument to
  // Read/WriteSegments so it can be accessed by On(read/write)segment
  // further up the stack.
  nsAHttpSegmentReader        *mSegmentReader;
  nsAHttpSegmentWriter        *mSegmentWriter;

  // The 24 bit SPDY stream ID
  uint32_t                    mStreamID;

  // The quanta upstream data frames are chopped into
  uint32_t                    mChunkSize;

  // Flag is set when all http request headers have been read and ID is stable
  uint32_t                     mSynFrameComplete     : 1;

  // Flag is set when the HTTP processor has more data to send
  // but has blocked in doing so.
  uint32_t                     mRequestBlockedOnRead : 1;

  // Flag is set when a FIN has been placed on a data or syn packet
  // (i.e after the client has closed)
  uint32_t                     mSentFinOnData        : 1;

  // Flag is set after the response frame bearing the fin bit has
  // been processed. (i.e. after the server has closed).
  uint32_t                     mRecvdFin             : 1;

  // Flag is set after syn reply received
  uint32_t                     mFullyOpen            : 1;

  // Flag is set after the WAITING_FOR Transport event has been generated
  uint32_t                     mSentWaitingFor       : 1;

  // Flag is set after TCP send autotuning has been disabled
  uint32_t                     mSetTCPSocketBuffer   : 1;

  // The InlineFrame and associated data is used for composing control
  // frames and data frame headers.
  nsAutoArrayPtr<char>         mTxInlineFrame;
  uint32_t                     mTxInlineFrameSize;
  uint32_t                     mTxInlineFrameUsed;

  // mTxStreamFrameSize tracks the progress of
  // transmitting a request body data frame. The data frame itself
  // is never copied into the spdy layer.
  uint32_t                     mTxStreamFrameSize;

  // Compression context and buffer for request header compression.
  // This is a copy of SpdySession2::mUpstreamZlib because it needs
  //  to remain the same in all streams of a session.
  z_stream                     *mZlib;
  nsCString                    mFlatHttpRequestHeaders;

  // Track the content-length of a request body so that we can
  // place the fin flag on the last data packet instead of waiting
  // for a stream closed indication. Relying on stream close results
  // in an extra 0-length runt packet and seems to have some interop
  // problems with the google servers.
  int64_t                      mRequestBodyLenRemaining;

  // based on nsISupportsPriority definitions
  int32_t                      mPriority;

  // For Progress Events
  uint64_t                     mTotalSent;
  uint64_t                     mTotalRead;
};

}} // namespace mozilla::net

#endif // mozilla_net_SpdyStream2_h
