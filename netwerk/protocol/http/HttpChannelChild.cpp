/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "nsHttp.h"
#include "nsICacheEntry.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/TabGroup.h"
#include "mozilla/ipc/FileDescriptorSetChild.h"
#include "mozilla/ipc/IPCStreamUtils.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/net/HttpChannelChild.h"

#include "AltDataOutputStreamChild.h"
#include "HttpBackgroundChannelChild.h"
#include "nsCOMPtr.h"
#include "nsISupportsPrimitives.h"
#include "nsChannelClassifier.h"
#include "nsGlobalWindow.h"
#include "nsStringStream.h"
#include "nsHttpChannel.h"
#include "nsHttpHandler.h"
#include "nsNetUtil.h"
#include "nsSerializationHelper.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/Performance.h"
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
#include "nsIDeprecationWarner.h"
#include "nsICompressConvStats.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMWindowUtils.h"
#include "nsIEventTarget.h"
#include "nsRedirectHistoryEntry.h"
#include "nsStreamUtils.h"
#include "nsThreadUtils.h"

#ifdef MOZ_TASK_TRACER
#include "GeckoTaskTracer.h"
#endif

using namespace mozilla::dom;
using namespace mozilla::ipc;

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(InterceptStreamListener,
                  nsIStreamListener,
                  nsIRequestObserver,
                  nsIProgressEventSink)

NS_IMETHODIMP
InterceptStreamListener::OnStartRequest(nsIRequest* aRequest, nsISupports* aContext)
{
  if (mOwner) {
    mOwner->DoOnStartRequest(mOwner, mContext);
  }
  return NS_OK;
}

NS_IMETHODIMP
InterceptStreamListener::OnStatus(nsIRequest* aRequest, nsISupports* aContext,
                                  nsresult status, const char16_t* aStatusArg)
{
  if (mOwner) {
    mOwner->DoOnStatus(mOwner, status);
  }
  return NS_OK;
}

NS_IMETHODIMP
InterceptStreamListener::OnProgress(nsIRequest* aRequest, nsISupports* aContext,
                                    int64_t aProgress, int64_t aProgressMax)
{
  if (mOwner) {
    mOwner->DoOnProgress(mOwner, aProgress, aProgressMax);
  }
  return NS_OK;
}

NS_IMETHODIMP
InterceptStreamListener::OnDataAvailable(nsIRequest* aRequest, nsISupports* aContext,
                                         nsIInputStream* aInputStream, uint64_t aOffset,
                                         uint32_t aCount)
{
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

    OnStatus(mOwner, aContext, NS_NET_STATUS_READING, NS_ConvertUTF8toUTF16(host).get());

    int64_t progress = aOffset + aCount;
    OnProgress(mOwner, aContext, progress, mOwner->mSynthesizedStreamLength);
  }

  mOwner->DoOnDataAvailable(mOwner, mContext, aInputStream, aOffset, aCount);
  return NS_OK;
}

NS_IMETHODIMP
InterceptStreamListener::OnStopRequest(nsIRequest* aRequest, nsISupports* aContext, nsresult aStatusCode)
{
  if (mOwner) {
    mOwner->DoPreOnStopRequest(aStatusCode);
    mOwner->DoOnStopRequest(mOwner, aStatusCode, mContext);
  }
  Cleanup();
  return NS_OK;
}

void
InterceptStreamListener::Cleanup()
{
  mOwner = nullptr;
  mContext = nullptr;
}

//-----------------------------------------------------------------------------
// HttpChannelChild
//-----------------------------------------------------------------------------

HttpChannelChild::HttpChannelChild()
  : HttpAsyncAborter<HttpChannelChild>(this)
  , mSynthesizedStreamLength(0)
  , mIsFromCache(false)
  , mCacheEntryAvailable(false)
  , mCacheExpirationTime(nsICacheEntry::NO_EXPIRATION_TIME)
  , mSendResumeAt(false)
  , mDeletingChannelSent(false)
  , mIPCOpen(false)
  , mKeptAlive(false)
  , mUnknownDecoderInvolved(false)
  , mDivertingToParent(false)
  , mFlushedForDiversion(false)
  , mSuspendSent(false)
  , mSynthesizedResponse(false)
  , mShouldInterceptSubsequentRedirect(false)
  , mRedirectingForSubsequentSynthesizedResponse(false)
  , mPostRedirectChannelShouldIntercept(false)
  , mPostRedirectChannelShouldUpgrade(false)
  , mShouldParentIntercept(false)
  , mSuspendParentAfterSynthesizeResponse(false)
  , mEventTargetMutex("HttpChannelChild::EventTargetMutex")
{
  LOG(("Creating HttpChannelChild @%p\n", this));

  mChannelCreationTime = PR_Now();
  mChannelCreationTimestamp = TimeStamp::Now();
  mAsyncOpenTime = TimeStamp::Now();
  mEventQ = new ChannelEventQueue(static_cast<nsIHttpChannel*>(this));
}

HttpChannelChild::~HttpChannelChild()
{
  LOG(("Destroying HttpChannelChild @%p\n", this));

  ReleaseMainThreadOnlyReferences();
}

void
HttpChannelChild::ReleaseMainThreadOnlyReferences()
{
  if (NS_IsMainThread()) {
      // Already on main thread, let dtor to
      // take care of releasing references
      return;
  }

  nsTArray<nsCOMPtr<nsISupports>> arrayToRelease;
  arrayToRelease.AppendElement(mCacheKey.forget());

  NS_DispatchToMainThread(new ProxyReleaseRunnable(Move(arrayToRelease)));
}
//-----------------------------------------------------------------------------
// HttpChannelChild::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ADDREF(HttpChannelChild)

NS_IMETHODIMP_(MozExternalRefCountType) HttpChannelChild::Release()
{
  nsrefcnt count = --mRefCnt;
  MOZ_ASSERT(int32_t(count) >= 0, "dup release");
  NS_LOG_RELEASE(this, count, "HttpChannelChild");

  // Normally we Send_delete in OnStopRequest, but when we need to retain the
  // remote channel for security info IPDL itself holds 1 reference, so we
  // Send_delete when refCnt==1.  But if !mIPCOpen, then there's nobody to send
  // to, so we fall through.
  if (mKeptAlive && count == 1 && mIPCOpen) {
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
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIAssociatedContentSecurity, GetAssociatedContentSecurity())
  NS_INTERFACE_MAP_ENTRY(nsIDivertableChannel)
  NS_INTERFACE_MAP_ENTRY(nsIThreadRetargetableRequest)
NS_INTERFACE_MAP_END_INHERITING(HttpBaseChannel)

//-----------------------------------------------------------------------------
// HttpChannelChild::PHttpChannelChild
//-----------------------------------------------------------------------------

void
HttpChannelChild::AddIPDLReference()
{
  MOZ_ASSERT(!mIPCOpen, "Attempt to retain more than one IPDL reference");
  mIPCOpen = true;
  AddRef();
}

void
HttpChannelChild::ReleaseIPDLReference()
{
  MOZ_ASSERT(mIPCOpen, "Attempt to release nonexistent IPDL reference");
  mIPCOpen = false;
  Release();
}

void
HttpChannelChild::OnBackgroundChildReady(HttpBackgroundChannelChild* aBgChild)
{
  LOG(("HttpChannelChild::OnBackgroundChildReady [this=%p, bgChild=%p]\n",
       this, aBgChild));
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mBgChild);

  MOZ_ASSERT(mBgChild == aBgChild);
}

void
HttpChannelChild::OnBackgroundChildDestroyed()
{
  LOG(("HttpChannelChild::OnBackgroundChildDestroyed [this=%p]\n", this));
  MOZ_ASSERT(NS_IsMainThread());

  mBgChild = nullptr;
}

class AssociateApplicationCacheEvent : public ChannelEvent
{
  public:
    AssociateApplicationCacheEvent(HttpChannelChild* aChild,
                                   const nsCString &aGroupID,
                                   const nsCString &aClientID)
    : mChild(aChild)
    , groupID(aGroupID)
    , clientID(aClientID) {}

    void Run() { mChild->AssociateApplicationCache(groupID, clientID); }

    already_AddRefed<nsIEventTarget> GetEventTarget()
    {
      MOZ_ASSERT(mChild);
      nsCOMPtr<nsIEventTarget> target = mChild->GetNeckoTarget();
      return target.forget();
    }
  private:
    HttpChannelChild* mChild;
    nsCString groupID;
    nsCString clientID;
};

mozilla::ipc::IPCResult
HttpChannelChild::RecvAssociateApplicationCache(const nsCString &groupID,
                                                const nsCString &clientID)
{
  LOG(("HttpChannelChild::RecvAssociateApplicationCache [this=%p]\n", this));
  mEventQ->RunOrEnqueue(new AssociateApplicationCacheEvent(this, groupID,
                                                           clientID));
  return IPC_OK();
}

void
HttpChannelChild::AssociateApplicationCache(const nsCString &groupID,
                                            const nsCString &clientID)
{
  LOG(("HttpChannelChild::AssociateApplicationCache [this=%p]\n", this));
  nsresult rv;
  mApplicationCache = do_CreateInstance(NS_APPLICATIONCACHE_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return;

  mLoadedFromApplicationCache = true;
  mApplicationCache->InitAsHandle(groupID, clientID);
}

class StartRequestEvent : public ChannelEvent
{
 public:
  StartRequestEvent(HttpChannelChild* aChild,
                    const nsresult& aChannelStatus,
                    const nsHttpResponseHead& aResponseHead,
                    const bool& aUseResponseHead,
                    const nsHttpHeaderArray& aRequestHeaders,
                    const bool& aIsFromCache,
                    const bool& aCacheEntryAvailable,
                    const uint32_t& aCacheExpirationTime,
                    const nsCString& aCachedCharset,
                    const nsCString& aSecurityInfoSerialization,
                    const NetAddr& aSelfAddr,
                    const NetAddr& aPeerAddr,
                    const uint32_t& aCacheKey,
                    const nsCString& altDataType,
                    const int64_t& altDataLen)
  : mChild(aChild)
  , mChannelStatus(aChannelStatus)
  , mResponseHead(aResponseHead)
  , mRequestHeaders(aRequestHeaders)
  , mUseResponseHead(aUseResponseHead)
  , mIsFromCache(aIsFromCache)
  , mCacheEntryAvailable(aCacheEntryAvailable)
  , mCacheExpirationTime(aCacheExpirationTime)
  , mCachedCharset(aCachedCharset)
  , mSecurityInfoSerialization(aSecurityInfoSerialization)
  , mSelfAddr(aSelfAddr)
  , mPeerAddr(aPeerAddr)
  , mCacheKey(aCacheKey)
  , mAltDataType(altDataType)
  , mAltDataLen(altDataLen)
  {}

  void Run()
  {
    LOG(("StartRequestEvent [this=%p]\n", mChild));
    mChild->OnStartRequest(mChannelStatus, mResponseHead, mUseResponseHead,
                           mRequestHeaders, mIsFromCache, mCacheEntryAvailable,
                           mCacheExpirationTime, mCachedCharset,
                           mSecurityInfoSerialization, mSelfAddr, mPeerAddr,
                           mCacheKey, mAltDataType, mAltDataLen);
  }

  already_AddRefed<nsIEventTarget> GetEventTarget()
  {
    MOZ_ASSERT(mChild);
    nsCOMPtr<nsIEventTarget> target = mChild->GetNeckoTarget();
    return target.forget();
  }
 private:
  HttpChannelChild* mChild;
  nsresult mChannelStatus;
  nsHttpResponseHead mResponseHead;
  nsHttpHeaderArray mRequestHeaders;
  bool mUseResponseHead;
  bool mIsFromCache;
  bool mCacheEntryAvailable;
  uint32_t mCacheExpirationTime;
  nsCString mCachedCharset;
  nsCString mSecurityInfoSerialization;
  NetAddr mSelfAddr;
  NetAddr mPeerAddr;
  uint32_t mCacheKey;
  nsCString mAltDataType;
  int64_t mAltDataLen;
};

mozilla::ipc::IPCResult
HttpChannelChild::RecvOnStartRequest(const nsresult& channelStatus,
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
                                     const int64_t& altDataLen)
{
  LOG(("HttpChannelChild::RecvOnStartRequest [this=%p]\n", this));
  // mFlushedForDiversion and mDivertingToParent should NEVER be set at this
  // stage, as they are set in the listener's OnStartRequest.
  MOZ_RELEASE_ASSERT(!mFlushedForDiversion,
    "mFlushedForDiversion should be unset before OnStartRequest!");
  MOZ_RELEASE_ASSERT(!mDivertingToParent,
    "mDivertingToParent should be unset before OnStartRequest!");


  mRedirectCount = redirectCount;

  mEventQ->RunOrEnqueue(new StartRequestEvent(this, channelStatus, responseHead,
                                              useResponseHead, requestHeaders,
                                              isFromCache, cacheEntryAvailable,
                                              cacheExpirationTime,
                                              cachedCharset,
                                              securityInfoSerialization,
                                              selfAddr, peerAddr, cacheKey,
                                              altDataType, altDataLen));

  MOZ_ASSERT(mBgChild);
  if (mBgChild) {
    mBgChild->OnStartRequestReceived();
  }

  return IPC_OK();
}

void
HttpChannelChild::OnStartRequest(const nsresult& channelStatus,
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
                                 const int64_t& altDataLen)
{
  LOG(("HttpChannelChild::OnStartRequest [this=%p]\n", this));

  // mFlushedForDiversion and mDivertingToParent should NEVER be set at this
  // stage, as they are set in the listener's OnStartRequest.
  MOZ_RELEASE_ASSERT(!mFlushedForDiversion,
    "mFlushedForDiversion should be unset before OnStartRequest!");
  MOZ_RELEASE_ASSERT(!mDivertingToParent,
    "mDivertingToParent should be unset before OnStartRequest!");

  if (!mCanceled && NS_SUCCEEDED(mStatus)) {
    mStatus = channelStatus;
  }

  if (useResponseHead && !mCanceled)
    mResponseHead = new nsHttpResponseHead(responseHead);

  if (!securityInfoSerialization.IsEmpty()) {
    NS_DeserializeObject(securityInfoSerialization,
                         getter_AddRefs(mSecurityInfo));
  }

  mIsFromCache = isFromCache;
  mCacheEntryAvailable = cacheEntryAvailable;
  mCacheExpirationTime = cacheExpirationTime;
  mCachedCharset = cachedCharset;
  mSelfAddr = selfAddr;
  mPeerAddr = peerAddr;

  mAvailableCachedAltDataType = altDataType;
  mAltDataLength = altDataLen;

  mAfterOnStartRequestBegun = true;

  AutoEventEnqueuer ensureSerialDispatch(mEventQ);

  nsresult rv;
  nsCOMPtr<nsISupportsPRUint32> container =
    do_CreateInstance(NS_SUPPORTS_PRUINT32_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    Cancel(rv);
    return;
  }

  rv = container->SetData(cacheKey);
  if (NS_FAILED(rv)) {
    Cancel(rv);
    return;
  }
  mCacheKey = container;

  // replace our request headers with what actually got sent in the parent
  mRequestHead.SetHeaders(requestHeaders);

  // Note: this is where we would notify "http-on-examine-response" observers.
  // We have deliberately disabled this for child processes (see bug 806753)
  //
  // gHttpHandler->OnExamineResponse(this);

  mTracingEnabled = false;

  DoOnStartRequest(this, mListenerContext);
}

namespace {

class SyntheticDiversionListener final : public nsIStreamListener
{
  RefPtr<HttpChannelChild> mChannel;

  ~SyntheticDiversionListener()
  {
  }

public:
  explicit SyntheticDiversionListener(HttpChannelChild* aChannel)
    : mChannel(aChannel)
  {
    MOZ_ASSERT(mChannel);
  }

  NS_IMETHOD
  OnStartRequest(nsIRequest* aRequest, nsISupports* aContext) override
  {
    MOZ_ASSERT_UNREACHABLE("SyntheticDiversionListener should never see OnStartRequest");
    return NS_OK;
  }

  NS_IMETHOD
  OnStopRequest(nsIRequest* aRequest, nsISupports* aContext,
                nsresult aStatus) override
  {
    mChannel->SendDivertOnStopRequest(aStatus);
    return NS_OK;
  }

  NS_IMETHOD
  OnDataAvailable(nsIRequest* aRequest, nsISupports* aContext,
                  nsIInputStream* aInputStream, uint64_t aOffset,
                  uint32_t aCount) override
  {
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

} // anonymous namespace

void
HttpChannelChild::DoOnStartRequest(nsIRequest* aRequest, nsISupports* aContext)
{
  LOG(("HttpChannelChild::DoOnStartRequest [this=%p]\n", this));

  // In theory mListener should not be null, but in practice sometimes it is.
  MOZ_ASSERT(mListener);
  if (!mListener) {
    Cancel(NS_ERROR_FAILURE);
    return;
  }
  nsresult rv = mListener->OnStartRequest(aRequest, aContext);
  if (NS_FAILED(rv)) {
    Cancel(rv);
    return;
  }

  if (mDivertingToParent) {
    mListener = nullptr;
    mListenerContext = nullptr;
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
  rv = DoApplyContentConversions(mListener, getter_AddRefs(listener),
                                 mListenerContext);
  if (NS_FAILED(rv)) {
    Cancel(rv);
  } else if (listener) {
    mListener = listener;
    mCompressListener = listener;
  }
}

class TransportAndDataEvent : public ChannelEvent
{
 public:
  TransportAndDataEvent(HttpChannelChild* child,
                        const nsresult& channelStatus,
                        const nsresult& transportStatus,
                        const nsCString& data,
                        const uint64_t& offset,
                        const uint32_t& count)
  : mChild(child)
  , mChannelStatus(channelStatus)
  , mTransportStatus(transportStatus)
  , mData(data)
  , mOffset(offset)
  , mCount(count) {}

  void Run()
  {
    mChild->OnTransportAndData(mChannelStatus, mTransportStatus,
                               mOffset, mCount, mData);
  }

  already_AddRefed<nsIEventTarget> GetEventTarget()
  {
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

void
HttpChannelChild::ProcessOnTransportAndData(const nsresult& aChannelStatus,
                                            const nsresult& aTransportStatus,
                                            const uint64_t& aOffset,
                                            const uint32_t& aCount,
                                            const nsCString& aData)
{
  LOG(("HttpChannelChild::ProcessOnTransportAndData [this=%p]\n", this));
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(!mFlushedForDiversion,
                     "Should not be receiving any more callbacks from parent!");
  mEventQ->RunOrEnqueue(new TransportAndDataEvent(this, aChannelStatus,
                                                  aTransportStatus, aData,
                                                  aOffset, aCount),
                        mDivertingToParent);
}

class MaybeDivertOnDataHttpEvent : public ChannelEvent
{
 public:
  MaybeDivertOnDataHttpEvent(HttpChannelChild* child,
                             const nsCString& data,
                             const uint64_t& offset,
                             const uint32_t& count)
  : mChild(child)
  , mData(data)
  , mOffset(offset)
  , mCount(count) {}

  void Run()
  {
    mChild->MaybeDivertOnData(mData, mOffset, mCount);
  }

  already_AddRefed<nsIEventTarget> GetEventTarget()
  {
    MOZ_ASSERT(mChild);
    nsCOMPtr<nsIEventTarget> target = mChild->GetNeckoTarget();
    return target.forget();
  }
 private:
  HttpChannelChild* mChild;
  nsCString mData;
  uint64_t mOffset;
  uint32_t mCount;
};

void
HttpChannelChild::MaybeDivertOnData(const nsCString& data,
                                    const uint64_t& offset,
                                    const uint32_t& count)
{
  LOG(("HttpChannelChild::MaybeDivertOnData [this=%p]", this));

  if (mDivertingToParent) {
    SendDivertOnDataAvailable(data, offset, count);
  }
}

void
HttpChannelChild::OnTransportAndData(const nsresult& channelStatus,
                                     const nsresult& transportStatus,
                                     const uint64_t& offset,
                                     const uint32_t& count,
                                     const nsCString& data)
{
  LOG(("HttpChannelChild::OnTransportAndData [this=%p]\n", this));

  if (!mCanceled && NS_SUCCEEDED(mStatus)) {
    mStatus = channelStatus;
  }

  // For diversion to parent, just SendDivertOnDataAvailable.
  if (mDivertingToParent) {
    MOZ_RELEASE_ASSERT(!mFlushedForDiversion,
      "Should not be processing any more callbacks from parent!");

    SendDivertOnDataAvailable(data, offset, count);
    return;
  }

  if (mCanceled)
    return;

  if (mUnknownDecoderInvolved) {
    LOG(("UnknownDecoder is involved queue OnDataAvailable call. [this=%p]",
         this));
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

    DebugOnly<nsresult> rv =
      neckoTarget->Dispatch(
        NS_NewRunnableFunction([self, transportStatus, progress, progressMax]() {
          self->DoOnStatus(self, transportStatus);
          self->DoOnProgress(self, progress, progressMax);
        }), NS_DISPATCH_NORMAL);
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
  nsresult rv = NS_NewByteInputStream(getter_AddRefs(stringStream), data.get(),
                                      count, NS_ASSIGNMENT_DEPEND);
  if (NS_FAILED(rv)) {
    Cancel(rv);
    return;
  }

  DoOnDataAvailable(this, mListenerContext, stringStream, offset, count);
  stringStream->Close();
}

void
HttpChannelChild::DoOnStatus(nsIRequest* aRequest, nsresult status)
{
  LOG(("HttpChannelChild::DoOnStatus [this=%p]\n", this));
  MOZ_ASSERT(NS_IsMainThread());

  if (mCanceled)
    return;

  // cache the progress sink so we don't have to query for it each time.
  if (!mProgressSink)
    GetCallback(mProgressSink);

  // Temporary fix for bug 1116124
  // See 1124971 - Child removes LOAD_BACKGROUND flag from channel
  if (status == NS_OK)
    return;

  // block status/progress after Cancel or OnStopRequest has been called,
  // or if channel has LOAD_BACKGROUND set.
  if (mProgressSink && NS_SUCCEEDED(mStatus) && mIsPending &&
      !(mLoadFlags & LOAD_BACKGROUND))
  {
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

void
HttpChannelChild::DoOnProgress(nsIRequest* aRequest, int64_t progress, int64_t progressMax)
{
  LOG(("HttpChannelChild::DoOnProgress [this=%p]\n", this));
  MOZ_ASSERT(NS_IsMainThread());

  if (mCanceled)
    return;

  // cache the progress sink so we don't have to query for it each time.
  if (!mProgressSink)
    GetCallback(mProgressSink);

  // block status/progress after Cancel or OnStopRequest has been called,
  // or if channel has LOAD_BACKGROUND set.
  if (mProgressSink && NS_SUCCEEDED(mStatus) && mIsPending &&
      !(mLoadFlags & LOAD_BACKGROUND))
  {
    // OnProgress
    //
    if (progress > 0) {
      mProgressSink->OnProgress(aRequest, nullptr, progress, progressMax);
    }
  }
}

void
HttpChannelChild::DoOnDataAvailable(nsIRequest* aRequest, nsISupports* aContext,
                                    nsIInputStream* aStream,
                                    uint64_t offset, uint32_t count)
{
  LOG(("HttpChannelChild::DoOnDataAvailable [this=%p]\n", this));
  if (mCanceled)
    return;

  nsresult rv = mListener->OnDataAvailable(aRequest, aContext, aStream, offset, count);
  if (NS_FAILED(rv)) {
    CancelOnMainThread(rv);
  }
}

class StopRequestEvent : public ChannelEvent
{
 public:
  StopRequestEvent(HttpChannelChild* child,
                   const nsresult& channelStatus,
                   const ResourceTimingStruct& timing)
  : mChild(child)
  , mChannelStatus(channelStatus)
  , mTiming(timing) {}

  void Run() { mChild->OnStopRequest(mChannelStatus, mTiming); }

  already_AddRefed<nsIEventTarget> GetEventTarget()
  {
    MOZ_ASSERT(mChild);
    nsCOMPtr<nsIEventTarget> target = mChild->GetNeckoTarget();
    return target.forget();
  }
 private:
  HttpChannelChild* mChild;
  nsresult mChannelStatus;
  ResourceTimingStruct mTiming;
};

void
HttpChannelChild::ProcessOnStopRequest(const nsresult& aChannelStatus,
                                       const ResourceTimingStruct& aTiming)
{
  LOG(("HttpChannelChild::ProcessOnStopRequest [this=%p]\n", this));
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(!mFlushedForDiversion,
    "Should not be receiving any more callbacks from parent!");

  mEventQ->RunOrEnqueue(new StopRequestEvent(this, aChannelStatus, aTiming),
                        mDivertingToParent);
}

class MaybeDivertOnStopHttpEvent : public ChannelEvent
{
 public:
  MaybeDivertOnStopHttpEvent(HttpChannelChild* child,
                             const nsresult& channelStatus)
  : mChild(child)
  , mChannelStatus(channelStatus)
  {}

  void Run()
  {
    mChild->MaybeDivertOnStop(mChannelStatus);
  }

  already_AddRefed<nsIEventTarget> GetEventTarget()
  {
    MOZ_ASSERT(mChild);
    nsCOMPtr<nsIEventTarget> target = mChild->GetNeckoTarget();
    return target.forget();
  }
 private:
  HttpChannelChild* mChild;
  nsresult mChannelStatus;
};

void
HttpChannelChild::MaybeDivertOnStop(const nsresult& aChannelStatus)
{
  LOG(("HttpChannelChild::MaybeDivertOnStop [this=%p, "
       "mDivertingToParent=%d status=%" PRIx32 "]", this, mDivertingToParent,
       static_cast<uint32_t>(aChannelStatus)));
  if (mDivertingToParent) {
    SendDivertOnStopRequest(aChannelStatus);
  }
}

void
HttpChannelChild::OnStopRequest(const nsresult& channelStatus,
                                const ResourceTimingStruct& timing)
{
  LOG(("HttpChannelChild::OnStopRequest [this=%p status=%" PRIx32 "]\n",
       this, static_cast<uint32_t>(channelStatus)));
  MOZ_ASSERT(NS_IsMainThread());

  if (mDivertingToParent) {
    MOZ_RELEASE_ASSERT(!mFlushedForDiversion,
      "Should not be processing any more callbacks from parent!");

    SendDivertOnStopRequest(channelStatus);
    return;
  }

  // In thread retargeting is enabled, there might be Runnable for
  // DoOnStatus/DoOnProgress sit in the main thread event target. We need to
  // ensure OnStopRequest is fired after that by postponing the
  // ChannelEventQueue processing to the end of main thread event target.
  // This workaround can be removed after bug 1338493 is complete.
  if (mODATarget) {
    {
      MutexAutoLock lock(mEventTargetMutex);
      mODATarget = nullptr;
    }
    mEventQ->Suspend();
    UniquePtr<ChannelEvent> stopEvent =
      MakeUnique<StopRequestEvent>(this, channelStatus, timing);
    mEventQ->PrependEvent(stopEvent);

    nsCOMPtr<nsIEventTarget> neckoTarget = GetNeckoTarget();
    MOZ_ASSERT(neckoTarget);

    DebugOnly<nsresult> rv = neckoTarget->Dispatch(
      NewRunnableMethod(mEventQ, &ChannelEventQueue::Resume), NS_DISPATCH_NORMAL);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    return;
  }

  if (mUnknownDecoderInvolved) {
   LOG(("UnknownDecoder is involved queue OnStopRequest call. [this=%p]",
        this));
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

  Performance* documentPerformance = GetPerformance();
  if (documentPerformance) {
      documentPerformance->AddEntry(this, this);
  }

  DoPreOnStopRequest(channelStatus);

  { // We must flush the queue before we Send__delete__
    // (although we really shouldn't receive any msgs after OnStop),
    // so make sure this goes out of scope before then.
    AutoEventEnqueuer ensureSerialDispatch(mEventQ);

    DoOnStopRequest(this, channelStatus, mListenerContext);
    // DoOnStopRequest() calls ReleaseListeners()
  }

  CleanupBackgroundChannel();

  // DocumentChannelCleanup actually nulls out mCacheEntry in the parent, which
  // we might need later to open the Alt-Data output stream, so just return here
  if (!mPreferredCachedAltDataType.IsEmpty()) {
    mKeptAlive = true;
    return;
  }

  if (mLoadFlags & LOAD_DOCUMENT_URI) {
    // Keep IPDL channel open, but only for updating security info.
    mKeptAlive = true;
    SendDocumentChannelCleanup();
  } else {
    // The parent process will respond by sending a DeleteSelf message and
    // making sure not to send any more messages after that.
    TrySendDeletingChannel();
  }
}

void
HttpChannelChild::DoPreOnStopRequest(nsresult aStatus)
{
  LOG(("HttpChannelChild::DoPreOnStopRequest [this=%p status=%" PRIx32 "]\n",
       this, static_cast<uint32_t>(aStatus)));
  mIsPending = false;

  if (!mCanceled && NS_SUCCEEDED(mStatus)) {
    mStatus = aStatus;
  }
}

void
HttpChannelChild::DoOnStopRequest(nsIRequest* aRequest, nsresult aChannelStatus, nsISupports* aContext)
{
  LOG(("HttpChannelChild::DoOnStopRequest [this=%p]\n", this));
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mIsPending);

  // NB: We use aChannelStatus here instead of mStatus because if there was an
  // nsCORSListenerProxy on this request, it will override the tracking
  // protection's return value.
  if (aChannelStatus == NS_ERROR_TRACKING_URI ||
      aChannelStatus == NS_ERROR_MALWARE_URI ||
      aChannelStatus == NS_ERROR_UNWANTED_URI ||
      aChannelStatus == NS_ERROR_BLOCKED_URI ||
      aChannelStatus == NS_ERROR_PHISHING_URI) {
    nsCString list, provider, prefix;

    nsresult rv = GetMatchedList(list);
    NS_ENSURE_SUCCESS_VOID(rv);

    rv = GetMatchedProvider(provider);
    NS_ENSURE_SUCCESS_VOID(rv);

    rv = GetMatchedPrefix(prefix);
    NS_ENSURE_SUCCESS_VOID(rv);

    nsChannelClassifier::SetBlockedContent(this, aChannelStatus, list, provider, prefix);
  }

  MOZ_ASSERT(!mOnStopRequestCalled,
             "We should not call OnStopRequest twice");

  // In theory mListener should not be null, but in practice sometimes it is.
  MOZ_ASSERT(mListener);
  if (mListener) {
    mListener->OnStopRequest(aRequest, aContext, mStatus);
  }
  mOnStopRequestCalled = true;

  ReleaseListeners();
  mCacheEntryAvailable = false;
  if (mLoadGroup)
    mLoadGroup->RemoveRequest(this, nullptr, mStatus);
}

class ProgressEvent : public ChannelEvent
{
 public:
  ProgressEvent(HttpChannelChild* child,
                const int64_t& progress,
                const int64_t& progressMax)
  : mChild(child)
  , mProgress(progress)
  , mProgressMax(progressMax) {}

  void Run() { mChild->OnProgress(mProgress, mProgressMax); }

  already_AddRefed<nsIEventTarget> GetEventTarget()
  {
    MOZ_ASSERT(mChild);
    nsCOMPtr<nsIEventTarget> target = mChild->GetNeckoTarget();
    return target.forget();
  }
 private:
  HttpChannelChild* mChild;
  int64_t mProgress, mProgressMax;
};

void
HttpChannelChild::ProcessOnProgress(const int64_t& aProgress,
                                    const int64_t& aProgressMax)
{
  LOG(("HttpChannelChild::ProcessOnProgress [this=%p]\n", this));
  MOZ_ASSERT(NS_IsMainThread());
  mEventQ->RunOrEnqueue(new ProgressEvent(this, aProgress, aProgressMax));
}

void
HttpChannelChild::OnProgress(const int64_t& progress,
                             const int64_t& progressMax)
{
  LOG(("HttpChannelChild::OnProgress [this=%p progress=%" PRId64 "/%" PRId64 "]\n",
       this, progress, progressMax));

  if (mCanceled)
    return;

  // cache the progress sink so we don't have to query for it each time.
  if (!mProgressSink) {
    GetCallback(mProgressSink);
  }

  AutoEventEnqueuer ensureSerialDispatch(mEventQ);

  // Block socket status event after Cancel or OnStopRequest has been called.
  if (mProgressSink && NS_SUCCEEDED(mStatus) && mIsPending)
  {
    if (progress > 0) {
      mProgressSink->OnProgress(this, nullptr, progress, progressMax);
    }
  }
}

class StatusEvent : public ChannelEvent
{
 public:
  StatusEvent(HttpChannelChild* child,
              const nsresult& status)
  : mChild(child)
  , mStatus(status) {}

  void Run() { mChild->OnStatus(mStatus); }

  already_AddRefed<nsIEventTarget> GetEventTarget()
  {
    MOZ_ASSERT(mChild);
    nsCOMPtr<nsIEventTarget> target = mChild->GetNeckoTarget();
    return target.forget();
  }
 private:
  HttpChannelChild* mChild;
  nsresult mStatus;
};

void
HttpChannelChild::ProcessOnStatus(const nsresult& aStatus)
{
  LOG(("HttpChannelChild::ProcessOnStatus [this=%p]\n", this));
  MOZ_ASSERT(NS_IsMainThread());
  mEventQ->RunOrEnqueue(new StatusEvent(this, aStatus));
}

void
HttpChannelChild::OnStatus(const nsresult& status)
{
  LOG(("HttpChannelChild::OnStatus [this=%p status=%" PRIx32 "]\n",
       this, static_cast<uint32_t>(status)));

  if (mCanceled)
    return;

  // cache the progress sink so we don't have to query for it each time.
  if (!mProgressSink)
    GetCallback(mProgressSink);

  AutoEventEnqueuer ensureSerialDispatch(mEventQ);

  // block socket status event after Cancel or OnStopRequest has been called,
  // or if channel has LOAD_BACKGROUND set
  if (mProgressSink && NS_SUCCEEDED(mStatus) && mIsPending &&
      !(mLoadFlags & LOAD_BACKGROUND))
  {
    nsAutoCString host;
    mURI->GetHost(host);
    mProgressSink->OnStatus(this, nullptr, status,
                            NS_ConvertUTF8toUTF16(host).get());
  }
}

class FailedAsyncOpenEvent : public ChannelEvent
{
 public:
  FailedAsyncOpenEvent(HttpChannelChild* child, const nsresult& status)
  : mChild(child)
  , mStatus(status) {}

  void Run() { mChild->FailedAsyncOpen(mStatus); }

  already_AddRefed<nsIEventTarget> GetEventTarget()
  {
    MOZ_ASSERT(mChild);
    nsCOMPtr<nsIEventTarget> target = mChild->GetNeckoTarget();
    return target.forget();
  }
 private:
  HttpChannelChild* mChild;
  nsresult mStatus;
};

mozilla::ipc::IPCResult
HttpChannelChild::RecvFailedAsyncOpen(const nsresult& status)
{
  LOG(("HttpChannelChild::RecvFailedAsyncOpen [this=%p]\n", this));
  mEventQ->RunOrEnqueue(new FailedAsyncOpenEvent(this, status));
  return IPC_OK();
}

// We need to have an implementation of this function just so that we can keep
// all references to mCallOnResume of type HttpChannelChild:  it's not OK in C++
// to set a member function ptr to a base class function.
void
HttpChannelChild::HandleAsyncAbort()
{
  HttpAsyncAborter<HttpChannelChild>::HandleAsyncAbort();

  // Ignore all the messages from background channel after channel aborted.
  CleanupBackgroundChannel();
}

void
HttpChannelChild::FailedAsyncOpen(const nsresult& status)
{
  LOG(("HttpChannelChild::FailedAsyncOpen [this=%p status=%" PRIx32 "]\n",
       this, static_cast<uint32_t>(status)));
  MOZ_ASSERT(NS_IsMainThread());

  // Might be called twice in race condition in theory.
  // (one by RecvFailedAsyncOpen, another by
  // HttpBackgroundChannelChild::ActorFailed)
  if (NS_WARN_IF(NS_FAILED(mStatus))) {
    return;
  }

  mStatus = status;

  // We're already being called from IPDL, therefore already "async"
  HandleAsyncAbort();

  if (mIPCOpen) {
    TrySendDeletingChannel();
  }
}

void
HttpChannelChild::CleanupBackgroundChannel()
{
  LOG(("HttpChannelChild::CleanupBackgroundChannel [this=%p bgChild=%p]\n",
       this, mBgChild.get()));
  if (!mBgChild) {
    return;
  }

  RefPtr<HttpBackgroundChannelChild> bgChild = mBgChild.forget();

  if (!NS_IsMainThread()) {
    SystemGroup::Dispatch(
      "HttpChannelChild::CleanupBackgroundChannel",
      TaskCategory::Other,
      NewRunnableMethod(bgChild, &HttpBackgroundChannelChild::OnChannelClosed));
  } else {
    bgChild->OnChannelClosed();
  }
}

void
HttpChannelChild::DoNotifyListenerCleanup()
{
  LOG(("HttpChannelChild::DoNotifyListenerCleanup [this=%p]\n", this));

  if (mInterceptListener) {
    mInterceptListener->Cleanup();
    mInterceptListener = nullptr;
  }
}

class DeleteSelfEvent : public ChannelEvent
{
 public:
  explicit DeleteSelfEvent(HttpChannelChild* child) : mChild(child) {}
  void Run() { mChild->DeleteSelf(); }

  already_AddRefed<nsIEventTarget> GetEventTarget()
  {
    MOZ_ASSERT(mChild);
    nsCOMPtr<nsIEventTarget> target = mChild->GetNeckoTarget();
    return target.forget();
  }
 private:
  HttpChannelChild* mChild;
};

mozilla::ipc::IPCResult
HttpChannelChild::RecvDeleteSelf()
{
  LOG(("HttpChannelChild::RecvDeleteSelf [this=%p]\n", this));
  mEventQ->RunOrEnqueue(new DeleteSelfEvent(this));
  return IPC_OK();
}

HttpChannelChild::OverrideRunnable::OverrideRunnable(HttpChannelChild* aChannel,
                                                     HttpChannelChild* aNewChannel,
                                                     InterceptStreamListener* aListener,
                                                     nsIInputStream* aInput,
                                                     nsAutoPtr<nsHttpResponseHead>& aHead)
{
  mChannel = aChannel;
  mNewChannel = aNewChannel;
  mListener = aListener;
  mInput = aInput;
  mHead = aHead;
}

void
HttpChannelChild::OverrideRunnable::OverrideWithSynthesizedResponse()
{
  if (mNewChannel) {
    mNewChannel->OverrideWithSynthesizedResponse(mHead, mInput, mListener);
  }
}

NS_IMETHODIMP
HttpChannelChild::OverrideRunnable::Run()
{
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

mozilla::ipc::IPCResult
HttpChannelChild::RecvFinishInterceptedRedirect()
{
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
    NewRunnableMethod(this, &HttpChannelChild::FinishInterceptedRedirect),
                      NS_DISPATCH_NORMAL);

  return IPC_OK();
}

void
HttpChannelChild::DeleteSelf()
{
  Send__delete__(this);
}

void HttpChannelChild::FinishInterceptedRedirect()
{
  nsresult rv;
  if (mLoadInfo && mLoadInfo->GetEnforceSecurity()) {
    MOZ_ASSERT(!mInterceptedRedirectContext, "the context should be null!");
    rv = AsyncOpen2(mInterceptedRedirectListener);
  } else {
    rv = AsyncOpen(mInterceptedRedirectListener, mInterceptedRedirectContext);
  }
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

mozilla::ipc::IPCResult
HttpChannelChild::RecvReportSecurityMessage(const nsString& messageTag,
                                            const nsString& messageCategory)
{
  DebugOnly<nsresult> rv = AddSecurityMessage(messageTag, messageCategory);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  return IPC_OK();
}

class Redirect1Event : public ChannelEvent
{
 public:
  Redirect1Event(HttpChannelChild* child,
                 const uint32_t& registrarId,
                 const URIParams& newURI,
                 const uint32_t& redirectFlags,
                 const nsHttpResponseHead& responseHead,
                 const nsACString& securityInfoSerialization,
                 const uint64_t& channelId)
  : mChild(child)
  , mRegistrarId(registrarId)
  , mNewURI(newURI)
  , mRedirectFlags(redirectFlags)
  , mResponseHead(responseHead)
  , mSecurityInfoSerialization(securityInfoSerialization)
  , mChannelId(channelId) {}

  void Run()
  {
    mChild->Redirect1Begin(mRegistrarId, mNewURI, mRedirectFlags,
                           mResponseHead, mSecurityInfoSerialization,
                           mChannelId);
  }

  already_AddRefed<nsIEventTarget> GetEventTarget()
  {
    MOZ_ASSERT(mChild);
    nsCOMPtr<nsIEventTarget> target = mChild->GetNeckoTarget();
    return target.forget();
  }
 private:
  HttpChannelChild*   mChild;
  uint32_t            mRegistrarId;
  URIParams           mNewURI;
  uint32_t            mRedirectFlags;
  nsHttpResponseHead  mResponseHead;
  nsCString           mSecurityInfoSerialization;
  uint64_t            mChannelId;
};

mozilla::ipc::IPCResult
HttpChannelChild::RecvRedirect1Begin(const uint32_t& registrarId,
                                     const URIParams& newUri,
                                     const uint32_t& redirectFlags,
                                     const nsHttpResponseHead& responseHead,
                                     const nsCString& securityInfoSerialization,
                                     const uint64_t& channelId,
                                     const NetAddr& oldPeerAddr)
{
  // TODO: handle security info
  LOG(("HttpChannelChild::RecvRedirect1Begin [this=%p]\n", this));
  // We set peer address of child to the old peer,
  // Then it will be updated to new peer in OnStartRequest
  mPeerAddr = oldPeerAddr;

  mEventQ->RunOrEnqueue(new Redirect1Event(this, registrarId, newUri,
                                           redirectFlags, responseHead,
                                           securityInfoSerialization,
                                           channelId));
  return IPC_OK();
}

nsresult
HttpChannelChild::SetupRedirect(nsIURI* uri,
                                const nsHttpResponseHead* responseHead,
                                const uint32_t& redirectFlags,
                                nsIChannel** outChannel)
{
  LOG(("HttpChannelChild::SetupRedirect [this=%p]\n", this));

  nsresult rv;
  nsCOMPtr<nsIIOService> ioService;
  rv = gHttpHandler->GetIOService(getter_AddRefs(ioService));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIChannel> newChannel;
  rv = NS_NewChannelInternal(getter_AddRefs(newChannel),
                             uri,
                             mLoadInfo,
                             nullptr, // aLoadGroup
                             nullptr, // aCallbacks
                             nsIRequest::LOAD_NORMAL,
                             ioService);
  NS_ENSURE_SUCCESS(rv, rv);

  // We won't get OnStartRequest, set cookies here.
  mResponseHead = new nsHttpResponseHead(*responseHead);

  bool rewriteToGET = HttpBaseChannel::ShouldRewriteRedirectToGET(mResponseHead->Status(),
                                                                  mRequestHead.ParsedMethod());

  rv = SetupReplacementChannel(uri, newChannel, !rewriteToGET, redirectFlags);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHttpChannelChild> httpChannelChild = do_QueryInterface(newChannel);
  if (httpChannelChild) {
    bool shouldUpgrade = false;
    auto channelChild = static_cast<HttpChannelChild*>(httpChannelChild.get());
    if (mShouldInterceptSubsequentRedirect) {
      // In the case where there was a synthesized response that caused a redirection,
      // we must force the new channel to intercept the request in the parent before a
      // network transaction is initiated.
      rv = httpChannelChild->ForceIntercepted(false, false);
    } else if (mRedirectMode == nsIHttpChannelInternal::REDIRECT_MODE_MANUAL &&
               ((redirectFlags & (nsIChannelEventSink::REDIRECT_TEMPORARY |
                                  nsIChannelEventSink::REDIRECT_PERMANENT)) != 0) &&
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

void
HttpChannelChild::Redirect1Begin(const uint32_t& registrarId,
                                 const URIParams& newUri,
                                 const uint32_t& redirectFlags,
                                 const nsHttpResponseHead& responseHead,
                                 const nsACString& securityInfoSerialization,
                                 const uint64_t& channelId)
{
  LOG(("HttpChannelChild::Redirect1Begin [this=%p]\n", this));

  nsCOMPtr<nsIURI> uri = DeserializeURI(newUri);

  if (!securityInfoSerialization.IsEmpty()) {
    NS_DeserializeObject(securityInfoSerialization,
                         getter_AddRefs(mSecurityInfo));
  }

  nsCOMPtr<nsIChannel> newChannel;
  nsresult rv = SetupRedirect(uri,
                              &responseHead,
                              redirectFlags,
                              getter_AddRefs(newChannel));

  if (NS_SUCCEEDED(rv)) {
    if (mRedirectChannelChild) {
      // Set the channelId allocated in parent to the child instance
      nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(mRedirectChannelChild);
      if (httpChannel) {
        rv = httpChannel->SetChannelId(channelId);
        MOZ_ASSERT(NS_SUCCEEDED(rv));
      }
      mRedirectChannelChild->ConnectParent(registrarId);
    }
    rv = gHttpHandler->AsyncOnChannelRedirect(this, newChannel, redirectFlags);
  }

  if (NS_FAILED(rv))
    OnRedirectVerifyCallback(rv);
}

void
HttpChannelChild::BeginNonIPCRedirect(nsIURI* responseURI,
                                      const nsHttpResponseHead* responseHead)
{
  LOG(("HttpChannelChild::BeginNonIPCRedirect [this=%p]\n", this));

  nsCOMPtr<nsIChannel> newChannel;
  nsresult rv = SetupRedirect(responseURI,
                              responseHead,
                              nsIChannelEventSink::REDIRECT_INTERNAL,
                              getter_AddRefs(newChannel));

  if (NS_SUCCEEDED(rv)) {
    // Ensure that the new channel shares the original channel's security information,
    // since it won't be provided via IPC. In particular, if the target of this redirect
    // is a synthesized response that has its own security info, the pre-redirect channel
    // has already received it and it must be propagated to the post-redirect channel.
    nsCOMPtr<nsIHttpChannelChild> channelChild = do_QueryInterface(newChannel);
    if (mSecurityInfo && channelChild) {
      HttpChannelChild* httpChannelChild = static_cast<HttpChannelChild*>(channelChild.get());
      httpChannelChild->OverrideSecurityInfoForNonIPCRedirect(mSecurityInfo);
    }

    rv = gHttpHandler->AsyncOnChannelRedirect(this,
                                              newChannel,
                                              nsIChannelEventSink::REDIRECT_INTERNAL);
  }

  if (NS_FAILED(rv))
    OnRedirectVerifyCallback(rv);
}

void
HttpChannelChild::OverrideSecurityInfoForNonIPCRedirect(nsISupports* securityInfo)
{
  mResponseCouldBeSynthesized = true;
  DebugOnly<nsresult> rv = OverrideSecurityInfo(securityInfo);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

class Redirect3Event : public ChannelEvent
{
 public:
  explicit Redirect3Event(HttpChannelChild* child) : mChild(child) {}
  void Run() { mChild->Redirect3Complete(nullptr); }

  already_AddRefed<nsIEventTarget> GetEventTarget()
  {
    MOZ_ASSERT(mChild);
    nsCOMPtr<nsIEventTarget> target = mChild->GetNeckoTarget();
    return target.forget();
  }
 private:
  HttpChannelChild* mChild;
};

mozilla::ipc::IPCResult
HttpChannelChild::RecvRedirect3Complete()
{
  LOG(("HttpChannelChild::RecvRedirect3Complete [this=%p]\n", this));
  mEventQ->RunOrEnqueue(new Redirect3Event(this));
  return IPC_OK();
}

class HttpFlushedForDiversionEvent : public ChannelEvent
{
 public:
  explicit HttpFlushedForDiversionEvent(HttpChannelChild* aChild)
  : mChild(aChild)
  {
    MOZ_RELEASE_ASSERT(aChild);
  }

  void Run()
  {
    mChild->FlushedForDiversion();
  }

  already_AddRefed<nsIEventTarget> GetEventTarget()
  {
    MOZ_ASSERT(mChild);
    nsCOMPtr<nsIEventTarget> target = mChild->GetNeckoTarget();
    return target.forget();
  }
 private:
  HttpChannelChild* mChild;
};

void
HttpChannelChild::ProcessFlushedForDiversion()
{
  LOG(("HttpChannelChild::ProcessFlushedForDiversion [this=%p]\n", this));
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(mDivertingToParent);

  mEventQ->RunOrEnqueue(new HttpFlushedForDiversionEvent(this), true);
}

void
HttpChannelChild::ProcessNotifyTrackingProtectionDisabled()
{
  LOG(("HttpChannelChild::ProcessNotifyTrackingProtectionDisabled [this=%p]\n", this));
  MOZ_ASSERT(NS_IsMainThread());

  nsChannelClassifier::NotifyTrackingProtectionDisabled(this);
}

void
HttpChannelChild::ProcessNotifyTrackingResource()
{
  LOG(("HttpChannelChild::ProcessNotifyTrackingResource [this=%p]\n", this));
  MOZ_ASSERT(NS_IsMainThread());

  SetIsTrackingResource();
}

void
HttpChannelChild::FlushedForDiversion()
{
  LOG(("HttpChannelChild::FlushedForDiversion [this=%p]\n", this));
  MOZ_RELEASE_ASSERT(mDivertingToParent);

  // Once this is set, it should not be unset before HttpChannelChild is taken
  // down. After it is set, no OnStart/OnData/OnStop callbacks should be
  // received from the parent channel, nor dequeued from the ChannelEventQueue.
  mFlushedForDiversion = true;

  SendDivertComplete();
}

void
HttpChannelChild::ProcessSetClassifierMatchedInfo(const nsCString& aList,
                                                  const nsCString& aProvider,
                                                  const nsCString& aPrefix)
{
  LOG(("HttpChannelChild::ProcessSetClassifierMatchedInfo [this=%p]\n", this));
  MOZ_ASSERT(NS_IsMainThread());

  SetMatchedInfo(aList, aProvider, aPrefix);
}

void
HttpChannelChild::ProcessDivertMessages()
{
  LOG(("HttpChannelChild::ProcessDivertMessages [this=%p]\n", this));
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(mDivertingToParent);
  MOZ_RELEASE_ASSERT(mSuspendCount > 0);

  // DivertTo() has been called on parent, so we can now start sending queued
  // IPDL messages back to parent listener.
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(Resume()));
}

// Returns true if has actually completed the redirect and cleaned up the
// channel, or false the interception logic kicked in and we need to asyncly
// call FinishInterceptedRedirect and CleanupRedirectingChannel.
// The argument is an optional OverrideRunnable that we pass to the redirected
// channel.
bool
HttpChannelChild::Redirect3Complete(OverrideRunnable* aRunnable)
{
  LOG(("HttpChannelChild::Redirect3Complete [this=%p]\n", this));
  nsresult rv = NS_OK;

  nsCOMPtr<nsIHttpChannelChild> chan = do_QueryInterface(mRedirectChannelChild);
  RefPtr<HttpChannelChild> httpChannelChild = static_cast<HttpChannelChild*>(chan.get());
  // Chrome channel has been AsyncOpen'd.  Reflect this in child.
  if (mRedirectChannelChild) {
    if (httpChannelChild) {
      httpChannelChild->mOverrideRunnable = aRunnable;
      httpChannelChild->mInterceptingChannel = this;
    }
    rv = mRedirectChannelChild->CompleteRedirectSetup(mListener,
                                                      mListenerContext);
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

void
HttpChannelChild::CleanupRedirectingChannel(nsresult rv)
{
  // Redirecting to new channel: shut this down and init new channel
  if (mLoadGroup)
    mLoadGroup->RemoveRequest(this, nullptr, NS_BINDING_ABORTED);

  if (NS_SUCCEEDED(rv)) {
    if (mLoadInfo) {
      nsCString remoteAddress;
      Unused << GetRemoteAddress(remoteAddress);
      nsCOMPtr<nsIRedirectHistoryEntry> entry =
        new nsRedirectHistoryEntry(GetURIPrincipal(), mReferrer, remoteAddress);

      mLoadInfo->AppendRedirectHistoryEntry(entry, false);
    }
  }
  else {
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
HttpChannelChild::ConnectParent(uint32_t registrarId)
{
  LOG(("HttpChannelChild::ConnectParent [this=%p]\n", this));
  mozilla::dom::TabChild* tabChild = nullptr;
  nsCOMPtr<nsITabChild> iTabChild;
  GetCallback(iTabChild);
  if (iTabChild) {
    tabChild = static_cast<mozilla::dom::TabChild*>(iTabChild.get());
  }
  if (MissingRequiredTabChild(tabChild, "http")) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  if (tabChild && !tabChild->IPCOpen()) {
    return NS_ERROR_FAILURE;
  }

  ContentChild* cc = static_cast<ContentChild*>(gNeckoChild->Manager());
  if (cc->IsShuttingDown()) {
    return NS_ERROR_FAILURE;
  }

  HttpBaseChannel::SetDocshellUserAgentOverride();

  // The socket transport in the chrome process now holds a logical ref to us
  // until OnStopRequest, or we do a redirect, or we hit an IPDL error.
  AddIPDLReference();

  // This must happen before the constructor message is sent. Otherwise messages
  // from the parent could arrive quickly and be delivered to the wrong event
  // target.
  SetEventTarget();

  HttpChannelConnectArgs connectArgs(registrarId, mShouldParentIntercept);
  PBrowserOrId browser = static_cast<ContentChild*>(gNeckoChild->Manager())
                         ->GetBrowserOrId(tabChild);
  if (!gNeckoChild->
        SendPHttpChannelConstructor(this, browser,
                                    IPC::SerializedLoadContext(this),
                                    connectArgs)) {
    return NS_ERROR_FAILURE;
  }

  {
    MOZ_ASSERT(!mBgChild);

    RefPtr<HttpBackgroundChannelChild> bgChild =
      new HttpBackgroundChannelChild();

    nsresult rv = bgChild->Init(this);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    mBgChild = bgChild.forget();
  }

  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::CompleteRedirectSetup(nsIStreamListener *listener,
                                        nsISupports *aContext)
{
  LOG(("HttpChannelChild::FinishRedirectSetup [this=%p]\n", this));

  NS_ENSURE_TRUE(!mIsPending, NS_ERROR_IN_PROGRESS);
  NS_ENSURE_TRUE(!mWasOpened, NS_ERROR_ALREADY_OPENED);

  if (mShouldParentIntercept) {
    // This is a redirected channel, and the corresponding parent channel has started
    // AsyncOpen but was intercepted and suspended. We must tear it down and start
    // fresh - we will intercept the child channel this time, before creating a new
    // parent channel unnecessarily.

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

  mIsPending = true;
  mWasOpened = true;
  mListener = listener;
  mListenerContext = aContext;

  // add ourselves to the load group.
  if (mLoadGroup)
    mLoadGroup->AddRequest(this, nullptr);

  // We already have an open IPDL connection to the parent. If on-modify-request
  // listeners or load group observers canceled us, let the parent handle it
  // and send it back to us naturally.
  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIAsyncVerifyRedirectCallback
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::OnRedirectVerifyCallback(nsresult result)
{
  LOG(("HttpChannelChild::OnRedirectVerifyCallback [this=%p]\n", this));
  nsresult rv;
  OptionalURIParams redirectURI;

  uint32_t referrerPolicy = REFERRER_POLICY_UNSET;
  OptionalURIParams referrerURI;
  SerializeURI(nullptr, referrerURI);

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

  bool forceHSTSPriming = false;
  bool mixedContentWouldBlock = false;
  if (newHttpChannel) {
    // Must not be called until after redirect observers called.
    newHttpChannel->SetOriginalURI(mOriginalURI);

    nsCOMPtr<nsILoadInfo> newLoadInfo;
    rv = newHttpChannel->GetLoadInfo(getter_AddRefs(newLoadInfo));
    if (NS_SUCCEEDED(rv) && newLoadInfo) {
      forceHSTSPriming = newLoadInfo->GetForceHSTSPriming();
      mixedContentWouldBlock = newLoadInfo->GetMixedContentWouldBlock();
    }

    rv = newHttpChannel->GetReferrerPolicy(&referrerPolicy);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    nsCOMPtr<nsIURI> newChannelReferrerURI;
    rv = newHttpChannel->GetReferrer(getter_AddRefs(newChannelReferrerURI));
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    SerializeURI(newChannelReferrerURI, referrerURI);
  }

  if (mRedirectingForSubsequentSynthesizedResponse) {
    nsCOMPtr<nsIHttpChannelChild> httpChannelChild = do_QueryInterface(mRedirectChannelChild);
    RefPtr<HttpChannelChild> redirectedChannel =
        static_cast<HttpChannelChild*>(httpChannelChild.get());
    // redirectChannel will be NULL if mRedirectChannelChild isn't a
    // nsIHttpChannelChild (it could be a DataChannelChild).

    RefPtr<InterceptStreamListener> streamListener =
        new InterceptStreamListener(redirectedChannel, mListenerContext);

    nsCOMPtr<nsIEventTarget> neckoTarget = GetNeckoTarget();
    MOZ_ASSERT(neckoTarget);

    Unused << neckoTarget->Dispatch(
      new OverrideRunnable(this, redirectedChannel, streamListener,
                           mSynthesizedInput, mResponseHead),
      NS_DISPATCH_NORMAL);

    return NS_OK;
  }

  RequestHeaderTuples emptyHeaders;
  RequestHeaderTuples* headerTuples = &emptyHeaders;
  nsLoadFlags loadFlags = 0;
  OptionalCorsPreflightArgs corsPreflightArgs = mozilla::void_t();

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
    //base->CallOnModifyRequestObservers();

    nsCOMPtr<nsIHttpChannelInternal> newHttpChannelInternal =
      do_QueryInterface(mRedirectChannelChild);
    if (newHttpChannelInternal) {
      nsCOMPtr<nsIURI> apiRedirectURI;
      nsresult rv = newHttpChannelInternal->GetApiRedirectToURI(
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

  bool chooseAppcache = false;
  nsCOMPtr<nsIApplicationCacheChannel> appCacheChannel =
    do_QueryInterface(newHttpChannel);
  if (appCacheChannel) {
    appCacheChannel->GetChooseApplicationCache(&chooseAppcache);
  }

  if (mIPCOpen)
    SendRedirect2Verify(result, *headerTuples, loadFlags, referrerPolicy,
                        referrerURI, redirectURI, corsPreflightArgs,
                        forceHSTSPriming, mixedContentWouldBlock, chooseAppcache);

  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIRequest
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::Cancel(nsresult status)
{
  LOG(("HttpChannelChild::Cancel [this=%p]\n", this));
  MOZ_ASSERT(NS_IsMainThread());

  if (!mCanceled) {
    // If this cancel occurs before nsHttpChannel has been set up, AsyncOpen
    // is responsible for cleaning up.
    mCanceled = true;
    mStatus = status;
    if (RemoteChannelExists())
      SendCancel(status);
    if (mSynthesizedResponsePump) {
      mSynthesizedResponsePump->Cancel(status);
    }
    mInterceptListener = nullptr;
  }
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::Suspend()
{
  LOG(("HttpChannelChild::Suspend [this=%p, mSuspendCount=%" PRIu32 ", "
       "mDivertingToParent=%d]\n", this, mSuspendCount+1, mDivertingToParent));
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
HttpChannelChild::Resume()
{
  LOG(("HttpChannelChild::Resume [this=%p, mSuspendCount=%" PRIu32 ", "
       "mDivertingToParent=%d]\n", this, mSuspendCount-1, mDivertingToParent));
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
      rv = AsyncCall(mCallOnResume);
      NS_ENSURE_SUCCESS(rv, rv);
      mCallOnResume = nullptr;
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
HttpChannelChild::GetSecurityInfo(nsISupports **aSecurityInfo)
{
  NS_ENSURE_ARG_POINTER(aSecurityInfo);
  NS_IF_ADDREF(*aSecurityInfo = mSecurityInfo);
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::AsyncOpen(nsIStreamListener *listener, nsISupports *aContext)
{
  MOZ_ASSERT(!mLoadInfo ||
             mLoadInfo->GetSecurityMode() == 0 ||
             mLoadInfo->GetInitialSecurityCheckDone() ||
             (mLoadInfo->GetSecurityMode() == nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL &&
              nsContentUtils::IsSystemPrincipal(mLoadInfo->LoadingPrincipal())),
             "security flags in loadInfo but asyncOpen2() not called");

  LOG(("HttpChannelChild::AsyncOpen [this=%p uri=%s]\n", this, mSpec.get()));

#ifdef DEBUG
  AssertPrivateBrowsingId();
#endif

  if (mCanceled)
    return mStatus;

  NS_ENSURE_TRUE(gNeckoChild != nullptr, NS_ERROR_FAILURE);
  NS_ENSURE_ARG_POINTER(listener);
  NS_ENSURE_TRUE(!mIsPending, NS_ERROR_IN_PROGRESS);
  NS_ENSURE_TRUE(!mWasOpened, NS_ERROR_ALREADY_OPENED);

  mAsyncOpenTime = TimeStamp::Now();
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
  nsresult rv;
  rv = NS_CheckPortSafety(mURI);
  if (NS_FAILED(rv))
    return rv;

  nsAutoCString cookie;
  if (NS_SUCCEEDED(mRequestHead.GetHeader(nsHttp::Cookie, cookie))) {
    mUserSetCookieHeader = cookie;
  }

  rv = AddCookiesToRequest();
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  //
  // NOTE: From now on we must return NS_OK; all errors must be handled via
  // OnStart/OnStopRequest
  //

  // We notify "http-on-opening-request" observers in the child
  // process so that devtools can capture a stack trace at the
  // appropriate spot.  See bug 806753 for some information about why
  // other http-* notifications are disabled in child processes.
  gHttpHandler->OnOpeningRequest(this);

  mIsPending = true;
  mWasOpened = true;
  mListener = listener;
  mListenerContext = aContext;

  // add ourselves to the load group.
  if (mLoadGroup)
    mLoadGroup->AddRequest(this, nullptr);

  if (mCanceled) {
    // We may have been canceled already, either by on-modify-request
    // listeners or by load group observers; in that case, don't create IPDL
    // connection. See nsHttpChannel::AsyncOpen().
    Unused << AsyncAbort(mStatus);
    return NS_OK;
  }

  // Set user agent override from docshell
  HttpBaseChannel::SetDocshellUserAgentOverride();

  MOZ_ASSERT_IF(mPostRedirectChannelShouldUpgrade,
                mPostRedirectChannelShouldIntercept);
  bool shouldUpgrade = mPostRedirectChannelShouldUpgrade;
  if (mPostRedirectChannelShouldIntercept ||
      ShouldInterceptURI(mURI, shouldUpgrade)) {
    mResponseCouldBeSynthesized = true;

    nsCOMPtr<nsINetworkInterceptController> controller;
    GetCallback(controller);

    mInterceptListener = new InterceptStreamListener(this, mListenerContext);

    RefPtr<InterceptedChannelContent> intercepted =
        new InterceptedChannelContent(this, controller,
                                      mInterceptListener, shouldUpgrade);
    intercepted->NotifyController();
    return NS_OK;
  }

  return ContinueAsyncOpen();
}

NS_IMETHODIMP
HttpChannelChild::AsyncOpen2(nsIStreamListener *aListener)
{
  nsCOMPtr<nsIStreamListener> listener = aListener;
  nsresult rv = nsContentSecurityManager::doContentSecurityCheck(this, listener);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    ReleaseListeners();
    return rv;
  }
  return AsyncOpen(listener, nullptr);
}

// Assigns an nsIEventTarget to our IPDL actor so that IPC messages are sent to
// the correct DocGroup/TabGroup.
void
HttpChannelChild::SetEventTarget()
{
  nsCOMPtr<nsILoadInfo> loadInfo;
  GetLoadInfo(getter_AddRefs(loadInfo));

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

already_AddRefed<nsIEventTarget>
HttpChannelChild::GetNeckoTarget()
{
  nsCOMPtr<nsIEventTarget> target;
  {
    MutexAutoLock lock(mEventTargetMutex);
    target = mNeckoTarget;
  }

  if (!target) {
    target = do_GetMainThread();
  }
  return target.forget();
}

already_AddRefed<nsIEventTarget>
HttpChannelChild::GetODATarget()
{
  nsCOMPtr<nsIEventTarget> target;
  {
    MutexAutoLock lock(mEventTargetMutex);
    target = mODATarget ? mODATarget : mNeckoTarget;
  }

  if (!target) {
    target = do_GetMainThread();
  }
  return target.forget();
}

nsresult
HttpChannelChild::ContinueAsyncOpen()
{
  nsCString appCacheClientId;
  if (mInheritApplicationCache) {
    // Pick up an application cache from the notification
    // callbacks if available
    nsCOMPtr<nsIApplicationCacheContainer> appCacheContainer;
    GetCallback(appCacheContainer);

    if (appCacheContainer) {
      nsCOMPtr<nsIApplicationCache> appCache;
      nsresult rv = appCacheContainer->GetApplicationCache(getter_AddRefs(appCache));
      if (NS_SUCCEEDED(rv) && appCache) {
        appCache->GetClientID(appCacheClientId);
      }
    }
  }

  //
  // Send request to the chrome process...
  //

  mozilla::dom::TabChild* tabChild = nullptr;
  nsCOMPtr<nsITabChild> iTabChild;
  GetCallback(iTabChild);
  if (iTabChild) {
    tabChild = static_cast<mozilla::dom::TabChild*>(iTabChild.get());
  }
  if (MissingRequiredTabChild(tabChild, "http")) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  // This id identifies the inner window's top-level document,
  // which changes on every new load or navigation.
  uint64_t contentWindowId = 0;
  if (tabChild) {
    MOZ_ASSERT(tabChild->WebNavigation());
    nsCOMPtr<nsIDocument> document = tabChild->GetDocument();
    if (document) {
      contentWindowId = document->InnerWindowID();
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
  SerializeURI(mReferrer, openArgs.referrer());
  openArgs.referrerPolicy() = mReferrerPolicy;
  SerializeURI(mAPIRedirectToURI, openArgs.apiRedirectTo());
  openArgs.loadFlags() = mLoadFlags;
  openArgs.requestHeaders() = mClientSetRequestHeaders;
  mRequestHead.Method(openArgs.requestMethod());
  openArgs.preferredAlternativeType() = mPreferredCachedAltDataType;

  AutoIPCStream autoStream(openArgs.uploadStream());
  if (mUploadStream) {
    autoStream.Serialize(mUploadStream, ContentChild::GetSingleton());
    autoStream.TakeOptionalValue();
  }
  
  if (mResponseHead) {
    openArgs.synthesizedResponseHead() = *mResponseHead;
    openArgs.suspendAfterSynthesizeResponse() =
      mSuspendParentAfterSynthesizeResponse;
  } else {
    openArgs.synthesizedResponseHead() = mozilla::void_t();
    openArgs.suspendAfterSynthesizeResponse() = false;
  }

  nsCOMPtr<nsISerializable> secInfoSer = do_QueryInterface(mSecurityInfo);
  if (secInfoSer) {
    NS_SerializeToString(secInfoSer, openArgs.synthesizedSecurityInfoSerialization());
  }

  OptionalCorsPreflightArgs optionalCorsPreflightArgs;
  GetClientSetCorsPreflightParameters(optionalCorsPreflightArgs);

  // NB: This call forces us to cache mTopWindowURI if we haven't already.
  nsCOMPtr<nsIURI> uri;
  GetTopWindowURI(getter_AddRefs(uri));

  SerializeURI(mTopWindowURI, openArgs.topWindowURI());

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
  openArgs.initialRwin() = mInitialRwin;

  uint32_t cacheKey = 0;
  if (mCacheKey) {
    nsCOMPtr<nsISupportsPRUint32> container = do_QueryInterface(mCacheKey);
    if (!container) {
      return NS_ERROR_ILLEGAL_VALUE;
    }

    nsresult rv = container->GetData(&cacheKey);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  openArgs.cacheKey() = cacheKey;

  openArgs.blockAuthPrompt() = mBlockAuthPrompt;

  openArgs.allowStaleCacheContent() = mAllowStaleCacheContent;

  openArgs.contentTypeHint() = mContentTypeHint;

  nsresult rv = mozilla::ipc::LoadInfoToLoadInfoArgs(mLoadInfo, &openArgs.loadInfo());
  NS_ENSURE_SUCCESS(rv, rv);

  EnsureRequestContextID();
  openArgs.requestContextID() = mRequestContextID;

  openArgs.channelId() = mChannelId;

  openArgs.contentWindowId() = contentWindowId;
  openArgs.topLevelOuterContentWindowId() = mTopLevelOuterContentWindowId;

  if (tabChild && !tabChild->IPCOpen()) {
    return NS_ERROR_FAILURE;
  }

  ContentChild* cc = static_cast<ContentChild*>(gNeckoChild->Manager());
  if (cc->IsShuttingDown()) {
    return NS_ERROR_FAILURE;
  }

  openArgs.launchServiceWorkerStart() = mLaunchServiceWorkerStart;
  openArgs.launchServiceWorkerEnd()   = mLaunchServiceWorkerEnd;
  openArgs.dispatchFetchEventStart()  = mDispatchFetchEventStart;
  openArgs.dispatchFetchEventEnd()    = mDispatchFetchEventEnd;
  openArgs.handleFetchEventStart()    = mHandleFetchEventStart;
  openArgs.handleFetchEventEnd()      = mHandleFetchEventEnd;

  // This must happen before the constructor message is sent. Otherwise messages
  // from the parent could arrive quickly and be delivered to the wrong event
  // target.
  SetEventTarget();

  // The socket transport in the chrome process now holds a logical ref to us
  // until OnStopRequest, or we do a redirect, or we hit an IPDL error.
  AddIPDLReference();

  PBrowserOrId browser = cc->GetBrowserOrId(tabChild);
  if (!gNeckoChild->SendPHttpChannelConstructor(this, browser,
                                                IPC::SerializedLoadContext(this),
                                                openArgs)) {
    return NS_ERROR_FAILURE;
  }

  {
    // Service worker might use the same HttpChannelChild to do async open
    // twice. Need to disconnect with previous background channel before
    // creating the new one.
    if (mBgChild) {
      RefPtr<HttpBackgroundChannelChild> prevBgChild = mBgChild.forget();
      prevBgChild->OnChannelClosed();
    }

    RefPtr<HttpBackgroundChannelChild> bgChild =
      new HttpBackgroundChannelChild();

    rv = bgChild->Init(this);
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
HttpChannelChild::SetReferrerWithPolicy(nsIURI *referrer,
                                       uint32_t referrerPolicy)
{
  ENSURE_CALLED_BEFORE_CONNECT();

  // remove old referrer if any, loop backwards
  for (int i = mClientSetRequestHeaders.Length() - 1; i >= 0; --i) {
    if (NS_LITERAL_CSTRING("Referer").Equals(mClientSetRequestHeaders[i].mHeader)) {
      mClientSetRequestHeaders.RemoveElementAt(i);
    }
  }

  nsresult rv = HttpBaseChannel::SetReferrerWithPolicy(referrer, referrerPolicy);
  if (NS_FAILED(rv))
    return rv;
  return NS_OK;

}
NS_IMETHODIMP
HttpChannelChild::SetRequestHeader(const nsACString& aHeader,
                                   const nsACString& aValue,
                                   bool aMerge)
{
  LOG(("HttpChannelChild::SetRequestHeader [this=%p]\n", this));
  nsresult rv = HttpBaseChannel::SetRequestHeader(aHeader, aValue, aMerge);
  if (NS_FAILED(rv))
    return rv;

  RequestHeaderTuple* tuple = mClientSetRequestHeaders.AppendElement();
  if (!tuple)
    return NS_ERROR_OUT_OF_MEMORY;

  tuple->mHeader = aHeader;
  tuple->mValue = aValue;
  tuple->mMerge = aMerge;
  tuple->mEmpty = false;
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::SetEmptyRequestHeader(const nsACString& aHeader)
{
  LOG(("HttpChannelChild::SetEmptyRequestHeader [this=%p]\n", this));
  nsresult rv = HttpBaseChannel::SetEmptyRequestHeader(aHeader);
  if (NS_FAILED(rv))
    return rv;

  RequestHeaderTuple* tuple = mClientSetRequestHeaders.AppendElement();
  if (!tuple)
    return NS_ERROR_OUT_OF_MEMORY;

  tuple->mHeader = aHeader;
  tuple->mMerge = false;
  tuple->mEmpty = true;
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::RedirectTo(nsIURI *newURI)
{
  // disabled until/unless addons run in child or something else needs this
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
HttpChannelChild::GetProtocolVersion(nsACString& aProtocolVersion)
{
  aProtocolVersion = mProtocolVersion;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIHttpChannelInternal
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::SetupFallbackChannel(const char *aFallbackKey)
{
  DROP_DEAD();
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsICacheInfoChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::GetCacheTokenExpirationTime(uint32_t *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  if (!mCacheEntryAvailable)
    return NS_ERROR_NOT_AVAILABLE;

  *_retval = mCacheExpirationTime;
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::GetCacheTokenCachedCharset(nsACString &_retval)
{
  if (!mCacheEntryAvailable)
    return NS_ERROR_NOT_AVAILABLE;

  _retval = mCachedCharset;
  return NS_OK;
}
NS_IMETHODIMP
HttpChannelChild::SetCacheTokenCachedCharset(const nsACString &aCharset)
{
  if (!mCacheEntryAvailable || !RemoteChannelExists())
    return NS_ERROR_NOT_AVAILABLE;

  mCachedCharset = aCharset;
  if (!SendSetCacheTokenCachedCharset(PromiseFlatCString(aCharset))) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::IsFromCache(bool *value)
{
  if (!mIsPending)
    return NS_ERROR_NOT_AVAILABLE;

  *value = mIsFromCache;
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::GetCacheKey(nsISupports **cacheKey)
{
  NS_IF_ADDREF(*cacheKey = mCacheKey);
  return NS_OK;
}
NS_IMETHODIMP
HttpChannelChild::SetCacheKey(nsISupports *cacheKey)
{
  ENSURE_CALLED_BEFORE_ASYNC_OPEN();

  mCacheKey = cacheKey;
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::SetAllowStaleCacheContent(bool aAllowStaleCacheContent)
{
  mAllowStaleCacheContent = aAllowStaleCacheContent;
  return NS_OK;
}
NS_IMETHODIMP
HttpChannelChild::GetAllowStaleCacheContent(bool *aAllowStaleCacheContent)
{
  NS_ENSURE_ARG(aAllowStaleCacheContent);
  *aAllowStaleCacheContent = mAllowStaleCacheContent;
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::PreferAlternativeDataType(const nsACString & aType)
{
  ENSURE_CALLED_BEFORE_ASYNC_OPEN();
  mPreferredCachedAltDataType = aType;
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::GetAlternativeDataType(nsACString & aType)
{
  // Must be called during or after OnStartRequest
  if (!mAfterOnStartRequestBegun) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  aType = mAvailableCachedAltDataType;
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::OpenAlternativeOutputStream(const nsACString & aType, nsIOutputStream * *_retval)
{
  MOZ_ASSERT(NS_IsMainThread(), "Main thread only");

  if (!mIPCOpen) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  if (static_cast<ContentChild*>(gNeckoChild->Manager())->IsShuttingDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  RefPtr<AltDataOutputStreamChild> stream =
    static_cast<AltDataOutputStreamChild*>(gNeckoChild->SendPAltDataOutputStreamConstructor(nsCString(aType), this));
  stream.forget(_retval);
  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIResumableChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::ResumeAt(uint64_t startPos, const nsACString& entityID)
{
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
HttpChannelChild::SetPriority(int32_t aPriority)
{
  int16_t newValue = clamped<int32_t>(aPriority, INT16_MIN, INT16_MAX);
  if (mPriority == newValue)
    return NS_OK;
  mPriority = newValue;
  if (RemoteChannelExists())
    SendSetPriority(mPriority);
  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIClassOfService
//-----------------------------------------------------------------------------
NS_IMETHODIMP
HttpChannelChild::SetClassFlags(uint32_t inFlags)
{
  if (mClassOfService == inFlags) {
    return NS_OK;
  }

  mClassOfService = inFlags;
  if (RemoteChannelExists()) {
    SendSetClassOfService(mClassOfService);
  }
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::AddClassFlags(uint32_t inFlags)
{
  mClassOfService |= inFlags;
  if (RemoteChannelExists()) {
    SendSetClassOfService(mClassOfService);
  }
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::ClearClassFlags(uint32_t inFlags)
{
  mClassOfService &= ~inFlags;
  if (RemoteChannelExists()) {
    SendSetClassOfService(mClassOfService);
  }
  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIProxiedChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::GetProxyInfo(nsIProxyInfo **aProxyInfo)
{
  DROP_DEAD();
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIApplicationCacheContainer
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::GetApplicationCache(nsIApplicationCache **aApplicationCache)
{
  NS_IF_ADDREF(*aApplicationCache = mApplicationCache);
  return NS_OK;
}
NS_IMETHODIMP
HttpChannelChild::SetApplicationCache(nsIApplicationCache *aApplicationCache)
{
  NS_ENSURE_TRUE(!mWasOpened, NS_ERROR_ALREADY_OPENED);

  mApplicationCache = aApplicationCache;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIApplicationCacheChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::GetApplicationCacheForWrite(nsIApplicationCache **aApplicationCache)
{
  *aApplicationCache = nullptr;
  return NS_OK;
}
NS_IMETHODIMP
HttpChannelChild::SetApplicationCacheForWrite(nsIApplicationCache *aApplicationCache)
{
  NS_ENSURE_TRUE(!mWasOpened, NS_ERROR_ALREADY_OPENED);

  // Child channels are not intended to be used for cache writes
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
HttpChannelChild::GetLoadedFromApplicationCache(bool *aLoadedFromApplicationCache)
{
  *aLoadedFromApplicationCache = mLoadedFromApplicationCache;
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::GetInheritApplicationCache(bool *aInherit)
{
  *aInherit = mInheritApplicationCache;
  return NS_OK;
}
NS_IMETHODIMP
HttpChannelChild::SetInheritApplicationCache(bool aInherit)
{
  mInheritApplicationCache = aInherit;
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::GetChooseApplicationCache(bool *aChoose)
{
  *aChoose = mChooseApplicationCache;
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::SetChooseApplicationCache(bool aChoose)
{
  mChooseApplicationCache = aChoose;
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::MarkOfflineCacheEntryAsForeign()
{
  SendMarkOfflineCacheEntryAsForeign();
  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIAssociatedContentSecurity
//-----------------------------------------------------------------------------

bool
HttpChannelChild::GetAssociatedContentSecurity(
                    nsIAssociatedContentSecurity** _result)
{
  if (!mSecurityInfo)
    return false;

  nsCOMPtr<nsIAssociatedContentSecurity> assoc =
      do_QueryInterface(mSecurityInfo);
  if (!assoc)
    return false;

  if (_result)
    assoc.forget(_result);
  return true;
}

NS_IMETHODIMP
HttpChannelChild::GetCountSubRequestsBrokenSecurity(
                    int32_t *aSubRequestsBrokenSecurity)
{
  nsCOMPtr<nsIAssociatedContentSecurity> assoc;
  if (!GetAssociatedContentSecurity(getter_AddRefs(assoc)))
    return NS_OK;

  return assoc->GetCountSubRequestsBrokenSecurity(aSubRequestsBrokenSecurity);
}
NS_IMETHODIMP
HttpChannelChild::SetCountSubRequestsBrokenSecurity(
                    int32_t aSubRequestsBrokenSecurity)
{
  nsCOMPtr<nsIAssociatedContentSecurity> assoc;
  if (!GetAssociatedContentSecurity(getter_AddRefs(assoc)))
    return NS_OK;

  return assoc->SetCountSubRequestsBrokenSecurity(aSubRequestsBrokenSecurity);
}

NS_IMETHODIMP
HttpChannelChild::GetCountSubRequestsNoSecurity(int32_t *aSubRequestsNoSecurity)
{
  nsCOMPtr<nsIAssociatedContentSecurity> assoc;
  if (!GetAssociatedContentSecurity(getter_AddRefs(assoc)))
    return NS_OK;

  return assoc->GetCountSubRequestsNoSecurity(aSubRequestsNoSecurity);
}
NS_IMETHODIMP
HttpChannelChild::SetCountSubRequestsNoSecurity(int32_t aSubRequestsNoSecurity)
{
  nsCOMPtr<nsIAssociatedContentSecurity> assoc;
  if (!GetAssociatedContentSecurity(getter_AddRefs(assoc)))
    return NS_OK;

  return assoc->SetCountSubRequestsNoSecurity(aSubRequestsNoSecurity);
}

NS_IMETHODIMP
HttpChannelChild::Flush()
{
  nsCOMPtr<nsIAssociatedContentSecurity> assoc;
  if (!GetAssociatedContentSecurity(getter_AddRefs(assoc)))
    return NS_OK;

  nsresult rv;
  int32_t broken, no;

  rv = assoc->GetCountSubRequestsBrokenSecurity(&broken);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = assoc->GetCountSubRequestsNoSecurity(&no);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mIPCOpen)
    SendUpdateAssociatedContentSecurity(broken, no);

  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIHttpChannelChild
//-----------------------------------------------------------------------------

NS_IMETHODIMP HttpChannelChild::AddCookiesToRequest()
{
  HttpBaseChannel::AddCookiesToRequest();
  return NS_OK;
}

NS_IMETHODIMP HttpChannelChild::GetClientSetRequestHeaders(RequestHeaderTuples **aRequestHeaders)
{
  *aRequestHeaders = &mClientSetRequestHeaders;
  return NS_OK;
}

void
HttpChannelChild::GetClientSetCorsPreflightParameters(OptionalCorsPreflightArgs& aArgs)
{
  if (mRequireCORSPreflight) {
    CorsPreflightArgs args;
    args.unsafeHeaders() = mUnsafeHeaders;
    aArgs = args;
  } else {
    aArgs = mozilla::void_t();
  }
}

NS_IMETHODIMP
HttpChannelChild::RemoveCorsPreflightCacheEntry(nsIURI* aURI,
                                                nsIPrincipal* aPrincipal)
{
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
  if (mIPCOpen) {
    result = SendRemoveCorsPreflightCacheEntry(uri, principalInfo);
  }
  return result ? NS_OK : NS_ERROR_FAILURE;
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIDivertableChannel
//-----------------------------------------------------------------------------
NS_IMETHODIMP
HttpChannelChild::DivertToParent(ChannelDiverterChild **aChild)
{
  LOG(("HttpChannelChild::DivertToParent [this=%p]\n", this));
  MOZ_RELEASE_ASSERT(aChild);
  MOZ_RELEASE_ASSERT(gNeckoChild);
  MOZ_RELEASE_ASSERT(!mDivertingToParent);

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
HttpChannelChild::UnknownDecoderInvolvedKeepData()
{
  LOG(("HttpChannelChild::UnknownDecoderInvolvedKeepData [this=%p]",
       this));
  mUnknownDecoderInvolved = true;
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::UnknownDecoderInvolvedOnStartRequestCalled()
{
  LOG(("HttpChannelChild::UnknownDecoderInvolvedOnStartRequestCalled "
       "[this=%p, mDivertingToParent=%d]", this, mDivertingToParent));
  mUnknownDecoderInvolved = false;

  nsresult rv = NS_OK;

  if (mDivertingToParent) {
    rv = mEventQ->PrependEvents(mUnknownDecoderEventQ);
  }
  mUnknownDecoderEventQ.Clear();

  return rv;
}

NS_IMETHODIMP
HttpChannelChild::GetDivertingToParent(bool* aDiverting)
{
  NS_ENSURE_ARG_POINTER(aDiverting);
  *aDiverting = mDivertingToParent;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIThreadRetargetableRequest
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::RetargetDeliveryTo(nsIEventTarget* aNewTarget)
{
  LOG(("HttpChannelChild::RetargetDeliveryTo [this=%p, aNewTarget=%p]",
       this, aNewTarget));

  MOZ_ASSERT(NS_IsMainThread(), "Should be called on main thread only");
  MOZ_ASSERT(!mODATarget);
  MOZ_ASSERT(aNewTarget);

  NS_ENSURE_ARG(aNewTarget);
  if (aNewTarget == NS_GetCurrentThread()) {
    NS_WARNING("Retargeting delivery to same thread");
    return NS_OK;
  }

  // Ensure that |mListener| and any subsequent listeners can be retargeted
  // to another thread.
  nsresult rv = NS_OK;
  nsCOMPtr<nsIThreadRetargetableStreamListener> retargetableListener =
      do_QueryInterface(mListener, &rv);
  if (!retargetableListener || NS_FAILED(rv)) {
    NS_WARNING("Listener is not retargetable");
    return NS_ERROR_NO_INTERFACE;
  }

  rv = retargetableListener->CheckListenerChain();
  if (NS_FAILED(rv)) {
    NS_WARNING("Subsequent listeners are not retargetable");
    return rv;
  }

  {
    MutexAutoLock lock(mEventTargetMutex);
    mODATarget = aNewTarget;
  }
  return NS_OK;
}

void
HttpChannelChild::ResetInterception()
{
  NS_ENSURE_TRUE_VOID(gNeckoChild != nullptr);

  if (mInterceptListener) {
    mInterceptListener->Cleanup();
  }
  mInterceptListener = nullptr;

  // The chance to intercept any further requests associated with this channel
  // (such as redirects) has passed.
  mLoadFlags |= LOAD_BYPASS_SERVICE_WORKER;

  // Continue with the original cross-process request
  nsresult rv = ContinueAsyncOpen();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Unused << AsyncAbort(rv);
  }
}

NS_IMETHODIMP
HttpChannelChild::GetResponseSynthesized(bool* aSynthesized)
{
  NS_ENSURE_ARG_POINTER(aSynthesized);
  *aSynthesized = mSynthesizedResponse;
  return NS_OK;
}

void
HttpChannelChild::TrySendDeletingChannel()
{
  if (!mDeletingChannelSent.compareExchange(false, true)) {
    // SendDeletingChannel is already sent.
    return;
  }

  if (NS_IsMainThread()) {
    if (NS_WARN_IF(!mIPCOpen)) {
      // IPC actor is detroyed already, do not send more messages.
      return;
    }

    Unused << PHttpChannelChild::SendDeletingChannel();
    return;
  }

  nsCOMPtr<nsIEventTarget> neckoTarget = GetNeckoTarget();
  MOZ_ASSERT(neckoTarget);

  DebugOnly<nsresult> rv =
    neckoTarget->Dispatch(
      NewNonOwningRunnableMethod(this, &HttpChannelChild::TrySendDeletingChannel),
      NS_DISPATCH_NORMAL);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

void
HttpChannelChild::OnCopyComplete(nsresult aStatus)
{
  nsCOMPtr<nsIRunnable> runnable = NewRunnableMethod<nsresult>(
    this, &HttpChannelChild::EnsureUploadStreamIsCloneableComplete, aStatus);
  nsCOMPtr<nsIEventTarget> neckoTarget = GetNeckoTarget();
  MOZ_ASSERT(neckoTarget);

  Unused << neckoTarget->Dispatch(runnable, NS_DISPATCH_NORMAL);
}

nsresult
HttpChannelChild::AsyncCall(void (HttpChannelChild::*funcPtr)(),
                            nsRunnableMethod<HttpChannelChild> **retval)
{
  nsresult rv;

  RefPtr<nsRunnableMethod<HttpChannelChild>> event =
    NewRunnableMethod(this, funcPtr);
  nsCOMPtr<nsIEventTarget> neckoTarget = GetNeckoTarget();
  MOZ_ASSERT(neckoTarget);

  rv = neckoTarget->Dispatch(event, NS_DISPATCH_NORMAL);

  if (NS_SUCCEEDED(rv) && retval) {
    *retval = event;
  }

  return rv;
}

class CancelEvent final : public ChannelEvent
{
public:
  CancelEvent(HttpChannelChild* aChild, nsresult aRv)
    : mChild(aChild)
    , mRv(aRv)
  {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(aChild);
  }

  void Run() {
    MOZ_ASSERT(NS_IsMainThread());
    mChild->Cancel(mRv);
  }

  already_AddRefed<nsIEventTarget> GetEventTarget()
  {
    MOZ_ASSERT(mChild);
    nsCOMPtr<nsIEventTarget> target = mChild->GetNeckoTarget();
    return target.forget();
  }

private:
  HttpChannelChild* mChild;
  const nsresult mRv;
};

void
HttpChannelChild::CancelOnMainThread(nsresult aRv)
{
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

void
HttpChannelChild::OverrideWithSynthesizedResponse(nsAutoPtr<nsHttpResponseHead>& aResponseHead,
                                                  nsIInputStream* aSynthesizedInput,
                                                  InterceptStreamListener* aStreamListener)
{
  mInterceptListener = aStreamListener;

  // Intercepted responses should already be decoded.  If its a redirect,
  // however, we want to respect the encoding of the final result instead.
  if (!nsHttpChannel::WillRedirect(aResponseHead)) {
    SetApplyConversion(false);
  }

  mResponseHead = aResponseHead;
  mSynthesizedResponse = true;

  if (nsHttpChannel::WillRedirect(mResponseHead)) {
    mShouldInterceptSubsequentRedirect = true;
    // Continue with the original cross-process request
    nsresult rv = ContinueAsyncOpen();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      rv = AsyncAbort(rv);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
    return;
  }

  // In our current implementation, the FetchEvent handler will copy the
  // response stream completely into the pipe backing the input stream so we
  // can treat the available as the length of the stream.
  uint64_t available;
  nsresult rv = aSynthesizedInput->Available(&available);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mSynthesizedStreamLength = -1;
  } else {
    mSynthesizedStreamLength = int64_t(available);
  }

  nsCOMPtr<nsIEventTarget> neckoTarget = GetNeckoTarget();
  MOZ_ASSERT(neckoTarget);

  rv = nsInputStreamPump::Create(getter_AddRefs(mSynthesizedResponsePump),
                                 aSynthesizedInput,
                                 int64_t(-1), int64_t(-1), 0, 0, true,
                                 neckoTarget);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aSynthesizedInput->Close();
    return;
  }

  rv = mSynthesizedResponsePump->AsyncRead(aStreamListener, nullptr);
  NS_ENSURE_SUCCESS_VOID(rv);

  // if this channel has been suspended previously, the pump needs to be
  // correspondingly suspended now that it exists.
  for (uint32_t i = 0; i < mSuspendCount; i++) {
    rv = mSynthesizedResponsePump->Suspend();
    NS_ENSURE_SUCCESS_VOID(rv);
  }

  if (mCanceled) {
    mSynthesizedResponsePump->Cancel(mStatus);
  }
}

NS_IMETHODIMP
HttpChannelChild::ForceIntercepted(bool aPostRedirectChannelShouldIntercept,
                                   bool aPostRedirectChannelShouldUpgrade)
{
  mShouldParentIntercept = true;
  mPostRedirectChannelShouldIntercept = aPostRedirectChannelShouldIntercept;
  mPostRedirectChannelShouldUpgrade = aPostRedirectChannelShouldUpgrade;
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::ForceIntercepted(uint64_t aInterceptionID)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

void
HttpChannelChild::ForceIntercepted(nsIInputStream* aSynthesizedInput)
{
  mSynthesizedInput = aSynthesizedInput;
  mSynthesizedResponse = true;
  mRedirectingForSubsequentSynthesizedResponse = true;
}

mozilla::ipc::IPCResult
HttpChannelChild::RecvIssueDeprecationWarning(const uint32_t& warning,
                                              const bool& asError)
{
  nsCOMPtr<nsIDeprecationWarner> warner;
  GetCallback(warner);
  if (warner) {
    warner->IssueWarning(warning, asError);
  }
  return IPC_OK();
}

bool
HttpChannelChild::ShouldInterceptURI(nsIURI* aURI,
                                     bool& aShouldUpgrade)
{
  bool isHttps = false;
  nsresult rv = aURI->SchemeIs("https", &isHttps);
  NS_ENSURE_SUCCESS(rv, false);
  nsCOMPtr<nsIPrincipal> resultPrincipal;
  if (!isHttps && mLoadInfo) {
      nsContentUtils::GetSecurityManager()->
        GetChannelResultPrincipal(this, getter_AddRefs(resultPrincipal));
  }
  OriginAttributes originAttributes;
  NS_ENSURE_TRUE(NS_GetOriginAttributes(this, originAttributes), false);
  rv = NS_ShouldSecureUpgrade(aURI,
                              mLoadInfo,
                              resultPrincipal,
                              mPrivateBrowsing,
                              mAllowSTS,
                              originAttributes,
                              aShouldUpgrade);
  NS_ENSURE_SUCCESS(rv, false);

  nsCOMPtr<nsIURI> upgradedURI;
  if (aShouldUpgrade) {
    rv = NS_GetSecureUpgradedURI(aURI, getter_AddRefs(upgradedURI));
    NS_ENSURE_SUCCESS(rv, false);
  }

  return ShouldIntercept(upgradedURI ? upgradedURI.get() : aURI);
}

mozilla::ipc::IPCResult
HttpChannelChild::RecvSetPriority(const int16_t& aPriority)
{
  mPriority = aPriority;
  return IPC_OK();
}

} // namespace net
} // namespace mozilla
