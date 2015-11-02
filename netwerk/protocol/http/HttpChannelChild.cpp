/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "nsHttp.h"
#include "nsICacheEntry.h"
#include "mozilla/unused.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/ipc/FileDescriptorSetChild.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/net/HttpChannelChild.h"

#include "nsChannelClassifier.h"
#include "nsStringStream.h"
#include "nsHttpHandler.h"
#include "nsNetUtil.h"
#include "nsSerializationHelper.h"
#include "mozilla/Attributes.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/net/ChannelDiverterChild.h"
#include "mozilla/net/DNS.h"
#include "SerializedLoadContext.h"
#include "nsInputStreamPump.h"
#include "InterceptedChannel.h"
#include "nsPerformance.h"
#include "mozIThirdPartyUtil.h"
#include "nsContentSecurityManager.h"
#include "nsIDeprecationWarner.h"

#ifdef OS_POSIX
#include "chrome/common/file_descriptor_set_posix.h"
#endif

using namespace mozilla::dom;
using namespace mozilla::ipc;

namespace mozilla {
namespace net {

extern bool
WillRedirect(const nsHttpResponseHead * response);

namespace {

const uint32_t kMaxFileDescriptorsPerMessage = 250;

#ifdef OS_POSIX
// Keep this in sync with other platforms.
static_assert(FileDescriptorSet::MAX_DESCRIPTORS_PER_MESSAGE == 250,
              "MAX_DESCRIPTORS_PER_MESSAGE mismatch!");
#endif

} // namespace

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
    mOwner->DoOnStopRequest(mOwner, mContext);
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
  , mIPCOpen(false)
  , mKeptAlive(false)
  , mUnknownDecoderInvolved(false)
  , mDivertingToParent(false)
  , mFlushedForDiversion(false)
  , mSuspendSent(false)
  , mSynthesizedResponse(false)
  , mShouldInterceptSubsequentRedirect(false)
  , mRedirectingForSubsequentSynthesizedResponse(false)
  , mShouldParentIntercept(false)
{
  LOG(("Creating HttpChannelChild @%x\n", this));

  mChannelCreationTime = PR_Now();
  mChannelCreationTimestamp = TimeStamp::Now();
  mAsyncOpenTime = TimeStamp::Now();
  mEventQ = new ChannelEventQueue(static_cast<nsIHttpChannel*>(this));
}

HttpChannelChild::~HttpChannelChild()
{
  LOG(("Destroying HttpChannelChild @%x\n", this));
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsISupports
//-----------------------------------------------------------------------------

// Override nsHashPropertyBag's AddRef: we don't need thread-safe refcnt
NS_IMPL_ADDREF(HttpChannelChild)

NS_IMETHODIMP_(MozExternalRefCountType) HttpChannelChild::Release()
{
  NS_PRECONDITION(0 != mRefCnt, "dup release");
  NS_ASSERT_OWNINGTHREAD(HttpChannelChild);
  --mRefCnt;
  NS_LOG_RELEASE(this, mRefCnt, "HttpChannelChild");

  // Normally we Send_delete in OnStopRequest, but when we need to retain the
  // remote channel for security info IPDL itself holds 1 reference, so we
  // Send_delete when refCnt==1.  But if !mIPCOpen, then there's nobody to send
  // to, so we fall through.
  if (mKeptAlive && mRefCnt == 1 && mIPCOpen) {
    mKeptAlive = false;
    // Send_delete calls NeckoChild::DeallocPHttpChannel, which will release
    // again to refcount==0
    PHttpChannelChild::Send__delete__(this);
    return 0;
  }

  if (mRefCnt == 0) {
    mRefCnt = 1; /* stabilize */
    delete this;
    return 0;
  }
  return mRefCnt;
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

class AssociateApplicationCacheEvent : public ChannelEvent
{
  public:
    AssociateApplicationCacheEvent(HttpChannelChild* child,
                                   const nsCString &groupID,
                                   const nsCString &clientID)
    : mChild(child)
    , groupID(groupID)
    , clientID(clientID) {}

    void Run() { mChild->AssociateApplicationCache(groupID, clientID); }
  private:
    HttpChannelChild* mChild;
    nsCString groupID;
    nsCString clientID;
};

bool
HttpChannelChild::RecvAssociateApplicationCache(const nsCString &groupID,
                                                const nsCString &clientID)
{
  LOG(("HttpChannelChild::RecvAssociateApplicationCache [this=%p]\n", this));
  if (mEventQ->ShouldEnqueue()) {
    mEventQ->Enqueue(new AssociateApplicationCacheEvent(this, groupID, clientID));
  } else {
    AssociateApplicationCache(groupID, clientID);
  }
  return true;
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
  StartRequestEvent(HttpChannelChild* child,
                    const nsresult& channelStatus,
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
                    const uint32_t& cacheKey)
  : mChild(child)
  , mChannelStatus(channelStatus)
  , mResponseHead(responseHead)
  , mRequestHeaders(requestHeaders)
  , mUseResponseHead(useResponseHead)
  , mIsFromCache(isFromCache)
  , mCacheEntryAvailable(cacheEntryAvailable)
  , mCacheExpirationTime(cacheExpirationTime)
  , mCachedCharset(cachedCharset)
  , mSecurityInfoSerialization(securityInfoSerialization)
  , mSelfAddr(selfAddr)
  , mPeerAddr(peerAddr)
  , mCacheKey(cacheKey)
  {}

  void Run()
  {
    LOG(("StartRequestEvent [this=%p]\n", mChild));
    mChild->OnStartRequest(mChannelStatus, mResponseHead, mUseResponseHead,
                           mRequestHeaders, mIsFromCache, mCacheEntryAvailable,
                           mCacheExpirationTime, mCachedCharset,
                           mSecurityInfoSerialization, mSelfAddr, mPeerAddr,
                           mCacheKey);
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
};

bool
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
                                     const uint32_t& cacheKey)
{
  LOG(("HttpChannelChild::RecvOnStartRequest [this=%p]\n", this));
  // mFlushedForDiversion and mDivertingToParent should NEVER be set at this
  // stage, as they are set in the listener's OnStartRequest.
  MOZ_RELEASE_ASSERT(!mFlushedForDiversion,
    "mFlushedForDiversion should be unset before OnStartRequest!");
  MOZ_RELEASE_ASSERT(!mDivertingToParent,
    "mDivertingToParent should be unset before OnStartRequest!");


  mRedirectCount = redirectCount;

  if (mEventQ->ShouldEnqueue()) {
    mEventQ->Enqueue(new StartRequestEvent(this, channelStatus, responseHead,
                                           useResponseHead, requestHeaders,
                                           isFromCache, cacheEntryAvailable,
                                           cacheExpirationTime, cachedCharset,
                                           securityInfoSerialization, selfAddr,
                                           peerAddr, cacheKey));
  } else {
    OnStartRequest(channelStatus, responseHead, useResponseHead, requestHeaders,
                   isFromCache, cacheEntryAvailable, cacheExpirationTime,
                   cachedCharset, securityInfoSerialization, selfAddr,
                   peerAddr, cacheKey);
  }
  return true;
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
                                 const uint32_t& cacheKey)
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
  mRequestHead.Headers() = requestHeaders;

  // Note: this is where we would notify "http-on-examine-response" observers.
  // We have deliberately disabled this for child processes (see bug 806753)
  //
  // gHttpHandler->OnExamineResponse(this);

  mTracingEnabled = false;

  DoOnStartRequest(this, mListenerContext);

  mSelfAddr = selfAddr;
  mPeerAddr = peerAddr;
}

void
HttpChannelChild::DoOnStartRequest(nsIRequest* aRequest, nsISupports* aContext)
{
  LOG(("HttpChannelChild::DoOnStartRequest [this=%p]\n", this));
  nsresult rv = mListener->OnStartRequest(aRequest, aContext);
  if (NS_FAILED(rv)) {
    Cancel(rv);
    return;
  }

  if (mDivertingToParent) {
    mListener = nullptr;
    mListenerContext = nullptr;
    if (mLoadGroup) {
      mLoadGroup->RemoveRequest(this, nullptr, mStatus);
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
  }
}

class TransportAndDataEvent : public ChannelEvent
{
 public:
  TransportAndDataEvent(HttpChannelChild* child,
                        const nsresult& channelStatus,
                        const nsresult& transportStatus,
                        const uint64_t& progress,
                        const uint64_t& progressMax,
                        const nsCString& data,
                        const uint64_t& offset,
                        const uint32_t& count)
  : mChild(child)
  , mChannelStatus(channelStatus)
  , mTransportStatus(transportStatus)
  , mProgress(progress)
  , mProgressMax(progressMax)
  , mData(data)
  , mOffset(offset)
  , mCount(count) {}

  void Run()
  {
    mChild->OnTransportAndData(mChannelStatus, mTransportStatus, mProgress,
                               mProgressMax, mData, mOffset, mCount);
  }
 private:
  HttpChannelChild* mChild;
  nsresult mChannelStatus;
  nsresult mTransportStatus;
  uint64_t mProgress;
  uint64_t mProgressMax;
  nsCString mData;
  uint64_t mOffset;
  uint32_t mCount;
};

bool
HttpChannelChild::RecvOnTransportAndData(const nsresult& channelStatus,
                                         const nsresult& transportStatus,
                                         const uint64_t& progress,
                                         const uint64_t& progressMax,
                                         const nsCString& data,
                                         const uint64_t& offset,
                                         const uint32_t& count)
{
  LOG(("HttpChannelChild::RecvOnTransportAndData [this=%p]\n", this));
  MOZ_RELEASE_ASSERT(!mFlushedForDiversion,
                     "Should not be receiving any more callbacks from parent!");

  if (mEventQ->ShouldEnqueue()) {
    mEventQ->Enqueue(new TransportAndDataEvent(this, channelStatus,
                                               transportStatus, progress,
                                               progressMax, data, offset,
                                               count));
  } else {
    MOZ_RELEASE_ASSERT(!mDivertingToParent,
                       "ShouldEnqueue when diverting to parent!");

    OnTransportAndData(channelStatus, transportStatus, progress, progressMax,
                       data, offset, count);
  }
  return true;
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
                                     const uint64_t progress,
                                     const uint64_t& progressMax,
                                     const nsCString& data,
                                     const uint64_t& offset,
                                     const uint32_t& count)
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
      new MaybeDivertOnDataHttpEvent(this, data, offset, count));
  }

  // Hold queue lock throughout all three calls, else we might process a later
  // necko msg in between them.
  AutoEventEnqueuer ensureSerialDispatch(mEventQ);

  DoOnStatus(this, transportStatus);
  DoOnProgress(this, progress, progressMax);

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
      MOZ_ASSERT((progressMax == -1) || (progress <= progressMax),
                 "unexpected progress values");
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
    Cancel(rv);
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
 private:
  HttpChannelChild* mChild;
  nsresult mChannelStatus;
  ResourceTimingStruct mTiming;
};

bool
HttpChannelChild::RecvOnStopRequest(const nsresult& channelStatus,
                                    const ResourceTimingStruct& timing)
{
  LOG(("HttpChannelChild::RecvOnStopRequest [this=%p]\n", this));
  MOZ_RELEASE_ASSERT(!mFlushedForDiversion,
    "Should not be receiving any more callbacks from parent!");

  if (mEventQ->ShouldEnqueue()) {
    mEventQ->Enqueue(new StopRequestEvent(this, channelStatus, timing));
  } else {
    MOZ_ASSERT(!mDivertingToParent, "ShouldEnqueue when diverting to parent!");

    OnStopRequest(channelStatus, timing);
  }
  return true;
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

 private:
  HttpChannelChild* mChild;
  nsresult mChannelStatus;
};

void
HttpChannelChild::MaybeDivertOnStop(const nsresult& aChannelStatus)
{
  LOG(("HttpChannelChild::MaybeDivertOnStop [this=%p, "
       "mDivertingToParent=%d status=%x]", this, mDivertingToParent,
       aChannelStatus));
  if (mDivertingToParent) {
    SendDivertOnStopRequest(aChannelStatus);
  }
}

void
HttpChannelChild::OnStopRequest(const nsresult& channelStatus,
                                const ResourceTimingStruct& timing)
{
  LOG(("HttpChannelChild::OnStopRequest [this=%p status=%x]\n",
       this, channelStatus));

  if (mDivertingToParent) {
    MOZ_RELEASE_ASSERT(!mFlushedForDiversion,
      "Should not be processing any more callbacks from parent!");

    SendDivertOnStopRequest(channelStatus);
    return;
  }

  if (mUnknownDecoderInvolved) {
   LOG(("UnknownDecoder is involved queue OnStopRequest call. [this=%p]",
        this));
    mUnknownDecoderEventQ.AppendElement(
      new MaybeDivertOnStopHttpEvent(this, channelStatus));
  }

  mTransactionTimings.domainLookupStart = timing.domainLookupStart;
  mTransactionTimings.domainLookupEnd = timing.domainLookupEnd;
  mTransactionTimings.connectStart = timing.connectStart;
  mTransactionTimings.connectEnd = timing.connectEnd;
  mTransactionTimings.requestStart = timing.requestStart;
  mTransactionTimings.responseStart = timing.responseStart;
  mTransactionTimings.responseEnd = timing.responseEnd;
  mAsyncOpenTime = timing.fetchStart;
  mRedirectStartTimeStamp = timing.redirectStart;
  mRedirectEndTimeStamp = timing.redirectEnd;

  nsPerformance* documentPerformance = GetPerformance();
  if (documentPerformance) {
      documentPerformance->AddEntry(this, this);
  }

  DoPreOnStopRequest(channelStatus);

  { // We must flush the queue before we Send__delete__
    // (although we really shouldn't receive any msgs after OnStop),
    // so make sure this goes out of scope before then.
    AutoEventEnqueuer ensureSerialDispatch(mEventQ);

    DoOnStopRequest(this, mListenerContext);
  }

  ReleaseListeners();

  if (mLoadFlags & LOAD_DOCUMENT_URI) {
    // Keep IPDL channel open, but only for updating security info.
    mKeptAlive = true;
    SendDocumentChannelCleanup();
  } else {
    // This calls NeckoChild::DeallocPHttpChannelChild(), which deletes |this| if IPDL
    // holds the last reference.  Don't rely on |this| existing after here.
    PHttpChannelChild::Send__delete__(this);
  }
}

void
HttpChannelChild::DoPreOnStopRequest(nsresult aStatus)
{
  LOG(("HttpChannelChild::DoPreOnStopRequest [this=%p status=%x]\n",
       this, aStatus));
  mIsPending = false;

  if (!mCanceled && NS_SUCCEEDED(mStatus)) {
    mStatus = aStatus;
  }
}

void
HttpChannelChild::DoOnStopRequest(nsIRequest* aRequest, nsISupports* aContext)
{
  LOG(("HttpChannelChild::DoOnStopRequest [this=%p]\n", this));
  MOZ_ASSERT(!mIsPending);

  if (mStatus == NS_ERROR_TRACKING_URI) {
    nsChannelClassifier::SetBlockedTrackingContent(this);
  }

  mListener->OnStopRequest(aRequest, aContext, mStatus);

  mListener = 0;
  mListenerContext = 0;
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
 private:
  HttpChannelChild* mChild;
  int64_t mProgress, mProgressMax;
};

bool
HttpChannelChild::RecvOnProgress(const int64_t& progress,
                                 const int64_t& progressMax)
{
  if (mEventQ->ShouldEnqueue())  {
    mEventQ->Enqueue(new ProgressEvent(this, progress, progressMax));
  } else {
    OnProgress(progress, progressMax);
  }
  return true;
}

void
HttpChannelChild::OnProgress(const int64_t& progress,
                             const int64_t& progressMax)
{
  LOG(("HttpChannelChild::OnProgress [this=%p progress=%lld/%lld]\n",
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
      MOZ_ASSERT((progressMax == -1) || (progress <= progressMax),
                 "unexpected progress values");
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
 private:
  HttpChannelChild* mChild;
  nsresult mStatus;
};

bool
HttpChannelChild::RecvOnStatus(const nsresult& status)
{
  if (mEventQ->ShouldEnqueue()) {
    mEventQ->Enqueue(new StatusEvent(this, status));
  } else {
    OnStatus(status);
  }
  return true;
}

void
HttpChannelChild::OnStatus(const nsresult& status)
{
  LOG(("HttpChannelChild::OnStatus [this=%p status=%x]\n", this, status));

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
 private:
  HttpChannelChild* mChild;
  nsresult mStatus;
};

bool
HttpChannelChild::RecvFailedAsyncOpen(const nsresult& status)
{
  LOG(("HttpChannelChild::RecvFailedAsyncOpen [this=%p]\n", this));
  if (mEventQ->ShouldEnqueue()) {
    mEventQ->Enqueue(new FailedAsyncOpenEvent(this, status));
  } else {
    FailedAsyncOpen(status);
  }
  return true;
}

// We need to have an implementation of this function just so that we can keep
// all references to mCallOnResume of type HttpChannelChild:  it's not OK in C++
// to set a member function ptr to a base class function.
void
HttpChannelChild::HandleAsyncAbort()
{
  HttpAsyncAborter<HttpChannelChild>::HandleAsyncAbort();
}

void
HttpChannelChild::FailedAsyncOpen(const nsresult& status)
{
  LOG(("HttpChannelChild::FailedAsyncOpen [this=%p status=%x]\n", this, status));

  mStatus = status;

  // We're already being called from IPDL, therefore already "async"
  HandleAsyncAbort();
}

void
HttpChannelChild::DoNotifyListenerCleanup()
{
  LOG(("HttpChannelChild::DoNotifyListenerCleanup [this=%p]\n", this));
  if (mIPCOpen)
    PHttpChannelChild::Send__delete__(this);
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
 private:
  HttpChannelChild* mChild;
};

bool
HttpChannelChild::RecvDeleteSelf()
{
  LOG(("HttpChannelChild::RecvDeleteSelf [this=%p]\n", this));
  if (mEventQ->ShouldEnqueue()) {
    mEventQ->Enqueue(new DeleteSelfEvent(this));
  } else {
    DeleteSelf();
  }
  return true;
}

void
HttpChannelChild::DeleteSelf()
{
  Send__delete__(this);
}

bool
HttpChannelChild::RecvReportSecurityMessage(const nsString& messageTag,
                                            const nsString& messageCategory)
{
  AddSecurityMessage(messageTag, messageCategory);
  return true;
}

class Redirect1Event : public ChannelEvent
{
 public:
  Redirect1Event(HttpChannelChild* child,
                 const uint32_t& newChannelId,
                 const URIParams& newURI,
                 const uint32_t& redirectFlags,
                 const nsHttpResponseHead& responseHead,
                 const nsACString& securityInfoSerialization)
  : mChild(child)
  , mNewChannelId(newChannelId)
  , mNewURI(newURI)
  , mRedirectFlags(redirectFlags)
  , mResponseHead(responseHead)
  , mSecurityInfoSerialization(securityInfoSerialization) {}

  void Run()
  {
    mChild->Redirect1Begin(mNewChannelId, mNewURI, mRedirectFlags,
                           mResponseHead, mSecurityInfoSerialization);
  }
 private:
  HttpChannelChild*   mChild;
  uint32_t            mNewChannelId;
  URIParams           mNewURI;
  uint32_t            mRedirectFlags;
  nsHttpResponseHead  mResponseHead;
  nsCString           mSecurityInfoSerialization;
};

bool
HttpChannelChild::RecvRedirect1Begin(const uint32_t& newChannelId,
                                     const URIParams& newUri,
                                     const uint32_t& redirectFlags,
                                     const nsHttpResponseHead& responseHead,
                                     const nsCString& securityInfoSerialization)
{
  // TODO: handle security info
  LOG(("HttpChannelChild::RecvRedirect1Begin [this=%p]\n", this));
  if (mEventQ->ShouldEnqueue()) {
    mEventQ->Enqueue(new Redirect1Event(this, newChannelId, newUri,
                                       redirectFlags, responseHead,
                                       securityInfoSerialization));
  } else {
    Redirect1Begin(newChannelId, newUri, redirectFlags, responseHead,
                   securityInfoSerialization);
  }
  return true;
}

nsresult
HttpChannelChild::SetupRedirect(nsIURI* uri,
                                const nsHttpResponseHead* responseHead,
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

  rv = SetupReplacementChannel(uri, newChannel, !rewriteToGET);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHttpChannelChild> httpChannelChild = do_QueryInterface(newChannel);
  if (mShouldInterceptSubsequentRedirect && httpChannelChild) {
    // In the case where there was a synthesized response that caused a redirection,
    // we must force the new channel to intercept the request in the parent before a
    // network transaction is initiated.
    httpChannelChild->ForceIntercepted();
  }

  mRedirectChannelChild = do_QueryInterface(newChannel);
  newChannel.forget(outChannel);

  return NS_OK;
}

void
HttpChannelChild::Redirect1Begin(const uint32_t& newChannelId,
                                 const URIParams& newUri,
                                 const uint32_t& redirectFlags,
                                 const nsHttpResponseHead& responseHead,
                                 const nsACString& securityInfoSerialization)
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
                              getter_AddRefs(newChannel));

  if (NS_SUCCEEDED(rv)) {
    if (mRedirectChannelChild) {
      mRedirectChannelChild->ConnectParent(newChannelId);
      rv = gHttpHandler->AsyncOnChannelRedirect(this,
                                                newChannel,
                                                redirectFlags);
    } else {
      LOG(("  redirecting to a protocol that doesn't implement"
           " nsIChildChannel"));
      rv = NS_ERROR_FAILURE;
    }
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
                              getter_AddRefs(newChannel));

  if (NS_SUCCEEDED(rv)) {
    rv = gHttpHandler->AsyncOnChannelRedirect(this,
                                              newChannel,
                                              nsIChannelEventSink::REDIRECT_INTERNAL);
  }

  if (NS_FAILED(rv))
    OnRedirectVerifyCallback(rv);
}

class Redirect3Event : public ChannelEvent
{
 public:
  explicit Redirect3Event(HttpChannelChild* child) : mChild(child) {}
  void Run() { mChild->Redirect3Complete(); }
 private:
  HttpChannelChild* mChild;
};

bool
HttpChannelChild::RecvRedirect3Complete()
{
  LOG(("HttpChannelChild::RecvRedirect3Complete [this=%p]\n", this));
  if (mEventQ->ShouldEnqueue()) {
    mEventQ->Enqueue(new Redirect3Event(this));
  } else {
    Redirect3Complete();
  }
  return true;
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
 private:
  HttpChannelChild* mChild;
};

bool
HttpChannelChild::RecvFlushedForDiversion()
{
  LOG(("HttpChannelChild::RecvFlushedForDiversion [this=%p]\n", this));
  MOZ_RELEASE_ASSERT(mDivertingToParent);
  MOZ_RELEASE_ASSERT(mEventQ->ShouldEnqueue());

  mEventQ->Enqueue(new HttpFlushedForDiversionEvent(this));

  return true;
}

bool
HttpChannelChild::RecvNotifyTrackingProtectionDisabled()
{
  nsChannelClassifier::NotifyTrackingProtectionDisabled(this);
  return true;
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

bool
HttpChannelChild::RecvDivertMessages()
{
  LOG(("HttpChannelChild::RecvDivertMessages [this=%p]\n", this));
  MOZ_RELEASE_ASSERT(mDivertingToParent);
  MOZ_RELEASE_ASSERT(mSuspendCount > 0);

  // DivertTo() has been called on parent, so we can now start sending queued
  // IPDL messages back to parent listener.
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(Resume()));

  return true;
}

void
HttpChannelChild::Redirect3Complete()
{
  LOG(("HttpChannelChild::Redirect3Complete [this=%p]\n", this));
  nsresult rv = NS_OK;

  // Chrome channel has been AsyncOpen'd.  Reflect this in child.
  if (mRedirectChannelChild)
    rv = mRedirectChannelChild->CompleteRedirectSetup(mListener,
                                                      mListenerContext);

  // Redirecting to new channel: shut this down and init new channel
  if (mLoadGroup)
    mLoadGroup->RemoveRequest(this, nullptr, NS_BINDING_ABORTED);

  if (NS_SUCCEEDED(rv)) {
    if (mLoadInfo) {
      mLoadInfo->AppendRedirectedPrincipal(GetURIPrincipal());
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
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIChildChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::ConnectParent(uint32_t id)
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

  // The socket transport in the chrome process now holds a logical ref to us
  // until OnStopRequest, or we do a redirect, or we hit an IPDL error.
  AddIPDLReference();

  HttpChannelConnectArgs connectArgs(id, mShouldParentIntercept);
  PBrowserOrId browser = static_cast<ContentChild*>(gNeckoChild->Manager())
                         ->GetBrowserOrId(tabChild);
  if (!gNeckoChild->
        SendPHttpChannelConstructor(this, browser,
                                    IPC::SerializedLoadContext(this),
                                    connectArgs)) {
    return NS_ERROR_FAILURE;
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
    PHttpChannelChild::Send__delete__(this);
    if (mLoadInfo && mLoadInfo->GetEnforceSecurity()) {
        MOZ_ASSERT(!aContext, "aContext should be null!");
        return AsyncOpen2(listener);
    }
    return AsyncOpen(listener, aContext);
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

class OverrideRunnable : public nsRunnable {
  RefPtr<HttpChannelChild> mChannel;
  RefPtr<HttpChannelChild> mNewChannel;
  RefPtr<InterceptStreamListener> mListener;
  nsCOMPtr<nsIInputStream> mInput;
  nsAutoPtr<nsHttpResponseHead> mHead;

public:
  OverrideRunnable(HttpChannelChild* aChannel,
                   HttpChannelChild* aNewChannel,
                   InterceptStreamListener* aListener,
                   nsIInputStream* aInput,
                   nsAutoPtr<nsHttpResponseHead>& aHead)
  : mChannel(aChannel)
  , mNewChannel(aNewChannel)
  , mListener(aListener)
  , mInput(aInput)
  , mHead(aHead)
  {
  }

  NS_IMETHOD Run() {
    mChannel->Redirect3Complete();
    mNewChannel->OverrideWithSynthesizedResponse(mHead, mInput, mListener);
    return NS_OK;
  }
};

NS_IMETHODIMP
HttpChannelChild::OnRedirectVerifyCallback(nsresult result)
{
  LOG(("HttpChannelChild::OnRedirectVerifyCallback [this=%p]\n", this));
  OptionalURIParams redirectURI;
  nsCOMPtr<nsIHttpChannel> newHttpChannel =
      do_QueryInterface(mRedirectChannelChild);

  if (newHttpChannel) {
    // Must not be called until after redirect observers called.
    newHttpChannel->SetOriginalURI(mOriginalURI);
  }

  if (mRedirectingForSubsequentSynthesizedResponse) {
    nsCOMPtr<nsIHttpChannelChild> httpChannelChild = do_QueryInterface(mRedirectChannelChild);
    MOZ_ASSERT(httpChannelChild);
    RefPtr<HttpChannelChild> redirectedChannel =
        static_cast<HttpChannelChild*>(httpChannelChild.get());

    RefPtr<InterceptStreamListener> streamListener =
        new InterceptStreamListener(redirectedChannel, mListenerContext);

    NS_DispatchToMainThread(new OverrideRunnable(this, redirectedChannel,
                                                 streamListener, mSynthesizedInput,
                                                 mResponseHead));
    return NS_OK;
  }

  RequestHeaderTuples emptyHeaders;
  RequestHeaderTuples* headerTuples = &emptyHeaders;
  nsLoadFlags loadFlags = 0;

  nsCOMPtr<nsIHttpChannelChild> newHttpChannelChild =
      do_QueryInterface(mRedirectChannelChild);
  if (newHttpChannelChild && NS_SUCCEEDED(result)) {
    newHttpChannelChild->AddCookiesToRequest();
    newHttpChannelChild->GetClientSetRequestHeaders(&headerTuples);
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

  if (mIPCOpen)
    SendRedirect2Verify(result, *headerTuples, loadFlags, redirectURI);

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
  LOG(("HttpChannelChild::Suspend [this=%p, mSuspendCount=%lu, "
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
  LOG(("HttpChannelChild::Resume [this=%p, mSuspendCount=%lu, "
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
      AsyncCall(mCallOnResume);
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
  MOZ_ASSERT(!mLoadInfo || mLoadInfo->GetSecurityMode() == 0 ||
             mLoadInfo->GetInitialSecurityCheckDone(),
             "security flags in loadInfo but asyncOpen2() not called");

  LOG(("HttpChannelChild::AsyncOpen [this=%p uri=%s]\n", this, mSpec.get()));

  if (mCanceled)
    return mStatus;

  NS_ENSURE_TRUE(gNeckoChild != nullptr, NS_ERROR_FAILURE);
  NS_ENSURE_ARG_POINTER(listener);
  NS_ENSURE_TRUE(!mIsPending, NS_ERROR_IN_PROGRESS);
  NS_ENSURE_TRUE(!mWasOpened, NS_ERROR_ALREADY_OPENED);

  mAsyncOpenTime = TimeStamp::Now();

  // Port checked in parent, but duplicate here so we can return with error
  // immediately
  nsresult rv;
  rv = NS_CheckPortSafety(mURI);
  if (NS_FAILED(rv))
    return rv;

  const char *cookieHeader = mRequestHead.PeekHeader(nsHttp::Cookie);
  if (cookieHeader) {
    mUserSetCookieHeader = cookieHeader;
  }

  AddCookiesToRequest();

  //
  // NOTE: From now on we must return NS_OK; all errors must be handled via
  // OnStart/OnStopRequest
  //

  // Note: this is where we would notify "http-on-modify-request" observers.
  // We have deliberately disabled this for child processes (see bug 806753)
  //
  // notify "http-on-modify-request" observers
  //CallOnModifyRequestObservers();

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
    AsyncAbort(mStatus);
    return NS_OK;
  }

  if (ShouldIntercept()) {
    mResponseCouldBeSynthesized = true;

    nsCOMPtr<nsINetworkInterceptController> controller;
    GetCallback(controller);

    mInterceptListener = new InterceptStreamListener(this, mListenerContext);

    RefPtr<InterceptedChannelContent> intercepted =
        new InterceptedChannelContent(this, controller, mInterceptListener);
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
  NS_ENSURE_SUCCESS(rv, rv);
  return AsyncOpen(listener, nullptr);
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
  openArgs.requestMethod() = mRequestHead.Method();

  nsTArray<mozilla::ipc::FileDescriptor> fds;
  SerializeInputStream(mUploadStream, openArgs.uploadStream(), fds);

  if (mResponseHead) {
    openArgs.synthesizedResponseHead() = *mResponseHead;
  } else {
    openArgs.synthesizedResponseHead() = mozilla::void_t();
  }

  nsCOMPtr<nsISerializable> secInfoSer = do_QueryInterface(mSecurityInfo);
  if (secInfoSer) {
    NS_SerializeToString(secInfoSer, openArgs.synthesizedSecurityInfoSerialization());
  }

  OptionalFileDescriptorSet optionalFDs;

  if (fds.IsEmpty()) {
    optionalFDs = mozilla::void_t();
  } else if (fds.Length() <= kMaxFileDescriptorsPerMessage) {
    optionalFDs = nsTArray<mozilla::ipc::FileDescriptor>();
    optionalFDs.get_ArrayOfFileDescriptor().SwapElements(fds);
  } else {
    MOZ_ASSERT(gNeckoChild->Manager());

    PFileDescriptorSetChild* fdSet =
      gNeckoChild->Manager()->SendPFileDescriptorSetConstructor(fds[0]);
    for (uint32_t i = 1; i < fds.Length(); ++i) {
      unused << fdSet->SendAddFileDescriptor(fds[i]);
    }

    optionalFDs = fdSet;
  }

  OptionalCorsPreflightArgs optionalCorsPreflightArgs;
  if (mRequireCORSPreflight) {
    CorsPreflightArgs args;
    args.withCredentials() = mWithCredentials;
    args.unsafeHeaders() = mUnsafeHeaders;
    nsresult rv = PrincipalToPrincipalInfo(mPreflightPrincipal, &args.preflightPrincipal());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    optionalCorsPreflightArgs = args;
  } else {
    optionalCorsPreflightArgs = mozilla::void_t();
  }

  nsCOMPtr<mozIThirdPartyUtil> util(do_GetService(THIRDPARTYUTIL_CONTRACTID));
  if (util) {
    bool thirdParty;
    nsresult rv = util->IsThirdPartyChannel(this, nullptr, &thirdParty);
    if (NS_FAILED(rv)) {
      // If we couldn't compute whether this is a third-party load, assume that
      // it is.
      thirdParty = true;
    }

    mThirdPartyFlags |= thirdParty ?
      nsIHttpChannelInternal::THIRD_PARTY_PARENT_IS_THIRD_PARTY :
      nsIHttpChannelInternal::THIRD_PARTY_PARENT_IS_SAME_PARTY;
    nsCOMPtr<nsIURI> uri;
    GetTopWindowURI(getter_AddRefs(uri));
  }

  SerializeURI(mTopWindowURI, openArgs.topWindowURI());

  openArgs.fds() = optionalFDs;

  openArgs.preflightArgs() = optionalCorsPreflightArgs;

  openArgs.uploadStreamHasHeaders() = mUploadStreamHasHeaders;
  openArgs.priority() = mPriority;
  openArgs.classOfService() = mClassOfService;
  openArgs.redirectionLimit() = mRedirectionLimit;
  openArgs.allowPipelining() = mAllowPipelining;
  openArgs.allowSTS() = mAllowSTS;
  openArgs.thirdPartyFlags() = mThirdPartyFlags;
  openArgs.resumeAt() = mSendResumeAt;
  openArgs.startPos() = mStartPos;
  openArgs.entityID() = mEntityID;
  openArgs.chooseApplicationCache() = mChooseApplicationCache;
  openArgs.appCacheClientID() = appCacheClientId;
  openArgs.allowSpdy() = mAllowSpdy;
  openArgs.allowAltSvc() = mAllowAltSvc;
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

  nsresult rv = mozilla::ipc::LoadInfoToLoadInfoArgs(mLoadInfo, &openArgs.loadInfo());
  NS_ENSURE_SUCCESS(rv, rv);

  EnsureSchedulingContextID();
  char scid[NSID_LENGTH];
  mSchedulingContextID.ToProvidedString(scid);
  openArgs.schedulingContextID().AssignASCII(scid);

  // The socket transport in the chrome process now holds a logical ref to us
  // until OnStopRequest, or we do a redirect, or we hit an IPDL error.
  AddIPDLReference();

  PBrowserOrId browser = static_cast<ContentChild*>(gNeckoChild->Manager())
                         ->GetBrowserOrId(tabChild);
  gNeckoChild->SendPHttpChannelConstructor(this, browser,
                                           IPC::SerializedLoadContext(this),
                                           openArgs);

  if (optionalFDs.type() ==
        OptionalFileDescriptorSet::TPFileDescriptorSetChild) {
    FileDescriptorSetChild* fdSetActor =
      static_cast<FileDescriptorSetChild*>(
        optionalFDs.get_PFileDescriptorSetChild());

    fdSetActor->ForgetFileDescriptors(fds);
  }

  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIHttpChannel
//-----------------------------------------------------------------------------

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

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIHttpChannelInternal
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::SetupFallbackChannel(const char *aFallbackKey)
{
  DROP_DEAD();
}

// The next four _should_ be implemented, but we need to figure out how
// to transfer the data from the chrome process first.

NS_IMETHODIMP
HttpChannelChild::GetRemoteAddress(nsACString & _result)
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
HttpChannelChild::GetRemotePort(int32_t * _result)
{
  NS_ENSURE_ARG_POINTER(_result);
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
HttpChannelChild::GetLocalAddress(nsACString & _result)
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
HttpChannelChild::GetLocalPort(int32_t * _result)
{
  NS_ENSURE_ARG_POINTER(_result);
  return NS_ERROR_NOT_AVAILABLE;
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

  // If we have a synthesized response, then there is no parent actor.  We
  // need to make this work somehow, but for now avoid crashing.  (bug 1220681)
  if (mSynthesizedResponse) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // We must fail DivertToParent() if there's no parent end of the channel (and
  // won't be!) due to early failure.
  if (NS_FAILED(mStatus) && !RemoteChannelExists()) {
    return mStatus;
  }

  nsresult rv = Suspend();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Once this is set, it should not be unset before the child is taken down.
  mDivertingToParent = true;

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
  NS_ENSURE_SUCCESS_VOID(rv);
}

NS_IMETHODIMP
HttpChannelChild::GetResponseSynthesized(bool* aSynthesized)
{
  NS_ENSURE_ARG_POINTER(aSynthesized);
  *aSynthesized = mSynthesizedResponse;
  return NS_OK;
}

void
HttpChannelChild::OverrideWithSynthesizedResponse(nsAutoPtr<nsHttpResponseHead>& aResponseHead,
                                                  nsIInputStream* aSynthesizedInput,
                                                  InterceptStreamListener* aStreamListener)
{
  mInterceptListener = aStreamListener;

  // Intercepted responses should already be decoded.  If its a redirect,
  // however, we want to respect the encoding of the final result instead.
  if (!WillRedirect(aResponseHead)) {
    SetApplyConversion(false);
  }

  mResponseHead = aResponseHead;
  mSynthesizedResponse = true;

  if (WillRedirect(mResponseHead)) {
    mShouldInterceptSubsequentRedirect = true;
    // Continue with the original cross-process request
    nsresult rv = ContinueAsyncOpen();
    NS_ENSURE_SUCCESS_VOID(rv);
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

  rv = nsInputStreamPump::Create(getter_AddRefs(mSynthesizedResponsePump),
                                 aSynthesizedInput,
                                 int64_t(-1), int64_t(-1), 0, 0, true);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aSynthesizedInput->Close();
    return;
  }

  rv = mSynthesizedResponsePump->AsyncRead(aStreamListener, nullptr);
  NS_ENSURE_SUCCESS_VOID(rv);

  // if this channel has been suspended previously, the pump needs to be
  // correspondingly suspended now that it exists.
  for (uint32_t i = 0; i < mSuspendCount; i++) {
    nsresult rv = mSynthesizedResponsePump->Suspend();
    NS_ENSURE_SUCCESS_VOID(rv);
  }

  if (mCanceled) {
    mSynthesizedResponsePump->Cancel(mStatus);
  }
}

NS_IMETHODIMP
HttpChannelChild::ForceIntercepted()
{
  mShouldParentIntercept = true;
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

bool
HttpChannelChild::RecvIssueDeprecationWarning(const uint32_t& warning,
                                              const bool& asError)
{
  nsCOMPtr<nsIDeprecationWarner> warner;
  GetCallback(warner);
  if (warner) {
    warner->IssueWarning(warning, asError);
  }
  return true;
}

} // namespace net
} // namespace mozilla
