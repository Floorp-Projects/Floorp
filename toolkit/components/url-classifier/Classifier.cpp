//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Classifier.h"
#include "LookupCacheV4.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsISimpleEnumerator.h"
#include "nsIRandomGenerator.h"
#include "nsIInputStream.h"
#include "nsISeekableStream.h"
#include "nsIFile.h"
#include "nsNetCID.h"
#include "nsPrintfCString.h"
#include "nsThreadUtils.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Logging.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/Base64.h"
#include "mozilla/Unused.h"

// MOZ_LOG=UrlClassifierDbService:5
extern mozilla::LazyLogModule gUrlClassifierDbServiceLog;
#define LOG(args) MOZ_LOG(gUrlClassifierDbServiceLog, mozilla::LogLevel::Debug, args)
#define LOG_ENABLED() MOZ_LOG_TEST(gUrlClassifierDbServiceLog, mozilla::LogLevel::Debug)

#define STORE_DIRECTORY      NS_LITERAL_CSTRING("safebrowsing")
#define TO_DELETE_DIR_SUFFIX NS_LITERAL_CSTRING("-to_delete")
#define BACKUP_DIR_SUFFIX    NS_LITERAL_CSTRING("-backup")

#define METADATA_SUFFIX      NS_LITERAL_CSTRING(".metadata")

namespace mozilla {
namespace safebrowsing {

namespace {

// A scoped-clearer for nsTArray<TableUpdate*>.
// The owning elements will be deleted and the array itself
// will be cleared on exiting the scope.
class ScopedUpdatesClearer {
public:
  explicit ScopedUpdatesClearer(nsTArray<TableUpdate*> *aUpdates)
    : mUpdatesArrayRef(aUpdates)
  {
    for (auto update : *aUpdates) {
      mUpdatesPointerHolder.AppendElement(update);
    }
  }

  ~ScopedUpdatesClearer()
  {
    mUpdatesArrayRef->Clear();
  }

private:
  nsTArray<TableUpdate*>* mUpdatesArrayRef;
  nsTArray<nsAutoPtr<TableUpdate>> mUpdatesPointerHolder;
};

} // End of unnamed namespace.

void
Classifier::SplitTables(const nsACString& str, nsTArray<nsCString>& tables)
{
  tables.Clear();

  nsACString::const_iterator begin, iter, end;
  str.BeginReading(begin);
  str.EndReading(end);
  while (begin != end) {
    iter = begin;
    FindCharInReadable(',', iter, end);
    nsDependentCSubstring table = Substring(begin,iter);
    if (!table.IsEmpty()) {
      tables.AppendElement(Substring(begin, iter));
    }
    begin = iter;
    if (begin != end) {
      begin++;
    }
  }
}

static nsresult
DeriveProviderFromPref(const nsACString& aTableName, nsCString& aProviderName)
{
  // Check all preferences "browser.safebrowsing.provider.[provider].list"
  // to see which one contains |aTableName|.

  nsCOMPtr<nsIPrefService> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  NS_ENSURE_TRUE(prefs, NS_ERROR_FAILURE);
  nsCOMPtr<nsIPrefBranch> prefBranch;
  nsresult rv = prefs->GetBranch("browser.safebrowsing.provider.",
                                  getter_AddRefs(prefBranch));
  NS_ENSURE_SUCCESS(rv, rv);

  // We've got a pref branch for "browser.safebrowsing.provider.".
  // Enumerate all children prefs and parse providers.
  uint32_t childCount;
  char** childArray;
  rv = prefBranch->GetChildList("", &childCount, &childArray);
  NS_ENSURE_SUCCESS(rv, rv);

  // Collect providers from childArray.
  nsTHashtable<nsCStringHashKey> providers;
  for (uint32_t i = 0; i < childCount; i++) {
    nsCString child(childArray[i]);
    auto dotPos = child.FindChar('.');
    if (dotPos < 0) {
      continue;
    }

    nsDependentCSubstring provider = Substring(child, 0, dotPos);

    providers.PutEntry(provider);
  }
  NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(childCount, childArray);

  // Now we have all providers. Check which one owns |aTableName|.
  // e.g. The owning lists of provider "google" is defined in
  // "browser.safebrowsing.provider.google.lists".
  for (auto itr = providers.Iter(); !itr.Done(); itr.Next()) {
    auto entry = itr.Get();
    nsCString provider(entry->GetKey());
    nsPrintfCString owninListsPref("%s.lists", provider.get());

    nsXPIDLCString owningLists;
    nsresult rv = prefBranch->GetCharPref(owninListsPref.get(),
                                          getter_Copies(owningLists));
    if (NS_FAILED(rv)) {
      continue;
    }

    // We've got the owning lists (represented as string) of |provider|.
    // Parse the string and see if |aTableName| is included.
    nsTArray<nsCString> tables;
    Classifier::SplitTables(owningLists, tables);
    if (tables.Contains(aTableName)) {
      aProviderName = provider;
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}

// Lookup the provider name by table name on non-main thread.
// On main thread, just call DeriveProviderFromPref() instead
// but DeriveProviderFromPref is supposed to used internally.
static nsCString
GetProviderByTableName(const nsACString& aTableName)
{
  MOZ_ASSERT(!NS_IsMainThread(), "GetProviderByTableName MUST be called "
                                 "on non-main thread.");
  nsCString providerName;

  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction([&aTableName,
                                                    &providerName] () -> void {
    nsresult rv = DeriveProviderFromPref(aTableName, providerName);
    if (NS_FAILED(rv)) {
      LOG(("No provider found for %s", nsCString(aTableName).get()));
    }
  });

  nsCOMPtr<nsIThread> mainThread = do_GetMainThread();
  SyncRunnable::DispatchToThread(mainThread, r);

  return providerName;
}

nsresult
Classifier::GetPrivateStoreDirectory(nsIFile* aRootStoreDirectory,
                                     const nsACString& aTableName,
                                     nsIFile** aPrivateStoreDirectory)
{
  NS_ENSURE_ARG_POINTER(aPrivateStoreDirectory);

  if (!StringEndsWith(aTableName, NS_LITERAL_CSTRING("-proto"))) {
    // Only V4 table names (ends with '-proto') would be stored
    // to per-provider sub-directory.
    nsCOMPtr<nsIFile>(aRootStoreDirectory).forget(aPrivateStoreDirectory);
    return NS_OK;
  }

  nsCString providerName = GetProviderByTableName(aTableName);
  if (providerName.IsEmpty()) {
    // When failing to get provider, just store in the root folder.
    nsCOMPtr<nsIFile>(aRootStoreDirectory).forget(aPrivateStoreDirectory);
    return NS_OK;
  }

  nsCOMPtr<nsIFile> providerDirectory;

  // Clone first since we are gonna create a new directory.
  nsresult rv = aRootStoreDirectory->Clone(getter_AddRefs(providerDirectory));
  NS_ENSURE_SUCCESS(rv, rv);

  // Append the provider name to the root store directory.
  rv = providerDirectory->AppendNative(providerName);
  NS_ENSURE_SUCCESS(rv, rv);

  // Ensure existence of the provider directory.
  bool dirExists;
  rv = providerDirectory->Exists(&dirExists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!dirExists) {
    LOG(("Creating private directory for %s", nsCString(aTableName).get()));
    rv = providerDirectory->Create(nsIFile::DIRECTORY_TYPE, 0755);
    NS_ENSURE_SUCCESS(rv, rv);
    providerDirectory.forget(aPrivateStoreDirectory);
    return rv;
  }

  // Store directory exists. Check if it's a directory.
  bool isDir;
  rv = providerDirectory->IsDirectory(&isDir);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!isDir) {
    return NS_ERROR_FILE_DESTINATION_NOT_DIR;
  }

  providerDirectory.forget(aPrivateStoreDirectory);

  return NS_OK;
}

Classifier::Classifier()
{
}

Classifier::~Classifier()
{
  Close();
}

nsresult
Classifier::SetupPathNames()
{
  // Get the root directory where to store all the databases.
  nsresult rv = mCacheDirectory->Clone(getter_AddRefs(mRootStoreDirectory));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mRootStoreDirectory->AppendNative(STORE_DIRECTORY);
  NS_ENSURE_SUCCESS(rv, rv);

  // Make sure LookupCaches (which are persistent and survive updates)
  // are reading/writing in the right place. We will be moving their
  // files "underneath" them during backup/restore.
  for (uint32_t i = 0; i < mLookupCaches.Length(); i++) {
    mLookupCaches[i]->UpdateRootDirHandle(mRootStoreDirectory);
  }

  // Directory where to move a backup before an update.
  rv = mCacheDirectory->Clone(getter_AddRefs(mBackupDirectory));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mBackupDirectory->AppendNative(STORE_DIRECTORY + BACKUP_DIR_SUFFIX);
  NS_ENSURE_SUCCESS(rv, rv);

  // Directory where to move the backup so we can atomically
  // delete (really move) it.
  rv = mCacheDirectory->Clone(getter_AddRefs(mToDeleteDirectory));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mToDeleteDirectory->AppendNative(STORE_DIRECTORY + TO_DELETE_DIR_SUFFIX);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
Classifier::CreateStoreDirectory()
{
  // Ensure the safebrowsing directory exists.
  bool storeExists;
  nsresult rv = mRootStoreDirectory->Exists(&storeExists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!storeExists) {
    rv = mRootStoreDirectory->Create(nsIFile::DIRECTORY_TYPE, 0755);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    bool storeIsDir;
    rv = mRootStoreDirectory->IsDirectory(&storeIsDir);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!storeIsDir)
      return NS_ERROR_FILE_DESTINATION_NOT_DIR;
  }

  return NS_OK;
}

nsresult
Classifier::Open(nsIFile& aCacheDirectory)
{
  // Remember the Local profile directory.
  nsresult rv = aCacheDirectory.Clone(getter_AddRefs(mCacheDirectory));
  NS_ENSURE_SUCCESS(rv, rv);

  // Create the handles to the update and backup directories.
  rv = SetupPathNames();
  NS_ENSURE_SUCCESS(rv, rv);

  // Clean up any to-delete directories that haven't been deleted yet.
  rv = CleanToDelete();
  NS_ENSURE_SUCCESS(rv, rv);

  // Check whether we have an incomplete update and recover from the
  // backup if so.
  rv = RecoverBackups();
  NS_ENSURE_SUCCESS(rv, rv);

  // Make sure the main store directory exists.
  rv = CreateStoreDirectory();
  NS_ENSURE_SUCCESS(rv, rv);

  mCryptoHash = do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Build the list of know urlclassifier lists
  // XXX: Disk IO potentially on the main thread during startup
  RegenActiveTables();

  return NS_OK;
}

void
Classifier::Close()
{
  DropStores();
}

void
Classifier::Reset()
{
  DropStores();

  mRootStoreDirectory->Remove(true);
  mBackupDirectory->Remove(true);
  mToDeleteDirectory->Remove(true);

  CreateStoreDirectory();

  mTableFreshness.Clear();
  RegenActiveTables();
}

void
Classifier::ResetTables(ClearType aType, const nsTArray<nsCString>& aTables)
{
  for (uint32_t i = 0; i < aTables.Length(); i++) {
    LOG(("Resetting table: %s", aTables[i].get()));
    // Spoil this table by marking it as no known freshness
    mTableFreshness.Remove(aTables[i]);
    LookupCache *cache = GetLookupCache(aTables[i]);
    if (cache) {
      // Remove any cached Completes for this table if clear type is Clear_Cache
      if (aType == Clear_Cache) {
        cache->ClearCache();
      } else {
        cache->ClearAll();
      }
    }
  }

  // Clear on-disk database if clear type is Clear_All
  if (aType == Clear_All) {
    DeleteTables(mRootStoreDirectory, aTables);

    RegenActiveTables();
  }
}

void
Classifier::DeleteTables(nsIFile* aDirectory, const nsTArray<nsCString>& aTables)
{
  nsCOMPtr<nsISimpleEnumerator> entries;
  nsresult rv = aDirectory->GetDirectoryEntries(getter_AddRefs(entries));
  NS_ENSURE_SUCCESS_VOID(rv);

  bool hasMore;
  while (NS_SUCCEEDED(rv = entries->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> supports;
    rv = entries->GetNext(getter_AddRefs(supports));
    NS_ENSURE_SUCCESS_VOID(rv);

    nsCOMPtr<nsIFile> file = do_QueryInterface(supports);
    NS_ENSURE_TRUE_VOID(file);

    // If |file| is a directory, recurse to find its entries as well.
    bool isDirectory;
    if (NS_FAILED(file->IsDirectory(&isDirectory))) {
      continue;
    }
    if (isDirectory) {
      DeleteTables(file, aTables);
      continue;
    }

    nsCString leafName;
    rv = file->GetNativeLeafName(leafName);
    NS_ENSURE_SUCCESS_VOID(rv);

    leafName.Truncate(leafName.RFind("."));
    if (aTables.Contains(leafName)) {
      if (NS_FAILED(file->Remove(false))) {
        NS_WARNING(nsPrintfCString("Fail to remove file %s from the disk",
                                   leafName.get()).get());
      }
    }
  }
  NS_ENSURE_SUCCESS_VOID(rv);
}

void
Classifier::AbortUpdateAndReset(const nsCString& aTable)
{
  LOG(("Abort updating table %s.", aTable.get()));

  // ResetTables will clear both in-memory & on-disk data.
  ResetTables(Clear_All, nsTArray<nsCString> { aTable });

  // Remove the backup and delete directory since we are aborting
  // from an update.
  Unused << RemoveBackupTables();
  Unused << CleanToDelete();
}

void
Classifier::TableRequest(nsACString& aResult)
{
  // Generating v2 table info.
  nsTArray<nsCString> tables;
  ActiveTables(tables);
  for (uint32_t i = 0; i < tables.Length(); i++) {
    HashStore store(tables[i], mRootStoreDirectory);

    nsresult rv = store.Open();
    if (NS_FAILED(rv))
      continue;

    aResult.Append(store.TableName());
    aResult.Append(';');

    ChunkSet &adds = store.AddChunks();
    ChunkSet &subs = store.SubChunks();

    if (adds.Length() > 0) {
      aResult.AppendLiteral("a:");
      nsAutoCString addList;
      adds.Serialize(addList);
      aResult.Append(addList);
    }

    if (subs.Length() > 0) {
      if (adds.Length() > 0)
        aResult.Append(':');
      aResult.AppendLiteral("s:");
      nsAutoCString subList;
      subs.Serialize(subList);
      aResult.Append(subList);
    }

    aResult.Append('\n');
  }

  // Load meta data from *.metadata files in the root directory.
  // Specifically for v4 tables.
  nsCString metadata;
  nsresult rv = LoadMetadata(mRootStoreDirectory, metadata);
  NS_ENSURE_SUCCESS_VOID(rv);
  aResult.Append(metadata);
}

nsresult
Classifier::Check(const nsACString& aSpec,
                  const nsACString& aTables,
                  uint32_t aFreshnessGuarantee,
                  LookupResultArray& aResults)
{
  Telemetry::AutoTimer<Telemetry::URLCLASSIFIER_CL_CHECK_TIME> timer;

  // Get the set of fragments based on the url. This is necessary because we
  // only look up at most 5 URLs per aSpec, even if aSpec has more than 5
  // components.
  nsTArray<nsCString> fragments;
  nsresult rv = LookupCache::GetLookupFragments(aSpec, &fragments);
  NS_ENSURE_SUCCESS(rv, rv);

  nsTArray<nsCString> activeTables;
  SplitTables(aTables, activeTables);

  nsTArray<LookupCache*> cacheArray;
  for (uint32_t i = 0; i < activeTables.Length(); i++) {
    LOG(("Checking table %s", activeTables[i].get()));
    LookupCache *cache = GetLookupCache(activeTables[i]);
    if (cache) {
      cacheArray.AppendElement(cache);
    } else {
      return NS_ERROR_FAILURE;
    }
  }

  // Now check each lookup fragment against the entries in the DB.
  for (uint32_t i = 0; i < fragments.Length(); i++) {
    Completion lookupHash;
    lookupHash.FromPlaintext(fragments[i], mCryptoHash);

    if (LOG_ENABLED()) {
      nsAutoCString checking;
      lookupHash.ToHexString(checking);
      LOG(("Checking fragment %s, hash %s (%X)", fragments[i].get(),
           checking.get(), lookupHash.ToUint32()));
    }

    for (uint32_t i = 0; i < cacheArray.Length(); i++) {
      LookupCache *cache = cacheArray[i];
      bool has, complete;
      rv = cache->Has(lookupHash, &has, &complete);
      NS_ENSURE_SUCCESS(rv, rv);
      if (has) {
        LookupResult *result = aResults.AppendElement();
        if (!result)
          return NS_ERROR_OUT_OF_MEMORY;

        int64_t age;
        bool found = mTableFreshness.Get(cache->TableName(), &age);
        if (!found) {
          age = 24 * 60 * 60; // just a large number
        } else {
          int64_t now = (PR_Now() / PR_USEC_PER_SEC);
          age = now - age;
        }

        LOG(("Found a result in %s: %s (Age: %Lds)",
             cache->TableName().get(),
             complete ? "complete." : "Not complete.",
             age));

        result->hash.complete = lookupHash;
        result->mComplete = complete;
        result->mFresh = (age < aFreshnessGuarantee);
        result->mTableName.Assign(cache->TableName());
      }
    }

  }

  return NS_OK;
}

nsresult
Classifier::ApplyUpdates(nsTArray<TableUpdate*>* aUpdates)
{
  Telemetry::AutoTimer<Telemetry::URLCLASSIFIER_CL_UPDATE_TIME> timer;

  PRIntervalTime clockStart = 0;
  if (LOG_ENABLED()) {
    clockStart = PR_IntervalNow();
  }

  nsresult rv;

  {
    ScopedUpdatesClearer scopedUpdatesClearer(aUpdates);

    LOG(("Backup before update."));

    rv = BackupTables();
    NS_ENSURE_SUCCESS(rv, rv);

    LOG(("Applying %d table updates.", aUpdates->Length()));

    for (uint32_t i = 0; i < aUpdates->Length(); i++) {
      // Previous UpdateHashStore() may have consumed this update..
      if ((*aUpdates)[i]) {
        // Run all updates for one table
        nsCString updateTable(aUpdates->ElementAt(i)->TableName());

        if (TableUpdate::Cast<TableUpdateV2>((*aUpdates)[i])) {
          rv = UpdateHashStore(aUpdates, updateTable);
        } else {
          rv = UpdateTableV4(aUpdates, updateTable);
        }

        if (NS_FAILED(rv)) {
          if (rv != NS_ERROR_OUT_OF_MEMORY) {
            AbortUpdateAndReset(updateTable);
          }
          return rv;
        }
      }
    }

  } // End of scopedUpdatesClearer scope.

  rv = RegenActiveTables();
  NS_ENSURE_SUCCESS(rv, rv);

  LOG(("Cleaning up backups."));

  // Move the backup directory away (signaling the transaction finished
  // successfully). This is atomic.
  rv = RemoveBackupTables();
  NS_ENSURE_SUCCESS(rv, rv);

  // Do the actual deletion of the backup files.
  rv = CleanToDelete();
  NS_ENSURE_SUCCESS(rv, rv);

  LOG(("Done applying updates."));

  if (LOG_ENABLED()) {
    PRIntervalTime clockEnd = PR_IntervalNow();
    LOG(("update took %dms\n",
         PR_IntervalToMilliseconds(clockEnd - clockStart)));
  }

  return NS_OK;
}

nsresult
Classifier::ApplyFullHashes(nsTArray<TableUpdate*>* aUpdates)
{
  LOG(("Applying %d table gethashes.", aUpdates->Length()));

  ScopedUpdatesClearer scopedUpdatesClearer(aUpdates);
  for (uint32_t i = 0; i < aUpdates->Length(); i++) {
    TableUpdate *update = aUpdates->ElementAt(i);

    nsresult rv = UpdateCache(update);
    NS_ENSURE_SUCCESS(rv, rv);

    aUpdates->ElementAt(i) = nullptr;
  }

  return NS_OK;
}

int64_t
Classifier::GetLastUpdateTime(const nsACString& aTableName)
{
  int64_t age;
  bool found = mTableFreshness.Get(aTableName, &age);
  return found ? (age * PR_MSEC_PER_SEC) : 0;
}

void
Classifier::SetLastUpdateTime(const nsACString &aTable,
                              uint64_t updateTime)
{
  LOG(("Marking table %s as last updated on %u",
       PromiseFlatCString(aTable).get(), updateTime));
  mTableFreshness.Put(aTable, updateTime / PR_MSEC_PER_SEC);
}

void
Classifier::DropStores()
{
  for (uint32_t i = 0; i < mLookupCaches.Length(); i++) {
    delete mLookupCaches[i];
  }
  mLookupCaches.Clear();
}

nsresult
Classifier::RegenActiveTables()
{
  mActiveTablesCache.Clear();

  nsTArray<nsCString> foundTables;
  ScanStoreDir(foundTables);

  for (uint32_t i = 0; i < foundTables.Length(); i++) {
    nsCString table(foundTables[i]);
    HashStore store(table, mRootStoreDirectory);

    nsresult rv = store.Open();
    if (NS_FAILED(rv))
      continue;

    LookupCache *lookupCache = GetLookupCache(store.TableName());
    if (!lookupCache) {
      continue;
    }

    if (!lookupCache->IsPrimed())
      continue;

    const ChunkSet &adds = store.AddChunks();
    const ChunkSet &subs = store.SubChunks();

    if (adds.Length() == 0 && subs.Length() == 0)
      continue;

    LOG(("Active table: %s", store.TableName().get()));
    mActiveTablesCache.AppendElement(store.TableName());
  }

  return NS_OK;
}

nsresult
Classifier::ScanStoreDir(nsTArray<nsCString>& aTables)
{
  nsCOMPtr<nsISimpleEnumerator> entries;
  nsresult rv = mRootStoreDirectory->GetDirectoryEntries(getter_AddRefs(entries));
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasMore;
  while (NS_SUCCEEDED(rv = entries->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> supports;
    rv = entries->GetNext(getter_AddRefs(supports));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIFile> file = do_QueryInterface(supports);

    nsCString leafName;
    rv = file->GetNativeLeafName(leafName);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCString suffix(NS_LITERAL_CSTRING(".sbstore"));

    int32_t dot = leafName.RFind(suffix, 0);
    if (dot != -1) {
      leafName.Cut(dot, suffix.Length());
      aTables.AppendElement(leafName);
    }
  }
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
Classifier::ActiveTables(nsTArray<nsCString>& aTables)
{
  aTables = mActiveTablesCache;
  return NS_OK;
}

nsresult
Classifier::CleanToDelete()
{
  bool exists;
  nsresult rv = mToDeleteDirectory->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (exists) {
    rv = mToDeleteDirectory->Remove(true);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
Classifier::BackupTables()
{
  // We have to work in reverse here: first move the normal directory
  // away to be the backup directory, then copy the files over
  // to the normal directory. This ensures that if we crash the backup
  // dir always has a valid, complete copy, instead of a partial one,
  // because that's the one we will copy over the normal store dir.

  nsCString backupDirName;
  nsresult rv = mBackupDirectory->GetNativeLeafName(backupDirName);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString storeDirName;
  rv = mRootStoreDirectory->GetNativeLeafName(storeDirName);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mRootStoreDirectory->MoveToNative(nullptr, backupDirName);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mRootStoreDirectory->CopyToNative(nullptr, storeDirName);
  NS_ENSURE_SUCCESS(rv, rv);

  // We moved some things to new places, so move the handles around, too.
  rv = SetupPathNames();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
Classifier::RemoveBackupTables()
{
  nsCString toDeleteName;
  nsresult rv = mToDeleteDirectory->GetNativeLeafName(toDeleteName);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mBackupDirectory->MoveToNative(nullptr, toDeleteName);
  NS_ENSURE_SUCCESS(rv, rv);

  // mBackupDirectory now points to toDelete, fix that up.
  rv = SetupPathNames();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
Classifier::RecoverBackups()
{
  bool backupExists;
  nsresult rv = mBackupDirectory->Exists(&backupExists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (backupExists) {
    // Remove the safebrowsing dir if it exists
    nsCString storeDirName;
    rv = mRootStoreDirectory->GetNativeLeafName(storeDirName);
    NS_ENSURE_SUCCESS(rv, rv);

    bool storeExists;
    rv = mRootStoreDirectory->Exists(&storeExists);
    NS_ENSURE_SUCCESS(rv, rv);

    if (storeExists) {
      rv = mRootStoreDirectory->Remove(true);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // Move the backup to the store location
    rv = mBackupDirectory->MoveToNative(nullptr, storeDirName);
    NS_ENSURE_SUCCESS(rv, rv);

    // mBackupDirectory now points to storeDir, fix up.
    rv = SetupPathNames();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

bool
Classifier::CheckValidUpdate(nsTArray<TableUpdate*>* aUpdates,
                             const nsACString& aTable)
{
  // take the quick exit if there is no valid update for us
  // (common case)
  uint32_t validupdates = 0;

  for (uint32_t i = 0; i < aUpdates->Length(); i++) {
    TableUpdate *update = aUpdates->ElementAt(i);
    if (!update || !update->TableName().Equals(aTable))
      continue;
    if (update->Empty()) {
      aUpdates->ElementAt(i) = nullptr;
      continue;
    }
    validupdates++;
  }

  if (!validupdates) {
    // This can happen if the update was only valid for one table.
    return false;
  }

  return true;
}

/*
 * This will consume+delete updates from the passed nsTArray.
*/
nsresult
Classifier::UpdateHashStore(nsTArray<TableUpdate*>* aUpdates,
                            const nsACString& aTable)
{
  LOG(("Classifier::UpdateHashStore(%s)", PromiseFlatCString(aTable).get()));

  HashStore store(aTable, mRootStoreDirectory);

  if (!CheckValidUpdate(aUpdates, store.TableName())) {
    return NS_OK;
  }

  nsresult rv = store.Open();
  NS_ENSURE_SUCCESS(rv, rv);
  rv = store.BeginUpdate();
  NS_ENSURE_SUCCESS(rv, rv);

  // Read the part of the store that is (only) in the cache
  LookupCacheV2* lookupCache =
    LookupCache::Cast<LookupCacheV2>(GetLookupCache(store.TableName()));
  if (!lookupCache) {
    return NS_ERROR_FAILURE;
  }

  // Clear cache when update
  lookupCache->ClearCache();

  FallibleTArray<uint32_t> AddPrefixHashes;
  rv = lookupCache->GetPrefixes(AddPrefixHashes);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = store.AugmentAdds(AddPrefixHashes);
  NS_ENSURE_SUCCESS(rv, rv);
  AddPrefixHashes.Clear();

  uint32_t applied = 0;

  for (uint32_t i = 0; i < aUpdates->Length(); i++) {
    TableUpdate *update = aUpdates->ElementAt(i);
    if (!update || !update->TableName().Equals(store.TableName()))
      continue;

    rv = store.ApplyUpdate(*update);
    NS_ENSURE_SUCCESS(rv, rv);

    applied++;

    auto updateV2 = TableUpdate::Cast<TableUpdateV2>(update);
    if (updateV2) {
      LOG(("Applied update to table %s:", store.TableName().get()));
      LOG(("  %d add chunks", updateV2->AddChunks().Length()));
      LOG(("  %d add prefixes", updateV2->AddPrefixes().Length()));
      LOG(("  %d add completions", updateV2->AddCompletes().Length()));
      LOG(("  %d sub chunks", updateV2->SubChunks().Length()));
      LOG(("  %d sub prefixes", updateV2->SubPrefixes().Length()));
      LOG(("  %d sub completions", updateV2->SubCompletes().Length()));
      LOG(("  %d add expirations", updateV2->AddExpirations().Length()));
      LOG(("  %d sub expirations", updateV2->SubExpirations().Length()));
    }

    aUpdates->ElementAt(i) = nullptr;
  }

  LOG(("Applied %d update(s) to %s.", applied, store.TableName().get()));

  rv = store.Rebuild();
  NS_ENSURE_SUCCESS(rv, rv);

  LOG(("Table %s now has:", store.TableName().get()));
  LOG(("  %d add chunks", store.AddChunks().Length()));
  LOG(("  %d add prefixes", store.AddPrefixes().Length()));
  LOG(("  %d add completions", store.AddCompletes().Length()));
  LOG(("  %d sub chunks", store.SubChunks().Length()));
  LOG(("  %d sub prefixes", store.SubPrefixes().Length()));
  LOG(("  %d sub completions", store.SubCompletes().Length()));

  rv = store.WriteFile();
  NS_ENSURE_SUCCESS(rv, rv);

  // At this point the store is updated and written out to disk, but
  // the data is still in memory.  Build our quick-lookup table here.
  rv = lookupCache->Build(store.AddPrefixes(), store.AddCompletes());
  NS_ENSURE_SUCCESS(rv, rv);

#if defined(DEBUG)
  lookupCache->DumpCompletions();
#endif
  rv = lookupCache->WriteFile();
  NS_ENSURE_SUCCESS(rv, rv);

  int64_t now = (PR_Now() / PR_USEC_PER_SEC);
  LOG(("Successfully updated %s", store.TableName().get()));
  mTableFreshness.Put(store.TableName(), now);

  return NS_OK;
}

nsresult
Classifier::UpdateTableV4(nsTArray<TableUpdate*>* aUpdates,
                          const nsACString& aTable)
{
  LOG(("Classifier::UpdateTableV4(%s)", PromiseFlatCString(aTable).get()));

  if (!CheckValidUpdate(aUpdates, aTable)) {
    return NS_OK;
  }

  LookupCacheV4* lookupCache =
    LookupCache::Cast<LookupCacheV4>(GetLookupCache(aTable));
  if (!lookupCache) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = NS_OK;

  // If there are multiple updates for the same table, prefixes1 & prefixes2
  // will act as input and output in turn to reduce memory copy overhead.
  PrefixStringMap prefixes1, prefixes2;
  PrefixStringMap* input = &prefixes1;
  PrefixStringMap* output = &prefixes2;

  TableUpdateV4* lastAppliedUpdate = nullptr;
  for (uint32_t i = 0; i < aUpdates->Length(); i++) {
    TableUpdate *update = aUpdates->ElementAt(i);
    if (!update || !update->TableName().Equals(aTable)) {
      continue;
    }

    auto updateV4 = TableUpdate::Cast<TableUpdateV4>(update);
    NS_ENSURE_TRUE(updateV4, NS_ERROR_FAILURE);

    if (updateV4->IsFullUpdate()) {
      input->Clear();
      output->Clear();
      rv = lookupCache->ApplyUpdate(updateV4, *input, *output);
      if (NS_FAILED(rv)) {
        return rv;
      }
    } else {
      // If both prefix sets are empty, this means we are doing a partial update
      // without a prior full/partial update in the loop. In this case we should
      // get prefixes from the lookup cache first.
      if (prefixes1.IsEmpty() && prefixes2.IsEmpty()) {
        lookupCache->GetPrefixes(prefixes1);
      } else {
        MOZ_ASSERT(prefixes1.IsEmpty() ^ prefixes2.IsEmpty());

        // When there are multiple partial updates, input should always point
        // to the non-empty prefix set(filled by previous full/partial update).
        // output should always point to the empty prefix set.
        input = prefixes1.IsEmpty() ? &prefixes2 : &prefixes1;
        output = prefixes1.IsEmpty() ? &prefixes1 : &prefixes2;
      }

      rv = lookupCache->ApplyUpdate(updateV4, *input, *output);
      if (NS_FAILED(rv)) {
        return rv;
      }

      input->Clear();
    }

    // Keep track of the last applied update.
    lastAppliedUpdate = updateV4;

    aUpdates->ElementAt(i) = nullptr;
  }

  rv = lookupCache->Build(*output);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = lookupCache->WriteFile();
  NS_ENSURE_SUCCESS(rv, rv);

  if (lastAppliedUpdate) {
    LOG(("Write meta data of the last applied update."));
    rv = lookupCache->WriteMetadata(lastAppliedUpdate);
    NS_ENSURE_SUCCESS(rv, rv);
  }


  int64_t now = (PR_Now() / PR_USEC_PER_SEC);
  LOG(("Successfully updated %s\n", PromiseFlatCString(aTable).get()));
  mTableFreshness.Put(aTable, now);

  return NS_OK;
}

nsresult
Classifier::UpdateCache(TableUpdate* aUpdate)
{
  if (!aUpdate) {
    return NS_OK;
  }

  nsAutoCString table(aUpdate->TableName());
  LOG(("Classifier::UpdateCache(%s)", table.get()));

  LookupCache *lookupCache = GetLookupCache(table);
  if (!lookupCache) {
    return NS_ERROR_FAILURE;
  }

  auto updateV2 = TableUpdate::Cast<TableUpdateV2>(aUpdate);
  lookupCache->AddCompletionsToCache(updateV2->AddCompletes());

#if defined(DEBUG)
  lookupCache->DumpCache();
#endif

  return NS_OK;
}

LookupCache *
Classifier::GetLookupCache(const nsACString& aTable)
{
  for (uint32_t i = 0; i < mLookupCaches.Length(); i++) {
    if (mLookupCaches[i]->TableName().Equals(aTable)) {
      return mLookupCaches[i];
    }
  }

  // TODO : Bug 1302600, It would be better if we have a more general non-main
  //        thread method to convert table name to protocol version. Currently
  //        we can only know this by checking if the table name ends with '-proto'.
  UniquePtr<LookupCache> cache;
  if (StringEndsWith(aTable, NS_LITERAL_CSTRING("-proto"))) {
    cache = MakeUnique<LookupCacheV4>(aTable, mRootStoreDirectory);
  } else {
    cache = MakeUnique<LookupCacheV2>(aTable, mRootStoreDirectory);
  }

  nsresult rv = cache->Init();
  if (NS_FAILED(rv)) {
    return nullptr;
  }
  rv = cache->Open();
  if (NS_FAILED(rv)) {
    if (rv == NS_ERROR_FILE_CORRUPTED) {
      Reset();
    }
    return nullptr;
  }
  mLookupCaches.AppendElement(cache.get());
  return cache.release();
}

nsresult
Classifier::ReadNoiseEntries(const Prefix& aPrefix,
                             const nsACString& aTableName,
                             uint32_t aCount,
                             PrefixArray* aNoiseEntries)
{
  // TODO : Bug 1297962, support adding noise for v4
  LookupCacheV2 *cache = static_cast<LookupCacheV2*>(GetLookupCache(aTableName));
  if (!cache) {
    return NS_ERROR_FAILURE;
  }

  FallibleTArray<uint32_t> prefixes;
  nsresult rv = cache->GetPrefixes(prefixes);
  NS_ENSURE_SUCCESS(rv, rv);

  size_t idx = prefixes.BinaryIndexOf(aPrefix.ToUint32());

  if (idx == nsTArray<uint32_t>::NoIndex) {
    NS_WARNING("Could not find prefix in PrefixSet during noise lookup");
    return NS_ERROR_FAILURE;
  }

  idx -= idx % aCount;

  for (size_t i = 0; (i < aCount) && ((idx+i) < prefixes.Length()); i++) {
    Prefix newPref;
    newPref.FromUint32(prefixes[idx+i]);
    if (newPref != aPrefix) {
      aNoiseEntries->AppendElement(newPref);
    }
  }

  return NS_OK;
}

nsresult
Classifier::LoadMetadata(nsIFile* aDirectory, nsACString& aResult)
{
  nsCOMPtr<nsISimpleEnumerator> entries;
  nsresult rv = aDirectory->GetDirectoryEntries(getter_AddRefs(entries));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_ARG_POINTER(entries);

  bool hasMore;
  while (NS_SUCCEEDED(rv = entries->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> supports;
    rv = entries->GetNext(getter_AddRefs(supports));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIFile> file = do_QueryInterface(supports);

    // If |file| is a directory, recurse to find its entries as well.
    bool isDirectory;
    if (NS_FAILED(file->IsDirectory(&isDirectory))) {
      continue;
    }
    if (isDirectory) {
      LoadMetadata(file, aResult);
      continue;
    }

    // Truncate file extension to get the table name.
    nsCString tableName;
    rv = file->GetNativeLeafName(tableName);
    NS_ENSURE_SUCCESS(rv, rv);

    int32_t dot = tableName.RFind(METADATA_SUFFIX, 0);
    if (dot == -1) {
      continue;
    }
    tableName.Cut(dot, METADATA_SUFFIX.Length());

    LookupCacheV4* lookupCache =
      LookupCache::Cast<LookupCacheV4>(GetLookupCache(tableName));
    if (!lookupCache) {
      continue;
    }

    nsCString state;
    nsCString checksum;
    rv = lookupCache->LoadMetadata(state, checksum);
    if (NS_FAILED(rv)) {
      LOG(("Failed to get metadata for table %s", tableName.get()));
      continue;
    }

    // The state might include '\n' so that we have to encode.
    nsAutoCString stateBase64;
    rv = Base64Encode(state, stateBase64);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoCString checksumBase64;
    rv = Base64Encode(checksum, checksumBase64);
    NS_ENSURE_SUCCESS(rv, rv);

    LOG(("Appending state '%s' and checksum '%s' for table %s",
         stateBase64.get(), checksumBase64.get(), tableName.get()));

    aResult.AppendPrintf("%s;%s:%s\n", tableName.get(),
                                       stateBase64.get(),
                                       checksumBase64.get());
  }

  return rv;
}

} // namespace safebrowsing
} // namespace mozilla
