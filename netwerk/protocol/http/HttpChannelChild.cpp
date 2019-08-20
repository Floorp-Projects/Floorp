/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "nsHttp.h"
#include "nsICacheEntry.h"
#include "mozilla/AntiTrackingCommon.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/ServiceWorkerUtils.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/dom/TabGroup.h"
#include "mozilla/extensions/StreamFilterParent.h"
#include "mozilla/ipc/FileDescriptorSetChild.h"
#include "mozilla/ipc/IPCStreamUtils.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/net/HttpChannelChild.h"
#include "mozilla/net/UrlClassifierCommon.h"
#include "mozilla/net/UrlClassifierFeatureFactory.h"

#include "AltDataOutputStreamChild.h"
#include "CookieServiceChild.h"
#include "HttpBackgroundChannelChild.h"
#include "nsCOMPtr.h"
#include "nsISupportsPrimitives.h"
#include "nsContentPolicyUtils.h"
#include "nsDOMNavigationTiming.h"
#include "nsGlobalWindow.h"
#include "nsStringStream.h"
#include "nsHttpChannel.h"
#include "nsHttpHandler.h"
#include "nsNetUtil.h"
#include "nsSerializationHelper.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/PerformanceStorage.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/net/ChannelDiverterChild.h"
#include "mozilla/net/DNS.h"
#include "SerializedLoadContext.h"
#include "nsInputStreamPump.h"
#include "InterceptedChannel.h"
#include "mozIThirdPartyUtil.h"
#include "nsContentSecurityManager.h"
#include "nsICompressConvStats.h"
#include "nsIDeprecationWarner.h"
#include "mozilla/dom/Document.h"
#include "nsIDOMWindowUtils.h"
#include "nsIEventTarget.h"
#include "nsIScriptError.h"
#include "nsRedirectHistoryEntry.h"
#include "nsSocketTransportService2.h"
#include "nsStreamUtils.h"
#include "nsThreadUtils.h"
#include "nsCORSListenerProxy.h"
#include "nsApplicationCache.h"
#include "ClassifierDummyChannel.h"

#ifdef MOZ_TASK_TRACER
#  include "GeckoTaskTracer.h"
#endif

#ifdef MOZ_GECKO_PROFILER
#  include "ProfilerMarkerPayload.h"
#endif

#include <functional>

using namespace mozilla::dom;
using namespace mozilla::ipc;

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(InterceptStreamListener, nsIStreamListener,
                  nsIRequestObserver, nsIProgressEventSink)

NS_IMETHODIMP
InterceptStreamListener::OnStartRequest(nsIRequest* aRequest) {
  if (mOwner) {
    mOwner->DoOnStartRequest(mOwner, nullptr);
  }
  return NS_OK;
}

NS_IMETHODIMP
InterceptStreamListener::OnStatus(nsIRequest* aRequest, nsISupports* aContext,
                                  nsresult status, const char16_t* aStatusArg) {
  if (mOwner) {
    mOwner->DoOnStatus(mOwner, status);
  }
  return NS_OK;
}

NS_IMETHODIMP
InterceptStreamListener::OnProgress(nsIRequest* aRequest, nsISupports* aContext,
                                    int64_t aProgress, int64_t aProgressMax) {
  if (mOwner) {
    mOwner->DoOnProgress(mOwner, aProgress, aProgressMax);
  }
  return NS_OK;
}

NS_IMETHODIMP
InterceptStreamListener::OnDataAvailable(nsIRequest* aRequest,
                                         nsIInputStream* aInputStream,
                                         uint64_t aOffset, uint32_t aCount) {
  if (!mOwner) {
    return NS_OK;
  }

  uint32_t loadFlags;
  mOwner->GetLoadFlags(&loadFlags);

  if (!(loadFlags & HttpBaseChannel::LOAD_BACKGROUND)) {
    nsCOMPtr<nsIURI> uri;
    mOwner->GetURI(getter_AddRefs(uri));

    nsAutoCString host;
    uri->GetHost(host);

    OnStatus(mOwner, nullptr, NS_NET_STATUS_READING,
             NS_ConvertUTF8toUTF16(host).get());

    int64_t progress = aOffset + aCount;
    OnProgress(mOwner, nullptr, progress, mOwner->mSynthesizedStreamLength);
  }

  mOwner->DoOnDataAvailable(mOwner, nullptr, aInputStream, aOffset, aCount);
  return NS_OK;
}

NS_IMETHODIMP
InterceptStreamListener::OnStopRequest(nsIRequest* aRequest,
                                       nsresult aStatusCode) {
  if (mOwner) {
    mOwner->DoPreOnStopRequest(aStatusCode);
    mOwner->DoOnStopRequest(mOwner, aStatusCode, mContext);
  }
  Cleanup();
  return NS_OK;
}

void InterceptStreamListener::Cleanup() {
  mOwner = nullptr;
  mContext = nullptr;
}

//-----------------------------------------------------------------------------
// HttpChannelChild
//-----------------------------------------------------------------------------

HttpChannelChild::HttpChannelChild()
    : HttpAsyncAborter<HttpChannelChild>(this),
      NeckoTargetHolder(nullptr),
      mBgChildMutex("HttpChannelChild::BgChildMutex"),
      mEventTargetMutex("HttpChannelChild::EventTargetMutex"),
      mSynthesizedStreamLength(0),
      mCacheEntryId(0),
      mCacheKey(0),
      mCacheFetchCount(0),
      mCacheExpirationTime(nsICacheEntry::NO_EXPIRATION_TIME),
      mDeletingChannelSent(false),
      mUnknownDecoderInvolved(false),
      mDivertingToParent(false),
      mFlushedForDiversion(false),
      mIsFromCache(false),
      mIsRacing(false),
      mCacheNeedToReportBytesReadInitialized(false),
      mNeedToReportBytesRead(true),
      mCacheEntryAvailable(false),
      mAltDataCacheEntryAvailable(false),
      mSendResumeAt(false),
      mKeptAlive(false),
      mIPCActorDeleted(false),
      mSuspendSent(false),
      mSynthesizedResponse(false),
      mShouldInterceptSubsequentRedirect(false),
      mRedirectingForSubsequentSynthesizedResponse(false),
      mPostRedirectChannelShouldIntercept(false),
      mPostRedirectChannelShouldUpgrade(false),
      mShouldParentIntercept(false),
      mSuspendParentAfterSynthesizeResponse(false) {
  LOG(("Creating HttpChannelChild @%p\n", this));

  mChannelCreationTime = PR_Now();
  mChannelCreationTimestamp = TimeStamp::Now();
  mLastStatusReported =
      mChannelCreationTimestamp;  // in case we enable the profiler after Init()
  mAsyncOpenTime = TimeStamp::Now();
  mEventQ = new ChannelEventQueue(static_cast<nsIHttpChannel*>(this));

  // Ensure that the cookie service is initialized before the first
  // IPC HTTP channel is created.
  // We require that the parent cookie service actor exists while
  // processing HTTP responses.
  RefPtr<CookieServiceChild> cookieService = CookieServiceChild::GetSingleton();
}

HttpChannelChild::~HttpChannelChild() {
  LOG(("Destroying HttpChannelChild @%p\n", this));

  ReleaseMainThreadOnlyReferences();
}

void HttpChannelChild::ReleaseMainThreadOnlyReferences() {
  if (NS_IsMainThread()) {
    // Already on main thread, let dtor to
    // take care of releasing references
    return;
  }

  nsTArray<nsCOMPtr<nsISupports>> arrayToRelease;
  arrayToRelease.AppendElement(mRedirectChannelChild.forget());

  // To solve multiple inheritence of nsISupports in InterceptStreamListener
  nsCOMPtr<nsIStreamListener> listener = mInterceptListener.forget();
  arrayToRelease.AppendElement(listener.forget());

  arrayToRelease.AppendElement(mInterceptedRedirectListener.forget());
  arrayToRelease.AppendElement(mInterceptedRedirectContext.forget());

  NS_DispatchToMainThread(new ProxyReleaseRunnable(std::move(arrayToRelease)));
}
//-----------------------------------------------------------------------------
// HttpChannelChild::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ADDREF(HttpChannelChild)

NS_IMETHODIMP_(MozExternalRefCountType) HttpChannelChild::Release() {
  if (!NS_IsMainThread()) {
    nsrefcnt count = mRefCnt;
    nsresult rv = NS_DispatchToMainThread(NewNonOwningRunnableMethod(
        "HttpChannelChild::Release", this, &HttpChannelChild::Release));

    // Continue Release procedure if failed to dispatch to main thread.
    if (!NS_WARN_IF(NS_FAILED(rv))) {
      return count - 1;
    }
  }

  nsrefcnt count = --mRefCnt;
  MOZ_ASSERT(int32_t(count) >= 0, "dup release");
  NS_LOG_RELEASE(this, count, "HttpChannelChild");

  // Normally we Send_delete in OnStopRequest, but when we need to retain the
  // remote channel for security info IPDL itself holds 1 reference, so we
  // Send_delete when refCnt==1.  But if !CanSend(), then there's nobody to send
  // to, so we fall through.
  if (mKeptAlive && count == 1 && CanSend()) {
    mKeptAlive = false;
    // We send a message to the parent, which calls SendDelete, and then the
    // child calling Send__delete__() to finally drop the refcount to 0.
    TrySendDeletingChannel();
    return 1;
  }

  if (count == 0) {
    mRefCnt = 1; /* stabilize */
    delete this;
    return 0;
  }
  return count;
}

NS_INTERFACE_MAP_BEGIN(HttpChannelChild)
  NS_INTERFACE_MAP_ENTRY(nsIRequest)
  NS_INTERFACE_MAP_ENTRY(nsIChannel)
  NS_INTERFACE_MAP_ENTRY(nsIHttpChannel)
  NS_INTERFACE_MAP_ENTRY(nsIHttpChannelInternal)
  NS_INTERFACE_MAP_ENTRY(nsICacheInfoChannel)
  NS_INTERFACE_MAP_ENTRY(nsIResumableChannel)
  NS_INTERFACE_MAP_ENTRY(nsISupportsPriority)
  NS_INTERFACE_MAP_ENTRY(nsIClassOfService)
  NS_INTERFACE_MAP_ENTRY(nsIProxiedChannel)
  NS_INTERFACE_MAP_ENTRY(nsITraceableChannel)
  NS_INTERFACE_MAP_ENTRY(nsIApplicationCacheContainer)
  NS_INTERFACE_MAP_ENTRY(nsIApplicationCacheChannel)
  NS_INTERFACE_MAP_ENTRY(nsIAsyncVerifyRedirectCallback)
  NS_INTERFACE_MAP_ENTRY(nsIChildChannel)
  NS_INTERFACE_MAP_ENTRY(nsIHttpChannelChild)
  NS_INTERFACE_MAP_ENTRY(nsIDivertableChannel)
  NS_INTERFACE_MAP_ENTRY(nsIThreadRetargetableRequest)
  NS_INTERFACE_MAP_ENTRY_CONCRETE(HttpChannelChild)
NS_INTERFACE_MAP_END_INHERITING(HttpBaseChannel)

//-----------------------------------------------------------------------------
// HttpChannelChild::PHttpChannelChild
//-----------------------------------------------------------------------------

void HttpChannelChild::OnBackgroundChildReady(
    HttpBackgroundChannelChild* aBgChild) {
  LOG(("HttpChannelChild::OnBackgroundChildReady [this=%p, bgChild=%p]\n", this,
       aBgChild));
  MOZ_ASSERT(OnSocketThread());

  {
    MutexAutoLock lock(mBgChildMutex);

    // mBgChild might be removed or replaced while the original background
    // channel is inited on STS thread.
    if (mBgChild != aBgChild) {
      return;
    }

    MOZ_ASSERT(mBgInitFailCallback);
    mBgInitFailCallback = nullptr;
  }
}

void HttpChannelChild::OnBackgroundChildDestroyed(
    HttpBackgroundChannelChild* aBgChild) {
  LOG(("HttpChannelChild::OnBackgroundChildDestroyed [this=%p]\n", this));
  // This function might be called during shutdown phase, so OnSocketThread()
  // might return false even on STS thread. Use IsOnCurrentThreadInfallible()
  // to get correct information.
  MOZ_ASSERT(gSocketTransportService);
  MOZ_ASSERT(gSocketTransportService->IsOnCurrentThreadInfallible());

  nsCOMPtr<nsIRunnable> callback;
  {
    MutexAutoLock lock(mBgChildMutex);

    // mBgChild might be removed or replaced while the original background
    // channel is destroyed on STS thread.
    if (aBgChild != mBgChild) {
      return;
    }

    mBgChild = nullptr;
    callback = mBgInitFailCallback.forget();
  }

  if (callback) {
    nsCOMPtr<nsIEventTarget> neckoTarget = GetNeckoTarget();
    neckoTarget->Dispatch(callback, NS_DISPATCH_NORMAL);
  }
}

class AssociateApplicationCacheEvent
    : public NeckoTargetChannelEvent<HttpChannelChild> {
 public:
  AssociateApplicationCacheEvent(HttpChannelChild* aChild,
                                 const nsCString& aGroupID,
                                 const nsCString& aClientID)
      : NeckoTargetChannelEvent<HttpChannelChild>(aChild),
        groupID(aGroupID),
        clientID(aClientID) {}

  void Run() override { mChild->AssociateApplicationCache(groupID, clientID); }

 private:
  nsCString groupID;
  nsCString clientID;
};

mozilla::ipc::IPCResult HttpChannelChild::RecvAssociateApplicationCache(
    const nsCString& groupID, const nsCString& clientID) {
  LOG(("HttpChannelChild::RecvAssociateApplicationCache [this=%p]\n", this));
  mEventQ->RunOrEnqueue(
      new AssociateApplicationCacheEvent(this, groupID, clientID));
  return IPC_OK();
}

void HttpChannelChild::AssociateApplicationCache(const nsCString& groupID,
                                                 const nsCString& clientID) {
  LOG(("HttpChannelChild::AssociateApplicationCache [this=%p]\n", this));
  mApplicationCache = new nsApplicationCache();

  mLoadedFromApplicationCache = true;
  mApplicationCache->InitAsHandle(groupID, clientID);
}

class StartRequestEvent : public NeckoTargetChannelEvent<HttpChannelChild> {
 public:
  StartRequestEvent(
      HttpChannelChild* aChild, const nsresult& aChannelStatus,
      const nsHttpResponseHead& aResponseHead, const bool& aUseResponseHead,
      const nsHttpHeaderArray& aRequestHeaders,
      const ParentLoadInfoForwarderArgs& loadInfoForwarder,
      const bool& aIsFromCache, const bool& aIsRacing,
      const bool& aCacheEntryAvailable, const uint64_t& aCacheEntryId,
      const int32_t& aCacheFetchCount, const uint32_t& aCacheExpirationTime,
      const nsCString& aCachedCharset,
      const nsCString& aSecurityInfoSerialization, const NetAddr& aSelfAddr,
      const NetAddr& aPeerAddr, const uint32_t& aCacheKey,
      const nsCString& altDataType, const int64_t& altDataLen,
      const bool& deliveringAltData, const bool& aApplyConversion,
      const bool& aIsResolvedByTRR, const ResourceTimingStruct& aTiming,
      const bool& aAllRedirectsSameOrigin)
      : NeckoTargetChannelEvent<HttpChannelChild>(aChild),
        mChannelStatus(aChannelStatus),
        mResponseHead(aResponseHead),
        mRequestHeaders(aRequestHeaders),
        mUseResponseHead(aUseResponseHead),
        mApplyConversion(aApplyConversion),
        mIsFromCache(aIsFromCache),
        mIsRacing(aIsRacing),
        mCacheEntryAvailable(aCacheEntryAvailable),
        mCacheEntryId(aCacheEntryId),
        mCacheFetchCount(aCacheFetchCount),
        mCacheExpirationTime(aCacheExpirationTime),
        mCachedCharset(aCachedCharset),
        mSecurityInfoSerialization(aSecurityInfoSerialization),
        mSelfAddr(aSelfAddr),
        mPeerAddr(aPeerAddr),
        mCacheKey(aCacheKey),
        mAltDataType(altDataType),
        mAltDataLen(altDataLen),
        mDeliveringAltData(deliveringAltData),
        mLoadInfoForwarder(loadInfoForwarder),
        mIsResolvedByTRR(aIsResolvedByTRR),
        mTiming(aTiming),
        mAllRedirectsSameOrigin(aAllRedirectsSameOrigin) {}

  void Run() override {
    LOG(("StartRequestEvent [this=%p]\n", mChild));
    mChild->OnStartRequest(
        mChannelStatus, mResponseHead, mUseResponseHead, mRequestHeaders,
        mLoadInfoForwarder, mIsFromCache, mIsRacing, mCacheEntryAvailable,
        mCacheEntryId, mCacheFetchCount, mCacheExpirationTime, mCachedCharset,
        mSecurityInfoSerialization, mSelfAddr, mPeerAddr, mCacheKey,
        mAltDataType, mAltDataLen, mDeliveringAltData, mApplyConversion,
        mIsResolvedByTRR, mTiming, mAllRedirectsSameOrigin);
  }

 private:
  nsresult mChannelStatus;
  nsHttpResponseHead mResponseHead;
  nsHttpHeaderArray mRequestHeaders;
  bool mUseResponseHead;
  bool mApplyConversion;
  bool mIsFromCache;
  bool mIsRacing;
  bool mCacheEntryAvailable;
  uint64_t mCacheEntryId;
  int32_t mCacheFetchCount;
  uint32_t mCacheExpirationTime;
  nsCString mCachedCharset;
  nsCString mSecurityInfoSerialization;
  NetAddr mSelfAddr;
  NetAddr mPeerAddr;
  uint32_t mCacheKey;
  nsCString mAltDataType;
  int64_t mAltDataLen;
  bool mDeliveringAltData;
  ParentLoadInfoForwarderArgs mLoadInfoForwarder;
  bool mIsResolvedByTRR;
  ResourceTimingStruct mTiming;
  bool mAllRedirectsSameOrigin;
};

mozilla::ipc::IPCResult HttpChannelChild::RecvOnStartRequest(
    const nsresult& channelStatus, const nsHttpResponseHead& responseHead,
    const bool& useResponseHead, const nsHttpHeaderArray& requestHeaders,
    const ParentLoadInfoForwarderArgs& loadInfoForwarder,
    const bool& isFromCache, const bool& isRacing,
    const bool& cacheEntryAvailable, const uint64_t& cacheEntryId,
    const int32_t& cacheFetchCount, const uint32_t& cacheExpirationTime,
    const nsCString& cachedCharset, const nsCString& securityInfoSerialization,
    const NetAddr& selfAddr, const NetAddr& peerAddr,
    const int16_t& redirectCount, const uint32_t& cacheKey,
    const nsCString& altDataType, const int64_t& altDataLen,
    const bool& deliveringAltData, const bool& aApplyConversion,
    const bool& aIsResolvedByTRR, const ResourceTimingStruct& aTiming,
    const bool& aAllRedirectsSameOrigin) {
  AUTO_PROFILER_LABEL("HttpChannelChild::RecvOnStartRequest", NETWORK);
  LOG(("HttpChannelChild::RecvOnStartRequest [this=%p]\n", this));
  // mFlushedForDiversion and mDivertingToParent should NEVER be set at this
  // stage, as they are set in the listener's OnStartRequest.
  MOZ_RELEASE_ASSERT(
      !mFlushedForDiversion,
      "mFlushedForDiversion should be unset before OnStartRequest!");
  MOZ_RELEASE_ASSERT(
      !mDivertingToParent,
      "mDivertingToParent should be unset before OnStartRequest!");

  mRedirectCount = redirectCount;

  mEventQ->RunOrEnqueue(new StartRequestEvent(
      this, channelStatus, responseHead, useResponseHead, requestHeaders,
      loadInfoForwarder, isFromCache, isRacing, cacheEntryAvailable,
      cacheEntryId, cacheFetchCount, cacheExpirationTime, cachedCharset,
      securityInfoSerialization, selfAddr, peerAddr, cacheKey, altDataType,
      altDataLen, deliveringAltData, aApplyConversion, aIsResolvedByTRR,
      aTiming, aAllRedirectsSameOrigin));

  {
    // Child's mEventQ is to control the execution order of the IPC messages
    // from both main thread IPDL and PBackground IPDL.
    // To guarantee the ordering, PBackground IPC messages that are sent after
    // OnStartRequest will be throttled until OnStartRequest hits the Child's
    // mEventQ.
    MutexAutoLock lock(mBgChildMutex);

    if (mBgChild) {
      MOZ_RELEASE_ASSERT(gSocketTransportService);
      DebugOnly<nsresult> rv = gSocketTransportService->Dispatch(
          NewRunnableMethod(
              "HttpBackgroundChannelChild::OnStartRequestReceived", mBgChild,
              &HttpBackgroundChannelChild::OnStartRequestReceived),
          NS_DISPATCH_NORMAL);
    }
  }

  return IPC_OK();
}

void HttpChannelChild::OnStartRequest(
    const nsresult& channelStatus, const nsHttpResponseHead& responseHead,
    const bool& useResponseHead, const nsHttpHeaderArray& requestHeaders,
    const ParentLoadInfoForwarderArgs& loadInfoForwarder,
    const bool& isFromCache, const bool& isRacing,
    const bool& cacheEntryAvailable, const uint64_t& cacheEntryId,
    const int32_t& cacheFetchCount, const uint32_t& cacheExpirationTime,
    const nsCString& cachedCharset, const nsCString& securityInfoSerialization,
    const NetAddr& selfAddr, const NetAddr& peerAddr, const uint32_t& cacheKey,
    const nsCString& altDataType, const int64_t& altDataLen,
    const bool& deliveringAltData, const bool& aApplyConversion,
    const bool& aIsResolvedByTRR, const ResourceTimingStruct& aTiming,
    const bool& aAllRedirectsSameOrigin) {
  LOG(("HttpChannelChild::OnStartRequest [this=%p]\n", this));

  // mFlushedForDiversion and mDivertingToParent should NEVER be set at this
  // stage, as they are set in the listener's OnStartRequest.
  MOZ_RELEASE_ASSERT(
      !mFlushedForDiversion,
      "mFlushedForDiversion should be unset before OnStartRequest!");
  MOZ_RELEASE_ASSERT(
      !mDivertingToParent,
      "mDivertingToParent should be unset before OnStartRequest!");

  // If this channel was aborted by ActorDestroy, then there may be other
  // OnStartRequest/OnStopRequest/OnDataAvailable IPC messages that need to
  // be handled. In that case we just ignore them to avoid calling the listener
  // twice.
  if (mOnStartRequestCalled && mIPCActorDeleted) {
    return;
  }

  if (!mCanceled && NS_SUCCEEDED(mStatus)) {
    mStatus = channelStatus;
  }

  // Cookies headers should not be visible to the child process
  MOZ_ASSERT(!requestHeaders.HasHeader(nsHttp::Cookie));
  MOZ_ASSERT(!nsHttpResponseHead(responseHead).HasHeader(nsHttp::Set_Cookie));

  if (useResponseHead && !mCanceled)
    mResponseHead = new nsHttpResponseHead(responseHead);

  if (!securityInfoSerialization.IsEmpty()) {
    nsresult rv = NS_DeserializeObject(securityInfoSerialization,
                                       getter_AddRefs(mSecurityInfo));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                          "Deserializing security info should not fail");
    Unused << rv;  // So we don't get an unused error in release builds.
  }

  ipc::MergeParentLoadInfoForwarder(loadInfoForwarder, mLoadInfo);

  mIsFromCache = isFromCache;
  mIsRacing = isRacing;
  mCacheEntryAvailable = cacheEntryAvailable;
  mCacheEntryId = cacheEntryId;
  mCacheFetchCount = cacheFetchCount;
  mCacheExpirationTime = cacheExpirationTime;
  mCachedCharset = cachedCharset;
  mSelfAddr = selfAddr;
  mPeerAddr = peerAddr;

  mAvailableCachedAltDataType = altDataType;
  mDeliveringAltData = deliveringAltData;
  mAltDataLength = altDataLen;
  mResolvedByTRR = aIsResolvedByTRR;

  SetApplyConversion(aApplyConversion);

  mAfterOnStartRequestBegun = true;

  AutoEventEnqueuer ensureSerialDispatch(mEventQ);

  mCacheKey = cacheKey;

  // replace our request headers with what actually got sent in the parent
  mRequestHead.SetHeaders(requestHeaders);

  // Note: this is where we would notify "http-on-examine-response" observers.
  // We have deliberately disabled this for child processes (see bug 806753)
  //
  // gHttpHandler->OnExamineResponse(this);

  mTracingEnabled = false;

  mTransactionTimings = aTiming;

  mAllRedirectsSameOrigin = aAllRedirectsSameOrigin;

  DoOnStartRequest(this, nullptr);
}

class SyntheticDiversionListener final : public nsIStreamListener {
  RefPtr<HttpChannelChild> mChannel;

  ~SyntheticDiversionListener() = default;

 public:
  explicit SyntheticDiversionListener(HttpChannelChild* aChannel)
      : mChannel(aChannel) {
    MOZ_ASSERT(mChannel);
  }

  NS_IMETHOD
  OnStartRequest(nsIRequest* aRequest) override {
    MOZ_ASSERT_UNREACHABLE(
        "SyntheticDiversionListener should never see OnStartRequest");
    return NS_OK;
  }

  NS_IMETHOD
  OnStopRequest(nsIRequest* aRequest, nsresult aStatus) override {
    if (mChannel->CanSend()) {
      mChannel->SendDivertOnStopRequest(aStatus);
      mChannel->SendDivertComplete();
    }
    return NS_OK;
  }

  NS_IMETHOD
  OnDataAvailable(nsIRequest* aRequest, nsIInputStream* aInputStream,
                  uint64_t aOffset, uint32_t aCount) override {
    if (!mChannel->CanSend()) {
      aRequest->Cancel(NS_ERROR_ABORT);
      return NS_ERROR_ABORT;
    }

    nsAutoCString data;
    nsresult rv = NS_ConsumeStream(aInputStream, aCount, data);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aRequest->Cancel(rv);
      return rv;
    }

    mChannel->SendDivertOnDataAvailable(data, aOffset, aCount);
    return NS_OK;
  }

  NS_DECL_ISUPPORTS
};

NS_IMPL_ISUPPORTS(SyntheticDiversionListener, nsIStreamListener);

void HttpChannelChild::DoOnStartRequest(nsIRequest* aRequest,
                                        nsISupports* aContext) {
  nsresult rv;

  LOG(("HttpChannelChild::DoOnStartRequest [this=%p]\n", this));

  // In theory mListener should not be null, but in practice sometimes it is.
  MOZ_ASSERT(mListener);
  if (!mListener) {
    Cancel(NS_ERROR_FAILURE);
    return;
  }

  if (mSynthesizedResponsePump && mLoadFlags & LOAD_CALL_CONTENT_SNIFFERS) {
    mSynthesizedResponsePump->PeekStream(CallTypeSniffers,
                                         static_cast<nsIChannel*>(this));
  }

  if (mListener) {
    nsCOMPtr<nsIStreamListener> listener(mListener);
    mOnStartRequestCalled = true;
    rv = listener->OnStartRequest(aRequest);
  } else {
    rv = NS_ERROR_UNEXPECTED;
  }
  mOnStartRequestCalled = true;

  if (NS_FAILED(rv)) {
    Cancel(rv);
    return;
  }

  if (mDivertingToParent) {
    mListener = nullptr;
    mCompressListener = nullptr;
    if (mLoadGroup) {
      mLoadGroup->RemoveRequest(this, nullptr, mStatus);
    }

    // If the response has been synthesized in the child, then we are going
    // be getting OnDataAvailable and OnStopRequest from the synthetic
    // stream pump.  We need to forward these back to the parent diversion
    // listener.
    if (mSynthesizedResponse) {
      mListener = new SyntheticDiversionListener(this);
    }

    return;
  }

  nsCOMPtr<nsIStreamListener> listener;
  rv = DoApplyContentConversions(mListener, getter_AddRefs(listener), nullptr);
  if (NS_FAILED(rv)) {
    Cancel(rv);
  } else if (listener) {
    mListener = listener;
    mCompressListener = listener;
  }
}

class TransportAndDataEvent : public ChannelEvent {
 public:
  TransportAndDataEvent(HttpChannelChild* child, const nsresult& channelStatus,
                        const nsresult& transportStatus, const nsCString& data,
                        const uint64_t& offset, const uint32_t& count)
      : mChild(child),
        mChannelStatus(channelStatus),
        mTransportStatus(transportStatus),
        mData(data),
        mOffset(offset),
        mCount(count) {}

  void Run() override {
    mChild->OnTransportAndData(mChannelStatus, mTransportStatus, mOffset,
                               mCount, mData);
  }

  already_AddRefed<nsIEventTarget> GetEventTarget() override {
    MOZ_ASSERT(mChild);
    nsCOMPtr<nsIEventTarget> target = mChild->GetODATarget();
    return target.forget();
  }

 private:
  HttpChannelChild* mChild;
  nsresult mChannelStatus;
  nsresult mTransportStatus;
  nsCString mData;
  uint64_t mOffset;
  uint32_t mCount;
};

void HttpChannelChild::ProcessOnTransportAndData(
    const nsresult& aChannelStatus, const nsresult& aTransportStatus,
    const uint64_t& aOffset, const uint32_t& aCount, const nsCString& aData) {
  LOG(("HttpChannelChild::ProcessOnTransportAndData [this=%p]\n", this));
  MOZ_ASSERT(OnSocketThread());
  MOZ_RELEASE_ASSERT(!mFlushedForDiversion,
                     "Should not be receiving any more callbacks from parent!");
  mEventQ->RunOrEnqueue(
      new TransportAndDataEvent(this, aChannelStatus, aTransportStatus, aData,
                                aOffset, aCount),
      mDivertingToParent);
}

class MaybeDivertOnDataHttpEvent
    : public NeckoTargetChannelEvent<HttpChannelChild> {
 public:
  MaybeDivertOnDataHttpEvent(HttpChannelChild* child, const nsCString& data,
                             const uint64_t& offset, const uint32_t& count)
      : NeckoTargetChannelEvent<HttpChannelChild>(child),
        mData(data),
        mOffset(offset),
        mCount(count) {}

  void Run() override { mChild->MaybeDivertOnData(mData, mOffset, mCount); }

 private:
  nsCString mData;
  uint64_t mOffset;
  uint32_t mCount;
};

void HttpChannelChild::MaybeDivertOnData(const nsCString& data,
                                         const uint64_t& offset,
                                         const uint32_t& count) {
  LOG(("HttpChannelChild::MaybeDivertOnData [this=%p]", this));

  if (mDivertingToParent) {
    SendDivertOnDataAvailable(data, offset, count);
  }
}

void HttpChannelChild::OnTransportAndData(const nsresult& channelStatus,
                                          const nsresult& transportStatus,
                                          const uint64_t& offset,
                                          const uint32_t& count,
                                          const nsCString& data) {
  LOG(("HttpChannelChild::OnTransportAndData [this=%p]\n", this));

  if (!mCanceled && NS_SUCCEEDED(mStatus)) {
    mStatus = channelStatus;
  }

  // For diversion to parent, just SendDivertOnDataAvailable.
  if (mDivertingToParent) {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_RELEASE_ASSERT(
        !mFlushedForDiversion,
        "Should not be processing any more callbacks from parent!");

    SendDivertOnDataAvailable(data, offset, count);
    return;
  }

  if (mCanceled) return;

  if (mUnknownDecoderInvolved) {
    LOG(("UnknownDecoder is involved queue OnDataAvailable call. [this=%p]",
         this));
    MOZ_ASSERT(NS_IsMainThread());
    mUnknownDecoderEventQ.AppendElement(
        MakeUnique<MaybeDivertOnDataHttpEvent>(this, data, offset, count));
  }

  // Hold queue lock throughout all three calls, else we might process a later
  // necko msg in between them.
  AutoEventEnqueuer ensureSerialDispatch(mEventQ);

  int64_t progressMax;
  if (NS_FAILED(GetContentLength(&progressMax))) {
    progressMax = -1;
  }

  const int64_t progress = offset + count;

  // OnTransportAndData will be run on retargeted thread if applicable, however
  // OnStatus/OnProgress event can only be fired on main thread. We need to
  // dispatch the status/progress event handling back to main thread with the
  // appropriate event target for networking.
  if (NS_IsMainThread()) {
    DoOnStatus(this, transportStatus);
    DoOnProgress(this, progress, progressMax);
  } else {
    RefPtr<HttpChannelChild> self = this;
    nsCOMPtr<nsIEventTarget> neckoTarget = GetNeckoTarget();
    MOZ_ASSERT(neckoTarget);

    DebugOnly<nsresult> rv = neckoTarget->Dispatch(
        NS_NewRunnableFunction(
            "net::HttpChannelChild::OnTransportAndData",
            [self, transportStatus, progress, progressMax]() {
              self->DoOnStatus(self, transportStatus);
              self->DoOnProgress(self, progress, progressMax);
            }),
        NS_DISPATCH_NORMAL);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  // OnDataAvailable
  //
  // NOTE: the OnDataAvailable contract requires the client to read all the data
  // in the inputstream.  This code relies on that ('data' will go away after
  // this function).  Apparently the previous, non-e10s behavior was to actually
  // support only reading part of the data, allowing later calls to read the
  // rest.
  nsCOMPtr<nsIInputStream> stringStream;
  nsresult rv =
      NS_NewByteInputStream(getter_AddRefs(stringStream),
                            MakeSpan(data).To(count), NS_ASSIGNMENT_DEPEND);
  if (NS_FAILED(rv)) {
    Cancel(rv);
    return;
  }

  DoOnDataAvailable(this, nullptr, stringStream, offset, count);
  stringStream->Close();

  if (NeedToReportBytesRead()) {
    mUnreportBytesRead += count;
    if (mUnreportBytesRead >= gHttpHandler->SendWindowSize() >> 2) {
      if (NS_IsMainThread()) {
        Unused << SendBytesRead(mUnreportBytesRead);
      } else {
        // PHttpChannel connects to the main thread
        RefPtr<HttpChannelChild> self = this;
        int32_t bytesRead = mUnreportBytesRead;
        nsCOMPtr<nsIEventTarget> neckoTarget = GetNeckoTarget();
        MOZ_ASSERT(neckoTarget);

        DebugOnly<nsresult> rv = neckoTarget->Dispatch(
            NS_NewRunnableFunction("net::HttpChannelChild::SendBytesRead",
                                   [self, bytesRead]() {
                                     Unused << self->SendBytesRead(bytesRead);
                                   }),
            NS_DISPATCH_NORMAL);
        MOZ_ASSERT(NS_SUCCEEDED(rv));
      }
      mUnreportBytesRead = 0;
    }
  }
}

bool HttpChannelChild::NeedToReportBytesRead() {
  if (mCacheNeedToReportBytesReadInitialized) {
    // No need to send SendRecvBytes when diversion starts since the parent
    // process will suspend for diversion triggered in during OnStrartRequest at
    // child side, which is earlier. Parent will take over the flow control
    // after the diverting starts. Sending |SendBytesRead| is redundant.
    return mNeedToReportBytesRead && !mDivertingToParent;
  }

  // Might notify parent for partial cache, and the IPC message is ignored by
  // parent.
  int64_t contentLength = -1;
  if (gHttpHandler->SendWindowSize() == 0 || mIsFromCache ||
      NS_FAILED(GetContentLength(&contentLength)) ||
      contentLength < gHttpHandler->SendWindowSize()) {
    mNeedToReportBytesRead = false;
  }

  mCacheNeedToReportBytesReadInitialized = true;
  return mNeedToReportBytesRead;
}

void HttpChannelChild::DoOnStatus(nsIRequest* aRequest, nsresult status) {
  LOG(("HttpChannelChild::DoOnStatus [this=%p]\n", this));
  MOZ_ASSERT(NS_IsMainThread());

  if (mCanceled) return;

  // cache the progress sink so we don't have to query for it each time.
  if (!mProgressSink) GetCallback(mProgressSink);

  // Temporary fix for bug 1116124
  // See 1124971 - Child removes LOAD_BACKGROUND flag from channel
  if (status == NS_OK) return;

  // block status/progress after Cancel or OnStopRequest has been called,
  // or if channel has LOAD_BACKGROUND set.
  if (mProgressSink && NS_SUCCEEDED(mStatus) && mIsPending &&
      !(mLoadFlags & LOAD_BACKGROUND)) {
    // OnStatus
    //
    MOZ_ASSERT(status == NS_NET_STATUS_RECEIVING_FROM ||
               status == NS_NET_STATUS_READING);

    nsAutoCString host;
    mURI->GetHost(host);
    mProgressSink->OnStatus(aRequest, nullptr, status,
                            NS_ConvertUTF8toUTF16(host).get());
  }
}

void HttpChannelChild::DoOnProgress(nsIRequest* aRequest, int64_t progress,
                                    int64_t progressMax) {
  LOG(("HttpChannelChild::DoOnProgress [this=%p]\n", this));
  MOZ_ASSERT(NS_IsMainThread());

  if (mCanceled) return;

  // cache the progress sink so we don't have to query for it each time.
  if (!mProgressSink) GetCallback(mProgressSink);

  // block status/progress after Cancel or OnStopRequest has been called,
  // or if channel has LOAD_BACKGROUND set.
  if (mProgressSink && NS_SUCCEEDED(mStatus) && mIsPending &&
      !(mLoadFlags & LOAD_BACKGROUND)) {
    // OnProgress
    //
    if (progress > 0) {
      mProgressSink->OnProgress(aRequest, nullptr, progress, progressMax);
    }
  }
}

void HttpChannelChild::DoOnDataAvailable(nsIRequest* aRequest,
                                         nsISupports* aContext,
                                         nsIInputStream* aStream,
                                         uint64_t offset, uint32_t count) {
  AUTO_PROFILER_LABEL("HttpChannelChild::DoOnDataAvailable", NETWORK);
  LOG(("HttpChannelChild::DoOnDataAvailable [this=%p]\n", this));
  if (mCanceled) return;

  if (mListener) {
    nsCOMPtr<nsIStreamListener> listener(mListener);
    nsresult rv = listener->OnDataAvailable(aRequest, aStream, offset, count);
    if (NS_FAILED(rv)) {
      CancelOnMainThread(rv);
    }
  }
}

class StopRequestEvent : public NeckoTargetChannelEvent<HttpChannelChild> {
 public:
  StopRequestEvent(HttpChannelChild* child, const nsresult& channelStatus,
                   const ResourceTimingStruct& timing,
                   const nsHttpHeaderArray& aResponseTrailers)
      : NeckoTargetChannelEvent<HttpChannelChild>(child),
        mChannelStatus(channelStatus),
        mTiming(timing),
        mResponseTrailers(aResponseTrailers) {}

  void Run() override {
    mChild->OnStopRequest(mChannelStatus, mTiming, mResponseTrailers);
  }

 private:
  nsresult mChannelStatus;
  ResourceTimingStruct mTiming;
  nsHttpHeaderArray mResponseTrailers;
};

void HttpChannelChild::ProcessOnStopRequest(
    const nsresult& aChannelStatus, const ResourceTimingStruct& aTiming,
    const nsHttpHeaderArray& aResponseTrailers) {
  LOG(("HttpChannelChild::ProcessOnStopRequest [this=%p]\n", this));
  MOZ_ASSERT(OnSocketThread());
  MOZ_RELEASE_ASSERT(!mFlushedForDiversion,
                     "Should not be receiving any more callbacks from parent!");

  mEventQ->RunOrEnqueue(
      new StopRequestEvent(this, aChannelStatus, aTiming, aResponseTrailers),
      mDivertingToParent);
}

class MaybeDivertOnStopHttpEvent
    : public NeckoTargetChannelEvent<HttpChannelChild> {
 public:
  MaybeDivertOnStopHttpEvent(HttpChannelChild* child,
                             const nsresult& channelStatus)
      : NeckoTargetChannelEvent<HttpChannelChild>(child),
        mChannelStatus(channelStatus) {}

  void Run() override { mChild->MaybeDivertOnStop(mChannelStatus); }

 private:
  nsresult mChannelStatus;
};

void HttpChannelChild::MaybeDivertOnStop(const nsresult& aChannelStatus) {
  LOG(
      ("HttpChannelChild::MaybeDivertOnStop [this=%p, "
       "mDivertingToParent=%d status=%" PRIx32 "]",
       this, static_cast<bool>(mDivertingToParent),
       static_cast<uint32_t>(aChannelStatus)));
  if (mDivertingToParent) {
    SendDivertOnStopRequest(aChannelStatus);
  }
}

void HttpChannelChild::OnStopRequest(
    const nsresult& channelStatus, const ResourceTimingStruct& timing,
    const nsHttpHeaderArray& aResponseTrailers) {
  LOG(("HttpChannelChild::OnStopRequest [this=%p status=%" PRIx32 "]\n", this,
       static_cast<uint32_t>(channelStatus)));
  MOZ_ASSERT(NS_IsMainThread());

  // If this channel was aborted by ActorDestroy, then there may be other
  // OnStartRequest/OnStopRequest/OnDataAvailable IPC messages that need to
  // be handled. In that case we just ignore them to avoid calling the listener
  // twice.
  if (mOnStopRequestCalled && mIPCActorDeleted) {
    return;
  }

  if (mDivertingToParent) {
    MOZ_RELEASE_ASSERT(
        !mFlushedForDiversion,
        "Should not be processing any more callbacks from parent!");

    SendDivertOnStopRequest(channelStatus);
    return;
  }

  if (mUnknownDecoderInvolved) {
    LOG(("UnknownDecoder is involved queue OnStopRequest call. [this=%p]",
         this));
    MOZ_ASSERT(NS_IsMainThread());
    mUnknownDecoderEventQ.AppendElement(
        MakeUnique<MaybeDivertOnStopHttpEvent>(this, channelStatus));
  }

  nsCOMPtr<nsICompressConvStats> conv = do_QueryInterface(mCompressListener);
  if (conv) {
    conv->GetDecodedDataLength(&mDecodedBodySize);
  }

  mTransactionTimings.domainLookupStart = timing.domainLookupStart;
  mTransactionTimings.domainLookupEnd = timing.domainLookupEnd;
  mTransactionTimings.connectStart = timing.connectStart;
  mTransactionTimings.tcpConnectEnd = timing.tcpConnectEnd;
  mTransactionTimings.secureConnectionStart = timing.secureConnectionStart;
  mTransactionTimings.connectEnd = timing.connectEnd;
  mTransactionTimings.requestStart = timing.requestStart;
  mTransactionTimings.responseStart = timing.responseStart;
  mTransactionTimings.responseEnd = timing.responseEnd;

  // Do not overwrite or adjust the original mAsyncOpenTime by timing.fetchStart
  // We must use the original child process time in order to account for child
  // side work and IPC transit overhead.
  // XXX: This depends on TimeStamp being equivalent across processes.
  // This is true for modern hardware but for older platforms it is not always
  // true.

  mRedirectStartTimeStamp = timing.redirectStart;
  mRedirectEndTimeStamp = timing.redirectEnd;
  mTransferSize = timing.transferSize;
  mEncodedBodySize = timing.encodedBodySize;
  mProtocolVersion = timing.protocolVersion;

  mCacheReadStart = timing.cacheReadStart;
  mCacheReadEnd = timing.cacheReadEnd;

#ifdef MOZ_GECKO_PROFILER
  if (profiler_is_active()) {
    int32_t priority = PRIORITY_NORMAL;
    GetPriority(&priority);
    profiler_add_network_marker(mURI, priority, mChannelId,
                                NetworkLoadType::LOAD_STOP, mLastStatusReported,
                                TimeStamp::Now(), mTransferSize, kCacheUnknown,
                                &mTransactionTimings);
  }
#endif

  mResponseTrailers = new nsHttpHeaderArray(aResponseTrailers);

  DoPreOnStopRequest(channelStatus);

  {  // We must flush the queue before we Send__delete__
    // (although we really shouldn't receive any msgs after OnStop),
    // so make sure this goes out of scope before then.
    AutoEventEnqueuer ensureSerialDispatch(mEventQ);

    DoOnStopRequest(this, channelStatus, nullptr);
    // DoOnStopRequest() calls ReleaseListeners()
  }

  // If unknownDecoder is involved and the received content is short we will
  // know whether we need to divert to parent only after OnStopRequest of the
  // listeners chain is called in DoOnStopRequest. At that moment
  // unknownDecoder will call OnStartRequest of the real listeners of the
  // channel including the OnStopRequest of UrlLoader which decides whether we
  // need to divert to parent.
  // If we are diverting to parent we should not do a cleanup.
  if (mDivertingToParent) {
    LOG(
        ("HttpChannelChild::OnStopRequest  - We are diverting to parent, "
         "postpone cleaning up."));
    return;
  }

  CleanupBackgroundChannel();

  // If there is a possibility we might want to write alt data to the cache
  // entry, we keep the channel alive. We still send the DocumentChannelCleanup
  // message but request the cache entry to be kept by the parent.
  // If the channel has failed, the cache entry is in a non-writtable state and
  // we want to release it to not block following consumers.
  if (NS_SUCCEEDED(channelStatus) && !mPreferredCachedAltDataTypes.IsEmpty()) {
    mKeptAlive = true;
    SendDocumentChannelCleanup(false);  // don't clear cache entry
    return;
  }

  if (mLoadFlags & LOAD_DOCUMENT_URI) {
    // Keep IPDL channel open, but only for updating security info.
    // If IPDL is already closed, then do nothing.
    if (CanSend()) {
      mKeptAlive = true;
      SendDocumentChannelCleanup(true);
    }
  } else {
    // The parent process will respond by sending a DeleteSelf message and
    // making sure not to send any more messages after that.
    TrySendDeletingChannel();
  }
}

void HttpChannelChild::DoPreOnStopRequest(nsresult aStatus) {
  AUTO_PROFILER_LABEL("HttpChannelChild::DoPreOnStopRequest", NETWORK);
  LOG(("HttpChannelChild::DoPreOnStopRequest [this=%p status=%" PRIx32 "]\n",
       this, static_cast<uint32_t>(aStatus)));
  mIsPending = false;

  MaybeCallSynthesizedCallback();

  MaybeReportTimingData();

  if (!mCanceled && NS_SUCCEEDED(mStatus)) {
    mStatus = aStatus;
  }

  CollectOMTTelemetry();
}

void HttpChannelChild::CollectOMTTelemetry() {
  MOZ_ASSERT(NS_IsMainThread());

  // Only collect telemetry for HTTP channel that is loaded successfully and
  // completely.
  if (mCanceled || NS_FAILED(mStatus)) {
    return;
  }

  // Use content policy type to accumulate data by usage.
  nsContentPolicyType type = mLoadInfo ? mLoadInfo->InternalContentPolicyType()
                                       : nsIContentPolicy::TYPE_OTHER;

  nsAutoCString key(NS_CP_ContentTypeName(type));

  Telemetry::AccumulateCategoricalKeyed(key, mOMTResult);
}

void HttpChannelChild::DoOnStopRequest(nsIRequest* aRequest,
                                       nsresult aChannelStatus,
                                       nsISupports* aContext) {
  AUTO_PROFILER_LABEL("HttpChannelChild::DoOnStopRequest", NETWORK);
  LOG(("HttpChannelChild::DoOnStopRequest [this=%p]\n", this));
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mIsPending);

  auto checkForBlockedContent = [&]() {
    // NB: We use aChannelStatus here instead of mStatus because if there was an
    // nsCORSListenerProxy on this request, it will override the tracking
    // protection's return value.
    if (UrlClassifierFeatureFactory::IsClassifierBlockingErrorCode(
            aChannelStatus) ||
        aChannelStatus == NS_ERROR_MALWARE_URI ||
        aChannelStatus == NS_ERROR_UNWANTED_URI ||
        aChannelStatus == NS_ERROR_BLOCKED_URI ||
        aChannelStatus == NS_ERROR_HARMFUL_URI ||
        aChannelStatus == NS_ERROR_PHISHING_URI) {
      nsCString list, provider, fullhash;

      nsresult rv = GetMatchedList(list);
      NS_ENSURE_SUCCESS_VOID(rv);

      rv = GetMatchedProvider(provider);
      NS_ENSURE_SUCCESS_VOID(rv);

      rv = GetMatchedFullHash(fullhash);
      NS_ENSURE_SUCCESS_VOID(rv);

      UrlClassifierCommon::SetBlockedContent(this, aChannelStatus, list,
                                             provider, fullhash);
    }
  };
  checkForBlockedContent();

  MOZ_ASSERT(!mOnStopRequestCalled, "We should not call OnStopRequest twice");

  // In theory mListener should not be null, but in practice sometimes it is.
  MOZ_ASSERT(mListener);
  if (mListener) {
    nsCOMPtr<nsIStreamListener> listener(mListener);
    mOnStopRequestCalled = true;
    listener->OnStopRequest(aRequest, mStatus);
  }
  mOnStopRequestCalled = true;

  // notify "http-on-stop-connect" observers
  gHttpHandler->OnStopRequest(this);

  ReleaseListeners();

  // If a preferred alt-data type was set, the parent would hold a reference to
  // the cache entry in case the child calls openAlternativeOutputStream().
  // (see nsHttpChannel::OnStopRequest)
  if (!mPreferredCachedAltDataTypes.IsEmpty()) {
    mAltDataCacheEntryAvailable = mCacheEntryAvailable;
  }
  mCacheEntryAvailable = false;

  if (mLoadGroup) mLoadGroup->RemoveRequest(this, nullptr, mStatus);
}

class ProgressEvent : public NeckoTargetChannelEvent<HttpChannelChild> {
 public:
  ProgressEvent(HttpChannelChild* child, const int64_t& progress,
                const int64_t& progressMax)
      : NeckoTargetChannelEvent<HttpChannelChild>(child),
        mProgress(progress),
        mProgressMax(progressMax) {}

  void Run() override { mChild->OnProgress(mProgress, mProgressMax); }

 private:
  int64_t mProgress, mProgressMax;
};

void HttpChannelChild::ProcessOnProgress(const int64_t& aProgress,
                                         const int64_t& aProgressMax) {
  LOG(("HttpChannelChild::ProcessOnProgress [this=%p]\n", this));
  MOZ_ASSERT(OnSocketThread());
  mEventQ->RunOrEnqueue(new ProgressEvent(this, aProgress, aProgressMax));
}

void HttpChannelChild::OnProgress(const int64_t& progress,
                                  const int64_t& progressMax) {
  AUTO_PROFILER_LABEL("HttpChannelChild::OnProgress", NETWORK);
  LOG(("HttpChannelChild::OnProgress [this=%p progress=%" PRId64 "/%" PRId64
       "]\n",
       this, progress, progressMax));

  if (mCanceled) return;

  // cache the progress sink so we don't have to query for it each time.
  if (!mProgressSink) {
    GetCallback(mProgressSink);
  }

  AutoEventEnqueuer ensureSerialDispatch(mEventQ);

  // Block socket status event after Cancel or OnStopRequest has been called.
  if (mProgressSink && NS_SUCCEEDED(mStatus) && mIsPending) {
    if (progress > 0) {
      mProgressSink->OnProgress(this, nullptr, progress, progressMax);
    }
  }
}

class StatusEvent : public NeckoTargetChannelEvent<HttpChannelChild> {
 public:
  StatusEvent(HttpChannelChild* child, const nsresult& status)
      : NeckoTargetChannelEvent<HttpChannelChild>(child), mStatus(status) {}

  void Run() override { mChild->OnStatus(mStatus); }

 private:
  nsresult mStatus;
};

void HttpChannelChild::ProcessOnStatus(const nsresult& aStatus) {
  LOG(("HttpChannelChild::ProcessOnStatus [this=%p]\n", this));
  MOZ_ASSERT(OnSocketThread());
  mEventQ->RunOrEnqueue(new StatusEvent(this, aStatus));
}

void HttpChannelChild::OnStatus(const nsresult& status) {
  AUTO_PROFILER_LABEL("HttpChannelChild::OnStatus", NETWORK);
  LOG(("HttpChannelChild::OnStatus [this=%p status=%" PRIx32 "]\n", this,
       static_cast<uint32_t>(status)));

  if (mCanceled) return;

  // cache the progress sink so we don't have to query for it each time.
  if (!mProgressSink) GetCallback(mProgressSink);

  AutoEventEnqueuer ensureSerialDispatch(mEventQ);

  // block socket status event after Cancel or OnStopRequest has been called,
  // or if channel has LOAD_BACKGROUND set
  if (mProgressSink && NS_SUCCEEDED(mStatus) && mIsPending &&
      !(mLoadFlags & LOAD_BACKGROUND)) {
    nsAutoCString host;
    mURI->GetHost(host);
    mProgressSink->OnStatus(this, nullptr, status,
                            NS_ConvertUTF8toUTF16(host).get());
  }
}

class FailedAsyncOpenEvent : public NeckoTargetChannelEvent<HttpChannelChild> {
 public:
  FailedAsyncOpenEvent(HttpChannelChild* child, const nsresult& status)
      : NeckoTargetChannelEvent<HttpChannelChild>(child), mStatus(status) {}

  void Run() override { mChild->FailedAsyncOpen(mStatus); }

 private:
  nsresult mStatus;
};

mozilla::ipc::IPCResult HttpChannelChild::RecvFailedAsyncOpen(
    const nsresult& status) {
  LOG(("HttpChannelChild::RecvFailedAsyncOpen [this=%p]\n", this));
  mEventQ->RunOrEnqueue(new FailedAsyncOpenEvent(this, status));
  return IPC_OK();
}

// We need to have an implementation of this function just so that we can keep
// all references to mCallOnResume of type HttpChannelChild:  it's not OK in C++
// to set a member function ptr to a base class function.
void HttpChannelChild::HandleAsyncAbort() {
  HttpAsyncAborter<HttpChannelChild>::HandleAsyncAbort();

  // Ignore all the messages from background channel after channel aborted.
  CleanupBackgroundChannel();
}

void HttpChannelChild::FailedAsyncOpen(const nsresult& status) {
  LOG(("HttpChannelChild::FailedAsyncOpen [this=%p status=%" PRIx32 "]\n", this,
       static_cast<uint32_t>(status)));
  MOZ_ASSERT(NS_IsMainThread());

  // Might be called twice in race condition in theory.
  // (one by RecvFailedAsyncOpen, another by
  // HttpBackgroundChannelChild::ActorFailed)
  if (mOnStartRequestCalled) {
    return;
  }

  if (NS_SUCCEEDED(mStatus)) {
    mStatus = status;
  }

  // We're already being called from IPDL, therefore already "async"
  HandleAsyncAbort();

  if (CanSend()) {
    TrySendDeletingChannel();
  }
}

void HttpChannelChild::CleanupBackgroundChannel() {
  MutexAutoLock lock(mBgChildMutex);

  AUTO_PROFILER_LABEL("HttpChannelChild::CleanupBackgroundChannel", NETWORK);
  LOG(("HttpChannelChild::CleanupBackgroundChannel [this=%p bgChild=%p]\n",
       this, mBgChild.get()));

  mBgInitFailCallback = nullptr;

  if (!mBgChild) {
    return;
  }

  RefPtr<HttpBackgroundChannelChild> bgChild = mBgChild.forget();

  MOZ_RELEASE_ASSERT(gSocketTransportService);
  if (!OnSocketThread()) {
    gSocketTransportService->Dispatch(
        NewRunnableMethod("HttpBackgroundChannelChild::OnChannelClosed",
                          bgChild,
                          &HttpBackgroundChannelChild::OnChannelClosed),
        NS_DISPATCH_NORMAL);
  } else {
    bgChild->OnChannelClosed();
  }
}

void HttpChannelChild::DoNotifyListenerCleanup() {
  LOG(("HttpChannelChild::DoNotifyListenerCleanup [this=%p]\n", this));

  if (mInterceptListener) {
    mInterceptListener->Cleanup();
    mInterceptListener = nullptr;
  }

  MaybeCallSynthesizedCallback();
}

void HttpChannelChild::DoAsyncAbort(nsresult aStatus) {
  Unused << AsyncAbort(aStatus);
}

class DeleteSelfEvent : public NeckoTargetChannelEvent<HttpChannelChild> {
 public:
  explicit DeleteSelfEvent(HttpChannelChild* child)
      : NeckoTargetChannelEvent<HttpChannelChild>(child) {}
  void Run() override { mChild->DeleteSelf(); }
};

mozilla::ipc::IPCResult HttpChannelChild::RecvDeleteSelf() {
  LOG(("HttpChannelChild::RecvDeleteSelf [this=%p]\n", this));
  mEventQ->RunOrEnqueue(new DeleteSelfEvent(this));
  return IPC_OK();
}

HttpChannelChild::OverrideRunnable::OverrideRunnable(
    HttpChannelChild* aChannel, HttpChannelChild* aNewChannel,
    InterceptStreamListener* aListener, nsIInputStream* aInput,
    nsIInterceptedBodyCallback* aCallback, nsAutoPtr<nsHttpResponseHead>& aHead,
    nsICacheInfoChannel* aCacheInfo)
    : Runnable("net::HttpChannelChild::OverrideRunnable") {
  mChannel = aChannel;
  mNewChannel = aNewChannel;
  mListener = aListener;
  mInput = aInput;
  mCallback = aCallback;
  mHead = aHead;
  mSynthesizedCacheInfo = aCacheInfo;
}

void HttpChannelChild::OverrideRunnable::OverrideWithSynthesizedResponse() {
  if (mNewChannel) {
    mNewChannel->OverrideWithSynthesizedResponse(
        mHead, mInput, mCallback, mListener, mSynthesizedCacheInfo);
  }
}

NS_IMETHODIMP
HttpChannelChild::OverrideRunnable::Run() {
  // Check to see if the channel was canceled in the middle of the redirect.
  nsresult rv = NS_OK;
  Unused << mChannel->GetStatus(&rv);
  if (NS_FAILED(rv)) {
    if (mCallback) {
      mCallback->BodyComplete(rv);
      mCallback = nullptr;
    }
    mChannel->CleanupRedirectingChannel(rv);
    if (mNewChannel) {
      mNewChannel->Cancel(rv);
    }
    return NS_OK;
  }

  bool ret = mChannel->Redirect3Complete(this);

  // If the method returns false, it means the IPDL connection is being
  // asyncly torn down and reopened, and OverrideWithSynthesizedResponse
  // will be called later from FinishInterceptedRedirect. This object will
  // be assigned to HttpChannelChild::mOverrideRunnable in order to do so.
  // If it is true, we can call the method right now.
  if (ret) {
    OverrideWithSynthesizedResponse();
  }

  return NS_OK;
}

mozilla::ipc::IPCResult HttpChannelChild::RecvFinishInterceptedRedirect() {
  // Hold a ref to this to keep it from being deleted by Send__delete__()
  RefPtr<HttpChannelChild> self(this);
  Send__delete__(this);

  {
    // Reset the event target since the IPC actor is about to be destroyed.
    // Following channel event should be handled on main thread.
    MutexAutoLock lock(mEventTargetMutex);
    mNeckoTarget = nullptr;
  }

  // The IPDL connection was torn down by a interception logic in
  // CompleteRedirectSetup, and we need to call FinishInterceptedRedirect.
  nsCOMPtr<nsIEventTarget> neckoTarget = GetNeckoTarget();
  MOZ_ASSERT(neckoTarget);

  Unused << neckoTarget->Dispatch(
      NewRunnableMethod("net::HttpChannelChild::FinishInterceptedRedirect",
                        this, &HttpChannelChild::FinishInterceptedRedirect),
      NS_DISPATCH_NORMAL);

  return IPC_OK();
}

void HttpChannelChild::DeleteSelf() { Send__delete__(this); }

class ContinueDoNotifyListenerEvent
    : public NeckoTargetChannelEvent<HttpChannelChild> {
 public:
  explicit ContinueDoNotifyListenerEvent(HttpChannelChild* child)
      : NeckoTargetChannelEvent<HttpChannelChild>(child) {}
  void Run() override { mChild->ContinueDoNotifyListener(); }
};

void HttpChannelChild::DoNotifyListener() {
  LOG(("HttpChannelChild::DoNotifyListener this=%p", this));
  MOZ_ASSERT(NS_IsMainThread());

  // In case nsHttpChannel::OnStartRequest wasn't called (e.g. due to flag
  // LOAD_ONLY_IF_MODIFIED) we want to set mAfterOnStartRequestBegun to true
  // before notifying listener.
  if (!mAfterOnStartRequestBegun) {
    mAfterOnStartRequestBegun = true;
  }

  if (mListener && !mOnStartRequestCalled) {
    nsCOMPtr<nsIStreamListener> listener = mListener;
    mOnStartRequestCalled = true;  // avoid reentrancy bugs by setting this now
    listener->OnStartRequest(this);
  }
  mOnStartRequestCalled = true;

  mEventQ->RunOrEnqueue(new ContinueDoNotifyListenerEvent(this));
}

void HttpChannelChild::ContinueDoNotifyListener() {
  LOG(("HttpChannelChild::ContinueDoNotifyListener this=%p", this));
  MOZ_ASSERT(NS_IsMainThread());

  // Make sure mIsPending is set to false. At this moment we are done from
  // the point of view of our consumer and we have to report our self
  // as not-pending.
  mIsPending = false;

  if (mListener && !mOnStopRequestCalled) {
    nsCOMPtr<nsIStreamListener> listener = mListener;
    mOnStopRequestCalled = true;
    listener->OnStopRequest(this, mStatus);
  }
  mOnStopRequestCalled = true;

  // notify "http-on-stop-request" observers
  gHttpHandler->OnStopRequest(this);

  // This channel has finished its job, potentially release any tail-blocked
  // requests with this.
  RemoveAsNonTailRequest();

  // We have to make sure to drop the references to listeners and callbacks
  // no longer needed.
  ReleaseListeners();

  DoNotifyListenerCleanup();

  // If this is a navigation, then we must let the docshell flush the reports
  // to the console later.  The LoadDocument() is pointing at the detached
  // document that started the navigation.  We want to show the reports on the
  // new document.  Otherwise the console is wiped and the user never sees
  // the information.
  if (!IsNavigation()) {
    if (mLoadGroup) {
      FlushConsoleReports(mLoadGroup);
    } else if (mLoadInfo) {
      RefPtr<dom::Document> doc;
      mLoadInfo->GetLoadingDocument(getter_AddRefs(doc));
      FlushConsoleReports(doc);
    }
  }
}

void HttpChannelChild::FinishInterceptedRedirect() {
  nsresult rv;
  MOZ_ASSERT(!mInterceptedRedirectContext, "the context should be null!");
  rv = AsyncOpen(mInterceptedRedirectListener);

  mInterceptedRedirectListener = nullptr;
  mInterceptedRedirectContext = nullptr;

  if (mInterceptingChannel) {
    mInterceptingChannel->CleanupRedirectingChannel(rv);
    mInterceptingChannel = nullptr;
  }

  if (mOverrideRunnable) {
    mOverrideRunnable->OverrideWithSynthesizedResponse();
    mOverrideRunnable = nullptr;
  }
}

mozilla::ipc::IPCResult HttpChannelChild::RecvReportSecurityMessage(
    const nsString& messageTag, const nsString& messageCategory) {
  DebugOnly<nsresult> rv = AddSecurityMessage(messageTag, messageCategory);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  return IPC_OK();
}

class Redirect1Event : public NeckoTargetChannelEvent<HttpChannelChild> {
 public:
  Redirect1Event(HttpChannelChild* child, const uint32_t& registrarId,
                 const URIParams& newURI, const uint32_t& newLoadFlags,
                 const uint32_t& redirectFlags,
                 const ParentLoadInfoForwarderArgs& loadInfoForwarder,
                 const nsHttpResponseHead& responseHead,
                 const nsACString& securityInfoSerialization,
                 const uint64_t& channelId, const ResourceTimingStruct& timing)
      : NeckoTargetChannelEvent<HttpChannelChild>(child),
        mRegistrarId(registrarId),
        mNewURI(newURI),
        mNewLoadFlags(newLoadFlags),
        mRedirectFlags(redirectFlags),
        mResponseHead(responseHead),
        mSecurityInfoSerialization(securityInfoSerialization),
        mChannelId(channelId),
        mLoadInfoForwarder(loadInfoForwarder),
        mTiming(timing) {}

  void Run() override {
    mChild->Redirect1Begin(mRegistrarId, mNewURI, mNewLoadFlags, mRedirectFlags,
                           mLoadInfoForwarder, mResponseHead,
                           mSecurityInfoSerialization, mChannelId, mTiming);
  }

 private:
  uint32_t mRegistrarId;
  URIParams mNewURI;
  uint32_t mNewLoadFlags;
  uint32_t mRedirectFlags;
  nsHttpResponseHead mResponseHead;
  nsCString mSecurityInfoSerialization;
  uint64_t mChannelId;
  ParentLoadInfoForwarderArgs mLoadInfoForwarder;
  ResourceTimingStruct mTiming;
};

mozilla::ipc::IPCResult HttpChannelChild::RecvRedirect1Begin(
    const uint32_t& registrarId, const URIParams& newUri,
    const uint32_t& newLoadFlags, const uint32_t& redirectFlags,
    const ParentLoadInfoForwarderArgs& loadInfoForwarder,
    const nsHttpResponseHead& responseHead,
    const nsCString& securityInfoSerialization, const uint64_t& channelId,
    const NetAddr& oldPeerAddr, const ResourceTimingStruct& timing) {
  // TODO: handle security info
  LOG(("HttpChannelChild::RecvRedirect1Begin [this=%p]\n", this));
  // We set peer address of child to the old peer,
  // Then it will be updated to new peer in OnStartRequest
  mPeerAddr = oldPeerAddr;

  // Cookies headers should not be visible to the child process
  MOZ_ASSERT(!nsHttpResponseHead(responseHead).HasHeader(nsHttp::Set_Cookie));

  mEventQ->RunOrEnqueue(new Redirect1Event(
      this, registrarId, newUri, newLoadFlags, redirectFlags, loadInfoForwarder,
      responseHead, securityInfoSerialization, channelId, timing));
  return IPC_OK();
}

nsresult HttpChannelChild::SetupRedirect(nsIURI* uri,
                                         const nsHttpResponseHead* responseHead,
                                         const uint32_t& redirectFlags,
                                         nsIChannel** outChannel) {
  LOG(("HttpChannelChild::SetupRedirect [this=%p]\n", this));

  nsresult rv;
  nsCOMPtr<nsIIOService> ioService;
  rv = gHttpHandler->GetIOService(getter_AddRefs(ioService));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIChannel> newChannel;
  nsCOMPtr<nsILoadInfo> redirectLoadInfo =
      CloneLoadInfoForRedirect(uri, redirectFlags);
  rv = NS_NewChannelInternal(getter_AddRefs(newChannel), uri, redirectLoadInfo,
                             nullptr,  // PerformanceStorage
                             nullptr,  // aLoadGroup
                             nullptr,  // aCallbacks
                             nsIRequest::LOAD_NORMAL, ioService);
  NS_ENSURE_SUCCESS(rv, rv);

  // We won't get OnStartRequest, set cookies here.
  mResponseHead = new nsHttpResponseHead(*responseHead);

  bool rewriteToGET = HttpBaseChannel::ShouldRewriteRedirectToGET(
      mResponseHead->Status(), mRequestHead.ParsedMethod());

  rv = SetupReplacementChannel(uri, newChannel, !rewriteToGET, redirectFlags);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHttpChannelChild> httpChannelChild =
      do_QueryInterface(newChannel);
  if (httpChannelChild) {
    bool shouldUpgrade = false;
    auto channelChild = static_cast<HttpChannelChild*>(httpChannelChild.get());
    if (mShouldInterceptSubsequentRedirect) {
      // In the case where there was a synthesized response that caused a
      // redirection, we must force the new channel to intercept the request in
      // the parent before a network transaction is initiated.
      rv = httpChannelChild->ForceIntercepted(false, false);
    } else if (mRedirectMode == nsIHttpChannelInternal::REDIRECT_MODE_MANUAL &&
               ((redirectFlags & (nsIChannelEventSink::REDIRECT_TEMPORARY |
                                  nsIChannelEventSink::REDIRECT_PERMANENT)) !=
                0) &&
               channelChild->ShouldInterceptURI(uri, shouldUpgrade)) {
      // In the case where the redirect mode is manual, we need to check whether
      // the post-redirect channel needs to be intercepted.  If that is the
      // case, force the new channel to intercept the request in the parent
      // similar to the case above, but also remember that ShouldInterceptURI()
      // returned true to avoid calling it a second time.
      rv = httpChannelChild->ForceIntercepted(true, shouldUpgrade);
    }
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  mRedirectChannelChild = do_QueryInterface(newChannel);
  newChannel.forget(outChannel);

  return NS_OK;
}

void HttpChannelChild::Redirect1Begin(
    const uint32_t& registrarId, const URIParams& newOriginalURI,
    const uint32_t& newLoadFlags, const uint32_t& redirectFlags,
    const ParentLoadInfoForwarderArgs& loadInfoForwarder,
    const nsHttpResponseHead& responseHead,
    const nsACString& securityInfoSerialization, const uint64_t& channelId,
    const ResourceTimingStruct& timing) {
  nsresult rv;

  LOG(("HttpChannelChild::Redirect1Begin [this=%p]\n", this));

  ipc::MergeParentLoadInfoForwarder(loadInfoForwarder, mLoadInfo);

  nsCOMPtr<nsIURI> uri = DeserializeURI(newOriginalURI);

  mTransactionTimings = timing;
  PROFILER_ADD_NETWORK_MARKER(mURI, mPriority, mChannelId,
                              NetworkLoadType::LOAD_REDIRECT,
                              mLastStatusReported, TimeStamp::Now(), 0,
                              kCacheUnknown, &mTransactionTimings, uri);

  if (!securityInfoSerialization.IsEmpty()) {
    rv = NS_DeserializeObject(securityInfoSerialization,
                              getter_AddRefs(mSecurityInfo));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                          "Deserializing security info should not fail");
  }

  nsCOMPtr<nsIChannel> newChannel;
  rv = SetupRedirect(uri, &responseHead, redirectFlags,
                     getter_AddRefs(newChannel));

  if (NS_SUCCEEDED(rv)) {
    MOZ_ALWAYS_SUCCEEDS(newChannel->SetLoadFlags(newLoadFlags));

    if (mRedirectChannelChild) {
      // Set the channelId allocated in parent to the child instance
      nsCOMPtr<nsIHttpChannel> httpChannel =
          do_QueryInterface(mRedirectChannelChild);
      if (httpChannel) {
        rv = httpChannel->SetChannelId(channelId);
        MOZ_ASSERT(NS_SUCCEEDED(rv));
      }
      mRedirectChannelChild->ConnectParent(registrarId);
    }

    nsCOMPtr<nsIEventTarget> target = GetNeckoTarget();
    MOZ_ASSERT(target);

    rv = gHttpHandler->AsyncOnChannelRedirect(this, newChannel, redirectFlags,
                                              target);
  }

  if (NS_FAILED(rv)) OnRedirectVerifyCallback(rv);
}

void HttpChannelChild::BeginNonIPCRedirect(
    nsIURI* responseURI, const nsHttpResponseHead* responseHead,
    bool aResponseRedirected) {
  LOG(("HttpChannelChild::BeginNonIPCRedirect [this=%p]\n", this));

  // This method is only used by child-side service workers.  It should not be
  // used by new code.  We want to remove it in the future.
  MOZ_DIAGNOSTIC_ASSERT(mSynthesizedResponse);

  // If the response has been redirected, propagate all the URLs to content.
  // Thus, the exact value of the redirect flag does not matter as long as it's
  // not REDIRECT_INTERNAL.
  const uint32_t redirectFlag = aResponseRedirected
                                    ? nsIChannelEventSink::REDIRECT_TEMPORARY
                                    : nsIChannelEventSink::REDIRECT_INTERNAL;

  nsCOMPtr<nsIChannel> newChannel;
  nsresult rv = SetupRedirect(responseURI, responseHead, redirectFlag,
                              getter_AddRefs(newChannel));

  if (NS_SUCCEEDED(rv)) {
    // Ensure that the new channel shares the original channel's security
    // information, since it won't be provided via IPC. In particular, if the
    // target of this redirect is a synthesized response that has its own
    // security info, the pre-redirect channel has already received it and it
    // must be propagated to the post-redirect channel.
    nsCOMPtr<nsIHttpChannelChild> channelChild = do_QueryInterface(newChannel);
    if (mSecurityInfo && channelChild) {
      HttpChannelChild* httpChannelChild =
          static_cast<HttpChannelChild*>(channelChild.get());
      httpChannelChild->OverrideSecurityInfoForNonIPCRedirect(mSecurityInfo);
    }

    // Normally we don't propagate the LoadInfo's service worker tainting
    // synthesis flag on redirect.  A real redirect normally will want to allow
    // normal tainting to proceed from its starting taint.  For this particular
    // redirect, though, we are performing a redirect to communicate the URL of
    // the service worker synthetic response itself.  This redirect still
    // represents the synthetic response, so we must preserve the flag.
    if (mLoadInfo->GetServiceWorkerTaintingSynthesized()) {
      nsCOMPtr<nsILoadInfo> newChannelLoadInfo = newChannel->LoadInfo();
      if (newChannelLoadInfo) {
        newChannelLoadInfo->SynthesizeServiceWorkerTainting(
            mLoadInfo->GetTainting());
      }
    }

    nsCOMPtr<nsIEventTarget> target = GetNeckoTarget();
    MOZ_ASSERT(target);

    rv = gHttpHandler->AsyncOnChannelRedirect(this, newChannel, redirectFlag,
                                              target);
  }

  if (NS_FAILED(rv)) OnRedirectVerifyCallback(rv);
}

void HttpChannelChild::OverrideSecurityInfoForNonIPCRedirect(
    nsISupports* securityInfo) {
  mResponseCouldBeSynthesized = true;
  DebugOnly<nsresult> rv = OverrideSecurityInfo(securityInfo);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

class Redirect3Event : public NeckoTargetChannelEvent<HttpChannelChild> {
 public:
  explicit Redirect3Event(HttpChannelChild* child)
      : NeckoTargetChannelEvent<HttpChannelChild>(child) {}
  void Run() override { mChild->Redirect3Complete(nullptr); }
};

mozilla::ipc::IPCResult HttpChannelChild::RecvRedirect3Complete() {
  LOG(("HttpChannelChild::RecvRedirect3Complete [this=%p]\n", this));
  mEventQ->RunOrEnqueue(new Redirect3Event(this));
  return IPC_OK();
}

class HttpFlushedForDiversionEvent
    : public NeckoTargetChannelEvent<HttpChannelChild> {
 public:
  explicit HttpFlushedForDiversionEvent(HttpChannelChild* aChild)
      : NeckoTargetChannelEvent<HttpChannelChild>(aChild) {
    MOZ_RELEASE_ASSERT(aChild);
  }

  void Run() override { mChild->FlushedForDiversion(); }
};

void HttpChannelChild::ProcessFlushedForDiversion() {
  LOG(("HttpChannelChild::ProcessFlushedForDiversion [this=%p]\n", this));
  MOZ_ASSERT(OnSocketThread());
  MOZ_RELEASE_ASSERT(mDivertingToParent);

  mEventQ->RunOrEnqueue(new HttpFlushedForDiversionEvent(this), true);
}

void HttpChannelChild::ProcessNotifyChannelClassifierProtectionDisabled(
    uint32_t aAcceptedReason) {
  LOG(
      ("HttpChannelChild::ProcessNotifyChannelClassifierProtectionDisabled "
       "[this=%p aAcceptedReason=%" PRIu32 "]\n",
       this, aAcceptedReason));
  MOZ_ASSERT(OnSocketThread());

  RefPtr<HttpChannelChild> self = this;
  nsCOMPtr<nsIEventTarget> neckoTarget = GetNeckoTarget();
  neckoTarget->Dispatch(
      NS_NewRunnableFunction(
          "AntiTrackingCommon::NotifyChannelClassifierProtectionDisabled",
          [self, aAcceptedReason]() {
            UrlClassifierCommon::NotifyChannelClassifierProtectionDisabled(
                self, aAcceptedReason);
          }),
      NS_DISPATCH_NORMAL);
}

void HttpChannelChild::ProcessNotifyCookieAllowed() {
  LOG(("HttpChannelChild::ProcessNotifyCookieAllowed [this=%p]\n", this));
  MOZ_ASSERT(OnSocketThread());

  RefPtr<HttpChannelChild> self = this;
  nsCOMPtr<nsIEventTarget> neckoTarget = GetNeckoTarget();
  neckoTarget->Dispatch(
      NS_NewRunnableFunction(
          "UrlClassifierCommon::NotifyBlockingDecision",
          [self]() {
            AntiTrackingCommon::NotifyBlockingDecision(
                self, AntiTrackingCommon::BlockingDecision::eAllow, 0);
          }),
      NS_DISPATCH_NORMAL);
}

void HttpChannelChild::ProcessNotifyCookieBlocked(uint32_t aRejectedReason) {
  LOG(("HttpChannelChild::ProcessNotifyCookieBlocked [this=%p]\n", this));
  MOZ_ASSERT(OnSocketThread());

  RefPtr<HttpChannelChild> self = this;
  nsCOMPtr<nsIEventTarget> neckoTarget = GetNeckoTarget();
  neckoTarget->Dispatch(
      NS_NewRunnableFunction("AntiTrackingCommon::NotifyBlockingDecision",
                             [self, aRejectedReason]() {
                               AntiTrackingCommon::NotifyBlockingDecision(
                                   self,
                                   AntiTrackingCommon::BlockingDecision::eBlock,
                                   aRejectedReason);
                             }),
      NS_DISPATCH_NORMAL);
}

void HttpChannelChild::ProcessNotifyClassificationFlags(
    uint32_t aClassificationFlags, bool aIsThirdParty) {
  LOG(
      ("HttpChannelChild::ProcessNotifyClassificationFlags thirdparty=%d "
       "flags=%" PRIu32 " [this=%p]\n",
       static_cast<int>(aIsThirdParty), aClassificationFlags, this));
  MOZ_ASSERT(OnSocketThread());

  AddClassificationFlags(aClassificationFlags, aIsThirdParty);
}

void HttpChannelChild::ProcessNotifyFlashPluginStateChanged(
    nsIHttpChannel::FlashPluginState aState) {
  LOG(("HttpChannelChild::ProcessNotifyFlashPluginStateChanged [this=%p]\n",
       this));
  MOZ_ASSERT(OnSocketThread());

  SetFlashPluginState(aState);
}

void HttpChannelChild::FlushedForDiversion() {
  LOG(("HttpChannelChild::FlushedForDiversion [this=%p]\n", this));
  MOZ_RELEASE_ASSERT(mDivertingToParent);

  // Once this is set, it should not be unset before HttpChannelChild is taken
  // down. After it is set, no OnStart/OnData/OnStop callbacks should be
  // received from the parent channel, nor dequeued from the ChannelEventQueue.
  mFlushedForDiversion = true;

  // If we're synthesized, it's up to the SyntheticDiversionListener to invoke
  // SendDivertComplete after it has sent the DivertOnStopRequestMessage.
  if (!mSynthesizedResponse) {
    SendDivertComplete();
  }
}

void HttpChannelChild::ProcessSetClassifierMatchedInfo(
    const nsCString& aList, const nsCString& aProvider,
    const nsCString& aFullHash) {
  LOG(("HttpChannelChild::ProcessSetClassifierMatchedInfo [this=%p]\n", this));
  MOZ_ASSERT(OnSocketThread());

  nsCOMPtr<nsIEventTarget> neckoTarget = GetNeckoTarget();
  neckoTarget->Dispatch(
      NewRunnableMethod<const nsCString, const nsCString, const nsCString>(
          "HttpChannelChild::SetMatchedInfo", this,
          &HttpChannelChild::SetMatchedInfo, aList, aProvider, aFullHash),
      NS_DISPATCH_NORMAL);
}

void HttpChannelChild::ProcessSetClassifierMatchedTrackingInfo(
    const nsCString& aLists, const nsCString& aFullHashes) {
  LOG(("HttpChannelChild::ProcessSetClassifierMatchedTrackingInfo [this=%p]\n",
       this));
  MOZ_ASSERT(OnSocketThread());

  nsTArray<nsCString> lists, fullhashes;
  for (const nsACString& token : aLists.Split(',')) {
    lists.AppendElement(token);
  }
  for (const nsACString& token : aFullHashes.Split(',')) {
    fullhashes.AppendElement(token);
  }

  nsCOMPtr<nsIEventTarget> neckoTarget = GetNeckoTarget();
  neckoTarget->Dispatch(
      NewRunnableMethod<const nsTArray<nsCString>, const nsTArray<nsCString>>(
          "HttpChannelChild::SetMatchedTrackingInfo", this,
          &HttpChannelChild::SetMatchedTrackingInfo, lists, fullhashes),
      NS_DISPATCH_NORMAL);
}

void HttpChannelChild::ProcessDivertMessages() {
  LOG(("HttpChannelChild::ProcessDivertMessages [this=%p]\n", this));
  MOZ_ASSERT(OnSocketThread());
  MOZ_RELEASE_ASSERT(mDivertingToParent);

  // DivertTo() has been called on parent, so we can now start sending queued
  // IPDL messages back to parent listener.
  nsCOMPtr<nsIEventTarget> neckoTarget = GetNeckoTarget();
  MOZ_ASSERT(neckoTarget);
  nsresult rv =
      neckoTarget->Dispatch(NewRunnableMethod("HttpChannelChild::Resume", this,
                                              &HttpChannelChild::Resume),
                            NS_DISPATCH_NORMAL);

  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
}

// Returns true if has actually completed the redirect and cleaned up the
// channel, or false the interception logic kicked in and we need to asyncly
// call FinishInterceptedRedirect and CleanupRedirectingChannel.
// The argument is an optional OverrideRunnable that we pass to the redirected
// channel.
bool HttpChannelChild::Redirect3Complete(OverrideRunnable* aRunnable) {
  LOG(("HttpChannelChild::Redirect3Complete [this=%p]\n", this));
  MOZ_ASSERT(NS_IsMainThread());
  nsresult rv = NS_OK;

  nsCOMPtr<nsIHttpChannelChild> chan = do_QueryInterface(mRedirectChannelChild);
  RefPtr<HttpChannelChild> httpChannelChild =
      static_cast<HttpChannelChild*>(chan.get());
  // Chrome channel has been AsyncOpen'd.  Reflect this in child.
  if (mRedirectChannelChild) {
    if (httpChannelChild) {
      httpChannelChild->mOverrideRunnable = aRunnable;
      httpChannelChild->mInterceptingChannel = this;
    }
    rv = mRedirectChannelChild->CompleteRedirectSetup(mListener, nullptr);
  }

  if (!httpChannelChild || !httpChannelChild->mShouldParentIntercept) {
    // The redirect channel either isn't a HttpChannelChild, or the interception
    // logic wasn't triggered, so we can clean it up right here.
    CleanupRedirectingChannel(rv);
    if (httpChannelChild) {
      httpChannelChild->mOverrideRunnable = nullptr;
      httpChannelChild->mInterceptingChannel = nullptr;
    }
    return true;
  }
  return false;
}

mozilla::ipc::IPCResult HttpChannelChild::RecvCancelRedirected() {
  CleanupRedirectingChannel(NS_BINDING_REDIRECTED);
  return IPC_OK();
}

void HttpChannelChild::CleanupRedirectingChannel(nsresult rv) {
  // Redirecting to new channel: shut this down and init new channel
  if (mLoadGroup) mLoadGroup->RemoveRequest(this, nullptr, NS_BINDING_ABORTED);

  if (NS_SUCCEEDED(rv)) {
    if (mLoadInfo) {
      nsCString remoteAddress;
      Unused << GetRemoteAddress(remoteAddress);
      nsCOMPtr<nsIURI> referrer;
      if (mReferrerInfo) {
        referrer = mReferrerInfo->GetComputedReferrer();
      }

      nsCOMPtr<nsIRedirectHistoryEntry> entry = new nsRedirectHistoryEntry(
          GetURIPrincipal(), referrer, remoteAddress);

      mLoadInfo->AppendRedirectHistoryEntry(entry, false);
    }
  } else {
    NS_WARNING("CompleteRedirectSetup failed, HttpChannelChild already open?");
  }

  // Release ref to new channel.
  mRedirectChannelChild = nullptr;

  if (mInterceptListener) {
    mInterceptListener->Cleanup();
    mInterceptListener = nullptr;
  }
  ReleaseListeners();
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIChildChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::ConnectParent(uint32_t registrarId) {
  LOG(("HttpChannelChild::ConnectParent [this=%p, id=%" PRIu32 "]\n", this,
       registrarId));
  MOZ_ASSERT(NS_IsMainThread());
  mozilla::dom::BrowserChild* browserChild = nullptr;
  nsCOMPtr<nsIBrowserChild> iBrowserChild;
  GetCallback(iBrowserChild);
  if (iBrowserChild) {
    browserChild =
        static_cast<mozilla::dom::BrowserChild*>(iBrowserChild.get());
  }
  if (MissingRequiredBrowserChild(browserChild, "http")) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  if (browserChild && !browserChild->IPCOpen()) {
    return NS_ERROR_FAILURE;
  }

  ContentChild* cc = static_cast<ContentChild*>(gNeckoChild->Manager());
  if (cc->IsShuttingDown()) {
    return NS_ERROR_FAILURE;
  }

  HttpBaseChannel::SetDocshellUserAgentOverride();

  // The socket transport in the chrome process now holds a logical ref to us
  // until OnStopRequest, or we do a redirect, or we hit an IPDL error.
  AddRef();

  // This must happen before the constructor message is sent. Otherwise messages
  // from the parent could arrive quickly and be delivered to the wrong event
  // target.
  SetEventTarget();

  HttpChannelConnectArgs connectArgs(registrarId, mShouldParentIntercept);
  PBrowserOrId browser = static_cast<ContentChild*>(gNeckoChild->Manager())
                             ->GetBrowserOrId(browserChild);
  if (!gNeckoChild->SendPHttpChannelConstructor(
          this, browser, IPC::SerializedLoadContext(this), connectArgs)) {
    return NS_ERROR_FAILURE;
  }

  {
    MutexAutoLock lock(mBgChildMutex);

    MOZ_ASSERT(!mBgChild);
    MOZ_ASSERT(!mBgInitFailCallback);

    mBgInitFailCallback = NewRunnableMethod<nsresult>(
        "HttpChannelChild::OnRedirectVerifyCallback", this,
        &HttpChannelChild::OnRedirectVerifyCallback, NS_ERROR_FAILURE);

    RefPtr<HttpBackgroundChannelChild> bgChild =
        new HttpBackgroundChannelChild();

    MOZ_RELEASE_ASSERT(gSocketTransportService);

    RefPtr<HttpChannelChild> self = this;
    nsresult rv = gSocketTransportService->Dispatch(
        NewRunnableMethod<RefPtr<HttpChannelChild>>(
            "HttpBackgroundChannelChild::Init", bgChild,
            &HttpBackgroundChannelChild::Init, std::move(self)),
        NS_DISPATCH_NORMAL);

    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    mBgChild = bgChild.forget();
  }

  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::CompleteRedirectSetup(nsIStreamListener* listener,
                                        nsISupports* aContext) {
  LOG(("HttpChannelChild::FinishRedirectSetup [this=%p]\n", this));
  MOZ_ASSERT(NS_IsMainThread());

  NS_ENSURE_TRUE(!mIsPending, NS_ERROR_IN_PROGRESS);
  NS_ENSURE_TRUE(!mWasOpened, NS_ERROR_ALREADY_OPENED);

  if (mShouldParentIntercept) {
    // This is a redirected channel, and the corresponding parent channel has
    // started AsyncOpen but was intercepted and suspended. We must tear it down
    // and start fresh - we will intercept the child channel this time, before
    // creating a new parent channel unnecessarily.

    // Since this method is called from RecvRedirect3Complete which itself is
    // called from either OnRedirectVerifyCallback via OverrideRunnable, or from
    // RecvRedirect3Complete. The order of events must always be:
    //  1. Teardown the IPDL connection
    //  2. AsyncOpen the connection again
    //  3. Cleanup the redirecting channel (the one calling Redirect3Complete)
    //  4. [optional] Call OverrideWithSynthesizedResponse on the redirected
    //  channel if the call came from OverrideRunnable.
    mInterceptedRedirectListener = listener;
    mInterceptedRedirectContext = aContext;

    // This will send a message to the parent notifying it that we are closing
    // down. After closing the IPC channel, we will proceed to execute
    // FinishInterceptedRedirect() which AsyncOpen's the channel again.
    SendFinishInterceptedRedirect();

    // XXX valentin: The interception logic should be rewritten to avoid
    // calling AsyncOpen on the channel _after_ we call Send__delete__()
    return NS_OK;
  }

  /*
   * No need to check for cancel: we don't get here if nsHttpChannel canceled
   * before AsyncOpen(); if it's canceled after that, OnStart/Stop will just
   * get called with error code as usual.  So just setup mListener and make the
   * channel reflect AsyncOpen'ed state.
   */

  mLastStatusReported = TimeStamp::Now();
  PROFILER_ADD_NETWORK_MARKER(mURI, mPriority, mChannelId,
                              NetworkLoadType::LOAD_START,
                              mChannelCreationTimestamp, mLastStatusReported, 0,
                              kCacheUnknown, nullptr, nullptr);
  mIsPending = true;
  mWasOpened = true;
  mListener = listener;

  // add ourselves to the load group.
  if (mLoadGroup) mLoadGroup->AddRequest(this, nullptr);

  // We already have an open IPDL connection to the parent. If on-modify-request
  // listeners or load group observers canceled us, let the parent handle it
  // and send it back to us naturally.
  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIAsyncVerifyRedirectCallback
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::OnRedirectVerifyCallback(nsresult result) {
  LOG(("HttpChannelChild::OnRedirectVerifyCallback [this=%p]\n", this));
  MOZ_ASSERT(NS_IsMainThread());
  Maybe<URIParams> redirectURI;
  nsresult rv;

  nsCOMPtr<nsIHttpChannel> newHttpChannel =
      do_QueryInterface(mRedirectChannelChild);

  if (NS_SUCCEEDED(result) && !mRedirectChannelChild) {
    // mRedirectChannelChild doesn't exist means we're redirecting to a protocol
    // that doesn't implement nsIChildChannel. The redirect result should be set
    // as failed by veto listeners and shouldn't enter this condition. As the
    // last resort, we synthesize the error result as NS_ERROR_DOM_BAD_URI here
    // to let nsHttpChannel::ContinueProcessResponse2 know it's redirecting to
    // another protocol and throw an error.
    LOG(("  redirecting to a protocol that doesn't implement nsIChildChannel"));
    result = NS_ERROR_DOM_BAD_URI;
  }

  nsCOMPtr<nsIReferrerInfo> referrerInfo;
  if (newHttpChannel) {
    // Must not be called until after redirect observers called.
    newHttpChannel->SetOriginalURI(mOriginalURI);
    referrerInfo = newHttpChannel->GetReferrerInfo();
  }

  if (mRedirectingForSubsequentSynthesizedResponse) {
    nsCOMPtr<nsIHttpChannelChild> httpChannelChild =
        do_QueryInterface(mRedirectChannelChild);
    RefPtr<HttpChannelChild> redirectedChannel =
        static_cast<HttpChannelChild*>(httpChannelChild.get());
    // redirectChannel will be NULL if mRedirectChannelChild isn't a
    // nsIHttpChannelChild (it could be a DataChannelChild).

    RefPtr<InterceptStreamListener> streamListener =
        new InterceptStreamListener(redirectedChannel, nullptr);

    nsCOMPtr<nsIEventTarget> neckoTarget = GetNeckoTarget();
    MOZ_ASSERT(neckoTarget);

    nsCOMPtr<nsIInterceptedBodyCallback> callback =
        mSynthesizedCallback.forget();

    Unused << neckoTarget->Dispatch(
        new OverrideRunnable(this, redirectedChannel, streamListener,
                             mSynthesizedInput, callback, mResponseHead,
                             mSynthesizedCacheInfo),
        NS_DISPATCH_NORMAL);

    return NS_OK;
  }

  RequestHeaderTuples emptyHeaders;
  RequestHeaderTuples* headerTuples = &emptyHeaders;
  nsLoadFlags loadFlags = 0;
  Maybe<CorsPreflightArgs> corsPreflightArgs;

  nsCOMPtr<nsIHttpChannelChild> newHttpChannelChild =
      do_QueryInterface(mRedirectChannelChild);
  if (newHttpChannelChild && NS_SUCCEEDED(result)) {
    rv = newHttpChannelChild->AddCookiesToRequest();
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    rv = newHttpChannelChild->GetClientSetRequestHeaders(&headerTuples);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    newHttpChannelChild->GetClientSetCorsPreflightParameters(corsPreflightArgs);
  }

  /* If the redirect was canceled, bypass OMR and send an empty API
   * redirect URI */
  SerializeURI(nullptr, redirectURI);

  if (NS_SUCCEEDED(result)) {
    // Note: this is where we would notify "http-on-modify-response" observers.
    // We have deliberately disabled this for child processes (see bug 806753)
    //
    // After we verify redirect, nsHttpChannel may hit the network: must give
    // "http-on-modify-request" observers the chance to cancel before that.
    // base->CallOnModifyRequestObservers();

    nsCOMPtr<nsIHttpChannelInternal> newHttpChannelInternal =
        do_QueryInterface(mRedirectChannelChild);
    if (newHttpChannelInternal) {
      nsCOMPtr<nsIURI> apiRedirectURI;
      rv = newHttpChannelInternal->GetApiRedirectToURI(
          getter_AddRefs(apiRedirectURI));
      if (NS_SUCCEEDED(rv) && apiRedirectURI) {
        /* If there was an API redirect of this channel, we need to send it
         * up here, since it can't be sent via SendAsyncOpen. */
        SerializeURI(apiRedirectURI, redirectURI);
      }
    }

    nsCOMPtr<nsIRequest> request = do_QueryInterface(mRedirectChannelChild);
    if (request) {
      request->GetLoadFlags(&loadFlags);
    }
  }

  MaybeCallSynthesizedCallback();

  bool chooseAppcache = false;
  nsCOMPtr<nsIApplicationCacheChannel> appCacheChannel =
      do_QueryInterface(newHttpChannel);
  if (appCacheChannel) {
    appCacheChannel->GetChooseApplicationCache(&chooseAppcache);
  }

  nsCOMPtr<nsILoadInfo> newChannelLoadInfo = nullptr;
  nsCOMPtr<nsIChannel> newChannel = do_QueryInterface(mRedirectChannelChild);
  if (newChannel) {
    newChannelLoadInfo = newChannel->LoadInfo();
  }

  ChildLoadInfoForwarderArgs loadInfoForwarder;
  LoadInfoToChildLoadInfoForwarder(newChannelLoadInfo, &loadInfoForwarder);

  if (CanSend())
    SendRedirect2Verify(result, *headerTuples, loadInfoForwarder, loadFlags,
                        referrerInfo, redirectURI, corsPreflightArgs,
                        chooseAppcache);

  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIRequest
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::Cancel(nsresult status) {
  LOG(("HttpChannelChild::Cancel [this=%p, status=%" PRIx32 "]\n", this,
       static_cast<uint32_t>(status)));
  LogCallingScriptLocation(this);

  MOZ_ASSERT(NS_IsMainThread());

  if (!mCanceled) {
    // If this cancel occurs before nsHttpChannel has been set up, AsyncOpen
    // is responsible for cleaning up.
    mCanceled = true;
    mStatus = status;
    if (RemoteChannelExists()) {
      SendCancel(status);
    }

    // If the channel is intercepted and already pumping, then just
    // cancel the pump.  This will call OnStopRequest().
    if (mSynthesizedResponsePump) {
      mSynthesizedResponsePump->Cancel(status);
    }

    // If we are canceled while intercepting, but not yet pumping, then
    // we must call AsyncAbort() to trigger OnStopRequest().
    else if (mInterceptListener) {
      mInterceptListener->Cleanup();
      mInterceptListener = nullptr;
      Unused << AsyncAbort(status);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::Suspend() {
  LOG(("HttpChannelChild::Suspend [this=%p, mSuspendCount=%" PRIu32 ", "
       "mDivertingToParent=%d]\n",
       this, mSuspendCount + 1, static_cast<bool>(mDivertingToParent)));
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_TRUE(RemoteChannelExists() || mInterceptListener,
                 NS_ERROR_NOT_AVAILABLE);

  // SendSuspend only once, when suspend goes from 0 to 1.
  // Don't SendSuspend at all if we're diverting callbacks to the parent;
  // suspend will be called at the correct time in the parent itself.
  if (!mSuspendCount++ && !mDivertingToParent) {
    if (RemoteChannelExists()) {
      SendSuspend();
      mSuspendSent = true;
    }
  }
  if (mSynthesizedResponsePump) {
    mSynthesizedResponsePump->Suspend();
  }
  mEventQ->Suspend();

  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::Resume() {
  LOG(("HttpChannelChild::Resume [this=%p, mSuspendCount=%" PRIu32 ", "
       "mDivertingToParent=%d]\n",
       this, mSuspendCount - 1, static_cast<bool>(mDivertingToParent)));
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_TRUE(RemoteChannelExists() || mInterceptListener,
                 NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(mSuspendCount > 0, NS_ERROR_UNEXPECTED);

  nsresult rv = NS_OK;

  // SendResume only once, when suspend count drops to 0.
  // Don't SendResume at all if we're diverting callbacks to the parent (unless
  // suspend was sent earlier); otherwise, resume will be called at the correct
  // time in the parent itself.
  if (!--mSuspendCount && (!mDivertingToParent || mSuspendSent)) {
    if (RemoteChannelExists()) {
      SendResume();
    }
    if (mCallOnResume) {
      nsCOMPtr<nsIEventTarget> neckoTarget = GetNeckoTarget();
      MOZ_ASSERT(neckoTarget);

      RefPtr<HttpChannelChild> self = this;
      std::function<nsresult(HttpChannelChild*)> callOnResume = nullptr;
      std::swap(callOnResume, mCallOnResume);
      rv = neckoTarget->Dispatch(
          NS_NewRunnableFunction(
              "net::HttpChannelChild::mCallOnResume",
              [callOnResume, self{std::move(self)}]() { callOnResume(self); }),
          NS_DISPATCH_NORMAL);
    }
  }
  if (mSynthesizedResponsePump) {
    mSynthesizedResponsePump->Resume();
  }
  mEventQ->Resume();

  return rv;
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::GetSecurityInfo(nsISupports** aSecurityInfo) {
  NS_ENSURE_ARG_POINTER(aSecurityInfo);
  NS_IF_ADDREF(*aSecurityInfo = mSecurityInfo);
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::AsyncOpen(nsIStreamListener* aListener) {
  LOG(("HttpChannelChild::AsyncOpen [this=%p uri=%s]\n", this, mSpec.get()));

  nsresult rv = AsyncOpenInternal(aListener);
  if (NS_FAILED(rv)) {
    uint32_t blockingReason = 0;
    if (mLoadInfo) {
      mLoadInfo->GetRequestBlockingReason(&blockingReason);
    }
    LOG(
        ("HttpChannelChild::AsyncOpen failed [this=%p rv=0x%08x "
         "blocking-reason=%u]\n",
         this, static_cast<uint32_t>(rv), blockingReason));

    gHttpHandler->OnFailedOpeningRequest(this);
  }
  return rv;
}

nsresult HttpChannelChild::AsyncOpenInternal(nsIStreamListener* aListener) {
  nsresult rv;

  nsCOMPtr<nsIStreamListener> listener = aListener;
  rv = nsContentSecurityManager::doContentSecurityCheck(this, listener);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    ReleaseListeners();
    return rv;
  }

  MOZ_ASSERT(
      !mLoadInfo || mLoadInfo->GetSecurityMode() == 0 ||
          mLoadInfo->GetInitialSecurityCheckDone() ||
          (mLoadInfo->GetSecurityMode() ==
               nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL &&
           nsContentUtils::IsSystemPrincipal(mLoadInfo->LoadingPrincipal())),
      "security flags in loadInfo but doContentSecurityCheck() not called");

  LogCallingScriptLocation(this);

  if (!mLoadGroup && !mCallbacks) {
    // If no one called SetLoadGroup or SetNotificationCallbacks, the private
    // state has not been updated on PrivateBrowsingChannel (which we derive
    // from) Hence, we have to call UpdatePrivateBrowsing() here
    UpdatePrivateBrowsing();
  }

#ifdef DEBUG
  AssertPrivateBrowsingId();
#endif

  if (mCanceled) {
    ReleaseListeners();
    return mStatus;
  }

  NS_ENSURE_TRUE(gNeckoChild != nullptr, NS_ERROR_FAILURE);
  NS_ENSURE_ARG_POINTER(listener);
  NS_ENSURE_TRUE(!mIsPending, NS_ERROR_IN_PROGRESS);
  NS_ENSURE_TRUE(!mWasOpened, NS_ERROR_ALREADY_OPENED);

  if (MaybeWaitForUploadStreamLength(listener, nullptr)) {
    return NS_OK;
  }

  if (!mAsyncOpenTimeOverriden) {
    mAsyncOpenTime = TimeStamp::Now();
  }

#ifdef MOZ_TASK_TRACER
  if (tasktracer::IsStartLogging()) {
    nsCOMPtr<nsIURI> uri;
    GetURI(getter_AddRefs(uri));
    nsAutoCString urispec;
    uri->GetSpec(urispec);
    tasktracer::AddLabel("HttpChannelChild::AsyncOpen %s", urispec.get());
  }
#endif

  // Port checked in parent, but duplicate here so we can return with error
  // immediately
  rv = NS_CheckPortSafety(mURI);
  if (NS_FAILED(rv)) {
    ReleaseListeners();
    return rv;
  }

  nsAutoCString cookie;
  if (NS_SUCCEEDED(mRequestHead.GetHeader(nsHttp::Cookie, cookie))) {
    mUserSetCookieHeader = cookie;
  }

  DebugOnly<nsresult> check = AddCookiesToRequest();
  MOZ_ASSERT(NS_SUCCEEDED(check));

  //
  // NOTE: From now on we must return NS_OK; all errors must be handled via
  // OnStart/OnStopRequest
  //

  // We notify "http-on-opening-request" observers in the child
  // process so that devtools can capture a stack trace at the
  // appropriate spot.  See bug 806753 for some information about why
  // other http-* notifications are disabled in child processes.
  gHttpHandler->OnOpeningRequest(this);

  mLastStatusReported = TimeStamp::Now();
  PROFILER_ADD_NETWORK_MARKER(mURI, mPriority, mChannelId,
                              NetworkLoadType::LOAD_START,
                              mChannelCreationTimestamp, mLastStatusReported, 0,
                              kCacheUnknown, nullptr, nullptr);

  mIsPending = true;
  mWasOpened = true;
  mListener = listener;

  // add ourselves to the load group.
  if (mLoadGroup) mLoadGroup->AddRequest(this, nullptr);

  if (mCanceled) {
    // We may have been canceled already, either by on-modify-request
    // listeners or by load group observers; in that case, don't create IPDL
    // connection. See nsHttpChannel::AsyncOpen().
    ReleaseListeners();
    return mStatus;
  }

  // Set user agent override from docshell
  HttpBaseChannel::SetDocshellUserAgentOverride();

  MOZ_ASSERT_IF(mPostRedirectChannelShouldUpgrade,
                mPostRedirectChannelShouldIntercept);
  bool shouldUpgrade = mPostRedirectChannelShouldUpgrade;
  if (mPostRedirectChannelShouldIntercept ||
      ShouldInterceptURI(mURI, shouldUpgrade)) {
    RefPtr<HttpChannelChild> self = this;

    std::function<void(bool)> callback = [self,
                                          shouldUpgrade](bool aStorageAllowed) {
      if (!aStorageAllowed) {
        nsresult rv = self->ContinueAsyncOpen();
        if (NS_WARN_IF(NS_FAILED(rv))) {
          Unused << self->AsyncAbort(rv);
        }
        return;
      }

      self->mResponseCouldBeSynthesized = true;

      nsCOMPtr<nsINetworkInterceptController> controller;
      self->GetCallback(controller);

      self->mInterceptListener = new InterceptStreamListener(self, nullptr);

      RefPtr<InterceptedChannelContent> intercepted =
          new InterceptedChannelContent(
              self, controller, self->mInterceptListener, shouldUpgrade);
      intercepted->NotifyController();
    };

    ClassifierDummyChannel::StorageAllowedState state =
        ClassifierDummyChannel::StorageAllowed(this, callback);
    if (state == ClassifierDummyChannel::eStorageGranted) {
      callback(true);
      return NS_OK;
    }

    if (state == ClassifierDummyChannel::eAsyncNeeded) {
      // The async callback will be executed eventually.
      return NS_OK;
    }

    MOZ_ASSERT(state == ClassifierDummyChannel::eStorageDenied);
    // Fall through
  }

  rv = ContinueAsyncOpen();
  if (NS_FAILED(rv)) {
    ReleaseListeners();
  }
  return rv;
}

// Assigns an nsIEventTarget to our IPDL actor so that IPC messages are sent to
// the correct DocGroup/TabGroup.
void HttpChannelChild::SetEventTarget() {
  nsCOMPtr<nsILoadInfo> loadInfo = LoadInfo();

  nsCOMPtr<nsIEventTarget> target =
      nsContentUtils::GetEventTargetByLoadInfo(loadInfo, TaskCategory::Network);

  if (!target) {
    return;
  }

  gNeckoChild->SetEventTargetForActor(this, target);

  {
    MutexAutoLock lock(mEventTargetMutex);
    mNeckoTarget = target;
  }
}

already_AddRefed<nsIEventTarget> HttpChannelChild::GetNeckoTarget() {
  nsCOMPtr<nsIEventTarget> target;
  {
    MutexAutoLock lock(mEventTargetMutex);
    target = mNeckoTarget;
  }

  if (!target) {
    target = GetMainThreadEventTarget();
  }
  return target.forget();
}

already_AddRefed<nsIEventTarget> HttpChannelChild::GetODATarget() {
  nsCOMPtr<nsIEventTarget> target;
  {
    MutexAutoLock lock(mEventTargetMutex);
    target = mODATarget ? mODATarget : mNeckoTarget;
  }

  if (!target) {
    target = GetMainThreadEventTarget();
  }
  return target.forget();
}

nsresult HttpChannelChild::ContinueAsyncOpen() {
  nsresult rv;
  nsCString appCacheClientId;
  if (mInheritApplicationCache) {
    // Pick up an application cache from the notification
    // callbacks if available
    nsCOMPtr<nsIApplicationCacheContainer> appCacheContainer;
    GetCallback(appCacheContainer);

    if (appCacheContainer) {
      nsCOMPtr<nsIApplicationCache> appCache;
      nsresult rv =
          appCacheContainer->GetApplicationCache(getter_AddRefs(appCache));
      if (NS_SUCCEEDED(rv) && appCache) {
        appCache->GetClientID(appCacheClientId);
      }
    }
  }

  //
  // Send request to the chrome process...
  //

  mozilla::dom::BrowserChild* browserChild = nullptr;
  nsCOMPtr<nsIBrowserChild> iBrowserChild;
  GetCallback(iBrowserChild);
  if (iBrowserChild) {
    browserChild =
        static_cast<mozilla::dom::BrowserChild*>(iBrowserChild.get());
  }
  if (MissingRequiredBrowserChild(browserChild, "http")) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  // This id identifies the inner window's top-level document,
  // which changes on every new load or navigation.
  uint64_t contentWindowId = 0;
  TimeStamp navigationStartTimeStamp;
  if (browserChild) {
    MOZ_ASSERT(browserChild->WebNavigation());
    if (RefPtr<Document> document = browserChild->GetTopLevelDocument()) {
      contentWindowId = document->InnerWindowID();
      nsDOMNavigationTiming* navigationTiming = document->GetNavigationTiming();
      if (navigationTiming) {
        navigationStartTimeStamp =
            navigationTiming->GetNavigationStartTimeStamp();
      }
      mTopLevelOuterContentWindowId = document->OuterWindowID();
    }
  }
  SetTopLevelContentWindowId(contentWindowId);

  HttpChannelOpenArgs openArgs;
  // No access to HttpChannelOpenArgs members, but they each have a
  // function with the struct name that returns a ref.
  SerializeURI(mURI, openArgs.uri());
  SerializeURI(mOriginalURI, openArgs.original());
  SerializeURI(mDocumentURI, openArgs.doc());
  SerializeURI(mAPIRedirectToURI, openArgs.apiRedirectTo());
  openArgs.loadFlags() = mLoadFlags;
  openArgs.requestHeaders() = mClientSetRequestHeaders;
  mRequestHead.Method(openArgs.requestMethod());
  openArgs.preferredAlternativeTypes() = mPreferredCachedAltDataTypes;
  openArgs.referrerInfo() = mReferrerInfo;

  AutoIPCStream autoStream(openArgs.uploadStream());
  if (mUploadStream) {
    autoStream.Serialize(mUploadStream, ContentChild::GetSingleton());
    autoStream.TakeOptionalValue();
  }

  if (mResponseHead) {
    openArgs.synthesizedResponseHead() = Some(*mResponseHead);
    openArgs.suspendAfterSynthesizeResponse() =
        mSuspendParentAfterSynthesizeResponse;
  } else {
    openArgs.suspendAfterSynthesizeResponse() = false;
  }

  nsCOMPtr<nsISerializable> secInfoSer = do_QueryInterface(mSecurityInfo);
  if (secInfoSer) {
    NS_SerializeToString(secInfoSer,
                         openArgs.synthesizedSecurityInfoSerialization());
  }

  Maybe<CorsPreflightArgs> optionalCorsPreflightArgs;
  GetClientSetCorsPreflightParameters(optionalCorsPreflightArgs);

  // NB: This call forces us to cache mTopWindowURI and
  // mContentBlockingAllowListPrincipal if we haven't already.
  nsCOMPtr<nsIURI> uri;
  GetTopWindowURI(mURI, getter_AddRefs(uri));

  SerializeURI(mTopWindowURI, openArgs.topWindowURI());

  if (mContentBlockingAllowListPrincipal) {
    PrincipalInfo principalInfo;
    rv = PrincipalToPrincipalInfo(mContentBlockingAllowListPrincipal,
                                  &principalInfo);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    openArgs.contentBlockingAllowListPrincipal() = principalInfo;
  } else {
    openArgs.contentBlockingAllowListPrincipal() = void_t();
  }

  openArgs.preflightArgs() = optionalCorsPreflightArgs;

  openArgs.uploadStreamHasHeaders() = mUploadStreamHasHeaders;
  openArgs.priority() = mPriority;
  openArgs.classOfService() = mClassOfService;
  openArgs.redirectionLimit() = mRedirectionLimit;
  openArgs.allowSTS() = mAllowSTS;
  openArgs.thirdPartyFlags() = mThirdPartyFlags;
  openArgs.resumeAt() = mSendResumeAt;
  openArgs.startPos() = mStartPos;
  openArgs.entityID() = mEntityID;
  openArgs.chooseApplicationCache() = mChooseApplicationCache;
  openArgs.appCacheClientID() = appCacheClientId;
  openArgs.allowSpdy() = mAllowSpdy;
  openArgs.allowAltSvc() = mAllowAltSvc;
  openArgs.beConservative() = mBeConservative;
  openArgs.tlsFlags() = mTlsFlags;
  openArgs.initialRwin() = mInitialRwin;

  openArgs.cacheKey() = mCacheKey;

  openArgs.blockAuthPrompt() = mBlockAuthPrompt;

  openArgs.allowStaleCacheContent() = mAllowStaleCacheContent;

  openArgs.contentTypeHint() = mContentTypeHint;

  rv = mozilla::ipc::LoadInfoToLoadInfoArgs(mLoadInfo, &openArgs.loadInfo());
  NS_ENSURE_SUCCESS(rv, rv);

  EnsureRequestContextID();
  openArgs.requestContextID() = mRequestContextID;

  openArgs.corsMode() = mCorsMode;
  openArgs.redirectMode() = mRedirectMode;

  openArgs.channelId() = mChannelId;

  openArgs.integrityMetadata() = mIntegrityMetadata;

  openArgs.contentWindowId() = contentWindowId;
  openArgs.topLevelOuterContentWindowId() = mTopLevelOuterContentWindowId;

  LOG(("HttpChannelChild::ContinueAsyncOpen this=%p gid=%" PRIu64
       " topwinid=%" PRIx64,
       this, mChannelId, mTopLevelOuterContentWindowId));

  if (browserChild && !browserChild->IPCOpen()) {
    return NS_ERROR_FAILURE;
  }

  ContentChild* cc = static_cast<ContentChild*>(gNeckoChild->Manager());
  if (cc->IsShuttingDown()) {
    return NS_ERROR_FAILURE;
  }

  openArgs.launchServiceWorkerStart() = mLaunchServiceWorkerStart;
  openArgs.launchServiceWorkerEnd() = mLaunchServiceWorkerEnd;
  openArgs.dispatchFetchEventStart() = mDispatchFetchEventStart;
  openArgs.dispatchFetchEventEnd() = mDispatchFetchEventEnd;
  openArgs.handleFetchEventStart() = mHandleFetchEventStart;
  openArgs.handleFetchEventEnd() = mHandleFetchEventEnd;

  openArgs.forceMainDocumentChannel() = mForceMainDocumentChannel;

  openArgs.navigationStartTimeStamp() = navigationStartTimeStamp;
  openArgs.hasSandboxedAuxiliaryNavigations() =
      GetHasSandboxedAuxiliaryNavigations();

  // This must happen before the constructor message is sent. Otherwise messages
  // from the parent could arrive quickly and be delivered to the wrong event
  // target.
  SetEventTarget();

  // The socket transport in the chrome process now holds a logical ref to us
  // until OnStopRequest, or we do a redirect, or we hit an IPDL error.
  AddRef();

  PBrowserOrId browser = cc->GetBrowserOrId(browserChild);
  if (!gNeckoChild->SendPHttpChannelConstructor(
          this, browser, IPC::SerializedLoadContext(this), openArgs)) {
    return NS_ERROR_FAILURE;
  }

  {
    MutexAutoLock lock(mBgChildMutex);

    MOZ_RELEASE_ASSERT(gSocketTransportService);

    // Service worker might use the same HttpChannelChild to do async open
    // twice. Need to disconnect with previous background channel before
    // creating the new one, to prevent receiving further notification
    // from it.
    if (mBgChild) {
      RefPtr<HttpBackgroundChannelChild> prevBgChild = mBgChild.forget();
      gSocketTransportService->Dispatch(
          NewRunnableMethod("HttpBackgroundChannelChild::OnChannelClosed",
                            prevBgChild,
                            &HttpBackgroundChannelChild::OnChannelClosed),
          NS_DISPATCH_NORMAL);
    }

    MOZ_ASSERT(!mBgInitFailCallback);

    mBgInitFailCallback = NewRunnableMethod<nsresult>(
        "HttpChannelChild::FailedAsyncOpen", this,
        &HttpChannelChild::FailedAsyncOpen, NS_ERROR_FAILURE);

    RefPtr<HttpBackgroundChannelChild> bgChild =
        new HttpBackgroundChannelChild();

    RefPtr<HttpChannelChild> self = this;
    nsresult rv = gSocketTransportService->Dispatch(
        NewRunnableMethod<RefPtr<HttpChannelChild>>(
            "HttpBackgroundChannelChild::Init", bgChild,
            &HttpBackgroundChannelChild::Init, self),
        NS_DISPATCH_NORMAL);

    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    mBgChild = bgChild.forget();
  }

  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIHttpChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::SetRequestHeader(const nsACString& aHeader,
                                   const nsACString& aValue, bool aMerge) {
  LOG(("HttpChannelChild::SetRequestHeader [this=%p]\n", this));
  nsresult rv = HttpBaseChannel::SetRequestHeader(aHeader, aValue, aMerge);
  if (NS_FAILED(rv)) return rv;

  RequestHeaderTuple* tuple = mClientSetRequestHeaders.AppendElement();
  if (!tuple) return NS_ERROR_OUT_OF_MEMORY;

  tuple->mHeader = aHeader;
  tuple->mValue = aValue;
  tuple->mMerge = aMerge;
  tuple->mEmpty = false;
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::SetEmptyRequestHeader(const nsACString& aHeader) {
  LOG(("HttpChannelChild::SetEmptyRequestHeader [this=%p]\n", this));
  nsresult rv = HttpBaseChannel::SetEmptyRequestHeader(aHeader);
  if (NS_FAILED(rv)) return rv;

  RequestHeaderTuple* tuple = mClientSetRequestHeaders.AppendElement();
  if (!tuple) return NS_ERROR_OUT_OF_MEMORY;

  tuple->mHeader = aHeader;
  tuple->mMerge = false;
  tuple->mEmpty = true;
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::RedirectTo(nsIURI* newURI) {
  // disabled until/unless addons run in child or something else needs this
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
HttpChannelChild::UpgradeToSecure() {
  // disabled until/unless addons run in child or something else needs this
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
HttpChannelChild::GetProtocolVersion(nsACString& aProtocolVersion) {
  aProtocolVersion = mProtocolVersion;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIHttpChannelInternal
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::SetupFallbackChannel(const char* aFallbackKey) {
  DROP_DEAD();
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsICacheInfoChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::GetCacheTokenFetchCount(int32_t* _retval) {
  NS_ENSURE_ARG_POINTER(_retval);
  MOZ_ASSERT(NS_IsMainThread());

  if (mSynthesizedCacheInfo) {
    return mSynthesizedCacheInfo->GetCacheTokenFetchCount(_retval);
  }

  if (!mCacheEntryAvailable && !mAltDataCacheEntryAvailable) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  *_retval = mCacheFetchCount;
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::GetCacheTokenExpirationTime(uint32_t* _retval) {
  NS_ENSURE_ARG_POINTER(_retval);
  MOZ_ASSERT(NS_IsMainThread());

  if (mSynthesizedCacheInfo) {
    return mSynthesizedCacheInfo->GetCacheTokenExpirationTime(_retval);
  }

  if (!mCacheEntryAvailable) return NS_ERROR_NOT_AVAILABLE;

  *_retval = mCacheExpirationTime;
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::GetCacheTokenCachedCharset(nsACString& _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  if (mSynthesizedCacheInfo) {
    return mSynthesizedCacheInfo->GetCacheTokenCachedCharset(_retval);
  }

  if (!mCacheEntryAvailable) return NS_ERROR_NOT_AVAILABLE;

  _retval = mCachedCharset;
  return NS_OK;
}
NS_IMETHODIMP
HttpChannelChild::SetCacheTokenCachedCharset(const nsACString& aCharset) {
  if (mSynthesizedCacheInfo) {
    return mSynthesizedCacheInfo->SetCacheTokenCachedCharset(aCharset);
  }

  if (!mCacheEntryAvailable || !RemoteChannelExists())
    return NS_ERROR_NOT_AVAILABLE;

  mCachedCharset = aCharset;
  if (!SendSetCacheTokenCachedCharset(PromiseFlatCString(aCharset))) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::IsFromCache(bool* value) {
  if (mSynthesizedCacheInfo) {
    return mSynthesizedCacheInfo->IsFromCache(value);
  }

  if (!mIsPending) return NS_ERROR_NOT_AVAILABLE;

  *value = mIsFromCache;
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::GetCacheEntryId(uint64_t* aCacheEntryId) {
  if (mSynthesizedCacheInfo) {
    return mSynthesizedCacheInfo->GetCacheEntryId(aCacheEntryId);
  }

  bool fromCache = false;
  if (NS_FAILED(IsFromCache(&fromCache)) || !fromCache ||
      !mCacheEntryAvailable) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  *aCacheEntryId = mCacheEntryId;
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::IsRacing(bool* aIsRacing) {
  if (!mAfterOnStartRequestBegun) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  *aIsRacing = mIsRacing;
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::GetCacheKey(uint32_t* cacheKey) {
  MOZ_ASSERT(NS_IsMainThread());
  if (mSynthesizedCacheInfo) {
    return mSynthesizedCacheInfo->GetCacheKey(cacheKey);
  }

  *cacheKey = mCacheKey;
  return NS_OK;
}
NS_IMETHODIMP
HttpChannelChild::SetCacheKey(uint32_t cacheKey) {
  if (mSynthesizedCacheInfo) {
    return mSynthesizedCacheInfo->SetCacheKey(cacheKey);
  }

  ENSURE_CALLED_BEFORE_ASYNC_OPEN();

  mCacheKey = cacheKey;
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::SetAllowStaleCacheContent(bool aAllowStaleCacheContent) {
  if (mSynthesizedCacheInfo) {
    return mSynthesizedCacheInfo->SetAllowStaleCacheContent(
        aAllowStaleCacheContent);
  }

  mAllowStaleCacheContent = aAllowStaleCacheContent;
  return NS_OK;
}
NS_IMETHODIMP
HttpChannelChild::GetAllowStaleCacheContent(bool* aAllowStaleCacheContent) {
  if (mSynthesizedCacheInfo) {
    return mSynthesizedCacheInfo->GetAllowStaleCacheContent(
        aAllowStaleCacheContent);
  }

  NS_ENSURE_ARG(aAllowStaleCacheContent);
  *aAllowStaleCacheContent = mAllowStaleCacheContent;
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::PreferAlternativeDataType(const nsACString& aType,
                                            const nsACString& aContentType,
                                            bool aDeliverAltData) {
  ENSURE_CALLED_BEFORE_ASYNC_OPEN();

  if (mSynthesizedCacheInfo) {
    return mSynthesizedCacheInfo->PreferAlternativeDataType(aType, aContentType,
                                                            aDeliverAltData);
  }

  mPreferredCachedAltDataTypes.AppendElement(PreferredAlternativeDataTypeParams(
      nsCString(aType), nsCString(aContentType), aDeliverAltData));
  return NS_OK;
}

const nsTArray<PreferredAlternativeDataTypeParams>&
HttpChannelChild::PreferredAlternativeDataTypes() {
  return mPreferredCachedAltDataTypes;
}

NS_IMETHODIMP
HttpChannelChild::GetAlternativeDataType(nsACString& aType) {
  if (mSynthesizedCacheInfo) {
    return mSynthesizedCacheInfo->GetAlternativeDataType(aType);
  }

  // Must be called during or after OnStartRequest
  if (!mAfterOnStartRequestBegun) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  aType = mAvailableCachedAltDataType;
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::OpenAlternativeOutputStream(const nsACString& aType,
                                              int64_t aPredictedSize,
                                              nsIAsyncOutputStream** _retval) {
  MOZ_ASSERT(NS_IsMainThread(), "Main thread only");

  if (mSynthesizedCacheInfo) {
    return mSynthesizedCacheInfo->OpenAlternativeOutputStream(
        aType, aPredictedSize, _retval);
  }

  if (!CanSend()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  if (static_cast<ContentChild*>(gNeckoChild->Manager())->IsShuttingDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsIEventTarget> neckoTarget = GetNeckoTarget();
  MOZ_ASSERT(neckoTarget);

  RefPtr<AltDataOutputStreamChild> stream = new AltDataOutputStreamChild();
  stream->AddIPDLReference();

  gNeckoChild->SetEventTargetForActor(stream, neckoTarget);

  if (!gNeckoChild->SendPAltDataOutputStreamConstructor(
          stream, nsCString(aType), aPredictedSize, this)) {
    return NS_ERROR_FAILURE;
  }

  stream.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::GetOriginalInputStream(nsIInputStreamReceiver* aReceiver) {
  if (aReceiver == nullptr) {
    return NS_ERROR_INVALID_ARG;
  }

  if (!CanSend()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mOriginalInputStreamReceiver = aReceiver;
  Unused << SendOpenOriginalCacheInputStream();

  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::GetAltDataInputStream(const nsACString& aType,
                                        nsIInputStreamReceiver* aReceiver) {
  if (aReceiver == nullptr) {
    return NS_ERROR_INVALID_ARG;
  }

  if (!CanSend()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mAltDataInputStreamReceiver = aReceiver;
  Unused << SendOpenAltDataCacheInputStream(nsCString(aType));

  return NS_OK;
}

mozilla::ipc::IPCResult HttpChannelChild::RecvOriginalCacheInputStreamAvailable(
    const Maybe<IPCStream>& aStream) {
  nsCOMPtr<nsIInputStream> stream = DeserializeIPCStream(aStream);
  nsCOMPtr<nsIInputStreamReceiver> receiver;
  receiver.swap(mOriginalInputStreamReceiver);
  if (receiver) {
    receiver->OnInputStreamReady(stream);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult HttpChannelChild::RecvAltDataCacheInputStreamAvailable(
    const Maybe<IPCStream>& aStream) {
  nsCOMPtr<nsIInputStream> stream = DeserializeIPCStream(aStream);
  nsCOMPtr<nsIInputStreamReceiver> receiver;
  receiver.swap(mAltDataInputStreamReceiver);
  if (receiver) {
    receiver->OnInputStreamReady(stream);
  }

  return IPC_OK();
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIResumableChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::ResumeAt(uint64_t startPos, const nsACString& entityID) {
  LOG(("HttpChannelChild::ResumeAt [this=%p]\n", this));
  ENSURE_CALLED_BEFORE_CONNECT();
  mStartPos = startPos;
  mEntityID = entityID;
  mSendResumeAt = true;
  return NS_OK;
}

// GetEntityID is shared in HttpBaseChannel

//-----------------------------------------------------------------------------
// HttpChannelChild::nsISupportsPriority
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::SetPriority(int32_t aPriority) {
  LOG(("HttpChannelChild::SetPriority %p p=%d", this, aPriority));

  int16_t newValue = clamped<int32_t>(aPriority, INT16_MIN, INT16_MAX);
  if (mPriority == newValue) return NS_OK;
  mPriority = newValue;
  if (RemoteChannelExists()) SendSetPriority(mPriority);
  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIClassOfService
//-----------------------------------------------------------------------------
NS_IMETHODIMP
HttpChannelChild::SetClassFlags(uint32_t inFlags) {
  if (mClassOfService == inFlags) {
    return NS_OK;
  }

  mClassOfService = inFlags;

  LOG(("HttpChannelChild %p ClassOfService=%u", this, mClassOfService));

  if (RemoteChannelExists()) {
    SendSetClassOfService(mClassOfService);
  }
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::AddClassFlags(uint32_t inFlags) {
  mClassOfService |= inFlags;

  LOG(("HttpChannelChild %p ClassOfService=%u", this, mClassOfService));

  if (RemoteChannelExists()) {
    SendSetClassOfService(mClassOfService);
  }
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::ClearClassFlags(uint32_t inFlags) {
  mClassOfService &= ~inFlags;

  LOG(("HttpChannelChild %p ClassOfService=%u", this, mClassOfService));

  if (RemoteChannelExists()) {
    SendSetClassOfService(mClassOfService);
  }
  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIProxiedChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::GetProxyInfo(nsIProxyInfo** aProxyInfo) { DROP_DEAD(); }

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIApplicationCacheContainer
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::GetApplicationCache(nsIApplicationCache** aApplicationCache) {
  NS_IF_ADDREF(*aApplicationCache = mApplicationCache);
  return NS_OK;
}
NS_IMETHODIMP
HttpChannelChild::SetApplicationCache(nsIApplicationCache* aApplicationCache) {
  NS_ENSURE_TRUE(!mWasOpened, NS_ERROR_ALREADY_OPENED);

  mApplicationCache = aApplicationCache;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIApplicationCacheChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::GetApplicationCacheForWrite(
    nsIApplicationCache** aApplicationCache) {
  *aApplicationCache = nullptr;
  return NS_OK;
}
NS_IMETHODIMP
HttpChannelChild::SetApplicationCacheForWrite(
    nsIApplicationCache* aApplicationCache) {
  NS_ENSURE_TRUE(!mWasOpened, NS_ERROR_ALREADY_OPENED);

  // Child channels are not intended to be used for cache writes
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
HttpChannelChild::GetLoadedFromApplicationCache(
    bool* aLoadedFromApplicationCache) {
  *aLoadedFromApplicationCache = mLoadedFromApplicationCache;
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::GetInheritApplicationCache(bool* aInherit) {
  *aInherit = mInheritApplicationCache;
  return NS_OK;
}
NS_IMETHODIMP
HttpChannelChild::SetInheritApplicationCache(bool aInherit) {
  mInheritApplicationCache = aInherit;
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::GetChooseApplicationCache(bool* aChoose) {
  *aChoose = mChooseApplicationCache;
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::SetChooseApplicationCache(bool aChoose) {
  mChooseApplicationCache = aChoose;
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::MarkOfflineCacheEntryAsForeign() {
  SendMarkOfflineCacheEntryAsForeign();
  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIHttpChannelChild
//-----------------------------------------------------------------------------

NS_IMETHODIMP HttpChannelChild::AddCookiesToRequest() {
  HttpBaseChannel::AddCookiesToRequest();
  return NS_OK;
}

NS_IMETHODIMP HttpChannelChild::GetClientSetRequestHeaders(
    RequestHeaderTuples** aRequestHeaders) {
  *aRequestHeaders = &mClientSetRequestHeaders;
  return NS_OK;
}

void HttpChannelChild::GetClientSetCorsPreflightParameters(
    Maybe<CorsPreflightArgs>& aArgs) {
  if (mRequireCORSPreflight) {
    CorsPreflightArgs args;
    args.unsafeHeaders() = mUnsafeHeaders;
    aArgs.emplace(args);
  } else {
    aArgs = Nothing();
  }
}

NS_IMETHODIMP
HttpChannelChild::RemoveCorsPreflightCacheEntry(nsIURI* aURI,
                                                nsIPrincipal* aPrincipal) {
  URIParams uri;
  SerializeURI(aURI, uri);
  PrincipalInfo principalInfo;
  nsresult rv = PrincipalToPrincipalInfo(aPrincipal, &principalInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  bool result = false;
  // Be careful to not attempt to send a message to the parent after the
  // actor has been destroyed.
  if (CanSend()) {
    result = SendRemoveCorsPreflightCacheEntry(uri, principalInfo);
  }
  return result ? NS_OK : NS_ERROR_FAILURE;
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIDivertableChannel
//-----------------------------------------------------------------------------
NS_IMETHODIMP
HttpChannelChild::DivertToParent(ChannelDiverterChild** aChild) {
  LOG(("HttpChannelChild::DivertToParent [this=%p]\n", this));
  MOZ_RELEASE_ASSERT(aChild);
  MOZ_RELEASE_ASSERT(gNeckoChild);
  MOZ_RELEASE_ASSERT(!mDivertingToParent);
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv = NS_OK;

  // If the channel was intercepted, then we likely do not have an IPC actor
  // yet.  We need one, though, in order to have a parent side to divert to.
  // Therefore, create the actor just in time for us to suspend and divert it.
  if (mSynthesizedResponse && !RemoteChannelExists()) {
    mSuspendParentAfterSynthesizeResponse = true;
    rv = ContinueAsyncOpen();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // We must fail DivertToParent() if there's no parent end of the channel (and
  // won't be!) due to early failure.
  if (NS_FAILED(mStatus) && !RemoteChannelExists()) {
    return mStatus;
  }

  // Once this is set, it should not be unset before the child is taken down.
  mDivertingToParent = true;

  rv = Suspend();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (static_cast<ContentChild*>(gNeckoChild->Manager())->IsShuttingDown()) {
    return NS_ERROR_FAILURE;
  }

  HttpChannelDiverterArgs args;
  args.mChannelChild() = this;
  args.mApplyConversion() = mApplyConversion;

  PChannelDiverterChild* diverter =
      gNeckoChild->SendPChannelDiverterConstructor(args);
  MOZ_RELEASE_ASSERT(diverter);

  *aChild = static_cast<ChannelDiverterChild*>(diverter);

  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::UnknownDecoderInvolvedKeepData() {
  LOG(("HttpChannelChild::UnknownDecoderInvolvedKeepData [this=%p]", this));
  MOZ_ASSERT(NS_IsMainThread());

  mUnknownDecoderInvolved = true;
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::UnknownDecoderInvolvedOnStartRequestCalled() {
  LOG(
      ("HttpChannelChild::UnknownDecoderInvolvedOnStartRequestCalled "
       "[this=%p, mDivertingToParent=%d]",
       this, static_cast<bool>(mDivertingToParent)));
  MOZ_ASSERT(NS_IsMainThread());

  mUnknownDecoderInvolved = false;

  nsresult rv = NS_OK;

  if (mDivertingToParent) {
    rv = mEventQ->PrependEvents(mUnknownDecoderEventQ);
  }
  mUnknownDecoderEventQ.Clear();

  return rv;
}

NS_IMETHODIMP
HttpChannelChild::GetDivertingToParent(bool* aDiverting) {
  NS_ENSURE_ARG_POINTER(aDiverting);
  *aDiverting = mDivertingToParent;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIThreadRetargetableRequest
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::RetargetDeliveryTo(nsIEventTarget* aNewTarget) {
  LOG(("HttpChannelChild::RetargetDeliveryTo [this=%p, aNewTarget=%p]", this,
       aNewTarget));

  MOZ_ASSERT(NS_IsMainThread(), "Should be called on main thread only");
  MOZ_ASSERT(!mODATarget);
  MOZ_ASSERT(aNewTarget);

  NS_ENSURE_ARG(aNewTarget);
  if (aNewTarget->IsOnCurrentThread()) {
    NS_WARNING("Retargeting delivery to same thread");
    mOMTResult = LABELS_HTTP_CHILD_OMT_STATS::successMainThread;
    return NS_OK;
  }

  // Ensure that |mListener| and any subsequent listeners can be retargeted
  // to another thread.
  nsresult rv = NS_OK;
  nsCOMPtr<nsIThreadRetargetableStreamListener> retargetableListener =
      do_QueryInterface(mListener, &rv);
  if (!retargetableListener || NS_FAILED(rv)) {
    NS_WARNING("Listener is not retargetable");
    mOMTResult = LABELS_HTTP_CHILD_OMT_STATS::failListener;
    return NS_ERROR_NO_INTERFACE;
  }

  rv = retargetableListener->CheckListenerChain();
  if (NS_FAILED(rv)) {
    NS_WARNING("Subsequent listeners are not retargetable");
    mOMTResult = LABELS_HTTP_CHILD_OMT_STATS::failListenerChain;
    return rv;
  }

  {
    MutexAutoLock lock(mEventTargetMutex);
    mODATarget = aNewTarget;
  }

  mOMTResult = LABELS_HTTP_CHILD_OMT_STATS::success;
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::GetDeliveryTarget(nsIEventTarget** aEventTarget) {
  MutexAutoLock lock(mEventTargetMutex);

  nsCOMPtr<nsIEventTarget> target = mODATarget;
  if (!mODATarget) {
    target = GetCurrentThreadEventTarget();
  }
  target.forget(aEventTarget);
  return NS_OK;
}

void HttpChannelChild::ResetInterception() {
  NS_ENSURE_TRUE_VOID(gNeckoChild != nullptr);

  if (mInterceptListener) {
    mInterceptListener->Cleanup();
  }
  mInterceptListener = nullptr;

  // The chance to intercept any further requests associated with this channel
  // (such as redirects) has passed.
  if (mRedirectMode != nsIHttpChannelInternal::REDIRECT_MODE_MANUAL) {
    mLoadFlags |= LOAD_BYPASS_SERVICE_WORKER;
  }

  // If the channel has already been aborted or canceled, just stop.
  if (NS_FAILED(mStatus)) {
    return;
  }

  // Continue with the original cross-process request
  nsresult rv = ContinueAsyncOpen();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Unused << Cancel(rv);
  }
}

void HttpChannelChild::TrySendDeletingChannel() {
  AUTO_PROFILER_LABEL("HttpChannelChild::TrySendDeletingChannel", NETWORK);
  if (!mDeletingChannelSent.compareExchange(false, true)) {
    // SendDeletingChannel is already sent.
    return;
  }

  if (NS_IsMainThread()) {
    if (NS_WARN_IF(!CanSend())) {
      // IPC actor is detroyed already, do not send more messages.
      return;
    }

    Unused << PHttpChannelChild::SendDeletingChannel();
    return;
  }

  nsCOMPtr<nsIEventTarget> neckoTarget = GetNeckoTarget();
  MOZ_ASSERT(neckoTarget);

  DebugOnly<nsresult> rv = neckoTarget->Dispatch(
      NewNonOwningRunnableMethod(
          "net::HttpChannelChild::TrySendDeletingChannel", this,
          &HttpChannelChild::TrySendDeletingChannel),
      NS_DISPATCH_NORMAL);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

void HttpChannelChild::OnCopyComplete(nsresult aStatus) {
  nsCOMPtr<nsIRunnable> runnable = NewRunnableMethod<nsresult>(
      "net::HttpBaseChannel::EnsureUploadStreamIsCloneableComplete", this,
      &HttpChannelChild::EnsureUploadStreamIsCloneableComplete, aStatus);
  nsCOMPtr<nsIEventTarget> neckoTarget = GetNeckoTarget();
  MOZ_ASSERT(neckoTarget);

  Unused << neckoTarget->Dispatch(runnable, NS_DISPATCH_NORMAL);
}

nsresult HttpChannelChild::AsyncCallImpl(
    void (HttpChannelChild::*funcPtr)(),
    nsRunnableMethod<HttpChannelChild>** retval) {
  nsresult rv;

  RefPtr<nsRunnableMethod<HttpChannelChild>> event =
      NewRunnableMethod("net::HttpChannelChild::AsyncCall", this, funcPtr);
  nsCOMPtr<nsIEventTarget> neckoTarget = GetNeckoTarget();
  MOZ_ASSERT(neckoTarget);

  rv = neckoTarget->Dispatch(event, NS_DISPATCH_NORMAL);

  if (NS_SUCCEEDED(rv) && retval) {
    *retval = event;
  }

  return rv;
}

nsresult HttpChannelChild::SetReferrerHeader(const nsACString& aReferrer) {
  // Normally this would be ENSURE_CALLED_BEFORE_CONNECT, but since the
  // "connect" is done in the main process, and mRequestObserversCalled is never
  // set in the ChannelChild, before connect basically means before asyncOpen.
  ENSURE_CALLED_BEFORE_ASYNC_OPEN();

  // remove old referrer if any, loop backwards
  for (int i = mClientSetRequestHeaders.Length() - 1; i >= 0; --i) {
    if (NS_LITERAL_CSTRING("Referer").Equals(
            mClientSetRequestHeaders[i].mHeader)) {
      mClientSetRequestHeaders.RemoveElementAt(i);
    }
  }

  return HttpBaseChannel::SetReferrerHeader(aReferrer);
}

class CancelEvent final : public NeckoTargetChannelEvent<HttpChannelChild> {
 public:
  CancelEvent(HttpChannelChild* aChild, nsresult aRv)
      : NeckoTargetChannelEvent<HttpChannelChild>(aChild), mRv(aRv) {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(aChild);
  }

  void Run() override {
    MOZ_ASSERT(NS_IsMainThread());
    mChild->Cancel(mRv);
  }

 private:
  const nsresult mRv;
};

void HttpChannelChild::CancelOnMainThread(nsresult aRv) {
  LOG(("HttpChannelChild::CancelOnMainThread [this=%p]", this));

  if (NS_IsMainThread()) {
    Cancel(aRv);
    return;
  }

  mEventQ->Suspend();
  // Cancel is expected to preempt any other channel events, thus we put this
  // event in the front of mEventQ to make sure nsIStreamListener not receiving
  // any ODA/OnStopRequest callbacks.
  UniquePtr<ChannelEvent> cancelEvent = MakeUnique<CancelEvent>(this, aRv);
  mEventQ->PrependEvent(cancelEvent);
  mEventQ->Resume();
}

void HttpChannelChild::OverrideWithSynthesizedResponse(
    nsAutoPtr<nsHttpResponseHead>& aResponseHead,
    nsIInputStream* aSynthesizedInput,
    nsIInterceptedBodyCallback* aSynthesizedCallback,
    InterceptStreamListener* aStreamListener,
    nsICacheInfoChannel* aCacheInfoChannel) {
  MOZ_ASSERT(NS_IsMainThread());
  nsresult rv = NS_OK;
  auto autoCleanup = MakeScopeExit([&] {
    // Auto-cancel on failure.  Do this first to get mStatus set, if necessary.
    if (NS_FAILED(rv)) {
      Cancel(rv);
    }

    // If we early exit before taking ownership of the body, then automatically
    // invoke the callback.  This could be due to an error or because we're not
    // going to consume it due to a redirect, etc.
    if (aSynthesizedCallback) {
      aSynthesizedCallback->BodyComplete(mStatus);
    }
  });

  if (NS_FAILED(mStatus)) {
    return;
  }

  mInterceptListener = aStreamListener;

  // Intercepted responses should already be decoded.  If its a redirect,
  // however, we want to respect the encoding of the final result instead.
  if (!nsHttpChannel::WillRedirect(aResponseHead)) {
    SetApplyConversion(false);
  }

  mResponseHead = aResponseHead;
  mSynthesizedResponse = true;

  mSynthesizedInput = aSynthesizedInput;

  if (!mSynthesizedInput) {
    rv = NS_NewCStringInputStream(getter_AddRefs(mSynthesizedInput),
                                  EmptyCString());
    NS_ENSURE_SUCCESS_VOID(rv);
  }

  if (nsHttpChannel::WillRedirect(mResponseHead)) {
    // Normally we handle redirect limits in the parent process.  The way
    // e10s synthesized redirects work, however, the parent process does not
    // get an accurate redirect count.  Therefore we need to enforce it here.
    rv = CheckRedirectLimit(nsIChannelEventSink::REDIRECT_TEMPORARY);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      Cancel(rv);
      return;
    }

    mShouldInterceptSubsequentRedirect = true;
    if (mInterceptListener) {
      mInterceptListener->Cleanup();
      mInterceptListener = nullptr;
    }
    // Continue with the original cross-process request
    rv = ContinueAsyncOpen();
    return;
  }

  // For progress we trust the content-length for the "maximum" size.
  // We can't determine the full size from the stream itself since we
  // only receive the data incrementally.  We can't trust Available()
  // here.
  // TODO: We could implement an nsIFixedLengthInputStream interface and
  //       QI to it here.  This would let us determine the total length
  //       for streams that support it.  See bug 1388774.
  rv = GetContentLength(&mSynthesizedStreamLength);
  if (NS_FAILED(rv)) {
    mSynthesizedStreamLength = -1;
  }

  nsCOMPtr<nsIEventTarget> neckoTarget = GetNeckoTarget();
  MOZ_ASSERT(neckoTarget);

  rv = nsInputStreamPump::Create(getter_AddRefs(mSynthesizedResponsePump),
                                 mSynthesizedInput, 0, 0, true, neckoTarget);
  NS_ENSURE_SUCCESS_VOID(rv);

  mSynthesizedCacheInfo = aCacheInfoChannel;

  rv = mSynthesizedResponsePump->AsyncRead(aStreamListener, nullptr);
  NS_ENSURE_SUCCESS_VOID(rv);

  // The pump is started, so take ownership of the body callback.  We
  // clear the argument to avoid auto-completing it via the ScopeExit
  // lambda.
  mSynthesizedCallback = aSynthesizedCallback;
  aSynthesizedCallback = nullptr;

  // if this channel has been suspended previously, the pump needs to be
  // correspondingly suspended now that it exists.
  for (uint32_t i = 0; i < mSuspendCount; i++) {
    rv = mSynthesizedResponsePump->Suspend();
    NS_ENSURE_SUCCESS_VOID(rv);
  }

  MOZ_DIAGNOSTIC_ASSERT(!mCanceled);
}

NS_IMETHODIMP
HttpChannelChild::ForceIntercepted(bool aPostRedirectChannelShouldIntercept,
                                   bool aPostRedirectChannelShouldUpgrade) {
  MOZ_ASSERT(NS_IsMainThread());
  mShouldParentIntercept = true;
  mPostRedirectChannelShouldIntercept = aPostRedirectChannelShouldIntercept;
  mPostRedirectChannelShouldUpgrade = aPostRedirectChannelShouldUpgrade;
  return NS_OK;
}

void HttpChannelChild::ForceIntercepted(
    nsIInputStream* aSynthesizedInput,
    nsIInterceptedBodyCallback* aSynthesizedCallback,
    nsICacheInfoChannel* aCacheInfo) {
  MOZ_ASSERT(NS_IsMainThread());
  mSynthesizedInput = aSynthesizedInput;
  mSynthesizedCallback = aSynthesizedCallback;
  mSynthesizedCacheInfo = aCacheInfo;
  mSynthesizedResponse = true;
  mRedirectingForSubsequentSynthesizedResponse = true;
}

mozilla::ipc::IPCResult HttpChannelChild::RecvIssueDeprecationWarning(
    const uint32_t& warning, const bool& asError) {
  nsCOMPtr<nsIDeprecationWarner> warner;
  GetCallback(warner);
  if (warner) {
    warner->IssueWarning(warning, asError);
  }
  return IPC_OK();
}

bool HttpChannelChild::ShouldInterceptURI(nsIURI* aURI, bool& aShouldUpgrade) {
  nsCOMPtr<nsIPrincipal> resultPrincipal;
  if (!aURI->SchemeIs("https") && mLoadInfo) {
    nsContentUtils::GetSecurityManager()->GetChannelResultPrincipal(
        this, getter_AddRefs(resultPrincipal));
  }
  OriginAttributes originAttributes;
  NS_ENSURE_TRUE(NS_GetOriginAttributes(this, originAttributes), false);
  bool notused = false;
  nsresult rv = NS_ShouldSecureUpgrade(
      aURI, mLoadInfo, resultPrincipal, mPrivateBrowsing, mAllowSTS,
      originAttributes, aShouldUpgrade, nullptr, notused);
  NS_ENSURE_SUCCESS(rv, false);

  nsCOMPtr<nsIURI> upgradedURI;
  if (aShouldUpgrade) {
    rv = NS_GetSecureUpgradedURI(aURI, getter_AddRefs(upgradedURI));
    NS_ENSURE_SUCCESS(rv, false);
  }

  return ShouldIntercept(upgradedURI ? upgradedURI.get() : aURI);
}

mozilla::ipc::IPCResult HttpChannelChild::RecvSetPriority(
    const int16_t& aPriority) {
  mPriority = aPriority;
  return IPC_OK();
}

mozilla::ipc::IPCResult HttpChannelChild::RecvAttachStreamFilter(
    Endpoint<extensions::PStreamFilterParent>&& aEndpoint) {
  extensions::StreamFilterParent::Attach(this, std::move(aEndpoint));
  return IPC_OK();
}

mozilla::ipc::IPCResult HttpChannelChild::RecvCancelDiversion() {
  MOZ_ASSERT(NS_IsMainThread());

  // This method is a very special case for cancellation of a diverted
  // intercepted channel.  Normally we would go through the mEventQ in order to
  // serialize event execution in the face of sync XHR and now background
  // channels.  However, similar to how CancelOnMainThread describes, Cancel()
  // pre-empts everything.  (And frankly, we want this stack frame on the stack
  // if a crash happens.)
  Cancel(NS_ERROR_ABORT);
  return IPC_OK();
}

void HttpChannelChild::ActorDestroy(ActorDestroyReason aWhy) {
  MOZ_ASSERT(NS_IsMainThread());

  // OnStartRequest might be dropped if IPDL is destroyed abnormally
  // and BackgroundChild might have pending IPC messages.
  // Clean up BackgroundChild at this time to prevent memleak.
  if (aWhy != Deletion) {
    // Make sure all the messages are processed.
    AutoEventEnqueuer ensureSerialDispatch(mEventQ);

    mStatus = NS_ERROR_DOCSHELL_DYING;
    HandleAsyncAbort();

    // Cleanup the background channel before we resume the eventQ so we don't
    // get any other events.
    CleanupBackgroundChannel();

    mIPCActorDeleted = true;
    mCanceled = true;
  }
}

mozilla::ipc::IPCResult HttpChannelChild::RecvLogBlockedCORSRequest(
    const nsString& aMessage, const nsCString& aCategory) {
  Unused << LogBlockedCORSRequest(aMessage, aCategory);
  return IPC_OK();
}

NS_IMETHODIMP
HttpChannelChild::LogBlockedCORSRequest(const nsAString& aMessage,
                                        const nsACString& aCategory) {
  if (mLoadInfo) {
    uint64_t innerWindowID = mLoadInfo->GetInnerWindowID();
    bool privateBrowsing =
        !!mLoadInfo->GetOriginAttributes().mPrivateBrowsingId;
    bool fromChromeContext =
        nsContentUtils::IsSystemPrincipal(mLoadInfo->TriggeringPrincipal());
    nsCORSListenerProxy::LogBlockedCORSRequest(
        innerWindowID, privateBrowsing, fromChromeContext, aMessage, aCategory);
  }
  return NS_OK;
}

mozilla::ipc::IPCResult HttpChannelChild::RecvLogMimeTypeMismatch(
    const nsCString& aMessageName, const bool& aWarning, const nsString& aURL,
    const nsString& aContentType) {
  Unused << LogMimeTypeMismatch(aMessageName, aWarning, aURL, aContentType);
  return IPC_OK();
}

NS_IMETHODIMP
HttpChannelChild::LogMimeTypeMismatch(const nsACString& aMessageName,
                                      bool aWarning, const nsAString& aURL,
                                      const nsAString& aContentType) {
  RefPtr<Document> doc;
  if (mLoadInfo) {
    mLoadInfo->GetLoadingDocument(getter_AddRefs(doc));
  }

  AutoTArray<nsString, 2> params;
  params.AppendElement(aURL);
  params.AppendElement(aContentType);
  nsContentUtils::ReportToConsole(
      aWarning ? nsIScriptError::warningFlag : nsIScriptError::errorFlag,
      NS_LITERAL_CSTRING("MIMEMISMATCH"), doc,
      nsContentUtils::eSECURITY_PROPERTIES, nsCString(aMessageName).get(),
      params);
  return NS_OK;
}

void HttpChannelChild::MaybeCallSynthesizedCallback() {
  if (!mSynthesizedCallback) {
    return;
  }

  mSynthesizedCallback->BodyComplete(mStatus);
  mSynthesizedCallback = nullptr;
}

nsresult HttpChannelChild::CrossProcessRedirectFinished(nsresult aStatus) {
  if (!CanSend()) {
    return NS_BINDING_FAILED;
  }

  if (!mCanceled && NS_SUCCEEDED(mStatus)) {
    mStatus = aStatus;
  }

  // The loadInfo is updated in nsDocShell::OpenInitializedChannel to have the
  // correct attributes (such as browsingContextID).
  // We need to send it to the parent channel so the two match, which is done
  nsCOMPtr<nsILoadInfo> loadInfo;
  MOZ_ALWAYS_SUCCEEDS(GetLoadInfo(getter_AddRefs(loadInfo)));
  Maybe<LoadInfoArgs> loadInfoArgs;
  MOZ_ALWAYS_SUCCEEDS(
      mozilla::ipc::LoadInfoToLoadInfoArgs(loadInfo, &loadInfoArgs));
  Unused << SendCrossProcessRedirectDone(mStatus, loadInfoArgs);
  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
