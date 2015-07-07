/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWyciwyg.h"
#include "nsWyciwygChannel.h"
#include "nsILoadGroup.h"
#include "nsNetUtil.h"
#include "nsNetCID.h"
#include "LoadContextInfo.h"
#include "nsICacheService.h" // only to initialize
#include "nsICacheStorageService.h"
#include "nsICacheStorage.h"
#include "nsICacheEntry.h"
#include "CacheObserver.h"
#include "nsCharsetSource.h"
#include "nsProxyRelease.h"
#include "nsThreadUtils.h"
#include "nsIEventTarget.h"
#include "nsIInputStream.h"
#include "nsIInputStreamPump.h"
#include "nsIOutputStream.h"
#include "nsIProgressEventSink.h"
#include "nsIURI.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/unused.h"
#include "nsProxyRelease.h"

typedef mozilla::net::LoadContextInfo LoadContextInfo;

// Must release mChannel on the main thread
class nsWyciwygAsyncEvent : public nsRunnable {
public:
  explicit nsWyciwygAsyncEvent(nsWyciwygChannel *aChannel) : mChannel(aChannel) {}

  ~nsWyciwygAsyncEvent()
  {
    nsCOMPtr<nsIThread> thread = do_GetMainThread();
    NS_WARN_IF_FALSE(thread, "Couldn't get the main thread!");
    if (thread) {
      nsIWyciwygChannel *chan = static_cast<nsIWyciwygChannel *>(mChannel);
      mozilla::unused << mChannel.forget();
      NS_ProxyRelease(thread, chan);
    }
  }
protected:
  nsRefPtr<nsWyciwygChannel> mChannel;
};

class nsWyciwygSetCharsetandSourceEvent : public nsWyciwygAsyncEvent {
public:
  explicit nsWyciwygSetCharsetandSourceEvent(nsWyciwygChannel *aChannel)
    : nsWyciwygAsyncEvent(aChannel) {}

  NS_IMETHOD Run()
  {
    mChannel->SetCharsetAndSourceInternal();
    return NS_OK;
  }
};

class nsWyciwygWriteEvent : public nsWyciwygAsyncEvent {
public:
  nsWyciwygWriteEvent(nsWyciwygChannel *aChannel, const nsAString &aData)
    : nsWyciwygAsyncEvent(aChannel), mData(aData) {}

  NS_IMETHOD Run()
  {
    mChannel->WriteToCacheEntryInternal(mData);
    return NS_OK;
  }
private:
  nsString mData;
};

class nsWyciwygCloseEvent : public nsWyciwygAsyncEvent {
public:
  nsWyciwygCloseEvent(nsWyciwygChannel *aChannel, nsresult aReason)
    : nsWyciwygAsyncEvent(aChannel), mReason(aReason) {}

  NS_IMETHOD Run()
  {
    mChannel->CloseCacheEntryInternal(mReason);
    return NS_OK;
  }
private:
  nsresult mReason;
};


// nsWyciwygChannel methods 
nsWyciwygChannel::nsWyciwygChannel()
  : mMode(NONE),
    mStatus(NS_OK),
    mIsPending(false),
    mCharsetAndSourceSet(false),
    mNeedToWriteCharset(false),
    mCharsetSource(kCharsetUninitialized),
    mContentLength(-1),
    mLoadFlags(LOAD_NORMAL),
    mAppId(NECKO_NO_APP_ID),
    mInBrowser(false)
{
}

nsWyciwygChannel::~nsWyciwygChannel() 
{
  if (mLoadInfo) {
    nsCOMPtr<nsIThread> mainThread;
    NS_GetMainThread(getter_AddRefs(mainThread));

    nsILoadInfo *forgetableLoadInfo;
    mLoadInfo.forget(&forgetableLoadInfo);
    NS_ProxyRelease(mainThread, forgetableLoadInfo, false);
  }
}

NS_IMPL_ISUPPORTS(nsWyciwygChannel,
                  nsIChannel,
                  nsIRequest,
                  nsIStreamListener,
                  nsIRequestObserver,
                  nsICacheEntryOpenCallback,
                  nsIWyciwygChannel,
                  nsIPrivateBrowsingChannel)

nsresult
nsWyciwygChannel::Init(nsIURI* uri)
{
  NS_ENSURE_ARG_POINTER(uri);

  nsresult rv;

  if (!mozilla::net::CacheObserver::UseNewCache()) {
    // Since nsWyciwygChannel can use the new cache API off the main thread
    // and that API normally does this initiation, we need to take care
    // of initiating the old cache service here manually.  Will be removed
    // with bug 913828.
    MOZ_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsICacheService> service =
      do_GetService(NS_CACHESERVICE_CONTRACTID, &rv);
  }

  mURI = uri;
  mOriginalURI = uri;

  nsCOMPtr<nsICacheStorageService> serv =
    do_GetService("@mozilla.org/netwerk/cache-storage-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = serv->GetIoTarget(getter_AddRefs(mCacheIOTarget));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsIRequest methods:
///////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsWyciwygChannel::GetName(nsACString &aName)
{
  return mURI->GetSpec(aName);
}
 
NS_IMETHODIMP
nsWyciwygChannel::IsPending(bool *aIsPending)
{
  *aIsPending = mIsPending;
  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygChannel::GetStatus(nsresult *aStatus)
{
  if (NS_SUCCEEDED(mStatus) && mPump)
    mPump->GetStatus(aStatus);
  else
    *aStatus = mStatus;
  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygChannel::Cancel(nsresult status)
{
  mStatus = status;
  if (mPump)
    mPump->Cancel(status);
  // else we're waiting for OnCacheEntryAvailable
  return NS_OK;
}
 
NS_IMETHODIMP
nsWyciwygChannel::Suspend()
{
  if (mPump)
    mPump->Suspend();
  // XXX else, we'll ignore this ... and that's probably bad!
  return NS_OK;
}
 
NS_IMETHODIMP
nsWyciwygChannel::Resume()
{
  if (mPump)
    mPump->Resume();
  // XXX else, we'll ignore this ... and that's probably bad!
  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygChannel::GetLoadGroup(nsILoadGroup* *aLoadGroup)
{
  *aLoadGroup = mLoadGroup;
  NS_IF_ADDREF(*aLoadGroup);
  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygChannel::SetLoadGroup(nsILoadGroup* aLoadGroup)
{
  if (!CanSetLoadGroup(aLoadGroup)) {
    return NS_ERROR_FAILURE;
  }

  mLoadGroup = aLoadGroup;
  NS_QueryNotificationCallbacks(mCallbacks,
                                mLoadGroup,
                                NS_GET_IID(nsIProgressEventSink),
                                getter_AddRefs(mProgressSink));
  mPrivateBrowsing = NS_UsePrivateBrowsing(this);
  NS_GetAppInfo(this, &mAppId, &mInBrowser);
  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygChannel::SetLoadFlags(uint32_t aLoadFlags)
{
  mLoadFlags = aLoadFlags;
  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygChannel::GetLoadFlags(uint32_t * aLoadFlags)
{
  *aLoadFlags = mLoadFlags;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIChannel methods:
///////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsWyciwygChannel::GetOriginalURI(nsIURI* *aURI)
{
  *aURI = mOriginalURI;
  NS_ADDREF(*aURI);
  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygChannel::SetOriginalURI(nsIURI* aURI)
{
  NS_ENSURE_ARG_POINTER(aURI);
  mOriginalURI = aURI;
  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygChannel::GetURI(nsIURI* *aURI)
{
  *aURI = mURI;
  NS_IF_ADDREF(*aURI);
  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygChannel::GetOwner(nsISupports **aOwner)
{
  NS_IF_ADDREF(*aOwner = mOwner);
  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygChannel::SetOwner(nsISupports* aOwner)
{
  mOwner = aOwner;
  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygChannel::GetLoadInfo(nsILoadInfo **aLoadInfo)
{
  NS_IF_ADDREF(*aLoadInfo = mLoadInfo);
  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygChannel::SetLoadInfo(nsILoadInfo* aLoadInfo)
{
  mLoadInfo = aLoadInfo;
  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygChannel::GetNotificationCallbacks(nsIInterfaceRequestor* *aCallbacks)
{
  *aCallbacks = mCallbacks.get();
  NS_IF_ADDREF(*aCallbacks);
  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygChannel::SetNotificationCallbacks(nsIInterfaceRequestor* aNotificationCallbacks)
{
  if (!CanSetCallbacks(aNotificationCallbacks)) {
    return NS_ERROR_FAILURE;
  }

  mCallbacks = aNotificationCallbacks;
  NS_QueryNotificationCallbacks(mCallbacks,
                                mLoadGroup,
                                NS_GET_IID(nsIProgressEventSink),
                                getter_AddRefs(mProgressSink));

  mPrivateBrowsing = NS_UsePrivateBrowsing(this);
  NS_GetAppInfo(this, &mAppId, &mInBrowser);

  return NS_OK;
}

NS_IMETHODIMP 
nsWyciwygChannel::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
  NS_IF_ADDREF(*aSecurityInfo = mSecurityInfo);

  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygChannel::GetContentType(nsACString &aContentType)
{
  aContentType.AssignLiteral(WYCIWYG_TYPE);
  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygChannel::SetContentType(const nsACString &aContentType)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWyciwygChannel::GetContentCharset(nsACString &aContentCharset)
{
  aContentCharset.AssignLiteral("UTF-16");
  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygChannel::SetContentCharset(const nsACString &aContentCharset)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWyciwygChannel::GetContentDisposition(uint32_t *aContentDisposition)
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsWyciwygChannel::SetContentDisposition(uint32_t aContentDisposition)
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsWyciwygChannel::GetContentDispositionFilename(nsAString &aContentDispositionFilename)
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsWyciwygChannel::SetContentDispositionFilename(const nsAString &aContentDispositionFilename)
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsWyciwygChannel::GetContentDispositionHeader(nsACString &aContentDispositionHeader)
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsWyciwygChannel::GetContentLength(int64_t *aContentLength)
{
  *aContentLength = mContentLength;
  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygChannel::SetContentLength(int64_t aContentLength)
{
  mContentLength = aContentLength;

  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygChannel::Open(nsIInputStream ** aReturn)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWyciwygChannel::AsyncOpen(nsIStreamListener *listener, nsISupports *ctx)
{
  LOG(("nsWyciwygChannel::AsyncOpen [this=%p]\n", this));
  MOZ_ASSERT(mMode == NONE, "nsWyciwygChannel already open");

  NS_ENSURE_TRUE(!mIsPending, NS_ERROR_IN_PROGRESS);
  NS_ENSURE_TRUE(mMode == NONE, NS_ERROR_IN_PROGRESS);
  NS_ENSURE_ARG_POINTER(listener);

  mMode = READING;

  // open a cache entry for this channel...
  // mIsPending set to true since OnCacheEntryAvailable may be called
  // synchronously and fails when mIsPending found false.
  mIsPending = true;
  nsresult rv = OpenCacheEntry(mURI, nsICacheStorage::OPEN_READONLY |
                                     nsICacheStorage::CHECK_MULTITHREADED);
  if (NS_FAILED(rv)) {
    LOG(("nsWyciwygChannel::OpenCacheEntry failed [rv=%x]\n", rv));
    mIsPending = false;
    return rv;
  }

  // There is no code path that would invoke the listener sooner than
  // we get to this line in case OnCacheEntryAvailable is invoked
  // synchronously.
  mListener = listener;
  mListenerContext = ctx;

  if (mLoadGroup)
    mLoadGroup->AddRequest(this, nullptr);

  return NS_OK;
}

//////////////////////////////////////////////////////////////////////////////
// nsIWyciwygChannel
//////////////////////////////////////////////////////////////////////////////

nsresult
nsWyciwygChannel::EnsureWriteCacheEntry()
{
  MOZ_ASSERT(mMode == WRITING, "nsWyciwygChannel not open for writing");

  if (!mCacheEntry) {
    // OPEN_TRUNCATE will give us the entry instantly
    nsresult rv = OpenCacheEntry(mURI, nsICacheStorage::OPEN_TRUNCATE);
    if (NS_FAILED(rv) || !mCacheEntry) {
      LOG(("  could not synchronously open cache entry for write!"));
      return NS_ERROR_FAILURE;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygChannel::WriteToCacheEntry(const nsAString &aData)
{
  if (mMode == READING) {
    LOG(("nsWyciwygChannel::WriteToCacheEntry already open for reading"));
    MOZ_ASSERT(false);
    return NS_ERROR_UNEXPECTED;
  }

  mMode = WRITING;

  if (mozilla::net::CacheObserver::UseNewCache()) {
    nsresult rv = EnsureWriteCacheEntry();
    if (NS_FAILED(rv)) return rv;
  }

  return mCacheIOTarget->Dispatch(new nsWyciwygWriteEvent(this, aData),
                                  NS_DISPATCH_NORMAL);
}

nsresult
nsWyciwygChannel::WriteToCacheEntryInternal(const nsAString &aData)
{
  LOG(("nsWyciwygChannel::WriteToCacheEntryInternal [this=%p]", this));
  NS_ASSERTION(IsOnCacheIOThread(), "wrong thread");

  nsresult rv;

  // With the new cache entry this will just pass as a no-op since we
  // are opening the entry in WriteToCacheEntry.
  rv = EnsureWriteCacheEntry();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (mLoadFlags & INHIBIT_PERSISTENT_CACHING) {
    rv = mCacheEntry->SetMetaDataElement("inhibit-persistent-caching", "1");
    if (NS_FAILED(rv)) return rv;
  }

  if (mSecurityInfo) {
    mCacheEntry->SetSecurityInfo(mSecurityInfo);
  }

  if (mNeedToWriteCharset) {
    WriteCharsetAndSourceToCache(mCharsetSource, mCharset);
    mNeedToWriteCharset = false;
  }

  uint32_t out;
  if (!mCacheOutputStream) {
    // Get the outputstream from the cache entry.
    rv = mCacheEntry->OpenOutputStream(0, getter_AddRefs(mCacheOutputStream));    
    if (NS_FAILED(rv)) return rv;

    // Write out a Byte Order Mark, so that we'll know if the data is
    // BE or LE when we go to read it.
    char16_t bom = 0xFEFF;
    rv = mCacheOutputStream->Write((char *)&bom, sizeof(bom), &out);
    if (NS_FAILED(rv)) return rv;
  }

  return mCacheOutputStream->Write((const char *)PromiseFlatString(aData).get(),
                                   aData.Length() * sizeof(char16_t), &out);
}


NS_IMETHODIMP
nsWyciwygChannel::CloseCacheEntry(nsresult reason)
{
  return mCacheIOTarget->Dispatch(new nsWyciwygCloseEvent(this, reason),
                                  NS_DISPATCH_NORMAL);
}

nsresult
nsWyciwygChannel::CloseCacheEntryInternal(nsresult reason)
{
  NS_ASSERTION(IsOnCacheIOThread(), "wrong thread");

  if (mCacheEntry) {
    LOG(("nsWyciwygChannel::CloseCacheEntryInternal [this=%p ]", this));
    mCacheOutputStream = 0;
    mCacheInputStream = 0;

    if (NS_FAILED(reason))
      mCacheEntry->AsyncDoom(nullptr); // here we were calling Doom() ...

    mCacheEntry = 0;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygChannel::SetSecurityInfo(nsISupports *aSecurityInfo)
{
  mSecurityInfo = aSecurityInfo;

  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygChannel::SetCharsetAndSource(int32_t aSource,
                                      const nsACString& aCharset)
{
  NS_ENSURE_ARG(!aCharset.IsEmpty());

  mCharsetAndSourceSet = true;
  mCharset = aCharset;
  mCharsetSource = aSource;

  return mCacheIOTarget->Dispatch(new nsWyciwygSetCharsetandSourceEvent(this),
                                  NS_DISPATCH_NORMAL);
}

void
nsWyciwygChannel::SetCharsetAndSourceInternal()
{
  NS_ASSERTION(IsOnCacheIOThread(), "wrong thread");

  if (mCacheEntry) {
    WriteCharsetAndSourceToCache(mCharsetSource, mCharset);
  } else {
    mNeedToWriteCharset = true;
  }
}

NS_IMETHODIMP
nsWyciwygChannel::GetCharsetAndSource(int32_t* aSource, nsACString& aCharset)
{
  if (mCharsetAndSourceSet) {
    *aSource = mCharsetSource;
    aCharset = mCharset;
    return NS_OK;
  }

  if (!mCacheEntry) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsXPIDLCString data;
  mCacheEntry->GetMetaDataElement("charset", getter_Copies(data));

  if (data.IsEmpty()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsXPIDLCString sourceStr;
  mCacheEntry->GetMetaDataElement("charset-source", getter_Copies(sourceStr));

  int32_t source;
  nsresult err;
  source = sourceStr.ToInteger(&err);
  if (NS_FAILED(err) || source == 0) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  *aSource = source;
  aCharset = data;
  return NS_OK;
}

//////////////////////////////////////////////////////////////////////////////
// nsICacheEntryOpenCallback
//////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsWyciwygChannel::OnCacheEntryCheck(nsICacheEntry* entry, nsIApplicationCache* appCache,
                                    uint32_t* aResult)
{
  *aResult = ENTRY_WANTED;
  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygChannel::OnCacheEntryAvailable(nsICacheEntry *aCacheEntry,
                                        bool aNew,
                                        nsIApplicationCache* aAppCache,
                                        nsresult aStatus)
{
  LOG(("nsWyciwygChannel::OnCacheEntryAvailable [this=%p entry=%p "
       "new=%d status=%x]\n", this, aCacheEntry, aNew, aStatus));

  // if the channel's already fired onStopRequest, 
  // then we should ignore this event.
  if (!mIsPending && !aNew)
    return NS_OK;

  // otherwise, we have to handle this event.
  if (NS_SUCCEEDED(aStatus))
    mCacheEntry = aCacheEntry;
  else if (NS_SUCCEEDED(mStatus))
    mStatus = aStatus;

  nsresult rv = NS_OK;
  if (NS_FAILED(mStatus)) {
    LOG(("channel was canceled [this=%p status=%x]\n", this, mStatus));
    rv = mStatus;
  }
  else if (!aNew) { // advance to the next state...
    rv = ReadFromCache();
  }

  // a failure from Connect means that we have to abort the channel.
  if (NS_FAILED(rv)) {
    CloseCacheEntry(rv);

    if (!aNew) {
      // Since OnCacheEntryAvailable can be called directly from AsyncOpen
      // we must dispatch.
      NS_DispatchToCurrentThread(NS_NewRunnableMethod(
        this, &nsWyciwygChannel::NotifyListener));
    }
  }

  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsWyciwygChannel::nsIStreamListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsWyciwygChannel::OnDataAvailable(nsIRequest *request, nsISupports *ctx,
                                  nsIInputStream *input,
                                  uint64_t offset, uint32_t count)
{
  LOG(("nsWyciwygChannel::OnDataAvailable [this=%p request=%x offset=%llu count=%u]\n",
      this, request, offset, count));

  nsresult rv;
  
  rv = mListener->OnDataAvailable(this, mListenerContext, input, offset, count);

  // XXX handle 64-bit stuff for real
  if (mProgressSink && NS_SUCCEEDED(rv)) {
    mProgressSink->OnProgress(this, nullptr, offset + count, mContentLength);
  }

  return rv; // let the pump cancel on failure
}

//////////////////////////////////////////////////////////////////////////////
// nsIRequestObserver
//////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsWyciwygChannel::OnStartRequest(nsIRequest *request, nsISupports *ctx)
{
  LOG(("nsWyciwygChannel::OnStartRequest [this=%p request=%x\n",
      this, request));

  return mListener->OnStartRequest(this, mListenerContext);
}


NS_IMETHODIMP
nsWyciwygChannel::OnStopRequest(nsIRequest *request, nsISupports *ctx, nsresult status)
{
  LOG(("nsWyciwygChannel::OnStopRequest [this=%p request=%x status=%d\n",
      this, request, status));

  if (NS_SUCCEEDED(mStatus))
    mStatus = status;

  mListener->OnStopRequest(this, mListenerContext, mStatus);
  mListener = 0;
  mListenerContext = 0;

  if (mLoadGroup)
    mLoadGroup->RemoveRequest(this, nullptr, mStatus);

  CloseCacheEntry(mStatus);
  mPump = 0;
  mIsPending = false;

  // Drop notification callbacks to prevent cycles.
  mCallbacks = 0;
  mProgressSink = 0;

  return NS_OK;
}

//////////////////////////////////////////////////////////////////////////////
// Helper functions
//////////////////////////////////////////////////////////////////////////////

nsresult
nsWyciwygChannel::OpenCacheEntry(nsIURI *aURI,
                                 uint32_t aOpenFlags)
{
  nsresult rv;

  nsCOMPtr<nsICacheStorageService> cacheService =
    do_GetService("@mozilla.org/netwerk/cache-storage-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  bool anonymous = mLoadFlags & LOAD_ANONYMOUS;
  nsRefPtr<LoadContextInfo> loadInfo = mozilla::net::GetLoadContextInfo(
    mPrivateBrowsing, mAppId, mInBrowser, anonymous);

  nsCOMPtr<nsICacheStorage> cacheStorage;
  if (mLoadFlags & INHIBIT_PERSISTENT_CACHING)
    rv = cacheService->MemoryCacheStorage(loadInfo, getter_AddRefs(cacheStorage));
  else
    rv = cacheService->DiskCacheStorage(loadInfo, false, getter_AddRefs(cacheStorage));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = cacheStorage->AsyncOpenURI(aURI, EmptyCString(), aOpenFlags, this);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsWyciwygChannel::ReadFromCache()
{
  LOG(("nsWyciwygChannel::ReadFromCache [this=%p] ", this));

  NS_ENSURE_TRUE(mCacheEntry, NS_ERROR_FAILURE);
  nsresult rv;

  // Get the stored security info
  mCacheEntry->GetSecurityInfo(getter_AddRefs(mSecurityInfo));

  nsAutoCString tmpStr;
  rv = mCacheEntry->GetMetaDataElement("inhibit-persistent-caching",
                                       getter_Copies(tmpStr));
  if (NS_SUCCEEDED(rv) && tmpStr.EqualsLiteral("1"))
    mLoadFlags |= INHIBIT_PERSISTENT_CACHING;

  // Get a transport to the cached data...
  rv = mCacheEntry->OpenInputStream(0, getter_AddRefs(mCacheInputStream));
  if (NS_FAILED(rv))
    return rv;
  NS_ENSURE_TRUE(mCacheInputStream, NS_ERROR_UNEXPECTED);

  rv = NS_NewInputStreamPump(getter_AddRefs(mPump), mCacheInputStream);
  if (NS_FAILED(rv)) return rv;

  // Pump the cache data downstream
  return mPump->AsyncRead(this, nullptr);
}

void
nsWyciwygChannel::WriteCharsetAndSourceToCache(int32_t aSource,
                                               const nsCString& aCharset)
{
  NS_ASSERTION(IsOnCacheIOThread(), "wrong thread");
  NS_PRECONDITION(mCacheEntry, "Better have cache entry!");
  
  mCacheEntry->SetMetaDataElement("charset", aCharset.get());

  nsAutoCString source;
  source.AppendInt(aSource);
  mCacheEntry->SetMetaDataElement("charset-source", source.get());
}

void
nsWyciwygChannel::NotifyListener()
{    
  if (mListener) {
    mListener->OnStartRequest(this, mListenerContext);
    mListener->OnStopRequest(this, mListenerContext, mStatus);
    mListener = 0;
    mListenerContext = 0;
  }

  mIsPending = false;

  // Remove ourselves from the load group.
  if (mLoadGroup) {
    mLoadGroup->RemoveRequest(this, nullptr, mStatus);
  }
}

bool
nsWyciwygChannel::IsOnCacheIOThread()
{
  bool correctThread;
  mCacheIOTarget->IsOnCurrentThread(&correctThread);
  return correctThread;
}

// vim: ts=2 sw=2
