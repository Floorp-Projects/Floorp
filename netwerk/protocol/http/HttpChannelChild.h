/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_HttpChannelChild_h
#define mozilla_net_HttpChannelChild_h

#include "mozilla/Mutex.h"
#include "mozilla/Telemetry.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/net/HttpBaseChannel.h"
#include "mozilla/net/NeckoTargetHolder.h"
#include "mozilla/net/PHttpChannelChild.h"
#include "mozilla/net/ChannelEventQueue.h"

#include "nsIStreamListener.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIProgressEventSink.h"
#include "nsICacheInfoChannel.h"
#include "nsIApplicationCache.h"
#include "nsIApplicationCacheChannel.h"
#include "nsIResumableChannel.h"
#include "nsIProxiedChannel.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsIChildChannel.h"
#include "nsIHttpChannelChild.h"
#include "nsIDivertableChannel.h"
#include "nsIMultiPartChannel.h"
#include "nsIThreadRetargetableRequest.h"
#include "mozilla/net/DNS.h"

using mozilla::Telemetry::LABELS_HTTP_CHILD_OMT_STATS;

class nsIEventTarget;
class nsInputStreamPump;
class nsIInterceptedBodyCallback;

#define HTTP_CHANNEL_CHILD_IID                       \
  {                                                  \
    0x321bd99e, 0x2242, 0x4dc6, {                    \
      0xbb, 0xec, 0xd5, 0x06, 0x29, 0x7c, 0x39, 0x83 \
    }                                                \
  }

namespace mozilla {
namespace net {

class HttpBackgroundChannelChild;
class InterceptedChannelContent;
class InterceptStreamListener;
class SyntheticDiversionListener;

class HttpChannelChild final : public PHttpChannelChild,
                               public HttpBaseChannel,
                               public HttpAsyncAborter<HttpChannelChild>,
                               public nsICacheInfoChannel,
                               public nsIProxiedChannel,
                               public nsIApplicationCacheChannel,
                               public nsIAsyncVerifyRedirectCallback,
                               public nsIChildChannel,
                               public nsIHttpChannelChild,
                               public nsIDivertableChannel,
                               public nsIMultiPartChannel,
                               public nsIThreadRetargetableRequest,
                               public NeckoTargetHolder {
  virtual ~HttpChannelChild();

 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSICACHEINFOCHANNEL
  NS_DECL_NSIPROXIEDCHANNEL
  NS_DECL_NSIAPPLICATIONCACHECONTAINER
  NS_DECL_NSIAPPLICATIONCACHECHANNEL
  NS_DECL_NSIASYNCVERIFYREDIRECTCALLBACK
  NS_DECL_NSICHILDCHANNEL
  NS_DECL_NSIHTTPCHANNELCHILD
  NS_DECL_NSIDIVERTABLECHANNEL
  NS_DECL_NSIMULTIPARTCHANNEL
  NS_DECL_NSITHREADRETARGETABLEREQUEST
  NS_DECLARE_STATIC_IID_ACCESSOR(HTTP_CHANNEL_CHILD_IID)

  HttpChannelChild();

  // Methods HttpBaseChannel didn't implement for us or that we override.
  //
  // nsIRequest
  NS_IMETHOD Cancel(nsresult status) override;
  NS_IMETHOD Suspend() override;
  NS_IMETHOD Resume() override;
  // nsIChannel
  NS_IMETHOD GetSecurityInfo(nsISupports** aSecurityInfo) override;
  NS_IMETHOD AsyncOpen(nsIStreamListener* aListener) override;

  // HttpBaseChannel::nsIHttpChannel
  NS_IMETHOD SetRequestHeader(const nsACString& aHeader,
                              const nsACString& aValue, bool aMerge) override;
  NS_IMETHOD SetEmptyRequestHeader(const nsACString& aHeader) override;
  NS_IMETHOD RedirectTo(nsIURI* newURI) override;
  NS_IMETHOD UpgradeToSecure() override;
  NS_IMETHOD GetProtocolVersion(nsACString& aProtocolVersion) override;
  void DoDiagnosticAssertWhenOnStopNotCalledOnDestroy() override;
  // nsIHttpChannelInternal
  NS_IMETHOD SetupFallbackChannel(const char* aFallbackKey) override;
  // nsISupportsPriority
  NS_IMETHOD SetPriority(int32_t value) override;
  // nsIClassOfService
  NS_IMETHOD SetClassFlags(uint32_t inFlags) override;
  NS_IMETHOD AddClassFlags(uint32_t inFlags) override;
  NS_IMETHOD ClearClassFlags(uint32_t inFlags) override;
  // nsIResumableChannel
  NS_IMETHOD ResumeAt(uint64_t startPos, const nsACString& entityID) override;

  nsresult SetReferrerHeader(const nsACString& aReferrer,
                             bool aRespectBeforeConnect) override;

  [[nodiscard]] bool IsSuspended();

  void FlushedForDiversion();

  void OnCopyComplete(nsresult aStatus) override;

  // Callback while background channel is ready.
  void OnBackgroundChildReady(HttpBackgroundChannelChild* aBgChild);
  // Callback while background channel is destroyed.
  void OnBackgroundChildDestroyed(HttpBackgroundChannelChild* aBgChild);

  nsresult CrossProcessRedirectFinished(nsresult aStatus);

 protected:
  mozilla::ipc::IPCResult RecvOnStartRequest(
      const nsresult& channelStatus, const nsHttpResponseHead& responseHead,
      const bool& useResponseHead, const nsHttpHeaderArray& requestHeaders,
      const ParentLoadInfoForwarderArgs& loadInfoForwarder,
      const bool& isFromCache, const bool& isRacing,
      const bool& cacheEntryAvailable, const uint64_t& cacheEntryId,
      const int32_t& cacheFetchCount, const uint32_t& cacheExpirationTime,
      const nsCString& cachedCharset,
      const nsCString& securityInfoSerialization, const NetAddr& selfAddr,
      const NetAddr& peerAddr, const int16_t& redirectCount,
      const uint32_t& cacheKey, const nsCString& altDataType,
      const int64_t& altDataLen, const bool& deliveringAltData,
      const bool& aApplyConversion, const bool& aIsResolvedByTRR,
      const ResourceTimingStructArgs& aTiming,
      const bool& aAllRedirectsSameOrigin, const Maybe<uint32_t>& aMultiPartID,
      const bool& aIsLastPartOfMultiPart,
      const nsILoadInfo::CrossOriginOpenerPolicy& aOpenerPolicy) override;
  mozilla::ipc::IPCResult RecvOnTransportAndData(
      const nsresult& aChannelStatus, const nsresult& aTransportStatus,
      const uint64_t& aOffset, const uint32_t& aCount,
      const nsCString& aData) override;

  mozilla::ipc::IPCResult RecvOnStopRequest(
      const nsresult& aChannelStatus, const ResourceTimingStructArgs& aTiming,
      const TimeStamp& aLastActiveTabOptHit,
      const nsHttpHeaderArray& aResponseTrailers,
      nsTArray<ConsoleReportCollected>&& aConsoleReports) override;
  mozilla::ipc::IPCResult RecvFailedAsyncOpen(const nsresult& status) override;
  mozilla::ipc::IPCResult RecvRedirect1Begin(
      const uint32_t& registrarId, const URIParams& newURI,
      const uint32_t& newLoadFlags, const uint32_t& redirectFlags,
      const ParentLoadInfoForwarderArgs& loadInfoForwarder,
      const nsHttpResponseHead& responseHead,
      const nsCString& securityInfoSerialization, const uint64_t& channelId,
      const NetAddr& oldPeerAddr,
      const ResourceTimingStructArgs& aTiming) override;
  mozilla::ipc::IPCResult RecvRedirect3Complete() override;
  mozilla::ipc::IPCResult RecvAssociateApplicationCache(
      const nsCString& groupID, const nsCString& clientID) override;
  mozilla::ipc::IPCResult RecvDeleteSelf() override;
  mozilla::ipc::IPCResult RecvFinishInterceptedRedirect() override;

  mozilla::ipc::IPCResult RecvReportSecurityMessage(
      const nsString& messageTag, const nsString& messageCategory) override;

  mozilla::ipc::IPCResult RecvIssueDeprecationWarning(
      const uint32_t& warning, const bool& asError) override;

  mozilla::ipc::IPCResult RecvSetPriority(const int16_t& aPriority) override;

  mozilla::ipc::IPCResult RecvAttachStreamFilter(
      Endpoint<extensions::PStreamFilterParent>&& aEndpoint) override;

  mozilla::ipc::IPCResult RecvCancelDiversion() override;

  mozilla::ipc::IPCResult RecvCancelRedirected() override;

  mozilla::ipc::IPCResult RecvOriginalCacheInputStreamAvailable(
      const Maybe<IPCStream>& aStream) override;

  mozilla::ipc::IPCResult RecvAltDataCacheInputStreamAvailable(
      const Maybe<IPCStream>& aStream) override;

  mozilla::ipc::IPCResult RecvOverrideReferrerInfoDuringBeginConnect(
      nsIReferrerInfo* aReferrerInfo) override;

  mozilla::ipc::IPCResult RecvNotifyClassificationFlags(
      const uint32_t& aClassificationFlags, const bool& aIsThirdParty) override;

  mozilla::ipc::IPCResult RecvNotifyFlashPluginStateChanged(
      const nsIHttpChannel::FlashPluginState& aState) override;

  mozilla::ipc::IPCResult RecvSetClassifierMatchedInfo(
      const ClassifierInfo& info) override;

  mozilla::ipc::IPCResult RecvSetClassifierMatchedTrackingInfo(
      const ClassifierInfo& info) override;

  mozilla::ipc::IPCResult RecvOnAfterLastPart(const nsresult& aStatus) override;

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual void DoNotifyListenerCleanup() override;

  virtual void DoAsyncAbort(nsresult aStatus) override;

  nsresult AsyncCall(
      void (HttpChannelChild::*funcPtr)(),
      nsRunnableMethod<HttpChannelChild>** retval = nullptr) override {
    // Normally, this method would just be implemented directly, but clang
    // miscompiles the corresponding non-virtual thunk on linux x86.
    // It however doesn't when going though a non-virtual method.
    // https://bugs.llvm.org/show_bug.cgi?id=38466
    return AsyncCallImpl(funcPtr, retval);
  };

  // Get event target for processing network events.
  already_AddRefed<nsIEventTarget> GetNeckoTarget() override;

  virtual mozilla::ipc::IPCResult RecvLogBlockedCORSRequest(
      const nsString& aMessage, const nsCString& aCategory) override;
  NS_IMETHOD LogBlockedCORSRequest(const nsAString& aMessage,
                                   const nsACString& aCategory) override;

  virtual mozilla::ipc::IPCResult RecvLogMimeTypeMismatch(
      const nsCString& aMessageName, const bool& aWarning, const nsString& aURL,
      const nsString& aContentType) override;
  NS_IMETHOD LogMimeTypeMismatch(const nsACString& aMessageName, bool aWarning,
                                 const nsAString& aURL,
                                 const nsAString& aContentType) override;

  mozilla::ipc::IPCResult RecvOnProgress(const int64_t& aProgress,
                                         const int64_t& aProgressMax) override;

  mozilla::ipc::IPCResult RecvOnStatus(const nsresult& aStatus) override;

 private:
  // We want to handle failure result of AsyncOpen, hence AsyncOpen calls the
  // Internal method
  nsresult AsyncOpenInternal(nsIStreamListener* aListener);

  nsresult AsyncCallImpl(void (HttpChannelChild::*funcPtr)(),
                         nsRunnableMethod<HttpChannelChild>** retval);

  class OverrideRunnable : public Runnable {
   public:
    OverrideRunnable(HttpChannelChild* aChannel, HttpChannelChild* aNewChannel,
                     InterceptStreamListener* aListener, nsIInputStream* aInput,
                     nsIInterceptedBodyCallback* aCallback,
                     UniquePtr<nsHttpResponseHead>&& aHead,
                     nsICacheInfoChannel* aCacheInfo);

    NS_IMETHOD Run() override;
    void OverrideWithSynthesizedResponse();

   private:
    RefPtr<HttpChannelChild> mChannel;
    RefPtr<HttpChannelChild> mNewChannel;
    RefPtr<InterceptStreamListener> mListener;
    nsCOMPtr<nsIInputStream> mInput;
    nsCOMPtr<nsIInterceptedBodyCallback> mCallback;
    UniquePtr<nsHttpResponseHead> mHead;
    nsCOMPtr<nsICacheInfoChannel> mSynthesizedCacheInfo;
  };

  // Sets the event target for future IPC messages. Messages will either be
  // directed to the TabGroup or DocGroup, depending on the LoadInfo associated
  // with the channel. Should be called when a new channel is being set up,
  // before the constructor message is sent to the parent.
  void SetEventTarget();

  // Get event target for ODA.
  already_AddRefed<nsIEventTarget> GetODATarget();

  [[nodiscard]] nsresult ContinueAsyncOpen();

  // Callbacks while receiving OnTransportAndData/OnStopRequest/OnProgress/
  // OnStatus/FlushedForDiversion/DivertMessages on background IPC channel.
  void ProcessOnTransportAndData(const nsresult& aChannelStatus,
                                 const nsresult& aStatus,
                                 const uint64_t& aOffset,
                                 const uint32_t& aCount,
                                 const nsCString& aData);
  void ProcessOnStopRequest(
      const nsresult& aChannelStatus, const ResourceTimingStructArgs& aTiming,
      const nsHttpHeaderArray& aResponseTrailers,
      const nsTArray<ConsoleReportCollected>& aConsoleReports);
  void ProcessFlushedForDiversion();
  void ProcessDivertMessages();

  // Return true if we need to tell the parent the size of unreported received
  // data
  bool NeedToReportBytesRead();
  int32_t mUnreportBytesRead = 0;

  void DoOnStartRequest(nsIRequest* aRequest, nsISupports* aContext);
  void DoOnStatus(nsIRequest* aRequest, nsresult status);
  void DoOnProgress(nsIRequest* aRequest, int64_t progress,
                    int64_t progressMax);
  void DoOnDataAvailable(nsIRequest* aRequest, nsISupports* aContext,
                         nsIInputStream* aStream, uint64_t offset,
                         uint32_t count);
  void DoPreOnStopRequest(nsresult aStatus);
  void DoOnStopRequest(nsIRequest* aRequest, nsresult aChannelStatus,
                       nsISupports* aContext);

  bool ShouldInterceptURI(nsIURI* aURI, bool& aShouldUpgrade);

  // Discard the prior interception and continue with the original network
  // request.
  void ResetInterception();

  // Override this channel's pending response with a synthesized one. The
  // content will be asynchronously read from the pump.
  void OverrideWithSynthesizedResponse(
      UniquePtr<nsHttpResponseHead>& aResponseHead,
      nsIInputStream* aSynthesizedInput,
      nsIInterceptedBodyCallback* aSynthesizedCallback,
      InterceptStreamListener* aStreamListener,
      nsICacheInfoChannel* aCacheInfoChannel);

  void ForceIntercepted(nsIInputStream* aSynthesizedInput,
                        nsIInterceptedBodyCallback* aSynthesizedCallback,
                        nsICacheInfoChannel* aCacheInfo);

  // Try send DeletingChannel message to parent side. Dispatch an async task to
  // main thread if invoking on non-main thread.
  void TrySendDeletingChannel();

  // Try invoke Cancel if on main thread, or prepend a CancelEvent in mEventQ to
  // ensure Cacnel is processed before any other channel events.
  void CancelOnMainThread(nsresult aRv);

  void MaybeCallSynthesizedCallback();

 private:
  // this section is for main-thread-only object
  // all the references need to be proxy released on main thread.
  nsCOMPtr<nsIChildChannel> mRedirectChannelChild;
  RefPtr<InterceptStreamListener> mInterceptListener;
  // Needed to call AsyncOpen in FinishInterceptedRedirect
  nsCOMPtr<nsIStreamListener> mInterceptedRedirectListener;

  // Proxy release all members above on main thread.
  void ReleaseMainThreadOnlyReferences();

 private:
  nsCString mCachedCharset;
  nsCString mProtocolVersion;

  RequestHeaderTuples mClientSetRequestHeaders;
  RefPtr<nsInputStreamPump> mSynthesizedResponsePump;
  nsCOMPtr<nsIInputStream> mSynthesizedInput;
  nsCOMPtr<nsIInterceptedBodyCallback> mSynthesizedCallback;
  nsCOMPtr<nsICacheInfoChannel> mSynthesizedCacheInfo;
  RefPtr<ChannelEventQueue> mEventQ;

  nsCOMPtr<nsIInputStreamReceiver> mOriginalInputStreamReceiver;
  nsCOMPtr<nsIInputStreamReceiver> mAltDataInputStreamReceiver;

  // Used to ensure atomicity of mBgChild and mBgInitFailCallback
  Mutex mBgChildMutex;

  // Associated HTTP background channel
  RefPtr<HttpBackgroundChannelChild> mBgChild;

  // Error handling procedure if failed to establish PBackground IPC
  nsCOMPtr<nsIRunnable> mBgInitFailCallback;

  // Remove the association with background channel after OnStopRequest
  // or AsyncAbort.
  void CleanupBackgroundChannel();

  // Needed to call CleanupRedirectingChannel in FinishInterceptedRedirect
  RefPtr<HttpChannelChild> mInterceptingChannel;
  // Used to call OverrideWithSynthesizedResponse in FinishInterceptedRedirect
  RefPtr<OverrideRunnable> mOverrideRunnable;

  // Target thread for delivering ODA.
  nsCOMPtr<nsIEventTarget> mODATarget;
  // Used to ensure atomicity of mNeckoTarget / mODATarget;
  Mutex mEventTargetMutex;

  // If nsUnknownDecoder is involved OnStartRequest call will be delayed and
  // this queue keeps OnDataAvailable data until OnStartRequest is finally
  // called.
  nsTArray<UniquePtr<ChannelEvent>> mUnknownDecoderEventQ;

  TimeStamp mLastStatusReported;

  int64_t mSynthesizedStreamLength;
  uint64_t mCacheEntryId;

  // The result of RetargetDeliveryTo for this channel.
  // |notRequested| represents OMT is not requested by the channel owner.
  LABELS_HTTP_CHILD_OMT_STATS mOMTResult =
      LABELS_HTTP_CHILD_OMT_STATS::notRequested;

  uint32_t mCacheKey;
  int32_t mCacheFetchCount;
  uint32_t mCacheExpirationTime;

  // If we're handling a multi-part response, then this is set to the current
  // part ID during OnStartRequest.
  Maybe<uint32_t> mMultiPartID;

  // To ensure only one SendDeletingChannel is triggered.
  Atomic<bool> mDeletingChannelSent;

  Atomic<bool, ReleaseAcquire> mUnknownDecoderInvolved;

  // Once set, OnData and possibly OnStop will be diverted to the parent.
  Atomic<bool, ReleaseAcquire> mDivertingToParent;
  // Once set, no OnStart/OnData/OnStop callbacks should be received from the
  // parent channel, nor dequeued from the ChannelEventQueue.
  Atomic<bool, ReleaseAcquire> mFlushedForDiversion;

  Atomic<bool, SequentiallyConsistent> mIsFromCache;
  Atomic<bool, SequentiallyConsistent> mIsRacing;
  // Set if we get the result and cache |mNeedToReportBytesRead|
  Atomic<bool, SequentiallyConsistent> mCacheNeedToReportBytesReadInitialized;
  // True if we need to tell the parent the size of unreported received data
  Atomic<bool, SequentiallyConsistent> mNeedToReportBytesRead;

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  bool mDoDiagnosticAssertWhenOnStopNotCalledOnDestroy = false;
  bool mAsyncOpenSucceeded = false;
  bool mSuccesfullyRedirected = false;
  Maybe<ActorDestroyReason> mActorDestroyReason;
#endif

  uint8_t mCacheEntryAvailable : 1;
  uint8_t mAltDataCacheEntryAvailable : 1;

  // If ResumeAt is called before AsyncOpen, we need to send extra data upstream
  uint8_t mSendResumeAt : 1;

  uint8_t mKeptAlive : 1;  // IPC kept open, but only for security info

  // Set when ActorDestroy(ActorDestroyReason::Deletion) is called
  // The channel must ignore any following OnStart/Stop/DataAvailable messages
  uint8_t mIPCActorDeleted : 1;

  // Set if SendSuspend is called. Determines if SendResume is needed when
  // diverting callbacks to parent.
  uint8_t mSuspendSent : 1;

  // Set if a response was synthesized, indicating that any forthcoming
  // redirects should be intercepted.
  uint8_t mSynthesizedResponse : 1;

  // Set if a synthesized response should cause us to explictly allows
  // intercepting an expected forthcoming redirect.
  uint8_t mShouldInterceptSubsequentRedirect : 1;
  // Set if a redirection is being initiated to facilitate providing a
  // synthesized response to a channel using a different principal than the
  // current one.
  uint8_t mRedirectingForSubsequentSynthesizedResponse : 1;

  // Set if a manual redirect mode channel needs to be intercepted in the
  // parent.
  uint8_t mPostRedirectChannelShouldIntercept : 1;
  // Set if a manual redirect mode channel needs to be upgraded to a secure URI
  // when it's being considered for interception.  Can only be true if
  // mPostRedirectChannelShouldIntercept is true.
  uint8_t mPostRedirectChannelShouldUpgrade : 1;

  // Set if the corresponding parent channel should force an interception to
  // occur before the network transaction is initiated.
  uint8_t mShouldParentIntercept : 1;

  // Set if the corresponding parent channel should suspend after a response
  // is synthesized.
  uint8_t mSuspendParentAfterSynthesizeResponse : 1;

  // True if this channel is a multi-part channel, and the last part
  // is currently being processed.
  uint8_t mIsLastPartOfMultiPart : 1;

  void FinishInterceptedRedirect();
  void CleanupRedirectingChannel(nsresult rv);

  // true after successful AsyncOpen until OnStopRequest completes.
  bool RemoteChannelExists() { return CanSend() && !mKeptAlive; }

  void AssociateApplicationCache(const nsCString& groupID,
                                 const nsCString& clientID);
  void OnStartRequest(
      const nsresult& channelStatus, const nsHttpResponseHead& responseHead,
      const bool& useResponseHead, const nsHttpHeaderArray& requestHeaders,
      const ParentLoadInfoForwarderArgs& loadInfoForwarder,
      const bool& isFromCache, const bool& isRacing,
      const bool& cacheEntryAvailable, const uint64_t& cacheEntryId,
      const int32_t& cacheFetchCount, const uint32_t& cacheExpirationTime,
      const nsCString& cachedCharset,
      const nsCString& securityInfoSerialization, const NetAddr& selfAddr,
      const NetAddr& peerAddr, const uint32_t& cacheKey,
      const nsCString& altDataType, const int64_t& altDataLen,
      const bool& deliveringAltData, const bool& aApplyConversion,
      const bool& aIsResolvedByTRR, const ResourceTimingStructArgs& aTiming,
      const bool& aAllRedirectsSameOrigin, const Maybe<uint32_t>& aMultiPartID,
      const bool& aIsLastPartOfMultiPart,
      const nsILoadInfo::CrossOriginOpenerPolicy& aOpenerPolicy);
  void MaybeDivertOnData(const nsCString& data, const uint64_t& offset,
                         const uint32_t& count);
  void OnTransportAndData(const nsresult& channelStatus, const nsresult& status,
                          const uint64_t& offset, const uint32_t& count,
                          const nsCString& data);
  void OnStopRequest(const nsresult& channelStatus,
                     const ResourceTimingStructArgs& timing,
                     const nsHttpHeaderArray& aResponseTrailers,
                     const nsTArray<ConsoleReportCollected>& aConsoleReports);
  void MaybeDivertOnStop(const nsresult& aChannelStatus);
  void FailedAsyncOpen(const nsresult& status);
  void HandleAsyncAbort();
  void Redirect1Begin(const uint32_t& registrarId, const URIParams& newUri,
                      const uint32_t& newLoadFlags,
                      const uint32_t& redirectFlags,
                      const ParentLoadInfoForwarderArgs& loadInfoForwarder,
                      const nsHttpResponseHead& responseHead,
                      const nsACString& securityInfoSerialization,
                      const uint64_t& channelId,
                      const ResourceTimingStructArgs& timing);
  bool Redirect3Complete(OverrideRunnable* aRunnable);
  void DeleteSelf();
  void DoNotifyListener();
  void ContinueDoNotifyListener();
  void OnAfterLastPart(const nsresult& aStatus);

  // Create a a new channel to be used in a redirection, based on the provided
  // response headers.
  [[nodiscard]] nsresult SetupRedirect(nsIURI* uri,
                                       const nsHttpResponseHead* responseHead,
                                       const uint32_t& redirectFlags,
                                       nsIChannel** outChannel);

  // Perform a redirection without communicating with the parent process at all.
  void BeginNonIPCRedirect(nsIURI* responseURI,
                           const nsHttpResponseHead* responseHead,
                           bool responseRedirected);

  // Override the default security info pointer during a non-IPC redirection.
  void OverrideSecurityInfoForNonIPCRedirect(nsISupports* securityInfo);

  // Collect telemetry for the successful rate of OMT.
  void CollectOMTTelemetry();

  friend class HttpAsyncAborter<HttpChannelChild>;
  friend class InterceptStreamListener;
  friend class InterceptedChannelContent;
  friend class HttpBackgroundChannelChild;
  friend class NeckoTargetChannelFunctionEvent;
};

NS_DEFINE_STATIC_IID_ACCESSOR(HttpChannelChild, HTTP_CHANNEL_CHILD_IID)

// A stream listener interposed between the nsInputStreamPump used for
// intercepted channels and this channel's original listener. This is only used
// to ensure the original listener sees the channel as the request object, and
// to synthesize OnStatus and OnProgress notifications.
class InterceptStreamListener : public nsIStreamListener,
                                public nsIProgressEventSink {
  RefPtr<HttpChannelChild> mOwner;
  nsCOMPtr<nsISupports> mContext;
  virtual ~InterceptStreamListener() = default;

 public:
  InterceptStreamListener(HttpChannelChild* aOwner, nsISupports* aContext)
      : mOwner(aOwner), mContext(aContext) {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIPROGRESSEVENTSINK

  void Cleanup();
};

//-----------------------------------------------------------------------------
// inline functions
//-----------------------------------------------------------------------------

inline bool HttpChannelChild::IsSuspended() { return mSuspendCount != 0; }

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_HttpChannelChild_h
