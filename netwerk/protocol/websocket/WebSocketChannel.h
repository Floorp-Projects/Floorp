/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_WebSocketChannel_h
#define mozilla_net_WebSocketChannel_h

#include "nsISupports.h"
#include "nsIInterfaceRequestor.h"
#include "nsIStreamListener.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsITimer.h"
#include "nsIDNSListener.h"
#include "nsINamed.h"
#include "nsIObserver.h"
#include "nsIProtocolProxyCallback.h"
#include "nsIChannelEventSink.h"
#include "nsIHttpChannelInternal.h"
#include "mozilla/net/WebSocketConnectionListener.h"
#include "mozilla/Mutex.h"
#include "BaseWebSocketChannel.h"

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsDeque.h"
#include "mozilla/Atomics.h"

class nsIAsyncVerifyRedirectCallback;
class nsIDashboardEventNotifier;
class nsIEventTarget;
class nsIHttpChannel;
class nsIRandomGenerator;
class nsISocketTransport;
class nsIURI;

namespace mozilla {
namespace net {

class OutboundMessage;
class OutboundEnqueuer;
class nsWSAdmissionManager;
class PMCECompression;
class CallOnMessageAvailable;
class CallOnStop;
class CallOnServerClose;
class CallAcknowledge;
class WebSocketEventService;
class WebSocketConnectionBase;

[[nodiscard]] extern nsresult CalculateWebSocketHashedSecret(
    const nsACString& aKey, nsACString& aHash);
extern void ProcessServerWebSocketExtensions(const nsACString& aExtensions,
                                             nsACString& aNegotiatedExtensions);

// Used to enforce "1 connecting websocket per host" rule, and reconnect delays
enum wsConnectingState {
  NOT_CONNECTING = 0,     // Not yet (or no longer) trying to open connection
  CONNECTING_QUEUED,      // Waiting for other ws to same host to finish opening
  CONNECTING_DELAYED,     // Delayed by "reconnect after failure" algorithm
  CONNECTING_IN_PROGRESS  // Started connection: waiting for result
};

class WebSocketChannel : public BaseWebSocketChannel,
                         public nsIHttpUpgradeListener,
                         public nsIStreamListener,
                         public nsIInputStreamCallback,
                         public nsIOutputStreamCallback,
                         public nsITimerCallback,
                         public nsIDNSListener,
                         public nsIObserver,
                         public nsIProtocolProxyCallback,
                         public nsIInterfaceRequestor,
                         public nsIChannelEventSink,
                         public nsINamed,
                         public WebSocketConnectionListener {
  friend class WebSocketFrame;

 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIHTTPUPGRADELISTENER
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIINPUTSTREAMCALLBACK
  NS_DECL_NSIOUTPUTSTREAMCALLBACK
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSIDNSLISTENER
  NS_DECL_NSIPROTOCOLPROXYCALLBACK
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSICHANNELEVENTSINK
  NS_DECL_NSIOBSERVER
  NS_DECL_NSINAMED

  // nsIWebSocketChannel methods BaseWebSocketChannel didn't implement for us
  //
  NS_IMETHOD AsyncOpen(nsIURI* aURI, const nsACString& aOrigin,
                       JS::Handle<JS::Value> aOriginAttributes,
                       uint64_t aWindowID, nsIWebSocketListener* aListener,
                       nsISupports* aContext, JSContext* aCx) override;
  NS_IMETHOD AsyncOpenNative(nsIURI* aURI, const nsACString& aOrigin,
                             const OriginAttributes& aOriginAttributes,
                             uint64_t aWindowID,
                             nsIWebSocketListener* aListener,
                             nsISupports* aContext) override;
  NS_IMETHOD Close(uint16_t aCode, const nsACString& aReason) override;
  NS_IMETHOD SendMsg(const nsACString& aMsg) override;
  NS_IMETHOD SendBinaryMsg(const nsACString& aMsg) override;
  NS_IMETHOD SendBinaryStream(nsIInputStream* aStream,
                              uint32_t length) override;
  NS_IMETHOD GetSecurityInfo(nsITransportSecurityInfo** aSecurityInfo) override;

  WebSocketChannel();
  static void Shutdown();

  // Off main thread URI access.
  void GetEffectiveURL(nsAString& aEffectiveURL) const override;
  bool IsEncrypted() const override;

  nsresult OnTransportAvailableInternal();
  void OnError(nsresult aStatus) override;
  void OnTCPClosed() override;
  nsresult OnDataReceived(uint8_t* aData, uint32_t aCount) override;

  const static uint32_t kControlFrameMask = 0x8;

  // First byte of the header
  const static uint8_t kFinalFragBit = 0x80;
  const static uint8_t kRsvBitsMask = 0x70;
  const static uint8_t kRsv1Bit = 0x40;
  const static uint8_t kRsv2Bit = 0x20;
  const static uint8_t kRsv3Bit = 0x10;
  const static uint8_t kOpcodeBitsMask = 0x0F;

  // Second byte of the header
  const static uint8_t kMaskBit = 0x80;
  const static uint8_t kPayloadLengthBitsMask = 0x7F;

 protected:
  ~WebSocketChannel() override;

 private:
  friend class OutboundEnqueuer;
  friend class nsWSAdmissionManager;
  friend class FailDelayManager;
  friend class CallOnMessageAvailable;
  friend class CallOnStop;
  friend class CallOnServerClose;
  friend class CallAcknowledge;

  // Common send code for binary + text msgs
  [[nodiscard]] nsresult SendMsgCommon(const nsACString& aMsg, bool isBinary,
                                       uint32_t length,
                                       nsIInputStream* aStream = nullptr);

  void EnqueueOutgoingMessage(nsDeque<OutboundMessage>& aQueue,
                              OutboundMessage* aMsg);
  void DoEnqueueOutgoingMessage();

  void PrimeNewOutgoingMessage();
  void DeleteCurrentOutGoingMessage();
  void GeneratePong(uint8_t* payload, uint32_t len);
  void GeneratePing();

  [[nodiscard]] nsresult OnNetworkChanged();
  [[nodiscard]] nsresult StartPinging();

  void BeginOpen(bool aCalledFromAdmissionManager);
  void BeginOpenInternal();
  [[nodiscard]] nsresult HandleExtensions();
  [[nodiscard]] nsresult SetupRequest();
  [[nodiscard]] nsresult ApplyForAdmission();
  [[nodiscard]] nsresult DoAdmissionDNS();
  [[nodiscard]] nsresult CallStartWebsocketData();
  [[nodiscard]] nsresult StartWebsocketData();
  uint16_t ResultToCloseCode(nsresult resultCode);
  void ReportConnectionTelemetry(nsresult aStatusCode);

  void StopSession(nsresult reason);
  void DoStopSession(nsresult reason);
  void AbortSession(nsresult reason);
  void ReleaseSession();
  void CleanupConnection();
  void IncrementSessionCount();
  void DecrementSessionCount();

  void EnsureHdrOut(uint32_t size);

  static void ApplyMask(uint32_t mask, uint8_t* data, uint64_t len);

  bool IsPersistentFramePtr();
  [[nodiscard]] nsresult ProcessInput(uint8_t* buffer, uint32_t count);
  [[nodiscard]] bool UpdateReadBuffer(uint8_t* buffer, uint32_t count,
                                      uint32_t accumulatedFragments,
                                      uint32_t* available);

  inline void ResetPingTimer() {
    mPingOutstanding = 0;
    if (mPingTimer) {
      if (!mPingInterval) {
        // The timer was created by forced ping and regular pinging is disabled,
        // so cancel and null out mPingTimer.
        mPingTimer->Cancel();
        mPingTimer = nullptr;
      } else {
        mPingTimer->SetDelay(mPingInterval);
      }
    }
  }

  void NotifyOnStart();

  nsCOMPtr<nsIEventTarget> mIOThread;
  // Set in AsyncOpenNative and AsyncOnChannelRedirect, modified in
  // DoStopSession on IO thread (.forget()).   Probably ok...
  nsCOMPtr<nsIHttpChannelInternal> mChannel;
  nsCOMPtr<nsIHttpChannel> mHttpChannel;

  nsCOMPtr<nsICancelable> mCancelable MOZ_GUARDED_BY(mMutex);
  // Mainthread only
  nsCOMPtr<nsIAsyncVerifyRedirectCallback> mRedirectCallback;
  // Set on Mainthread during AsyncOpen, used on IO thread and Mainthread
  nsCOMPtr<nsIRandomGenerator> mRandomGenerator;

  nsCString mHashedSecret;  // MainThread only

  // Used as key for connection managment: Initially set to hostname from URI,
  // then to IP address (unless we're leaving DNS resolution to a proxy server)
  // MainThread only
  nsCString mAddress;
  nsCString mPath;
  int32_t mPort;  // WS server port
  // Secondary key for the connection queue. Used by nsWSAdmissionManager.
  nsCString mOriginSuffix;  // MainThread only

  // Used for off main thread access to the URI string.
  // Set on MainThread in AsyncOpenNative, used on TargetThread and IO thread
  nsCString mHost;
  nsString mEffectiveURL;

  // Set on MainThread before multithread use, used on IO thread, cleared on
  // IOThread
  nsCOMPtr<nsISocketTransport> mTransport;
  nsCOMPtr<nsIAsyncInputStream> mSocketIn;
  nsCOMPtr<nsIAsyncOutputStream> mSocketOut;
  RefPtr<WebSocketConnectionBase> mConnection;

  // Only used on IO Thread (accessed when known-to-be-null in DoStopSession
  // on MainThread before mDataStarted)
  nsCOMPtr<nsITimer> mCloseTimer;
  // set in AsyncOpenInternal on MainThread, used on IO thread.
  // No multithread use before it's set, no changes after that.
  uint32_t mCloseTimeout; /* milliseconds */

  nsCOMPtr<nsITimer> mOpenTimer; /* Mainthread only */
  uint32_t mOpenTimeout;         /* milliseconds, MainThread only */
  wsConnectingState mConnecting; /* 0 if not connecting, MainThread only */
  // Set on MainThread, deleted on MainThread, used on MainThread or
  // IO Thread (in DoStopSession). Mutex required to access off-main-thread.
  nsCOMPtr<nsITimer> mReconnectDelayTimer MOZ_GUARDED_BY(mMutex);

  // Only touched on IOThread (DoStopSession reads it on MainThread if
  // we haven't connected yet (mDataStarted==false), and it's always null
  // until mDataStarted=true)
  nsCOMPtr<nsITimer> mPingTimer;

  // Created in DoStopSession on IO thread (mDataStarted=true), accessed
  // only from IO Thread
  nsCOMPtr<nsITimer> mLingeringCloseTimer;
  const static int32_t kLingeringCloseTimeout = 1000;
  const static int32_t kLingeringCloseThreshold = 50;

  RefPtr<WebSocketEventService> mService;  // effectively const

  int32_t
      mMaxConcurrentConnections;  // only used in AsyncOpenNative on MainThread

  // Set on MainThread in AsyncOpenNative; then used on IO thread
  uint64_t mInnerWindowID;

  // following members are accessed only on the main thread
  uint32_t mGotUpgradeOK : 1;
  uint32_t mRecvdHttpUpgradeTransport : 1;
  uint32_t mAllowPMCE : 1;
  uint32_t : 0;  // ensure these aren't mixed with the next set

  // following members are accessed only on the IO thread
  uint32_t mPingOutstanding : 1;
  uint32_t mReleaseOnTransmit : 1;
  uint32_t : 0;

  Atomic<bool> mDataStarted;
  // All changes to mRequestedClose happen under mutex, but since it's atomic,
  // it can be read anywhere without a lock
  Atomic<bool> mRequestedClose;
  // mServer/ClientClosed are only modified on IOThread
  Atomic<bool> mClientClosed;
  Atomic<bool> mServerClosed;
  // All changes to mStopped happen under mutex, but since it's atomic, it
  // can be read anywhere without a lock
  Atomic<bool> mStopped;
  Atomic<bool> mCalledOnStop;
  Atomic<bool> mTCPClosed;
  Atomic<bool> mOpenedHttpChannel;
  Atomic<bool> mIncrementedSessionCount;
  Atomic<bool> mDecrementedSessionCount;

  int32_t mMaxMessageSize;  // set on MainThread in AsyncOpenNative, read on IO
                            // thread
  // Set on IOThread, or on MainThread before mDataStarted.  Used on IO Thread
  // (after mDataStarted)
  nsresult mStopOnClose;
  uint16_t mServerCloseCode;     // only used on IO thread
  nsCString mServerCloseReason;  // only used on IO thread
  uint16_t mScriptCloseCode MOZ_GUARDED_BY(mMutex);
  nsCString mScriptCloseReason MOZ_GUARDED_BY(mMutex);

  // These are for the read buffers
  const static uint32_t kIncomingBufferInitialSize = 16 * 1024;
  // We're ok with keeping a buffer this size or smaller around for the life of
  // the websocket.  If a particular message needs bigger than this we'll
  // increase the buffer temporarily, then drop back down to this size.
  const static uint32_t kIncomingBufferStableSize = 128 * 1024;

  // Set at creation, used/modified only on IO thread
  uint8_t* mFramePtr;
  uint8_t* mBuffer;
  uint8_t mFragmentOpcode;
  uint32_t mFragmentAccumulator;
  uint32_t mBuffered;
  uint32_t mBufferSize;

  // These are for the send buffers
  const static int32_t kCopyBreak = 1000;

  // Only used on IO thread
  OutboundMessage* mCurrentOut;
  uint32_t mCurrentOutSent;
  nsDeque<OutboundMessage> mOutgoingMessages;
  nsDeque<OutboundMessage> mOutgoingPingMessages;
  nsDeque<OutboundMessage> mOutgoingPongMessages;
  uint32_t mHdrOutToSend;
  uint8_t* mHdrOut;
  uint8_t mOutHeader[kCopyBreak + 16]{0};

  // Set on MainThread in OnStartRequest (before mDataStarted), or in
  // HandleExtensions() or OnTransportAvailableInternal(),used on IO Thread
  // (after mDataStarted), cleared in DoStopSession on IOThread or on
  // MainThread (if mDataStarted == false).
  Mutex mCompressorMutex;
  UniquePtr<PMCECompression> mPMCECompressor MOZ_GUARDED_BY(mCompressorMutex);

  // Used by EnsureHdrOut, which isn't called anywhere
  uint32_t mDynamicOutputSize;
  uint8_t* mDynamicOutput;
  // Set on creation and AsyncOpen, read on both threads
  Atomic<bool> mPrivateBrowsing;

  nsCOMPtr<nsIDashboardEventNotifier>
      mConnectionLogService;  // effectively const

  mozilla::Mutex mMutex;
};

class WebSocketSSLChannel : public WebSocketChannel {
 public:
  WebSocketSSLChannel() { BaseWebSocketChannel::mEncrypted = true; }

 protected:
  virtual ~WebSocketSSLChannel() = default;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_WebSocketChannel_h
