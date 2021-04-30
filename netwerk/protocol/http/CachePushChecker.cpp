/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CachePushChecker.h"

#include "LoadContextInfo.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/net/SocketProcessChild.h"
#include "nsICacheEntry.h"
#include "nsICacheStorageService.h"
#include "nsICacheStorage.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(CachePushChecker, nsICacheEntryOpenCallback);

CachePushChecker::CachePushChecker(nsIURI* aPushedURL,
                                   const OriginAttributes& aOriginAttributes,
                                   const nsACString& aRequestString,
                                   std::function<void(bool)>&& aCallback)
    : mPushedURL(aPushedURL),
      mOriginAttributes(aOriginAttributes),
      mRequestString(aRequestString),
      mCallback(std::move(aCallback)),
      mCurrentEventTarget(GetCurrentEventTarget()) {}

nsresult CachePushChecker::DoCheck() {
  if (XRE_IsSocketProcess()) {
    RefPtr<CachePushChecker> self = this;
    return NS_DispatchToMainThread(
        NS_NewRunnableFunction(
            "CachePushChecker::DoCheck",
            [self]() {
              if (SocketProcessChild* child =
                      SocketProcessChild::GetSingleton()) {
                child
                    ->SendCachePushCheck(self->mPushedURL,
                                         self->mOriginAttributes,
                                         self->mRequestString)
                    ->Then(
                        GetCurrentSerialEventTarget(), __func__,
                        [self](bool aResult) { self->InvokeCallback(aResult); },
                        [](const mozilla::ipc::ResponseRejectReason) {});
              }
            }),
        NS_DISPATCH_NORMAL);
  }

  nsresult rv;
  nsCOMPtr<nsICacheStorageService> css =
      do_GetService("@mozilla.org/netwerk/cache-storage-service;1", &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  RefPtr<LoadContextInfo> lci = GetLoadContextInfo(false, mOriginAttributes);
  nsCOMPtr<nsICacheStorage> ds;
  rv = css->DiskCacheStorage(lci, getter_AddRefs(ds));
  if (NS_FAILED(rv)) {
    return rv;
  }

  return ds->AsyncOpenURI(
      mPushedURL, ""_ns,
      nsICacheStorage::OPEN_READONLY | nsICacheStorage::OPEN_SECRETLY, this);
}

NS_IMETHODIMP
CachePushChecker::OnCacheEntryCheck(nsICacheEntry* entry, uint32_t* result) {
  MOZ_ASSERT(XRE_IsParentProcess());

  // We never care to fully open the entry, since we won't actually use it.
  // We just want to be able to do all our checks to see if a future channel can
  // use this entry, or if we need to accept the push.
  *result = nsICacheEntryOpenCallback::ENTRY_NOT_WANTED;

  bool isForcedValid = false;
  entry->GetIsForcedValid(&isForcedValid);

  nsHttpRequestHead requestHead;
  requestHead.ParseHeaderSet(mRequestString.BeginReading());
  nsHttpResponseHead cachedResponseHead;
  bool acceptPush = true;
  auto onExitGuard = MakeScopeExit([&] { InvokeCallback(acceptPush); });

  nsresult rv =
      nsHttp::GetHttpResponseHeadFromCacheEntry(entry, &cachedResponseHead);
  if (NS_FAILED(rv)) {
    // Couldn't make sense of what's in the cache entry, go ahead and accept
    // the push.
    return NS_OK;
  }

  if ((cachedResponseHead.Status() / 100) != 2) {
    // Assume the push is sending us a success, while we don't have one in the
    // cache, so we'll accept the push.
    return NS_OK;
  }

  // Get the method that was used to generate the cached response
  nsCString buf;
  rv = entry->GetMetaDataElement("request-method", getter_Copies(buf));
  if (NS_FAILED(rv)) {
    // Can't check request method, accept the push
    return NS_OK;
  }
  nsAutoCString pushedMethod;
  requestHead.Method(pushedMethod);
  if (!buf.Equals(pushedMethod)) {
    // Methods don't match, accept the push
    return NS_OK;
  }

  int64_t size, contentLength;
  rv = nsHttp::CheckPartial(entry, &size, &contentLength, &cachedResponseHead);
  if (NS_FAILED(rv)) {
    // Couldn't figure out if this was partial or not, accept the push.
    return NS_OK;
  }

  if (size == int64_t(-1) || contentLength != size) {
    // This is partial content in the cache, accept the push.
    return NS_OK;
  }

  nsAutoCString requestedETag;
  if (NS_FAILED(requestHead.GetHeader(nsHttp::If_Match, requestedETag))) {
    // Can't check etag
    return NS_OK;
  }
  if (!requestedETag.IsEmpty()) {
    nsAutoCString cachedETag;
    if (NS_FAILED(cachedResponseHead.GetHeader(nsHttp::ETag, cachedETag))) {
      // Can't check etag
      return NS_OK;
    }
    if (!requestedETag.Equals(cachedETag)) {
      // ETags don't match, accept the push.
      return NS_OK;
    }
  }

  nsAutoCString imsString;
  Unused << requestHead.GetHeader(nsHttp::If_Modified_Since, imsString);
  if (!buf.IsEmpty()) {
    uint32_t ims = buf.ToInteger(&rv);
    uint32_t lm;
    rv = cachedResponseHead.GetLastModifiedValue(&lm);
    if (NS_SUCCEEDED(rv) && lm && lm < ims) {
      // The push appears to be newer than what's in our cache, accept it.
      return NS_OK;
    }
  }

  nsAutoCString cacheControlRequestHeader;
  Unused << requestHead.GetHeader(nsHttp::Cache_Control,
                                  cacheControlRequestHeader);
  CacheControlParser cacheControlRequest(cacheControlRequestHeader);
  if (cacheControlRequest.NoStore()) {
    // Don't use a no-store cache entry, accept the push.
    return NS_OK;
  }

  nsCString cachedAuth;
  rv = entry->GetMetaDataElement("auth", getter_Copies(cachedAuth));
  if (NS_SUCCEEDED(rv)) {
    uint32_t lastModifiedTime;
    rv = entry->GetLastModified(&lastModifiedTime);
    if (NS_SUCCEEDED(rv)) {
      if ((gHttpHandler->SessionStartTime() > lastModifiedTime) &&
          !cachedAuth.IsEmpty()) {
        // Need to revalidate this, as the auth is old. Accept the push.
        return NS_OK;
      }

      if (cachedAuth.IsEmpty() &&
          requestHead.HasHeader(nsHttp::Authorization)) {
        // They're pushing us something with auth, but we didn't cache anything
        // with auth. Accept the push.
        return NS_OK;
      }
    }
  }

  bool weaklyFramed, isImmutable;
  nsHttp::DetermineFramingAndImmutability(entry, &cachedResponseHead, true,
                                          &weaklyFramed, &isImmutable);

  // We'll need this value in later computations...
  uint32_t lastModifiedTime;
  rv = entry->GetLastModified(&lastModifiedTime);
  if (NS_FAILED(rv)) {
    // Ugh, this really sucks. OK, accept the push.
    return NS_OK;
  }

  // Determine if this is the first time that this cache entry
  // has been accessed during this session.
  bool fromPreviousSession =
      (gHttpHandler->SessionStartTime() > lastModifiedTime);

  bool validationRequired = nsHttp::ValidationRequired(
      isForcedValid, &cachedResponseHead, 0 /*NWGH: ??? - loadFlags*/, false,
      isImmutable, false, requestHead, entry, cacheControlRequest,
      fromPreviousSession);

  if (validationRequired) {
    // A real channel would most likely hit the net at this point, so let's
    // accept the push.
    return NS_OK;
  }

  // If we get here, then we would be able to use this cache entry. Cancel the
  // push so as not to waste any more bandwidth.
  acceptPush = false;
  return NS_OK;
}

NS_IMETHODIMP
CachePushChecker::OnCacheEntryAvailable(nsICacheEntry* entry, bool isNew,
                                        nsresult result) {
  // Nothing to do here, all the work is in OnCacheEntryCheck.
  return NS_OK;
}

void CachePushChecker::InvokeCallback(bool aResult) {
  RefPtr<CachePushChecker> self = this;
  auto task = [self, aResult]() { self->mCallback(aResult); };
  if (!mCurrentEventTarget->IsOnCurrentThread()) {
    mCurrentEventTarget->Dispatch(
        NS_NewRunnableFunction("CachePushChecker::InvokeCallback",
                               std::move(task)),
        NS_DISPATCH_NORMAL);
    return;
  }

  task();
}

}  // namespace net
}  // namespace mozilla
