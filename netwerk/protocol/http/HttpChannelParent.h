/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_HttpChannelParent_h
#define mozilla_net_HttpChannelParent_h

#include "ADivertableParentChannel.h"
#include "nsHttp.h"
#include "mozilla/net/PHttpChannelParent.h"
#include "mozilla/net/NeckoCommon.h"
#include "mozilla/net/NeckoParent.h"
#include "mozilla/MozPromise.h"
#include "nsIParentRedirectingChannel.h"
#include "nsIProgressEventSink.h"
#include "nsIChannelEventSink.h"
#include "nsIRedirectResultListener.h"
#include "nsHttpChannel.h"
#include "nsIAuthPromptProvider.h"
#include "mozilla/dom/ipc/IdType.h"
#include "nsIDeprecationWarner.h"
#include "nsIMultiPartChannel.h"

class nsICacheEntry;

#define HTTP_CHANNEL_PARENT_IID                      \
  {                                                  \
    0x982b2372, 0x7aa5, 0x4e8a, {                    \
      0xbd, 0x9f, 0x89, 0x74, 0xd7, 0xf0, 0x58, 0xeb \
    }                                                \
  }

namespace mozilla {

namespace dom {
class BrowserParent;
class PBrowserOrId;
}  // namespace dom

namespace net {

class HttpBackgroundChannelParent;
class ParentChannelListener;
class ChannelEventQueue;

// Note: nsIInterfaceRequestor must be the first base so that do_QueryObject()
// works correctly on this object, as it's needed to compute a void* pointing to
// the beginning of this object.

class HttpChannelParent final : public nsIInterfaceRequestor,
                                public PHttpChannelParent,
                                public nsIParentRedirectingChannel,
                                public nsIProgressEventSink,
                                public ADivertableParentChannel,
                                public nsIAuthPromptProvider,
                                public nsIDeprecationWarner,
                                public HttpChannelSecurityWarningReporter,
                                public nsIAsyncVerifyRedirectReadyCallback,
                                public nsIChannelEventSink,
                                public nsIRedirectResultListener,
                                public nsIMultiPartChannelListener {
  virtual ~HttpChannelParent();

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIPARENTCHANNEL
  NS_DECL_NSIPARENTREDIRECTINGCHANNEL
  NS_DECL_NSIPROGRESSEVENTSINK
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSIAUTHPROMPTPROVIDER
  NS_DECL_NSIDEPRECATIONWARNER
  NS_DECL_NSIASYNCVERIFYREDIRECTREADYCALLBACK
  NS_DECL_NSICHANNELEVENTSINK
  NS_DECL_NSIREDIRECTRESULTLISTENER
  NS_DECL_NSIMULTIPARTCHANNELLISTENER

  NS_DECLARE_STATIC_IID_ACCESSOR(HTTP_CHANNEL_PARENT_IID)

  HttpChannelParent(const dom::PBrowserOrId& iframeEmbedding,
                    nsILoadContext* aLoadContext, PBOverrideStatus aStatus);

  [[nodiscard]] bool Init(const HttpChannelCreationArgs& aOpenArgs);

  // ADivertableParentChannel functions.
  void DivertTo(nsIStreamListener* aListener) override;
  [[nodiscard]] nsresult SuspendForDiversion() override;
  [[nodiscard]] nsresult SuspendMessageDiversion() override;
  [[nodiscard]] nsresult ResumeMessageDiversion() override;
  [[nodiscard]] nsresult CancelDiversion() override;

  // Calls OnStartRequest for "DivertTo" listener, then notifies child channel
  // that it should divert OnDataAvailable and OnStopRequest calls to this
  // parent channel.
  void StartDiversion();

  // Handles calling OnStart/Stop if there are errors during diversion.
  // Called asynchronously from FailDiversion.
  void NotifyDiversionFailed(nsresult aErrorCode);

  // Forwarded to nsHttpChannel::SetApplyConversion.
  void SetApplyConversion(bool aApplyConversion) {
    if (mChannel) {
      mChannel->SetApplyConversion(aApplyConversion);
    }
  }

  [[nodiscard]] nsresult OpenAlternativeOutputStream(
      const nsACString& type, int64_t predictedSize,
      nsIAsyncOutputStream** _retval);

  // Callbacks for each asynchronous tasks required in AsyncOpen
  // procedure, will call InvokeAsyncOpen when all the expected
  // tasks is finished successfully or when any failure happened.
  // @see mAsyncOpenBarrier.
  void TryInvokeAsyncOpen(nsresult aRv);

  void InvokeAsyncOpen(nsresult rv);

  // Calls SendSetPriority if mIPCClosed is false.
  void DoSendSetPriority(int16_t aValue);

  // Callback while background channel is ready.
  void OnBackgroundParentReady(HttpBackgroundChannelParent* aBgParent);
  // Callback while background channel is destroyed.
  void OnBackgroundParentDestroyed();

  base::ProcessId OtherPid() const;

  // Inform the child actor that our referrer info was modified late during
  // BeginConnect.
  void OverrideReferrerInfoDuringBeginConnect(nsIReferrerInfo* aReferrerInfo);

 protected:
  // used to connect redirected-to channel in parent with just created
  // ChildChannel.  Used during redirects.
  [[nodiscard]] bool ConnectChannel(const uint32_t& channelId,
                                    const bool& shouldIntercept);

  [[nodiscard]] bool DoAsyncOpen(
      const URIParams& uri, const Maybe<URIParams>& originalUri,
      const Maybe<URIParams>& docUri, nsIReferrerInfo* aReferrerInfo,
      const Maybe<URIParams>& internalRedirectUri,
      const Maybe<URIParams>& topWindowUri, const uint32_t& loadFlags,
      const RequestHeaderTuples& requestHeaders, const nsCString& requestMethod,
      const Maybe<IPCStream>& uploadStream, const bool& uploadStreamHasHeaders,
      const int16_t& priority, const uint32_t& classOfService,
      const uint8_t& redirectionLimit, const bool& allowSTS,
      const uint32_t& thirdPartyFlags, const bool& doResumeAt,
      const uint64_t& startPos, const nsCString& entityID,
      const bool& chooseApplicationCache, const nsCString& appCacheClientID,
      const bool& allowSpdy, const bool& allowAltSvc,
      const bool& beConservative, const uint32_t& tlsFlags,
      const Maybe<LoadInfoArgs>& aLoadInfoArgs,
      const Maybe<nsHttpResponseHead>& aSynthesizedResponseHead,
      const nsCString& aSecurityInfoSerialization, const uint32_t& aCacheKey,
      const uint64_t& aRequestContextID,
      const Maybe<CorsPreflightArgs>& aCorsPreflightArgs,
      const uint32_t& aInitialRwin, const bool& aBlockAuthPrompt,
      const bool& aSuspendAfterSynthesizeResponse,
      const bool& aAllowStaleCacheContent,
      const bool& aPreferCacheLoadOverBypass, const nsCString& aContentTypeHint,
      const uint32_t& aCorsMode, const uint32_t& aRedirectMode,
      const uint64_t& aChannelId, const nsString& aIntegrityMetadata,
      const uint64_t& aContentWindowId,
      const nsTArray<PreferredAlternativeDataTypeParams>&
          aPreferredAlternativeTypes,
      const uint64_t& aTopLevelOuterContentWindowId,
      const TimeStamp& aLaunchServiceWorkerStart,
      const TimeStamp& aLaunchServiceWorkerEnd,
      const TimeStamp& aDispatchFetchEventStart,
      const TimeStamp& aDispatchFetchEventEnd,
      const TimeStamp& aHandleFetchEventStart,
      const TimeStamp& aHandleFetchEventEnd,
      const bool& aForceMainDocumentChannel,
      const TimeStamp& aNavigationStartTimeStamp,
      const bool& hasNonEmptySandboxingFlag);

  virtual mozilla::ipc::IPCResult RecvSetPriority(
      const int16_t& priority) override;
  virtual mozilla::ipc::IPCResult RecvSetClassOfService(
      const uint32_t& cos) override;
  virtual mozilla::ipc::IPCResult RecvSetCacheTokenCachedCharset(
      const nsCString& charset) override;
  virtual mozilla::ipc::IPCResult RecvSuspend() override;
  virtual mozilla::ipc::IPCResult RecvResume() override;
  virtual mozilla::ipc::IPCResult RecvCancel(const nsresult& status) override;
  virtual mozilla::ipc::IPCResult RecvRedirect2Verify(
      const nsresult& result, const RequestHeaderTuples& changedHeaders,
      const uint32_t& aSourceRequestBlockingReason,
      const Maybe<ChildLoadInfoForwarderArgs>& aTargetLoadInfoForwarder,
      const uint32_t& loadFlags, nsIReferrerInfo* aReferrerInfo,
      const Maybe<URIParams>& apiRedirectUri,
      const Maybe<CorsPreflightArgs>& aCorsPreflightArgs,
      const bool& aChooseAppcache) override;
  virtual mozilla::ipc::IPCResult RecvDocumentChannelCleanup(
      const bool& clearCacheEntry) override;
  virtual mozilla::ipc::IPCResult RecvMarkOfflineCacheEntryAsForeign() override;
  virtual mozilla::ipc::IPCResult RecvDivertOnDataAvailable(
      const nsCString& data, const uint64_t& offset,
      const uint32_t& count) override;
  virtual mozilla::ipc::IPCResult RecvDivertOnStopRequest(
      const nsresult& statusCode) override;
  virtual mozilla::ipc::IPCResult RecvDivertComplete() override;
  virtual mozilla::ipc::IPCResult RecvRemoveCorsPreflightCacheEntry(
      const URIParams& uri,
      const mozilla::ipc::PrincipalInfo& requestingPrincipal) override;
  virtual mozilla::ipc::IPCResult RecvBytesRead(const int32_t& aCount) override;
  virtual mozilla::ipc::IPCResult RecvOpenOriginalCacheInputStream() override;
  virtual mozilla::ipc::IPCResult RecvOpenAltDataCacheInputStream(
      const nsCString& aType) override;
  virtual void ActorDestroy(ActorDestroyReason why) override;

  // Supporting function for ADivertableParentChannel.
  [[nodiscard]] nsresult ResumeForDiversion();

  // Asynchronously calls NotifyDiversionFailed.
  void FailDiversion(nsresult aErrorCode);

  friend class ParentChannelListener;
  RefPtr<mozilla::dom::BrowserParent> mBrowserParent;

  [[nodiscard]] nsresult ReportSecurityMessage(
      const nsAString& aMessageTag, const nsAString& aMessageCategory) override;
  nsresult LogBlockedCORSRequest(const nsAString& aMessage,
                                 const nsACString& aCategory) override;
  nsresult LogMimeTypeMismatch(const nsACString& aMessageName, bool aWarning,
                               const nsAString& aURL,
                               const nsAString& aContentType) override;

  // Calls SendDeleteSelf and sets mIPCClosed to true because we should not
  // send any more messages after that. Bug 1274886
  [[nodiscard]] bool DoSendDeleteSelf();
  // Called to notify the parent channel to not send any more IPC messages.
  virtual mozilla::ipc::IPCResult RecvDeletingChannel() override;
  virtual mozilla::ipc::IPCResult RecvFinishInterceptedRedirect() override;

 private:
  void UpdateAndSerializeSecurityInfo(nsACString& aSerializedSecurityInfoOut);

  void DivertOnDataAvailable(const nsCString& data, const uint64_t& offset,
                             const uint32_t& count);
  void DivertOnStopRequest(const nsresult& statusCode);
  void DivertComplete();
  void MaybeFlushPendingDiversion();
  void ResponseSynthesized();

  // final step for Redirect2Verify procedure, will be invoked while both
  // redirecting and redirected channel are ready or any error happened.
  // OnRedirectVerifyCallback will be invoked for finishing the async
  // redirect verification procedure.
  void ContinueRedirect2Verify(const nsresult& aResult);

  void AsyncOpenFailed(nsresult aRv);

  // Request to pair with a HttpBackgroundChannelParent with the same channel
  // id, a promise will be returned so the caller can append callbacks on it.
  // If called multiple times before mBgParent is available, the same promise
  // will be returned and the callbacks will be invoked in order.
  [[nodiscard]] RefPtr<GenericNonExclusivePromise> WaitForBgParent();

  // Remove the association with background channel after main-thread IPC
  // is about to be destroyed or no further event is going to be sent, i.e.,
  // DocumentChannelCleanup.
  void CleanupBackgroundChannel();

  // Check if the channel needs to enable the flow control on the IPC channel.
  // That is, we may suspend the channel if the ODA-s to child process are not
  // consumed quickly enough. Otherwise, memory explosion could happen.
  bool NeedFlowControl();
  int32_t mSendWindowSize;

  friend class HttpBackgroundChannelParent;
  friend class DivertDataAvailableEvent;
  friend class DivertStopRequestEvent;
  friend class DivertCompleteEvent;

  RefPtr<HttpBaseChannel> mChannel;
  nsCOMPtr<nsICacheEntry> mCacheEntry;

  nsCOMPtr<nsIChannel> mRedirectChannel;
  nsCOMPtr<nsIAsyncVerifyRedirectCallback> mRedirectCallback;

  UniquePtr<class nsHttpChannel::OfflineCacheEntryAsForeignMarker>
      mOfflineForeignMarker;
  nsCOMPtr<nsILoadContext> mLoadContext;
  RefPtr<nsHttpHandler> mHttpHandler;

  RefPtr<ParentChannelListener> mParentListener;
  // The listener we are diverting to or will divert to if mPendingDiversion
  // is set.
  nsCOMPtr<nsIStreamListener> mDivertListener;

  RefPtr<ChannelEventQueue> mEventQ;

  RefPtr<HttpBackgroundChannelParent> mBgParent;

  MozPromiseHolder<GenericNonExclusivePromise> mPromise;
  MozPromiseRequestHolder<GenericNonExclusivePromise> mRequest;

  dom::TabId mNestedFrameId;

  // To calculate the delay caused by the e10s back-pressure suspension
  TimeStamp mResumedTimestamp;

  Atomic<bool> mIPCClosed;  // PHttpChannel actor has been Closed()

  // Corresponding redirect channel registrar Id. 0 means redirection is not
  // started.
  uint32_t mRedirectChannelId = 0;

  PBOverrideStatus mPBOverride;

  // Set to the canceled status value if the main channel was canceled.
  nsresult mStatus;

  // OnStatus is always called before OnProgress.
  // Set true in OnStatus if next OnProgress can be ignored
  // since the information can be recontructed from ODA.
  uint8_t mIgnoreProgress : 1;

  uint8_t mSentRedirect1BeginFailed : 1;
  uint8_t mReceivedRedirect2Verify : 1;
  uint8_t mHasSuspendedByBackPressure : 1;

  // Indicates that diversion has been requested, but we could not start it
  // yet because the channel is still being opened with a synthesized response.
  uint8_t mPendingDiversion : 1;
  // Once set, no OnStart/OnData/OnStop calls should be accepted; conversely, it
  // must be set when RecvDivertOnData/~DivertOnStop/~DivertComplete are
  // received from the child channel.
  uint8_t mDivertingFromChild : 1;

  // Set if OnStart|StopRequest was called during a diversion from the child.
  uint8_t mDivertedOnStartRequest : 1;

  uint8_t mSuspendedForDiversion : 1;

  // Set if this channel should be suspended after synthesizing a response.
  uint8_t mSuspendAfterSynthesizeResponse : 1;
  // Set if this channel will synthesize its response.
  uint8_t mWillSynthesizeResponse : 1;

  // Set if we get the result of and cache |mNeedFlowControl|
  uint8_t mCacheNeedFlowControlInitialized : 1;
  uint8_t mNeedFlowControl : 1;
  uint8_t mSuspendedForFlowControl : 1;

  // Set to true if we get OnStartRequest called with an nsIMultiPartChannel,
  // and expect multiple OnStartRequest calls.
  // When this happens we send OnTransportAndData and OnStopRequest over
  // PHttpChannel instead of PHttpBackgroundChannel to make synchronizing all
  // the parts easier.
  uint8_t mIsMultiPart : 1;

  // Number of events to wait before actually invoking AsyncOpen on the main
  // channel. For each asynchronous step required before InvokeAsyncOpen, should
  // increase 1 to mAsyncOpenBarrier and invoke TryInvokeAsyncOpen after
  // finished. This attribute is main thread only.
  uint8_t mAsyncOpenBarrier = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(HttpChannelParent, HTTP_CHANNEL_PARENT_IID)

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_HttpChannelParent_h
