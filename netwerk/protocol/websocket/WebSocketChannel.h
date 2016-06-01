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
#include "nsIObserver.h"
#include "nsIProtocolProxyCallback.h"
#include "nsIChannelEventSink.h"
#include "nsIHttpChannelInternal.h"
#include "nsIStringStream.h"
#include "BaseWebSocketChannel.h"

#ifdef MOZ_WIDGET_GONK
#include "nsINetworkInterface.h"
#include "nsProxyRelease.h"
#endif

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

extern nsresult
CalculateWebSocketHashedSecret(const nsACString& aKey, nsACString& aHash);
extern void
ProcessServerWebSocketExtensions(const nsACString& aExtensions,
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
                         public nsIChannelEventSink
{
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

  // nsIWebSocketChannel methods BaseWebSocketChannel didn't implement for us
  //
  NS_IMETHOD AsyncOpen(nsIURI *aURI,
                       const nsACString &aOrigin,
                       uint64_t aWindowID,
                       nsIWebSocketListener *aListener,
                       nsISupports *aContext) override;
  NS_IMETHOD Close(uint16_t aCode, const nsACString & aReason) override;
  NS_IMETHOD SendMsg(const nsACString &aMsg) override;
  NS_IMETHOD SendBinaryMsg(const nsACString &aMsg) override;
  NS_IMETHOD SendBinaryStream(nsIInputStream *aStream, uint32_t length) override;
  NS_IMETHOD GetSecurityInfo(nsISupports **aSecurityInfo) override;

  WebSocketChannel();
  static void Shutdown();
  bool IsOnTargetThread();

  // Off main thread URI access.
  void GetEffectiveURL(nsAString& aEffectiveURL) const override;
  bool IsEncrypted() const override;

  const static uint32_t kControlFrameMask   = 0x8;

  // First byte of the header
  const static uint8_t kFinalFragBit        = 0x80;
  const static uint8_t kRsvBitsMask         = 0x70;
  const static uint8_t kRsv1Bit             = 0x40;
  const static uint8_t kRsv2Bit             = 0x20;
  const static uint8_t kRsv3Bit             = 0x10;
  const static uint8_t kOpcodeBitsMask      = 0x0F;

  // Second byte of the header
  const static uint8_t kMaskBit             = 0x80;
  const static uint8_t kPayloadLengthBitsMask = 0x7F;

protected:
  virtual ~WebSocketChannel();

private:
  friend class OutboundEnqueuer;
  friend class nsWSAdmissionManager;
  friend class FailDelayManager;
  friend class CallOnMessageAvailable;
  friend class CallOnStop;
  friend class CallOnServerClose;
  friend class CallAcknowledge;

  // Common send code for binary + text msgs
  nsresult SendMsgCommon(const nsACString *aMsg, bool isBinary,
                         uint32_t length, nsIInputStream *aStream = nullptr);

  void EnqueueOutgoingMessage(nsDeque &aQueue, OutboundMessage *aMsg);

  void PrimeNewOutgoingMessage();
  void DeleteCurrentOutGoingMessage();
  void GeneratePong(uint8_t *payload, uint32_t len);
  void GeneratePing();

  nsresult OnNetworkChanged();
  nsresult StartPinging();

  void     BeginOpen(bool aCalledFromAdmissionManager);
  void     BeginOpenInternal();
  nsresult HandleExtensions();
  nsresult SetupRequest();
  nsresult ApplyForAdmission();
  nsresult DoAdmissionDNS();
  nsresult StartWebsocketData();
  uint16_t ResultToCloseCode(nsresult resultCode);
  void     ReportConnectionTelemetry();

  void StopSession(nsresult reason);
  void AbortSession(nsresult reason);
  void ReleaseSession();
  void CleanupConnection();
  void IncrementSessionCount();
  void DecrementSessionCount();

  void EnsureHdrOut(uint32_t size);

  static void ApplyMask(uint32_t mask, uint8_t *data, uint64_t len);

  bool     IsPersistentFramePtr();
  nsresult ProcessInput(uint8_t *buffer, uint32_t count);
  bool UpdateReadBuffer(uint8_t *buffer, uint32_t count,
                        uint32_t accumulatedFragments,
                        uint32_t *available);

  inline void ResetPingTimer()
  {
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

  nsCOMPtr<nsIEventTarget>                 mSocketThread;
  nsCOMPtr<nsIHttpChannelInternal>         mChannel;
  nsCOMPtr<nsIHttpChannel>                 mHttpChannel;
  nsCOMPtr<nsICancelable>                  mCancelable;
  nsCOMPtr<nsIAsyncVerifyRedirectCallback> mRedirectCallback;
  nsCOMPtr<nsIRandomGenerator>             mRandomGenerator;

  nsCString                       mHashedSecret;

  // Used as key for connection managment: Initially set to hostname from URI,
  // then to IP address (unless we're leaving DNS resolution to a proxy server)
  nsCString                       mAddress;
  int32_t                         mPort;          // WS server port

  // Used for off main thread access to the URI string.
  nsCString                       mHost;
  nsString                        mEffectiveURL;

  nsCOMPtr<nsISocketTransport>    mTransport;
  nsCOMPtr<nsIAsyncInputStream>   mSocketIn;
  nsCOMPtr<nsIAsyncOutputStream>  mSocketOut;

  nsCOMPtr<nsITimer>              mCloseTimer;
  uint32_t                        mCloseTimeout;  /* milliseconds */

  nsCOMPtr<nsITimer>              mOpenTimer;
  uint32_t                        mOpenTimeout;  /* milliseconds */
  wsConnectingState               mConnecting;   /* 0 if not connecting */
  nsCOMPtr<nsITimer>              mReconnectDelayTimer;

  nsCOMPtr<nsITimer>              mPingTimer;

  nsCOMPtr<nsITimer>              mLingeringCloseTimer;
  const static int32_t            kLingeringCloseTimeout =   1000;
  const static int32_t            kLingeringCloseThreshold = 50;

  RefPtr<WebSocketEventService>   mService;

  int32_t                         mMaxConcurrentConnections;

  uint64_t                        mInnerWindowID;

  // following members are accessed only on the main thread
  uint32_t                        mGotUpgradeOK              : 1;
  uint32_t                        mRecvdHttpUpgradeTransport : 1;
  uint32_t                        mAutoFollowRedirects       : 1;
  uint32_t                        mAllowPMCE                 : 1;
  uint32_t                                                   : 0;

  // following members are accessed only on the socket thread
  uint32_t                        mPingOutstanding           : 1;
  uint32_t                        mReleaseOnTransmit         : 1;
  uint32_t                                                   : 0;

  Atomic<bool>                    mDataStarted;
  Atomic<bool>                    mRequestedClose;
  Atomic<bool>                    mClientClosed;
  Atomic<bool>                    mServerClosed;
  Atomic<bool>                    mStopped;
  Atomic<bool>                    mCalledOnStop;
  Atomic<bool>                    mTCPClosed;
  Atomic<bool>                    mOpenedHttpChannel;
  Atomic<bool>                    mIncrementedSessionCount;
  Atomic<bool>                    mDecrementedSessionCount;

  int32_t                         mMaxMessageSize;
  nsresult                        mStopOnClose;
  uint16_t                        mServerCloseCode;
  nsCString                       mServerCloseReason;
  uint16_t                        mScriptCloseCode;
  nsCString                       mScriptCloseReason;

  // These are for the read buffers
  const static uint32_t kIncomingBufferInitialSize = 16 * 1024;
  // We're ok with keeping a buffer this size or smaller around for the life of
  // the websocket.  If a particular message needs bigger than this we'll
  // increase the buffer temporarily, then drop back down to this size.
  const static uint32_t kIncomingBufferStableSize = 128 * 1024;

  uint8_t                        *mFramePtr;
  uint8_t                        *mBuffer;
  uint8_t                         mFragmentOpcode;
  uint32_t                        mFragmentAccumulator;
  uint32_t                        mBuffered;
  uint32_t                        mBufferSize;

  // These are for the send buffers
  const static int32_t kCopyBreak = 1000;

  OutboundMessage                *mCurrentOut;
  uint32_t                        mCurrentOutSent;
  nsDeque                         mOutgoingMessages;
  nsDeque                         mOutgoingPingMessages;
  nsDeque                         mOutgoingPongMessages;
  uint32_t                        mHdrOutToSend;
  uint8_t                        *mHdrOut;
  uint8_t                         mOutHeader[kCopyBreak + 16];
  nsAutoPtr<PMCECompression>      mPMCECompressor;
  uint32_t                        mDynamicOutputSize;
  uint8_t                        *mDynamicOutput;
  bool                            mPrivateBrowsing;

  nsCOMPtr<nsIDashboardEventNotifier> mConnectionLogService;

// These members are used for network per-app metering (bug 855949)
// Currently, they are only available on gonk.
  Atomic<uint64_t, Relaxed>       mCountRecv;
  Atomic<uint64_t, Relaxed>       mCountSent;
  uint32_t                        mAppId;
  bool                            mIsInIsolatedMozBrowser;
#ifdef MOZ_WIDGET_GONK
  nsMainThreadPtrHandle<nsINetworkInfo> mActiveNetworkInfo;
#endif
  nsresult                        SaveNetworkStats(bool);
  void                            CountRecvBytes(uint64_t recvBytes)
  {
    mCountRecv += recvBytes;
    SaveNetworkStats(false);
  }
  void                            CountSentBytes(uint64_t sentBytes)
  {
    mCountSent += sentBytes;
    SaveNetworkStats(false);
  }
};

class WebSocketSSLChannel : public WebSocketChannel
{
public:
    WebSocketSSLChannel() { BaseWebSocketChannel::mEncrypted = true; }
protected:
    virtual ~WebSocketSSLChannel() {}
};

} // namespace net
} // namespace mozilla

#endif // mozilla_net_WebSocketChannel_h
