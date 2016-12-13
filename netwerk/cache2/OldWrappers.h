// Stuff to link the old imp to the new api - will go away!

#ifndef OLDWRAPPERS__H__
#define OLDWRAPPERS__H__

#include "nsICacheEntry.h"
#include "nsICacheListener.h"
#include "nsICacheStorage.h"

#include "nsCOMPtr.h"
#include "nsICacheEntryOpenCallback.h"
#include "nsICacheEntryDescriptor.h"
#include "nsICacheStorageVisitor.h"
#include "nsThreadUtils.h"
#include "mozilla/TimeStamp.h"

class nsIURI;
class nsICacheEntryOpenCallback;
class nsICacheStorageConsumptionObserver;
class nsIApplicationCache;
class nsILoadContextInfo;

namespace mozilla { namespace net {

class CacheStorage;

class _OldCacheEntryWrapper : public nsICacheEntry
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  // nsICacheEntryDescriptor
  NS_IMETHOD SetExpirationTime(uint32_t expirationTime) override
  {
    return !mOldDesc ? NS_ERROR_NULL_POINTER :
                       mOldDesc->SetExpirationTime(expirationTime);
  }
  nsresult OpenInputStream(uint32_t offset, nsIInputStream * *_retval)
  {
    return !mOldDesc ? NS_ERROR_NULL_POINTER :
                       mOldDesc->OpenInputStream(offset, _retval);
  }
  nsresult OpenOutputStream(uint32_t offset, nsIOutputStream * *_retval)
  {
    return !mOldDesc ? NS_ERROR_NULL_POINTER :
                       mOldDesc->OpenOutputStream(offset, _retval);
  }
  NS_IMETHOD OpenAlternativeOutputStream(const nsACString & type, nsIOutputStream * *_retval) override
  {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD OpenAlternativeInputStream(const nsACString & type, nsIInputStream * *_retval) override
  {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  NS_IMETHOD GetPredictedDataSize(int64_t *aPredictedDataSize) override
  {
    return !mOldDesc ? NS_ERROR_NULL_POINTER :
                       mOldDesc->GetPredictedDataSize(aPredictedDataSize);
  }
  NS_IMETHOD SetPredictedDataSize(int64_t aPredictedDataSize) override
  {
    return !mOldDesc ? NS_ERROR_NULL_POINTER :
                       mOldDesc->SetPredictedDataSize(aPredictedDataSize);
  }
  NS_IMETHOD GetSecurityInfo(nsISupports * *aSecurityInfo) override
  {
    return !mOldDesc ? NS_ERROR_NULL_POINTER :
                       mOldDesc->GetSecurityInfo(aSecurityInfo);
  }
  NS_IMETHOD SetSecurityInfo(nsISupports *aSecurityInfo) override
  {
    return !mOldDesc ? NS_ERROR_NULL_POINTER :
                       mOldDesc->SetSecurityInfo(aSecurityInfo);
  }
  NS_IMETHOD GetStorageDataSize(uint32_t *aStorageDataSize) override
  {
    return !mOldDesc ? NS_ERROR_NULL_POINTER :
                       mOldDesc->GetStorageDataSize(aStorageDataSize);
  }
  nsresult AsyncDoom(nsICacheListener *listener)
  {
    return !mOldDesc ? NS_ERROR_NULL_POINTER :
                       mOldDesc->AsyncDoom(listener);
  }
  NS_IMETHOD MarkValid(void) override
  {
    return !mOldDesc ? NS_ERROR_NULL_POINTER :
                       mOldDesc->MarkValid();
  }
  NS_IMETHOD Close(void) override
  {
    return !mOldDesc ? NS_ERROR_NULL_POINTER :
                       mOldDesc->Close();
  }
  NS_IMETHOD GetMetaDataElement(const char * key, char * *_retval) override
  {
    return !mOldDesc ? NS_ERROR_NULL_POINTER :
                       mOldDesc->GetMetaDataElement(key, _retval);
  }
  NS_IMETHOD SetMetaDataElement(const char * key, const char * value) override
  {
    return !mOldDesc ? NS_ERROR_NULL_POINTER :
                       mOldDesc->SetMetaDataElement(key, value);
  }

  NS_IMETHOD GetDiskStorageSizeInKB(uint32_t *aDiskStorageSize) override
  {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // nsICacheEntryInfo
  NS_IMETHOD GetKey(nsACString & aKey) override
  {
    return mOldInfo->GetKey(aKey);
  }
  NS_IMETHOD GetFetchCount(int32_t *aFetchCount) override
  {
    return mOldInfo->GetFetchCount(aFetchCount);
  }
  NS_IMETHOD GetLastFetched(uint32_t *aLastFetched) override
  {
    return mOldInfo->GetLastFetched(aLastFetched);
  }
  NS_IMETHOD GetLastModified(uint32_t *aLastModified) override
  {
    return mOldInfo->GetLastModified(aLastModified);
  }
  NS_IMETHOD GetExpirationTime(uint32_t *aExpirationTime) override
  {
    return mOldInfo->GetExpirationTime(aExpirationTime);
  }
  nsresult GetDataSize(uint32_t *aDataSize)
  {
    return mOldInfo->GetDataSize(aDataSize);
  }

  NS_IMETHOD AsyncDoom(nsICacheEntryDoomCallback* listener) override;
  NS_IMETHOD GetPersistent(bool *aPersistToDisk) override;
  NS_IMETHOD GetIsForcedValid(bool *aIsForcedValid) override;
  NS_IMETHOD ForceValidFor(uint32_t aSecondsToTheFuture) override;
  NS_IMETHOD SetValid() override { return NS_OK; }
  NS_IMETHOD MetaDataReady() override { return NS_OK; }
  NS_IMETHOD Recreate(bool, nsICacheEntry**) override;
  NS_IMETHOD GetDataSize(int64_t *size) override;
  NS_IMETHOD GetAltDataSize(int64_t *size) override;
  NS_IMETHOD OpenInputStream(int64_t offset, nsIInputStream * *_retval) override;
  NS_IMETHOD OpenOutputStream(int64_t offset, nsIOutputStream * *_retval) override;
  NS_IMETHOD MaybeMarkValid() override;
  NS_IMETHOD HasWriteAccess(bool aWriteOnly, bool *aWriteAccess) override;
  NS_IMETHOD VisitMetaData(nsICacheEntryMetaDataVisitor*) override;

  explicit _OldCacheEntryWrapper(nsICacheEntryDescriptor* desc);
  explicit _OldCacheEntryWrapper(nsICacheEntryInfo* info);

private:
  virtual ~_OldCacheEntryWrapper();

  _OldCacheEntryWrapper() = delete;
  nsICacheEntryDescriptor* mOldDesc; // ref holded in mOldInfo
  nsCOMPtr<nsICacheEntryInfo> mOldInfo;
};


class _OldCacheLoad : public Runnable
                    , public nsICacheListener
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSICACHELISTENER

  _OldCacheLoad(nsCSubstring const& aScheme,
                nsCSubstring const& aCacheKey,
                nsICacheEntryOpenCallback* aCallback,
                nsIApplicationCache* aAppCache,
                nsILoadContextInfo* aLoadInfo,
                bool aWriteToDisk,
                uint32_t aFlags);

  nsresult Start();

protected:
  virtual ~_OldCacheLoad();

private:
  void Check();

  nsCOMPtr<nsIEventTarget> mCacheThread;

  nsCString const mScheme;
  nsCString const mCacheKey;
  nsCOMPtr<nsICacheEntryOpenCallback> mCallback;
  nsCOMPtr<nsILoadContextInfo> mLoadInfo;
  uint32_t const mFlags;

  bool const mWriteToDisk : 1;
  bool mNew : 1;
  bool mOpening : 1;
  bool mSync : 1;

  nsCOMPtr<nsICacheEntry> mCacheEntry;
  nsresult mStatus;
  uint32_t mRunCount;
  nsCOMPtr<nsIApplicationCache> mAppCache;

  mozilla::TimeStamp mLoadStart;
};


class _OldStorage : public nsICacheStorage
{
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICACHESTORAGE

public:
  _OldStorage(nsILoadContextInfo* aInfo,
              bool aAllowDisk,
              bool aLookupAppCache,
              bool aOfflineStorage,
              nsIApplicationCache* aAppCache);

private:
  virtual ~_OldStorage();
  nsresult AssembleCacheKey(nsIURI *aURI, nsACString const & aIdExtension,
                            nsACString & aCacheKey, nsACString & aScheme);
  nsresult ChooseApplicationCache(nsCSubstring const &cacheKey, nsIApplicationCache** aCache);

  nsCOMPtr<nsILoadContextInfo> mLoadInfo;
  nsCOMPtr<nsIApplicationCache> mAppCache;
  bool const mWriteToDisk : 1;
  bool const mLookupAppCache : 1;
  bool const mOfflineStorage : 1;
};

class _OldVisitCallbackWrapper : public nsICacheVisitor
{
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICACHEVISITOR

  _OldVisitCallbackWrapper(char const * deviceID,
                           nsICacheStorageVisitor * cb,
                           bool visitEntries,
                           nsILoadContextInfo * aInfo)
  : mCB(cb)
  , mVisitEntries(visitEntries)
  , mDeviceID(deviceID)
  , mLoadInfo(aInfo)
  , mHit(false)
  {
  }

private:
  virtual ~_OldVisitCallbackWrapper();
  nsCOMPtr<nsICacheStorageVisitor> mCB;
  bool mVisitEntries;
  char const * mDeviceID;
  nsCOMPtr<nsILoadContextInfo> mLoadInfo;
  bool mHit; // set to true when the device was found
};

class _OldGetDiskConsumption : public Runnable,
                               public nsICacheVisitor
{
public:
  static nsresult Get(nsICacheStorageConsumptionObserver* aCallback);

private:
  explicit _OldGetDiskConsumption(nsICacheStorageConsumptionObserver* aCallback);
  virtual ~_OldGetDiskConsumption() {}
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSICACHEVISITOR
  NS_DECL_NSIRUNNABLE

  nsCOMPtr<nsICacheStorageConsumptionObserver> mCallback;
  int64_t mSize;
};

} // namespace net
} // namespace mozilla

#endif
