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
  NS_FORWARD_SAFE_NSICACHEENTRYDESCRIPTOR(mOldDesc)
  NS_FORWARD_NSICACHEENTRYINFO(mOldInfo->)

  NS_IMETHOD AsyncDoom(nsICacheEntryDoomCallback* listener);
  NS_IMETHOD GetPersistent(bool *aPersistToDisk);
  NS_IMETHOD GetIsForcedValid(bool *aIsForcedValid);
  NS_IMETHOD ForceValidFor(uint32_t aSecondsToTheFuture);
  NS_IMETHOD SetValid() { return NS_OK; }
  NS_IMETHOD MetaDataReady() { return NS_OK; }
  NS_IMETHOD Recreate(bool, nsICacheEntry**);
  NS_IMETHOD GetDataSize(int64_t *size);
  NS_IMETHOD OpenInputStream(int64_t offset, nsIInputStream * *_retval);
  NS_IMETHOD OpenOutputStream(int64_t offset, nsIOutputStream * *_retval);
  NS_IMETHOD MaybeMarkValid();
  NS_IMETHOD HasWriteAccess(bool aWriteOnly, bool *aWriteAccess);
  NS_IMETHOD VisitMetaData(nsICacheEntryMetaDataVisitor*);

  explicit _OldCacheEntryWrapper(nsICacheEntryDescriptor* desc);
  explicit _OldCacheEntryWrapper(nsICacheEntryInfo* info);

private:
  virtual ~_OldCacheEntryWrapper();

  _OldCacheEntryWrapper() MOZ_DELETE;
  nsICacheEntryDescriptor* mOldDesc; // ref holded in mOldInfo
  nsCOMPtr<nsICacheEntryInfo> mOldInfo;
};


class _OldCacheLoad : public nsRunnable
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
    MOZ_COUNT_CTOR(_OldVisitCallbackWrapper);
  }

private:
  virtual ~_OldVisitCallbackWrapper();
  nsCOMPtr<nsICacheStorageVisitor> mCB;
  bool mVisitEntries;
  char const * mDeviceID;
  nsCOMPtr<nsILoadContextInfo> mLoadInfo;
  bool mHit; // set to true when the device was found
};

class _OldGetDiskConsumption : public nsRunnable,
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

}}

#endif
