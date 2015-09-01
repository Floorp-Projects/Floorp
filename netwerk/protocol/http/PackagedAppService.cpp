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
#include "mozilla/DebugOnly.h"
#include "nsIHttpHeaderVisitor.h"
#include "mozilla/LoadContext.h"

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

////////////////////////////////////////////////////////////////////////////////

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

namespace { // anon

class HeaderCopier final : public nsIHttpHeaderVisitor
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIHTTPHEADERVISITOR

  explicit HeaderCopier(nsHttpResponseHead* aHead)
    : mHead(aHead)
  {
  }

private:
  ~HeaderCopier() {}
  bool ShouldCopy(const nsACString& aHeader) const;

  nsHttpResponseHead* mHead;
};

NS_IMPL_ISUPPORTS(HeaderCopier, nsIHttpHeaderVisitor)

NS_IMETHODIMP
HeaderCopier::VisitHeader(const nsACString& aHeader, const nsACString& aValue)
{
  if (!ShouldCopy(aHeader)) {
    return NS_OK;
  }

  return mHead->SetHeader(nsHttp::ResolveAtom(aHeader), aValue);
}

bool
HeaderCopier::ShouldCopy(const nsACString &aHeader) const
{
  nsHttpAtom header = nsHttp::ResolveAtom(aHeader);

  // Don't overwrite the existing headers.
  if (mHead->PeekHeader(header)) {
    return false;
  }

  // A black list of headers we shouldn't copy.
  static const nsHttpAtom kHeadersCopyBlacklist[] = {
    nsHttp::Authentication,
    nsHttp::Cache_Control,
    nsHttp::Connection,
    nsHttp::Content_Disposition,
    nsHttp::Content_Encoding,
    nsHttp::Content_Language,
    nsHttp::Content_Length,
    nsHttp::Content_Location,
    nsHttp::Content_MD5,
    nsHttp::Content_Range,
    nsHttp::Content_Type,
    nsHttp::ETag,
    nsHttp::Last_Modified,
    nsHttp::Proxy_Authenticate,
    nsHttp::Proxy_Connection,
    nsHttp::Set_Cookie,
    nsHttp::Set_Cookie2,
    nsHttp::TE,
    nsHttp::Trailer,
    nsHttp::Transfer_Encoding,
    nsHttp::Vary,
    nsHttp::WWW_Authenticate,
  };

  // Loop through the black list to check if we should copy this header.
  for (uint32_t i = 0; i < mozilla::ArrayLength(kHeadersCopyBlacklist); i++) {
    if (header == kHeadersCopyBlacklist[i]) {
      return false;
    }
  }

  return true;
}

} // anon

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

  nsRefPtr<HeaderCopier> headerCopier = new HeaderCopier(aHead);
  return httpChan->VisitResponseHeaders(headerCopier);
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

////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS(PackagedAppService::PackagedAppChannelListener, nsIStreamListener)

NS_IMETHODIMP
PackagedAppService::PackagedAppChannelListener::OnStartRequest(nsIRequest *aRequest,
                                                               nsISupports *aContext)
{
  bool isFromCache = false;
  nsCOMPtr<nsICacheInfoChannel> cacheChan = do_QueryInterface(aRequest);
  if (cacheChan) {
      cacheChan->IsFromCache(&isFromCache);
  }

  mDownloader->SetIsFromCache(isFromCache);
  LOG(("[%p] Downloader isFromCache: %d\n", mDownloader.get(), isFromCache));

  // XXX: This is the place to suspend the channel, doom existing cache entries
  // for previous resources, and then resume the channel.
  return mListener->OnStartRequest(aRequest, aContext);
}

NS_IMETHODIMP
PackagedAppService::PackagedAppChannelListener::OnStopRequest(nsIRequest *aRequest,
                                                    nsISupports *aContext,
                                                    nsresult aStatusCode)
{
  return mListener->OnStopRequest(aRequest, aContext, aStatusCode);
}

NS_IMETHODIMP
PackagedAppService::PackagedAppChannelListener::OnDataAvailable(nsIRequest *aRequest,
                                                      nsISupports *aContext,
                                                      nsIInputStream *aInputStream,
                                                      uint64_t aOffset,
                                                      uint32_t aCount)
{
  return mListener->OnDataAvailable(aRequest, aContext, aInputStream, aOffset, aCount);
}


////////////////////////////////////////////////////////////////////////////////

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

  {
    // We use this temp URI to generate a path that is relative
    // to the package URI and not to the root of the domain.
    nsCOMPtr<nsIURI> tempURI;
    NS_NewURI(getter_AddRefs(tempURI), "http://temp-domain.local/");
    tempURI->SetPath(contentLocation);
    // The path is now normalized.
    tempURI->GetPath(contentLocation);
    // Remove the leading slash.
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
  // If the package response comes from the cache, we want to preserve the
  // statusCode, so ClearCallbacks looks for the resource in the cache, instead
  // of returning NS_ERROR_FILE_NOT_FOUND.
  if (NS_SUCCEEDED(aStatusCode) && lastPart && !mIsFromCache) {
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
    if (array->Length() == 0) {
      // The download of this resource has already completed, hence we don't
      // need to wait for it to be inserted in the cache and we can serve it
      // right now, directly.  See also the CallCallbacks method bellow.
      LOG(("[%p]    > already downloaded\n", this));
      mCacheStorage->AsyncOpenURI(aURI, EmptyCString(),
                                  nsICacheStorage::OPEN_READONLY, aCallback);
    } else {
      LOG(("[%p]    > adding to array\n", this));
      // Add this resource to the callback array
      array->AppendObject(aCallback);
    }
  } else {
    LOG(("[%p]    > creating array\n", this));
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
  aURI->GetAsciiSpec(spec);

  nsCOMArray<nsICacheEntryOpenCallback>* array = mCallbacks.Get(spec);
  if (array) {
    // Call all the callbacks for this URI
    for (uint32_t i = 0; i < array->Length(); ++i) {
      nsCOMPtr<nsICacheEntryOpenCallback> callback(array->ObjectAt(i));
      // We call to AsyncOpenURI which automatically calls the callback.
      mCacheStorage->AsyncOpenURI(aURI, EmptyCString(),
                                  nsICacheStorage::OPEN_READONLY, callback);
    }
    // Clear the array but leave it in the hashtable
    // An empty array means that the resource was already downloaded, and a
    // new call to AddCallback can simply return it from the cache.
    array->Clear();
    LOG(("[%p]    > called callbacks\n", this));
  } else {
    // There were no listeners waiting for this resource, but we insert a new
    // empty array into the hashtable so if any new callbacks are added while
    // downloading the package, we can simply return it from the cache.
    nsCOMArray<nsICacheEntryOpenCallback>* newArray =
      new nsCOMArray<nsICacheEntryOpenCallback>();
    mCallbacks.Put(spec, newArray);
    LOG(("[%p]    > created array\n", this));
  }

  aEntry->ForceValidFor(0);
  return NS_OK;
}

nsresult
PackagedAppService::PackagedAppDownloader::ClearCallbacks(nsresult aResult)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread(), "mCallbacks hashtable is not thread safe");
  LOG(("[%p] PackagedAppService::PackagedAppDownloader::ClearCallbacks > packageKey:%s status:%X\n",
       this, mPackageKey.get(), aResult));

  for (auto iter = mCallbacks.Iter(); !iter.Done(); iter.Next()) {
    const nsACString& key = iter.Key();
    const nsCOMArray<nsICacheEntryOpenCallback>* callbackArray = iter.UserData();

    if (NS_SUCCEEDED(aResult)) {
      // For success conditions we try to open the cache entry.
      // This can occur when the package metadata is served from the cache,
      // as it hasn't changed, but the entries are still in the cache.
      nsCOMPtr<nsIURI> uri;
      DebugOnly<nsresult> rv = NS_NewURI(getter_AddRefs(uri), key);
      MOZ_ASSERT(NS_SUCCEEDED(rv));

      LOG(("[%p]    > calling AsyncOpenURI for %s\n", this, key.BeginReading()));
      for (uint32_t i = 0; i < callbackArray->Length(); ++i) {
        nsCOMPtr<nsICacheEntryOpenCallback> callback = callbackArray->ObjectAt(i);
        mCacheStorage->AsyncOpenURI(uri, EmptyCString(),
                                    nsICacheStorage::OPEN_READONLY, callback);
      }

    } else { // an error has occured
      // We just call all the callbacks and pass the error result
      LOG(("[%p]    > passing NULL cache entry for %s\n", this, key.BeginReading()));
      for (uint32_t i = 0; i < callbackArray->Length(); ++i) {
        nsCOMPtr<nsICacheEntryOpenCallback> callback = callbackArray->ObjectAt(i);
        callback->OnCacheEntryAvailable(nullptr, false, nullptr, aResult);
      }
    }

    // Finally, we remove this entry from the hashtable.
    iter.Remove();
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

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
PackagedAppService::GetResource(nsIChannel *aChannel,
                                nsICacheEntryOpenCallback *aCallback)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread(), "mDownloadingPackages hashtable is not thread safe");
  LOG(("[%p] PackagedAppService::GetResource(aChannel: %p, aCallback: %p)\n",
       this, aChannel, aCallback));

  if (!aChannel || !aCallback) {
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv;
  nsIScriptSecurityManager *securityManager =
    nsContentUtils::GetSecurityManager();
  if (!securityManager) {
    LOG(("[%p]    > No securityManager\n", this));
    return NS_ERROR_UNEXPECTED;
  }
  nsCOMPtr<nsIPrincipal> principal;
  rv = securityManager->GetChannelURIPrincipal(aChannel, getter_AddRefs(principal));
  if (NS_FAILED(rv) || !principal) {
    LOG(("[%p]    > Error getting principal rv=%X principal=%p\n",
         this, rv, principal.get()));
    return NS_FAILED(rv) ? rv : NS_ERROR_NULL_POINTER;
  }

  nsCOMPtr<nsILoadContextInfo> loadContextInfo = GetLoadContextInfo(aChannel);
  if (!loadContextInfo) {
    LOG(("[%p]    > Channel has no loadContextInfo\n", this));
    return NS_ERROR_NULL_POINTER;
  }

  nsLoadFlags loadFlags = 0;
  rv = aChannel->GetLoadFlags(&loadFlags);
  if (NS_FAILED(rv)) {
    LOG(("[%p]    > Error calling GetLoadFlags rv=%X\n", this, rv));
    return rv;
  }

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->GetLoadInfo();

  nsCOMPtr<nsIURI> uri;
  rv = principal->GetURI(getter_AddRefs(uri));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG(("[%p]    > Error calling GetURI rv=%X\n", this, rv));
    return rv;
  }

  LogURI("PackagedAppService::GetResource", this, uri, loadContextInfo);
  nsCOMPtr<nsIURI> packageURI;
  rv = GetPackageURI(uri, getter_AddRefs(packageURI));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsAutoCString key;
  CacheFileUtils::AppendKeyPrefix(loadContextInfo, key);

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

    downloader->AddCallback(uri, aCallback);
    return NS_OK;
  }

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannelInternal(
    getter_AddRefs(channel), packageURI,
    loadInfo,
    nullptr, nullptr, loadFlags);

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
  rv = downloader->Init(loadContextInfo, key);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  downloader->AddCallback(uri, aCallback);

  nsCOMPtr<nsIStreamConverterService> streamconv =
    do_GetService("@mozilla.org/streamConverters;1", &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIStreamListener> mimeConverter;
  rv = streamconv->AsyncConvertData(APPLICATION_PACKAGE, "*/*", downloader, nullptr,
                                    getter_AddRefs(mimeConverter));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Add the package to the hashtable.
  mDownloadingPackages.Put(key, downloader);

  nsRefPtr<PackagedAppChannelListener> listener =
    new PackagedAppChannelListener(downloader, mimeConverter);

  nsCOMPtr<nsIInterfaceRequestor> loadContext;
  aChannel->GetNotificationCallbacks(getter_AddRefs(loadContext));
  if (loadContext) {
    channel->SetNotificationCallbacks(loadContext);
  }

  if (loadInfo && loadInfo->GetEnforceSecurity()) {
    return channel->AsyncOpen2(listener);
  }

  return channel->AsyncOpen(listener, nullptr);
}

nsresult
PackagedAppService::NotifyPackageDownloaded(nsCString aKey)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread(), "mDownloadingPackages hashtable is not thread safe");
  mDownloadingPackages.Remove(aKey);
  LOG(("[%p] PackagedAppService::NotifyPackageDownloaded > %s\n", this, aKey.get()));
  return NS_OK;
}

} // namespace net
} // namespace mozilla
