/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_Http2Stream_h
#define mozilla_net_Http2Stream_h

// HTTP/2 - RFC7540
// https://www.rfc-editor.org/rfc/rfc7540.txt

#include "mozilla/Attributes.h"
#include "mozilla/UniquePtr.h"
#include "nsAHttpTransaction.h"
#include "nsISupportsPriority.h"
#include "SimpleBuffer.h"
#include "nsISupportsImpl.h"

class nsIInputStream;
class nsIOutputStream;

namespace mozilla {
class OriginAttributes;
}

namespace mozilla {
namespace net {

class nsStandardURL;
class Http2Session;
class Http2Decompressor;

class Http2Stream : public nsAHttpSegmentReader,
                    public nsAHttpSegmentWriter,
                    public SupportsWeakPtr {
 public:
  NS_DECL_NSAHTTPSEGMENTREADER
  NS_DECL_NSAHTTPSEGMENTWRITER
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Http2Stream, override)

  enum stateType {
    IDLE,
    RESERVED_BY_REMOTE,
    OPEN,
    CLOSED_BY_LOCAL,
    CLOSED_BY_REMOTE,
    CLOSED
  };

  const static int32_t kNormalPriority = 0x1000;
  const static int32_t kWorstPriority =
      kNormalPriority + nsISupportsPriority::PRIORITY_LOWEST;
  const static int32_t kBestPriority =
      kNormalPriority + nsISupportsPriority::PRIORITY_HIGHEST;

  Http2Stream(nsAHttpTransaction*, Http2Session*, int32_t, uint64_t);

  uint32_t StreamID() { return mStreamID; }
  Http2PushedStream* PushSource() { return mPushSource; }
  void ClearPushSource();

  stateType HTTPState() { return mState; }
  void SetHTTPState(stateType val) { mState = val; }

  [[nodiscard]] virtual nsresult ReadSegments(nsAHttpSegmentReader*, uint32_t,
                                              uint32_t*);
  [[nodiscard]] virtual nsresult WriteSegments(nsAHttpSegmentWriter*, uint32_t,
                                               uint32_t*);
  virtual bool DeferCleanup(nsresult status);

  // The consumer stream is the synthetic pull stream hooked up to this stream
  // http2PushedStream overrides it
  virtual Http2Stream* GetConsumerStream() { return nullptr; };

  const nsCString& Origin() const { return mOrigin; }
  const nsCString& Host() const { return mHeaderHost; }
  const nsCString& Path() const { return mHeaderPath; }

  bool RequestBlockedOnRead() {
    return static_cast<bool>(mRequestBlockedOnRead);
  }

  bool HasRegisteredID() { return mStreamID != 0; }

  nsAHttpTransaction* Transaction() { return mTransaction; }
  virtual nsIRequestContext* RequestContext() {
    return mTransaction ? mTransaction->RequestContext() : nullptr;
  }

  void Close(nsresult reason);
  void SetResponseIsComplete();

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

  void SetQueued(bool aStatus) { mQueued = aStatus ? 1 : 0; }
  bool Queued() { return mQueued; }

  void SetCountAsActive(bool aStatus) { mCountAsActive = aStatus ? 1 : 0; }
  bool CountAsActive() { return mCountAsActive; }

  void SetAllHeadersReceived();
  void UnsetAllHeadersReceived() { mAllHeadersReceived = 0; }
  bool AllHeadersReceived() { return mAllHeadersReceived; }

  void UpdateTransportSendEvents(uint32_t count);
  void UpdateTransportReadEvents(uint32_t count);

  // NS_ERROR_ABORT terminates stream, other failure terminates session
  [[nodiscard]] nsresult ConvertResponseHeaders(Http2Decompressor*, nsACString&,
                                                nsACString&, int32_t&);
  [[nodiscard]] nsresult ConvertPushHeaders(Http2Decompressor*, nsACString&,
                                            nsACString&);
  [[nodiscard]] nsresult ConvertResponseTrailers(Http2Decompressor*,
                                                 nsACString&);

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

  uint64_t LocalUnAcked();
  int64_t ClientReceiveWindow() { return mClientReceiveWindow; }

  bool BlockedOnRwin() { return mBlockedOnRwin; }

  uint32_t Priority() { return mPriority; }
  uint32_t PriorityDependency() { return mPriorityDependency; }
  uint8_t PriorityWeight() { return mPriorityWeight; }
  void SetPriority(uint32_t);
  void SetPriorityDependency(uint32_t, uint32_t);
  void UpdatePriorityDependency();

  uint64_t TransactionTabId() { return mTransactionTabId; }

  // A pull stream has an implicit sink, a pushed stream has a sink
  // once it is matched to a pull stream.
  virtual bool HasSink() { return true; }

  // This is a no-op on pull streams. Pushed streams override this.
  virtual void SetPushComplete(){};

  Http2Session* Session() { return mSession; }

  [[nodiscard]] static nsresult MakeOriginURL(const nsACString& origin,
                                              nsCOMPtr<nsIURI>& url);

  [[nodiscard]] static nsresult MakeOriginURL(const nsACString& scheme,
                                              const nsACString& origin,
                                              nsCOMPtr<nsIURI>& url);

  // Mirrors nsAHttpTransaction
  bool Do0RTT();
  nsresult Finish0RTT(bool aRestart, bool aAlpnIgnored);

  nsresult GetOriginAttributes(mozilla::OriginAttributes* oa);

  virtual void TopBrowsingContextIdChanged(uint64_t id);
  void TopBrowsingContextIdChangedInternal(
      uint64_t id);  // For use by pushed streams only

 protected:
  virtual ~Http2Stream();
  static void CreatePushHashKey(
      const nsCString& scheme, const nsCString& hostHeader,
      const mozilla::OriginAttributes& originAttributes, uint64_t serial,
      const nsACString& pathInfo, nsCString& outOrigin, nsCString& outKey);

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
  Http2Session* mSession;

  // These are temporary state variables to hold the argument to
  // Read/WriteSegments so it can be accessed by On(read/write)segment
  // further up the stack.
  nsAHttpSegmentReader* mSegmentReader;
  nsAHttpSegmentWriter* mSegmentWriter;

  nsCString mOrigin;
  nsCString mHeaderHost;
  nsCString mHeaderScheme;
  nsCString mHeaderPath;

  // Each stream goes from generating_headers to upstream_complete, perhaps
  // looping on multiple instances of generating_body and
  // sending_body for each frame in the upload.
  enum upstreamStateType mUpstreamState;

  // The HTTP/2 state for the stream from section 5.1
  enum stateType mState;

  // Flag is set when all http request headers have been read ID is not stable
  uint32_t mRequestHeadersDone : 1;

  // Flag is set when ID is stable and concurrency limits are met
  uint32_t mOpenGenerated : 1;

  // Flag is set when all http response headers have been read
  uint32_t mAllHeadersReceived : 1;

  // Flag is set when stream is queued inside the session due to
  // concurrency limits being exceeded
  uint32_t mQueued : 1;

  void ChangeState(enum upstreamStateType);

  virtual void AdjustInitialWindow();
  [[nodiscard]] nsresult TransmitFrame(const char*, uint32_t*,
                                       bool forceCommitment);

  // The underlying socket transport object is needed to propogate some events
  nsISocketTransport* mSocketTransport;

  uint8_t mPriorityWeight;       // h2 weight
  uint32_t mPriorityDependency;  // h2 stream id this one depends on
  uint64_t mCurrentTopBrowsingContextId;
  uint64_t mTransactionTabId;

 private:
  friend class mozilla::DefaultDelete<Http2Stream>;

  [[nodiscard]] nsresult ParseHttpRequestHeaders(const char*, uint32_t,
                                                 uint32_t*);
  [[nodiscard]] nsresult GenerateOpen();

  void AdjustPushedPriority();
  void GenerateDataFrameHeader(uint32_t, bool);

  [[nodiscard]] nsresult BufferInput(uint32_t, uint32_t*);

  // The underlying HTTP transaction. This pointer is used as the key
  // in the Http2Session mStreamTransactionHash so it is important to
  // keep a reference to it as long as this stream is a member of that hash.
  // (i.e. don't change it or release it after it is set in the ctor).
  RefPtr<nsAHttpTransaction> mTransaction;

  // The quanta upstream data frames are chopped into
  uint32_t mChunkSize;

  // Flag is set when the HTTP processor has more data to send
  // but has blocked in doing so.
  uint32_t mRequestBlockedOnRead : 1;

  // Flag is set after the response frame bearing the fin bit has
  // been processed. (i.e. after the server has closed).
  uint32_t mRecvdFin : 1;

  // Flag is set after 1st DATA frame has been passed to stream
  uint32_t mReceivedData : 1;

  // Flag is set after RST_STREAM has been received for this stream
  uint32_t mRecvdReset : 1;

  // Flag is set after RST_STREAM has been generated for this stream
  uint32_t mSentReset : 1;

  // Flag is set when stream is counted towards MAX_CONCURRENT streams in
  // session
  uint32_t mCountAsActive : 1;

  // Flag is set when a FIN has been placed on a data or header frame
  // (i.e after the client has closed)
  uint32_t mSentFin : 1;

  // Flag is set after the WAITING_FOR Transport event has been generated
  uint32_t mSentWaitingFor : 1;

  // Flag is set after TCP send autotuning has been disabled
  uint32_t mSetTCPSocketBuffer : 1;

  // Flag is set when OnWriteSegment is being called directly from stream
  // instead of transaction
  uint32_t mBypassInputBuffer : 1;

  // The InlineFrame and associated data is used for composing control
  // frames and data frame headers.
  UniquePtr<uint8_t[]> mTxInlineFrame;
  uint32_t mTxInlineFrameSize;
  uint32_t mTxInlineFrameUsed;

  // mTxStreamFrameSize tracks the progress of
  // transmitting a request body data frame. The data frame itself
  // is never copied into the spdy layer.
  uint32_t mTxStreamFrameSize;

  // Buffer for request header compression.
  nsCString mFlatHttpRequestHeaders;

  // Track the content-length of a request body so that we can
  // place the fin flag on the last data packet instead of waiting
  // for a stream closed indication. Relying on stream close results
  // in an extra 0-length runt packet and seems to have some interop
  // problems with the google servers. Connect does rely on stream
  // close by setting this to the max value.
  int64_t mRequestBodyLenRemaining;

  uint32_t mPriority;  // geckoish weight

  // mClientReceiveWindow, mServerReceiveWindow, and mLocalUnacked are for flow
  // control. *window are signed because the race conditions in asynchronous
  // SETTINGS messages can force them temporarily negative.

  // mClientReceiveWindow is how much data the server will send without getting
  // a
  //   window update
  int64_t mClientReceiveWindow;

  // mServerReceiveWindow is how much data the client is allowed to send without
  //   getting a window update
  int64_t mServerReceiveWindow;

  // LocalUnacked is the number of bytes received by the client but not
  //   yet reflected in a window update. Sending that update will increment
  //   ClientReceiveWindow
  uint64_t mLocalUnacked;

  // True when sending is suspended becuase the server receive window is
  //   <= 0
  bool mBlockedOnRwin;

  // For Progress Events
  uint64_t mTotalSent;
  uint64_t mTotalRead;

  // For Http2Push
  Http2PushedStream* mPushSource;

  // Used to store stream data when the transaction channel cannot keep up
  // and flow control has not yet kicked in.
  SimpleBuffer mSimpleBuffer;

  bool mAttempting0RTT;

  /// connect tunnels
 public:
  bool IsTunnel() { return mIsTunnel; }
  // TODO - remove as part of bug 1564120 fix?
  // This method saves the key the tunnel was registered under, so that when the
  // associated transaction connection info hash key changes, we still find it
  // and decrement the correct item in the session's tunnel hash table.
  nsCString& RegistrationKey();

 private:
  void ClearTransactionsBlockedOnTunnel();
  void MapStreamToPlainText();
  bool MapStreamToHttpConnection(const nsACString& aFlat407Headers,
                                 int32_t aHttpResponseCode = -1);

  bool mIsTunnel;
  bool mPlainTextTunnel;
  nsCString mRegistrationKey;

  /// websockets
 public:
  bool IsWebsocket() { return mIsWebsocket; }

 private:
  bool mIsWebsocket;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_Http2Stream_h
