/* -*- Mode: C++; indent-tab-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/Attributes.h"

#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Snprintf.h"

#include "nsCache.h"
#include "nsDiskCache.h"
#include "nsDiskCacheDeviceSQL.h"
#include "nsCacheService.h"
#include "nsApplicationCache.h"

#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsIURI.h"
#include "nsAutoPtr.h"
#include "nsEscape.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsString.h"
#include "nsPrintfCString.h"
#include "nsCRT.h"
#include "nsArrayUtils.h"
#include "nsIArray.h"
#include "nsIVariant.h"
#include "nsILoadContextInfo.h"
#include "nsThreadUtils.h"
#include "nsISerializable.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsSerializationHelper.h"

#include "mozIStorageService.h"
#include "mozIStorageStatement.h"
#include "mozIStorageFunction.h"
#include "mozStorageHelper.h"

#include "nsICacheVisitor.h"
#include "nsISeekableStream.h"

#include "mozilla/Telemetry.h"

#include "sqlite3.h"
#include "mozilla/storage.h"

using namespace mozilla;
using namespace mozilla::storage;
using mozilla::NeckoOriginAttributes;

static const char OFFLINE_CACHE_DEVICE_ID[] = { "offline" };

#define LOG(args) CACHE_LOG_DEBUG(args)

static uint32_t gNextTemporaryClientID = 0;

/*****************************************************************************
 * helpers
 */

static nsresult
EnsureDir(nsIFile *dir)
{
  bool exists;
  nsresult rv = dir->Exists(&exists);
  if (NS_SUCCEEDED(rv) && !exists)
    rv = dir->Create(nsIFile::DIRECTORY_TYPE, 0700);
  return rv;
}

static bool
DecomposeCacheEntryKey(const nsCString *fullKey,
                       const char **cid,
                       const char **key,
                       nsCString &buf)
{
  buf = *fullKey;

  int32_t colon = buf.FindChar(':');
  if (colon == kNotFound)
  {
    NS_ERROR("Invalid key");
    return false;
  }
  buf.SetCharAt('\0', colon);

  *cid = buf.get();
  *key = buf.get() + colon + 1;

  return true;
}

class AutoResetStatement
{
  public:
    explicit AutoResetStatement(mozIStorageStatement *s)
      : mStatement(s) {}
    ~AutoResetStatement() { mStatement->Reset(); }
    mozIStorageStatement *operator->() MOZ_NO_ADDREF_RELEASE_ON_RETURN { return mStatement; }
  private:
    mozIStorageStatement *mStatement;
};

class EvictionObserver
{
  public:
  EvictionObserver(mozIStorageConnection *db,
                   nsOfflineCacheEvictionFunction *evictionFunction)
    : mDB(db), mEvictionFunction(evictionFunction)
    {
      mDB->ExecuteSimpleSQL(
          NS_LITERAL_CSTRING("CREATE TEMP TRIGGER cache_on_delete BEFORE DELETE"
                             " ON moz_cache FOR EACH ROW BEGIN SELECT"
                             " cache_eviction_observer("
                             "  OLD.ClientID, OLD.key, OLD.generation);"
                             " END;"));
      mEvictionFunction->Reset();
    }

    ~EvictionObserver()
    {
      mDB->ExecuteSimpleSQL(
        NS_LITERAL_CSTRING("DROP TRIGGER cache_on_delete;"));
      mEvictionFunction->Reset();
    }

    void Apply() { return mEvictionFunction->Apply(); }

  private:
    mozIStorageConnection *mDB;
    RefPtr<nsOfflineCacheEvictionFunction> mEvictionFunction;
};

#define DCACHE_HASH_MAX  INT64_MAX
#define DCACHE_HASH_BITS 64

/**
 *  nsOfflineCache::Hash(const char * key)
 *
 *  This algorithm of this method implies nsOfflineCacheRecords will be stored
 *  in a certain order on disk.  If the algorithm changes, existing cache
 *  map files may become invalid, and therefore the kCurrentVersion needs
 *  to be revised.
 */
static uint64_t
DCacheHash(const char * key)
{
  // initval 0x7416f295 was chosen randomly
  return (uint64_t(nsDiskCache::Hash(key, 0)) << 32) | nsDiskCache::Hash(key, 0x7416f295);
}

/******************************************************************************
 * nsOfflineCacheEvictionFunction
 */

NS_IMPL_ISUPPORTS(nsOfflineCacheEvictionFunction, mozIStorageFunction)

// helper function for directly exposing the same data file binding
// path algorithm used in nsOfflineCacheBinding::Create
static nsresult
GetCacheDataFile(nsIFile *cacheDir, const char *key,
                 int generation, nsCOMPtr<nsIFile> &file)
{
  cacheDir->Clone(getter_AddRefs(file));
  if (!file)
    return NS_ERROR_OUT_OF_MEMORY;

  uint64_t hash = DCacheHash(key);

  uint32_t dir1 = (uint32_t) (hash & 0x0F);
  uint32_t dir2 = (uint32_t)((hash & 0xF0) >> 4);

  hash >>= 8;

  file->AppendNative(nsPrintfCString("%X", dir1));
  file->AppendNative(nsPrintfCString("%X", dir2));

  char leaf[64];
  PR_snprintf(leaf, sizeof(leaf), "%014llX-%X", hash, generation);
  return file->AppendNative(nsDependentCString(leaf));
}

NS_IMETHODIMP
nsOfflineCacheEvictionFunction::OnFunctionCall(mozIStorageValueArray *values, nsIVariant **_retval)
{
  LOG(("nsOfflineCacheEvictionFunction::OnFunctionCall\n"));

  *_retval = nullptr;

  uint32_t numEntries;
  nsresult rv = values->GetNumEntries(&numEntries);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ASSERTION(numEntries == 3, "unexpected number of arguments");

  uint32_t valueLen;
  const char *clientID = values->AsSharedUTF8String(0, &valueLen);
  const char *key = values->AsSharedUTF8String(1, &valueLen);
  nsAutoCString fullKey(clientID);
  fullKey.Append(':');
  fullKey.Append(key);
  int generation  = values->AsInt32(2);

  // If the key is currently locked, refuse to delete this row.
  if (mDevice->IsLocked(fullKey)) {
    NS_ADDREF(*_retval = new IntegerVariant(SQLITE_IGNORE));
    return NS_OK;
  }

  nsCOMPtr<nsIFile> file;
  rv = GetCacheDataFile(mDevice->CacheDirectory(), key,
                        generation, file);
  if (NS_FAILED(rv))
  {
    LOG(("GetCacheDataFile [key=%s generation=%d] failed [rv=%x]!\n",
         key, generation, rv));
    return rv;
  }

  mItems.AppendObject(file);

  return NS_OK;
}

void
nsOfflineCacheEvictionFunction::Apply()
{
  LOG(("nsOfflineCacheEvictionFunction::Apply\n"));

  for (int32_t i = 0; i < mItems.Count(); i++) {
    if (MOZ_LOG_TEST(gCacheLog, LogLevel::Debug)) {
      nsAutoCString path;
      mItems[i]->GetNativePath(path);
      LOG(("  removing %s\n", path.get()));
    }

    mItems[i]->Remove(false);
  }

  Reset();
}

class nsOfflineCacheDiscardCache : public nsRunnable
{
public:
  nsOfflineCacheDiscardCache(nsOfflineCacheDevice *device,
			     nsCString &group,
			     nsCString &clientID)
    : mDevice(device)
    , mGroup(group)
    , mClientID(clientID)
  {
  }

  NS_IMETHOD Run()
  {
    if (mDevice->IsActiveCache(mGroup, mClientID))
    {
      mDevice->DeactivateGroup(mGroup);
    }

    return mDevice->EvictEntries(mClientID.get());
  }

private:
  RefPtr<nsOfflineCacheDevice> mDevice;
  nsCString mGroup;
  nsCString mClientID;
};

/******************************************************************************
 * nsOfflineCacheDeviceInfo
 */

class nsOfflineCacheDeviceInfo final : public nsICacheDeviceInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICACHEDEVICEINFO

  explicit nsOfflineCacheDeviceInfo(nsOfflineCacheDevice* device)
    : mDevice(device)
  {}

private:
  ~nsOfflineCacheDeviceInfo() {}

  nsOfflineCacheDevice* mDevice;
};

NS_IMPL_ISUPPORTS(nsOfflineCacheDeviceInfo, nsICacheDeviceInfo)

NS_IMETHODIMP
nsOfflineCacheDeviceInfo::GetDescription(char **aDescription)
{
  *aDescription = NS_strdup("Offline cache device");
  return *aDescription ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsOfflineCacheDeviceInfo::GetUsageReport(char ** usageReport)
{
  nsAutoCString buffer;
  buffer.AssignLiteral("  <tr>\n"
                       "    <th>Cache Directory:</th>\n"
                       "    <td>");
  nsIFile *cacheDir = mDevice->CacheDirectory();
  if (!cacheDir)
    return NS_OK;

  nsAutoString path;
  nsresult rv = cacheDir->GetPath(path);
  if (NS_SUCCEEDED(rv))
    AppendUTF16toUTF8(path, buffer);
  else
    buffer.AppendLiteral("directory unavailable");
  
  buffer.AppendLiteral("</td>\n"
                       "  </tr>\n");

  *usageReport = ToNewCString(buffer);
  if (!*usageReport)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

NS_IMETHODIMP
nsOfflineCacheDeviceInfo::GetEntryCount(uint32_t *aEntryCount)
{
  *aEntryCount = mDevice->EntryCount();
  return NS_OK;
}

NS_IMETHODIMP
nsOfflineCacheDeviceInfo::GetTotalSize(uint32_t *aTotalSize)
{
  *aTotalSize = mDevice->CacheSize();
  return NS_OK;
}

NS_IMETHODIMP
nsOfflineCacheDeviceInfo::GetMaximumSize(uint32_t *aMaximumSize)
{
  *aMaximumSize = mDevice->CacheCapacity();
  return NS_OK;
}

/******************************************************************************
 * nsOfflineCacheBinding
 */

class nsOfflineCacheBinding final : public nsISupports
{
  ~nsOfflineCacheBinding() {}

public:
  NS_DECL_THREADSAFE_ISUPPORTS

  static nsOfflineCacheBinding *
      Create(nsIFile *cacheDir, const nsCString *key, int generation);

  enum { FLAG_NEW_ENTRY = 1 };

  nsCOMPtr<nsIFile> mDataFile;
  int               mGeneration;
  int		    mFlags;

  bool IsNewEntry() { return mFlags & FLAG_NEW_ENTRY; }
  void MarkNewEntry() { mFlags |= FLAG_NEW_ENTRY; }
  void ClearNewEntry() { mFlags &= ~FLAG_NEW_ENTRY; }
};

NS_IMPL_ISUPPORTS0(nsOfflineCacheBinding)

nsOfflineCacheBinding *
nsOfflineCacheBinding::Create(nsIFile *cacheDir,
                              const nsCString *fullKey,
                              int generation)
{
  nsCOMPtr<nsIFile> file;
  cacheDir->Clone(getter_AddRefs(file));
  if (!file)
    return nullptr;

  nsAutoCString keyBuf;
  const char *cid, *key;
  if (!DecomposeCacheEntryKey(fullKey, &cid, &key, keyBuf))
    return nullptr;

  uint64_t hash = DCacheHash(key);

  uint32_t dir1 = (uint32_t) (hash & 0x0F);
  uint32_t dir2 = (uint32_t)((hash & 0xF0) >> 4);

  hash >>= 8;

  // XXX we might want to create these directories up-front

  file->AppendNative(nsPrintfCString("%X", dir1));
  file->Create(nsIFile::DIRECTORY_TYPE, 00700);

  file->AppendNative(nsPrintfCString("%X", dir2));
  file->Create(nsIFile::DIRECTORY_TYPE, 00700);

  nsresult rv;
  char leaf[64];

  if (generation == -1)
  {
    file->AppendNative(NS_LITERAL_CSTRING("placeholder"));

    for (generation = 0; ; ++generation)
    {
      snprintf_literal(leaf, "%014" PRIX64 "-%X", hash, generation);

      rv = file->SetNativeLeafName(nsDependentCString(leaf));
      if (NS_FAILED(rv))
        return nullptr;
      rv = file->Create(nsIFile::NORMAL_FILE_TYPE, 00600);
      if (NS_FAILED(rv) && rv != NS_ERROR_FILE_ALREADY_EXISTS)
        return nullptr;
      if (NS_SUCCEEDED(rv))
        break;
    }
  }
  else
  {
    snprintf_literal(leaf, "%014" PRIX64 "-%X", hash, generation);
    rv = file->AppendNative(nsDependentCString(leaf));
    if (NS_FAILED(rv))
      return nullptr;
  }

  nsOfflineCacheBinding *binding = new nsOfflineCacheBinding;
  if (!binding)
    return nullptr;

  binding->mDataFile.swap(file);
  binding->mGeneration = generation;
  binding->mFlags = 0;
  return binding;
}

/******************************************************************************
 * nsOfflineCacheRecord
 */

struct nsOfflineCacheRecord
{
  const char    *clientID;
  const char    *key;
  const uint8_t *metaData;
  uint32_t       metaDataLen;
  int32_t        generation;
  int32_t        dataSize;
  int32_t        fetchCount;
  int64_t        lastFetched;
  int64_t        lastModified;
  int64_t        expirationTime;
};

static nsCacheEntry *
CreateCacheEntry(nsOfflineCacheDevice *device,
                 const nsCString *fullKey,
                 const nsOfflineCacheRecord &rec)
{
  nsCacheEntry *entry;

  if (device->IsLocked(*fullKey)) {
      return nullptr;
  }
  
  nsresult rv = nsCacheEntry::Create(fullKey->get(), // XXX enable sharing
                                     nsICache::STREAM_BASED,
                                     nsICache::STORE_OFFLINE,
                                     device, &entry);
  if (NS_FAILED(rv))
    return nullptr;

  entry->SetFetchCount((uint32_t) rec.fetchCount);
  entry->SetLastFetched(SecondsFromPRTime(rec.lastFetched));
  entry->SetLastModified(SecondsFromPRTime(rec.lastModified));
  entry->SetExpirationTime(SecondsFromPRTime(rec.expirationTime));
  entry->SetDataSize((uint32_t) rec.dataSize);

  entry->UnflattenMetaData((const char *) rec.metaData, rec.metaDataLen);

  // Restore security info, if present
  const char* info = entry->GetMetaDataElement("security-info");
  if (info) {
    nsCOMPtr<nsISupports> infoObj;
    rv = NS_DeserializeObject(nsDependentCString(info),
                              getter_AddRefs(infoObj));
    if (NS_FAILED(rv)) {
      delete entry;
      return nullptr;
    }
    entry->SetSecurityInfo(infoObj);
  }

  // create a binding object for this entry
  nsOfflineCacheBinding *binding =
      nsOfflineCacheBinding::Create(device->CacheDirectory(),
                                    fullKey,
                                    rec.generation);
  if (!binding)
  {
    delete entry;
    return nullptr;
  }
  entry->SetData(binding);

  return entry;
}


/******************************************************************************
 * nsOfflineCacheEntryInfo
 */

class nsOfflineCacheEntryInfo final : public nsICacheEntryInfo
{
  ~nsOfflineCacheEntryInfo() {}

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICACHEENTRYINFO

  nsOfflineCacheRecord *mRec;
};

NS_IMPL_ISUPPORTS(nsOfflineCacheEntryInfo, nsICacheEntryInfo)

NS_IMETHODIMP
nsOfflineCacheEntryInfo::GetClientID(char **result)
{
  *result = NS_strdup(mRec->clientID);
  return *result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsOfflineCacheEntryInfo::GetDeviceID(char ** deviceID)
{
  *deviceID = NS_strdup(OFFLINE_CACHE_DEVICE_ID);
  return *deviceID ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsOfflineCacheEntryInfo::GetKey(nsACString &clientKey)
{
  clientKey.Assign(mRec->key);
  return NS_OK;
}

NS_IMETHODIMP
nsOfflineCacheEntryInfo::GetFetchCount(int32_t *aFetchCount)
{
  *aFetchCount = mRec->fetchCount;
  return NS_OK;
}

NS_IMETHODIMP
nsOfflineCacheEntryInfo::GetLastFetched(uint32_t *aLastFetched)
{
  *aLastFetched = SecondsFromPRTime(mRec->lastFetched);
  return NS_OK;
}

NS_IMETHODIMP
nsOfflineCacheEntryInfo::GetLastModified(uint32_t *aLastModified)
{
  *aLastModified = SecondsFromPRTime(mRec->lastModified);
  return NS_OK;
}

NS_IMETHODIMP
nsOfflineCacheEntryInfo::GetExpirationTime(uint32_t *aExpirationTime)
{
  *aExpirationTime = SecondsFromPRTime(mRec->expirationTime);
  return NS_OK;
}

NS_IMETHODIMP
nsOfflineCacheEntryInfo::IsStreamBased(bool *aStreamBased)
{
  *aStreamBased = true;
  return NS_OK;
}

NS_IMETHODIMP
nsOfflineCacheEntryInfo::GetDataSize(uint32_t *aDataSize)
{
  *aDataSize = mRec->dataSize;
  return NS_OK;
}


/******************************************************************************
 * nsApplicationCacheNamespace
 */

NS_IMPL_ISUPPORTS(nsApplicationCacheNamespace, nsIApplicationCacheNamespace)

NS_IMETHODIMP
nsApplicationCacheNamespace::Init(uint32_t itemType,
                                  const nsACString &namespaceSpec,
                                  const nsACString &data)
{
  mItemType = itemType;
  mNamespaceSpec = namespaceSpec;
  mData = data;
  return NS_OK;
}

NS_IMETHODIMP
nsApplicationCacheNamespace::GetItemType(uint32_t *out)
{
  *out = mItemType;
  return NS_OK;
}

NS_IMETHODIMP
nsApplicationCacheNamespace::GetNamespaceSpec(nsACString &out)
{
  out = mNamespaceSpec;
  return NS_OK;
}

NS_IMETHODIMP
nsApplicationCacheNamespace::GetData(nsACString &out)
{
  out = mData;
  return NS_OK;
}

/******************************************************************************
 * nsApplicationCache
 */

NS_IMPL_ISUPPORTS(nsApplicationCache,
                  nsIApplicationCache,
                  nsISupportsWeakReference)

nsApplicationCache::nsApplicationCache()
  : mDevice(nullptr)
  , mValid(true)
{
}

nsApplicationCache::nsApplicationCache(nsOfflineCacheDevice *device,
                                       const nsACString &group,
                                       const nsACString &clientID)
  : mDevice(device)
  , mGroup(group)
  , mClientID(clientID)
  , mValid(true)
{
}

nsApplicationCache::~nsApplicationCache()
{
  if (!mDevice)
    return;

  {
    MutexAutoLock lock(mDevice->mLock);
    mDevice->mCaches.Remove(mClientID);
  }

  // If this isn't an active cache anymore, it can be destroyed.
  if (mValid && !mDevice->IsActiveCache(mGroup, mClientID))
    Discard();
}

void
nsApplicationCache::MarkInvalid()
{
  mValid = false;
}

NS_IMETHODIMP
nsApplicationCache::InitAsHandle(const nsACString &groupId,
                                 const nsACString &clientId)
{
  NS_ENSURE_FALSE(mDevice, NS_ERROR_ALREADY_INITIALIZED);
  NS_ENSURE_TRUE(mGroup.IsEmpty(), NS_ERROR_ALREADY_INITIALIZED);

  mGroup = groupId;
  mClientID = clientId;
  return NS_OK;
}

NS_IMETHODIMP
nsApplicationCache::GetManifestURI(nsIURI **out)
{
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), mGroup);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = uri->CloneIgnoringRef(out);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsApplicationCache::GetGroupID(nsACString &out)
{
  out = mGroup;
  return NS_OK;
}

NS_IMETHODIMP
nsApplicationCache::GetClientID(nsACString &out)
{
  out = mClientID;
  return NS_OK;
}

NS_IMETHODIMP
nsApplicationCache::GetProfileDirectory(nsIFile **out)
{
  if (mDevice->BaseDirectory())
      NS_ADDREF(*out = mDevice->BaseDirectory());
  else
      *out = nullptr;

  return NS_OK;
}

NS_IMETHODIMP
nsApplicationCache::GetActive(bool *out)
{
  NS_ENSURE_TRUE(mDevice, NS_ERROR_NOT_AVAILABLE);

  *out = mDevice->IsActiveCache(mGroup, mClientID);
  return NS_OK;
}

NS_IMETHODIMP
nsApplicationCache::Activate()
{
  NS_ENSURE_TRUE(mValid, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(mDevice, NS_ERROR_NOT_AVAILABLE);

  mDevice->ActivateCache(mGroup, mClientID);

  if (mDevice->AutoShutdown(this))
    mDevice = nullptr;

  return NS_OK;
}

NS_IMETHODIMP
nsApplicationCache::Discard()
{
  NS_ENSURE_TRUE(mValid, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(mDevice, NS_ERROR_NOT_AVAILABLE);

  mValid = false;

  nsCOMPtr<nsIRunnable> ev =
    new nsOfflineCacheDiscardCache(mDevice, mGroup, mClientID);
  nsresult rv = nsCacheService::DispatchToCacheIOThread(ev);
  return rv;
}

NS_IMETHODIMP
nsApplicationCache::MarkEntry(const nsACString &key,
                              uint32_t typeBits)
{
  NS_ENSURE_TRUE(mValid, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(mDevice, NS_ERROR_NOT_AVAILABLE);

  return mDevice->MarkEntry(mClientID, key, typeBits);
}


NS_IMETHODIMP
nsApplicationCache::UnmarkEntry(const nsACString &key,
                                uint32_t typeBits)
{
  NS_ENSURE_TRUE(mValid, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(mDevice, NS_ERROR_NOT_AVAILABLE);

  return mDevice->UnmarkEntry(mClientID, key, typeBits);
}

NS_IMETHODIMP
nsApplicationCache::GetTypes(const nsACString &key,
                             uint32_t *typeBits)
{
  NS_ENSURE_TRUE(mValid, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(mDevice, NS_ERROR_NOT_AVAILABLE);

  return mDevice->GetTypes(mClientID, key, typeBits);
}

NS_IMETHODIMP
nsApplicationCache::GatherEntries(uint32_t typeBits,
                                  uint32_t * count,
                                  char *** keys)
{
  NS_ENSURE_TRUE(mValid, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(mDevice, NS_ERROR_NOT_AVAILABLE);

  return mDevice->GatherEntries(mClientID, typeBits, count, keys);
}

NS_IMETHODIMP
nsApplicationCache::AddNamespaces(nsIArray *namespaces)
{
  NS_ENSURE_TRUE(mValid, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(mDevice, NS_ERROR_NOT_AVAILABLE);

  if (!namespaces)
    return NS_OK;

  mozStorageTransaction transaction(mDevice->mDB, false);

  uint32_t length;
  nsresult rv = namespaces->GetLength(&length);
  NS_ENSURE_SUCCESS(rv, rv);

  for (uint32_t i = 0; i < length; i++) {
    nsCOMPtr<nsIApplicationCacheNamespace> ns =
      do_QueryElementAt(namespaces, i);
    if (ns) {
      rv = mDevice->AddNamespace(mClientID, ns);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsApplicationCache::GetMatchingNamespace(const nsACString &key,
                                         nsIApplicationCacheNamespace **out)

{
  NS_ENSURE_TRUE(mValid, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(mDevice, NS_ERROR_NOT_AVAILABLE);

  return mDevice->GetMatchingNamespace(mClientID, key, out);
}

NS_IMETHODIMP
nsApplicationCache::GetUsage(uint32_t *usage)
{
  NS_ENSURE_TRUE(mValid, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(mDevice, NS_ERROR_NOT_AVAILABLE);

  return mDevice->GetUsage(mClientID, usage);
}

/******************************************************************************
 * nsCloseDBEvent
 *****************************************************************************/

class nsCloseDBEvent : public nsRunnable {
public:
  explicit nsCloseDBEvent(mozIStorageConnection *aDB)
  {
    mDB = aDB;
  }

  NS_IMETHOD Run()
  {
    mDB->Close();
    return NS_OK;
  }

protected:
  virtual ~nsCloseDBEvent() {}

private:
  nsCOMPtr<mozIStorageConnection> mDB;
};



/******************************************************************************
 * nsOfflineCacheDevice
 */

NS_IMPL_ISUPPORTS0(nsOfflineCacheDevice)

nsOfflineCacheDevice::nsOfflineCacheDevice()
  : mDB(nullptr)
  , mCacheCapacity(0)
  , mDeltaCounter(0)
  , mAutoShutdown(false)
  , mLock("nsOfflineCacheDevice.lock")
  , mActiveCaches(4)
  , mLockedEntries(32)
{
}

nsOfflineCacheDevice::~nsOfflineCacheDevice()
{}

/* static */
bool
nsOfflineCacheDevice::GetStrictFileOriginPolicy()
{
    nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);

    bool retval;
    if (prefs && NS_SUCCEEDED(prefs->GetBoolPref("security.fileuri.strict_origin_policy", &retval)))
        return retval;

    // As default value use true (be more strict)
    return true;
}

uint32_t
nsOfflineCacheDevice::CacheSize()
{
  AutoResetStatement statement(mStatement_CacheSize);

  bool hasRows;
  nsresult rv = statement->ExecuteStep(&hasRows);
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && hasRows, 0);
  
  return (uint32_t) statement->AsInt32(0);
}

uint32_t
nsOfflineCacheDevice::EntryCount()
{
  AutoResetStatement statement(mStatement_EntryCount);

  bool hasRows;
  nsresult rv = statement->ExecuteStep(&hasRows);
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && hasRows, 0);

  return (uint32_t) statement->AsInt32(0);
}

nsresult
nsOfflineCacheDevice::UpdateEntry(nsCacheEntry *entry)
{
  // Decompose the key into "ClientID" and "Key"
  nsAutoCString keyBuf;
  const char *cid, *key;

  if (!DecomposeCacheEntryKey(entry->Key(), &cid, &key, keyBuf))
    return NS_ERROR_UNEXPECTED;

  // Store security info, if it is serializable
  nsCOMPtr<nsISupports> infoObj = entry->SecurityInfo();
  nsCOMPtr<nsISerializable> serializable = do_QueryInterface(infoObj);
  if (infoObj && !serializable)
    return NS_ERROR_UNEXPECTED;

  if (serializable) {
    nsCString info;
    nsresult rv = NS_SerializeToString(serializable, info);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = entry->SetMetaDataElement("security-info", info.get());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCString metaDataBuf;
  uint32_t mdSize = entry->MetaDataSize();
  if (!metaDataBuf.SetLength(mdSize, fallible))
    return NS_ERROR_OUT_OF_MEMORY;
  char *md = metaDataBuf.BeginWriting();
  entry->FlattenMetaData(md, mdSize);

  nsOfflineCacheRecord rec;
  rec.metaData = (const uint8_t *) md;
  rec.metaDataLen = mdSize;
  rec.dataSize = entry->DataSize();
  rec.fetchCount = entry->FetchCount();
  rec.lastFetched = PRTimeFromSeconds(entry->LastFetched());
  rec.lastModified = PRTimeFromSeconds(entry->LastModified());
  rec.expirationTime = PRTimeFromSeconds(entry->ExpirationTime());

  AutoResetStatement statement(mStatement_UpdateEntry);

  nsresult rv;
  rv = statement->BindBlobByIndex(0, rec.metaData, rec.metaDataLen);
  nsresult tmp = statement->BindInt32ByIndex(1, rec.dataSize);
  if (NS_FAILED(tmp)) {
    rv = tmp;
  }
  tmp = statement->BindInt32ByIndex(2, rec.fetchCount);
  if (NS_FAILED(tmp)) {
    rv = tmp;
  }
  tmp = statement->BindInt64ByIndex(3, rec.lastFetched);
  if (NS_FAILED(tmp)) {
    rv = tmp;
  }
  tmp = statement->BindInt64ByIndex(4, rec.lastModified);
  if (NS_FAILED(tmp)) {
    rv = tmp;
  }
  tmp = statement->BindInt64ByIndex(5, rec.expirationTime);
  if (NS_FAILED(tmp)) {
    rv = tmp;
  }
  tmp = statement->BindUTF8StringByIndex(6, nsDependentCString(cid));
  if (NS_FAILED(tmp)) {
    rv = tmp;
  }
  tmp = statement->BindUTF8StringByIndex(7, nsDependentCString(key));
  if (NS_FAILED(tmp)) {
    rv = tmp;
  }
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasRows;
  rv = statement->ExecuteStep(&hasRows);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ASSERTION(!hasRows, "UPDATE should not result in output");
  return rv;
}

nsresult
nsOfflineCacheDevice::UpdateEntrySize(nsCacheEntry *entry, uint32_t newSize)
{
  // Decompose the key into "ClientID" and "Key"
  nsAutoCString keyBuf;
  const char *cid, *key;
  if (!DecomposeCacheEntryKey(entry->Key(), &cid, &key, keyBuf))
    return NS_ERROR_UNEXPECTED;

  AutoResetStatement statement(mStatement_UpdateEntrySize);

  nsresult rv = statement->BindInt32ByIndex(0, newSize);
  nsresult tmp = statement->BindUTF8StringByIndex(1, nsDependentCString(cid));
  if (NS_FAILED(tmp)) {
    rv = tmp;
  }
  tmp = statement->BindUTF8StringByIndex(2, nsDependentCString(key));
  if (NS_FAILED(tmp)) {
    rv = tmp;
  }
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasRows;
  rv = statement->ExecuteStep(&hasRows);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ASSERTION(!hasRows, "UPDATE should not result in output");
  return rv;
}

nsresult
nsOfflineCacheDevice::DeleteEntry(nsCacheEntry *entry, bool deleteData)
{
  if (deleteData)
  {
    nsresult rv = DeleteData(entry);
    if (NS_FAILED(rv))
      return rv;
  }

  // Decompose the key into "ClientID" and "Key"
  nsAutoCString keyBuf;
  const char *cid, *key;
  if (!DecomposeCacheEntryKey(entry->Key(), &cid, &key, keyBuf))
    return NS_ERROR_UNEXPECTED;

  AutoResetStatement statement(mStatement_DeleteEntry);

  nsresult rv = statement->BindUTF8StringByIndex(0, nsDependentCString(cid));
  nsresult rv2 = statement->BindUTF8StringByIndex(1, nsDependentCString(key));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_SUCCESS(rv2, rv2);

  bool hasRows;
  rv = statement->ExecuteStep(&hasRows);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ASSERTION(!hasRows, "DELETE should not result in output");
  return rv;
}

nsresult
nsOfflineCacheDevice::DeleteData(nsCacheEntry *entry)
{
  nsOfflineCacheBinding *binding = (nsOfflineCacheBinding *) entry->Data();
  NS_ENSURE_STATE(binding);

  return binding->mDataFile->Remove(false);
}

/**
 * nsCacheDevice implementation
 */

// This struct is local to nsOfflineCacheDevice::Init, but ISO C++98 doesn't
// allow a template (mozilla::ArrayLength) to be instantiated based on a local
// type.  Boo-urns!
struct StatementSql {
    nsCOMPtr<mozIStorageStatement> &statement;
    const char *sql;
    StatementSql (nsCOMPtr<mozIStorageStatement> &aStatement, const char *aSql):
      statement (aStatement), sql (aSql) {}
};

nsresult
nsOfflineCacheDevice::Init()
{
  MOZ_ASSERT(false, "Need to be initialized with sqlite");
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsOfflineCacheDevice::InitWithSqlite(mozIStorageService * ss)
{
  NS_ENSURE_TRUE(!mDB, NS_ERROR_ALREADY_INITIALIZED);

  // SetCacheParentDirectory must have been called
  NS_ENSURE_TRUE(mCacheDirectory, NS_ERROR_UNEXPECTED);

  // make sure the cache directory exists
  nsresult rv = EnsureDir(mCacheDirectory);
  NS_ENSURE_SUCCESS(rv, rv);

  // build path to index file
  nsCOMPtr<nsIFile> indexFile; 
  rv = mCacheDirectory->Clone(getter_AddRefs(indexFile));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = indexFile->AppendNative(NS_LITERAL_CSTRING("index.sqlite"));
  NS_ENSURE_SUCCESS(rv, rv);

  MOZ_ASSERT(ss, "nsOfflineCacheDevice::InitWithSqlite called before nsCacheService::Init() ?");
  NS_ENSURE_TRUE(ss, NS_ERROR_UNEXPECTED);

  rv = ss->OpenDatabase(indexFile, getter_AddRefs(mDB));
  NS_ENSURE_SUCCESS(rv, rv);

  mInitThread = do_GetCurrentThread();

  mDB->ExecuteSimpleSQL(NS_LITERAL_CSTRING("PRAGMA synchronous = OFF;"));

  // XXX ... other initialization steps

  // XXX in the future we may wish to verify the schema for moz_cache
  //     perhaps using "PRAGMA table_info" ?

  // build the table
  //
  //  "Generation" is the data file generation number.
  //
  rv = mDB->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TABLE IF NOT EXISTS moz_cache (\n"
                         "  ClientID        TEXT,\n"
                         "  Key             TEXT,\n"
                         "  MetaData        BLOB,\n"
                         "  Generation      INTEGER,\n"
                         "  DataSize        INTEGER,\n"
                         "  FetchCount      INTEGER,\n"
                         "  LastFetched     INTEGER,\n"
                         "  LastModified    INTEGER,\n"
                         "  ExpirationTime  INTEGER,\n"
                         "  ItemType        INTEGER DEFAULT 0\n"
                         ");\n"));
  NS_ENSURE_SUCCESS(rv, rv);

  // Databases from 1.9.0 don't have the ItemType column.  Add the column
  // here, but don't worry about failures (the column probably already exists)
  mDB->ExecuteSimpleSQL(
    NS_LITERAL_CSTRING("ALTER TABLE moz_cache ADD ItemType INTEGER DEFAULT 0"));

  // Create the table for storing cache groups.  All actions on
  // moz_cache_groups use the GroupID, so use it as the primary key.
  rv = mDB->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TABLE IF NOT EXISTS moz_cache_groups (\n"
                         " GroupID TEXT PRIMARY KEY,\n"
                         " ActiveClientID TEXT\n"
                         ");\n"));
  NS_ENSURE_SUCCESS(rv, rv);

  mDB->ExecuteSimpleSQL(
    NS_LITERAL_CSTRING("ALTER TABLE moz_cache_groups "
                       "ADD ActivateTimeStamp INTEGER DEFAULT 0"));

  // ClientID: clientID joining moz_cache and moz_cache_namespaces
  // tables.
  // Data: Data associated with this namespace (e.g. a fallback URI
  // for fallback entries).
  // ItemType: the type of namespace.
  rv = mDB->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TABLE IF NOT EXISTS"
                         " moz_cache_namespaces (\n"
                         " ClientID TEXT,\n"
                         " NameSpace TEXT,\n"
                         " Data TEXT,\n"
                         " ItemType INTEGER\n"
                          ");\n"));
   NS_ENSURE_SUCCESS(rv, rv);

  // Databases from 1.9.0 have a moz_cache_index that should be dropped
  rv = mDB->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("DROP INDEX IF EXISTS moz_cache_index"));
  NS_ENSURE_SUCCESS(rv, rv);

  // Key/ClientID pairs should be unique in the database.  All queries
  // against moz_cache use the Key (which is also the most unique), so
  // use it as the primary key for this index.
  rv = mDB->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE UNIQUE INDEX IF NOT EXISTS "
                         " moz_cache_key_clientid_index"
                         " ON moz_cache (Key, ClientID);"));
  NS_ENSURE_SUCCESS(rv, rv);

  // Used for ClientID lookups and to keep ClientID/NameSpace pairs unique.
  rv = mDB->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE UNIQUE INDEX IF NOT EXISTS"
                         " moz_cache_namespaces_clientid_index"
                         " ON moz_cache_namespaces (ClientID, NameSpace);"));
  NS_ENSURE_SUCCESS(rv, rv);

  // Used for namespace lookups.
  rv = mDB->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE INDEX IF NOT EXISTS"
                         " moz_cache_namespaces_namespace_index"
                         " ON moz_cache_namespaces (NameSpace);"));
  NS_ENSURE_SUCCESS(rv, rv);


  mEvictionFunction = new nsOfflineCacheEvictionFunction(this);
  if (!mEvictionFunction) return NS_ERROR_OUT_OF_MEMORY;

  rv = mDB->CreateFunction(NS_LITERAL_CSTRING("cache_eviction_observer"), 3, mEvictionFunction);
  NS_ENSURE_SUCCESS(rv, rv);

  // create all (most) of our statements up front
  StatementSql prepared[] = {
    StatementSql ( mStatement_CacheSize,         "SELECT Sum(DataSize) from moz_cache;" ),
    StatementSql ( mStatement_ApplicationCacheSize, "SELECT Sum(DataSize) from moz_cache WHERE ClientID = ?;" ),
    StatementSql ( mStatement_EntryCount,        "SELECT count(*) from moz_cache;" ),
    StatementSql ( mStatement_UpdateEntry,       "UPDATE moz_cache SET MetaData = ?, DataSize = ?, FetchCount = ?, LastFetched = ?, LastModified = ?, ExpirationTime = ? WHERE ClientID = ? AND Key = ?;" ),
    StatementSql ( mStatement_UpdateEntrySize,   "UPDATE moz_cache SET DataSize = ? WHERE ClientID = ? AND Key = ?;" ),
    StatementSql ( mStatement_DeleteEntry,       "DELETE FROM moz_cache WHERE ClientID = ? AND Key = ?;" ),
    StatementSql ( mStatement_FindEntry,         "SELECT MetaData, Generation, DataSize, FetchCount, LastFetched, LastModified, ExpirationTime, ItemType FROM moz_cache WHERE ClientID = ? AND Key = ?;" ),
    StatementSql ( mStatement_BindEntry,         "INSERT INTO moz_cache (ClientID, Key, MetaData, Generation, DataSize, FetchCount, LastFetched, LastModified, ExpirationTime) VALUES(?,?,?,?,?,?,?,?,?);" ),

    StatementSql ( mStatement_MarkEntry,         "UPDATE moz_cache SET ItemType = (ItemType | ?) WHERE ClientID = ? AND Key = ?;" ),
    StatementSql ( mStatement_UnmarkEntry,       "UPDATE moz_cache SET ItemType = (ItemType & ~?) WHERE ClientID = ? AND Key = ?;" ),
    StatementSql ( mStatement_GetTypes,          "SELECT ItemType FROM moz_cache WHERE ClientID = ? AND Key = ?;"),
    StatementSql ( mStatement_CleanupUnmarked,   "DELETE FROM moz_cache WHERE ClientID = ? AND Key = ? AND ItemType = 0;" ),
    StatementSql ( mStatement_GatherEntries,     "SELECT Key FROM moz_cache WHERE ClientID = ? AND (ItemType & ?) > 0;" ),

    StatementSql ( mStatement_ActivateClient,    "INSERT OR REPLACE INTO moz_cache_groups (GroupID, ActiveClientID, ActivateTimeStamp) VALUES (?, ?, ?);" ),
    StatementSql ( mStatement_DeactivateGroup,   "DELETE FROM moz_cache_groups WHERE GroupID = ?;" ),
    StatementSql ( mStatement_FindClient,        "SELECT ClientID, ItemType FROM moz_cache WHERE Key = ? ORDER BY LastFetched DESC, LastModified DESC;" ),

    // Search for namespaces that match the URI.  Use the <= operator
    // to ensure that we use the index on moz_cache_namespaces.
    StatementSql ( mStatement_FindClientByNamespace, "SELECT ns.ClientID, ns.ItemType FROM"
                                                     "  moz_cache_namespaces AS ns JOIN moz_cache_groups AS groups"
                                                     "  ON ns.ClientID = groups.ActiveClientID"
                                                     " WHERE ns.NameSpace <= ?1 AND ?1 GLOB ns.NameSpace || '*'"
                                                     " ORDER BY ns.NameSpace DESC, groups.ActivateTimeStamp DESC;"),
    StatementSql ( mStatement_FindNamespaceEntry,    "SELECT NameSpace, Data, ItemType FROM moz_cache_namespaces"
                                                     " WHERE ClientID = ?1"
                                                     " AND NameSpace <= ?2 AND ?2 GLOB NameSpace || '*'"
                                                     " ORDER BY NameSpace DESC;"),
    StatementSql ( mStatement_InsertNamespaceEntry,  "INSERT INTO moz_cache_namespaces (ClientID, NameSpace, Data, ItemType) VALUES(?, ?, ?, ?);"),
    StatementSql ( mStatement_EnumerateApps,         "SELECT GroupID, ActiveClientID FROM moz_cache_groups WHERE GroupID LIKE ?1;"),
    StatementSql ( mStatement_EnumerateGroups,       "SELECT GroupID, ActiveClientID FROM moz_cache_groups;"),
    StatementSql ( mStatement_EnumerateGroupsTimeOrder, "SELECT GroupID, ActiveClientID FROM moz_cache_groups ORDER BY ActivateTimeStamp;")
  };
  for (uint32_t i = 0; NS_SUCCEEDED(rv) && i < ArrayLength(prepared); ++i)
  {
    LOG(("Creating statement: %s\n", prepared[i].sql));

    rv = mDB->CreateStatement(nsDependentCString(prepared[i].sql),
                              getter_AddRefs(prepared[i].statement));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = InitActiveCaches();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

namespace {

nsresult
GetGroupForCache(const nsCSubstring &clientID, nsCString &group)
{
  group.Assign(clientID);
  group.Truncate(group.FindChar('|'));
  NS_UnescapeURL(group);

  return NS_OK;
}

void
AppendJARIdentifier(nsACString &_result, NeckoOriginAttributes const *aOriginAttributes)
{
  nsAutoCString suffix;
  aOriginAttributes->CreateSuffix(suffix);
  _result.Append('#');
  _result.Append(suffix);
}

} // namespace

// static
nsresult
nsOfflineCacheDevice::BuildApplicationCacheGroupID(nsIURI *aManifestURL,
                                                   NeckoOriginAttributes const *aOriginAttributes,
                                                   nsACString &_result)
{
  nsCOMPtr<nsIURI> newURI;
  nsresult rv = aManifestURL->CloneIgnoringRef(getter_AddRefs(newURI));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString manifestSpec;
  rv = newURI->GetAsciiSpec(manifestSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  _result.Assign(manifestSpec);

  if (aOriginAttributes) {
    AppendJARIdentifier(_result, aOriginAttributes);
  }

  return NS_OK;
}

nsresult
nsOfflineCacheDevice::InitActiveCaches()
{
  MutexAutoLock lock(mLock);

  AutoResetStatement statement(mStatement_EnumerateGroups);

  bool hasRows;
  nsresult rv = statement->ExecuteStep(&hasRows);
  NS_ENSURE_SUCCESS(rv, rv);

  while (hasRows)
  {
    nsAutoCString group;
    statement->GetUTF8String(0, group);
    nsCString clientID;
    statement->GetUTF8String(1, clientID);

    mActiveCaches.PutEntry(clientID);
    mActiveCachesByGroup.Put(group, new nsCString(clientID));

    rv = statement->ExecuteStep(&hasRows);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
nsOfflineCacheDevice::Shutdown()
{
  NS_ENSURE_TRUE(mDB, NS_ERROR_NOT_INITIALIZED);

  {
    MutexAutoLock lock(mLock);
    for (auto iter = mCaches.Iter(); !iter.Done(); iter.Next()) {
      nsCOMPtr<nsIApplicationCache> obj = do_QueryReferent(iter.UserData());
      if (obj) {
        auto appCache = static_cast<nsApplicationCache*>(obj.get());
        appCache->MarkInvalid();
      }
    }
  }

  {
  EvictionObserver evictionObserver(mDB, mEvictionFunction);

  // Delete all rows whose clientID is not an active clientID.
  nsresult rv = mDB->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DELETE FROM moz_cache WHERE rowid IN"
    "  (SELECT moz_cache.rowid FROM"
    "    moz_cache LEFT OUTER JOIN moz_cache_groups ON"
    "      (moz_cache.ClientID = moz_cache_groups.ActiveClientID)"
    "   WHERE moz_cache_groups.GroupID ISNULL)"));

  if (NS_FAILED(rv))
    NS_WARNING("Failed to clean up unused application caches.");
  else
    evictionObserver.Apply();

  // Delete all namespaces whose clientID is not an active clientID.
  rv = mDB->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DELETE FROM moz_cache_namespaces WHERE rowid IN"
    "  (SELECT moz_cache_namespaces.rowid FROM"
    "    moz_cache_namespaces LEFT OUTER JOIN moz_cache_groups ON"
    "      (moz_cache_namespaces.ClientID = moz_cache_groups.ActiveClientID)"
    "   WHERE moz_cache_groups.GroupID ISNULL)"));

  if (NS_FAILED(rv))
    NS_WARNING("Failed to clean up namespaces.");

  mEvictionFunction = 0;

  mStatement_CacheSize = nullptr;
  mStatement_ApplicationCacheSize = nullptr;
  mStatement_EntryCount = nullptr;
  mStatement_UpdateEntry = nullptr;
  mStatement_UpdateEntrySize = nullptr;
  mStatement_DeleteEntry = nullptr;
  mStatement_FindEntry = nullptr;
  mStatement_BindEntry = nullptr;
  mStatement_ClearDomain = nullptr;
  mStatement_MarkEntry = nullptr;
  mStatement_UnmarkEntry = nullptr;
  mStatement_GetTypes = nullptr;
  mStatement_FindNamespaceEntry = nullptr;
  mStatement_InsertNamespaceEntry = nullptr;
  mStatement_CleanupUnmarked = nullptr;
  mStatement_GatherEntries = nullptr;
  mStatement_ActivateClient = nullptr;
  mStatement_DeactivateGroup = nullptr;
  mStatement_FindClient = nullptr;
  mStatement_FindClientByNamespace = nullptr;
  mStatement_EnumerateApps = nullptr;
  mStatement_EnumerateGroups = nullptr;
  mStatement_EnumerateGroupsTimeOrder = nullptr;
  }

  // Close Database on the correct thread
  bool isOnCurrentThread = true;
  if (mInitThread)
    mInitThread->IsOnCurrentThread(&isOnCurrentThread);

  if (!isOnCurrentThread) {
    nsCOMPtr<nsIRunnable> ev = new nsCloseDBEvent(mDB);

    if (ev) {
      mInitThread->Dispatch(ev, NS_DISPATCH_NORMAL);
    }
  }
  else {
    mDB->Close();
  }

  mDB = nullptr;
  mInitThread = nullptr;

  return NS_OK;
}

const char *
nsOfflineCacheDevice::GetDeviceID()
{
  return OFFLINE_CACHE_DEVICE_ID;
}

nsCacheEntry *
nsOfflineCacheDevice::FindEntry(nsCString *fullKey, bool *collision)
{
  mozilla::Telemetry::AutoTimer<mozilla::Telemetry::CACHE_OFFLINE_SEARCH_2> timer;
  LOG(("nsOfflineCacheDevice::FindEntry [key=%s]\n", fullKey->get()));

  // SELECT * FROM moz_cache WHERE key = ?

  // Decompose the key into "ClientID" and "Key"
  nsAutoCString keyBuf;
  const char *cid, *key;
  if (!DecomposeCacheEntryKey(fullKey, &cid, &key, keyBuf))
    return nullptr;

  AutoResetStatement statement(mStatement_FindEntry);

  nsresult rv = statement->BindUTF8StringByIndex(0, nsDependentCString(cid));
  nsresult rv2 = statement->BindUTF8StringByIndex(1, nsDependentCString(key));
  NS_ENSURE_SUCCESS(rv, nullptr);
  NS_ENSURE_SUCCESS(rv2, nullptr);

  bool hasRows;
  rv = statement->ExecuteStep(&hasRows);
  if (NS_FAILED(rv) || !hasRows)
    return nullptr; // entry not found

  nsOfflineCacheRecord rec;
  statement->GetSharedBlob(0, &rec.metaDataLen,
                           (const uint8_t **) &rec.metaData);
  rec.generation     = statement->AsInt32(1);
  rec.dataSize       = statement->AsInt32(2);
  rec.fetchCount     = statement->AsInt32(3);
  rec.lastFetched    = statement->AsInt64(4);
  rec.lastModified   = statement->AsInt64(5);
  rec.expirationTime = statement->AsInt64(6);

  LOG(("entry: [%u %d %d %d %lld %lld %lld]\n",
        rec.metaDataLen,
        rec.generation,
        rec.dataSize,
        rec.fetchCount,
        rec.lastFetched,
        rec.lastModified,
        rec.expirationTime));

  nsCacheEntry *entry = CreateCacheEntry(this, fullKey, rec);

  if (entry)
  {
    // make sure that the data file exists
    nsOfflineCacheBinding *binding = (nsOfflineCacheBinding*)entry->Data();
    bool isFile;
    rv = binding->mDataFile->IsFile(&isFile);
    if (NS_FAILED(rv) || !isFile)
    {
      DeleteEntry(entry, false);
      delete entry;
      return nullptr;
    }

    // lock the entry
    Lock(*fullKey);
  }

  return entry;
}

nsresult
nsOfflineCacheDevice::DeactivateEntry(nsCacheEntry *entry)
{
  LOG(("nsOfflineCacheDevice::DeactivateEntry [key=%s]\n",
       entry->Key()->get()));

  // This method is called to inform us that the nsCacheEntry object is going
  // away.  We should persist anything that needs to be persisted, or if the
  // entry is doomed, we can go ahead and clear its storage.

  if (entry->IsDoomed())
  {
    // remove corresponding row and file if they exist

    // the row should have been removed in DoomEntry... we could assert that
    // that happened.  otherwise, all we have to do here is delete the file
    // on disk.
    DeleteData(entry);
  }
  else if (((nsOfflineCacheBinding *)entry->Data())->IsNewEntry())
  {
    // UPDATE the database row

    // Only new entries are updated, since offline cache is updated in
    // transactions.  New entries are those who is returned from
    // BindEntry().

    LOG(("nsOfflineCacheDevice::DeactivateEntry updating new entry\n"));
    UpdateEntry(entry);
  } else {
    LOG(("nsOfflineCacheDevice::DeactivateEntry "
	 "skipping update since entry is not dirty\n"));
  }

  // Unlock the entry
  Unlock(*entry->Key());

  delete entry;

  return NS_OK;
}

nsresult
nsOfflineCacheDevice::BindEntry(nsCacheEntry *entry)
{
  LOG(("nsOfflineCacheDevice::BindEntry [key=%s]\n", entry->Key()->get()));

  NS_ENSURE_STATE(!entry->Data());

  // This method is called to inform us that we have a new entry.  The entry
  // may collide with an existing entry in our DB, but if that happens we can
  // assume that the entry is not being used.

  // INSERT the database row

  // XXX Assumption: if the row already exists, then FindEntry would have
  // returned it.  if that entry was doomed, then DoomEntry would have removed
  // it from the table.  so, we should always have to insert at this point.

  // Decompose the key into "ClientID" and "Key"
  nsAutoCString keyBuf;
  const char *cid, *key;
  if (!DecomposeCacheEntryKey(entry->Key(), &cid, &key, keyBuf))
    return NS_ERROR_UNEXPECTED;

  // create binding, pick best generation number
  RefPtr<nsOfflineCacheBinding> binding =
      nsOfflineCacheBinding::Create(mCacheDirectory, entry->Key(), -1);
  if (!binding)
    return NS_ERROR_OUT_OF_MEMORY;
  binding->MarkNewEntry();

  nsOfflineCacheRecord rec;
  rec.clientID = cid;
  rec.key = key;
  rec.metaData = nullptr; // don't write any metadata now.
  rec.metaDataLen = 0;
  rec.generation = binding->mGeneration;
  rec.dataSize = 0;
  rec.fetchCount = entry->FetchCount();
  rec.lastFetched = PRTimeFromSeconds(entry->LastFetched());
  rec.lastModified = PRTimeFromSeconds(entry->LastModified());
  rec.expirationTime = PRTimeFromSeconds(entry->ExpirationTime());

  AutoResetStatement statement(mStatement_BindEntry);

  nsresult rv = statement->BindUTF8StringByIndex(0, nsDependentCString(rec.clientID));
  nsresult tmp = statement->BindUTF8StringByIndex(1, nsDependentCString(rec.key));
  if (NS_FAILED(tmp)) {
    rv = tmp;
  }
  tmp = statement->BindBlobByIndex(2, rec.metaData, rec.metaDataLen);
  if (NS_FAILED(tmp)) {
    rv = tmp;
  }
  tmp = statement->BindInt32ByIndex(3, rec.generation);
  if (NS_FAILED(tmp)) {
    rv = tmp;
  }
  tmp = statement->BindInt32ByIndex(4, rec.dataSize);
  if (NS_FAILED(tmp)) {
    rv = tmp;
  }
  tmp = statement->BindInt32ByIndex(5, rec.fetchCount);
  if (NS_FAILED(tmp)) {
    rv = tmp;
  }
  tmp = statement->BindInt64ByIndex(6, rec.lastFetched);
  if (NS_FAILED(tmp)) {
    rv = tmp;
  }
  tmp = statement->BindInt64ByIndex(7, rec.lastModified);
  if (NS_FAILED(tmp)) {
    rv = tmp;
  }
  tmp = statement->BindInt64ByIndex(8, rec.expirationTime);
  if (NS_FAILED(tmp)) {
    rv = tmp;
  }
  NS_ENSURE_SUCCESS(rv, rv);
  
  bool hasRows;
  rv = statement->ExecuteStep(&hasRows);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ASSERTION(!hasRows, "INSERT should not result in output");

  entry->SetData(binding);

  // lock the entry
  Lock(*entry->Key());

  return NS_OK;
}

void
nsOfflineCacheDevice::DoomEntry(nsCacheEntry *entry)
{
  LOG(("nsOfflineCacheDevice::DoomEntry [key=%s]\n", entry->Key()->get()));

  // This method is called to inform us that we should mark the entry to be
  // deleted when it is no longer in use.

  // We can go ahead and delete the corresponding row in our table,
  // but we must not delete the file on disk until we are deactivated.
  // In another word, the file should be deleted if the entry had been
  // deactivated.
  
  DeleteEntry(entry, !entry->IsActive());
}

nsresult
nsOfflineCacheDevice::OpenInputStreamForEntry(nsCacheEntry      *entry,
                                              nsCacheAccessMode  mode,
                                              uint32_t           offset,
                                              nsIInputStream   **result)
{
  LOG(("nsOfflineCacheDevice::OpenInputStreamForEntry [key=%s]\n",
       entry->Key()->get()));

  *result = nullptr;

  NS_ENSURE_TRUE(!offset || (offset < entry->DataSize()), NS_ERROR_INVALID_ARG);

  // return an input stream to the entry's data file.  the stream
  // may be read on a background thread.

  nsOfflineCacheBinding *binding = (nsOfflineCacheBinding *) entry->Data();
  NS_ENSURE_STATE(binding);

  nsCOMPtr<nsIInputStream> in;
  NS_NewLocalFileInputStream(getter_AddRefs(in), binding->mDataFile, PR_RDONLY);
  if (!in)
    return NS_ERROR_UNEXPECTED;

  // respect |offset| param
  if (offset != 0)
  {
    nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(in);
    NS_ENSURE_TRUE(seekable, NS_ERROR_UNEXPECTED);

    seekable->Seek(nsISeekableStream::NS_SEEK_SET, offset);
  }

  in.swap(*result);
  return NS_OK;
}

nsresult
nsOfflineCacheDevice::OpenOutputStreamForEntry(nsCacheEntry       *entry,
                                               nsCacheAccessMode   mode,
                                               uint32_t            offset,
                                               nsIOutputStream   **result)
{
  LOG(("nsOfflineCacheDevice::OpenOutputStreamForEntry [key=%s]\n",
       entry->Key()->get()));

  *result = nullptr;

  NS_ENSURE_TRUE(offset <= entry->DataSize(), NS_ERROR_INVALID_ARG);

  // return an output stream to the entry's data file.  we can assume
  // that the output stream will only be used on the main thread.

  nsOfflineCacheBinding *binding = (nsOfflineCacheBinding *) entry->Data();
  NS_ENSURE_STATE(binding);

  nsCOMPtr<nsIOutputStream> out;
  NS_NewLocalFileOutputStream(getter_AddRefs(out), binding->mDataFile,
                              PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE,
                              00600);
  if (!out)
    return NS_ERROR_UNEXPECTED;

  // respect |offset| param
  nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(out);
  NS_ENSURE_TRUE(seekable, NS_ERROR_UNEXPECTED);
  if (offset != 0)
    seekable->Seek(nsISeekableStream::NS_SEEK_SET, offset);

  // truncate the file at the given offset
  seekable->SetEOF();

  nsCOMPtr<nsIOutputStream> bufferedOut;
  nsresult rv =
    NS_NewBufferedOutputStream(getter_AddRefs(bufferedOut), out, 16 * 1024);
  NS_ENSURE_SUCCESS(rv, rv);

  bufferedOut.swap(*result);
  return NS_OK;
}

nsresult
nsOfflineCacheDevice::GetFileForEntry(nsCacheEntry *entry, nsIFile **result)
{
  LOG(("nsOfflineCacheDevice::GetFileForEntry [key=%s]\n",
       entry->Key()->get()));

  nsOfflineCacheBinding *binding = (nsOfflineCacheBinding *) entry->Data();
  NS_ENSURE_STATE(binding);

  NS_IF_ADDREF(*result = binding->mDataFile);
  return NS_OK;
}

nsresult
nsOfflineCacheDevice::OnDataSizeChange(nsCacheEntry *entry, int32_t deltaSize)
{
  LOG(("nsOfflineCacheDevice::OnDataSizeChange [key=%s delta=%d]\n",
      entry->Key()->get(), deltaSize));

  const int32_t DELTA_THRESHOLD = 1<<14; // 16k

  // called to notify us of an impending change in the total size of the
  // specified entry.

  uint32_t oldSize = entry->DataSize();
  NS_ASSERTION(deltaSize >= 0 || int32_t(oldSize) + deltaSize >= 0, "oops");
  uint32_t newSize = int32_t(oldSize) + deltaSize;
  UpdateEntrySize(entry, newSize);

  mDeltaCounter += deltaSize; // this may go negative

  if (mDeltaCounter >= DELTA_THRESHOLD)
  {
    if (CacheSize() > mCacheCapacity) {
      // the entry will overrun the cache capacity, doom the entry
      // and abort
#ifdef DEBUG
      nsresult rv =
#endif
        nsCacheService::DoomEntry(entry);
      NS_ASSERTION(NS_SUCCEEDED(rv), "DoomEntry() failed.");
      return NS_ERROR_ABORT;
    }

    mDeltaCounter = 0; // reset counter
  }

  return NS_OK;
}

nsresult
nsOfflineCacheDevice::Visit(nsICacheVisitor *visitor)
{
  NS_ENSURE_TRUE(Initialized(), NS_ERROR_NOT_INITIALIZED);

  // called to enumerate the offline cache.

  nsCOMPtr<nsICacheDeviceInfo> deviceInfo =
      new nsOfflineCacheDeviceInfo(this);

  bool keepGoing;
  nsresult rv = visitor->VisitDevice(OFFLINE_CACHE_DEVICE_ID, deviceInfo,
                                     &keepGoing);
  if (NS_FAILED(rv))
    return rv;
  
  if (!keepGoing)
    return NS_OK;

  // SELECT * from moz_cache;

  nsOfflineCacheRecord rec;
  RefPtr<nsOfflineCacheEntryInfo> info = new nsOfflineCacheEntryInfo;
  if (!info)
    return NS_ERROR_OUT_OF_MEMORY;
  info->mRec = &rec;

  // XXX may want to list columns explicitly
  nsCOMPtr<mozIStorageStatement> statement;
  rv = mDB->CreateStatement(
      NS_LITERAL_CSTRING("SELECT * FROM moz_cache;"),
      getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasRows;
  for (;;)
  {
    rv = statement->ExecuteStep(&hasRows);
    if (NS_FAILED(rv) || !hasRows)
      break;

    statement->GetSharedUTF8String(0, nullptr, &rec.clientID);
    statement->GetSharedUTF8String(1, nullptr, &rec.key);
    statement->GetSharedBlob(2, &rec.metaDataLen,
                             (const uint8_t **) &rec.metaData);
    rec.generation     = statement->AsInt32(3);
    rec.dataSize       = statement->AsInt32(4);
    rec.fetchCount     = statement->AsInt32(5);
    rec.lastFetched    = statement->AsInt64(6);
    rec.lastModified   = statement->AsInt64(7);
    rec.expirationTime = statement->AsInt64(8);

    bool keepGoing;
    rv = visitor->VisitEntry(OFFLINE_CACHE_DEVICE_ID, info, &keepGoing);
    if (NS_FAILED(rv) || !keepGoing)
      break;
  }

  info->mRec = nullptr;
  return NS_OK;
}

nsresult
nsOfflineCacheDevice::EvictEntries(const char *clientID)
{
  LOG(("nsOfflineCacheDevice::EvictEntries [cid=%s]\n",
       clientID ? clientID : ""));

  // called to evict all entries matching the given clientID.

  // need trigger to fire user defined function after a row is deleted
  // so we can delete the corresponding data file.
  EvictionObserver evictionObserver(mDB, mEvictionFunction);

  nsCOMPtr<mozIStorageStatement> statement;
  nsresult rv;
  if (clientID)
  {
    rv = mDB->CreateStatement(NS_LITERAL_CSTRING("DELETE FROM moz_cache WHERE ClientID=?;"),
                              getter_AddRefs(statement));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = statement->BindUTF8StringByIndex(0, nsDependentCString(clientID));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = statement->Execute();
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mDB->CreateStatement(NS_LITERAL_CSTRING("DELETE FROM moz_cache_groups WHERE ActiveClientID=?;"),
                              getter_AddRefs(statement));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = statement->BindUTF8StringByIndex(0, nsDependentCString(clientID));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = statement->Execute();
    NS_ENSURE_SUCCESS(rv, rv);

    // TODO - Should update internal hashtables.
    // Low priority, since this API is not widely used.
  }
  else
  {
    rv = mDB->CreateStatement(NS_LITERAL_CSTRING("DELETE FROM moz_cache;"),
                              getter_AddRefs(statement));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = statement->Execute();
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mDB->CreateStatement(NS_LITERAL_CSTRING("DELETE FROM moz_cache_groups;"),
                              getter_AddRefs(statement));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = statement->Execute();
    NS_ENSURE_SUCCESS(rv, rv);

    MutexAutoLock lock(mLock);
    mCaches.Clear();
    mActiveCaches.Clear();
    mActiveCachesByGroup.Clear();
  }

  evictionObserver.Apply();

  statement = nullptr;
  // Also evict any namespaces associated with this clientID.
  if (clientID)
  {
    rv = mDB->CreateStatement(NS_LITERAL_CSTRING("DELETE FROM moz_cache_namespaces WHERE ClientID=?"),
                              getter_AddRefs(statement));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = statement->BindUTF8StringByIndex(0, nsDependentCString(clientID));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else
  {
    rv = mDB->CreateStatement(NS_LITERAL_CSTRING("DELETE FROM moz_cache_namespaces;"),
                              getter_AddRefs(statement));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsOfflineCacheDevice::MarkEntry(const nsCString &clientID,
                                const nsACString &key,
                                uint32_t typeBits)
{
  LOG(("nsOfflineCacheDevice::MarkEntry [cid=%s, key=%s, typeBits=%d]\n",
       clientID.get(), PromiseFlatCString(key).get(), typeBits));

  AutoResetStatement statement(mStatement_MarkEntry);
  nsresult rv = statement->BindInt32ByIndex(0, typeBits);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindUTF8StringByIndex(1, clientID);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindUTF8StringByIndex(2, key);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsOfflineCacheDevice::UnmarkEntry(const nsCString &clientID,
                                  const nsACString &key,
                                  uint32_t typeBits)
{
  LOG(("nsOfflineCacheDevice::UnmarkEntry [cid=%s, key=%s, typeBits=%d]\n",
       clientID.get(), PromiseFlatCString(key).get(), typeBits));

  AutoResetStatement statement(mStatement_UnmarkEntry);
  nsresult rv = statement->BindInt32ByIndex(0, typeBits);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindUTF8StringByIndex(1, clientID);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindUTF8StringByIndex(2, key);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // Remove the entry if it is now empty.

  EvictionObserver evictionObserver(mDB, mEvictionFunction);

  AutoResetStatement cleanupStatement(mStatement_CleanupUnmarked);
  rv = cleanupStatement->BindUTF8StringByIndex(0, clientID);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = cleanupStatement->BindUTF8StringByIndex(1, key);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = cleanupStatement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  evictionObserver.Apply();

  return NS_OK;
}

nsresult
nsOfflineCacheDevice::GetMatchingNamespace(const nsCString &clientID,
                                           const nsACString &key,
                                           nsIApplicationCacheNamespace **out)
{
  LOG(("nsOfflineCacheDevice::GetMatchingNamespace [cid=%s, key=%s]\n",
       clientID.get(), PromiseFlatCString(key).get()));

  nsresult rv;

  AutoResetStatement statement(mStatement_FindNamespaceEntry);

  rv = statement->BindUTF8StringByIndex(0, clientID);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindUTF8StringByIndex(1, key);
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasRows;
  rv = statement->ExecuteStep(&hasRows);
  NS_ENSURE_SUCCESS(rv, rv);

  *out = nullptr;

  bool found = false;
  nsCString nsSpec;
  int32_t nsType = 0;
  nsCString nsData;

  while (hasRows)
  {
    int32_t itemType;
    rv = statement->GetInt32(2, &itemType);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!found || itemType > nsType)
    {
      nsType = itemType;

      rv = statement->GetUTF8String(0, nsSpec);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = statement->GetUTF8String(1, nsData);
      NS_ENSURE_SUCCESS(rv, rv);

      found = true;
    }

    rv = statement->ExecuteStep(&hasRows);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (found) {
    nsCOMPtr<nsIApplicationCacheNamespace> ns =
      new nsApplicationCacheNamespace();
    if (!ns)
      return NS_ERROR_OUT_OF_MEMORY;
    rv = ns->Init(nsType, nsSpec, nsData);
    NS_ENSURE_SUCCESS(rv, rv);

    ns.swap(*out);
  }

  return NS_OK;
}

nsresult
nsOfflineCacheDevice::CacheOpportunistically(const nsCString &clientID,
                                             const nsACString &key)
{
  // XXX: We should also be propagating this cache entry to other matching
  // caches.  See bug 444807.

  return MarkEntry(clientID, key, nsIApplicationCache::ITEM_OPPORTUNISTIC);
}

nsresult
nsOfflineCacheDevice::GetTypes(const nsCString &clientID,
                               const nsACString &key,
                               uint32_t *typeBits)
{
  LOG(("nsOfflineCacheDevice::GetTypes [cid=%s, key=%s]\n",
       clientID.get(), PromiseFlatCString(key).get()));

  AutoResetStatement statement(mStatement_GetTypes);
  nsresult rv = statement->BindUTF8StringByIndex(0, clientID);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindUTF8StringByIndex(1, key);
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasRows;
  rv = statement->ExecuteStep(&hasRows);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!hasRows)
    return NS_ERROR_CACHE_KEY_NOT_FOUND;

  *typeBits = statement->AsInt32(0);

  return NS_OK;
}

nsresult
nsOfflineCacheDevice::GatherEntries(const nsCString &clientID,
                                    uint32_t typeBits,
                                    uint32_t *count,
                                    char ***keys)
{
  LOG(("nsOfflineCacheDevice::GatherEntries [cid=%s, typeBits=%X]\n",
       clientID.get(), typeBits));

  AutoResetStatement statement(mStatement_GatherEntries);
  nsresult rv = statement->BindUTF8StringByIndex(0, clientID);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindInt32ByIndex(1, typeBits);
  NS_ENSURE_SUCCESS(rv, rv);

  return RunSimpleQuery(mStatement_GatherEntries, 0, count, keys);
}

nsresult
nsOfflineCacheDevice::AddNamespace(const nsCString &clientID,
                                   nsIApplicationCacheNamespace *ns)
{
  nsCString namespaceSpec;
  nsresult rv = ns->GetNamespaceSpec(namespaceSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString data;
  rv = ns->GetData(data);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t itemType;
  rv = ns->GetItemType(&itemType);
  NS_ENSURE_SUCCESS(rv, rv);

  LOG(("nsOfflineCacheDevice::AddNamespace [cid=%s, ns=%s, data=%s, type=%d]",
       clientID.get(), namespaceSpec.get(), data.get(), itemType));

  AutoResetStatement statement(mStatement_InsertNamespaceEntry);

  rv = statement->BindUTF8StringByIndex(0, clientID);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindUTF8StringByIndex(1, namespaceSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindUTF8StringByIndex(2, data);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindInt32ByIndex(3, itemType);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsOfflineCacheDevice::GetUsage(const nsACString &clientID,
                               uint32_t *usage)
{
  LOG(("nsOfflineCacheDevice::GetUsage [cid=%s]\n",
       PromiseFlatCString(clientID).get()));

  *usage = 0;

  AutoResetStatement statement(mStatement_ApplicationCacheSize);

  nsresult rv = statement->BindUTF8StringByIndex(0, clientID);
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasRows;
  rv = statement->ExecuteStep(&hasRows);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!hasRows)
    return NS_OK;

  *usage = static_cast<uint32_t>(statement->AsInt32(0));

  return NS_OK;
}

nsresult
nsOfflineCacheDevice::GetGroups(uint32_t *count,
                                 char ***keys)
{
  LOG(("nsOfflineCacheDevice::GetGroups"));

  return RunSimpleQuery(mStatement_EnumerateGroups, 0, count, keys);
}

nsresult
nsOfflineCacheDevice::GetGroupsTimeOrdered(uint32_t *count,
					   char ***keys)
{
  LOG(("nsOfflineCacheDevice::GetGroupsTimeOrder"));

  return RunSimpleQuery(mStatement_EnumerateGroupsTimeOrder, 0, count, keys);
}

bool
nsOfflineCacheDevice::IsLocked(const nsACString &key)
{
  MutexAutoLock lock(mLock);
  return mLockedEntries.GetEntry(key);
}

void
nsOfflineCacheDevice::Lock(const nsACString &key)
{
  MutexAutoLock lock(mLock);
  mLockedEntries.PutEntry(key);
}

void
nsOfflineCacheDevice::Unlock(const nsACString &key)
{
  MutexAutoLock lock(mLock);
  mLockedEntries.RemoveEntry(key);
}

nsresult
nsOfflineCacheDevice::RunSimpleQuery(mozIStorageStatement * statement,
                                     uint32_t resultIndex,
                                     uint32_t * count,
                                     char *** values)
{
  bool hasRows;
  nsresult rv = statement->ExecuteStep(&hasRows);
  NS_ENSURE_SUCCESS(rv, rv);

  nsTArray<nsCString> valArray;
  while (hasRows)
  {
    uint32_t length;
    valArray.AppendElement(
      nsDependentCString(statement->AsSharedUTF8String(resultIndex, &length)));

    rv = statement->ExecuteStep(&hasRows);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  *count = valArray.Length();
  char **ret = static_cast<char **>(moz_xmalloc(*count * sizeof(char*)));
  if (!ret) return NS_ERROR_OUT_OF_MEMORY;

  for (uint32_t i = 0; i <  *count; i++) {
    ret[i] = NS_strdup(valArray[i].get());
    if (!ret[i]) {
      NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(i, ret);
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  *values = ret;

  return NS_OK;
}

nsresult
nsOfflineCacheDevice::CreateApplicationCache(const nsACString &group,
                                             nsIApplicationCache **out)
{
  *out = nullptr;

  nsCString clientID;
  // Some characters are special in the clientID.  Escape the groupID
  // before putting it in to the client key.
  if (!NS_Escape(nsCString(group), clientID, url_Path)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  PRTime now = PR_Now();

  // Include the timestamp to guarantee uniqueness across runs, and
  // the gNextTemporaryClientID for uniqueness within a second.
  clientID.Append(nsPrintfCString("|%016lld|%d",
                                  now / PR_USEC_PER_SEC,
                                  gNextTemporaryClientID++));

  nsCOMPtr<nsIApplicationCache> cache = new nsApplicationCache(this,
                                                               group,
                                                               clientID);
  if (!cache)
    return NS_ERROR_OUT_OF_MEMORY;

  nsCOMPtr<nsIWeakReference> weak = do_GetWeakReference(cache);
  if (!weak)
    return NS_ERROR_OUT_OF_MEMORY;

  MutexAutoLock lock(mLock);
  mCaches.Put(clientID, weak);

  cache.swap(*out);

  return NS_OK;
}

nsresult
nsOfflineCacheDevice::GetApplicationCache(const nsACString &clientID,
                                          nsIApplicationCache **out)
{
  MutexAutoLock lock(mLock);
  return GetApplicationCache_Unlocked(clientID, out);
}

nsresult
nsOfflineCacheDevice::GetApplicationCache_Unlocked(const nsACString &clientID,
                                                   nsIApplicationCache **out)
{
  *out = nullptr;

  nsCOMPtr<nsIApplicationCache> cache;

  nsWeakPtr weak;
  if (mCaches.Get(clientID, getter_AddRefs(weak)))
    cache = do_QueryReferent(weak);

  if (!cache)
  {
    nsCString group;
    nsresult rv = GetGroupForCache(clientID, group);
    NS_ENSURE_SUCCESS(rv, rv);

    if (group.IsEmpty()) {
      return NS_OK;
    }

    cache = new nsApplicationCache(this, group, clientID);
    weak = do_GetWeakReference(cache);
    if (!weak)
      return NS_ERROR_OUT_OF_MEMORY;

    mCaches.Put(clientID, weak);
  }

  cache.swap(*out);

  return NS_OK;
}

nsresult
nsOfflineCacheDevice::GetActiveCache(const nsACString &group,
                                     nsIApplicationCache **out)
{
  *out = nullptr;

  MutexAutoLock lock(mLock);

  nsCString *clientID;
  if (mActiveCachesByGroup.Get(group, &clientID))
    return GetApplicationCache_Unlocked(*clientID, out);

  return NS_OK;
}

nsresult
nsOfflineCacheDevice::DeactivateGroup(const nsACString &group)
{
  nsCString *active = nullptr;

  AutoResetStatement statement(mStatement_DeactivateGroup);
  nsresult rv = statement->BindUTF8StringByIndex(0, group);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  MutexAutoLock lock(mLock);

  if (mActiveCachesByGroup.Get(group, &active))
  {
    mActiveCaches.RemoveEntry(*active);
    mActiveCachesByGroup.Remove(group);
    active = nullptr;
  }

  return NS_OK;
}

nsresult
nsOfflineCacheDevice::DiscardByAppId(int32_t appID, bool browserEntriesOnly)
{
  nsresult rv;

  nsAutoCString jaridsuffix;

  jaridsuffix.Append('%');

  // TODO - this method should accept NeckoOriginAttributes* from outside instead.
  // If passed null, we should then delegate to
  // nsCacheService::GlobalInstance()->EvictEntriesInternal(nsICache::STORE_OFFLINE);

  NeckoOriginAttributes oa;
  oa.mAppId = appID;
  oa.mInBrowser = browserEntriesOnly;
  AppendJARIdentifier(jaridsuffix, &oa);

  {
    AutoResetStatement statement(mStatement_EnumerateApps);
    rv = statement->BindUTF8StringByIndex(0, jaridsuffix);
    NS_ENSURE_SUCCESS(rv, rv);

    bool hasRows;
    rv = statement->ExecuteStep(&hasRows);
    NS_ENSURE_SUCCESS(rv, rv);

    while (hasRows) {
      nsAutoCString group;
      rv = statement->GetUTF8String(0, group);
      NS_ENSURE_SUCCESS(rv, rv);

      nsCString clientID;
      rv = statement->GetUTF8String(1, clientID);
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsIRunnable> ev =
        new nsOfflineCacheDiscardCache(this, group, clientID);

      rv = nsCacheService::DispatchToCacheIOThread(ev);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = statement->ExecuteStep(&hasRows);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  if (!browserEntriesOnly) {
    // If deleting app, delete any 'inBrowserElement' entries too
    rv = DiscardByAppId(appID, true);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

bool
nsOfflineCacheDevice::CanUseCache(nsIURI *keyURI,
                                  const nsACString &clientID,
                                  nsILoadContextInfo *loadContextInfo)
{
  {
    MutexAutoLock lock(mLock);
    if (!mActiveCaches.Contains(clientID))
      return false;
  }

  nsAutoCString groupID;
  nsresult rv = GetGroupForCache(clientID, groupID);
  NS_ENSURE_SUCCESS(rv, false);

  nsCOMPtr<nsIURI> groupURI;
  rv = NS_NewURI(getter_AddRefs(groupURI), groupID);
  if (NS_FAILED(rv)) {
    return false;
  }

  // When we are choosing an initial cache to load the top
  // level document from, the URL of that document must have
  // the same origin as the manifest, according to the spec.
  // The following check is here because explicit, fallback
  // and dynamic entries might have origin different from the
  // manifest origin.
  if (!NS_SecurityCompareURIs(keyURI, groupURI,
                              GetStrictFileOriginPolicy())) {
    return false;
  }

  // Check the groupID we found is equal to groupID based
  // on the load context demanding load from app cache.
  // This is check of extended origin.
  nsAutoCString demandedGroupID;

  const NeckoOriginAttributes *oa = loadContextInfo
    ? loadContextInfo->OriginAttributesPtr()
    : nullptr;
  rv = BuildApplicationCacheGroupID(groupURI, oa, demandedGroupID);
  NS_ENSURE_SUCCESS(rv, false);

  if (groupID != demandedGroupID) {
    return false;
  }

  return true;
}


nsresult
nsOfflineCacheDevice::ChooseApplicationCache(const nsACString &key,
                                             nsILoadContextInfo *loadContextInfo,
                                             nsIApplicationCache **out)
{
  *out = nullptr;

  nsCOMPtr<nsIURI> keyURI;
  nsresult rv = NS_NewURI(getter_AddRefs(keyURI), key);
  NS_ENSURE_SUCCESS(rv, rv);

  // First try to find a matching cache entry.
  AutoResetStatement statement(mStatement_FindClient);
  rv = statement->BindUTF8StringByIndex(0, key);
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasRows;
  rv = statement->ExecuteStep(&hasRows);
  NS_ENSURE_SUCCESS(rv, rv);

  while (hasRows) {
    int32_t itemType;
    rv = statement->GetInt32(1, &itemType);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!(itemType & nsIApplicationCache::ITEM_FOREIGN)) {
      nsAutoCString clientID;
      rv = statement->GetUTF8String(0, clientID);
      NS_ENSURE_SUCCESS(rv, rv);

      if (CanUseCache(keyURI, clientID, loadContextInfo)) {
        return GetApplicationCache(clientID, out);
      }
    }

    rv = statement->ExecuteStep(&hasRows);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // OK, we didn't find an exact match.  Search for a client with a
  // matching namespace.

  AutoResetStatement nsstatement(mStatement_FindClientByNamespace);

  rv = nsstatement->BindUTF8StringByIndex(0, key);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = nsstatement->ExecuteStep(&hasRows);
  NS_ENSURE_SUCCESS(rv, rv);

  while (hasRows)
  {
    int32_t itemType;
    rv = nsstatement->GetInt32(1, &itemType);
    NS_ENSURE_SUCCESS(rv, rv);

    // Don't associate with a cache based solely on a whitelist entry
    if (!(itemType & nsIApplicationCacheNamespace::NAMESPACE_BYPASS)) {
      nsAutoCString clientID;
      rv = nsstatement->GetUTF8String(0, clientID);
      NS_ENSURE_SUCCESS(rv, rv);

      if (CanUseCache(keyURI, clientID, loadContextInfo)) {
        return GetApplicationCache(clientID, out);
      }
    }

    rv = nsstatement->ExecuteStep(&hasRows);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
nsOfflineCacheDevice::CacheOpportunistically(nsIApplicationCache* cache,
                                             const nsACString &key)
{
  NS_ENSURE_ARG_POINTER(cache);

  nsresult rv;

  nsAutoCString clientID;
  rv = cache->GetClientID(clientID);
  NS_ENSURE_SUCCESS(rv, rv);

  return CacheOpportunistically(clientID, key);
}

nsresult
nsOfflineCacheDevice::ActivateCache(const nsCSubstring &group,
                                    const nsCSubstring &clientID)
{
  AutoResetStatement statement(mStatement_ActivateClient);
  nsresult rv = statement->BindUTF8StringByIndex(0, group);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindUTF8StringByIndex(1, clientID);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt32ByIndex(2, SecondsFromPRTime(PR_Now()));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  MutexAutoLock lock(mLock);

  nsCString *active;
  if (mActiveCachesByGroup.Get(group, &active))
  {
    mActiveCaches.RemoveEntry(*active);
    mActiveCachesByGroup.Remove(group);
    active = nullptr;
  }

  if (!clientID.IsEmpty())
  {
    mActiveCaches.PutEntry(clientID);
    mActiveCachesByGroup.Put(group, new nsCString(clientID));
  }

  return NS_OK;
}

bool
nsOfflineCacheDevice::IsActiveCache(const nsCSubstring &group,
                                    const nsCSubstring &clientID)
{
  nsCString *active = nullptr;
  MutexAutoLock lock(mLock);
  return mActiveCachesByGroup.Get(group, &active) && *active == clientID;
}

/**
 * Preference accessors
 */

void
nsOfflineCacheDevice::SetCacheParentDirectory(nsIFile *parentDir)
{
  if (Initialized())
  {
    NS_ERROR("cannot switch cache directory once initialized");
    return;
  }

  if (!parentDir)
  {
    mCacheDirectory = nullptr;
    return;
  }

  // ensure parent directory exists
  nsresult rv = EnsureDir(parentDir);
  if (NS_FAILED(rv))
  {
    NS_WARNING("unable to create parent directory");
    return;
  }

  mBaseDirectory = parentDir;

  // cache dir may not exist, but that's ok
  nsCOMPtr<nsIFile> dir;
  rv = parentDir->Clone(getter_AddRefs(dir));
  if (NS_FAILED(rv))
    return;
  rv = dir->AppendNative(NS_LITERAL_CSTRING("OfflineCache"));
  if (NS_FAILED(rv))
    return;

  mCacheDirectory = do_QueryInterface(dir);
}

void
nsOfflineCacheDevice::SetCapacity(uint32_t capacity)
{
  mCacheCapacity = capacity * 1024;
}

bool
nsOfflineCacheDevice::AutoShutdown(nsIApplicationCache * aAppCache)
{
  if (!mAutoShutdown)
    return false;

  mAutoShutdown = false;

  Shutdown();

  RefPtr<nsCacheService> cacheService = nsCacheService::GlobalInstance();
  cacheService->RemoveCustomOfflineDevice(this);

  nsAutoCString clientID;
  aAppCache->GetClientID(clientID);

  MutexAutoLock lock(mLock);
  mCaches.Remove(clientID);

  return true;
}
