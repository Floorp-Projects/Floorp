/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "PackagedAppService.h"
#include "nsICacheStorage.h"
#include "LoadContextInfo.h"
#include "nsICacheStorageService.h"
#include "nsIResponseHeadProvider.h"
#include "nsIMultiPartChannel.h"
#include "../../cache2/CacheFileUtils.h"
#include "nsStreamUtils.h"
#include "mozilla/Logging.h"

namespace mozilla {
namespace net {

static PackagedAppService *gPackagedAppService = nullptr;

static PRLogModuleInfo *gPASLog = nullptr;
#undef LOG
#define LOG(args) MOZ_LOG(gPASLog, mozilla::LogLevel::Debug, args)

NS_IMPL_ISUPPORTS(PackagedAppService, nsIPackagedAppService)

NS_IMPL_ISUPPORTS(PackagedAppService::CacheEntryWriter, nsIStreamListener)

static void
LogURI(const char *aFunctionName, void *self, nsIURI *aURI, nsILoadContextInfo *aInfo = nullptr)
{
  if (MOZ_LOG_TEST(gPASLog, LogLevel::Debug)) {
    nsAutoCString spec;
    if (aURI) {
      aURI->GetAsciiSpec(spec);
    } else {
      spec = "(null)";
    }

    nsAutoCString prefix;
    if (aInfo) {
      CacheFileUtils::AppendKeyPrefix(aInfo, prefix);
      prefix += ":";
    }

    LOG(("[%p] %s > %s%s\n", self, aFunctionName, prefix.get(), spec.get()));
  }
}

/* static */ nsresult
PackagedAppService::CacheEntryWriter::Create(nsIURI *aURI,
                                             nsICacheStorage *aStorage,
                                             CacheEntryWriter **aResult)
{
  nsRefPtr<CacheEntryWriter> writer = new CacheEntryWriter();
  nsresult rv = aStorage->OpenTruncate(aURI, EmptyCString(),
                                       getter_AddRefs(writer->mEntry));
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = writer->mEntry->ForceValidFor(PR_UINT32_MAX);
  if (NS_FAILED(rv)) {
    return rv;
  }

  writer.forget(aResult);
  return NS_OK;
}

nsresult
PackagedAppService::CacheEntryWriter::CopySecurityInfo(nsIChannel *aChannel)
{
  if (!aChannel) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsISupports> securityInfo;
  aChannel->GetSecurityInfo(getter_AddRefs(securityInfo));
  if (securityInfo) {
    mEntry->SetSecurityInfo(securityInfo);
  }

  return NS_OK;
}

/* static */ nsresult
PackagedAppService::CacheEntryWriter::CopyHeadersFromChannel(nsIChannel *aChannel,
                                                  nsHttpResponseHead *aHead)
{
  if (!aChannel || !aHead) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIHttpChannel> httpChan = do_QueryInterface(aChannel);
  if (!httpChan) {
    return NS_ERROR_FAILURE;
  }

  nsAutoCString value;
  httpChan->GetResponseHeader(NS_LITERAL_CSTRING("Content-Security-Policy"), value);
  aHead->SetHeader(nsHttp::ResolveAtom("Content-Security-Policy"), value);

  return NS_OK;
}

NS_METHOD
PackagedAppService::CacheEntryWriter::ConsumeData(nsIInputStream *aStream,
                                                  void *aClosure,
                                                  const char *aFromRawSegment,
                                                  uint32_t aToOffset,
                                                  uint32_t aCount,
                                                  uint32_t *aWriteCount)
{
  MOZ_ASSERT(aClosure, "The closure must not be null");
  CacheEntryWriter *self = static_cast<CacheEntryWriter*>(aClosure);
  MOZ_ASSERT(self->mOutputStream, "The stream should not be null");
  return self->mOutputStream->Write(aFromRawSegment, aCount, aWriteCount);
}

NS_IMETHODIMP
PackagedAppService::CacheEntryWriter::OnStartRequest(nsIRequest *aRequest,
                                                     nsISupports *aContext)
{
  nsresult rv;
  nsCOMPtr<nsIResponseHeadProvider> provider(do_QueryInterface(aRequest));
  if (!provider) {
    return NS_ERROR_INVALID_ARG;
  }
  nsHttpResponseHead *responseHead = provider->GetResponseHead();
  if (!responseHead) {
    return NS_ERROR_FAILURE;
  }

  mEntry->SetPredictedDataSize(responseHead->TotalEntitySize());

  rv = mEntry->SetMetaDataElement("request-method", "GET");
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIMultiPartChannel> multiPartChannel = do_QueryInterface(aRequest);
  if (!multiPartChannel) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIChannel> baseChannel;
  multiPartChannel->GetBaseChannel(getter_AddRefs(baseChannel));

  rv = CopySecurityInfo(baseChannel);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = CopyHeadersFromChannel(baseChannel, responseHead);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsAutoCString head;
  responseHead->Flatten(head, true);
  rv = mEntry->SetMetaDataElement("response-head", head.get());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mEntry->OpenOutputStream(0, getter_AddRefs(mOutputStream));
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
PackagedAppService::CacheEntryWriter::OnStopRequest(nsIRequest *aRequest,
                                                    nsISupports *aContext,
                                                    nsresult aStatusCode)
{
  if (mOutputStream) {
    mOutputStream->Close();
    mOutputStream = nullptr;
  }

  return NS_OK;
}

NS_IMETHODIMP
PackagedAppService::CacheEntryWriter::OnDataAvailable(nsIRequest *aRequest,
                                                      nsISupports *aContext,
                                                      nsIInputStream *aInputStream,
                                                      uint64_t aOffset,
                                                      uint32_t aCount)
{
  if (!aInputStream) {
    return NS_ERROR_INVALID_ARG;
  }
  // Calls ConsumeData to read the data into the cache entry
  uint32_t n;
  return aInputStream->ReadSegments(ConsumeData, this, aCount, &n);
}


NS_IMPL_ISUPPORTS(PackagedAppService::PackagedAppDownloader, nsIStreamListener)

nsresult
PackagedAppService::PackagedAppDownloader::Init(nsILoadContextInfo* aInfo,
                                                const nsCString& aKey)
{
  nsresult rv;
  nsCOMPtr<nsICacheStorageService> cacheStorageService =
    do_GetService("@mozilla.org/netwerk/cache-storage-service;1", &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = cacheStorageService->DiskCacheStorage(aInfo, false,
                                             getter_AddRefs(mCacheStorage));
  if (NS_FAILED(rv)) {
    return rv;
  }

  mPackageKey = aKey;
  return NS_OK;
}

NS_IMETHODIMP
PackagedAppService::PackagedAppDownloader::OnStartRequest(nsIRequest *aRequest,
                                                          nsISupports *aContext)
{
  // In case an error occurs in this method mWriter should be null
  // so we don't accidentally write to the previous resource's cache entry.
  mWriter = nullptr;

  nsCOMPtr<nsIURI> uri;
  nsresult rv = GetSubresourceURI(aRequest, getter_AddRefs(uri));

  LogURI("PackagedAppDownloader::OnStartRequest", this, uri);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_OK;
  }

  rv = CacheEntryWriter::Create(uri, mCacheStorage, getter_AddRefs(mWriter));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_OK;
  }

  MOZ_ASSERT(mWriter);
  rv = mWriter->OnStartRequest(aRequest, aContext);
  NS_WARN_IF(NS_FAILED(rv));
  return NS_OK;
}

nsresult
PackagedAppService::PackagedAppDownloader::GetSubresourceURI(nsIRequest * aRequest,
                                                             nsIURI ** aResult)
{
  nsresult rv;
  nsCOMPtr<nsIResponseHeadProvider> provider(do_QueryInterface(aRequest));
  nsCOMPtr<nsIChannel> chan(do_QueryInterface(aRequest));

  if (NS_WARN_IF(!provider || !chan)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsHttpResponseHead *responseHead = provider->GetResponseHead();
  if (NS_WARN_IF(!responseHead)) {
    return NS_ERROR_FAILURE;
  }
  nsAutoCString contentLocation;
  rv = responseHead->GetHeader(nsHttp::ResolveAtom("Content-Location"), contentLocation);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIURI> uri;
  rv = chan->GetURI(getter_AddRefs(uri));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsAutoCString path;
  rv = uri->GetPath(path);
  if (NS_FAILED(rv)) {
    return rv;
  }

  path += PACKAGED_APP_TOKEN;

  // TODO: make sure the path is normalized
  if (StringBeginsWith(contentLocation, NS_LITERAL_CSTRING("/"))) {
    contentLocation = Substring(contentLocation, 1);
  }

  path += contentLocation;

  nsCOMPtr<nsIURI> partURI;
  rv = uri->CloneIgnoringRef(getter_AddRefs(partURI));
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = partURI->SetPath(path);
  if (NS_FAILED(rv)) {
    return rv;
  }

  partURI.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
PackagedAppService::PackagedAppDownloader::OnStopRequest(nsIRequest *aRequest,
                                                         nsISupports *aContext,
                                                         nsresult aStatusCode)
{
  nsCOMPtr<nsIMultiPartChannel> multiChannel(do_QueryInterface(aRequest));
  nsresult rv;

  LOG(("[%p] PackagedAppDownloader::OnStopRequest > status:%X multiChannel:%p\n",
       this, aStatusCode, multiChannel.get()));

  // The request is normally a multiPartChannel. If it isn't, it generally means
  // an error has occurred in nsMultiMixedConv.
  // If an error occurred in OnStartRequest, mWriter could be null.
  if (multiChannel && mWriter) {
    mWriter->OnStopRequest(aRequest, aContext, aStatusCode);

    nsCOMPtr<nsIURI> uri;
    rv = GetSubresourceURI(aRequest, getter_AddRefs(uri));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return NS_OK;
    }

    nsCOMPtr<nsICacheEntry> entry;
    mWriter->mEntry.swap(entry);

    // We don't need the writer anymore - this will close its stream
    mWriter = nullptr;
    CallCallbacks(uri, entry, aStatusCode);
  }

  // lastPart will be true if this is the last part in the package,
  // or if aRequest isn't a multipart channel
  bool lastPart = true;
  if (multiChannel) {
    rv = multiChannel->GetIsLastPart(&lastPart);
    if (NS_SUCCEEDED(rv) && !lastPart) {
      // If this isn't the last part, we don't do the cleanup yet
      return NS_OK;
    }
  }

  // If this is the last part of the package, it means the requested resources
  // have not been found in the package so we return an appropriate error.
  if (NS_SUCCEEDED(aStatusCode) && lastPart) {
    aStatusCode = NS_ERROR_FILE_NOT_FOUND;
  }

  nsRefPtr<PackagedAppDownloader> kungFuDeathGrip(this);
  // NotifyPackageDownloaded removes the ref from the array. Keep a temp ref
  if (gPackagedAppService) {
    gPackagedAppService->NotifyPackageDownloaded(mPackageKey);
  }
  ClearCallbacks(aStatusCode);
  return NS_OK;
}

NS_IMETHODIMP
PackagedAppService::PackagedAppDownloader::OnDataAvailable(nsIRequest *aRequest,
                                                           nsISupports *aContext,
                                                           nsIInputStream *aInputStream,
                                                           uint64_t aOffset,
                                                           uint32_t aCount)
{
  if (!mWriter) {
    uint32_t n;
    return aInputStream->ReadSegments(NS_DiscardSegment, nullptr, aCount, &n);
  }
  return mWriter->OnDataAvailable(aRequest, aContext, aInputStream, aOffset,
                                  aCount);
}

nsresult
PackagedAppService::PackagedAppDownloader::AddCallback(nsIURI *aURI,
                                                       nsICacheEntryOpenCallback *aCallback)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread(), "mCallbacks hashtable is not thread safe");
  nsAutoCString spec;
  aURI->GetAsciiSpec(spec);

  LogURI("PackagedAppDownloader::AddCallback", this, aURI);
  LOG(("[%p]    > callback: %p\n", this, aCallback));

  // Check if we already have a resource waiting for this resource
  nsCOMArray<nsICacheEntryOpenCallback>* array = mCallbacks.Get(spec);
  if (array) {
    // Add this resource to the callback array
    array->AppendObject(aCallback);
  } else {
    // This is the first callback for this URI.
    // Create a new array and add the callback
    nsCOMArray<nsICacheEntryOpenCallback>* newArray =
      new nsCOMArray<nsICacheEntryOpenCallback>();
    newArray->AppendObject(aCallback);
    mCallbacks.Put(spec, newArray);
  }
  return NS_OK;
}

nsresult
PackagedAppService::PackagedAppDownloader::CallCallbacks(nsIURI *aURI,
                                                         nsICacheEntry *aEntry,
                                                         nsresult aResult)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread(), "mCallbacks hashtable is not thread safe");
  // Hold on to this entry while calling the callbacks
  nsCOMPtr<nsICacheEntry> handle(aEntry);

  LogURI("PackagedAppService::PackagedAppDownloader::CallCallbacks", this, aURI);
  LOG(("[%p]    > status:%X\n", this, aResult));

  nsAutoCString spec;
  aURI->GetSpec(spec);

  nsCOMArray<nsICacheEntryOpenCallback>* array = mCallbacks.Get(spec);
  if (array) {
    // Call all the callbacks for this URI
    for (uint32_t i = 0; i < array->Length(); ++i) {
      nsCOMPtr<nsICacheEntryOpenCallback> callback(array->ObjectAt(i));
      // We call to AsyncOpenURI which automatically calls the callback.
      mCacheStorage->AsyncOpenURI(aURI, EmptyCString(),
                                  nsICacheStorage::OPEN_READONLY, callback);
    }
    // Clear the array and remove it from the hashtable
    array->Clear();
    mCallbacks.Remove(spec);
    aEntry->ForceValidFor(0);
  }
  return NS_OK;
}

PLDHashOperator
PackagedAppService::PackagedAppDownloader::ClearCallbacksEnumerator(const nsACString& key,
  nsAutoPtr<nsCOMArray<nsICacheEntryOpenCallback> >& callbackArray,
  void* arg)
{
  MOZ_ASSERT(arg, "The void* parameter should be a pointer to nsresult");
  nsresult *result = static_cast<nsresult*>(arg);
  for (uint32_t i = 0; i < callbackArray->Length(); ++i) {
    nsCOMPtr<nsICacheEntryOpenCallback> callback = callbackArray->ObjectAt(i);
    callback->OnCacheEntryAvailable(nullptr, false, nullptr, *result);
  }
  // Remove entry from hashtable
  return PL_DHASH_REMOVE;
}

nsresult
PackagedAppService::PackagedAppDownloader::ClearCallbacks(nsresult aResult)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread(), "mCallbacks hashtable is not thread safe");
  LOG(("[%p] PackagedAppService::PackagedAppDownloader::ClearCallbacks > packageKey:%s status:%X\n",
       this, mPackageKey.get(), aResult));
  mCallbacks.Enumerate(ClearCallbacksEnumerator, &aResult);
  return NS_OK;
}

NS_IMPL_ISUPPORTS(PackagedAppService::CacheEntryChecker, nsICacheEntryOpenCallback)

NS_IMETHODIMP
PackagedAppService::CacheEntryChecker::OnCacheEntryCheck(nsICacheEntry *aEntry,
                                                         nsIApplicationCache *aApplicationCache,
                                                         uint32_t *_retval)
{
  nsresult rv = mCallback->OnCacheEntryCheck(aEntry, aApplicationCache, _retval);
  LOG(("[%p] PackagedAppService::CacheEntryChecker::OnCacheEntryCheck > rv=%X retval=%X\n",
       this, rv, _retval));
  return rv;
}

NS_IMETHODIMP
PackagedAppService::CacheEntryChecker::OnCacheEntryAvailable(nsICacheEntry *aEntry,
                                                             bool aNew,
                                                             nsIApplicationCache *aApplicationCache,
                                                             nsresult aResult)
{
  LogURI("PackagedAppService::CacheEntryChecker::OnCacheEntryAvailable",
         this, mURI, mLoadContextInfo);
  if (aResult == NS_ERROR_CACHE_KEY_NOT_FOUND) {
    MOZ_ASSERT(!aEntry, "No entry");
    LOG(("[%p]    > NOT FOUND\n", this));
    // trigger download
    // download checks if package download is already in progress
    gPackagedAppService->OpenNewPackageInternal(mURI, mCallback,
                                                mLoadContextInfo);
  } else {
    LOG(("[%p]    > FOUND ENTRY status:%X entry:%p\n", this, aResult, aEntry));
    // TODO: if aResult is another error code, should we pass it off to the
    // consumer, or should we try to download the package again?
    mCallback->OnCacheEntryAvailable(aEntry, aNew, aApplicationCache, aResult);
    // TODO: update last access entry for the entire package
  }
  return NS_OK;
}

PackagedAppService::PackagedAppService()
{
  gPackagedAppService = this;
  gPASLog = PR_NewLogModule("PackagedAppService");
  LOG(("[%p] Created PackagedAppService\n", this));
}

PackagedAppService::~PackagedAppService()
{
  LOG(("[%p] Destroying PackagedAppService\n", this));
  gPackagedAppService = nullptr;
}

/* static */ nsresult
PackagedAppService::GetPackageURI(nsIURI *aURI, nsIURI **aPackageURI)
{
  nsCOMPtr<nsIURL> url = do_QueryInterface(aURI);
  if (!url) {
    return NS_ERROR_INVALID_ARG;
  }

  nsAutoCString path;
  nsresult rv = url->GetFilePath(path);
  if (NS_FAILED(rv)) {
    return rv;
  }

  int32_t pos = path.Find(PACKAGED_APP_TOKEN);
  if (pos == kNotFound) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIURI> packageURI;
  rv = aURI->CloneIgnoringRef(getter_AddRefs(packageURI));
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = packageURI->SetPath(Substring(path, 0, pos));
  if (NS_FAILED(rv)) {
    return rv;
  }

  packageURI.forget(aPackageURI);

  return NS_OK;
}

NS_IMETHODIMP
PackagedAppService::RequestURI(nsIURI *aURI,
                               nsILoadContextInfo *aInfo,
                               nsICacheEntryOpenCallback *aCallback)
{
  // Check arguments are not null
  if (!aURI || !aCallback || !aInfo) {
    return NS_ERROR_INVALID_ARG;
  }

  nsAutoCString path;
  aURI->GetPath(path);
  int32_t pos = path.Find(PACKAGED_APP_TOKEN);
  if (pos == kNotFound) {
    return NS_ERROR_INVALID_ARG;
  }

  LogURI("PackagedAppService::RequestURI", this, aURI, aInfo);

  nsresult rv;
  nsCOMPtr<nsICacheStorageService> cacheStorageService =
    do_GetService("@mozilla.org/netwerk/cache-storage-service;1", &rv);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsICacheStorage> cacheStorage;

  rv = cacheStorageService->DiskCacheStorage(aInfo, false,
                                             getter_AddRefs(cacheStorage));

  nsRefPtr<CacheEntryChecker> checker = new CacheEntryChecker(aURI, aCallback, aInfo);
  return cacheStorage->AsyncOpenURI(aURI, EmptyCString(),
                                    nsICacheStorage::OPEN_READONLY, checker);
}

nsresult
PackagedAppService::NotifyPackageDownloaded(nsCString aKey)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread(), "mDownloadingPackages hashtable is not thread safe");
  mDownloadingPackages.Remove(aKey);
  LOG(("[%p] PackagedAppService::NotifyPackageDownloaded > %s\n", this, aKey.get()));
  return NS_OK;
}

nsresult
PackagedAppService::OpenNewPackageInternal(nsIURI *aURI,
                                           nsICacheEntryOpenCallback *aCallback,
                                           nsILoadContextInfo *aInfo)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread(), "mDownloadingPackages hashtable is not thread safe");

  nsAutoCString path;
  nsresult rv = aURI->GetPath(path);
  if (NS_FAILED(rv)) {
    return rv;
  }

  int32_t pos = path.Find(PACKAGED_APP_TOKEN);
  MOZ_ASSERT(pos != kNotFound,
             "This should never be called if the token is missing");

  nsCOMPtr<nsIURI> packageURI;
  rv = aURI->CloneIgnoringRef(getter_AddRefs(packageURI));
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = packageURI->SetPath(Substring(path, 0, pos));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsAutoCString key;
  CacheFileUtils::AppendKeyPrefix(aInfo, key);

  {
    nsAutoCString spec;
    packageURI->GetAsciiSpec(spec);
    key += ":";
    key += spec;
  }

  nsRefPtr<PackagedAppDownloader> downloader;
  if (mDownloadingPackages.Get(key, getter_AddRefs(downloader))) {
    // We have determined that the file is not in the cache.
    // If we find that the package that the file belongs to is currently being
    // downloaded, we will add the callback to the package's queue, and it will
    // be called once the file is processed and saved in the cache.

    downloader->AddCallback(aURI, aCallback);
    return NS_OK;
  }

  // We need to set this flag, because the package metadata
  // needs to have a separate entry for anonymous channels.
  uint32_t extra_flags = 0;
  if (aInfo->IsAnonymous()) {
    extra_flags = nsIRequest::LOAD_ANONYMOUS;
  }

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(
    getter_AddRefs(channel), packageURI, nsContentUtils::GetSystemPrincipal(),
    nsILoadInfo::SEC_NORMAL, nsIContentPolicy::TYPE_OTHER, nullptr, nullptr,
    nsIRequest::LOAD_NORMAL | extra_flags);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsICachingChannel> cacheChan(do_QueryInterface(channel));
  if (cacheChan) {
    // Each resource in the package will be put in its own cache entry
    // during the first load of the package, so we only want the channel to
    // cache the response head, not the entire content of the package.
    cacheChan->SetCacheOnlyMetadata(true);
  }

  downloader = new PackagedAppDownloader();
  rv = downloader->Init(aInfo, key);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  downloader->AddCallback(aURI, aCallback);

  nsCOMPtr<nsIStreamConverterService> streamconv =
    do_GetService("@mozilla.org/streamConverters;1", &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIStreamListener> mimeConverter;
  // Passing `this` as context, so nsMultiMixedConv knows this is a packaged app
  rv = streamconv->AsyncConvertData("multipart/mixed", "*/*", downloader, this,
                                    getter_AddRefs(mimeConverter));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Add the package to the hashtable.
  mDownloadingPackages.Put(key, downloader);

  return channel->AsyncOpen(mimeConverter, nullptr);
}

} // namespace net
} // namespace mozilla
