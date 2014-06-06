// Stuff to link the old imp to the new api - will go away!

#include "CacheLog.h"
#include "OldWrappers.h"
#include "CacheStorage.h"
#include "CacheStorageService.h"
#include "LoadContextInfo.h"
#include "nsCacheService.h"

#include "nsIURI.h"
#include "nsICacheSession.h"
#include "nsIApplicationCache.h"
#include "nsIApplicationCacheService.h"
#include "nsIStreamTransportService.h"
#include "nsIFile.h"
#include "nsICacheEntryDoomCallback.h"
#include "nsICacheListener.h"
#include "nsICacheStorageVisitor.h"

#include "nsServiceManagerUtils.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsProxyRelease.h"
#include "mozilla/Telemetry.h"

static NS_DEFINE_CID(kStreamTransportServiceCID,
                     NS_STREAMTRANSPORTSERVICE_CID);

static uint32_t const CHECK_MULTITHREADED = nsICacheStorage::CHECK_MULTITHREADED;

namespace mozilla {
namespace net {

namespace { // anon

// Fires the doom callback back on the main thread
// after the cache I/O thread is looped.

class DoomCallbackSynchronizer : public nsRunnable
{
public:
  DoomCallbackSynchronizer(nsICacheEntryDoomCallback* cb) : mCB(cb)
  {
    MOZ_COUNT_CTOR(DoomCallbackSynchronizer);
  }
  nsresult Dispatch();

private:
  virtual ~DoomCallbackSynchronizer()
  {
    MOZ_COUNT_DTOR(DoomCallbackSynchronizer);
  }

  NS_DECL_NSIRUNNABLE
  nsCOMPtr<nsICacheEntryDoomCallback> mCB;
};

nsresult DoomCallbackSynchronizer::Dispatch()
{
  nsresult rv;

  nsCOMPtr<nsICacheService> serv =
      do_GetService(NS_CACHESERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIEventTarget> eventTarget;
  rv = serv->GetCacheIOTarget(getter_AddRefs(eventTarget));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = eventTarget->Dispatch(this, NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP DoomCallbackSynchronizer::Run()
{
  if (!NS_IsMainThread()) {
    NS_DispatchToMainThread(this);
  }
  else {
    if (mCB)
      mCB->OnCacheEntryDoomed(NS_OK);
  }
  return NS_OK;
}

// Receives doom callback from the old API and forwards to the new API

class DoomCallbackWrapper : public nsICacheListener
{
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICACHELISTENER

  DoomCallbackWrapper(nsICacheEntryDoomCallback* cb) : mCB(cb)
  {
    MOZ_COUNT_CTOR(DoomCallbackWrapper);
  }

private:
  virtual ~DoomCallbackWrapper()
  {
    MOZ_COUNT_DTOR(DoomCallbackWrapper);
  }

  nsCOMPtr<nsICacheEntryDoomCallback> mCB;
};

NS_IMPL_ISUPPORTS(DoomCallbackWrapper, nsICacheListener);

NS_IMETHODIMP DoomCallbackWrapper::OnCacheEntryAvailable(nsICacheEntryDescriptor *descriptor,
                                                         nsCacheAccessMode accessGranted,
                                                         nsresult status)
{
  return NS_OK;
}

NS_IMETHODIMP DoomCallbackWrapper::OnCacheEntryDoomed(nsresult status)
{
  if (!mCB)
    return NS_ERROR_NULL_POINTER;

  mCB->OnCacheEntryDoomed(status);
  mCB = nullptr;
  return NS_OK;
}

} // anon

// _OldVisitCallbackWrapper
// Receives visit callbacks from the old API and forwards it to the new API

NS_IMPL_ISUPPORTS(_OldVisitCallbackWrapper, nsICacheVisitor)

_OldVisitCallbackWrapper::~_OldVisitCallbackWrapper()
{
  if (!mHit) {
    // The device has not been found, to not break the chain, simulate
    // storage info callback.
    mCB->OnCacheStorageInfo(0, 0, 0, nullptr);
  }

  if (mVisitEntries) {
    mCB->OnCacheEntryVisitCompleted();
  }

  MOZ_COUNT_DTOR(_OldVisitCallbackWrapper);
}

NS_IMETHODIMP _OldVisitCallbackWrapper::VisitDevice(const char * deviceID,
                                                    nsICacheDeviceInfo *deviceInfo,
                                                    bool *_retval)
{
  if (!mCB)
    return NS_ERROR_NULL_POINTER;

  *_retval = false;
  if (strcmp(deviceID, mDeviceID)) {
    // Not the device we want to visit
    return NS_OK;
  }

  mHit = true;

  nsresult rv;

  uint32_t capacity;
  rv = deviceInfo->GetMaximumSize(&capacity);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFile> dir;
  if (!strcmp(mDeviceID, "disk")) {
    nsCacheService::GetDiskCacheDirectory(getter_AddRefs(dir));
  } else if (!strcmp(mDeviceID, "offline")) {
    nsCacheService::GetAppCacheDirectory(getter_AddRefs(dir));
  }

  if (mLoadInfo->IsAnonymous()) {
    // Anonymous visiting reports 0, 0 since we cannot count that
    // early the number of anon entries.
    mCB->OnCacheStorageInfo(0, 0, capacity, dir);
  } else {
    // Non-anon visitor counts all non-anon + ALL ANON entries,
    // there is no way to determine the number of entries when
    // using the old cache APIs - there is no concept of anonymous
    // storage.
    uint32_t entryCount;
    rv = deviceInfo->GetEntryCount(&entryCount);
    NS_ENSURE_SUCCESS(rv, rv);

    uint32_t totalSize;
    rv = deviceInfo->GetTotalSize(&totalSize);
    NS_ENSURE_SUCCESS(rv, rv);

    mCB->OnCacheStorageInfo(entryCount, totalSize, capacity, dir);
  }

  *_retval = mVisitEntries;
  return NS_OK;
}

NS_IMETHODIMP _OldVisitCallbackWrapper::VisitEntry(const char * deviceID,
                                                   nsICacheEntryInfo *entryInfo,
                                                   bool *_retval)
{
  MOZ_ASSERT(!strcmp(deviceID, mDeviceID));

  nsresult rv;

  *_retval = true;

  // Read all informative properties from the entry.
  nsXPIDLCString clientId;
  rv = entryInfo->GetClientID(getter_Copies(clientId));
  if (NS_FAILED(rv))
    return NS_OK;

  if (mLoadInfo->IsPrivate() !=
      StringBeginsWith(clientId, NS_LITERAL_CSTRING("HTTP-memory-only-PB"))) {
    return NS_OK;
  }

  nsAutoCString cacheKey, enhanceId;
  rv = entryInfo->GetKey(cacheKey);
  if (NS_FAILED(rv))
    return NS_OK;

  if (StringBeginsWith(cacheKey, NS_LITERAL_CSTRING("anon&"))) {
    if (!mLoadInfo->IsAnonymous())
      return NS_OK;

    cacheKey = Substring(cacheKey, 5, cacheKey.Length());
  } else if (mLoadInfo->IsAnonymous()) {
    return NS_OK;
  }

  if (StringBeginsWith(cacheKey, NS_LITERAL_CSTRING("id="))) {
    int32_t uriSpecEnd = cacheKey.Find("&uri=");
    if (uriSpecEnd == kNotFound) // Corrupted, ignore
      return NS_OK;

    enhanceId = Substring(cacheKey, 3, uriSpecEnd - 3);
    cacheKey = Substring(cacheKey, uriSpecEnd + 1, cacheKey.Length());
  }

  if (StringBeginsWith(cacheKey, NS_LITERAL_CSTRING("uri="))) {
    cacheKey = Substring(cacheKey, 4, cacheKey.Length());
  }

  nsCOMPtr<nsIURI> uri;
  // cacheKey is strip of any prefixes
  rv = NS_NewURI(getter_AddRefs(uri), cacheKey);
  if (NS_FAILED(rv))
    return NS_OK;

  uint32_t dataSize;
  if (NS_FAILED(entryInfo->GetDataSize(&dataSize)))
    dataSize = 0;
  int32_t fetchCount;
  if (NS_FAILED(entryInfo->GetFetchCount(&fetchCount)))
    fetchCount = 0;
  uint32_t expirationTime;
  if (NS_FAILED(entryInfo->GetExpirationTime(&expirationTime)))
    expirationTime = 0;
  uint32_t lastModified;
  if (NS_FAILED(entryInfo->GetLastModified(&lastModified)))
    lastModified = 0;

  // Send them to the consumer.
  rv = mCB->OnCacheEntryInfo(
    uri, enhanceId, (int64_t)dataSize, fetchCount, lastModified, expirationTime);

  *_retval = NS_SUCCEEDED(rv);
  return NS_OK;
}

// _OldGetDiskConsumption

//static
nsresult _OldGetDiskConsumption::Get(nsICacheStorageConsumptionObserver* aCallback)
{
  nsresult rv;

  nsCOMPtr<nsICacheService> serv =
      do_GetService(NS_CACHESERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<_OldGetDiskConsumption> cb = new _OldGetDiskConsumption(aCallback);

  // _OldGetDiskConsumption stores the found size value, but until dispatched
  // to the main thread it doesn't call on the consupmtion observer. See bellow.
  rv = serv->VisitEntries(cb);
  NS_ENSURE_SUCCESS(rv, rv);

  // We are called from CacheStorageService::AsyncGetDiskConsumption whose IDL
  // documentation claims the callback is always delievered asynchronously
  // back to the main thread.  Despite we know the result synchronosusly when
  // querying the old cache, we need to stand the word and dispatch the result
  // to the main thread asynchronously.  Hence the dispatch here.
  return NS_DispatchToMainThread(cb);
}

NS_IMPL_ISUPPORTS_INHERITED(_OldGetDiskConsumption,
                            nsRunnable,
                            nsICacheVisitor)

_OldGetDiskConsumption::_OldGetDiskConsumption(
  nsICacheStorageConsumptionObserver* aCallback)
  : mCallback(aCallback)
  , mSize(0)
{
}

NS_IMETHODIMP
_OldGetDiskConsumption::Run()
{
  mCallback->OnNetworkCacheDiskConsumption(mSize);
  return NS_OK;
}

NS_IMETHODIMP
_OldGetDiskConsumption::VisitDevice(const char * deviceID,
                                    nsICacheDeviceInfo *deviceInfo,
                                    bool *_retval)
{
  if (!strcmp(deviceID, "disk")) {
    uint32_t size;
    nsresult rv = deviceInfo->GetTotalSize(&size);
    if (NS_SUCCEEDED(rv))
      mSize = (int64_t)size;
  }

  *_retval = false;
  return NS_OK;
}

NS_IMETHODIMP
_OldGetDiskConsumption::VisitEntry(const char * deviceID,
                                   nsICacheEntryInfo *entryInfo,
                                   bool *_retval)
{
  MOZ_CRASH("Unexpected");
  return NS_OK;
}


// _OldCacheEntryWrapper

_OldCacheEntryWrapper::_OldCacheEntryWrapper(nsICacheEntryDescriptor* desc)
: mOldDesc(desc), mOldInfo(desc)
{
  MOZ_COUNT_CTOR(_OldCacheEntryWrapper);
  LOG(("Creating _OldCacheEntryWrapper %p for descriptor %p", this, desc));
}

_OldCacheEntryWrapper::_OldCacheEntryWrapper(nsICacheEntryInfo* info)
: mOldDesc(nullptr), mOldInfo(info)
{
  MOZ_COUNT_CTOR(_OldCacheEntryWrapper);
  LOG(("Creating _OldCacheEntryWrapper %p for info %p", this, info));
}

_OldCacheEntryWrapper::~_OldCacheEntryWrapper()
{
  MOZ_COUNT_DTOR(_OldCacheEntryWrapper);
  LOG(("Destroying _OldCacheEntryWrapper %p for descriptor %p", this, mOldInfo.get()));
}

NS_IMPL_ISUPPORTS(_OldCacheEntryWrapper, nsICacheEntry)

NS_IMETHODIMP _OldCacheEntryWrapper::AsyncDoom(nsICacheEntryDoomCallback* listener)
{
  nsRefPtr<DoomCallbackWrapper> cb = listener
    ? new DoomCallbackWrapper(listener)
    : nullptr;
  return AsyncDoom(cb);
}

NS_IMETHODIMP _OldCacheEntryWrapper::GetDataSize(int64_t *aSize)
{
  uint32_t size;
  nsresult rv = GetDataSize(&size);
  if (NS_FAILED(rv))
    return rv;

  *aSize = size;
  return NS_OK;
}

NS_IMETHODIMP _OldCacheEntryWrapper::GetPersistent(bool *aPersistToDisk)
{
  if (!mOldDesc) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult rv;

  nsCacheStoragePolicy policy;
  rv = mOldDesc->GetStoragePolicy(&policy);
  NS_ENSURE_SUCCESS(rv, rv);

  *aPersistToDisk = policy != nsICache::STORE_IN_MEMORY;

  return NS_OK;
}

NS_IMETHODIMP _OldCacheEntryWrapper::Recreate(bool aMemoryOnly,
                                              nsICacheEntry** aResult)
{
  NS_ENSURE_TRUE(mOldDesc, NS_ERROR_NOT_AVAILABLE);

  nsCacheAccessMode mode;
  nsresult rv = mOldDesc->GetAccessGranted(&mode);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!(mode & nsICache::ACCESS_WRITE))
    return NS_ERROR_NOT_AVAILABLE;

  LOG(("_OldCacheEntryWrapper::Recreate [this=%p]", this));

  if (aMemoryOnly)
    mOldDesc->SetStoragePolicy(nsICache::STORE_IN_MEMORY);

  nsCOMPtr<nsICacheEntry> self(this);
  self.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP _OldCacheEntryWrapper::OpenInputStream(int64_t offset,
                                                     nsIInputStream * *_retval)
{
  if (offset > PR_UINT32_MAX)
    return NS_ERROR_INVALID_ARG;

  return OpenInputStream(uint32_t(offset), _retval);
}
NS_IMETHODIMP _OldCacheEntryWrapper::OpenOutputStream(int64_t offset,
                                                      nsIOutputStream * *_retval)
{
  if (offset > PR_UINT32_MAX)
    return NS_ERROR_INVALID_ARG;

  return OpenOutputStream(uint32_t(offset), _retval);
}

NS_IMETHODIMP _OldCacheEntryWrapper::MaybeMarkValid()
{
  LOG(("_OldCacheEntryWrapper::MaybeMarkValid [this=%p]", this));

  NS_ENSURE_TRUE(mOldDesc, NS_ERROR_NULL_POINTER);

  nsCacheAccessMode mode;
  nsresult rv = mOldDesc->GetAccessGranted(&mode);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mode & nsICache::ACCESS_WRITE) {
    LOG(("Marking cache entry valid [entry=%p, descr=%p]", this, mOldDesc));
    return mOldDesc->MarkValid();
  }

  LOG(("Not marking read-only cache entry valid [entry=%p, descr=%p]", this, mOldDesc));
  return NS_OK;
}

NS_IMETHODIMP _OldCacheEntryWrapper::HasWriteAccess(bool aWriteAllowed_unused, bool *aWriteAccess)
{
  NS_ENSURE_TRUE(mOldDesc, NS_ERROR_NULL_POINTER);
  NS_ENSURE_ARG(aWriteAccess);

  nsCacheAccessMode mode;
  nsresult rv = mOldDesc->GetAccessGranted(&mode);
  NS_ENSURE_SUCCESS(rv, rv);

  *aWriteAccess = !!(mode & nsICache::ACCESS_WRITE);

  LOG(("_OldCacheEntryWrapper::HasWriteAccess [this=%p, write-access=%d]", this, *aWriteAccess));

  return NS_OK;
}

namespace { // anon

class MetaDataVisitorWrapper : public nsICacheMetaDataVisitor
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSICACHEMETADATAVISITOR
  MetaDataVisitorWrapper(nsICacheEntryMetaDataVisitor* cb) : mCB(cb) {}
  virtual ~MetaDataVisitorWrapper() {}
  nsCOMPtr<nsICacheEntryMetaDataVisitor> mCB;
};

NS_IMPL_ISUPPORTS(MetaDataVisitorWrapper, nsICacheMetaDataVisitor)

NS_IMETHODIMP
MetaDataVisitorWrapper::VisitMetaDataElement(char const * key,
                                             char const * value,
                                             bool *goon)
{
  *goon = true;
  return mCB->OnMetaDataElement(key, value);
}

} // anon

NS_IMETHODIMP _OldCacheEntryWrapper::VisitMetaData(nsICacheEntryMetaDataVisitor* cb)
{
  nsRefPtr<MetaDataVisitorWrapper> w = new MetaDataVisitorWrapper(cb);
  return mOldDesc->VisitMetaData(w);
}

namespace { // anon

nsresult
GetCacheSessionNameForStoragePolicy(
        nsCSubstring const &scheme,
        nsCacheStoragePolicy storagePolicy,
        bool isPrivate,
        uint32_t appId,
        bool inBrowser,
        nsACString& sessionName)
{
  MOZ_ASSERT(!isPrivate || storagePolicy == nsICache::STORE_IN_MEMORY);

  // HTTP
  if (scheme.EqualsLiteral("http") ||
      scheme.EqualsLiteral("https")) {
    switch (storagePolicy) {
    case nsICache::STORE_IN_MEMORY:
      if (isPrivate)
        sessionName.AssignLiteral("HTTP-memory-only-PB");
      else
        sessionName.AssignLiteral("HTTP-memory-only");
      break;
    case nsICache::STORE_OFFLINE:
      // XXX This is actually never used, only added to prevent
      // any compatibility damage.
      sessionName.AssignLiteral("HTTP-offline");
      break;
    default:
      sessionName.AssignLiteral("HTTP");
      break;
    }
  }
  // WYCIWYG
  else if (scheme.EqualsLiteral("wyciwyg")) {
    if (isPrivate)
      sessionName.AssignLiteral("wyciwyg-private");
    else
      sessionName.AssignLiteral("wyciwyg");
  }
  // FTP
  else if (scheme.EqualsLiteral("ftp")) {
    if (isPrivate)
      sessionName.AssignLiteral("FTP-private");
    else
      sessionName.AssignLiteral("FTP");
  }
  // all remaining URL scheme
  else {
    // Since with the new API a consumer cannot specify its own session name
    // and partitioning of the cache is handled stricly only by the cache
    // back-end internally, we will use a separate session name to pretend
    // functionality of the new API wrapping the Darin's cache for all other
    // URL schemes.
    // Deliberately omitting |anonymous| since other session types don't
    // recognize it too.
    sessionName.AssignLiteral("other");
    if (isPrivate)
      sessionName.AppendLiteral("-private");
  }

  if (appId != nsILoadContextInfo::NO_APP_ID || inBrowser) {
    sessionName.Append('~');
    sessionName.AppendInt(appId);
    sessionName.Append('~');
    sessionName.AppendInt(inBrowser);
  }

  return NS_OK;
}

nsresult
GetCacheSession(nsCSubstring const &aScheme,
                bool aWriteToDisk,
                nsILoadContextInfo* aLoadInfo,
                nsIApplicationCache* aAppCache,
                nsICacheSession** _result)
{
  nsresult rv;

  nsCacheStoragePolicy storagePolicy;
  if (aAppCache)
    storagePolicy = nsICache::STORE_OFFLINE;
  else if (!aWriteToDisk || aLoadInfo->IsPrivate())
    storagePolicy = nsICache::STORE_IN_MEMORY;
  else
    storagePolicy = nsICache::STORE_ANYWHERE;

  nsAutoCString clientId;
  if (aAppCache) {
    aAppCache->GetClientID(clientId);
  }
  else {
    rv = GetCacheSessionNameForStoragePolicy(
      aScheme,
      storagePolicy,
      aLoadInfo->IsPrivate(),
      aLoadInfo->AppId(),
      aLoadInfo->IsInBrowserElement(),
      clientId);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  LOG(("  GetCacheSession for client=%s, policy=%d", clientId.get(), storagePolicy));

  nsCOMPtr<nsICacheService> serv =
      do_GetService(NS_CACHESERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsICacheSession> session;
  rv = nsCacheService::GlobalInstance()->CreateSessionInternal(clientId.get(),
                                                               storagePolicy,
                                                               nsICache::STREAM_BASED,
                                                               getter_AddRefs(session));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = session->SetIsPrivate(aLoadInfo->IsPrivate());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = session->SetDoomEntriesIfExpired(false);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aAppCache) {
    nsCOMPtr<nsIFile> profileDirectory;
    aAppCache->GetProfileDirectory(getter_AddRefs(profileDirectory));
    if (profileDirectory)
      rv = session->SetProfileDirectory(profileDirectory);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  session.forget(_result);
  return NS_OK;
}

} // anon


NS_IMPL_ISUPPORTS_INHERITED(_OldCacheLoad, nsRunnable, nsICacheListener)

_OldCacheLoad::_OldCacheLoad(nsCSubstring const& aScheme,
                             nsCSubstring const& aCacheKey,
                             nsICacheEntryOpenCallback* aCallback,
                             nsIApplicationCache* aAppCache,
                             nsILoadContextInfo* aLoadInfo,
                             bool aWriteToDisk,
                             uint32_t aFlags)
  : mScheme(aScheme)
  , mCacheKey(aCacheKey)
  , mCallback(aCallback)
  , mLoadInfo(GetLoadContextInfo(aLoadInfo))
  , mFlags(aFlags)
  , mWriteToDisk(aWriteToDisk)
  , mNew(true)
  , mOpening(true)
  , mSync(false)
  , mStatus(NS_ERROR_UNEXPECTED)
  , mRunCount(0)
  , mAppCache(aAppCache)
{
  MOZ_COUNT_CTOR(_OldCacheLoad);
}

_OldCacheLoad::~_OldCacheLoad()
{
  ProxyReleaseMainThread(mAppCache);
  MOZ_COUNT_DTOR(_OldCacheLoad);
}

nsresult _OldCacheLoad::Start()
{
  LOG(("_OldCacheLoad::Start [this=%p, key=%s]", this, mCacheKey.get()));

  mLoadStart = mozilla::TimeStamp::Now();

  nsresult rv;

  // Consumers that can invoke this code as first and off the main thread
  // are responsible for initiating these two services on the main thread.
  // Currently this is only nsWyciwygChannel.

  // XXX: Start the cache service; otherwise DispatchToCacheIOThread will
  // fail.
  nsCOMPtr<nsICacheService> service =
    do_GetService(NS_CACHESERVICE_CONTRACTID, &rv);

  // Ensure the stream transport service gets initialized on the main thread
  if (NS_SUCCEEDED(rv) && NS_IsMainThread()) {
    nsCOMPtr<nsIStreamTransportService> sts =
      do_GetService(kStreamTransportServiceCID, &rv);
  }

  if (NS_SUCCEEDED(rv)) {
    rv = service->GetCacheIOTarget(getter_AddRefs(mCacheThread));
  }

  if (NS_SUCCEEDED(rv)) {
    bool onCacheTarget;
    rv = mCacheThread->IsOnCurrentThread(&onCacheTarget);
    if (NS_SUCCEEDED(rv) && onCacheTarget) {
      mSync = true;
    }
  }

  if (NS_SUCCEEDED(rv)) {
    if (mSync) {
      rv = Run();
    }
    else {
      rv = mCacheThread->Dispatch(this, NS_DISPATCH_NORMAL);
    }
  }

  return rv;
}

NS_IMETHODIMP
_OldCacheLoad::Run()
{
  LOG(("_OldCacheLoad::Run [this=%p, key=%s, cb=%p]", this, mCacheKey.get(), mCallback.get()));

  nsresult rv;

  if (mOpening) {
    mOpening = false;
    nsCOMPtr<nsICacheSession> session;
    rv = GetCacheSession(mScheme, mWriteToDisk, mLoadInfo, mAppCache,
                         getter_AddRefs(session));
    if (NS_SUCCEEDED(rv)) {
      // AsyncOpenCacheEntry isn't really async when its called on the
      // cache service thread.

      nsCacheAccessMode cacheAccess;
      if (mFlags & nsICacheStorage::OPEN_TRUNCATE)
        cacheAccess = nsICache::ACCESS_WRITE;
      else if ((mFlags & nsICacheStorage::OPEN_READONLY) || mAppCache)
        cacheAccess = nsICache::ACCESS_READ;
      else
        cacheAccess = nsICache::ACCESS_READ_WRITE;

      LOG(("  session->AsyncOpenCacheEntry with access=%d", cacheAccess));

      bool bypassBusy = mFlags & nsICacheStorage::OPEN_BYPASS_IF_BUSY;

      if (mSync && cacheAccess == nsICache::ACCESS_WRITE) {
        nsCOMPtr<nsICacheEntryDescriptor> entry;
        rv = session->OpenCacheEntry(mCacheKey, cacheAccess, bypassBusy,
          getter_AddRefs(entry));

        nsCacheAccessMode grantedAccess = 0;
        if (NS_SUCCEEDED(rv)) {
          entry->GetAccessGranted(&grantedAccess);
        }

        return OnCacheEntryAvailable(entry, grantedAccess, rv);
      }

      rv = session->AsyncOpenCacheEntry(mCacheKey, cacheAccess, this, bypassBusy);
      if (NS_SUCCEEDED(rv))
        return NS_OK;
    }

    // Opening failed, propagate the error to the consumer
    LOG(("  Opening cache entry failed with rv=0x%08x", rv));
    mStatus = rv;
    mNew = false;
    NS_DispatchToMainThread(this);
  } else {
    if (!mCallback) {
      LOG(("  duplicate call, bypassed"));
      return NS_OK;
    }

    if (NS_SUCCEEDED(mStatus)) {
      if (mFlags & nsICacheStorage::OPEN_TRUNCATE) {
        mozilla::Telemetry::AccumulateTimeDelta(
          mozilla::Telemetry::NETWORK_CACHE_V1_TRUNCATE_TIME_MS,
          mLoadStart);
      }
      else if (mNew) {
        mozilla::Telemetry::AccumulateTimeDelta(
          mozilla::Telemetry::NETWORK_CACHE_V1_MISS_TIME_MS,
          mLoadStart);
      }
      else {
        mozilla::Telemetry::AccumulateTimeDelta(
          mozilla::Telemetry::NETWORK_CACHE_V1_HIT_TIME_MS,
          mLoadStart);
      }
    }

    if (!(mFlags & CHECK_MULTITHREADED))
      Check();

    // break cycles
    nsCOMPtr<nsICacheEntryOpenCallback> cb = mCallback.forget();
    mCacheThread = nullptr;
    nsCOMPtr<nsICacheEntry> entry = mCacheEntry.forget();

    rv = cb->OnCacheEntryAvailable(entry, mNew, mAppCache, mStatus);

    if (NS_FAILED(rv) && entry) {
      LOG(("  cb->OnCacheEntryAvailable failed with rv=0x%08x", rv));
      if (mNew)
        entry->AsyncDoom(nullptr);
      else
        entry->Close();
    }
  }

  return rv;
}

NS_IMETHODIMP
_OldCacheLoad::OnCacheEntryAvailable(nsICacheEntryDescriptor *entry,
                                     nsCacheAccessMode access,
                                     nsresult status)
{
  LOG(("_OldCacheLoad::OnCacheEntryAvailable [this=%p, ent=%p, cb=%p, appcache=%p, access=%x]",
    this, entry, mCallback.get(), mAppCache.get(), access));

  // XXX Bug 759805: Sometimes we will call this method directly from
  // HttpCacheQuery::Run when AsyncOpenCacheEntry fails, but
  // AsyncOpenCacheEntry will also call this method. As a workaround, we just
  // ensure we only execute this code once.
  NS_ENSURE_TRUE(mRunCount == 0, NS_ERROR_UNEXPECTED);
  ++mRunCount;

  mCacheEntry = entry ? new _OldCacheEntryWrapper(entry) : nullptr;
  mStatus = status;
  mNew = access == nsICache::ACCESS_WRITE;

  if (mFlags & CHECK_MULTITHREADED)
    Check();

  if (mSync)
    return Run();

  return NS_DispatchToMainThread(this);
}

void
_OldCacheLoad::Check()
{
  if (!mCacheEntry)
    return;

  if (mNew)
    return;

  uint32_t result;
  nsresult rv = mCallback->OnCacheEntryCheck(mCacheEntry, mAppCache, &result);
  LOG(("  OnCacheEntryCheck result ent=%p, cb=%p, appcache=%p, rv=0x%08x, result=%d",
    mCacheEntry.get(), mCallback.get(), mAppCache.get(), rv, result));

  if (NS_FAILED(rv)) {
    NS_WARNING("cache check failed");
  }

  if (NS_FAILED(rv) || result == nsICacheEntryOpenCallback::ENTRY_NOT_WANTED) {
    mCacheEntry->Close();
    mCacheEntry = nullptr;
    mStatus = NS_ERROR_CACHE_KEY_NOT_FOUND;
  }
}

NS_IMETHODIMP
_OldCacheLoad::OnCacheEntryDoomed(nsresult)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

// nsICacheStorage old cache wrapper

NS_IMPL_ISUPPORTS(_OldStorage, nsICacheStorage)

_OldStorage::_OldStorage(nsILoadContextInfo* aInfo,
                         bool aAllowDisk,
                         bool aLookupAppCache,
                         bool aOfflineStorage,
                         nsIApplicationCache* aAppCache)
: mLoadInfo(GetLoadContextInfo(aInfo))
, mAppCache(aAppCache)
, mWriteToDisk(aAllowDisk)
, mLookupAppCache(aLookupAppCache)
, mOfflineStorage(aOfflineStorage)
{
  MOZ_COUNT_CTOR(_OldStorage);
}

_OldStorage::~_OldStorage()
{
  MOZ_COUNT_DTOR(_OldStorage);
}

NS_IMETHODIMP _OldStorage::AsyncOpenURI(nsIURI *aURI,
                                        const nsACString & aIdExtension,
                                        uint32_t aFlags,
                                        nsICacheEntryOpenCallback *aCallback)
{
  NS_ENSURE_ARG(aURI);
  NS_ENSURE_ARG(aCallback);

#ifdef MOZ_LOGGING
  nsAutoCString uriSpec;
  aURI->GetAsciiSpec(uriSpec);
  LOG(("_OldStorage::AsyncOpenURI [this=%p, uri=%s, ide=%s, flags=%x]",
    this, uriSpec.get(), aIdExtension.BeginReading(), aFlags));
#endif

  nsresult rv;

  nsAutoCString cacheKey, scheme;
  rv = AssembleCacheKey(aURI, aIdExtension, cacheKey, scheme);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mAppCache && (mLookupAppCache || mOfflineStorage)) {
    rv = ChooseApplicationCache(cacheKey, getter_AddRefs(mAppCache));
    NS_ENSURE_SUCCESS(rv, rv);

    if (mAppCache) {
      // From a chosen appcache open only as readonly
      aFlags &= ~nsICacheStorage::OPEN_TRUNCATE;
    }
  }

  nsRefPtr<_OldCacheLoad> cacheLoad =
    new _OldCacheLoad(scheme, cacheKey, aCallback, mAppCache,
                      mLoadInfo, mWriteToDisk, aFlags);

  rv = cacheLoad->Start();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP _OldStorage::AsyncDoomURI(nsIURI *aURI, const nsACString & aIdExtension,
                                        nsICacheEntryDoomCallback* aCallback)
{
  LOG(("_OldStorage::AsyncDoomURI"));

  nsresult rv;

  nsAutoCString cacheKey, scheme;
  rv = AssembleCacheKey(aURI, aIdExtension, cacheKey, scheme);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsICacheSession> session;
  rv = GetCacheSession(scheme, mWriteToDisk, mLoadInfo, mAppCache,
                       getter_AddRefs(session));
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<DoomCallbackWrapper> cb = aCallback
    ? new DoomCallbackWrapper(aCallback)
    : nullptr;
  rv = session->DoomEntry(cacheKey, cb);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP _OldStorage::AsyncEvictStorage(nsICacheEntryDoomCallback* aCallback)
{
  LOG(("_OldStorage::AsyncEvictStorage"));

  nsresult rv;

  if (!mAppCache && mOfflineStorage) {
    // Special casing for pure offline storage
    if (mLoadInfo->AppId() == nsILoadContextInfo::NO_APP_ID &&
        !mLoadInfo->IsInBrowserElement()) {

      // Clear everything.
      nsCOMPtr<nsICacheService> serv =
          do_GetService(NS_CACHESERVICE_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = nsCacheService::GlobalInstance()->EvictEntriesInternal(nsICache::STORE_OFFLINE);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
      // Clear app or inbrowser staff.
      nsCOMPtr<nsIApplicationCacheService> appCacheService =
        do_GetService(NS_APPLICATIONCACHESERVICE_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = appCacheService->DiscardByAppId(mLoadInfo->AppId(),
                                           mLoadInfo->IsInBrowserElement());
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  else {
    if (mAppCache) {
      nsCOMPtr<nsICacheSession> session;
      rv = GetCacheSession(EmptyCString(),
                           mWriteToDisk, mLoadInfo, mAppCache,
                           getter_AddRefs(session));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = session->EvictEntries();
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
      // Oh, I'll be so happy when session names are gone...
      nsCOMPtr<nsICacheSession> session;
      rv = GetCacheSession(NS_LITERAL_CSTRING("http"),
                           mWriteToDisk, mLoadInfo, mAppCache,
                           getter_AddRefs(session));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = session->EvictEntries();
      NS_ENSURE_SUCCESS(rv, rv);

      rv = GetCacheSession(NS_LITERAL_CSTRING("wyciwyg"),
                           mWriteToDisk, mLoadInfo, mAppCache,
                           getter_AddRefs(session));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = session->EvictEntries();
      NS_ENSURE_SUCCESS(rv, rv);

      // This clears any data from scheme other then http, wyciwyg or ftp
      rv = GetCacheSession(EmptyCString(),
                           mWriteToDisk, mLoadInfo, mAppCache,
                           getter_AddRefs(session));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = session->EvictEntries();
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  if (aCallback) {
    nsRefPtr<DoomCallbackSynchronizer> sync =
      new DoomCallbackSynchronizer(aCallback);
    rv = sync->Dispatch();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP _OldStorage::AsyncVisitStorage(nsICacheStorageVisitor* aVisitor,
                                             bool aVisitEntries)
{
  LOG(("_OldStorage::AsyncVisitStorage"));

  NS_ENSURE_ARG(aVisitor);

  nsresult rv;

  nsCOMPtr<nsICacheService> serv =
    do_GetService(NS_CACHESERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  char* deviceID;
  if (mAppCache || mOfflineStorage) {
    deviceID = const_cast<char*>("offline");
  } else if (!mWriteToDisk || mLoadInfo->IsPrivate()) {
    deviceID = const_cast<char*>("memory");
  } else {
    deviceID = const_cast<char*>("disk");
  }

  nsRefPtr<_OldVisitCallbackWrapper> cb = new _OldVisitCallbackWrapper(
    deviceID, aVisitor, aVisitEntries, mLoadInfo);
  rv = nsCacheService::GlobalInstance()->VisitEntriesInternal(cb);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

// Internal

nsresult _OldStorage::AssembleCacheKey(nsIURI *aURI,
                                       nsACString const & aIdExtension,
                                       nsACString & aCacheKey,
                                       nsACString & aScheme)
{
  // Copied from nsHttpChannel::AssembleCacheKey

  aCacheKey.Truncate();

  nsresult rv;

  rv = aURI->GetScheme(aScheme);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString uriSpec;
  if (aScheme.EqualsLiteral("http") ||
      aScheme.EqualsLiteral("https")) {
    if (mLoadInfo->IsAnonymous()) {
      aCacheKey.AssignLiteral("anon&");
    }

    if (!aIdExtension.IsEmpty()) {
      aCacheKey.AppendPrintf("id=%s&", aIdExtension.BeginReading());
    }

    nsCOMPtr<nsIURI> noRefURI;
    rv = aURI->CloneIgnoringRef(getter_AddRefs(noRefURI));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = noRefURI->GetAsciiSpec(uriSpec);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!aCacheKey.IsEmpty()) {
      aCacheKey.AppendLiteral("uri=");
    }
  }
  else if (aScheme.EqualsLiteral("wyciwyg")) {
    rv = aURI->GetSpec(uriSpec);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    rv = aURI->GetAsciiSpec(uriSpec);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  aCacheKey.Append(uriSpec);

  return NS_OK;
}

nsresult _OldStorage::ChooseApplicationCache(nsCSubstring const &cacheKey,
                                             nsIApplicationCache** aCache)
{
  nsresult rv;

  nsCOMPtr<nsIApplicationCacheService> appCacheService =
    do_GetService(NS_APPLICATIONCACHESERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = appCacheService->ChooseApplicationCache(cacheKey, mLoadInfo, aCache);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

} // net
} // mozilla
