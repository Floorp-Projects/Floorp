/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_HttpChannelChild_h
#define mozilla_net_HttpChannelChild_h

#include "mozilla/Mutex.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/net/HttpBaseChannel.h"
#include "mozilla/net/PHttpChannelChild.h"
#include "mozilla/net/ChannelEventQueue.h"

#include "nsIStreamListener.h"
#include "nsILoadGroup.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIProgressEventSink.h"
#include "nsICacheInfoChannel.h"
#include "nsIApplicationCache.h"
#include "nsIApplicationCacheChannel.h"
#include "nsIUploadChannel2.h"
#include "nsIResumableChannel.h"
#include "nsIProxiedChannel.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsIAssociatedContentSecurity.h"
#include "nsIChildChannel.h"
#include "nsIHttpChannelChild.h"
#include "nsIDivertableChannel.h"
#include "nsIThreadRetargetableRequest.h"
#include "mozilla/net/DNS.h"

class nsIEventTarget;
class nsInputStreamPump;

namespace mozilla {
namespace net {

class InterceptedChannelContent;
class InterceptStreamListener;

class HttpChannelChild final : public PHttpChannelChild
                             , public HttpBaseChannel
                             , public HttpAsyncAborter<HttpChannelChild>
                             , public nsICacheInfoChannel
                             , public nsIProxiedChannel
                             , public nsIApplicationCacheChannel
                             , public nsIAsyncVerifyRedirectCallback
                             , public nsIAssociatedContentSecurity
                             , public nsIChildChannel
                             , public nsIHttpChannelChild
                             , public nsIDivertableChannel
                             , public nsIThreadRetargetableRequest
{
  virtual ~HttpChannelChild();
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSICACHEINFOCHANNEL
  NS_DECL_NSIPROXIEDCHANNEL
  NS_DECL_NSIAPPLICATIONCACHECONTAINER
  NS_DECL_NSIAPPLICATIONCACHECHANNEL
  NS_DECL_NSIASYNCVERIFYREDIRECTCALLBACK
  NS_DECL_NSIASSOCIATEDCONTENTSECURITY
  NS_DECL_NSICHILDCHANNEL
  NS_DECL_NSIHTTPCHANNELCHILD
  NS_DECL_NSIDIVERTABLECHANNEL
  NS_DECL_NSITHREADRETARGETABLEREQUEST

  HttpChannelChild();

  // Methods HttpBaseChannel didn't implement for us or that we override.
  //
  // nsIRequest
  NS_IMETHOD Cancel(nsresult status) override;
  NS_IMETHOD Suspend() override;
  NS_IMETHOD Resume() override;
  // nsIChannel
  NS_IMETHOD GetSecurityInfo(nsISupports **aSecurityInfo) override;
  NS_IMETHOD AsyncOpen(nsIStreamListener *listener, nsISupports *aContext) override;
  NS_IMETHOD AsyncOpen2(nsIStreamListener *aListener) override;

  // HttpBaseChannel::nsIHttpChannel
  NS_IMETHOD SetReferrerWithPolicy(nsIURI *referrer, uint32_t referrerPolicy) override;
  NS_IMETHOD SetRequestHeader(const nsACString& aHeader,
                              const nsACString& aValue,
                              bool aMerge) override;
  NS_IMETHOD SetEmptyRequestHeader(const nsACString& aHeader) override;
  NS_IMETHOD RedirectTo(nsIURI *newURI) override;
  NS_IMETHOD GetProtocolVersion(nsACString& aProtocolVersion) override;
  // nsIHttpChannelInternal
  NS_IMETHOD SetupFallbackChannel(const char *aFallbackKey) override;
  NS_IMETHOD ForceIntercepted(uint64_t aInterceptionID) override;
  // nsISupportsPriority
  NS_IMETHOD SetPriority(int32_t value) override;
  // nsIClassOfService
  NS_IMETHOD SetClassFlags(uint32_t inFlags) override;
  NS_IMETHOD AddClassFlags(uint32_t inFlags) override;
  NS_IMETHOD ClearClassFlags(uint32_t inFlags) override;
  // nsIResumableChannel
  NS_IMETHOD ResumeAt(uint64_t startPos, const nsACString& entityID) override;

  // IPDL holds a reference while the PHttpChannel protocol is live (starting at
  // AsyncOpen, and ending at either OnStopRequest or any IPDL error, either of
  // which call NeckoChild::DeallocPHttpChannelChild()).
  void AddIPDLReference();
  void ReleaseIPDLReference();

  MOZ_MUST_USE bool IsSuspended();

  mozilla::ipc::IPCResult RecvNotifyTrackingProtectionDisabled() override;
  mozilla::ipc::IPCResult RecvNotifyTrackingResource() override;
  void FlushedForDiversion();
  mozilla::ipc::IPCResult RecvSetClassifierMatchedInfo(const ClassifierInfo& aInfo) override;

  void OnCopyComplete(nsresult aStatus) override;

protected:
  mozilla::ipc::IPCResult RecvOnStartRequest(const nsresult& channelStatus,
                                             const nsHttpResponseHead& responseHead,
                                             const bool& useResponseHead,
                                             const nsHttpHeaderArray& requestHeaders,
                                             const bool& isFromCache,
                                             const bool& cacheEntryAvailable,
                                             const uint32_t& cacheExpirationTime,
                                             const nsCString& cachedCharset,
                                             const nsCString& securityInfoSerialization,
                                             const NetAddr& selfAddr,
                                             const NetAddr& peerAddr,
                                             const int16_t& redirectCount,
                                             const uint32_t& cacheKey,
                                             const nsCString& altDataType,
                                             const int64_t& altDataLen) override;
  mozilla::ipc::IPCResult RecvOnTransportAndData(const nsresult& channelStatus,
                                                 const nsresult& status,
                                                 const uint64_t& offset,
                                                 const uint32_t& count,
                                                 const nsCString& data) override;
  mozilla::ipc::IPCResult RecvOnStopRequest(const nsresult& statusCode, const ResourceTimingStruct& timing) override;
  mozilla::ipc::IPCResult RecvOnProgress(const int64_t& progress, const int64_t& progressMax) override;
  mozilla::ipc::IPCResult RecvOnStatus(const nsresult& status) override;
  mozilla::ipc::IPCResult RecvFailedAsyncOpen(const nsresult& status) override;
  mozilla::ipc::IPCResult RecvRedirect1Begin(const uint32_t& registrarId,
                                             const URIParams& newURI,
                                             const uint32_t& redirectFlags,
                                             const nsHttpResponseHead& responseHead,
                                             const nsCString& securityInfoSerialization,
                                             const uint64_t& channelId,
                                             const NetAddr& oldPeerAddr) override;
  mozilla::ipc::IPCResult RecvRedirect3Complete() override;
  mozilla::ipc::IPCResult RecvAssociateApplicationCache(const nsCString& groupID,
                                                        const nsCString& clientID) override;
  mozilla::ipc::IPCResult RecvFlushedForDiversion() override;
  mozilla::ipc::IPCResult RecvDivertMessages() override;
  mozilla::ipc::IPCResult RecvDeleteSelf() override;
  mozilla::ipc::IPCResult RecvFinishInterceptedRedirect() override;

  mozilla::ipc::IPCResult RecvReportSecurityMessage(const nsString& messageTag,
                                                    const nsString& messageCategory) override;

  mozilla::ipc::IPCResult RecvIssueDeprecationWarning(const uint32_t& warning,
                                                      const bool& asError) override;

  mozilla::ipc::IPCResult RecvSetPriority(const int16_t& aPriority) override;

  MOZ_MUST_USE bool
  GetAssociatedContentSecurity(nsIAssociatedContentSecurity** res = nullptr);
  virtual void DoNotifyListenerCleanup() override;

  NS_IMETHOD GetResponseSynthesized(bool* aSynthesized) override;

  nsresult
  AsyncCall(void (HttpChannelChild::*funcPtr)(),
            nsRunnableMethod<HttpChannelChild> **retval = nullptr) override;

private:
  // this section is for main-thread-only object
  // all the references need to be proxy released on main thread.
  nsCOMPtr<nsISupports> mCacheKey;

  // Proxy release all members above on main thread.
  void ReleaseMainThreadOnlyReferences();

private:

  class OverrideRunnable : public Runnable {
  public:
    OverrideRunnable(HttpChannelChild* aChannel,
                     HttpChannelChild* aNewChannel,
                     InterceptStreamListener* aListener,
                     nsIInputStream* aInput,
                     nsAutoPtr<nsHttpResponseHead>& aHead);

    NS_IMETHOD Run() override;
    void OverrideWithSynthesizedResponse();
  private:
    RefPtr<HttpChannelChild> mChannel;
    RefPtr<HttpChannelChild> mNewChannel;
    RefPtr<InterceptStreamListener> mListener;
    nsCOMPtr<nsIInputStream> mInput;
    nsAutoPtr<nsHttpResponseHead> mHead;
  };

  // Sets the event target for future IPC messages. Messages will either be
  // directed to the TabGroup or DocGroup, depending on the LoadInfo associated
  // with the channel. Should be called when a new channel is being set up,
  // before the constructor message is sent to the parent.
  void SetEventTarget();

  // Get event target for processing network events.
  already_AddRefed<nsIEventTarget> GetNeckoTarget();

  // Get event target for ODA.
  already_AddRefed<nsIEventTarget> GetODATarget();

  MOZ_MUST_USE nsresult ContinueAsyncOpen();

  void DoOnStartRequest(nsIRequest* aRequest, nsISupports* aContext);
  void DoOnStatus(nsIRequest* aRequest, nsresult status);
  void DoOnProgress(nsIRequest* aRequest, int64_t progress, int64_t progressMax);
  void DoOnDataAvailable(nsIRequest* aRequest, nsISupports* aContext, nsIInputStream* aStream,
                         uint64_t offset, uint32_t count);
  void DoPreOnStopRequest(nsresult aStatus);
  void DoOnStopRequest(nsIRequest* aRequest, nsresult aChannelStatus, nsISupports* aContext);

  bool ShouldInterceptURI(nsIURI* aURI, bool& aShouldUpgrade);

  // Discard the prior interception and continue with the original network request.
  void ResetInterception();

  // Override this channel's pending response with a synthesized one. The content will be
  // asynchronously read from the pump.
  void OverrideWithSynthesizedResponse(nsAutoPtr<nsHttpResponseHead>& aResponseHead,
                                       nsIInputStream* aSynthesizedInput,
                                       InterceptStreamListener* aStreamListener);

  void ForceIntercepted(nsIInputStream* aSynthesizedInput);

  // Try send DeletingChannel message to parent side. Dispatch an async task to
  // main thread if invoking on non-main thread.
  void TrySendDeletingChannel();

  // Try invoke Cancel if on main thread, or prepend a CancelEvent in mEventQ to
  // ensure Cacnel is processed before any other channel events.
  void CancelOnMainThread(nsresult aRv);

  RequestHeaderTuples mClientSetRequestHeaders;
  nsCOMPtr<nsIChildChannel> mRedirectChannelChild;
  RefPtr<InterceptStreamListener> mInterceptListener;
  RefPtr<nsInputStreamPump> mSynthesizedResponsePump;
  nsCOMPtr<nsIInputStream> mSynthesizedInput;
  int64_t mSynthesizedStreamLength;

  bool mIsFromCache;
  bool mCacheEntryAvailable;
  uint32_t     mCacheExpirationTime;
  nsCString    mCachedCharset;

  nsCString mProtocolVersion;

  // If ResumeAt is called before AsyncOpen, we need to send extra data upstream
  bool mSendResumeAt;

  // To ensure only one SendDeletingChannel is triggered.
  Atomic<bool> mDeletingChannelSent;

  Atomic<bool> mIPCOpen;
  bool mKeptAlive;            // IPC kept open, but only for security info
  RefPtr<ChannelEventQueue> mEventQ;

  // If nsUnknownDecoder is involved OnStartRequest call will be delayed and
  // this queue keeps OnDataAvailable data until OnStartRequest is finally
  // called.
  nsTArray<UniquePtr<ChannelEvent>> mUnknownDecoderEventQ;
  bool mUnknownDecoderInvolved;

  // Once set, OnData and possibly OnStop will be diverted to the parent.
  bool mDivertingToParent;
  // Once set, no OnStart/OnData/OnStop callbacks should be received from the
  // parent channel, nor dequeued from the ChannelEventQueue.
  bool mFlushedForDiversion;
  // Set if SendSuspend is called. Determines if SendResume is needed when
  // diverting callbacks to parent.
  bool mSuspendSent;

  // Set if a response was synthesized, indicating that any forthcoming redirects
  // should be intercepted.
  bool mSynthesizedResponse;

  // Set if a synthesized response should cause us to explictly allows intercepting
  // an expected forthcoming redirect.
  bool mShouldInterceptSubsequentRedirect;
  // Set if a redirection is being initiated to facilitate providing a synthesized
  // response to a channel using a different principal than the current one.
  bool mRedirectingForSubsequentSynthesizedResponse;

  // Set if a manual redirect mode channel needs to be intercepted in the
  // parent.
  bool mPostRedirectChannelShouldIntercept;
  // Set if a manual redirect mode channel needs to be upgraded to a secure URI
  // when it's being considered for interception.  Can only be true if
  // mPostRedirectChannelShouldIntercept is true.
  bool mPostRedirectChannelShouldUpgrade;

  // Set if the corresponding parent channel should force an interception to occur
  // before the network transaction is initiated.
  bool mShouldParentIntercept;

  // Set if the corresponding parent channel should suspend after a response
  // is synthesized.
  bool mSuspendParentAfterSynthesizeResponse;

  // Needed to call AsyncOpen in FinishInterceptedRedirect
  nsCOMPtr<nsIStreamListener> mInterceptedRedirectListener;
  nsCOMPtr<nsISupports> mInterceptedRedirectContext;
  // Needed to call CleanupRedirectingChannel in FinishInterceptedRedirect
  RefPtr<HttpChannelChild> mInterceptingChannel;
  // Used to call OverrideWithSynthesizedResponse in FinishInterceptedRedirect
  RefPtr<OverrideRunnable> mOverrideRunnable;

  // EventTarget for labeling networking events.
  nsCOMPtr<nsIEventTarget> mNeckoTarget;
  // Target thread for delivering ODA.
  nsCOMPtr<nsIEventTarget> mODATarget;
  // Used to ensure atomicity of mNeckoTarget / mODATarget;
  Mutex mEventTargetMutex;

  void FinishInterceptedRedirect();
  void CleanupRedirectingChannel(nsresult rv);

  // true after successful AsyncOpen until OnStopRequest completes.
  bool RemoteChannelExists() { return mIPCOpen && !mKeptAlive; }

  void AssociateApplicationCache(const nsCString &groupID,
                                 const nsCString &clientID);
  void OnStartRequest(const nsresult& channelStatus,
                      const nsHttpResponseHead& responseHead,
                      const bool& useResponseHead,
                      const nsHttpHeaderArray& requestHeaders,
                      const bool& isFromCache,
                      const bool& cacheEntryAvailable,
                      const uint32_t& cacheExpirationTime,
                      const nsCString& cachedCharset,
                      const nsCString& securityInfoSerialization,
                      const NetAddr& selfAddr,
                      const NetAddr& peerAddr,
                      const uint32_t& cacheKey,
                      const nsCString& altDataType,
                      const int64_t& altDataLen);
  void MaybeDivertOnData(const nsCString& data,
                         const uint64_t& offset,
                         const uint32_t& count);
  void OnTransportAndData(const nsresult& channelStatus,
                          const nsresult& status,
                          const uint64_t& offset,
                          const uint32_t& count,
                          const nsCString& data);
  void OnStopRequest(const nsresult& channelStatus, const ResourceTimingStruct& timing);
  void MaybeDivertOnStop(const nsresult& aChannelStatus);
  void OnProgress(const int64_t& progress, const int64_t& progressMax);
  void OnStatus(const nsresult& status);
  void FailedAsyncOpen(const nsresult& status);
  void HandleAsyncAbort();
  void Redirect1Begin(const uint32_t& registrarId,
                      const URIParams& newUri,
                      const uint32_t& redirectFlags,
                      const nsHttpResponseHead& responseHead,
                      const nsACString& securityInfoSerialization,
                      const uint64_t& channelId);
  bool Redirect3Complete(OverrideRunnable* aRunnable);
  void DeleteSelf();

  // Create a a new channel to be used in a redirection, based on the provided
  // response headers.
  MOZ_MUST_USE nsresult SetupRedirect(nsIURI* uri,
                                      const nsHttpResponseHead* responseHead,
                                      const uint32_t& redirectFlags,
                                      nsIChannel** outChannel);

  // Perform a redirection without communicating with the parent process at all.
  void BeginNonIPCRedirect(nsIURI* responseURI,
                           const nsHttpResponseHead* responseHead);

  // Override the default security info pointer during a non-IPC redirection.
  void OverrideSecurityInfoForNonIPCRedirect(nsISupports* securityInfo);

  friend class AssociateApplicationCacheEvent;
  friend class StartRequestEvent;
  friend class StopRequestEvent;
  friend class TransportAndDataEvent;
  friend class MaybeDivertOnDataHttpEvent;
  friend class MaybeDivertOnStopHttpEvent;
  friend class ProgressEvent;
  friend class StatusEvent;
  friend class FailedAsyncOpenEvent;
  friend class Redirect1Event;
  friend class Redirect3Event;
  friend class DeleteSelfEvent;
  friend class HttpFlushedForDiversionEvent;
  friend class CancelEvent;
  friend class HttpAsyncAborter<HttpChannelChild>;
  friend class InterceptStreamListener;
  friend class InterceptedChannelContent;
};

// A stream listener interposed between the nsInputStreamPump used for intercepted channels
// and this channel's original listener. This is only used to ensure the original listener
// sees the channel as the request object, and to synthesize OnStatus and OnProgress notifications.
class InterceptStreamListener : public nsIStreamListener
                              , public nsIProgressEventSink
{
  RefPtr<HttpChannelChild> mOwner;
  nsCOMPtr<nsISupports> mContext;
  virtual ~InterceptStreamListener() {}
 public:
  InterceptStreamListener(HttpChannelChild* aOwner, nsISupports* aContext)
  : mOwner(aOwner)
  , mContext(aContext)
  {
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIPROGRESSEVENTSINK

  void Cleanup();
};

//-----------------------------------------------------------------------------
// inline functions
//-----------------------------------------------------------------------------

inline bool
HttpChannelChild::IsSuspended()
{
  return mSuspendCount != 0;
}

} // namespace net
} // namespace mozilla

#endif // mozilla_net_HttpChannelChild_h
