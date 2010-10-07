/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *  The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jason Duell <jduell.mcbugs@gmail.com>
 *   Daniel Witte <dwitte@mozilla.com>
 *   Honza Bambas <honzab@firemni.cz>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsHttp.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/net/HttpChannelChild.h"

#include "nsStringStream.h"
#include "nsHttpHandler.h"
#include "nsMimeTypes.h"
#include "nsNetUtil.h"
#include "nsSerializationHelper.h"

namespace mozilla {
namespace net {

class ChildChannelEvent
{
 public:
  ChildChannelEvent() { MOZ_COUNT_CTOR(Callback); }
  virtual ~ChildChannelEvent() { MOZ_COUNT_DTOR(Callback); }
  virtual void Run() = 0;
};

// Ensures any incoming IPDL msgs are queued during its lifetime, and flushes
// the queue when it goes out of scope.
class AutoEventEnqueuer 
{
public:
  AutoEventEnqueuer(HttpChannelChild* channel) : mChannel(channel) 
  {
    mChannel->BeginEventQueueing();
  }
  ~AutoEventEnqueuer() 
  { 
    mChannel->EndEventQueueing();
    mChannel->FlushEventQueue(); 
  }
private:
    HttpChannelChild *mChannel;
};


//-----------------------------------------------------------------------------
// HttpChannelChild
//-----------------------------------------------------------------------------

HttpChannelChild::HttpChannelChild()
  : mIsFromCache(PR_FALSE)
  , mCacheEntryAvailable(PR_FALSE)
  , mCacheExpirationTime(nsICache::NO_EXPIRATION_TIME)
  , mSendResumeAt(false)
  , mSuspendCount(0)
  , mIPCOpen(false)
  , mKeptAlive(false)
  , mQueuePhase(PHASE_UNQUEUED)
{
  LOG(("Creating HttpChannelChild @%x\n", this));
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

  if (mRefCnt == 1 && mKeptAlive && mIPCOpen) {
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
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIAssociatedContentSecurity, GetAssociatedContentSecurity())
NS_INTERFACE_MAP_END_INHERITING(HttpBaseChannel)

//-----------------------------------------------------------------------------
// HttpChannelChild::PHttpChannelChild
//-----------------------------------------------------------------------------

void
HttpChannelChild::AddIPDLReference()
{
  NS_ABORT_IF_FALSE(!mIPCOpen, "Attempt to retain more than one IPDL reference");
  mIPCOpen = true;
  AddRef();
}

void
HttpChannelChild::ReleaseIPDLReference()
{
  NS_ABORT_IF_FALSE(mIPCOpen, "Attempt to release nonexistent IPDL reference");
  mIPCOpen = false;
  Release();
}

void
HttpChannelChild::FlushEventQueue()
{
  NS_ABORT_IF_FALSE(mQueuePhase != PHASE_UNQUEUED,
                    "Queue flushing should not occur if PHASE_UNQUEUED");
  
  // Queue already being flushed, or the channel's suspended.
  if (mQueuePhase != PHASE_FINISHED_QUEUEING || mSuspendCount)
    return;
  
  if (mEventQueue.Length() > 0) {
    // It is possible for new callbacks to be enqueued as we are
    // flushing the queue, so the queue must not be cleared until
    // all callbacks have run.
    mQueuePhase = PHASE_FLUSHING;
    
    nsRefPtr<HttpChannelChild> kungFuDeathGrip(this);
    PRUint32 i;
    for (i = 0; i < mEventQueue.Length(); i++) {
      mEventQueue[i]->Run();
      // If the callback ended up suspending us, abort all further flushing.
      if (mSuspendCount)
        break;
    }
    // We will always want to remove at least one finished callback.
    if (i < mEventQueue.Length())
      i++;

    mEventQueue.RemoveElementsAt(0, i);
  }

  if (mSuspendCount)
    mQueuePhase = PHASE_QUEUEING;
  else
    mQueuePhase = PHASE_UNQUEUED;
}

class StartRequestEvent : public ChildChannelEvent
{
 public:
  StartRequestEvent(HttpChannelChild* child,
                    const nsHttpResponseHead& responseHead,
                    const PRBool& useResponseHead,
                    const RequestHeaderTuples& requestHeaders,
                    const PRBool& isFromCache,
                    const PRBool& cacheEntryAvailable,
                    const PRUint32& cacheExpirationTime,
                    const nsCString& cachedCharset,
                    const nsCString& securityInfoSerialization)
  : mChild(child)
  , mResponseHead(responseHead)
  , mRequestHeaders(requestHeaders)
  , mUseResponseHead(useResponseHead)
  , mIsFromCache(isFromCache)
  , mCacheEntryAvailable(cacheEntryAvailable)
  , mCacheExpirationTime(cacheExpirationTime)
  , mCachedCharset(cachedCharset)
  , mSecurityInfoSerialization(securityInfoSerialization)
  {}

  void Run() 
  { 
    mChild->OnStartRequest(mResponseHead, mUseResponseHead, mRequestHeaders,
                           mIsFromCache, mCacheEntryAvailable,
                           mCacheExpirationTime, mCachedCharset,
                           mSecurityInfoSerialization);
  }
 private:
  HttpChannelChild* mChild;
  nsHttpResponseHead mResponseHead;
  RequestHeaderTuples mRequestHeaders;
  PRPackedBool mUseResponseHead;
  PRPackedBool mIsFromCache;
  PRPackedBool mCacheEntryAvailable;
  PRUint32 mCacheExpirationTime;
  nsCString mCachedCharset;
  nsCString mSecurityInfoSerialization;
};

bool 
HttpChannelChild::RecvOnStartRequest(const nsHttpResponseHead& responseHead,
                                     const PRBool& useResponseHead,
                                     const RequestHeaderTuples& requestHeaders,
                                     const PRBool& isFromCache,
                                     const PRBool& cacheEntryAvailable,
                                     const PRUint32& cacheExpirationTime,
                                     const nsCString& cachedCharset,
                                     const nsCString& securityInfoSerialization)
{
  if (ShouldEnqueue()) {
    EnqueueEvent(new StartRequestEvent(this, responseHead, useResponseHead,
                                       requestHeaders,
                                       isFromCache, cacheEntryAvailable,
                                       cacheExpirationTime, cachedCharset,
                                       securityInfoSerialization));
  } else {
    OnStartRequest(responseHead, useResponseHead, requestHeaders, isFromCache,
                   cacheEntryAvailable, cacheExpirationTime, cachedCharset,
                   securityInfoSerialization);
  }
  return true;
}

void 
HttpChannelChild::OnStartRequest(const nsHttpResponseHead& responseHead,
                                 const PRBool& useResponseHead,
                                 const RequestHeaderTuples& requestHeaders,
                                 const PRBool& isFromCache,
                                 const PRBool& cacheEntryAvailable,
                                 const PRUint32& cacheExpirationTime,
                                 const nsCString& cachedCharset,
                                 const nsCString& securityInfoSerialization)
{
  LOG(("HttpChannelChild::RecvOnStartRequest [this=%x]\n", this));

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

  AutoEventEnqueuer ensureSerialDispatch(this);

  // replace our request headers with what actually got sent in the parent
  mRequestHead.ClearHeaders();
  for (PRUint32 i = 0; i < requestHeaders.Length(); i++) {
    mRequestHead.Headers().SetHeader(nsHttp::ResolveAtom(requestHeaders[i].mHeader),
                                     requestHeaders[i].mValue);
  }

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
}

class DataAvailableEvent : public ChildChannelEvent
{
 public:
  DataAvailableEvent(HttpChannelChild* child,
                     const nsCString& data,
                     const PRUint32& offset,
                     const PRUint32& count)
  : mChild(child)
  , mData(data)
  , mOffset(offset)
  , mCount(count) {}

  void Run() { mChild->OnDataAvailable(mData, mOffset, mCount); }
 private:
  HttpChannelChild* mChild;
  nsCString mData;
  PRUint32 mOffset;
  PRUint32 mCount;
};

bool 
HttpChannelChild::RecvOnDataAvailable(const nsCString& data,
                                      const PRUint32& offset,
                                      const PRUint32& count)
{
  if (ShouldEnqueue()) {
    EnqueueEvent(new DataAvailableEvent(this, data, offset, count));
  } else {
    OnDataAvailable(data, offset, count);
  }
  return true;
}

void 
HttpChannelChild::OnDataAvailable(const nsCString& data,
                                  const PRUint32& offset,
                                  const PRUint32& count)
{
  LOG(("HttpChannelChild::OnDataAvailable [this=%x]\n", this));

  if (mCanceled)
    return;

  // NOTE: the OnDataAvailable contract requires the client to read all the data
  // in the inputstream.  This code relies on that ('data' will go away after
  // this function).  Apparently the previous, non-e10s behavior was to actually
  // support only reading part of the data, allowing later calls to read the
  // rest.
  nsCOMPtr<nsIInputStream> stringStream;
  nsresult rv = NS_NewByteInputStream(getter_AddRefs(stringStream),
                                      data.get(),
                                      count,
                                      NS_ASSIGNMENT_DEPEND);
  if (NS_FAILED(rv)) {
    Cancel(rv);
    return;
  }

  AutoEventEnqueuer ensureSerialDispatch(this);

  rv = mListener->OnDataAvailable(this, mListenerContext,
                                  stringStream, offset, count);
  stringStream->Close();
  if (NS_FAILED(rv)) {
    Cancel(rv);
  }
}

class StopRequestEvent : public ChildChannelEvent
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
  if (ShouldEnqueue()) {
    EnqueueEvent(new StopRequestEvent(this, statusCode));
  } else {
    OnStopRequest(statusCode);
  }
  return true;
}

void 
HttpChannelChild::OnStopRequest(const nsresult& statusCode)
{
  LOG(("HttpChannelChild::OnStopRequest [this=%x status=%u]\n", 
           this, statusCode));

  mIsPending = PR_FALSE;

  if (!mCanceled)
    mStatus = statusCode;

  { // We must flush the queue before we Send__delete__
    // (although we really shouldn't receive any msgs after OnStop),
    // so make sure this goes out of scope before then.
    AutoEventEnqueuer ensureSerialDispatch(this);

    mListener->OnStopRequest(this, mListenerContext, statusCode);

    mListener = 0;
    mListenerContext = 0;
    mCacheEntryAvailable = PR_FALSE;
    if (mLoadGroup)
      mLoadGroup->RemoveRequest(this, nsnull, statusCode);
  }

  if (!(mLoadFlags & LOAD_DOCUMENT_URI)) {
    // This calls NeckoChild::DeallocPHttpChannel(), which deletes |this| if IPDL
    // holds the last reference.  Don't rely on |this| existing after here.
    PHttpChannelChild::Send__delete__(this);
  } else {
    // We need to keep the document loading channel alive for further 
    // communication, mainly for collecting a security state values.
    mKeptAlive = true;
    SendDocumentChannelCleanup();
  }
}

class ProgressEvent : public ChildChannelEvent
{
 public:
  ProgressEvent(HttpChannelChild* child,
                const PRUint64& progress,
                const PRUint64& progressMax)
  : mChild(child)
  , mProgress(progress)
  , mProgressMax(progressMax) {}

  void Run() { mChild->OnProgress(mProgress, mProgressMax); }
 private:
  HttpChannelChild* mChild;
  PRUint64 mProgress, mProgressMax;
};

bool
HttpChannelChild::RecvOnProgress(const PRUint64& progress,
                                 const PRUint64& progressMax)
{
  if (ShouldEnqueue())  {
    EnqueueEvent(new ProgressEvent(this, progress, progressMax));
  } else {
    OnProgress(progress, progressMax);
  }
  return true;
}

void
HttpChannelChild::OnProgress(const PRUint64& progress,
                             const PRUint64& progressMax)
{
  LOG(("HttpChannelChild::OnProgress [this=%p progress=%llu/%llu]\n",
       this, progress, progressMax));

  if (mCanceled)
    return;

  // cache the progress sink so we don't have to query for it each time.
  if (!mProgressSink)
    GetCallback(mProgressSink);

  AutoEventEnqueuer ensureSerialDispatch(this);

  // block socket status event after Cancel or OnStopRequest has been called.
  if (mProgressSink && NS_SUCCEEDED(mStatus) && mIsPending && 
      !(mLoadFlags & LOAD_BACKGROUND)) 
  {
    if (progress > 0) {
      NS_ASSERTION(progress <= progressMax, "unexpected progress values");
      mProgressSink->OnProgress(this, nsnull, progress, progressMax);
    }
  }
}

class StatusEvent : public ChildChannelEvent
{
 public:
  StatusEvent(HttpChannelChild* child,
              const nsresult& status,
              const nsString& statusArg)
  : mChild(child)
  , mStatus(status)
  , mStatusArg(statusArg) {}

  void Run() { mChild->OnStatus(mStatus, mStatusArg); }
 private:
  HttpChannelChild* mChild;
  nsresult mStatus;
  nsString mStatusArg;
};

bool
HttpChannelChild::RecvOnStatus(const nsresult& status,
                               const nsString& statusArg)
{
  if (ShouldEnqueue()) {
    EnqueueEvent(new StatusEvent(this, status, statusArg));
  } else {
    OnStatus(status, statusArg);
  }
  return true;
}

void
HttpChannelChild::OnStatus(const nsresult& status,
                           const nsString& statusArg)
{
  LOG(("HttpChannelChild::OnStatus [this=%p status=%x]\n", this, status));

  if (mCanceled)
    return;

  // cache the progress sink so we don't have to query for it each time.
  if (!mProgressSink)
    GetCallback(mProgressSink);

  AutoEventEnqueuer ensureSerialDispatch(this);
  
  // block socket status event after Cancel or OnStopRequest has been called.
  if (mProgressSink && NS_SUCCEEDED(mStatus) && mIsPending && 
      !(mLoadFlags & LOAD_BACKGROUND)) 
  {
    mProgressSink->OnStatus(this, nsnull, status, statusArg.get());
  }
}

class CancelEvent : public ChildChannelEvent
{
 public:
  CancelEvent(HttpChannelChild* child, const nsresult& status)
  : mChild(child)
  , mStatus(status) {}

  void Run() { mChild->OnCancel(mStatus); }
 private:
  HttpChannelChild* mChild;
  nsresult mStatus;
};

bool
HttpChannelChild::RecvCancelEarly(const nsresult& status)
{
  if (ShouldEnqueue()) {
    EnqueueEvent(new CancelEvent(this, status));
  } else {
    OnCancel(status);
  }
  return true;
}

void
HttpChannelChild::OnCancel(const nsresult& status)
{
  LOG(("HttpChannelChild::OnCancel [this=%p status=%x]\n", this, status));

  if (mCanceled)
    return;

  mCanceled = true;
  mStatus = status;

  mIsPending = false;
  if (mLoadGroup)
    mLoadGroup->RemoveRequest(this, nsnull, mStatus);

  if (mListener) {
    mListener->OnStartRequest(this, mListenerContext);
    mListener->OnStopRequest(this, mListenerContext, mStatus);
  }

  mListener = NULL;
  mListenerContext = NULL;

  if (mIPCOpen)
    PHttpChannelChild::Send__delete__(this);
}

class Redirect1Event : public ChildChannelEvent
{
 public:
  Redirect1Event(HttpChannelChild* child,
                 PHttpChannelChild* newChannel,
                 const IPC::URI& newURI,
                 const PRUint32& redirectFlags,
                 const nsHttpResponseHead& responseHead)
  : mChild(child)
  , mNewChannel(newChannel)
  , mNewURI(newURI)
  , mRedirectFlags(redirectFlags)
  , mResponseHead(responseHead) {}

  void Run() 
  { 
    mChild->Redirect1Begin(mNewChannel, mNewURI, mRedirectFlags, 
                           mResponseHead); 
  }
 private:
  HttpChannelChild*   mChild;
  PHttpChannelChild*  mNewChannel;
  IPC::URI            mNewURI;
  PRUint32            mRedirectFlags;
  nsHttpResponseHead  mResponseHead;
};

bool
HttpChannelChild::RecvRedirect1Begin(PHttpChannelChild* newChannel,
                                     const IPC::URI& newURI,
                                     const PRUint32& redirectFlags,
                                     const nsHttpResponseHead& responseHead)
{
  if (ShouldEnqueue()) {
    EnqueueEvent(new Redirect1Event(this, newChannel, newURI, redirectFlags, 
                                    responseHead)); 
  } else {
    Redirect1Begin(newChannel, newURI, redirectFlags, responseHead);
  }
  return true;
}

void
HttpChannelChild::Redirect1Begin(PHttpChannelChild* newChannel,
                                 const IPC::URI& newURI,
                                 const PRUint32& redirectFlags,
                                 const nsHttpResponseHead& responseHead)
{
  HttpChannelChild* 
    newHttpChannelChild = static_cast<HttpChannelChild*>(newChannel);
  nsCOMPtr<nsIURI> uri(newURI);

  nsresult rv = 
    newHttpChannelChild->HttpBaseChannel::Init(uri, mCaps,
                                               mConnectionInfo->ProxyInfo());
  if (NS_FAILED(rv)) {
    // Cancel the channel and veto the redirect.
    Cancel(rv);
    SendRedirect2Result(rv, mRedirectChannelChild->mRequestHeaders);
    return;
  }

  // We won't get OnStartRequest, set cookies here.
  mResponseHead = new nsHttpResponseHead(responseHead);
  SetCookie(mResponseHead->PeekHeader(nsHttp::Set_Cookie));

  PRBool preserveMethod = (mResponseHead->Status() == 307);
  rv = SetupReplacementChannel(uri, newHttpChannelChild, preserveMethod);
  if (NS_FAILED(rv)) {
    // Cancel the channel and veto the redirect.
    Cancel(rv);
    SendRedirect2Result(rv, mRedirectChannelChild->mRequestHeaders);
    return;
  }

  mRedirectChannelChild = newHttpChannelChild;

  rv = gHttpHandler->AsyncOnChannelRedirect(this, 
                                            newHttpChannelChild, 
                                            redirectFlags);
  if (NS_FAILED(rv))
    OnRedirectVerifyCallback(rv);
}

class Redirect3Event : public ChildChannelEvent
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
  if (ShouldEnqueue()) {
    EnqueueEvent(new Redirect3Event(this));
  } else {
    Redirect3Complete();
  }
  return true;
}

void
HttpChannelChild::Redirect3Complete()
{
  nsresult rv;

  // Redirecting to new channel: shut this down and init new channel
  if (mLoadGroup)
    mLoadGroup->RemoveRequest(this, nsnull, NS_BINDING_ABORTED);

  // Chrome channel has been AsyncOpen'd.  Reflect this in child.
  rv = mRedirectChannelChild->CompleteRedirectSetup(mListener, 
                                                    mListenerContext);
  if (NS_FAILED(rv))
    Cancel(rv);
}

nsresult
HttpChannelChild::CompleteRedirectSetup(nsIStreamListener *listener, 
                                        nsISupports *aContext)
{
  LOG(("HttpChannelChild::FinishRedirectSetup [this=%x]\n", this));

  NS_ENSURE_TRUE(!mIsPending, NS_ERROR_IN_PROGRESS);
  NS_ENSURE_TRUE(!mWasOpened, NS_ERROR_ALREADY_OPENED);

  // notify "http-on-modify-request" observers
  gHttpHandler->OnModifyRequest(this);

  mIsPending = PR_TRUE;
  mWasOpened = PR_TRUE;
  mListener = listener;
  mListenerContext = aContext;

  // add ourselves to the load group. 
  if (mLoadGroup)
    mLoadGroup->AddRequest(this, nsnull);

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
  // Cookies may have been changed by redirect observers
  mRedirectChannelChild->AddCookiesToRequest();
  // Must not be called until after redirect observers called.
  mRedirectChannelChild->SetOriginalURI(mRedirectOriginalURI);

  return SendRedirect2Result(result, mRedirectChannelChild->mRequestHeaders);
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
    if (mIPCOpen)
      SendCancel(status);
  }
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::Suspend()
{
  NS_ENSURE_TRUE(mIPCOpen, NS_ERROR_NOT_AVAILABLE);
  SendSuspend();
  mSuspendCount++;
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::Resume()
{
  NS_ENSURE_TRUE(mIPCOpen, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(mSuspendCount > 0, NS_ERROR_UNEXPECTED);
  SendResume();
  mSuspendCount--;
  if (!mSuspendCount) {
    // If we were suspended outside of an event handler (bug 595972) we'll
    // consider ourselves unqueued. This is a legal state of affairs but
    // FlushEventQueue() can't easily ensure this fact, so we'll do some
    // fudging to set the invariants correctly.    
    if (mQueuePhase == PHASE_UNQUEUED)
      mQueuePhase = PHASE_FINISHED_QUEUEING;
    FlushEventQueue();
  }
  return NS_OK;
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
  LOG(("HttpChannelChild::AsyncOpen [this=%x uri=%s]\n", this, mSpec.get()));

  if (mCanceled)
    return mStatus;

  NS_ENSURE_TRUE(gNeckoChild != nsnull, NS_ERROR_FAILURE);
  NS_ENSURE_ARG_POINTER(listener);
  NS_ENSURE_TRUE(!mIsPending, NS_ERROR_IN_PROGRESS);
  NS_ENSURE_TRUE(!mWasOpened, NS_ERROR_ALREADY_OPENED);

  // Port checked in parent, but duplicate here so we can return with error
  // immediately
  nsresult rv;
  rv = NS_CheckPortSafety(mURI);
  if (NS_FAILED(rv))
    return rv;

  // Prepare uploadStream for POST data
  nsCAutoString uploadStreamData;
  PRInt32 uploadStreamInfo;

  if (mUploadStream) {
    // Read entire POST stream into string:
    // This is a temporary measure until bug 564553 is implemented:  we're doing
    // a blocking read of a potentially arbitrarily large stream, so this isn't
    // performant/safe for large file uploads.
    PRUint32 bytes;
    mUploadStream->Available(&bytes);
    if (bytes > 0) {
      rv = NS_ReadInputStreamToString(mUploadStream, uploadStreamData, bytes);
      if (NS_FAILED(rv))
        return rv;
    }

    uploadStreamInfo = mUploadStreamHasHeaders ? 
      eUploadStream_hasHeaders : eUploadStream_hasNoHeaders;
  } else {
    uploadStreamInfo = eUploadStream_null;
  }

  const char *cookieHeader = mRequestHead.PeekHeader(nsHttp::Cookie);
  if (cookieHeader) {
    mUserSetCookieHeader = cookieHeader;
  }

  AddCookiesToRequest();

  //
  // NOTE: From now on we must return NS_OK; all errors must be handled via
  // OnStart/OnStopRequest
  //

  // notify "http-on-modify-request" observers
  gHttpHandler->OnModifyRequest(this);

  mIsPending = PR_TRUE;
  mWasOpened = PR_TRUE;
  mListener = listener;
  mListenerContext = aContext;

  // add ourselves to the load group. 
  if (mLoadGroup)
    mLoadGroup->AddRequest(this, nsnull);

  if (mCanceled) {
    // We may have been canceled already, either by on-modify-request
    // listeners or by load group observers; in that case, don't create IPDL
    // connection. See nsHttpChannel::AsyncOpen().

    // Clear mCanceled here, or we will bail out at top of OnCancel().
    mCanceled = false;
    OnCancel(mStatus);
    return NS_OK;
  }

  //
  // Send request to the chrome process...
  //

  // FIXME: bug 558623: Combine constructor and SendAsyncOpen into one IPC msg

  mozilla::dom::TabChild* tabChild = nsnull;
  nsCOMPtr<nsITabChild> iTabChild;
  GetCallback(iTabChild);
  if (iTabChild) {
    tabChild = static_cast<mozilla::dom::TabChild*>(iTabChild.get());
  }

  // The socket transport in the chrome process now holds a logical ref to us
  // until OnStopRequest, or we do a redirect, or we hit an IPDL error.
  AddIPDLReference();

  gNeckoChild->SendPHttpChannelConstructor(this, tabChild);

  SendAsyncOpen(IPC::URI(mURI), IPC::URI(mOriginalURI),
                IPC::URI(mDocumentURI), IPC::URI(mReferrer), mLoadFlags,
                mRequestHeaders, mRequestHead.Method(), uploadStreamData,
                uploadStreamInfo, mPriority, mRedirectionLimit,
                mAllowPipelining, mForceAllowThirdPartyCookie, mSendResumeAt,
                mStartPos, mEntityID);

  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIHttpChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::SetRequestHeader(const nsACString& aHeader, 
                                   const nsACString& aValue, 
                                   PRBool aMerge)
{
  nsresult rv = HttpBaseChannel::SetRequestHeader(aHeader, aValue, aMerge);
  if (NS_FAILED(rv))
    return rv;

  RequestHeaderTuple* tuple = mRequestHeaders.AppendElement();
  if (!tuple)
    return NS_ERROR_OUT_OF_MEMORY;

  tuple->mHeader = aHeader;
  tuple->mValue = aValue;
  tuple->mMerge = aMerge;
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
HttpChannelChild::GetCacheTokenExpirationTime(PRUint32 *_retval)
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
  if (!mCacheEntryAvailable || !mIPCOpen)
    return NS_ERROR_NOT_AVAILABLE;

  mCachedCharset = aCharset;
  if (!SendSetCacheTokenCachedCharset(PromiseFlatCString(aCharset))) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::IsFromCache(PRBool *value)
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
HttpChannelChild::ResumeAt(PRUint64 startPos, const nsACString& entityID)
{
  ENSURE_CALLED_BEFORE_ASYNC_OPEN();
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
HttpChannelChild::SetPriority(PRInt32 aPriority)
{
  PRInt16 newValue = CLAMP(aPriority, PR_INT16_MIN, PR_INT16_MAX);
  if (mPriority == newValue)
    return NS_OK;
  mPriority = newValue;
  if (mIPCOpen) 
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
// HttpChannelChild::nsITraceableChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::SetNewListener(nsIStreamListener *listener, 
                                 nsIStreamListener **oldListener)
{
  DROP_DEAD();
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIApplicationCacheContainer
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::GetApplicationCache(nsIApplicationCache **aApplicationCache)
{
  DROP_DEAD();
}
NS_IMETHODIMP
HttpChannelChild::SetApplicationCache(nsIApplicationCache *aApplicationCache)
{
  // FIXME: redirects call. so stub OK for now. Fix in bug 536295.  
  return NS_ERROR_NOT_IMPLEMENTED;
}

//-----------------------------------------------------------------------------
// HttpChannelChild::nsIApplicationCacheChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelChild::GetLoadedFromApplicationCache(PRBool *retval)
{
  // FIXME: stub for bug 536295
  *retval = 0;
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::GetInheritApplicationCache(PRBool *aInheritApplicationCache)
{
  DROP_DEAD();
}
NS_IMETHODIMP
HttpChannelChild::SetInheritApplicationCache(PRBool aInheritApplicationCache)
{
  // FIXME: Browser calls this early, so stub OK for now. Fix in bug 536295.  
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelChild::GetChooseApplicationCache(PRBool *aChooseApplicationCache)
{
  DROP_DEAD();
}
NS_IMETHODIMP
HttpChannelChild::SetChooseApplicationCache(PRBool aChooseApplicationCache)
{
  // FIXME: Browser calls this early, so stub OK for now. Fix in bug 536295.  
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

/* attribute unsigned long countSubRequestsHighSecurity; */
NS_IMETHODIMP
HttpChannelChild::GetCountSubRequestsHighSecurity(
                    PRInt32 *aSubRequestsHighSecurity)
{
  nsCOMPtr<nsIAssociatedContentSecurity> assoc;
  if (!GetAssociatedContentSecurity(getter_AddRefs(assoc)))
    return NS_OK;

  return assoc->GetCountSubRequestsHighSecurity(aSubRequestsHighSecurity);
}
NS_IMETHODIMP
HttpChannelChild::SetCountSubRequestsHighSecurity(
                    PRInt32 aSubRequestsHighSecurity)
{
  nsCOMPtr<nsIAssociatedContentSecurity> assoc;
  if (!GetAssociatedContentSecurity(getter_AddRefs(assoc)))
    return NS_OK;

  return assoc->SetCountSubRequestsHighSecurity(aSubRequestsHighSecurity);
}

/* attribute unsigned long countSubRequestsLowSecurity; */
NS_IMETHODIMP
HttpChannelChild::GetCountSubRequestsLowSecurity(
                    PRInt32 *aSubRequestsLowSecurity)
{
  nsCOMPtr<nsIAssociatedContentSecurity> assoc;
  if (!GetAssociatedContentSecurity(getter_AddRefs(assoc)))
    return NS_OK;

  return assoc->GetCountSubRequestsLowSecurity(aSubRequestsLowSecurity);
}
NS_IMETHODIMP
HttpChannelChild::SetCountSubRequestsLowSecurity(
                    PRInt32 aSubRequestsLowSecurity)
{
  nsCOMPtr<nsIAssociatedContentSecurity> assoc;
  if (!GetAssociatedContentSecurity(getter_AddRefs(assoc)))
    return NS_OK;

  return assoc->SetCountSubRequestsLowSecurity(aSubRequestsLowSecurity);
}

/* attribute unsigned long countSubRequestsBrokenSecurity; */
NS_IMETHODIMP 
HttpChannelChild::GetCountSubRequestsBrokenSecurity(
                    PRInt32 *aSubRequestsBrokenSecurity)
{
  nsCOMPtr<nsIAssociatedContentSecurity> assoc;
  if (!GetAssociatedContentSecurity(getter_AddRefs(assoc)))
    return NS_OK;

  return assoc->GetCountSubRequestsBrokenSecurity(aSubRequestsBrokenSecurity);
}
NS_IMETHODIMP 
HttpChannelChild::SetCountSubRequestsBrokenSecurity(
                    PRInt32 aSubRequestsBrokenSecurity)
{
  nsCOMPtr<nsIAssociatedContentSecurity> assoc;
  if (!GetAssociatedContentSecurity(getter_AddRefs(assoc)))
    return NS_OK;

  return assoc->SetCountSubRequestsBrokenSecurity(aSubRequestsBrokenSecurity);
}

/* attribute unsigned long countSubRequestsNoSecurity; */
NS_IMETHODIMP
HttpChannelChild::GetCountSubRequestsNoSecurity(PRInt32 *aSubRequestsNoSecurity)
{
  nsCOMPtr<nsIAssociatedContentSecurity> assoc;
  if (!GetAssociatedContentSecurity(getter_AddRefs(assoc)))
    return NS_OK;

  return assoc->GetCountSubRequestsNoSecurity(aSubRequestsNoSecurity);
}
NS_IMETHODIMP
HttpChannelChild::SetCountSubRequestsNoSecurity(PRInt32 aSubRequestsNoSecurity)
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
  PRInt32 hi, low, broken, no;

  rv = assoc->GetCountSubRequestsHighSecurity(&hi);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = assoc->GetCountSubRequestsLowSecurity(&low);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = assoc->GetCountSubRequestsBrokenSecurity(&broken);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = assoc->GetCountSubRequestsNoSecurity(&no);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mIPCOpen)
    SendUpdateAssociatedContentSecurity(hi, low, broken, no);

  return NS_OK;
}

//------------------------------------------------------------------------------

}} // mozilla::net

