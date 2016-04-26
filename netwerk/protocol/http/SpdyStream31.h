/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_SpdyStream31_h
#define mozilla_net_SpdyStream31_h

#include "mozilla/Attributes.h"
#include "mozilla/UniquePtr.h"
#include "nsAHttpTransaction.h"

namespace mozilla { namespace net {

class SpdyStream31 : public nsAHttpSegmentReader
                   , public nsAHttpSegmentWriter
{
public:
  NS_DECL_NSAHTTPSEGMENTREADER
  NS_DECL_NSAHTTPSEGMENTWRITER

  SpdyStream31(nsAHttpTransaction *, SpdySession31 *, int32_t);

  uint32_t StreamID() { return mStreamID; }
  SpdyPushedStream31 *PushSource() { return mPushSource; }

  virtual nsresult ReadSegments(nsAHttpSegmentReader *,  uint32_t, uint32_t *);
  virtual nsresult WriteSegments(nsAHttpSegmentWriter *, uint32_t, uint32_t *);
  virtual bool DeferCleanupOnSuccess() { return false; }

  const nsAFlatCString &Origin()   const { return mOrigin; }

  bool RequestBlockedOnRead()
  {
    return static_cast<bool>(mRequestBlockedOnRead);
  }

  bool GetFullyOpen();
  // returns failure if stream cannot be made ready and stream
  // should be canceled
  nsresult SetFullyOpen();

  bool HasRegisteredID() { return mStreamID != 0; }

  nsAHttpTransaction *Transaction() { return mTransaction; }
  virtual nsIRequestContext *RequestContext()
  {
    return mTransaction ? mTransaction->RequestContext() : nullptr;
  }

  void Close(nsresult reason);

  void SetRecvdFin(bool aStatus) { mRecvdFin = aStatus ? 1 : 0; }
  bool RecvdFin() { return mRecvdFin; }

  void SetRecvdData(bool aStatus) { mReceivedData = aStatus ? 1 : 0; }
  bool RecvdData() { return mReceivedData; }

  void SetQueued(bool aStatus) { mQueued = aStatus ? 1 : 0; }
  bool Queued() { return mQueued; }

  void SetCountAsActive(bool aStatus) { mCountAsActive = aStatus ? 1 : 0; }
  bool CountAsActive() { return mCountAsActive; }

  void UpdateTransportSendEvents(uint32_t count);
  void UpdateTransportReadEvents(uint32_t count);

  // The zlib header compression dictionary defined by SPDY.
  static const unsigned char kDictionary[1423];

  nsresult Uncompress(z_stream *, char *, uint32_t);
  nsresult ConvertHeaders(nsACString &);

  void UpdateRemoteWindow(int32_t delta);
  int64_t RemoteWindow() { return mRemoteWindow; }

  void DecrementLocalWindow(uint32_t delta) {
    mLocalWindow -= delta;
    mLocalUnacked += delta;
  }

  void IncrementLocalWindow(uint32_t delta) {
    mLocalWindow += delta;
    mLocalUnacked -= delta;
  }

  uint64_t LocalUnAcked() { return mLocalUnacked; }
  int64_t  LocalWindow()  { return mLocalWindow; }

  bool     BlockedOnRwin() { return mBlockedOnRwin; }
  bool     ChannelPipeFull();

  // A pull stream has an implicit sink, a pushed stream has a sink
  // once it is matched to a pull stream.
  virtual bool HasSink() { return true; }

  virtual ~SpdyStream31();

protected:
  nsresult FindHeader(nsCString, nsDependentCSubstring &);

  static void CreatePushHashKey(const nsCString &scheme,
                                const nsCString &hostHeader,
                                uint64_t serial,
                                const nsCSubstring &pathInfo,
                                nsCString &outOrigin,
                                nsCString &outKey);

  enum stateType {
    GENERATING_SYN_STREAM,
    GENERATING_REQUEST_BODY,
    SENDING_REQUEST_BODY,
    SENDING_FIN_STREAM,
    UPSTREAM_COMPLETE
  };

  uint32_t mStreamID;

  // The session that this stream is a subset of
  SpdySession31 *mSession;

  nsCString     mOrigin;

  // Each stream goes from syn_stream to upstream_complete, perhaps
  // looping on multiple instances of generating_request_body and
  // sending_request_body for each SPDY chunk in the upload.
  enum stateType mUpstreamState;

  // Flag is set when all http request headers have been read
  uint32_t                     mRequestHeadersDone   : 1;

  // Flag is set when stream ID is stable
  uint32_t                     mSynFrameGenerated    : 1;

  // Flag is set when a FIN has been placed on a data or syn packet
  // (i.e after the client has closed)
  uint32_t                     mSentFinOnData        : 1;

  // Flag is set when stream is queued inside the session due to
  // concurrency limits being exceeded
  uint32_t                     mQueued               : 1;

  void     ChangeState(enum stateType);

private:
  friend class nsAutoPtr<SpdyStream31>;

  nsresult ParseHttpRequestHeaders(const char *, uint32_t, uint32_t *);
  nsresult GenerateSynFrame();

  void     AdjustInitialWindow();
  nsresult TransmitFrame(const char *, uint32_t *, bool forceCommitment);
  void     GenerateDataFrameHeader(uint32_t, bool);

  void     CompressToFrame(const nsACString &);
  void     CompressToFrame(const nsACString *);
  void     CompressToFrame(const char *, uint32_t);
  void     CompressToFrame(uint32_t);
  void     CompressFlushFrame();
  void     ExecuteCompress(uint32_t);

  // The underlying HTTP transaction. This pointer is used as the key
  // in the SpdySession31 mStreamTransactionHash so it is important to
  // keep a reference to it as long as this stream is a member of that hash.
  // (i.e. don't change it or release it after it is set in the ctor).
  RefPtr<nsAHttpTransaction> mTransaction;

  // The underlying socket transport object is needed to propogate some events
  nsISocketTransport         *mSocketTransport;

  // These are temporary state variables to hold the argument to
  // Read/WriteSegments so it can be accessed by On(read/write)segment
  // further up the stack.
  nsAHttpSegmentReader        *mSegmentReader;
  nsAHttpSegmentWriter        *mSegmentWriter;

  // The quanta upstream data frames are chopped into
  uint32_t                    mChunkSize;

  // Flag is set when the HTTP processor has more data to send
  // but has blocked in doing so.
  uint32_t                     mRequestBlockedOnRead : 1;

  // Flag is set after the response frame bearing the fin bit has
  // been processed. (i.e. after the server has closed).
  uint32_t                     mRecvdFin             : 1;

  // Flag is set after syn reply received
  uint32_t                     mFullyOpen            : 1;

  // Flag is set after the WAITING_FOR Transport event has been generated
  uint32_t                     mSentWaitingFor       : 1;

  // Flag is set after 1st DATA frame has been passed to stream, after
  // which additional HEADERS data is invalid
  uint32_t                     mReceivedData         : 1;

  // Flag is set after TCP send autotuning has been disabled
  uint32_t                     mSetTCPSocketBuffer   : 1;

  // Flag is set when stream is counted towards MAX_CONCURRENT streams in session
  uint32_t                     mCountAsActive        : 1;

  // The InlineFrame and associated data is used for composing control
  // frames and data frame headers.
  UniquePtr<uint8_t[]>         mTxInlineFrame;
  uint32_t                     mTxInlineFrameSize;
  uint32_t                     mTxInlineFrameUsed;

  // mTxStreamFrameSize tracks the progress of
  // transmitting a request body data frame. The data frame itself
  // is never copied into the spdy layer.
  uint32_t                     mTxStreamFrameSize;

  // Compression context and buffer for request header compression.
  // This is a copy of SpdySession31::mUpstreamZlib because it needs
  //  to remain the same in all streams of a session.
  z_stream                     *mZlib;
  nsCString                    mFlatHttpRequestHeaders;

  // These are used for decompressing downstream spdy response headers
  uint32_t             mDecompressBufferSize;
  uint32_t             mDecompressBufferUsed;
  uint32_t             mDecompressedBytes;
  UniquePtr<char[]>    mDecompressBuffer;

  // Track the content-length of a request body so that we can
  // place the fin flag on the last data packet instead of waiting
  // for a stream closed indication. Relying on stream close results
  // in an extra 0-length runt packet and seems to have some interop
  // problems with the google servers. Connect does rely on stream
  // close by setting this to the max value.
  int64_t                      mRequestBodyLenRemaining;

  // based on nsISupportsPriority definitions
  int32_t                      mPriority;

  // mLocalWindow, mRemoteWindow, and mLocalUnacked are for flow control.
  // *window are signed because the race conditions in asynchronous SETTINGS
  // messages can force them temporarily negative.

  // LocalWindow is how much data the server will send without getting a
  //   window update
  int64_t                      mLocalWindow;

  // RemoteWindow is how much data the client is allowed to send without
  //   getting a window update
  int64_t                      mRemoteWindow;

  // LocalUnacked is the number of bytes received by the client but not
  //   yet reflected in a window update. Sending that update will increment
  //   LocalWindow
  uint64_t                     mLocalUnacked;

  // True when sending is suspended becuase the remote flow control window is
  //   <= 0
  bool                         mBlockedOnRwin;

  // For Progress Events
  uint64_t                     mTotalSent;
  uint64_t                     mTotalRead;

  // For SpdyPush
  SpdyPushedStream31 *mPushSource;

/// connect tunnels
public:
  bool IsTunnel() { return mIsTunnel; }
private:
  void ClearTransactionsBlockedOnTunnel();
  void MapStreamToPlainText();
  void MapStreamToHttpConnection();

  bool mIsTunnel;
  bool mPlainTextTunnel;
};

} // namespace net
} // namespace mozilla

#endif // mozilla_net_SpdyStream31_h
