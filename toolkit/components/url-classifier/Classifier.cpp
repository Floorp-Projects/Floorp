//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Classifier.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsISimpleEnumerator.h"
#include "nsIRandomGenerator.h"
#include "nsIInputStream.h"
#include "nsISeekableStream.h"
#include "nsIFile.h"
#include "nsAutoPtr.h"
#include "mozilla/Telemetry.h"
#include "prlog.h"

// NSPR_LOG_MODULES=UrlClassifierDbService:5
extern PRLogModuleInfo *gUrlClassifierDbServiceLog;
#if defined(PR_LOGGING)
#define LOG(args) PR_LOG(gUrlClassifierDbServiceLog, PR_LOG_DEBUG, args)
#define LOG_ENABLED() PR_LOG_TEST(gUrlClassifierDbServiceLog, 4)
#else
#define LOG(args)
#define LOG_ENABLED() (false)
#endif

#define STORE_DIRECTORY      NS_LITERAL_CSTRING("safebrowsing")
#define TO_DELETE_DIR_SUFFIX NS_LITERAL_CSTRING("-to_delete")
#define BACKUP_DIR_SUFFIX    NS_LITERAL_CSTRING("-backup")

namespace mozilla {
namespace safebrowsing {

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

Classifier::Classifier()
  : mFreshTime(45 * 60)
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
  nsresult rv = mCacheDirectory->Clone(getter_AddRefs(mStoreDirectory));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mStoreDirectory->AppendNative(STORE_DIRECTORY);
  NS_ENSURE_SUCCESS(rv, rv);

  // Make sure LookupCaches (which are persistent and survive updates)
  // are reading/writing in the right place. We will be moving their
  // files "underneath" them during backup/restore.
  for (uint32_t i = 0; i < mLookupCaches.Length(); i++) {
    mLookupCaches[i]->UpdateDirHandle(mStoreDirectory);
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
  nsresult rv = mStoreDirectory->Exists(&storeExists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!storeExists) {
    rv = mStoreDirectory->Create(nsIFile::DIRECTORY_TYPE, 0755);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    bool storeIsDir;
    rv = mStoreDirectory->IsDirectory(&storeIsDir);
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

  mStoreDirectory->Remove(true);
  mBackupDirectory->Remove(true);
  mToDeleteDirectory->Remove(true);

  CreateStoreDirectory();

  mTableFreshness.Clear();
  RegenActiveTables();
}

void
Classifier::TableRequest(nsACString& aResult)
{
  nsTArray<nsCString> tables;
  ActiveTables(tables);
  for (uint32_t i = 0; i < tables.Length(); i++) {
    nsAutoPtr<HashStore> store(new HashStore(tables[i], mStoreDirectory));
    if (!store)
      continue;

    nsresult rv = store->Open();
    if (NS_FAILED(rv))
      continue;

    aResult.Append(store->TableName());
    aResult.Append(';');

    ChunkSet &adds = store->AddChunks();
    ChunkSet &subs = store->SubChunks();

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
}

nsresult
Classifier::Check(const nsACString& aSpec,
                  const nsACString& aTables,
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

    // Get list of host keys to look up
    Completion hostKey;
    rv = LookupCache::GetKey(fragments[i], &hostKey, mCryptoHash);
    if (NS_FAILED(rv)) {
      // Local host on the network.
      continue;
    }

#if DEBUG && defined(PR_LOGGING)
    if (LOG_ENABLED()) {
      nsAutoCString checking;
      lookupHash.ToHexString(checking);
      LOG(("Checking fragment %s, hash %s (%X)", fragments[i].get(),
           checking.get(), lookupHash.ToUint32()));
    }
#endif
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
        result->mFresh = (age < mFreshTime);
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

#if defined(PR_LOGGING)
  PRIntervalTime clockStart = 0;
  if (LOG_ENABLED() || true) {
    clockStart = PR_IntervalNow();
  }
#endif

  LOG(("Backup before update."));

  nsresult rv = BackupTables();
  NS_ENSURE_SUCCESS(rv, rv);

  LOG(("Applying table updates."));

  for (uint32_t i = 0; i < aUpdates->Length(); i++) {
    // Previous ApplyTableUpdates() may have consumed this update..
    if ((*aUpdates)[i]) {
      // Run all updates for one table
      nsCString updateTable(aUpdates->ElementAt(i)->TableName());
      rv = ApplyTableUpdates(aUpdates, updateTable);
      if (NS_FAILED(rv)) {
        if (rv != NS_ERROR_OUT_OF_MEMORY) {
          Reset();
        }
        return rv;
      }
    }
  }
  aUpdates->Clear();

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

#if defined(PR_LOGGING)
  if (LOG_ENABLED() || true) {
    PRIntervalTime clockEnd = PR_IntervalNow();
    LOG(("update took %dms\n",
         PR_IntervalToMilliseconds(clockEnd - clockStart)));
  }
#endif

  return NS_OK;
}

nsresult
Classifier::MarkSpoiled(nsTArray<nsCString>& aTables)
{
  for (uint32_t i = 0; i < aTables.Length(); i++) {
    LOG(("Spoiling table: %s", aTables[i].get()));
    // Spoil this table by marking it as no known freshness
    mTableFreshness.Remove(aTables[i]);
    // Remove any cached Completes for this table
    LookupCache *cache = GetLookupCache(aTables[i]);
    if (cache) {
      cache->ClearCompleteCache();
    }
  }
  return NS_OK;
}

void
Classifier::DropStores()
{
  for (uint32_t i = 0; i < mHashStores.Length(); i++) {
    delete mHashStores[i];
  }
  mHashStores.Clear();
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
    nsAutoPtr<HashStore> store(new HashStore(nsCString(foundTables[i]), mStoreDirectory));
    if (!store)
      return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = store->Open();
    if (NS_FAILED(rv))
      continue;

    LookupCache *lookupCache = GetLookupCache(store->TableName());
    if (!lookupCache) {
      continue;
    }

    if (!lookupCache->IsPrimed())
      continue;

    const ChunkSet &adds = store->AddChunks();
    const ChunkSet &subs = store->SubChunks();

    if (adds.Length() == 0 && subs.Length() == 0)
      continue;

    LOG(("Active table: %s", store->TableName().get()));
    mActiveTablesCache.AppendElement(store->TableName());
  }

  return NS_OK;
}

nsresult
Classifier::ScanStoreDir(nsTArray<nsCString>& aTables)
{
  nsCOMPtr<nsISimpleEnumerator> entries;
  nsresult rv = mStoreDirectory->GetDirectoryEntries(getter_AddRefs(entries));
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
  rv = mStoreDirectory->GetNativeLeafName(storeDirName);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mStoreDirectory->MoveToNative(nullptr, backupDirName);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mStoreDirectory->CopyToNative(nullptr, storeDirName);
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
    rv = mStoreDirectory->GetNativeLeafName(storeDirName);
    NS_ENSURE_SUCCESS(rv, rv);

    bool storeExists;
    rv = mStoreDirectory->Exists(&storeExists);
    NS_ENSURE_SUCCESS(rv, rv);

    if (storeExists) {
      rv = mStoreDirectory->Remove(true);
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

/*
 * This will consume+delete updates from the passed nsTArray.
*/
nsresult
Classifier::ApplyTableUpdates(nsTArray<TableUpdate*>* aUpdates,
                              const nsACString& aTable)
{
  LOG(("Classifier::ApplyTableUpdates(%s)", PromiseFlatCString(aTable).get()));

  nsAutoPtr<HashStore> store(new HashStore(aTable, mStoreDirectory));

  if (!store)
    return NS_ERROR_FAILURE;

  // take the quick exit if there is no valid update for us
  // (common case)
  uint32_t validupdates = 0;

  for (uint32_t i = 0; i < aUpdates->Length(); i++) {
    TableUpdate *update = aUpdates->ElementAt(i);
    if (!update || !update->TableName().Equals(store->TableName()))
      continue;
    if (update->Empty()) {
      aUpdates->ElementAt(i) = nullptr;
      delete update;
      continue;
    }
    validupdates++;
  }

  if (!validupdates) {
    // This can happen if the update was only valid for one table.
    return NS_OK;
  }

  nsresult rv = store->Open();
  NS_ENSURE_SUCCESS(rv, rv);
  rv = store->BeginUpdate();
  NS_ENSURE_SUCCESS(rv, rv);

  // Read the part of the store that is (only) in the cache
  LookupCache *prefixSet = GetLookupCache(store->TableName());
  if (!prefixSet) {
    return NS_ERROR_FAILURE;
  }
  nsTArray<uint32_t> AddPrefixHashes;
  rv = prefixSet->GetPrefixes(&AddPrefixHashes);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = store->AugmentAdds(AddPrefixHashes);
  NS_ENSURE_SUCCESS(rv, rv);
  AddPrefixHashes.Clear();

  uint32_t applied = 0;
  bool updateFreshness = false;
  bool hasCompletes = false;

  for (uint32_t i = 0; i < aUpdates->Length(); i++) {
    TableUpdate *update = aUpdates->ElementAt(i);
    if (!update || !update->TableName().Equals(store->TableName()))
      continue;

    rv = store->ApplyUpdate(*update);
    NS_ENSURE_SUCCESS(rv, rv);

    applied++;

    LOG(("Applied update to table %s:", store->TableName().get()));
    LOG(("  %d add chunks", update->AddChunks().Length()));
    LOG(("  %d add prefixes", update->AddPrefixes().Length()));
    LOG(("  %d add completions", update->AddCompletes().Length()));
    LOG(("  %d sub chunks", update->SubChunks().Length()));
    LOG(("  %d sub prefixes", update->SubPrefixes().Length()));
    LOG(("  %d sub completions", update->SubCompletes().Length()));
    LOG(("  %d add expirations", update->AddExpirations().Length()));
    LOG(("  %d sub expirations", update->SubExpirations().Length()));

    if (!update->IsLocalUpdate()) {
      updateFreshness = true;
      LOG(("Remote update, updating freshness"));
    }

    if (update->AddCompletes().Length() > 0
        || update->SubCompletes().Length() > 0) {
      hasCompletes = true;
      LOG(("Contains Completes, keeping cache."));
    }

    aUpdates->ElementAt(i) = nullptr;
    delete update;
  }

  LOG(("Applied %d update(s) to %s.", applied, store->TableName().get()));

  rv = store->Rebuild();
  NS_ENSURE_SUCCESS(rv, rv);

  // Not an update with Completes, clear all completes data.
  if (!hasCompletes) {
    store->ClearCompletes();
  }

  LOG(("Table %s now has:", store->TableName().get()));
  LOG(("  %d add chunks", store->AddChunks().Length()));
  LOG(("  %d add prefixes", store->AddPrefixes().Length()));
  LOG(("  %d add completions", store->AddCompletes().Length()));
  LOG(("  %d sub chunks", store->SubChunks().Length()));
  LOG(("  %d sub prefixes", store->SubPrefixes().Length()));
  LOG(("  %d sub completions", store->SubCompletes().Length()));

  rv = store->WriteFile();
  NS_ENSURE_SUCCESS(rv, rv);

  // At this point the store is updated and written out to disk, but
  // the data is still in memory.  Build our quick-lookup table here.
  rv = prefixSet->Build(store->AddPrefixes(), store->AddCompletes());
  NS_ENSURE_SUCCESS(rv, rv);

#if defined(DEBUG) && defined(PR_LOGGING)
  prefixSet->Dump();
#endif
  rv = prefixSet->WriteFile();
  NS_ENSURE_SUCCESS(rv, rv);

  if (updateFreshness) {
    int64_t now = (PR_Now() / PR_USEC_PER_SEC);
    LOG(("Successfully updated %s", store->TableName().get()));
    mTableFreshness.Put(store->TableName(), now);
  }

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

  LookupCache *cache = new LookupCache(aTable, mStoreDirectory);
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
  mLookupCaches.AppendElement(cache);
  return cache;
}

nsresult
Classifier::ReadNoiseEntries(const Prefix& aPrefix,
                             const nsACString& aTableName,
                             uint32_t aCount,
                             PrefixArray* aNoiseEntries)
{
  LookupCache *cache = GetLookupCache(aTableName);
  if (!cache) {
    return NS_ERROR_FAILURE;
  }

  nsTArray<uint32_t> prefixes;
  nsresult rv = cache->GetPrefixes(&prefixes);
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

} // namespace safebrowsing
} // namespace mozilla
