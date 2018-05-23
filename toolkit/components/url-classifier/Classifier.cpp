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
#include "mozilla/EndianUtils.h"
#include "mozilla/Telemetry.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Logging.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/Base64.h"
#include "mozilla/Unused.h"
#include "mozilla/UniquePtr.h"
#include "nsIUrlClassifierUtils.h"
#include "nsUrlClassifierDBService.h"

// MOZ_LOG=UrlClassifierDbService:5
extern mozilla::LazyLogModule gUrlClassifierDbServiceLog;
#define LOG(args) MOZ_LOG(gUrlClassifierDbServiceLog, mozilla::LogLevel::Debug, args)
#define LOG_ENABLED() MOZ_LOG_TEST(gUrlClassifierDbServiceLog, mozilla::LogLevel::Debug)

#define STORE_DIRECTORY      NS_LITERAL_CSTRING("safebrowsing")
#define TO_DELETE_DIR_SUFFIX NS_LITERAL_CSTRING("-to_delete")
#define BACKUP_DIR_SUFFIX    NS_LITERAL_CSTRING("-backup")
#define UPDATING_DIR_SUFFIX  NS_LITERAL_CSTRING("-updating")

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
  nsTArray<UniquePtr<TableUpdate>> mUpdatesPointerHolder;
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

nsresult
Classifier::GetPrivateStoreDirectory(nsIFile* aRootStoreDirectory,
                                     const nsACString& aTableName,
                                     const nsACString& aProvider,
                                     nsIFile** aPrivateStoreDirectory)
{
  NS_ENSURE_ARG_POINTER(aPrivateStoreDirectory);

  if (!StringEndsWith(aTableName, NS_LITERAL_CSTRING("-proto"))) {
    // Only V4 table names (ends with '-proto') would be stored
    // to per-provider sub-directory.
    nsCOMPtr<nsIFile>(aRootStoreDirectory).forget(aPrivateStoreDirectory);
    return NS_OK;
  }

  if (aProvider.IsEmpty()) {
    // When failing to get provider, just store in the root folder.
    nsCOMPtr<nsIFile>(aRootStoreDirectory).forget(aPrivateStoreDirectory);
    return NS_OK;
  }

  nsCOMPtr<nsIFile> providerDirectory;

  // Clone first since we are gonna create a new directory.
  nsresult rv = aRootStoreDirectory->Clone(getter_AddRefs(providerDirectory));
  NS_ENSURE_SUCCESS(rv, rv);

  // Append the provider name to the root store directory.
  rv = providerDirectory->AppendNative(aProvider);
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
  : mIsTableRequestResultOutdated(true)
  , mUpdateInterrupted(true)
{
  NS_NewNamedThread(NS_LITERAL_CSTRING("Classifier Update"),
                    getter_AddRefs(mUpdateThread));
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

  // Directory where to be working on the update.
  rv = mCacheDirectory->Clone(getter_AddRefs(mUpdatingDirectory));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mUpdatingDirectory->AppendNative(STORE_DIRECTORY + UPDATING_DIR_SUFFIX);
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
  // This is still required for backward compatibility.
  rv = CleanToDelete();
  NS_ENSURE_SUCCESS(rv, rv);

  // If we met a crash during the previous update, "safebrowsing-updating"
  // directory will exist and let's remove it.
  rv = mUpdatingDirectory->Remove(true);
  if (NS_SUCCEEDED(rv)) {
    // If the "safebrowsing-updating" exists, it implies a crash occurred
    // in the previous update.
    LOG(("We may have hit a crash in the previous update."));
  }

  // Check whether we have an incomplete update and recover from the
  // backup if so.
  rv = RecoverBackups();
  NS_ENSURE_SUCCESS(rv, rv);

  // Make sure the main store directory exists.
  rv = CreateStoreDirectory();
  NS_ENSURE_SUCCESS(rv, rv);

  // Build the list of know urlclassifier lists
  // XXX: Disk IO potentially on the main thread during startup
  RegenActiveTables();

  return NS_OK;
}

void
Classifier::Close()
{
  // Close will be called by PreShutdown, so it is important to note that
  // things put here should not affect an ongoing update thread.
  DropStores();
}

void
Classifier::Reset()
{
  MOZ_ASSERT(NS_GetCurrentThread() != mUpdateThread,
             "Reset() MUST NOT be called on update thread");

  LOG(("Reset() is called so we interrupt the update."));
  mUpdateInterrupted = true;

  auto resetFunc = [=] {
    DropStores();

    mRootStoreDirectory->Remove(true);
    mBackupDirectory->Remove(true);
    mUpdatingDirectory->Remove(true);
    mToDeleteDirectory->Remove(true);

    CreateStoreDirectory();

    RegenActiveTables();
  };

  if (!mUpdateThread) {
    LOG(("Async update has been disabled. Just Reset() on worker thread."));
    resetFunc();
    return;
  }

  nsCOMPtr<nsIRunnable> r =
    NS_NewRunnableFunction("safebrowsing::Classifier::Reset", resetFunc);
  SyncRunnable::DispatchToThread(mUpdateThread, r);
}

void
Classifier::ResetTables(ClearType aType, const nsTArray<nsCString>& aTables)
{
  for (uint32_t i = 0; i < aTables.Length(); i++) {
    LOG(("Resetting table: %s", aTables[i].get()));
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
  nsCOMPtr<nsIDirectoryEnumerator> entries;
  nsresult rv = aDirectory->GetDirectoryEntries(getter_AddRefs(entries));
  NS_ENSURE_SUCCESS_VOID(rv);

  nsCOMPtr<nsIFile> file;
  while (NS_SUCCEEDED(rv = entries->GetNextFile(getter_AddRefs(file))) && file) {
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

    // Remove file extension if there's one.
    int32_t dotPosition = leafName.RFind(".");
    if (dotPosition >= 0) {
      leafName.Truncate(dotPosition);
    }

    if (!leafName.IsEmpty() && aTables.Contains(leafName)) {
      if (NS_FAILED(file->Remove(false))) {
        NS_WARNING(nsPrintfCString("Fail to remove file %s from the disk",
                                   leafName.get()).get());
      }
    }
  }
  NS_ENSURE_SUCCESS_VOID(rv);
}

void
Classifier::TableRequest(nsACString& aResult)
{
  MOZ_ASSERT(!NS_IsMainThread(),
             "TableRequest must be called on the classifier worker thread.");

  // This function and all disk I/O are guaranteed to occur
  // on the same thread so we don't need to add a lock around.
  if (!mIsTableRequestResultOutdated) {
    aResult = mTableRequestResult;
    return;
  }

  // Generating v2 table info.
  nsTArray<nsCString> tables;
  ActiveTables(tables);
  for (uint32_t i = 0; i < tables.Length(); i++) {
    HashStore store(tables[i], GetProvider(tables[i]), mRootStoreDirectory);

    nsresult rv = store.Open();
    if (NS_FAILED(rv)) {
      continue;
    }

    ChunkSet &adds = store.AddChunks();
    ChunkSet &subs = store.SubChunks();

    // Open HashStore will always succeed even that is not a v2 table.
    // So skip tables without add and sub chunks.
    if (adds.Length() == 0 && subs.Length() == 0) {
      continue;
    }

    aResult.Append(store.TableName());
    aResult.Append(';');

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
  if (NS_SUCCEEDED(rv)) {
    aResult.Append(metadata);
  }

  // Update the TableRequest result in-memory cache.
  mTableRequestResult = aResult;
  mIsTableRequestResultOutdated = false;
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
    lookupHash.FromPlaintext(fragments[i]);

    if (LOG_ENABLED()) {
      nsAutoCString checking;
      lookupHash.ToHexString(checking);
      LOG(("Checking fragment %s, hash %s (%X)", fragments[i].get(),
           checking.get(), lookupHash.ToUint32()));
    }

    for (uint32_t i = 0; i < cacheArray.Length(); i++) {
      LookupCache *cache = cacheArray[i];
      bool has, confirmed;
      uint32_t matchLength;

      rv = cache->Has(lookupHash, &has, &matchLength, &confirmed);
      NS_ENSURE_SUCCESS(rv, rv);

      if (has) {
        LookupResult *result = aResults.AppendElement(fallible);
        if (!result) {
          return NS_ERROR_OUT_OF_MEMORY;
        }
        LOG(("Found a result in %s: %s",
             cache->TableName().get(),
             confirmed ? "confirmed." : "Not confirmed."));

        result->hash.complete = lookupHash;
        result->mConfirmed = confirmed;
        result->mTableName.Assign(cache->TableName());
        result->mPartialHashLength = confirmed ? COMPLETE_SIZE : matchLength;
        result->mProtocolV2 = LookupCache::Cast<LookupCacheV2>(cache);
      }
    }
  }

  return NS_OK;
}

static nsresult
SwapDirectoryContent(nsIFile* aDir1,
                     nsIFile* aDir2,
                     nsIFile* aParentDir,
                     nsIFile* aTempDir)
{
  // Pre-condition: |aDir1| and |aDir2| are directory and their parent
  //                are both |aParentDir|.
  //
  // Post-condition: The locations where aDir1 and aDir2 point to will not
  //                 change but their contents will be exchanged. If we failed
  //                 to swap their content, everything will be rolled back.

  nsAutoCString tempDirName;
  aTempDir->GetNativeLeafName(tempDirName);

  nsresult rv;

  nsAutoCString dirName1, dirName2;
  aDir1->GetNativeLeafName(dirName1);
  aDir2->GetNativeLeafName(dirName2);

  LOG(("Swapping directories %s and %s...", dirName1.get(),
                                            dirName2.get()));

  // 1. Rename "dirName1" to "temp"
  rv = aDir1->RenameToNative(nullptr, tempDirName);
  if (NS_FAILED(rv)) {
    LOG(("Unable to rename %s to %s", dirName1.get(),
                                      tempDirName.get()));
    return rv; // Nothing to roll back.
  }

  // 1.1. Create a handle for temp directory. This is required since
  //      |nsIFile.rename| will not change the location where the
  //      object points to.
  nsCOMPtr<nsIFile> tempDirectory;
  rv = aParentDir->Clone(getter_AddRefs(tempDirectory));
  rv = tempDirectory->AppendNative(tempDirName);

  // 2. Rename "dirName2" to "dirName1".
  rv = aDir2->RenameToNative(nullptr, dirName1);
  if (NS_FAILED(rv)) {
    LOG(("Failed to rename %s to %s. Rename temp directory back to %s",
         dirName2.get(), dirName1.get(), dirName1.get()));
    nsresult rbrv = tempDirectory->RenameToNative(nullptr, dirName1);
    NS_ENSURE_SUCCESS(rbrv, rbrv);
    return rv;
  }

  // 3. Rename "temp" to "dirName2".
  rv = tempDirectory->RenameToNative(nullptr, dirName2);
  if (NS_FAILED(rv)) {
    LOG(("Failed to rename temp directory to %s. ", dirName2.get()));
    // We've done (1) renaming "dir1 to temp" and
    //            (2) renaming "dir2 to dir1"
    // so the rollback is
    //            (1) renaming "dir1 to dir2" and
    //            (2) renaming "temp to dir1"
    nsresult rbrv; // rollback result
    rbrv = aDir1->RenameToNative(nullptr, dirName2);
    NS_ENSURE_SUCCESS(rbrv, rbrv);
    rbrv = tempDirectory->RenameToNative(nullptr, dirName1);
    NS_ENSURE_SUCCESS(rbrv, rbrv);
    return rv;
  }

  return rv;
}

void
Classifier::RemoveUpdateIntermediaries()
{
  // Remove old LookupCaches.
  for (auto c: mNewLookupCaches) {
    delete c;
  }
  mNewLookupCaches.Clear();

  // Remove the "old" directory. (despite its looking-new name)
  if (NS_FAILED(mUpdatingDirectory->Remove(true))) {
    // If the directory is locked from removal for some reason,
    // we will fail here and it doesn't matter until the next
    // update. (the next udpate will fail due to the removable
    // "safebrowsing-udpating" directory.)
    LOG(("Failed to remove updating directory."));
  }
}

void
Classifier::CopyAndInvalidateFullHashCache()
{
  MOZ_ASSERT(NS_GetCurrentThread() != mUpdateThread,
             "CopyAndInvalidateFullHashCache cannot be called on update thread "
             "since it mutates mLookupCaches which is only safe on "
             "worker thread.");

  // New lookup caches are built from disk, data likes cache which is
  // generated online won't exist. We have to manually copy cache from
  // old LookupCache to new LookupCache.
  for (auto& newCache: mNewLookupCaches) {
    for (auto& oldCache: mLookupCaches) {
      if (oldCache->TableName() == newCache->TableName()) {
        newCache->CopyFullHashCache(oldCache);
        break;
      }
    }
  }

  // Clear cache when update.
  // Invalidate cache entries in CopyAndInvalidateFullHashCache because only
  // at this point we will have cache data in LookupCache.
  for (auto& newCache: mNewLookupCaches) {
    newCache->InvalidateExpiredCacheEntries();
  }
}

void
Classifier::MergeNewLookupCaches()
{
  MOZ_ASSERT(NS_GetCurrentThread() != mUpdateThread,
             "MergeNewLookupCaches cannot be called on update thread "
             "since it mutates mLookupCaches which is only safe on "
             "worker thread.");

  for (auto& newCache: mNewLookupCaches) {
    // For each element in mNewLookCaches, it will be swapped with
    //   - An old cache in mLookupCache with the same table name or
    //   - nullptr (mLookupCache will be expaned) otherwise.
    size_t swapIndex = 0;
    for (; swapIndex < mLookupCaches.Length(); swapIndex++) {
      if (mLookupCaches[swapIndex]->TableName() == newCache->TableName()) {
        break;
      }
    }
    if (swapIndex == mLookupCaches.Length()) {
      mLookupCaches.AppendElement(nullptr);
    }

    Swap(mLookupCaches[swapIndex], newCache);
    mLookupCaches[swapIndex]->UpdateRootDirHandle(mRootStoreDirectory);
  }

  // At this point, mNewLookupCaches's length remains the same but
  // will contain either old cache (override) or nullptr (append).
}

nsresult
Classifier::SwapInNewTablesAndCleanup()
{
  nsresult rv;

  // Step 1. Swap in on-disk tables. The idea of using "safebrowsing-backup"
  // as the intermediary directory is we can get databases recovered if
  // crash occurred in any step of the swap. (We will recover from
  // "safebrowsing-backup" in OpenDb().)
  rv = SwapDirectoryContent(mUpdatingDirectory,  // contains new tables
                            mRootStoreDirectory, // contains old tables
                            mCacheDirectory,     // common parent dir
                            mBackupDirectory);   // intermediary dir for swap
  if (NS_FAILED(rv)) {
    LOG(("Failed to swap in on-disk tables."));
    RemoveUpdateIntermediaries();
    return rv;
  }

  // Step 2. Merge mNewLookupCaches into mLookupCaches. The outdated
  // LookupCaches will be stored in mNewLookupCaches and be cleaned
  // up later.
  MergeNewLookupCaches();

  // Step 3. Re-generate active tables based on on-disk tables.
  rv = RegenActiveTables();
  if (NS_FAILED(rv)) {
    LOG(("Failed to re-generate active tables!"));
  }

  // Step 4. Clean up intermediaries for update.
  RemoveUpdateIntermediaries();

  // Step 5. Invalidate cached tableRequest request.
  mIsTableRequestResultOutdated = true;

  LOG(("Done swap in updated tables."));

  return rv;
}

void Classifier::FlushAndDisableAsyncUpdate()
{
  LOG(("Classifier::FlushAndDisableAsyncUpdate [%p, %p]", this, mUpdateThread.get()));

  if (!mUpdateThread) {
    LOG(("Async update has been disabled."));
    return;
  }

  mUpdateThread->Shutdown();
  mUpdateThread = nullptr;
}

nsresult
Classifier::AsyncApplyUpdates(nsTArray<TableUpdate*>* aUpdates,
                              const AsyncUpdateCallback& aCallback)
{
  LOG(("Classifier::AsyncApplyUpdates"));

  if (!mUpdateThread) {
    LOG(("Async update has already been disabled."));
    return NS_ERROR_FAILURE;
  }

  //         Caller thread      |       Update thread
  // --------------------------------------------------------
  //                            |    ApplyUpdatesBackground
  //    (processing other task) |    (bg-update done. ping back to caller thread)
  //    (processing other task) |    idle...
  //    ApplyUpdatesForeground  |
  //          callback          |

  mUpdateInterrupted = false;
  nsresult rv = mRootStoreDirectory->Clone(getter_AddRefs(mRootStoreDirectoryForUpdate));
  if (NS_FAILED(rv)) {
    LOG(("Failed to clone mRootStoreDirectory for update."));
    return rv;
  }

  nsCOMPtr<nsIThread> callerThread = NS_GetCurrentThread();
  MOZ_ASSERT(callerThread != mUpdateThread);

  nsCOMPtr<nsIRunnable> bgRunnable =
    NS_NewRunnableFunction("safebrowsing::Classifier::AsyncApplyUpdates", [=] {
      MOZ_ASSERT(NS_GetCurrentThread() == mUpdateThread,
                 "MUST be on update thread");

      LOG(("Step 1. ApplyUpdatesBackground on update thread."));
      nsCString failedTableName;
      nsresult bgRv = ApplyUpdatesBackground(aUpdates, failedTableName);

      nsCOMPtr<nsIRunnable> fgRunnable = NS_NewRunnableFunction(
        "safebrowsing::Classifier::AsyncApplyUpdates", [=] {
          MOZ_ASSERT(NS_GetCurrentThread() == callerThread,
                     "MUST be on caller thread");

          LOG(("Step 2. ApplyUpdatesForeground on caller thread"));
          nsresult rv = ApplyUpdatesForeground(bgRv, failedTableName);
          ;

          LOG(("Step 3. Updates applied! Fire callback."));

          aCallback(rv);
        });
      callerThread->Dispatch(fgRunnable, NS_DISPATCH_NORMAL);
    });

  return mUpdateThread->Dispatch(bgRunnable, NS_DISPATCH_NORMAL);
}

nsresult
Classifier::ApplyUpdatesBackground(nsTArray<TableUpdate*>* aUpdates,
                                   nsACString& aFailedTableName)
{
  // |mUpdateInterrupted| is guaranteed to have been unset.
  // If |mUpdateInterrupted| is set at any point, Reset() must have
  // been called then we need to interrupt the update process.
  // We only add checkpoints for non-trivial tasks.

  if (!aUpdates || aUpdates->Length() == 0) {
    return NS_OK;
  }

  nsCOMPtr<nsIUrlClassifierUtils> urlUtil =
    do_GetService(NS_URLCLASSIFIERUTILS_CONTRACTID);

  nsCString provider;
  // Assume all TableUpdate objects should have the same provider.
  urlUtil->GetTelemetryProvider((*aUpdates)[0]->TableName(), provider);

  Telemetry::AutoTimer<Telemetry::URLCLASSIFIER_CL_KEYED_UPDATE_TIME>
    keyedTimer(provider);

  PRIntervalTime clockStart = 0;
  if (LOG_ENABLED()) {
    clockStart = PR_IntervalNow();
  }

  nsresult rv;

  {
    ScopedUpdatesClearer scopedUpdatesClearer(aUpdates);

    {
      // Check point 1: Copying file takes time so we check here.
      if (mUpdateInterrupted) {
        LOG(("Update is interrupted. Don't copy files."));
        return NS_OK;
      }

      rv = CopyInUseDirForUpdate(); // i.e. mUpdatingDirectory will be setup.
      if (NS_FAILED(rv)) {
        LOG(("Failed to copy in-use directory for update."));
        return rv;
      }
    }

    LOG(("Applying %zu table updates.", aUpdates->Length()));

    for (uint32_t i = 0; i < aUpdates->Length(); i++) {
      // Previous UpdateHashStore() may have consumed this update..
      if ((*aUpdates)[i]) {
        // Run all updates for one table
        nsCString updateTable(aUpdates->ElementAt(i)->TableName());

        // Check point 2: Processing downloaded data takes time.
        if (mUpdateInterrupted) {
          LOG(("Update is interrupted. Stop building new tables."));
          return NS_OK;
        }

        // Will update the mirrored in-memory and on-disk databases.
        if (TableUpdate::Cast<TableUpdateV2>((*aUpdates)[i])) {
          rv = UpdateHashStore(aUpdates, updateTable);
        } else {
          rv = UpdateTableV4(aUpdates, updateTable);
        }

        if (NS_FAILED(rv)) {
          aFailedTableName = updateTable;
          RemoveUpdateIntermediaries();
          return rv;
        }
      }
    }

  } // End of scopedUpdatesClearer scope.

  if (LOG_ENABLED()) {
    PRIntervalTime clockEnd = PR_IntervalNow();
    LOG(("update took %dms\n",
         PR_IntervalToMilliseconds(clockEnd - clockStart)));
  }

  return rv;
}

nsresult
Classifier::ApplyUpdatesForeground(nsresult aBackgroundRv,
                                   const nsACString& aFailedTableName)
{
  if (mUpdateInterrupted) {
    LOG(("Update is interrupted! Just remove update intermediaries."));
    RemoveUpdateIntermediaries();
    return NS_OK;
  }
  if (NS_SUCCEEDED(aBackgroundRv)) {
    // Copy and Invalidate fullhash cache here because this call requires
    // mLookupCaches which is only available on work-thread
    CopyAndInvalidateFullHashCache();

    return SwapInNewTablesAndCleanup();
  }
  if (NS_ERROR_OUT_OF_MEMORY != aBackgroundRv) {
    ResetTables(Clear_All, nsTArray<nsCString> { nsCString(aFailedTableName) });
  }
  return aBackgroundRv;
}

nsresult
Classifier::ApplyFullHashes(nsTArray<TableUpdate*>* aUpdates)
{
  LOG(("Applying %zu table gethashes.", aUpdates->Length()));

  ScopedUpdatesClearer scopedUpdatesClearer(aUpdates);
  for (uint32_t i = 0; i < aUpdates->Length(); i++) {
    TableUpdate *update = aUpdates->ElementAt(i);

    nsresult rv = UpdateCache(update);
    NS_ENSURE_SUCCESS(rv, rv);

    aUpdates->ElementAt(i) = nullptr;
  }

  return NS_OK;
}

void
Classifier::GetCacheInfo(const nsACString& aTable,
                         nsIUrlClassifierCacheInfo** aCache)
{
  LookupCache* lookupCache = GetLookupCache(aTable);
  if (!lookupCache) {
    return;
  }

  lookupCache->GetCacheInfo(aCache);
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
  ScanStoreDir(mRootStoreDirectory, foundTables);

  for (uint32_t i = 0; i < foundTables.Length(); i++) {
    nsCString table(foundTables[i]);

    LookupCache *lookupCache = GetLookupCache(table);
    if (!lookupCache) {
      LOG(("Inactive table (no cache): %s", table.get()));
      continue;
    }

    if (!lookupCache->IsPrimed()) {
      LOG(("Inactive table (cache not primed): %s", table.get()));
      continue;
    }

    if (LookupCache::Cast<LookupCacheV4>(lookupCache)) {
      LOG(("Active v4 table: %s", table.get()));
    } else {
      HashStore store(table, GetProvider(table), mRootStoreDirectory);

      nsresult rv = store.Open();
      if (NS_FAILED(rv)) {
        continue;
      }

      const ChunkSet &adds = store.AddChunks();
      const ChunkSet &subs = store.SubChunks();

      if (adds.Length() == 0 && subs.Length() == 0) {
        continue;
      }

      LOG(("Active v2 table: %s", store.TableName().get()));
    }

    mActiveTablesCache.AppendElement(table);
  }

  return NS_OK;
}

nsresult
Classifier::ScanStoreDir(nsIFile* aDirectory, nsTArray<nsCString>& aTables)
{
  nsCOMPtr<nsIDirectoryEnumerator> entries;
  nsresult rv = aDirectory->GetDirectoryEntries(getter_AddRefs(entries));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFile> file;
  while (NS_SUCCEEDED(rv = entries->GetNextFile(getter_AddRefs(file))) && file) {
    // If |file| is a directory, recurse to find its entries as well.
    bool isDirectory;
    if (NS_FAILED(file->IsDirectory(&isDirectory))) {
      continue;
    }
    if (isDirectory) {
      ScanStoreDir(file, aTables);
      continue;
    }

    nsCString leafName;
    rv = file->GetNativeLeafName(leafName);
    NS_ENSURE_SUCCESS(rv, rv);

    // Both v2 and v4 contain .pset file
    nsCString suffix(NS_LITERAL_CSTRING(".pset"));

    int32_t dot = leafName.RFind(suffix);
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

#ifdef MOZ_SAFEBROWSING_DUMP_FAILED_UPDATES

already_AddRefed<nsIFile>
Classifier::GetFailedUpdateDirectroy()
{
  nsCString failedUpdatekDirName = STORE_DIRECTORY + nsCString("-failedupdate");

  nsCOMPtr<nsIFile> failedUpdatekDirectory;
  if (NS_FAILED(mCacheDirectory->Clone(getter_AddRefs(failedUpdatekDirectory))) ||
      NS_FAILED(failedUpdatekDirectory->AppendNative(failedUpdatekDirName))) {
    LOG(("Failed to init failedUpdatekDirectory."));
    return nullptr;
  }

  return failedUpdatekDirectory.forget();
}

nsresult
Classifier::DumpRawTableUpdates(const nsACString& aRawUpdates)
{
  LOG(("Dumping raw table updates..."));

  DumpFailedUpdate();

  nsCOMPtr<nsIFile> failedUpdatekDirectory = GetFailedUpdateDirectroy();

  // Create tableupdate.bin and dump raw table update data.
  nsCOMPtr<nsIFile> rawTableUpdatesFile;
  nsCOMPtr<nsIOutputStream> outputStream;
  if (NS_FAILED(failedUpdatekDirectory->Clone(getter_AddRefs(rawTableUpdatesFile))) ||
      NS_FAILED(rawTableUpdatesFile->AppendNative(nsCString("tableupdates.bin"))) ||
      NS_FAILED(NS_NewLocalFileOutputStream(getter_AddRefs(outputStream),
                                            rawTableUpdatesFile,
                                            PR_WRONLY | PR_TRUNCATE | PR_CREATE_FILE))) {
    LOG(("Failed to create file to dump raw table updates."));
    return NS_ERROR_FAILURE;
  }

  // Write out the data.
  uint32_t written;
  nsresult rv = outputStream->Write(aRawUpdates.BeginReading(),
                                    aRawUpdates.Length(), &written);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(written == aRawUpdates.Length(), NS_ERROR_FAILURE);

  return rv;
}

nsresult
Classifier::DumpFailedUpdate()
{
  LOG(("Dumping failed update..."));

  nsCOMPtr<nsIFile> failedUpdatekDirectory = GetFailedUpdateDirectroy();

  // Remove the "failed update" directory no matter it exists or not.
  // Failure is fine because the directory may not exist.
  failedUpdatekDirectory->Remove(true);

  nsCString failedUpdatekDirName;
  nsresult rv = failedUpdatekDirectory->GetNativeLeafName(failedUpdatekDirName);
  NS_ENSURE_SUCCESS(rv, rv);

  // Copy the in-use directory to a clean "failed update" directory.
  nsCOMPtr<nsIFile> inUseDirectory;
  if (NS_FAILED(mRootStoreDirectory->Clone(getter_AddRefs(inUseDirectory))) ||
      NS_FAILED(inUseDirectory->CopyToNative(nullptr, failedUpdatekDirName))) {
    LOG(("Failed to move in-use to the \"failed update\" directory %s",
         failedUpdatekDirName.get()));
    return NS_ERROR_FAILURE;
  }

  return rv;
}

#endif // MOZ_SAFEBROWSING_DUMP_FAILED_UPDATES

nsresult
Classifier::CopyInUseDirForUpdate()
{
  LOG(("Copy in-use directory content for update."));

  // We copy everything from in-use directory to a temporary directory
  // for updating.

  nsCString updatingDirName;
  nsresult rv = mUpdatingDirectory->GetNativeLeafName(updatingDirName);
  NS_ENSURE_SUCCESS(rv, rv);

  // Remove the destination directory first (just in case) the do the copy.
  mUpdatingDirectory->Remove(true);
  if (!mRootStoreDirectoryForUpdate) {
    LOG(("mRootStoreDirectoryForUpdate is null."));
    return NS_ERROR_NULL_POINTER;
  }
  rv = mRootStoreDirectoryForUpdate->CopyToNative(nullptr, updatingDirName);
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

nsCString
Classifier::GetProvider(const nsACString& aTableName)
{
  nsCOMPtr<nsIUrlClassifierUtils> urlUtil =
    do_GetService(NS_URLCLASSIFIERUTILS_CONTRACTID);

  nsCString provider;
  nsresult rv = urlUtil->GetProvider(aTableName, provider);

  return NS_SUCCEEDED(rv) ? provider : EmptyCString();
}

/*
 * This will consume+delete updates from the passed nsTArray.
*/
nsresult
Classifier::UpdateHashStore(nsTArray<TableUpdate*>* aUpdates,
                            const nsACString& aTable)
{
  if (nsUrlClassifierDBService::ShutdownHasStarted()) {
    return NS_ERROR_UC_UPDATE_SHUTDOWNING;
  }

  LOG(("Classifier::UpdateHashStore(%s)", PromiseFlatCString(aTable).get()));

  HashStore store(aTable, GetProvider(aTable), mUpdatingDirectory);

  if (!CheckValidUpdate(aUpdates, store.TableName())) {
    return NS_OK;
  }

  nsresult rv = store.Open();
  NS_ENSURE_SUCCESS(rv, rv);
  rv = store.BeginUpdate();
  NS_ENSURE_SUCCESS(rv, rv);

  // Read the part of the store that is (only) in the cache
  LookupCacheV2* lookupCache =
    LookupCache::Cast<LookupCacheV2>(GetLookupCacheForUpdate(store.TableName()));
  if (!lookupCache) {
    return NS_ERROR_UC_UPDATE_TABLE_NOT_FOUND;
  }

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
      LOG(("  %zu add prefixes", updateV2->AddPrefixes().Length()));
      LOG(("  %zu add completions", updateV2->AddCompletes().Length()));
      LOG(("  %d sub chunks", updateV2->SubChunks().Length()));
      LOG(("  %zu sub prefixes", updateV2->SubPrefixes().Length()));
      LOG(("  %zu sub completions", updateV2->SubCompletes().Length()));
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
  LOG(("  %zu add prefixes", store.AddPrefixes().Length()));
  LOG(("  %zu add completions", store.AddCompletes().Length()));
  LOG(("  %d sub chunks", store.SubChunks().Length()));
  LOG(("  %zu sub prefixes", store.SubPrefixes().Length()));
  LOG(("  %zu sub completions", store.SubCompletes().Length()));

  rv = store.WriteFile();
  NS_ENSURE_SUCCESS(rv, rv);

  // At this point the store is updated and written out to disk, but
  // the data is still in memory.  Build our quick-lookup table here.
  rv = lookupCache->Build(store.AddPrefixes(), store.AddCompletes());
  NS_ENSURE_SUCCESS(rv, NS_ERROR_UC_UPDATE_BUILD_PREFIX_FAILURE);

#if defined(DEBUG)
  lookupCache->DumpCompletions();
#endif
  rv = lookupCache->WriteFile();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_UC_UPDATE_FAIL_TO_WRITE_DISK);

  LOG(("Successfully updated %s", store.TableName().get()));

  return NS_OK;
}

nsresult
Classifier::UpdateTableV4(nsTArray<TableUpdate*>* aUpdates,
                          const nsACString& aTable)
{
  MOZ_ASSERT(!NS_IsMainThread(),
             "UpdateTableV4 must be called on the classifier worker thread.");
  if (nsUrlClassifierDBService::ShutdownHasStarted()) {
    return NS_ERROR_UC_UPDATE_SHUTDOWNING;
  }

  LOG(("Classifier::UpdateTableV4(%s)", PromiseFlatCString(aTable).get()));

  if (!CheckValidUpdate(aUpdates, aTable)) {
    return NS_OK;
  }

  LookupCacheV4* lookupCache =
    LookupCache::Cast<LookupCacheV4>(GetLookupCacheForUpdate(aTable));
  if (!lookupCache) {
    return NS_ERROR_UC_UPDATE_TABLE_NOT_FOUND;
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
    NS_ENSURE_TRUE(updateV4, NS_ERROR_UC_UPDATE_TABLE_NOT_FOUND);

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
  NS_ENSURE_SUCCESS(rv, NS_ERROR_UC_UPDATE_BUILD_PREFIX_FAILURE);

  rv = lookupCache->WriteFile();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_UC_UPDATE_FAIL_TO_WRITE_DISK);

  if (lastAppliedUpdate) {
    LOG(("Write meta data of the last applied update."));
    rv = lookupCache->WriteMetadata(lastAppliedUpdate);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_UC_UPDATE_FAIL_TO_WRITE_DISK);
  }

  LOG(("Successfully updated %s\n", PromiseFlatCString(aTable).get()));

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

  auto lookupV2 = LookupCache::Cast<LookupCacheV2>(lookupCache);
  if (lookupV2) {
    auto updateV2 = TableUpdate::Cast<TableUpdateV2>(aUpdate);
    lookupV2->AddGethashResultToCache(updateV2->AddCompletes(),
                                      updateV2->MissPrefixes());
  } else {
    auto lookupV4 = LookupCache::Cast<LookupCacheV4>(lookupCache);
    if (!lookupV4) {
      return NS_ERROR_FAILURE;
    }

    auto updateV4 = TableUpdate::Cast<TableUpdateV4>(aUpdate);
    lookupV4->AddFullHashResponseToCache(updateV4->FullHashResponse());
  }

#if defined(DEBUG)
  lookupCache->DumpCache();
#endif

  return NS_OK;
}

LookupCache *
Classifier::GetLookupCache(const nsACString& aTable, bool aForUpdate)
{
  // GetLookupCache(aForUpdate==true) can only be called on update thread.
  MOZ_ASSERT_IF(aForUpdate, NS_GetCurrentThread() == mUpdateThread);

  nsTArray<LookupCache*>& lookupCaches = aForUpdate ? mNewLookupCaches
                                                    : mLookupCaches;
  auto& rootStoreDirectory = aForUpdate ? mUpdatingDirectory
                                        : mRootStoreDirectory;

  for (auto c: lookupCaches) {
    if (c->TableName().Equals(aTable)) {
      return c;
    }
  }

  // We don't want to create lookupcache when shutdown is already happening.
  if (nsUrlClassifierDBService::ShutdownHasStarted()) {
    return nullptr;
  }

  // TODO : Bug 1302600, It would be better if we have a more general non-main
  //        thread method to convert table name to protocol version. Currently
  //        we can only know this by checking if the table name ends with '-proto'.
  UniquePtr<LookupCache> cache;
  nsCString provider = GetProvider(aTable);
  if (StringEndsWith(aTable, NS_LITERAL_CSTRING("-proto"))) {
    cache = MakeUnique<LookupCacheV4>(aTable, provider, rootStoreDirectory);
  } else {
    cache = MakeUnique<LookupCacheV2>(aTable, provider, rootStoreDirectory);
  }

  nsresult rv = cache->Init();
  if (NS_FAILED(rv)) {
    return nullptr;
  }
  rv = cache->Open();
  if (NS_SUCCEEDED(rv)) {
    lookupCaches.AppendElement(cache.get());
    return cache.release();
  }

  // At this point we failed to open LookupCache.
  //
  // GetLookupCache for update and for other usage will run on update thread
  // and worker thread respectively (Bug 1339760). Removing stuff only in
  // their own realms potentially increases the concurrency.

  if (aForUpdate) {
    // Remove intermediaries no matter if it's due to file corruption or not.
    RemoveUpdateIntermediaries();
    return nullptr;
  }

  // Non-update case.
  if (rv == NS_ERROR_FILE_CORRUPTED) {
    Reset(); // Not including the update intermediaries.
  }
  return nullptr;
}

nsresult
Classifier::ReadNoiseEntries(const Prefix& aPrefix,
                             const nsACString& aTableName,
                             uint32_t aCount,
                             PrefixArray* aNoiseEntries)
{
  FallibleTArray<uint32_t> prefixes;
  nsresult rv;

  LookupCache *cache = GetLookupCache(aTableName);
  if (!cache) {
    return NS_ERROR_FAILURE;
  }

  LookupCacheV2* cacheV2 = LookupCache::Cast<LookupCacheV2>(cache);
  if (cacheV2) {
    rv = cacheV2->GetPrefixes(prefixes);
  } else {
    rv = LookupCache::Cast<LookupCacheV4>(cache)->GetFixedLengthPrefixes(prefixes);
  }

  NS_ENSURE_SUCCESS(rv, rv);

  if (prefixes.Length() == 0) {
    NS_WARNING("Could not find prefix in PrefixSet during noise lookup");
    return NS_ERROR_FAILURE;
  }

  // We do not want to simply pick random prefixes, because this would allow
  // averaging out the noise by analysing the traffic from Firefox users.
  // Instead, we ensure the 'noise' is the same for the same prefix by seeding
  // the random number generator with the prefix. We prefer not to use rand()
  // which isn't thread safe, and the reseeding of which could trip up other
  // parts othe code that expect actual random numbers.
  // Here we use a simple LCG (Linear Congruential Generator) to generate
  // random numbers. We seed the LCG with the prefix we are generating noise
  // for.
  // http://en.wikipedia.org/wiki/Linear_congruential_generator

  uint32_t m = prefixes.Length();
  uint32_t a = aCount % m;
  uint32_t idx = aPrefix.ToUint32() % m;

  for (size_t i = 0; i < aCount; i++) {
    idx = (a * idx + a) % m;

    Prefix newPrefix;
    uint32_t hash = prefixes[idx];
    // In the case V4 little endian, we did swapping endian when converting from char* to
    // int, should revert endian to make sure we will send hex string correctly
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=1283007#c23
    if (!cacheV2 && !bool(MOZ_BIG_ENDIAN)) {
      hash = NativeEndian::swapFromBigEndian(prefixes[idx]);
    }

    newPrefix.FromUint32(hash);
    if (newPrefix != aPrefix) {
      aNoiseEntries->AppendElement(newPrefix);
    }
  }

  return NS_OK;
}

nsresult
Classifier::LoadMetadata(nsIFile* aDirectory, nsACString& aResult)
{
  nsCOMPtr<nsIDirectoryEnumerator> entries;
  nsresult rv = aDirectory->GetDirectoryEntries(getter_AddRefs(entries));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_ARG_POINTER(entries);

  nsCOMPtr<nsIFile> file;
  while (NS_SUCCEEDED(rv = entries->GetNextFile(getter_AddRefs(file))) && file) {
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

    int32_t dot = tableName.RFind(METADATA_SUFFIX);
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
