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
#include "mozilla/extensions/StreamFilterParent.h"
#include "mozilla/net/HttpBaseChannel.h"
#include "mozilla/net/NeckoTargetHolder.h"
#include "mozilla/net/PHttpChannelChild.h"
#include "mozilla/net/ChannelEventQueue.h"

#include "nsIStreamListener.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIProgressEventSink.h"
#include "nsICacheEntry.h"
#include "nsICacheInfoChannel.h"
#include "nsIResumableChannel.h"
#include "nsIProxiedChannel.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsIChildChannel.h"
#include "nsIHttpChannelChild.h"
#include "nsIMultiPartChannel.h"
#include "nsIThreadRetargetableRequest.h"
#include "mozilla/net/DNS.h"

using mozilla::Telemetry::LABELS_HTTP_CHILD_OMT_STATS;

class nsIEventTarget;
class nsIInterceptedBodyCallback;
class nsISerialEventTarget;
class nsITransportSecurityInfo;
class nsInputStreamPump;

#define HTTP_CHANNEL_CHILD_IID                       \
  {                                                  \
    0x321bd99e, 0x2242, 0x4dc6, {                    \
      0xbb, 0xec, 0xd5, 0x06, 0x29, 0x7c, 0x39, 0x83 \
    }                                                \
  }

namespace mozilla::net {

class HttpBackgroundChannelChild;

class HttpChannelChild final : public PHttpChannelChild,
                               public HttpBaseChannel,
                               public HttpAsyncAborter<HttpChannelChild>,
                               public nsICacheInfoChannel,
                               public nsIProxiedChannel,
                               public nsIAsyncVerifyRedirectCallback,
                               public nsIChildChannel,
                               public nsIHttpChannelChild,
                               public nsIMultiPartChannel,
                               public nsIThreadRetargetableRequest,
                               public NeckoTargetHolder {
  virtual ~HttpChannelChild();

 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSICACHEINFOCHANNEL
  NS_DECL_NSIPROXIEDCHANNEL
  NS_DECL_NSIASYNCVERIFYREDIRECTCALLBACK
  NS_DECL_NSICHILDCHANNEL
  NS_DECL_NSIHTTPCHANNELCHILD
  NS_DECL_NSIMULTIPARTCHANNEL
  NS_DECL_NSITHREADRETARGETABLEREQUEST
  NS_DECLARE_STATIC_IID_ACCESSOR(HTTP_CHANNEL_CHILD_IID)

  HttpChannelChild();

  // Methods HttpBaseChannel didn't implement for us or that we override.
  //
  // nsIRequest
  NS_IMETHOD SetCanceledReason(const nsACString& aReason) override;
  NS_IMETHOD GetCanceledReason(nsACString& aReason) override;
  NS_IMETHOD CancelWithReason(nsresult status,
                              const nsACString& reason) override;
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
  NS_IMETHOD GetIsAuthChannel(bool* aIsAuthChannel) override;
  NS_IMETHOD SetEarlyHintObserver(nsIEarlyHintObserver* aObserver) override;
  // nsISupportsPriority
  NS_IMETHOD SetPriority(int32_t value) override;
  // nsIClassOfService
  NS_IMETHOD SetClassFlags(uint32_t inFlags) override;
  NS_IMETHOD AddClassFlags(uint32_t inFlags) override;
  NS_IMETHOD ClearClassFlags(uint32_t inFlags) override;
  NS_IMETHOD SetClassOfService(ClassOfService inCos) override;
  NS_IMETHOD SetIncremental(bool inIncremental) override;
  // nsIResumableChannel
  NS_IMETHOD ResumeAt(uint64_t startPos, const nsACString& entityID) override;

  nsresult SetReferrerHeader(const nsACString& aReferrer,
                             bool aRespectBeforeConnect) override;

  [[nodiscard]] bool IsSuspended();

  // Callback while background channel is ready.
  void OnBackgroundChildReady(HttpBackgroundChannelChild* aBgChild);
  // Callback while background channel is destroyed.
  void OnBackgroundChildDestroyed(HttpBackgroundChannelChild* aBgChild);

  nsresult CrossProcessRedirectFinished(nsresult aStatus);

 protected:
  mozilla::ipc::IPCResult RecvOnStartRequestSent() override;
  mozilla::ipc::IPCResult RecvFailedAsyncOpen(const nsresult& status) override;
  mozilla::ipc::IPCResult RecvRedirect1Begin(
      const uint32_t& registrarId, nsIURI* newOriginalURI,
      const uint32_t& newLoadFlags, const uint32_t& redirectFlags,
      const ParentLoadInfoForwarderArgs& loadInfoForwarder,
      const nsHttpResponseHead& responseHead,
      nsITransportSecurityInfo* securityInfo, const uint64_t& channelId,
      const NetAddr& oldPeerAddr,
      const ResourceTimingStructArgs& aTiming) override;
  mozilla::ipc::IPCResult RecvRedirect3Complete() override;
  mozilla::ipc::IPCResult RecvDeleteSelf() override;

  mozilla::ipc::IPCResult RecvReportSecurityMessage(
      const nsAString& messageTag, const nsAString& messageCategory) override;

  mozilla::ipc::IPCResult RecvSetPriority(const int16_t& aPriority) override;

  mozilla::ipc::IPCResult RecvOriginalCacheInputStreamAvailable(
      const Maybe<IPCStream>& aStream) override;

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
  already_AddRefed<nsISerialEventTarget> GetNeckoTarget() override;

  virtual mozilla::ipc::IPCResult RecvLogBlockedCORSRequest(
      const nsAString& aMessage, const nsACString& aCategory) override;
  NS_IMETHOD LogBlockedCORSRequest(const nsAString& aMessage,
                                   const nsACString& aCategory) override;

  virtual mozilla::ipc::IPCResult RecvLogMimeTypeMismatch(
      const nsACString& aMessageName, const bool& aWarning,
      const nsAString& aURL, const nsAString& aContentType) override;
  NS_IMETHOD LogMimeTypeMismatch(const nsACString& aMessageName, bool aWarning,
                                 const nsAString& aURL,
                                 const nsAString& aContentType) override;

 private:
  // We want to handle failure result of AsyncOpen, hence AsyncOpen calls the
  // Internal method
  nsresult AsyncOpenInternal(nsIStreamListener* aListener);

  nsresult AsyncCallImpl(void (HttpChannelChild::*funcPtr)(),
                         nsRunnableMethod<HttpChannelChild>** retval);

  // Sets the event target for future IPC messages. Messages will either be
  // directed to the TabGroup or DocGroup, depending on the LoadInfo associated
  // with the channel. Should be called when a new channel is being set up,
  // before the constructor message is sent to the parent.
  void SetEventTarget();

  // Get event target for ODA.
  already_AddRefed<nsIEventTarget> GetODATarget();

  [[nodiscard]] nsresult ContinueAsyncOpen();
  void ProcessOnStartRequest(const nsHttpResponseHead& aResponseHead,
                             const bool& aUseResponseHead,
                             const nsHttpHeaderArray& aRequestHeaders,
                             const HttpChannelOnStartRequestArgs& aArgs,
                             const HttpChannelAltDataStream& aAltData);

  // Callbacks while receiving OnTransportAndData/OnStopRequest/OnProgress/
  // OnStatus/FlushedForDiversion/DivertMessages on background IPC channel.
  void ProcessOnTransportAndData(const nsresult& aChannelStatus,
                                 const nsresult& aTransportStatus,
                                 const uint64_t& aOffset,
                                 const uint32_t& aCount,
                                 const nsACString& aData);
  void ProcessOnStopRequest(const nsresult& aChannelStatus,
                            const ResourceTimingStructArgs& aTiming,
                            const nsHttpHeaderArray& aResponseTrailers,
                            nsTArray<ConsoleReportCollected>&& aConsoleReports,
                            bool aFromSocketProcess);
  void ProcessOnConsoleReport(
      nsTArray<ConsoleReportCollected>&& aConsoleReports);

  void ProcessNotifyClassificationFlags(uint32_t aClassificationFlags,
                                        bool aIsThirdParty);
  void ProcessSetClassifierMatchedInfo(const nsACString& aList,
                                       const nsACString& aProvider,
                                       const nsACString& aFullHash);
  void ProcessSetClassifierMatchedTrackingInfo(const nsACString& aLists,
                                               const nsACString& aFullHashes);
  void ProcessOnAfterLastPart(const nsresult& aStatus);
  void ProcessOnProgress(const int64_t& aProgress, const int64_t& aProgressMax);

  void ProcessOnStatus(const nsresult& aStatus);

  void ProcessAttachStreamFilter(
      Endpoint<extensions::PStreamFilterParent>&& aEndpoint);

  // Return true if we need to tell the parent the size of unreported received
  // data
  bool NeedToReportBytesRead();
  int32_t mUnreportBytesRead = 0;

  void DoOnConsoleReport(nsTArray<ConsoleReportCollected>&& aConsoleReports);
  void DoOnStartRequest(nsIRequest* aRequest);
  void DoOnStatus(nsIRequest* aRequest, nsresult status);
  void DoOnProgress(nsIRequest* aRequest, int64_t progress,
                    int64_t progressMax);
  void DoOnDataAvailable(nsIRequest* aRequest, nsIInputStream* aStream,
                         uint64_t offset, uint32_t count);
  void DoPreOnStopRequest(nsresult aStatus);
  void DoOnStopRequest(nsIRequest* aRequest, nsresult aChannelStatus);
  void ContinueOnStopRequest();

  // Try send DeletingChannel message to parent side. Dispatch an async task to
  // main thread if invoking on non-main thread.
  void TrySendDeletingChannel();

  // Try invoke Cancel if on main thread, or prepend a CancelEvent in mEventQ to
  // ensure Cacnel is processed before any other channel events.
  void CancelOnMainThread(nsresult aRv, const nsACString& aReason);

  nsresult MaybeLogCOEPError(nsresult aStatus);

 private:
  // this section is for main-thread-only object
  // all the references need to be proxy released on main thread.
  nsCOMPtr<nsIChildChannel> mRedirectChannelChild;

  // Proxy release all members above on main thread.
  void ReleaseMainThreadOnlyReferences();

 private:
  nsCString mProtocolVersion;

  RequestHeaderTuples mClientSetRequestHeaders;
  RefPtr<ChannelEventQueue> mEventQ;

  nsCOMPtr<nsIInputStreamReceiver> mOriginalInputStreamReceiver;
  nsCOMPtr<nsIInputStream> mAltDataInputStream;

  // Used to ensure atomicity of mBgChild and mBgInitFailCallback
  Mutex mBgChildMutex{"HttpChannelChild::BgChildMutex"};

  // Associated HTTP background channel
  RefPtr<HttpBackgroundChannelChild> mBgChild MOZ_GUARDED_BY(mBgChildMutex);

  // Error handling procedure if failed to establish PBackground IPC
  nsCOMPtr<nsIRunnable> mBgInitFailCallback MOZ_GUARDED_BY(mBgChildMutex);

  // Remove the association with background channel after OnStopRequest
  // or AsyncAbort.
  void CleanupBackgroundChannel();

  // Target thread for delivering ODA.
  nsCOMPtr<nsIEventTarget> mODATarget MOZ_GUARDED_BY(mEventTargetMutex);
  // Used to ensure atomicity of mNeckoTarget / mODATarget;
  Mutex mEventTargetMutex{"HttpChannelChild::EventTargetMutex"};

  TimeStamp mLastStatusReported;

  uint64_t mCacheEntryId{0};

  // The result of RetargetDeliveryTo for this channel.
  // |notRequested| represents OMT is not requested by the channel owner.
  LABELS_HTTP_CHILD_OMT_STATS mOMTResult =
      LABELS_HTTP_CHILD_OMT_STATS::notRequested;

  uint32_t mCacheKey{0};
  int32_t mCacheFetchCount{0};
  uint32_t mCacheExpirationTime{
      static_cast<uint32_t>(nsICacheEntry::NO_EXPIRATION_TIME)};

  // If we're handling a multi-part response, then this is set to the current
  // part ID during OnStartRequest.
  Maybe<uint32_t> mMultiPartID;

  // To ensure only one SendDeletingChannel is triggered.
  Atomic<bool> mDeletingChannelSent{false};

  Atomic<bool, SequentiallyConsistent> mIsFromCache{false};
  Atomic<bool, SequentiallyConsistent> mIsRacing{false};
  // Set if we get the result and cache |mNeedToReportBytesRead|
  Atomic<bool, SequentiallyConsistent> mCacheNeedToReportBytesReadInitialized{
      false};
  // True if we need to tell the parent the size of unreported received data
  Atomic<bool, SequentiallyConsistent> mNeedToReportBytesRead{true};

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  bool mDoDiagnosticAssertWhenOnStopNotCalledOnDestroy = false;
  bool mAsyncOpenSucceeded = false;
  bool mSuccesfullyRedirected = false;
  bool mRemoteChannelExistedAtCancel = false;
  bool mEverHadBgChildAtAsyncOpen = false;
  bool mEverHadBgChildAtConnectParent = false;
  bool mCreateBackgroundChannelFailed = false;
  bool mBgInitFailCallbackTriggered = false;
  bool mCanSendAtCancel = false;
  // State of the HttpBackgroundChannelChild's event queue during destruction.
  enum BckChildQueueStatus {
    // BckChild never told us
    BCKCHILD_UNKNOWN,
    // BckChild was empty at the time of destruction
    BCKCHILD_EMPTY,
    // BckChild was keeping events in the queue at the destruction time!
    BCKCHILD_NON_EMPTY
  };
  Atomic<BckChildQueueStatus> mBackgroundChildQueueFinalState{BCKCHILD_UNKNOWN};
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

  // True if this channel is a multi-part channel, and the first part
  // is currently being processed.
  uint8_t mIsFirstPartOfMultiPart : 1;

  // True if this channel is a multi-part channel, and the last part
  // is currently being processed.
  uint8_t mIsLastPartOfMultiPart : 1;

  // True if this channel is suspended by ConnectParent and not resumed by
  // CompleteRedirectSetup/RecvDeleteSelf.
  uint8_t mSuspendForWaitCompleteRedirectSetup : 1;

  // True if RecvOnStartRequestSent was received.
  uint8_t mRecvOnStartRequestSentCalled : 1;

  // True if this channel is for a document and suspended by waiting for
  // permission or cookie. That is, RecvOnStartRequestSent is received.
  uint8_t mSuspendedByWaitingForPermissionCookie : 1;

  void CleanupRedirectingChannel(nsresult rv);

  // Calls OnStartRequest and/or OnStopRequest on our listener in case we didn't
  // do that so far.  If we already did, it will just release references to
  // cleanup.
  void NotifyOrReleaseListeners(nsresult rv);

  // true after successful AsyncOpen until OnStopRequest completes.
  bool RemoteChannelExists() { return CanSend() && !mKeptAlive; }

  void OnStartRequest(const nsHttpResponseHead& aResponseHead,
                      const bool& aUseResponseHead,
                      const nsHttpHeaderArray& aRequestHeaders,
                      const HttpChannelOnStartRequestArgs& aArgs);
  void OnTransportAndData(const nsresult& channelStatus, const nsresult& status,
                          const uint64_t& offset, const uint32_t& count,
                          const nsACString& data);
  void OnStopRequest(const nsresult& channelStatus,
                     const ResourceTimingStructArgs& timing,
                     const nsHttpHeaderArray& aResponseTrailers);
  void FailedAsyncOpen(const nsresult& status);
  void HandleAsyncAbort();
  void Redirect1Begin(const uint32_t& registrarId, nsIURI* newOriginalURI,
                      const uint32_t& newLoadFlags,
                      const uint32_t& redirectFlags,
                      const ParentLoadInfoForwarderArgs& loadInfoForwarder,
                      const nsHttpResponseHead& responseHead,
                      nsITransportSecurityInfo* securityInfo,
                      const uint64_t& channelId,
                      const ResourceTimingStructArgs& timing);
  void Redirect3Complete();
  void DeleteSelf();
  void DoNotifyListener();
  void ContinueDoNotifyListener();
  void OnAfterLastPart(const nsresult& aStatus);
  void MaybeConnectToSocketProcess();

  // Create a a new channel to be used in a redirection, based on the provided
  // response headers.
  [[nodiscard]] nsresult SetupRedirect(nsIURI* uri,
                                       const nsHttpResponseHead* responseHead,
                                       const uint32_t& redirectFlags,
                                       nsIChannel** outChannel);

  // Collect telemetry for the successful rate of OMT.
  void CollectOMTTelemetry();

  friend class HttpAsyncAborter<HttpChannelChild>;
  friend class InterceptStreamListener;
  friend class InterceptedChannelContent;
  friend class HttpBackgroundChannelChild;
  friend class NeckoTargetChannelFunctionEvent;
};

NS_DEFINE_STATIC_IID_ACCESSOR(HttpChannelChild, HTTP_CHANNEL_CHILD_IID)

//-----------------------------------------------------------------------------
// inline functions
//-----------------------------------------------------------------------------

inline bool HttpChannelChild::IsSuspended() { return mSuspendCount != 0; }

}  // namespace mozilla::net

#endif  // mozilla_net_HttpChannelChild_h
