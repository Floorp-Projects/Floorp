/* vim:set ts=2 sw=2 sts=2 et cin: */
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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by IBM Corporation are Copyright (C) 2004
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net>
 *   Dave Camp <dcamp@mozilla.com>
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

#include "nsCache.h"
#include "nsDiskCache.h"
#include "nsDiskCacheDeviceSQL.h"
#include "nsCacheService.h"

#include "nsNetUtil.h"
#include "nsAutoPtr.h"
#include "nsString.h"
#include "nsPrintfCString.h"
#include "nsCRT.h"
#include "nsIVariant.h"

#include "mozIStorageService.h"
#include "mozIStorageStatement.h"
#include "mozIStorageFunction.h"
#include "mozStorageHelper.h"

#include "nsICacheVisitor.h"
#include "nsISeekableStream.h"

static const char OFFLINE_CACHE_DEVICE_ID[] = { "offline" };

#define LOG(args) CACHE_LOG_DEBUG(args)

static PRUint32 gNextTemporaryClientID = 0;

/*****************************************************************************
 * helpers
 */

static nsresult
EnsureDir(nsIFile *dir)
{
  PRBool exists;
  nsresult rv = dir->Exists(&exists);
  if (NS_SUCCEEDED(rv) && !exists)
    rv = dir->Create(nsIFile::DIRECTORY_TYPE, 0700);
  return rv;
}

static PRBool
DecomposeCacheEntryKey(const nsCString *fullKey,
                       const char **cid,
                       const char **key,
                       nsCString &buf)
{
  buf = *fullKey;

  PRInt32 colon = buf.FindChar(':');
  if (colon == kNotFound)
  {
    NS_ERROR("Invalid key");
    return PR_FALSE;
  }
  buf.SetCharAt('\0', colon);

  *cid = buf.get();
  *key = buf.get() + colon + 1;

  return PR_TRUE;
}

class AutoResetStatement
{
  public:
    AutoResetStatement(mozIStorageStatement *s)
      : mStatement(s) {}
    ~AutoResetStatement() { mStatement->Reset(); }
    mozIStorageStatement *operator->() { return mStatement; }
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
          NS_LITERAL_CSTRING("CREATE TEMP TRIGGER cache_on_delete AFTER DELETE"
                             " ON moz_cache FOR EACH ROW BEGIN SELECT"
                             " cache_eviction_observer("
                             "  OLD.key, OLD.generation);"
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
    nsRefPtr<nsOfflineCacheEvictionFunction> mEvictionFunction;
};

#define DCACHE_HASH_MAX  LL_MAXINT
#define DCACHE_HASH_BITS 64

/**
 *  nsOfflineCache::Hash(const char * key)
 *
 *  This algorithm of this method implies nsOfflineCacheRecords will be stored
 *  in a certain order on disk.  If the algorithm changes, existing cache
 *  map files may become invalid, and therefore the kCurrentVersion needs
 *  to be revised.
 */
static PRUint64
DCacheHash(const char * key)
{
  PRUint64 h = 0;
  for (const PRUint8* s = (PRUint8*) key; *s != '\0'; ++s)
    h = (h >> (DCACHE_HASH_BITS - 4)) ^ (h << 4) ^ *s;
  return (h == 0 ? DCACHE_HASH_MAX : h);
}

/******************************************************************************
 * nsOfflineCacheEvictionFunction
 */

NS_IMPL_ISUPPORTS1(nsOfflineCacheEvictionFunction, mozIStorageFunction)

// helper function for directly exposing the same data file binding
// path algorithm used in nsOfflineCacheBinding::Create
static nsresult
GetCacheDataFile(nsIFile *cacheDir, const char *key,
                 int generation, nsCOMPtr<nsIFile> &file)
{
  cacheDir->Clone(getter_AddRefs(file));
  if (!file)
    return NS_ERROR_OUT_OF_MEMORY;

  PRUint64 hash = DCacheHash(key);

  PRUint32 dir1 = (PRUint32) (hash & 0x0F);
  PRUint32 dir2 = (PRUint32)((hash & 0xF0) >> 4);

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

  *_retval = nsnull;

  PRUint32 numEntries;
  nsresult rv = values->GetNumEntries(&numEntries);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ASSERTION(numEntries == 2, "unexpected number of arguments");

  PRUint32 valueLen;
  const char *key = values->AsSharedUTF8String(0, &valueLen);
  int generation  = values->AsInt32(1);

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

  for (PRInt32 i = 0; i < mItems.Count(); i++) {
#if defined(PR_LOGGING)
    nsCAutoString path;
    mItems[i]->GetNativePath(path);
    LOG(("  removing %s\n", path.get()));
#endif

    mItems[i]->Remove(PR_FALSE);
  }

  Reset();
}

/******************************************************************************
 * nsOfflineCacheDeviceInfo
 */

class nsOfflineCacheDeviceInfo : public nsICacheDeviceInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICACHEDEVICEINFO

  nsOfflineCacheDeviceInfo(nsOfflineCacheDevice* device)
    : mDevice(device)
  {}

private:
  nsOfflineCacheDevice* mDevice;
};

NS_IMPL_ISUPPORTS1(nsOfflineCacheDeviceInfo, nsICacheDeviceInfo)

NS_IMETHODIMP
nsOfflineCacheDeviceInfo::GetDescription(char **aDescription)
{
  *aDescription = NS_strdup("Offline cache device");
  return *aDescription ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsOfflineCacheDeviceInfo::GetUsageReport(char ** usageReport)
{
  nsCAutoString buffer;
  buffer.AppendLiteral("\n<tr>\n<td><b>Cache Directory:</b></td>\n<td><tt> ");
  
  nsILocalFile *cacheDir = mDevice->CacheDirectory();
  if (!cacheDir)
    return NS_OK;

  nsAutoString path;
  nsresult rv = cacheDir->GetPath(path);
  if (NS_SUCCEEDED(rv))
    AppendUTF16toUTF8(path, buffer);
  else
    buffer.AppendLiteral("directory unavailable");
  buffer.AppendLiteral("</tt></td>\n</tr>\n");

  *usageReport = ToNewCString(buffer);
  if (!*usageReport)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

NS_IMETHODIMP
nsOfflineCacheDeviceInfo::GetEntryCount(PRUint32 *aEntryCount)
{
  *aEntryCount = mDevice->EntryCount();
  return NS_OK;
}

NS_IMETHODIMP
nsOfflineCacheDeviceInfo::GetTotalSize(PRUint32 *aTotalSize)
{
  *aTotalSize = mDevice->CacheSize();
  return NS_OK;
}

NS_IMETHODIMP
nsOfflineCacheDeviceInfo::GetMaximumSize(PRUint32 *aMaximumSize)
{
  *aMaximumSize = mDevice->CacheCapacity();
  return NS_OK;
}

/******************************************************************************
 * nsOfflineCacheBinding
 */

class nsOfflineCacheBinding : public nsISupports
{
public:
  NS_DECL_ISUPPORTS

  static nsOfflineCacheBinding *
      Create(nsIFile *cacheDir, const nsCString *key, int generation);

  nsCOMPtr<nsIFile> mDataFile;
  int               mGeneration;
};

NS_IMPL_THREADSAFE_ISUPPORTS0(nsOfflineCacheBinding)

nsOfflineCacheBinding *
nsOfflineCacheBinding::Create(nsIFile *cacheDir,
                              const nsCString *fullKey,
                              int generation)
{
  nsCOMPtr<nsIFile> file;
  cacheDir->Clone(getter_AddRefs(file));
  if (!file)
    return nsnull;

  nsCAutoString keyBuf;
  const char *cid, *key;
  if (!DecomposeCacheEntryKey(fullKey, &cid, &key, keyBuf))
    return nsnull;

  PRUint64 hash = DCacheHash(key);

  PRUint32 dir1 = (PRUint32) (hash & 0x0F);
  PRUint32 dir2 = (PRUint32)((hash & 0xF0) >> 4);

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
      PR_snprintf(leaf, sizeof(leaf), "%014llX-%X", hash, generation);

      rv = file->SetNativeLeafName(nsDependentCString(leaf));
      if (NS_FAILED(rv))
        return nsnull;
      rv = file->Create(nsIFile::NORMAL_FILE_TYPE, 00600);
      if (NS_FAILED(rv) && rv != NS_ERROR_FILE_ALREADY_EXISTS)
        return nsnull;
      if (NS_SUCCEEDED(rv))
        break;
    }
  }
  else
  {
    PR_snprintf(leaf, sizeof(leaf), "%014llX-%X", hash, generation);
    rv = file->AppendNative(nsDependentCString(leaf));
    if (NS_FAILED(rv))
      return nsnull;
  }

  nsOfflineCacheBinding *binding = new nsOfflineCacheBinding;
  if (!binding)
    return nsnull;

  binding->mDataFile.swap(file);
  binding->mGeneration = generation;
  return binding;
}

/******************************************************************************
 * nsOfflineCacheRecord
 */

struct nsOfflineCacheRecord
{
  const char    *clientID;
  const char    *key;
  const PRUint8 *metaData;
  PRUint32       metaDataLen;
  PRInt32        generation;
  PRInt32        flags;
  PRInt32        dataSize;
  PRInt32        fetchCount;
  PRInt64        lastFetched;
  PRInt64        lastModified;
  PRInt64        expirationTime;
};

static nsCacheEntry *
CreateCacheEntry(nsOfflineCacheDevice *device,
                 const nsCString *fullKey,
                 const nsOfflineCacheRecord &rec)
{
  if (rec.flags != 0)
  {
    LOG(("refusing to load busy entry\n"));
    return nsnull;
  }

  nsCacheEntry *entry;
  
  nsresult rv = nsCacheEntry::Create(fullKey->get(), // XXX enable sharing
                                     nsICache::STREAM_BASED,
                                     nsICache::STORE_OFFLINE,
                                     device, &entry);
  if (NS_FAILED(rv))
    return nsnull;

  entry->SetFetchCount((PRUint32) rec.fetchCount);
  entry->SetLastFetched(SecondsFromPRTime(rec.lastFetched));
  entry->SetLastModified(SecondsFromPRTime(rec.lastModified));
  entry->SetExpirationTime(SecondsFromPRTime(rec.expirationTime));
  entry->SetDataSize((PRUint32) rec.dataSize);

  entry->UnflattenMetaData((const char *) rec.metaData, rec.metaDataLen);

  // create a binding object for this entry
  nsOfflineCacheBinding *binding =
      nsOfflineCacheBinding::Create(device->CacheDirectory(),
                                    fullKey,
                                    rec.generation);
  if (!binding)
  {
    delete entry;
    return nsnull;
  }
  entry->SetData(binding);

  return entry;
}


/******************************************************************************
 * nsOfflineCacheEntryInfo
 */

class nsOfflineCacheEntryInfo : public nsICacheEntryInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICACHEENTRYINFO

  nsOfflineCacheRecord *mRec;
};

NS_IMPL_ISUPPORTS1(nsOfflineCacheEntryInfo, nsICacheEntryInfo)

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
nsOfflineCacheEntryInfo::GetFetchCount(PRInt32 *aFetchCount)
{
  *aFetchCount = mRec->fetchCount;
  return NS_OK;
}

NS_IMETHODIMP
nsOfflineCacheEntryInfo::GetLastFetched(PRUint32 *aLastFetched)
{
  *aLastFetched = SecondsFromPRTime(mRec->lastFetched);
  return NS_OK;
}

NS_IMETHODIMP
nsOfflineCacheEntryInfo::GetLastModified(PRUint32 *aLastModified)
{
  *aLastModified = SecondsFromPRTime(mRec->lastModified);
  return NS_OK;
}

NS_IMETHODIMP
nsOfflineCacheEntryInfo::GetExpirationTime(PRUint32 *aExpirationTime)
{
  *aExpirationTime = SecondsFromPRTime(mRec->expirationTime);
  return NS_OK;
}

NS_IMETHODIMP
nsOfflineCacheEntryInfo::IsStreamBased(PRBool *aStreamBased)
{
  *aStreamBased = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsOfflineCacheEntryInfo::GetDataSize(PRUint32 *aDataSize)
{
  *aDataSize = mRec->dataSize;
  return NS_OK;
}


/******************************************************************************
 * nsOfflineCacheDevice
 */

nsOfflineCacheDevice::nsOfflineCacheDevice()
  : mDB(nsnull)
  , mCacheCapacity(0)
  , mDeltaCounter(0)
{
}

nsOfflineCacheDevice::~nsOfflineCacheDevice()
{
  Shutdown();
}

PRUint32
nsOfflineCacheDevice::CacheSize()
{
  AutoResetStatement statement(mStatement_CacheSize);

  PRBool hasRows;
  nsresult rv = statement->ExecuteStep(&hasRows);
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && hasRows, 0);
  
  return (PRUint32) statement->AsInt32(0);
}

PRUint32
nsOfflineCacheDevice::EntryCount()
{
  AutoResetStatement statement(mStatement_EntryCount);

  PRBool hasRows;
  nsresult rv = statement->ExecuteStep(&hasRows);
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && hasRows, 0);

  return (PRUint32) statement->AsInt32(0);
}

nsresult
nsOfflineCacheDevice::UpdateEntry(nsCacheEntry *entry)
{
  // Decompose the key into "ClientID" and "Key"
  nsCAutoString keyBuf;
  const char *cid, *key;
  if (!DecomposeCacheEntryKey(entry->Key(), &cid, &key, keyBuf))
    return NS_ERROR_UNEXPECTED;

  nsCString metaDataBuf;
  PRUint32 mdSize = entry->MetaDataSize();
  if (!EnsureStringLength(metaDataBuf, mdSize))
    return NS_ERROR_OUT_OF_MEMORY;
  char *md = metaDataBuf.BeginWriting();
  entry->FlattenMetaData(md, mdSize);

  nsOfflineCacheRecord rec;
  rec.metaData = (const PRUint8 *) md;
  rec.metaDataLen = mdSize;
  rec.flags = 0;  // mark entry as inactive
  rec.dataSize = entry->DataSize();
  rec.fetchCount = entry->FetchCount();
  rec.lastFetched = PRTimeFromSeconds(entry->LastFetched());
  rec.lastModified = PRTimeFromSeconds(entry->LastModified());
  rec.expirationTime = PRTimeFromSeconds(entry->ExpirationTime());

  AutoResetStatement statement(mStatement_UpdateEntry);

  nsresult rv;
  rv  = statement->BindBlobParameter(0, rec.metaData, rec.metaDataLen);
  rv |= statement->BindInt32Parameter(1, rec.flags);
  rv |= statement->BindInt32Parameter(2, rec.dataSize);
  rv |= statement->BindInt32Parameter(3, rec.fetchCount);
  rv |= statement->BindInt64Parameter(4, rec.lastFetched);
  rv |= statement->BindInt64Parameter(5, rec.lastModified);
  rv |= statement->BindInt64Parameter(6, rec.expirationTime);
  rv |= statement->BindUTF8StringParameter(7, nsDependentCString(cid));
  rv |= statement->BindUTF8StringParameter(8, nsDependentCString(key));
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasRows;
  rv = statement->ExecuteStep(&hasRows);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ASSERTION(!hasRows, "UPDATE should not result in output");
  return rv;
}

nsresult
nsOfflineCacheDevice::UpdateEntrySize(nsCacheEntry *entry, PRUint32 newSize)
{
  // Decompose the key into "ClientID" and "Key"
  nsCAutoString keyBuf;
  const char *cid, *key;
  if (!DecomposeCacheEntryKey(entry->Key(), &cid, &key, keyBuf))
    return NS_ERROR_UNEXPECTED;

  AutoResetStatement statement(mStatement_UpdateEntrySize);

  nsresult rv;
  rv  = statement->BindInt32Parameter(0, newSize);
  rv |= statement->BindUTF8StringParameter(1, nsDependentCString(cid));
  rv |= statement->BindUTF8StringParameter(2, nsDependentCString(key));
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasRows;
  rv = statement->ExecuteStep(&hasRows);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ASSERTION(!hasRows, "UPDATE should not result in output");
  return rv;
}

nsresult
nsOfflineCacheDevice::DeleteEntry(nsCacheEntry *entry, PRBool deleteData)
{
  if (deleteData)
  {
    nsresult rv = DeleteData(entry);
    if (NS_FAILED(rv))
      return rv;
  }

  // Decompose the key into "ClientID" and "Key"
  nsCAutoString keyBuf;
  const char *cid, *key;
  if (!DecomposeCacheEntryKey(entry->Key(), &cid, &key, keyBuf))
    return NS_ERROR_UNEXPECTED;

  AutoResetStatement statement(mStatement_DeleteEntry);

  nsresult rv;
  rv  = statement->BindUTF8StringParameter(0, nsDependentCString(cid));
  rv |= statement->BindUTF8StringParameter(1, nsDependentCString(key));
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasRows;
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

  return binding->mDataFile->Remove(PR_FALSE);
}

/**
 * nsCacheDevice implementation
 */

nsresult
nsOfflineCacheDevice::Init()
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

  nsCOMPtr<mozIStorageService> ss =
      do_GetService("@mozilla.org/storage/service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ss->OpenDatabase(indexFile, getter_AddRefs(mDB));
  NS_ENSURE_SUCCESS(rv, rv);

  mDB->ExecuteSimpleSQL(NS_LITERAL_CSTRING("PRAGMA synchronous = OFF;"));

  // XXX ... other initialization steps

  // XXX in the future we may wish to verify the schema for moz_cache
  //     perhaps using "PRAGMA table_info" ?

  // build the table
  //
  //  "Generation" is the data file generation number.
  //  "Flags" is a bit-field indicating the state of the entry.
  //
  mDB->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TABLE moz_cache (\n"
                         "  ClientID        TEXT,\n"
                         "  Key             TEXT,\n"
                         "  MetaData        BLOB,\n"
                         "  Generation      INTEGER,\n"
                         "  Flags           INTEGER,\n"
                         "  DataSize        INTEGER,\n"
                         "  FetchCount      INTEGER,\n"
                         "  LastFetched     INTEGER,\n"
                         "  LastModified    INTEGER,\n"
                         "  ExpirationTime  INTEGER\n"
                         ");\n"));
  // maybe the table already exists, so don't bother checking for errors.

  // build the ownership table
  mDB->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TABLE moz_cache_owners (\n"
                         " ClientID TEXT,\n"
                         " Domain TEXT,\n"
                         " URI TEXT,\n"
                         " Key TEXT\n"
                         ");\n"));
  // maybe the table already exists, so don't bother checking for errors.

  mDB->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE UNIQUE INDEX moz_cache_index"
                         " ON moz_cache (ClientID, Key);"));
  // maybe the index already exists, so don't bother checking for errors.

  mEvictionFunction = new nsOfflineCacheEvictionFunction(this);
  if (!mEvictionFunction) return NS_ERROR_OUT_OF_MEMORY;

  rv = mDB->CreateFunction(NS_LITERAL_CSTRING("cache_eviction_observer"), 2, mEvictionFunction);
  NS_ENSURE_SUCCESS(rv, rv);

  // create all (most) of our statements up front
  struct StatementSql {
    nsCOMPtr<mozIStorageStatement> &statement;
    const char *sql;
    StatementSql (nsCOMPtr<mozIStorageStatement> &aStatement, const char *aSql):
      statement (aStatement), sql (aSql) {}
  } prepared[] = {
    StatementSql ( mStatement_CacheSize,         "SELECT Sum(DataSize) from moz_cache;" ),
    StatementSql ( mStatement_EntryCount,        "SELECT count(*) from moz_cache;" ),
    StatementSql ( mStatement_UpdateEntry,       "UPDATE moz_cache SET MetaData = ?, Flags = ?, DataSize = ?, FetchCount = ?, LastFetched = ?, LastModified = ?, ExpirationTime = ? WHERE ClientID = ? AND Key = ?;" ),
    StatementSql ( mStatement_UpdateEntrySize,   "UPDATE moz_cache SET DataSize = ? WHERE ClientID = ? AND Key = ?;" ),
    StatementSql ( mStatement_UpdateEntryFlags,  "UPDATE moz_cache SET Flags = ? WHERE ClientID = ? AND Key = ?;" ),
    StatementSql ( mStatement_DeleteEntry,       "DELETE FROM moz_cache WHERE ClientID = ? AND Key = ?;" ),
    StatementSql ( mStatement_FindEntry,         "SELECT MetaData, Generation, Flags, DataSize, FetchCount, LastFetched, LastModified, ExpirationTime FROM moz_cache WHERE ClientID = ? AND Key = ?;" ),
    StatementSql ( mStatement_BindEntry,         "INSERT INTO moz_cache (ClientID, Key, MetaData, Generation, Flags, DataSize, FetchCount, LastFetched, LastModified, ExpirationTime) VALUES(?,?,?,?,?,?,?,?,?,?);" ),
    StatementSql ( mStatement_ClearOwnership,    "DELETE FROM moz_cache_owners WHERE ClientId = ? AND Domain = ? AND URI = ?;" ),
    StatementSql ( mStatement_RemoveOwnership,   "DELETE FROM moz_cache_owners WHERE ClientID = ? AND Domain = ? AND URI = ? AND Key = ?;" ),
    StatementSql ( mStatement_ClearDomain,       "DELETE FROM moz_cache_owners WHERE ClientID = ? AND Domain = ?;" ),
    StatementSql ( mStatement_AddOwnership,      "INSERT INTO moz_cache_owners (ClientID, Domain, URI, Key) VALUES (?, ?, ?, ?);" ),
    StatementSql ( mStatement_CheckOwnership,    "SELECT Key From moz_cache_owners WHERE ClientID = ? AND Domain = ? AND URI = ? AND Key = ?;" ),
    StatementSql ( mStatement_ListOwned,         "SELECT Key FROM moz_cache_owners WHERE ClientID = ? AND Domain = ? AND URI = ?;" ),
    StatementSql ( mStatement_ListOwners,        "SELECT DISTINCT Domain, URI FROM moz_cache_owners WHERE ClientID = ?;"),
    StatementSql ( mStatement_ListOwnerDomains,  "SELECT DISTINCT Domain FROM moz_cache_owners WHERE ClientID = ?;"),
    StatementSql ( mStatement_ListOwnerURIs,     "SELECT DISTINCT URI FROM moz_cache_owners WHERE ClientID = ? AND Domain = ?;"),
    StatementSql ( mStatement_DeleteConflicts,   "DELETE FROM moz_cache WHERE rowid IN (SELECT old.rowid FROM moz_cache AS old, moz_cache AS new WHERE old.Key = new.Key AND old.ClientID = ? AND new.ClientID = ?)"),
    StatementSql ( mStatement_DeleteUnowned,     "DELETE FROM moz_cache WHERE rowid IN (SELECT moz_cache.rowid FROM moz_cache LEFT OUTER JOIN moz_cache_owners ON (moz_cache.ClientID = moz_cache_owners.ClientID AND moz_cache.Key = moz_cache_owners.Key) WHERE moz_cache.ClientID = ? AND moz_cache_owners.Domain ISNULL);" ),
    StatementSql ( mStatement_SwapClientID,       "UPDATE OR REPLACE moz_cache SET ClientID = ? WHERE ClientID = ?;")
  };
  for (PRUint32 i=0; i<NS_ARRAY_LENGTH(prepared); ++i)
  {
    rv |= mDB->CreateStatement(nsDependentCString(prepared[i].sql),
                               getter_AddRefs(prepared[i].statement));
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // Clear up any dangling active flags
  rv = mDB->ExecuteSimpleSQL(
         NS_LITERAL_CSTRING("UPDATE moz_cache"
                            " SET Flags=(Flags & ~1)"
                            " WHERE (Flags & 1);"));
  NS_ENSURE_SUCCESS(rv, rv);

  // Clear up dangling temporary sessions
  EvictionObserver evictionObserver(mDB, mEvictionFunction);

  rv = mDB->ExecuteSimpleSQL(
    NS_LITERAL_CSTRING("DELETE FROM moz_cache"
                       " WHERE (ClientID GLOB \"TempClient*\")"));
  NS_ENSURE_SUCCESS(rv, rv);

  evictionObserver.Apply();

  return NS_OK;
}

nsresult
nsOfflineCacheDevice::Shutdown()
{
  NS_ENSURE_TRUE(mDB, NS_ERROR_NOT_INITIALIZED);

  mDB = 0;
  mEvictionFunction = 0;

  return NS_OK;
}

const char *
nsOfflineCacheDevice::GetDeviceID()
{
  return OFFLINE_CACHE_DEVICE_ID;
}

nsCacheEntry *
nsOfflineCacheDevice::FindEntry(nsCString *fullKey, PRBool *collision)
{
  LOG(("nsOfflineCacheDevice::FindEntry [key=%s]\n", fullKey->get()));

  // SELECT * FROM moz_cache WHERE key = ?

  // Decompose the key into "ClientID" and "Key"
  nsCAutoString keyBuf;
  const char *cid, *key;
  if (!DecomposeCacheEntryKey(fullKey, &cid, &key, keyBuf))
    return nsnull;

  AutoResetStatement statement(mStatement_FindEntry);

  nsresult rv;
  rv  = statement->BindUTF8StringParameter(0, nsDependentCString(cid));
  rv |= statement->BindUTF8StringParameter(1, nsDependentCString(key));
  NS_ENSURE_SUCCESS(rv, nsnull);

  PRBool hasRows;
  rv = statement->ExecuteStep(&hasRows);
  if (NS_FAILED(rv) || !hasRows)
    return nsnull; // entry not found

  nsOfflineCacheRecord rec;
  statement->GetSharedBlob(0, &rec.metaDataLen,
                           (const PRUint8 **) &rec.metaData);
  rec.generation     = statement->AsInt32(1);
  rec.flags          = statement->AsInt32(2);
  rec.dataSize       = statement->AsInt32(3);
  rec.fetchCount     = statement->AsInt32(4);
  rec.lastFetched    = statement->AsInt64(5);
  rec.lastModified   = statement->AsInt64(6);
  rec.expirationTime = statement->AsInt64(7);

  LOG(("entry: [%u %d %d %d %d %lld %lld %lld]\n",
        rec.metaDataLen,
        rec.generation,
        rec.flags,
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
    PRBool isFile;
    rv = binding->mDataFile->IsFile(&isFile);
    if (NS_FAILED(rv) || !isFile)
    {
      DeleteEntry(entry, PR_FALSE);
      delete entry;
      return nsnull;
    }

    statement->Reset();

    // mark as active
    AutoResetStatement updateStatement(mStatement_UpdateEntryFlags);
    rec.flags |= 0x1;
    rv |= updateStatement->BindInt32Parameter(0, rec.flags);
    rv |= updateStatement->BindUTF8StringParameter(1, nsDependentCString(cid));
    rv |= updateStatement->BindUTF8StringParameter(2, nsDependentCString(key));
    if (NS_FAILED(rv))
    {
      delete entry;
      return nsnull;
    }

    rv = updateStatement->ExecuteStep(&hasRows);
    if (NS_FAILED(rv))
    {
      delete entry;
      return nsnull;
    }

    NS_ASSERTION(!hasRows, "UPDATE should not result in output");
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
  else
  {
    // UPDATE the database row

    // XXX Assumption: the row already exists because it was either created
    // with a call to BindEntry or it was there when we called FindEntry.

    UpdateEntry(entry);
  }

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
  nsCAutoString keyBuf;
  const char *cid, *key;
  if (!DecomposeCacheEntryKey(entry->Key(), &cid, &key, keyBuf))
    return NS_ERROR_UNEXPECTED;

  // create binding, pick best generation number
  nsRefPtr<nsOfflineCacheBinding> binding =
      nsOfflineCacheBinding::Create(mCacheDirectory, entry->Key(), -1);
  if (!binding)
    return NS_ERROR_OUT_OF_MEMORY;

  nsOfflineCacheRecord rec;
  rec.clientID = cid;
  rec.key = key;
  rec.metaData = NULL; // don't write any metadata now.
  rec.metaDataLen = 0;
  rec.generation = binding->mGeneration;
  rec.flags = 0x1;  // mark entry as active, we'll reset this in DeactivateEntry
  rec.dataSize = 0;
  rec.fetchCount = entry->FetchCount();
  rec.lastFetched = PRTimeFromSeconds(entry->LastFetched());
  rec.lastModified = PRTimeFromSeconds(entry->LastModified());
  rec.expirationTime = PRTimeFromSeconds(entry->ExpirationTime());

  AutoResetStatement statement(mStatement_BindEntry);

  nsresult rv;
  rv  = statement->BindUTF8StringParameter(0, nsDependentCString(rec.clientID));
  rv |= statement->BindUTF8StringParameter(1, nsDependentCString(rec.key));
  rv |= statement->BindBlobParameter(2, rec.metaData, rec.metaDataLen);
  rv |= statement->BindInt32Parameter(3, rec.generation);
  rv |= statement->BindInt32Parameter(4, rec.flags);
  rv |= statement->BindInt32Parameter(5, rec.dataSize);
  rv |= statement->BindInt32Parameter(6, rec.fetchCount);
  rv |= statement->BindInt64Parameter(7, rec.lastFetched);
  rv |= statement->BindInt64Parameter(8, rec.lastModified);
  rv |= statement->BindInt64Parameter(9, rec.expirationTime);
  NS_ENSURE_SUCCESS(rv, rv);
  
  PRBool hasRows;
  rv = statement->ExecuteStep(&hasRows);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ASSERTION(!hasRows, "INSERT should not result in output");

  entry->SetData(binding);
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
  
  DeleteEntry(entry, PR_FALSE);
}

nsresult
nsOfflineCacheDevice::OpenInputStreamForEntry(nsCacheEntry      *entry,
                                              nsCacheAccessMode  mode,
                                              PRUint32           offset,
                                              nsIInputStream   **result)
{
  LOG(("nsOfflineCacheDevice::OpenInputStreamForEntry [key=%s]\n",
       entry->Key()->get()));

  *result = nsnull;

  NS_ENSURE_TRUE(offset < entry->DataSize(), NS_ERROR_INVALID_ARG);

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
                                               PRUint32            offset,
                                               nsIOutputStream   **result)
{
  LOG(("nsOfflineCacheDevice::OpenOutputStreamForEntry [key=%s]\n",
       entry->Key()->get()));

  *result = nsnull;

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
  NS_NewBufferedOutputStream(getter_AddRefs(bufferedOut), out, 16 * 1024);
  if (!bufferedOut)
    return NS_ERROR_UNEXPECTED;

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
nsOfflineCacheDevice::OnDataSizeChange(nsCacheEntry *entry, PRInt32 deltaSize)
{
  LOG(("nsOfflineCacheDevice::OnDataSizeChange [key=%s delta=%d]\n",
      entry->Key()->get(), deltaSize));

  const PRInt32 DELTA_THRESHOLD = 1<<14; // 16k

  // called to notify us of an impending change in the total size of the
  // specified entry.

  PRUint32 oldSize = entry->DataSize();
  NS_ASSERTION(deltaSize >= 0 || PRInt32(oldSize) + deltaSize >= 0, "oops");
  PRUint32 newSize = PRInt32(oldSize) + deltaSize;
  UpdateEntrySize(entry, newSize);

  mDeltaCounter += deltaSize; // this may go negative

  if (mDeltaCounter >= DELTA_THRESHOLD)
  {
    if (CacheSize() > mCacheCapacity) {
      // the entry will overrun the cache capacity, doom the entry
      // and abort
      nsresult rv = nsCacheService::DoomEntry(entry);
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

  PRBool keepGoing;
  nsresult rv = visitor->VisitDevice(OFFLINE_CACHE_DEVICE_ID, deviceInfo,
                                     &keepGoing);
  if (NS_FAILED(rv))
    return rv;
  
  if (!keepGoing)
    return NS_OK;

  // SELECT * from moz_cache;

  nsOfflineCacheRecord rec;
  nsRefPtr<nsOfflineCacheEntryInfo> info = new nsOfflineCacheEntryInfo;
  if (!info)
    return NS_ERROR_OUT_OF_MEMORY;
  info->mRec = &rec;

  // XXX may want to list columns explicitly
  nsCOMPtr<mozIStorageStatement> statement;
  rv = mDB->CreateStatement(
      NS_LITERAL_CSTRING("SELECT * FROM moz_cache;"),
      getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasRows;
  for (;;)
  {
    rv = statement->ExecuteStep(&hasRows);
    if (NS_FAILED(rv) || !hasRows)
      break;

    statement->GetSharedUTF8String(0, NULL, &rec.clientID);
    statement->GetSharedUTF8String(1, NULL, &rec.key);
    statement->GetSharedBlob(2, &rec.metaDataLen,
                             (const PRUint8 **) &rec.metaData);
    rec.generation     = statement->AsInt32(3);
    rec.flags          = statement->AsInt32(4);
    rec.dataSize       = statement->AsInt32(5);
    rec.fetchCount     = statement->AsInt32(6);
    rec.lastFetched    = statement->AsInt64(7);
    rec.lastModified   = statement->AsInt64(8);
    rec.expirationTime = statement->AsInt64(9);

    PRBool keepGoing;
    rv = visitor->VisitEntry(OFFLINE_CACHE_DEVICE_ID, info, &keepGoing);
    if (NS_FAILED(rv) || !keepGoing)
      break;
  }

  info->mRec = nsnull;
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

  const char *deleteCmd;
  if (clientID)
  {
    deleteCmd =
      PR_smprintf("DELETE FROM moz_cache WHERE ClientID=%q AND Flags=0;",
                  clientID);
    if (!deleteCmd)
      return NS_ERROR_OUT_OF_MEMORY;
  }
  else
  {
    deleteCmd = "DELETE FROM moz_cache WHERE Flags = 0;";
  }

  nsresult rv = mDB->ExecuteSimpleSQL(nsDependentCString(deleteCmd));
  if (clientID)
    PR_smprintf_free((char *) deleteCmd);
  NS_ENSURE_SUCCESS(rv, rv);

  evictionObserver.Apply();

  return NS_OK;
}

nsresult
nsOfflineCacheDevice::RunSimpleQuery(mozIStorageStatement * statement,
                                     PRUint32 resultIndex,
                                     PRUint32 * count,
                                     char *** values)
{
  PRBool hasRows;
  nsresult rv = statement->ExecuteStep(&hasRows);
  NS_ENSURE_SUCCESS(rv, rv);

  nsTArray<nsCString> valArray;
  while (hasRows)
  {
    PRUint32 length;
    valArray.AppendElement(
      nsDependentCString(statement->AsSharedUTF8String(resultIndex, &length)));

    rv = statement->ExecuteStep(&hasRows);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  *count = valArray.Length();
  char **ret = static_cast<char **>(NS_Alloc(*count * sizeof(char*)));
  if (!ret) return NS_ERROR_OUT_OF_MEMORY;

  for (PRUint32 i = 0; i <  *count; i++) {
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
nsOfflineCacheDevice::GetOwnerDomains(const char * clientID,
                                      PRUint32 * count,
                                      char *** domains)
{
  LOG(("nsOfflineCacheDevice::GetOwnerDomains [cid=%s]\n", clientID));

  AutoResetStatement statement(mStatement_ListOwnerDomains);
  nsresult rv = statement->BindUTF8StringParameter(
                                          0, nsDependentCString(clientID));
  NS_ENSURE_SUCCESS(rv, rv);

  return RunSimpleQuery(mStatement_ListOwnerDomains, 0, count, domains);
}

nsresult
nsOfflineCacheDevice::GetOwnerURIs(const char * clientID,
                                   const nsACString & ownerDomain,
                                   PRUint32 * count,
                                   char *** domains)
{
  LOG(("nsOfflineCacheDevice::GetOwnerURIs [cid=%s]\n", clientID));

  AutoResetStatement statement(mStatement_ListOwnerURIs);
  nsresult rv = statement->BindUTF8StringParameter(
                                          0, nsDependentCString(clientID));
  rv = statement->BindUTF8StringParameter(
                                          1, ownerDomain);
  NS_ENSURE_SUCCESS(rv, rv);

  return RunSimpleQuery(mStatement_ListOwnerURIs, 0, count, domains);
}

nsresult
nsOfflineCacheDevice::SetOwnedKeys(const char * clientID,
                                   const nsACString & ownerDomain,
                                   const nsACString & ownerURI,
                                   PRUint32 count,
                                   const char ** keys)
{
  LOG(("nsOfflineCacheDevice::SetOwnedKeys [cid=%s]\n", clientID));
  mozStorageTransaction transaction(mDB, PR_FALSE);

  nsDependentCString clientIDStr(clientID);

  AutoResetStatement clearStatement(mStatement_ClearOwnership);
  nsresult rv = clearStatement->BindUTF8StringParameter(
                                               0, clientIDStr);
  rv |= clearStatement->BindUTF8StringParameter(1, ownerDomain);
  rv |= clearStatement->BindUTF8StringParameter(2, ownerURI);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = clearStatement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRUint32 i = 0; i < count; i++)
  {
    AutoResetStatement insertStatement(mStatement_AddOwnership);
    rv = insertStatement->BindUTF8StringParameter(0, clientIDStr);
    rv |= insertStatement->BindUTF8StringParameter(1, ownerDomain);
    rv |= insertStatement->BindUTF8StringParameter(2, ownerURI);
    rv |= insertStatement->BindUTF8StringParameter(3, nsDependentCString(keys[i]));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = insertStatement->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return transaction.Commit();
}

nsresult
nsOfflineCacheDevice::GetOwnedKeys(const char * clientID,
                                   const nsACString & ownerDomain,
                                   const nsACString & ownerURI,
                                   PRUint32 * count,
                                   char *** keys)
{
  LOG(("nsOfflineCacheDevice::GetOwnedKeys [cid=%s]\n", clientID));

  AutoResetStatement statement(mStatement_ListOwned);
  nsresult rv = statement->BindUTF8StringParameter(
                                           0, nsDependentCString(clientID));
  rv |= statement->BindUTF8StringParameter(1, ownerDomain);
  rv |= statement->BindUTF8StringParameter(2, ownerURI);
  NS_ENSURE_SUCCESS(rv, rv);

  return RunSimpleQuery(mStatement_ListOwned, 0, count, keys);
}

nsresult
nsOfflineCacheDevice::AddOwnedKey(const char * clientID,
                                  const nsACString & ownerDomain,
                                  const nsACString & ownerURI,
                                  const nsACString & key)
{
  LOG(("nsOfflineCacheDevice::AddOwnedKey [cid=%s]\n", clientID));

  PRBool isOwned;
  nsresult rv = KeyIsOwned(clientID, ownerDomain, ownerURI, key, &isOwned);
  NS_ENSURE_SUCCESS(rv, rv);
  if (isOwned) return NS_OK;

  AutoResetStatement statement(mStatement_AddOwnership);
  rv = statement->BindUTF8StringParameter(0, nsDependentCString(clientID));
  rv |= statement->BindUTF8StringParameter(1, ownerDomain);
  rv |= statement->BindUTF8StringParameter(2, ownerURI);
  rv |= statement->BindUTF8StringParameter(3, key);
  NS_ENSURE_SUCCESS(rv, rv);

  return statement->Execute();
}

nsresult
nsOfflineCacheDevice::RemoveOwnedKey(const char * clientID,
                                     const nsACString & ownerDomain,
                                     const nsACString & ownerURI,
                                     const nsACString & key)
{
  LOG(("nsOfflineCacheDevice::RemoveOwnedKey [cid=%s]\n", clientID));

  PRBool isOwned;
  nsresult rv = KeyIsOwned(clientID, ownerDomain, ownerURI, key, &isOwned);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!isOwned) return NS_ERROR_NOT_AVAILABLE;

  AutoResetStatement statement(mStatement_RemoveOwnership);
  rv = statement->BindUTF8StringParameter(0, nsDependentCString(clientID));
  rv |= statement->BindUTF8StringParameter(1, ownerDomain);
  rv |= statement->BindUTF8StringParameter(2, ownerURI);
  rv |= statement->BindUTF8StringParameter(3, key);
  NS_ENSURE_SUCCESS(rv, rv);

  return statement->Execute();
}

nsresult
nsOfflineCacheDevice::KeyIsOwned(const char * clientID,
                                 const nsACString & ownerDomain,
                                 const nsACString & ownerURI,
                                 const nsACString & key,
                                 PRBool * isOwned)
{
  AutoResetStatement statement(mStatement_CheckOwnership);
  nsresult rv = statement->BindUTF8StringParameter(
                                           0, nsDependentCString(clientID));
  rv |= statement->BindUTF8StringParameter(1, ownerDomain);
  rv |= statement->BindUTF8StringParameter(2, ownerURI);
  rv |= statement->BindUTF8StringParameter(3, key);
  NS_ENSURE_SUCCESS(rv, rv);

  return statement->ExecuteStep(isOwned);
}

nsresult
nsOfflineCacheDevice::ClearKeysOwnedByDomain(const char *clientID,
                                             const nsACString &domain)
{
  LOG(("nsOfflineCacheDevice::ClearKeysOwnedByDomain [cid=%s]\n", clientID));

  AutoResetStatement statement(mStatement_ClearDomain);
  nsresult rv = statement->BindUTF8StringParameter(
                                           0, nsDependentCString(clientID));
  rv |= statement->BindUTF8StringParameter(1, domain);
  NS_ENSURE_SUCCESS(rv, rv);

  return statement->Execute();
}

nsresult
nsOfflineCacheDevice::EvictUnownedEntries(const char *clientID)
{
  LOG(("nsOfflineCacheDevice::EvictUnownedEntries [cid=%s]\n", clientID));
  EvictionObserver evictionObserver(mDB, mEvictionFunction);

  AutoResetStatement statement(mStatement_DeleteUnowned);
  nsresult rv = statement->BindUTF8StringParameter(
                                              0, nsDependentCString(clientID));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  evictionObserver.Apply();
  return NS_OK;
}

nsresult
nsOfflineCacheDevice::CreateTemporaryClientID(nsACString &clientID)
{
  nsCAutoString str;
  str.AssignLiteral("TempClient");
  str.AppendInt(gNextTemporaryClientID++);

  clientID.Assign(str);

  return NS_OK;
}

nsresult
nsOfflineCacheDevice::MergeTemporaryClientID(const char *clientID,
                                             const char *fromClientID)
{
  LOG(("nsOfflineCacheDevice::MergeTemporaryClientID [cid=%s, from=%s]\n",
       clientID, fromClientID));
  mozStorageTransaction transaction(mDB, PR_FALSE);

  // Move over ownerships
  AutoResetStatement listOwnersStatement(mStatement_ListOwners);
  nsresult rv = listOwnersStatement->BindUTF8StringParameter(
                                          0, nsDependentCString(fromClientID));
  NS_ENSURE_SUCCESS(rv, rv);

  // List all the owners in the new session
  nsTArray<nsCString> domainArray;
  nsTArray<nsCString> uriArray;

  PRBool hasRows;
  rv = listOwnersStatement->ExecuteStep(&hasRows);
  NS_ENSURE_SUCCESS(rv, rv);
  while (hasRows)
  {
    PRUint32 length;
    domainArray.AppendElement(
      nsDependentCString(listOwnersStatement->AsSharedUTF8String(0, &length)));
    uriArray.AppendElement(
      nsDependentCString(listOwnersStatement->AsSharedUTF8String(1, &length)));

    rv = listOwnersStatement->ExecuteStep(&hasRows);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Now move over each ownership set
  for (PRUint32 i = 0; i < domainArray.Length(); i++) {
    PRUint32 count;
    char **keys;
    rv = GetOwnedKeys(fromClientID, domainArray[i], uriArray[i],
                      &count, &keys);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = SetOwnedKeys(clientID, domainArray[i], uriArray[i],
                      count, const_cast<const char **>(keys));
    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(count, keys);
    NS_ENSURE_SUCCESS(rv, rv);

    // Now clear out the temporary session's copy
    rv = SetOwnedKeys(fromClientID, domainArray[i], uriArray[i], 0, 0);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  EvictionObserver evictionObserver(mDB, mEvictionFunction);

  AutoResetStatement deleteStatement(mStatement_DeleteConflicts);
  rv = deleteStatement->BindUTF8StringParameter(
                                          0, nsDependentCString(clientID));
  rv |= deleteStatement->BindUTF8StringParameter(
                                          1, nsDependentCString(fromClientID));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = deleteStatement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  AutoResetStatement swapStatement(mStatement_SwapClientID);
  rv = swapStatement->BindUTF8StringParameter(
                                          0, nsDependentCString(clientID));
  rv |= swapStatement->BindUTF8StringParameter(
                                          1, nsDependentCString(fromClientID));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = swapStatement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  evictionObserver.Apply();

  return NS_OK;
}

/**
 * Preference accessors
 */

void
nsOfflineCacheDevice::SetCacheParentDirectory(nsILocalFile *parentDir)
{
  if (Initialized())
  {
    NS_ERROR("cannot switch cache directory once initialized");
    return;
  }

  if (!parentDir)
  {
    mCacheDirectory = nsnull;
    return;
  }

  // ensure parent directory exists
  nsresult rv = EnsureDir(parentDir);
  if (NS_FAILED(rv))
  {
    NS_WARNING("unable to create parent directory");
    return;
  }

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
nsOfflineCacheDevice::SetCapacity(PRUint32 capacity)
{
  mCacheCapacity = capacity * 1024;
}
