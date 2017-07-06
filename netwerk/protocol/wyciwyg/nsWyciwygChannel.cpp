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
#include "nsCharsetSource.h"
#include "nsProxyRelease.h"
#include "nsThreadUtils.h"
#include "nsIInputStream.h"
#include "nsIInputStreamPump.h"
#include "nsIOutputStream.h"
#include "nsIProgressEventSink.h"
#include "nsIURI.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Unused.h"
#include "mozilla/BasePrincipal.h"
#include "nsProxyRelease.h"
#include "nsContentSecurityManager.h"
#include "nsContentUtils.h"

typedef mozilla::net::LoadContextInfo LoadContextInfo;

// nsWyciwygChannel methods
nsWyciwygChannel::nsWyciwygChannel()
  : mMode(NONE),
    mStatus(NS_OK),
    mIsPending(false),
    mNeedToWriteCharset(false),
    mCharsetSource(kCharsetUninitialized),
    mContentLength(-1),
    mLoadFlags(LOAD_NORMAL),
    mNeedToSetSecurityInfo(false)
{
}

nsWyciwygChannel::~nsWyciwygChannel()
{
  if (mLoadInfo) {
    NS_ReleaseOnMainThread(
      "nsWyciwygChannel::mLoadInfo", mLoadInfo.forget(), false);
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

  mURI = uri;
  mOriginalURI = uri;

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
  UpdatePrivateBrowsing();
  NS_GetOriginAttributes(this, mOriginAttributes);

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

NS_IMETHODIMP
nsWyciwygChannel::GetIsDocument(bool *aIsDocument)
{
  return NS_GetIsDocumentChannel(this, aIsDocument);
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

  UpdatePrivateBrowsing();
  NS_GetOriginAttributes(this, mOriginAttributes);

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
  aContentCharset.AssignLiteral("UTF-16LE");
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
nsWyciwygChannel::Open2(nsIInputStream** aStream)
{
  nsCOMPtr<nsIStreamListener> listener;
  nsresult rv = nsContentSecurityManager::doContentSecurityCheck(this, listener);
  NS_ENSURE_SUCCESS(rv, rv);
  return Open(aStream);
}

NS_IMETHODIMP
nsWyciwygChannel::AsyncOpen(nsIStreamListener *listener, nsISupports *ctx)
{
  MOZ_ASSERT(!mLoadInfo ||
             mLoadInfo->GetSecurityMode() == 0 ||
             mLoadInfo->GetInitialSecurityCheckDone() ||
             (mLoadInfo->GetSecurityMode() == nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL &&
              nsContentUtils::IsSystemPrincipal(mLoadInfo->LoadingPrincipal())),
             "security flags in loadInfo but asyncOpen2() not called");

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
  nsresult rv = OpenCacheEntryForReading(mURI);
  if (NS_FAILED(rv)) {
    LOG(("nsWyciwygChannel::OpenCacheEntryForReading failed [rv=%" PRIx32 "]\n",
         static_cast<uint32_t>(rv)));
    mIsPending = false;
    mCallbacks = nullptr;
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

NS_IMETHODIMP
nsWyciwygChannel::AsyncOpen2(nsIStreamListener *aListener)
{
  nsCOMPtr<nsIStreamListener> listener = aListener;
  nsresult rv = nsContentSecurityManager::doContentSecurityCheck(this, listener);
  if (NS_FAILED(rv)) {
    mIsPending = false;
    mCallbacks = nullptr;
    return rv;
  }
  return AsyncOpen(listener, nullptr);
}

//////////////////////////////////////////////////////////////////////////////
// nsIWyciwygChannel
//////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsWyciwygChannel::WriteToCacheEntry(const nsAString &aData)
{
  LOG(("nsWyciwygChannel::WriteToCacheEntry [this=%p]", this));

  nsresult rv;

  if (mMode == READING) {
    LOG(("nsWyciwygChannel::WriteToCacheEntry already open for reading"));
    MOZ_ASSERT(false);
    return NS_ERROR_UNEXPECTED;
  }

  mMode = WRITING;

  if (!mCacheEntry) {
    nsresult rv = OpenCacheEntryForWriting(mURI);
    if (NS_FAILED(rv) || !mCacheEntry) {
      LOG(("  could not synchronously open cache entry for write!"));
      return NS_ERROR_FAILURE;
    }
  }

  if (mLoadFlags & INHIBIT_PERSISTENT_CACHING) {
    rv = mCacheEntry->SetMetaDataElement("inhibit-persistent-caching", "1");
    if (NS_FAILED(rv)) return rv;
  }

  if (mNeedToSetSecurityInfo) {
    mCacheEntry->SetSecurityInfo(mSecurityInfo);
    mNeedToSetSecurityInfo = false;
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
  if (mCacheEntry) {
    LOG(("nsWyciwygChannel::CloseCacheEntry [this=%p ]", this));
    mCacheOutputStream = nullptr;
    mCacheInputStream = nullptr;

    if (NS_FAILED(reason)) {
      mCacheEntry->AsyncDoom(nullptr);
    }

    mCacheEntry = nullptr;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygChannel::SetSecurityInfo(nsISupports *aSecurityInfo)
{
  if (mMode == READING) {
    MOZ_ASSERT(false);
    return NS_ERROR_UNEXPECTED;
  }

  mSecurityInfo = aSecurityInfo;

  if (mCacheEntry) {
    return mCacheEntry->SetSecurityInfo(mSecurityInfo);
  }

  mNeedToSetSecurityInfo = true;

  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygChannel::SetCharsetAndSource(int32_t aSource,
                                      const nsACString& aCharset)
{
  NS_ENSURE_ARG(!aCharset.IsEmpty());

  if (mCacheEntry) {
    WriteCharsetAndSourceToCache(mCharsetSource, mCharset);
  } else {
    MOZ_ASSERT(mMode != WRITING, "We must have an entry!");
    if (mMode == READING) {
      return NS_ERROR_NOT_AVAILABLE;
    }
    mNeedToWriteCharset = true;
    mCharsetSource = aSource;
    mCharset = aCharset;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygChannel::GetCharsetAndSource(int32_t* aSource, nsACString& aCharset)
{
  MOZ_ASSERT(mMode == READING);

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
       "new=%d status=%" PRIx32 "]\n", this, aCacheEntry, aNew, static_cast<uint32_t>(aStatus)));

  MOZ_RELEASE_ASSERT(!aNew, "New entry must not be returned when flag "
                            "OPEN_READONLY is used!");

  // if the channel's already fired onStopRequest,
  // then we should ignore this event.
  if (!mIsPending)
    return NS_OK;

  if (NS_SUCCEEDED(mStatus)) {
    if (NS_SUCCEEDED(aStatus)) {
      MOZ_ASSERT(aCacheEntry);
      mCacheEntry = aCacheEntry;
      nsresult rv = ReadFromCache();
      if (NS_FAILED(rv)) {
        mStatus = rv;
      }
    } else {
      mStatus = aStatus;
    }
  }

  if (NS_FAILED(mStatus)) {
    LOG(("channel was canceled [this=%p status=%" PRIx32 "]\n", this, static_cast<uint32_t>(mStatus)));
    // Since OnCacheEntryAvailable can be called directly from AsyncOpen
    // we must dispatch.
    NS_DispatchToCurrentThread(
      mozilla::NewRunnableMethod("nsWyciwygChannel::NotifyListener",
                                 this,
                                 &nsWyciwygChannel::NotifyListener));
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
  LOG(("nsWyciwygChannel::OnDataAvailable [this=%p request=%p offset=%" PRIu64 " count=%u]\n",
      this, request, offset, count));

  nsresult rv;

  nsCOMPtr<nsIStreamListener> listener = mListener;
  nsCOMPtr<nsISupports> listenerContext = mListenerContext;

  if (listener) {
    rv = listener->OnDataAvailable(this, listenerContext, input, offset, count);
  } else {
    MOZ_ASSERT(false, "We must have a listener!");
    rv = NS_ERROR_UNEXPECTED;
  }

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
  LOG(("nsWyciwygChannel::OnStartRequest [this=%p request=%p]\n",
      this, request));

  nsCOMPtr<nsIStreamListener> listener = mListener;
  nsCOMPtr<nsISupports> listenerContext = mListenerContext;

  if (listener) {
    return listener->OnStartRequest(this, listenerContext);
  }

  MOZ_ASSERT(false, "We must have a listener!");
  return NS_ERROR_UNEXPECTED;
}


NS_IMETHODIMP
nsWyciwygChannel::OnStopRequest(nsIRequest *request, nsISupports *ctx, nsresult status)
{
  LOG(("nsWyciwygChannel::OnStopRequest [this=%p request=%p status=%" PRIu32 "]\n",
       this, request, static_cast<uint32_t>(status)));

  if (NS_SUCCEEDED(mStatus))
    mStatus = status;

  mIsPending = false;

  nsCOMPtr<nsIStreamListener> listener;
  nsCOMPtr<nsISupports> listenerContext;
  listener.swap(mListener);
  listenerContext.swap(mListenerContext);

  if (listener) {
    listener->OnStopRequest(this, listenerContext, mStatus);
  } else {
    MOZ_ASSERT(false, "We must have a listener!");
  }

  if (mLoadGroup)
    mLoadGroup->RemoveRequest(this, nullptr, mStatus);

  CloseCacheEntry(mStatus);
  mPump = nullptr;

  // Drop notification callbacks to prevent cycles.
  mCallbacks = nullptr;
  mProgressSink = nullptr;

  return NS_OK;
}

//////////////////////////////////////////////////////////////////////////////
// Helper functions
//////////////////////////////////////////////////////////////////////////////

nsresult
nsWyciwygChannel::GetCacheStorage(nsICacheStorage **_retval)
{
  nsresult rv;

  nsCOMPtr<nsICacheStorageService> cacheService =
    do_GetService("@mozilla.org/netwerk/cache-storage-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  bool anonymous = mLoadFlags & LOAD_ANONYMOUS;
  mOriginAttributes.SyncAttributesWithPrivateBrowsing(mPrivateBrowsing);
  RefPtr<LoadContextInfo> loadInfo = mozilla::net::GetLoadContextInfo(anonymous, mOriginAttributes);

  if (mLoadFlags & INHIBIT_PERSISTENT_CACHING) {
    return cacheService->MemoryCacheStorage(loadInfo, _retval);
  }

  return cacheService->DiskCacheStorage(loadInfo, false, _retval);
}

nsresult
nsWyciwygChannel::OpenCacheEntryForReading(nsIURI *aURI)
{
  nsresult rv;

  nsCOMPtr<nsICacheStorage> cacheStorage;
  rv = GetCacheStorage(getter_AddRefs(cacheStorage));
  NS_ENSURE_SUCCESS(rv, rv);

  return cacheStorage->AsyncOpenURI(aURI, EmptyCString(),
                                    nsICacheStorage::OPEN_READONLY |
                                    nsICacheStorage::CHECK_MULTITHREADED,
                                    this);
}

nsresult
nsWyciwygChannel::OpenCacheEntryForWriting(nsIURI *aURI)
{
  nsresult rv;

  nsCOMPtr<nsICacheStorage> cacheStorage;
  rv = GetCacheStorage(getter_AddRefs(cacheStorage));
  NS_ENSURE_SUCCESS(rv, rv);

  return cacheStorage->OpenTruncate(aURI, EmptyCString(),
                                    getter_AddRefs(mCacheEntry));
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
  NS_PRECONDITION(mCacheEntry, "Better have cache entry!");

  mCacheEntry->SetMetaDataElement("charset", aCharset.get());

  nsAutoCString source;
  source.AppendInt(aSource);
  mCacheEntry->SetMetaDataElement("charset-source", source.get());
}

void
nsWyciwygChannel::NotifyListener()
{
  nsCOMPtr<nsIStreamListener> listener;
  nsCOMPtr<nsISupports> listenerContext;

  listener.swap(mListener);
  listenerContext.swap(mListenerContext);

  if (listener) {
    listener->OnStartRequest(this, listenerContext);
    mIsPending = false;
    listener->OnStopRequest(this, listenerContext, mStatus);
  } else {
    MOZ_ASSERT(false, "We must have the listener!");
    mIsPending = false;
  }

  CloseCacheEntry(mStatus);

  // Remove ourselves from the load group.
  if (mLoadGroup) {
    mLoadGroup->RemoveRequest(this, nullptr, mStatus);
  }
}

// vim: ts=2 sw=2
