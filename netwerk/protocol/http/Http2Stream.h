/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_Http2Stream_h
#define mozilla_net_Http2Stream_h

#include "mozilla/Attributes.h"
#include "nsAHttpTransaction.h"
#include "nsISupportsPriority.h"

namespace mozilla {
namespace net {

class Http2Session;
class Http2Decompressor;

class Http2Stream
  : public nsAHttpSegmentReader
  , public nsAHttpSegmentWriter
{
public:
  NS_DECL_NSAHTTPSEGMENTREADER
  NS_DECL_NSAHTTPSEGMENTWRITER

  enum stateType {
    IDLE,
    RESERVED_BY_REMOTE,
    OPEN,
    CLOSED_BY_LOCAL,
    CLOSED_BY_REMOTE,
    CLOSED
  };

  const static int32_t kNormalPriority = 0x1000;
  const static int32_t kWorstPriority = kNormalPriority + nsISupportsPriority::PRIORITY_LOWEST;
  const static int32_t kBestPriority = kNormalPriority + nsISupportsPriority::PRIORITY_HIGHEST;

  Http2Stream(nsAHttpTransaction *, Http2Session *, int32_t);

  uint32_t StreamID() { return mStreamID; }
  Http2PushedStream *PushSource() { return mPushSource; }

  stateType HTTPState() { return mState; }
  void SetHTTPState(stateType val) { mState = val; }

  virtual nsresult ReadSegments(nsAHttpSegmentReader *,  uint32_t, uint32_t *);
  virtual nsresult WriteSegments(nsAHttpSegmentWriter *, uint32_t, uint32_t *);
  virtual bool DeferCleanupOnSuccess() { return false; }

  const nsAFlatCString &Origin() const { return mOrigin; }

  bool RequestBlockedOnRead()
  {
    return static_cast<bool>(mRequestBlockedOnRead);
  }

  bool HasRegisteredID() { return mStreamID != 0; }

  nsAHttpTransaction *Transaction() { return mTransaction; }
  virtual nsILoadGroupConnectionInfo *LoadGroupConnectionInfo()
  {
    return mTransaction ? mTransaction->LoadGroupConnectionInfo() : nullptr;
  }

  void Close(nsresult reason);

  void SetRecvdFin(bool aStatus);
  bool RecvdFin() { return mRecvdFin; }

  void SetRecvdData(bool aStatus) { mReceivedData = aStatus ? 1 : 0; }
  bool RecvdData() { return mReceivedData; }

  void SetSentFin(bool aStatus);
  bool SentFin() { return mSentFin; }

  void SetRecvdReset(bool aStatus);
  bool RecvdReset() { return mRecvdReset; }

  void SetSentReset(bool aStatus);
  bool SentReset() { return mSentReset; }

  void SetCountAsActive(bool aStatus) { mCountAsActive = aStatus ? 1 : 0; }
  bool CountAsActive() { return mCountAsActive; }

  void SetAllHeadersReceived();
  bool AllHeadersReceived() { return mAllHeadersReceived; }

  void UpdateTransportSendEvents(uint32_t count);
  void UpdateTransportReadEvents(uint32_t count);

  // NS_ERROR_ABORT terminates stream, other failure terminates session
  nsresult ConvertResponseHeaders(Http2Decompressor *, nsACString &, nsACString &);
  nsresult ConvertPushHeaders(Http2Decompressor *, nsACString &, nsACString &);

  bool AllowFlowControlledWrite();
  void UpdateServerReceiveWindow(int32_t delta);
  int64_t ServerReceiveWindow() { return mServerReceiveWindow; }

  void DecrementClientReceiveWindow(uint32_t delta) {
    mClientReceiveWindow -= delta;
    mLocalUnacked += delta;
  }

  void IncrementClientReceiveWindow(uint32_t delta) {
    mClientReceiveWindow += delta;
    mLocalUnacked -= delta;
  }

  uint64_t LocalUnAcked() { return mLocalUnacked; }
  int64_t  ClientReceiveWindow()  { return mClientReceiveWindow; }

  bool     BlockedOnRwin() { return mBlockedOnRwin; }

  uint32_t Priority() { return mPriority; }
  void SetPriority(uint32_t);
  void SetPriorityDependency(uint32_t, uint8_t, bool);

  // A pull stream has an implicit sink, a pushed stream has a sink
  // once it is matched to a pull stream.
  virtual bool HasSink() { return true; }

  virtual ~Http2Stream();

protected:
  static void CreatePushHashKey(const nsCString &scheme,
                                const nsCString &hostHeader,
                                uint64_t serial,
                                const nsCSubstring &pathInfo,
                                nsCString &outOrigin,
                                nsCString &outKey);

  // These internal states track request generation
  enum upstreamStateType {
    GENERATING_HEADERS,
    GENERATING_BODY,
    SENDING_BODY,
    SENDING_FIN_STREAM,
    UPSTREAM_COMPLETE
  };

  uint32_t mStreamID;

  // The session that this stream is a subset of
  Http2Session *mSession;

  nsCString     mOrigin;
  nsCString     mHeaderHost;
  nsCString     mHeaderScheme;
  nsCString     mHeaderPath;

  // Each stream goes from generating_headers to upstream_complete, perhaps
  // looping on multiple instances of generating_body and
  // sending_body for each frame in the upload.
  enum upstreamStateType mUpstreamState;

  // The HTTP/2 state for the stream from section 5.1
  enum stateType mState;

  // Flag is set when all http request headers have been read and ID is stable
  uint32_t                     mAllHeadersSent       : 1;

  // Flag is set when all http request headers have been read and ID is stable
  uint32_t                     mAllHeadersReceived   : 1;

  void     ChangeState(enum upstreamStateType);

private:
  friend class nsAutoPtr<Http2Stream>;

  nsresult ParseHttpRequestHeaders(const char *, uint32_t, uint32_t *);
  void     AdjustPushedPriority();
  void     AdjustInitialWindow();
  nsresult TransmitFrame(const char *, uint32_t *, bool forceCommitment);
  void     GenerateDataFrameHeader(uint32_t, bool);

  // The underlying HTTP transaction. This pointer is used as the key
  // in the Http2Session mStreamTransactionHash so it is important to
  // keep a reference to it as long as this stream is a member of that hash.
  // (i.e. don't change it or release it after it is set in the ctor).
  nsRefPtr<nsAHttpTransaction> mTransaction;

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

  // Flag is set after 1st DATA frame has been passed to stream
  uint32_t                     mReceivedData         : 1;

  // Flag is set after RST_STREAM has been received for this stream
  uint32_t                     mRecvdReset           : 1;

  // Flag is set after RST_STREAM has been generated for this stream
  uint32_t                     mSentReset            : 1;

  // Flag is set when stream is counted towards MAX_CONCURRENT streams in session
  uint32_t                     mCountAsActive        : 1;

  // Flag is set when a FIN has been placed on a data or header frame
  // (i.e after the client has closed)
  uint32_t                     mSentFin              : 1;

  // Flag is set after the WAITING_FOR Transport event has been generated
  uint32_t                     mSentWaitingFor       : 1;

  // Flag is set after TCP send autotuning has been disabled
  uint32_t                     mSetTCPSocketBuffer   : 1;

  // The InlineFrame and associated data is used for composing control
  // frames and data frame headers.
  nsAutoArrayPtr<uint8_t>      mTxInlineFrame;
  uint32_t                     mTxInlineFrameSize;
  uint32_t                     mTxInlineFrameUsed;

  // mTxStreamFrameSize tracks the progress of
  // transmitting a request body data frame. The data frame itself
  // is never copied into the spdy layer.
  uint32_t                     mTxStreamFrameSize;

  // Buffer for request header compression.
  nsCString                    mFlatHttpRequestHeaders;

  // Track the content-length of a request body so that we can
  // place the fin flag on the last data packet instead of waiting
  // for a stream closed indication. Relying on stream close results
  // in an extra 0-length runt packet and seems to have some interop
  // problems with the google servers. Connect does rely on stream
  // close by setting this to the max value.
  int64_t                      mRequestBodyLenRemaining;

  uint32_t                     mPriority;
  uint8_t                      mPriorityWeight;

  // mClientReceiveWindow, mServerReceiveWindow, and mLocalUnacked are for flow control.
  // *window are signed because the race conditions in asynchronous SETTINGS
  // messages can force them temporarily negative.

  // mClientReceiveWindow is how much data the server will send without getting a
  //   window update
  int64_t                      mClientReceiveWindow;

  // mServerReceiveWindow is how much data the client is allowed to send without
  //   getting a window update
  int64_t                      mServerReceiveWindow;

  // LocalUnacked is the number of bytes received by the client but not
  //   yet reflected in a window update. Sending that update will increment
  //   ClientReceiveWindow
  uint64_t                     mLocalUnacked;

  // True when sending is suspended becuase the server receive window is
  //   <= 0
  bool                         mBlockedOnRwin;

  // For Progress Events
  uint64_t                     mTotalSent;
  uint64_t                     mTotalRead;

  // For Http2Push
  Http2PushedStream *mPushSource;

/// connect tunnels
public:
  bool IsTunnel() { return mIsTunnel; }
private:
  void ClearTransactionsBlockedOnTunnel();
  void MapStreamToHttpConnection();

  bool mIsTunnel;
};

} // namespace mozilla::net
} // namespace mozilla

#endif // mozilla_net_Http2Stream_h
