/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_WebSocketChannel_h
#define mozilla_net_WebSocketChannel_h

#include "nsIURI.h"
#include "nsISupports.h"
#include "nsIInterfaceRequestor.h"
#include "nsIEventTarget.h"
#include "nsIStreamListener.h"
#include "nsIProtocolHandler.h"
#include "nsISocketTransport.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsILoadGroup.h"
#include "nsITimer.h"
#include "nsIDNSListener.h"
#include "nsIHttpChannel.h"
#include "nsIChannelEventSink.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsIStringStream.h"
#include "nsIHttpChannelInternal.h"
#include "nsIRandomGenerator.h"
#include "BaseWebSocketChannel.h"
#include "nsIDashboardEventNotifier.h"

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsDeque.h"

namespace mozilla { namespace net {

class OutboundMessage;
class OutboundEnqueuer;
class nsWSAdmissionManager;
class nsWSCompression;
class CallOnMessageAvailable;
class CallOnStop;
class CallOnServerClose;
class CallAcknowledge;

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
                         public nsIInterfaceRequestor,
                         public nsIChannelEventSink
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIHTTPUPGRADELISTENER
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIINPUTSTREAMCALLBACK
  NS_DECL_NSIOUTPUTSTREAMCALLBACK
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSIDNSLISTENER
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSICHANNELEVENTSINK

  // nsIWebSocketChannel methods BaseWebSocketChannel didn't implement for us
  //
  NS_IMETHOD AsyncOpen(nsIURI *aURI,
                       const nsACString &aOrigin,
                       nsIWebSocketListener *aListener,
                       nsISupports *aContext);
  NS_IMETHOD Close(uint16_t aCode, const nsACString & aReason);
  NS_IMETHOD SendMsg(const nsACString &aMsg);
  NS_IMETHOD SendBinaryMsg(const nsACString &aMsg);
  NS_IMETHOD SendBinaryStream(nsIInputStream *aStream, uint32_t length);
  NS_IMETHOD GetSecurityInfo(nsISupports **aSecurityInfo);

  WebSocketChannel();
  static void Shutdown();

  enum {
    // Non Control Frames
    kContinuation = 0x0,
    kText =         0x1,
    kBinary =       0x2,

    // Control Frames
    kClose =        0x8,
    kPing =         0x9,
    kPong =         0xA
  };

  const static uint32_t kControlFrameMask   = 0x8;
  const static uint8_t kMaskBit             = 0x80;
  const static uint8_t kFinalFragBit        = 0x80;

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
                         uint32_t length, nsIInputStream *aStream = NULL);

  void EnqueueOutgoingMessage(nsDeque &aQueue, OutboundMessage *aMsg);

  void PrimeNewOutgoingMessage();
  void DeleteCurrentOutGoingMessage();
  void GeneratePong(uint8_t *payload, uint32_t len);
  void GeneratePing();

  void     BeginOpen();
  nsresult HandleExtensions();
  nsresult SetupRequest();
  nsresult ApplyForAdmission();
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
  void ApplyMask(uint32_t mask, uint8_t *data, uint64_t len);

  bool     IsPersistentFramePtr();
  nsresult ProcessInput(uint8_t *buffer, uint32_t count);
  bool UpdateReadBuffer(uint8_t *buffer, uint32_t count,
                        uint32_t accumulatedFragments,
                        uint32_t *available);

  inline void ResetPingTimer()
  {
    if (mPingTimer) {
      mPingOutstanding = 0;
      mPingTimer->SetDelay(mPingInterval);
    }
  }

  nsCOMPtr<nsIEventTarget>                 mSocketThread;
  nsCOMPtr<nsIHttpChannelInternal>         mChannel;
  nsCOMPtr<nsIHttpChannel>                 mHttpChannel;
  nsCOMPtr<nsICancelable>                  mDNSRequest;
  nsCOMPtr<nsIAsyncVerifyRedirectCallback> mRedirectCallback;
  nsCOMPtr<nsIRandomGenerator>             mRandomGenerator;

  nsCString                       mHashedSecret;

  // Used as key for connection managment: Initially set to hostname from URI,
  // then to IP address (unless we're leaving DNS resolution to a proxy server)
  nsCString                       mAddress;
  int32_t                         mPort;          // WS server port

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

  int32_t                         mMaxConcurrentConnections;

  uint32_t                        mGotUpgradeOK              : 1;
  uint32_t                        mRecvdHttpUpgradeTransport : 1;
  uint32_t                        mRequestedClose            : 1;
  uint32_t                        mClientClosed              : 1;
  uint32_t                        mServerClosed              : 1;
  uint32_t                        mStopped                   : 1;
  uint32_t                        mCalledOnStop              : 1;
  uint32_t                        mPingOutstanding           : 1;
  uint32_t                        mAllowCompression          : 1;
  uint32_t                        mAutoFollowRedirects       : 1;
  uint32_t                        mReleaseOnTransmit         : 1;
  uint32_t                        mTCPClosed                 : 1;
  uint32_t                        mOpenedHttpChannel         : 1;
  uint32_t                        mDataStarted               : 1;
  uint32_t                        mIncrementedSessionCount   : 1;
  uint32_t                        mDecrementedSessionCount   : 1;

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
  nsCOMPtr<nsIStreamListener>     mInflateReader;
  nsCOMPtr<nsIStringInputStream>  mInflateStream;

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
  nsWSCompression                *mCompressor;
  uint32_t                        mDynamicOutputSize;
  uint8_t                        *mDynamicOutput;

  nsCOMPtr<nsIDashboardEventNotifier> mConnectionLogService;
  uint32_t mSerial;
  static uint32_t sSerialSeed;
};

class WebSocketSSLChannel : public WebSocketChannel
{
public:
    WebSocketSSLChannel() { BaseWebSocketChannel::mEncrypted = true; }
protected:
    virtual ~WebSocketSSLChannel() {}
};

}} // namespace mozilla::net

#endif // mozilla_net_WebSocketChannel_h
