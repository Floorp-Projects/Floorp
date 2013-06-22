/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "nsHttp.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/net/HttpChannelChild.h"

#include "nsStringStream.h"
#include "nsHttpHandler.h"
#include "nsMimeTypes.h"
#include "nsNetUtil.h"
#include "nsSerializationHelper.h"
#include "base/compiler_specific.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/net/DNS.h"

using namespace mozilla::dom;
using namespace mozilla::ipc;

namespace mozilla {
namespace net {

//-----------------------------------------------------------------------------
// HttpChannelChild
//-----------------------------------------------------------------------------

HttpChannelChild::HttpChannelChild()
  : ALLOW_THIS_IN_INITIALIZER_LIST(HttpAsyncAborter<HttpChannelChild>(this))
  , mIsFromCache(false)
  , mCacheEntryAvailable(false)
  , mCacheExpirationTime(nsICache::NO_EXPIRATION_TIME)
  , mSendResumeAt(false)
  , mIPCOpen(false)
  , mKeptAlive(false)
{
  LOG(("Creating HttpChannelChild @%x\n", this));

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

NS_IMETHODIMP_(nsrefcnt) HttpChannelChild::Release()
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
  NS_INTERFACE_MAP_ENTRY(nsIProxiedChannel)
  NS_INTERFACE_MAP_ENTRY(nsITraceableChannel)
  NS_INTERFACE_MAP_ENTRY(nsIApplicationCacheContainer)
  NS_INTERFACE_MAP_ENTRY(nsIApplicationCacheChannel)
  NS_INTERFACE_MAP_ENTRY(nsIAsyncVerifyRedirectCallback)
  NS_INTERFACE_MAP_ENTRY(nsIChildChannel)
  NS_INTERFACE_MAP_ENTRY(nsIHttpChannelChild)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIAssociatedContentSecurity, GetAssociatedContentSecurity())
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
                    const nsHttpResponseHead& responseHead,
                    const bool& useResponseHead,
                    const nsHttpHeaderArray& requestHeaders,
                    const bool& isFromCache,
                    const bool& cacheEntryAvailable,
                    const uint32_t& cacheExpirationTime,
                    const nsCString& cachedCharset,
                    const nsCString& securityInfoSerialization,
                    const NetAddr& selfAddr,
                    const NetAddr& peerAddr)
  : mChild(child)
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
  {}

  void Run()
  {
    mChild->OnStartRequest(mResponseHead, mUseResponseHead, mRequestHeaders,
                           mIsFromCache, mCacheEntryAvailable,
                           mCacheExpirationTime, mCachedCharset,
                           mSecurityInfoSerialization, mSelfAddr, mPeerAddr);
  }
 private:
  HttpChannelChild* mChild;
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
};

bool
HttpChannelChild::RecvOnStartRequest(const nsHttpResponseHead& responseHead,
                                     const bool& useResponseHead,
                                     const nsHttpHeaderArray& requestHeaders,
                                     const bool& isFromCache,
                                     const bool& cacheEntryAvailable,
                                     const uint32_t& cacheExpirationTime,
                                     const nsCString& cachedCharset,
                                     const nsCString& securityInfoSerialization,
                                     const NetAddr& selfAddr,
                                     const NetAddr& peerAddr)
{
  if (mEventQ->ShouldEnqueue()) {
    mEventQ->Enqueue(new StartRequestEvent(this, responseHead, useResponseHead,
                                          requestHeaders, isFromCache,
                                          cacheEntryAvailable,
                                          cacheExpirationTime, cachedCharset,
                                          securityInfoSerialization, selfAddr,
                                          peerAddr));
  } else {
    OnStartRequest(responseHead, useResponseHead, requestHeaders, isFromCache,
                   cacheEntryAvailable, cacheExpirationTime, cachedCharset,
                   securityInfoSerialization, selfAddr, peerAddr);
  }
  return true;
}

void
HttpChannelChild::OnStartRequest(const nsHttpResponseHead& responseHead,
                                 const bool& useResponseHead,
                                 const nsHttpHeaderArray& requestHeaders,
                                 const bool& isFromCache,
                                 const bool& cacheEntryAvailable,
                                 const uint32_t& cacheExpirationTime,
                                 const nsCString& cachedCharset,
                                 const nsCString& securityInfoSerialization,
                                 const NetAddr& selfAddr,
                                 const NetAddr& peerAddr)
{
  LOG(("HttpChannelChild::RecvOnStartRequest [this=%p]\n", this));

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

  // replace our request headers with what actually got sent in the parent
  mRequestHead.Headers() = requestHeaders;

  // Note: this is where we would notify "http-on-examine-response" observers.
  // We have deliberately disabled this for child processes (see bug 806753)
  //
  // gHttpHandler->OnExamineResponse(this);

  mTracingEnabled = false;

  nsresult rv = mListener->OnStartRequest(this, mListenerContext);
  if (NS_FAILED(rv)) {
    Cancel(rv);
    return;
  }

  if (mResponseHead)
    SetCookie(mResponseHead->PeekHeader(nsHttp::Set_Cookie));

  rv = ApplyContentConversions();
  if (NS_FAILED(rv))
    Cancel(rv);

  mSelfAddr = selfAddr;
  mPeerAddr = peerAddr;
}

class TransportAndDataEvent : public ChannelEvent
{
 public:
  TransportAndDataEvent(HttpChannelChild* child,
                        const nsresult& status,
                        const uint64_t& progress,
                        const uint64_t& progressMax,
                        const nsCString& data,
                        const uint64_t& offset,
                        const uint32_t& count)
  : mChild(child)
  , mStatus(status)
  , mProgress(progress)
  , mProgressMax(progressMax)
  , mData(data)
  , mOffset(offset)
  , mCount(count) {}

  void Run() { mChild->OnTransportAndData(mStatus, mProgress, mProgressMax,
                                          mData, mOffset, mCount); }
 private:
  HttpChannelChild* mChild;
  nsresult mStatus;
  uint64_t mProgress;
  uint64_t mProgressMax;
  nsCString mData;
  uint64_t mOffset;
  uint32_t mCount;
};

bool
HttpChannelChild::RecvOnTransportAndData(const nsresult& status,
                                         const uint64_t& progress,
                                         const uint64_t& progressMax,
                                         const nsCString& data,
                                         const uint64_t& offset,
                                         const uint32_t& count)
{
  if (mEventQ->ShouldEnqueue()) {
    mEventQ->Enqueue(new TransportAndDataEvent(this, status, progress,
                                              progressMax, data, offset,
                                              count));
  } else {
    OnTransportAndData(status, progress, progressMax, data, offset, count);
  }
  return true;
}

void
HttpChannelChild::OnTransportAndData(const nsresult& status,
                                     const uint64_t progress,
                                     const uint64_t& progressMax,
                                     const nsCString& data,
                                     const uint64_t& offset,
                                     const uint32_t& count)
{
  LOG(("HttpChannelChild::OnTransportAndData [this=%p]\n", this));

  if (mCanceled)
    return;

  // cache the progress sink so we don't have to query for it each time.
  if (!mProgressSink)
    GetCallback(mProgressSink);

  // Hold queue lock throughout all three calls, else we might process a later
  // necko msg in between them.
  AutoEventEnqueuer ensureSerialDispatch(mEventQ);

  // block status/progress after Cancel or OnStopRequest has been called,
  // or if channel has LOAD_BACKGROUND set.
  // - JDUELL: may not need mStatus/mIsPending checks, given this is always called
  //   during OnDataAvailable, and we've already checked mCanceled.  Code
  //   dupe'd from nsHttpChannel
  if (mProgressSink && NS_SUCCEEDED(mStatus) && mIsPending &&
      !(mLoadFlags & LOAD_BACKGROUND))
  {
    // OnStatus
    //
    MOZ_ASSERT(status == NS_NET_STATUS_RECEIVING_FROM ||
               status == NS_NET_STATUS_READING);

    nsAutoCString host;
    mURI->GetHost(host);
    mProgressSink->OnStatus(this, nullptr, status,
                            NS_ConvertUTF8toUTF16(host).get());
    // OnProgress
    //
    if (progress > 0) {
      MOZ_ASSERT(progress <= progressMax, "unexpected progress values");
      mProgressSink->OnProgress(this, nullptr, progress, progressMax);
    }
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

  rv = mListener->OnDataAvailable(this, mListenerContext,
                                  stringStream, offset, count);
  stringStream->Close();
  if (NS_FAILED(rv)) {
    Cancel(rv);
  }
}

class StopRequestEvent : public ChannelEvent
{
 public:
  StopRequestEvent(HttpChannelChild* child,
                   const nsresult& statusCode)
  : mChild(child)
  , mStatusCode(statusCode) {}

  void Run() { mChild->OnStopRequest(mStatusCode); }
 private:
  HttpChannelChild* mChild;
  nsresult mStatusCode;
};

bool
HttpChannelChild::RecvOnStopRequest(const nsresult& statusCode)
{
  if (mEventQ->ShouldEnqueue()) {
    mEventQ->Enqueue(new StopRequestEvent(this, statusCode));
  } else {
    OnStopRequest(statusCode);
  }
  return true;
}

void
HttpChannelChild::OnStopRequest(const nsresult& statusCode)
{
  LOG(("HttpChannelChild::OnStopRequest [this=%p status=%x]\n",
           this, statusCode));

  mIsPending = false;

  if (!mCanceled && NS_SUCCEEDED(mStatus))
    mStatus = statusCode;

  { // We must flush the queue before we Send__delete__
    // (although we really shouldn't receive any msgs after OnStop),
    // so make sure this goes out of scope before then.
    AutoEventEnqueuer ensureSerialDispatch(mEventQ);

    mListener->OnStopRequest(this, mListenerContext, mStatus);

    mListener = 0;
    mListenerContext = 0;
    mCacheEntryAvailable = false;
    if (mLoadGroup)
      mLoadGroup->RemoveRequest(this, nullptr, mStatus);
  }

  if (mLoadFlags & LOAD_DOCUMENT_URI) {
    // Keep IPDL channel open, but only for updating security info.
    mKeptAlive = true;
    SendDocumentChannelCleanup();
  } else {
    // This calls NeckoChild::DeallocPHttpChannel(), which deletes |this| if IPDL
    // holds the last reference.  Don't rely on |this| existing after here.
    PHttpChannelChild::Send__delete__(this);
  }
}

class ProgressEvent : public ChannelEvent
{
 public:
  ProgressEvent(HttpChannelChild* child,
                const uint64_t& progress,
                const uint64_t& progressMax)
  : mChild(child)
  , mProgress(progress)
  , mProgressMax(progressMax) {}

  void Run() { mChild->OnProgress(mProgress, mProgressMax); }
 private:
  HttpChannelChild* mChild;
  uint64_t mProgress, mProgressMax;
};

bool
HttpChannelChild::RecvOnProgress(const uint64_t& progress,
                                 const uint64_t& progressMax)
{
  if (mEventQ->ShouldEnqueue())  {
    mEventQ->Enqueue(new ProgressEvent(this, progress, progressMax));
  } else {
    OnProgress(progress, progressMax);
  }
  return true;
}

void
HttpChannelChild::OnProgress(const uint64_t& progress,
                             const uint64_t& progressMax)
{
  LOG(("HttpChannelChild::OnProgress [this=%p progress=%llu/%llu]\n",
       this, progress, progressMax));

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
    if (progress > 0) {
      MOZ_ASSERT(progress <= progressMax, "unexpected progress values");
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
  mIsPending = false;
  // We're already being called from IPDL, therefore already "async"
  HandleAsyncAbort();
}

void
HttpChannelChild::DoNotifyListenerCleanup()
{
  if (mIPCOpen)
    PHttpChannelChild::Send__delete__(this);
}

class DeleteSelfEvent : public ChannelEvent
{
 public:
  DeleteSelfEvent(HttpChannelChild* child) : mChild(child) {}
  void Run() { mChild->DeleteSelf(); }
 private:
  HttpChannelChild* mChild;
};

bool
HttpChannelChild::RecvDeleteSelf()
{
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

class Redirect1Event : public ChannelEvent
{
 public:
  Redirect1Event(HttpChannelChild* child,
                 const uint32_t& newChannelId,
                 const URIParams& newURI,
                 const uint32_t& redirectFlags,
                 const nsHttpResponseHead& responseHead)
  : mChild(child)
  , mNewChannelId(newChannelId)
  , mNewURI(newURI)
  , mRedirectFlags(redirectFlags)
  , mResponseHead(responseHead) {}

  void Run()
  {
    mChild->Redirect1Begin(mNewChannelId, mNewURI, mRedirectFlags,
                           mResponseHead);
  }
 private:
  HttpChannelChild*   mChild;
  uint32_t            mNewChannelId;
  URIParams           mNewURI;
  uint32_t            mRedirectFlags;
  nsHttpResponseHead  mResponseHead;
};

bool
HttpChannelChild::RecvRedirect1Begin(const uint32_t& newChannelId,
                                     const URIParams& newUri,
                                     const uint32_t& redirectFlags,
                                     const nsHttpResponseHead& responseHead)
{
  if (mEventQ->ShouldEnqueue()) {
    mEventQ->Enqueue(new Redirect1Event(this, newChannelId, newUri,
                                       redirectFlags, responseHead));
  } else {
    Redirect1Begin(newChannelId, newUri, redirectFlags, responseHead);
  }
  return true;
}

void
HttpChannelChild::Redirect1Begin(const uint32_t& newChannelId,
                                 const URIParams& newUri,
                                 const uint32_t& redirectFlags,
                                 const nsHttpResponseHead& responseHead)
{
  nsresult rv;

  nsCOMPtr<nsIIOService> ioService;
  rv = gHttpHandler->GetIOService(getter_AddRefs(ioService));
  if (NS_FAILED(rv)) {
    // Veto redirect.  nsHttpChannel decides to cancel or continue.
    OnRedirectVerifyCallback(rv);
    return;
  }

  nsCOMPtr<nsIURI> uri = DeserializeURI(newUri);

  nsCOMPtr<nsIChannel> newChannel;
  rv = ioService->NewChannelFromURI(uri, getter_AddRefs(newChannel));
  if (NS_FAILED(rv)) {
    // Veto redirect.  nsHttpChannel decides to cancel or continue.
    OnRedirectVerifyCallback(rv);
    return;
  }

  // We won't get OnStartRequest, set cookies here.
  mResponseHead = new nsHttpResponseHead(responseHead);
  SetCookie(mResponseHead->PeekHeader(nsHttp::Set_Cookie));

  bool rewriteToGET = nsHttp::ShouldRewriteRedirectToGET(
                               mResponseHead->Status(), mRequestHead.Method());

  rv = SetupReplacementChannel(uri, newChannel, !rewriteToGET);
  if (NS_FAILED(rv)) {
    // Veto redirect.  nsHttpChannel decides to cancel or continue.
    OnRedirectVerifyCallback(rv);
    return;
  }

  mRedirectChannelChild = do_QueryInterface(newChannel);
  if (mRedirectChannelChild) {
    mRedirectChannelChild->ConnectParent(newChannelId);
  } else {
    NS_ERROR("Redirecting to a protocol that doesn't support universal protocol redirect");
  }

  rv = gHttpHandler->AsyncOnChannelRedirect(this,
                                            newChannel,
                                            redirectFlags);
  if (NS_FAILED(rv))
    OnRedirectVerifyCallback(rv);
}

class Redirect3Event : public ChannelEvent
{
 public:
  Redirect3Event(HttpChannelChild* child) : mChild(child) {}
  void Run() { mChild->Redirect3Complete(); }
 private:
  HttpChannelChild* mChild;
};

bool
HttpChannelChild::RecvRedirect3Complete()
{
  if (mEventQ->ShouldEnqueue()) {
    mEventQ->Enqueue(new Redirect3Event(this));
  } else {
    Redirect3Complete();
  }
  return true;
}

void
HttpChannelChild::Redirect3Complete()
{
  nsresult rv = NS_OK;

  // Chrome channel has been AsyncOpen'd.  Reflect this in child.
  if (mRedirectChannelChild)
    rv = mRedirectChannelChild->CompleteRedirectSetup(mListener,
                                                      mListenerContext);

  // Redirecting to new channel: shut this down and init new channel
  if (mLoadGroup)
    mLoadGroup->RemoveRequest(this, nullptr, NS_BINDING_ABORTED);

  if (NS_FAILED(rv))
    NS_WARNING("CompleteRedirectSetup failed, HttpChannelChild already open?");

  // Release ref to new channel.
  mRedirectChannelChild = nullptr;
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIChildChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::ConnectParent(uint32_t id)
{
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

  if (!gNeckoChild->SendPHttpChannelConstructor(
                      this, tabChild, IPC::SerializedLoadContext(this))) {
    return NS_ERROR_FAILURE;
  }

  if (!SendConnectChannel(id))
    return NS_ERROR_FAILURE;

  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::CompleteRedirectSetup(nsIStreamListener *listener,
                                        nsISupports *aContext)
{
  LOG(("HttpChannelChild::FinishRedirectSetup [this=%p]\n", this));

  NS_ENSURE_TRUE(!mIsPending, NS_ERROR_IN_PROGRESS);
  NS_ENSURE_TRUE(!mWasOpened, NS_ERROR_ALREADY_OPENED);

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
  OptionalURIParams redirectURI;
  nsCOMPtr<nsIHttpChannel> newHttpChannel =
      do_QueryInterface(mRedirectChannelChild);

  if (newHttpChannel) {
    // Must not be called until after redirect observers called.
    newHttpChannel->SetOriginalURI(mOriginalURI);
  }

  RequestHeaderTuples emptyHeaders;
  RequestHeaderTuples* headerTuples = &emptyHeaders;

  nsCOMPtr<nsIHttpChannelChild> newHttpChannelChild =
      do_QueryInterface(mRedirectChannelChild);
  if (newHttpChannelChild && NS_SUCCEEDED(result)) {
    newHttpChannelChild->AddCookiesToRequest();
    newHttpChannelChild->GetClientSetRequestHeaders(&headerTuples);
  }

  if (NS_SUCCEEDED(result)) {
    // we know this is an HttpChannelChild
    HttpChannelChild* base =
      static_cast<HttpChannelChild*>(mRedirectChannelChild.get());
    // Note: this is where we would notify "http-on-modify-response" observers.
    // We have deliberately disabled this for child processes (see bug 806753)
    //
    // After we verify redirect, nsHttpChannel may hit the network: must give
    // "http-on-modify-request" observers the chance to cancel before that.
    //base->CallOnModifyRequestObservers();

    /* If there was an API redirect of this redirect, we need to send it
     * down here, since it can't get sent via SendAsyncOpen. */
    SerializeURI(base->mAPIRedirectToURI, redirectURI);
  } else {
    /* If the redirect was canceled, bypass OMR and send an empty API
     * redirect URI */
    SerializeURI(nullptr, redirectURI);
  }

  if (mIPCOpen)
    SendRedirect2Verify(result, *headerTuples, redirectURI);

  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIRequest
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::Cancel(nsresult status)
{
  if (!mCanceled) {
    // If this cancel occurs before nsHttpChannel has been set up, AsyncOpen
    // is responsible for cleaning up.
    mCanceled = true;
    mStatus = status;
    if (RemoteChannelExists())
      SendCancel(status);
  }
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::Suspend()
{
  NS_ENSURE_TRUE(RemoteChannelExists(), NS_ERROR_NOT_AVAILABLE);
  if (!mSuspendCount++) {
    SendSuspend();
  }
  mEventQ->Suspend();

  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::Resume()
{
  NS_ENSURE_TRUE(RemoteChannelExists(), NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(mSuspendCount > 0, NS_ERROR_UNEXPECTED);

  nsresult rv = NS_OK;

  if (!--mSuspendCount) {
    SendResume();
    if (mCallOnResume) {
      AsyncCall(mCallOnResume);
      mCallOnResume = nullptr;
    }
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
  LOG(("HttpChannelChild::AsyncOpen [this=%p uri=%s]\n", this, mSpec.get()));

  if (mCanceled)
    return mStatus;

  NS_ENSURE_TRUE(gNeckoChild != nullptr, NS_ERROR_FAILURE);
  NS_ENSURE_ARG_POINTER(listener);
  NS_ENSURE_TRUE(!mIsPending, NS_ERROR_IN_PROGRESS);
  NS_ENSURE_TRUE(!mWasOpened, NS_ERROR_ALREADY_OPENED);

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

  nsCString appCacheClientId;
  if (mInheritApplicationCache) {
    // Pick up an application cache from the notification
    // callbacks if available
    nsCOMPtr<nsIApplicationCacheContainer> appCacheContainer;
    GetCallback(appCacheContainer);

    if (appCacheContainer) {
      nsCOMPtr<nsIApplicationCache> appCache;
      rv = appCacheContainer->GetApplicationCache(getter_AddRefs(appCache));
      if (NS_SUCCEEDED(rv) && appCache) {
        appCache->GetClientID(appCacheClientId);
      }
    }
  }

  //
  // Send request to the chrome process...
  //

  // FIXME: bug 558623: Combine constructor and SendAsyncOpen into one IPC msg

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

  gNeckoChild->SendPHttpChannelConstructor(this, tabChild,
                                           IPC::SerializedLoadContext(this));

  URIParams uri;
  SerializeURI(mURI, uri);

  OptionalURIParams originalURI, documentURI, referrer, redirectURI;
  SerializeURI(mOriginalURI, originalURI);
  SerializeURI(mDocumentURI, documentURI);
  SerializeURI(mReferrer, referrer);
  SerializeURI(mAPIRedirectToURI, redirectURI);

  OptionalInputStreamParams uploadStream;
  SerializeInputStream(mUploadStream, uploadStream);

  SendAsyncOpen(uri, originalURI, documentURI, referrer, redirectURI, mLoadFlags,
                mClientSetRequestHeaders, mRequestHead.Method(), uploadStream,
                mUploadStreamHasHeaders, mPriority, mRedirectionLimit,
                mAllowPipelining, mForceAllowThirdPartyCookie, mSendResumeAt,
                mStartPos, mEntityID, mChooseApplicationCache,
                appCacheClientId, mAllowSpdy);

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
  nsresult rv = HttpBaseChannel::SetRequestHeader(aHeader, aValue, aMerge);
  if (NS_FAILED(rv))
    return rv;

  RequestHeaderTuple* tuple = mClientSetRequestHeaders.AppendElement();
  if (!tuple)
    return NS_ERROR_OUT_OF_MEMORY;

  tuple->mHeader = aHeader;
  tuple->mValue = aValue;
  tuple->mMerge = aMerge;
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

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIResumableChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::ResumeAt(uint64_t startPos, const nsACString& entityID)
{
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

/* attribute unsigned long countSubRequestsBrokenSecurity; */
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

/* attribute unsigned long countSubRequestsNoSecurity; */
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

//------------------------------------------------------------------------------

}} // mozilla::net
