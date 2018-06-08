//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Classifier_h__
#define Classifier_h__

#include "Entries.h"
#include "HashStore.h"
#include "ProtocolParser.h"
#include "LookupCache.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIFile.h"
#include "nsDataHashtable.h"

class nsIThread;

namespace mozilla {
namespace safebrowsing {

/**
 * Maintains the stores and LookupCaches for the url classifier.
 */
class Classifier {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Classifier);

  Classifier();

  nsresult Open(nsIFile& aCacheDirectory);
  void Close();
  void Reset(); // Not including any intermediary for update.

  /**
   * Clear data for specific tables.
   * If ClearType is Clear_Cache, this function will only clear cache in lookup
   * cache, otherwise, it will clear data in lookup cache and data stored on disk.
   */
  enum ClearType {
    Clear_Cache,
    Clear_All,
  };
  void ResetTables(ClearType aType, const nsTArray<nsCString>& aTables);

  /**
   * Get the list of active tables and their chunks in a format
   * suitable for an update request.
   */
  void TableRequest(nsACString& aResult);

  /*
   * Get all tables that we know about.
   */
  nsresult ActiveTables(nsTArray<nsCString>& aTables) const;

  /**
   * Check a URL against the specified tables.
   */
  nsresult Check(const nsACString& aSpec,
                 const nsACString& tables,
                 LookupResultArray& aResults);

  /**
   * Asynchronously apply updates to the in-use databases. When the
   * update is complete, the caller can be notified by |aCallback|, which
   * will occur on the caller thread.
   */
  using AsyncUpdateCallback = std::function<void(nsresult)>;
  nsresult AsyncApplyUpdates(const TableUpdateArray& aUpdates,
                             const AsyncUpdateCallback& aCallback);

  /**
   * Wait until the ongoing async update is finished and callback
   * is fired. Once this function returns, AsyncApplyUpdates is
   * no longer available.
   */
  void FlushAndDisableAsyncUpdate();

  /**
   * Apply full hashes retrived from gethash to cache.
   */
  nsresult ApplyFullHashes(ConstTableUpdateArray& aUpdates);

  /*
   * Get a bunch of extra prefixes to query for completion
   * and mask the real entry being requested
   */
  nsresult ReadNoiseEntries(const Prefix& aPrefix,
                            const nsACString& aTableName,
                            uint32_t aCount,
                            PrefixArray& aNoiseEntries);

#ifdef MOZ_SAFEBROWSING_DUMP_FAILED_UPDATES
  nsresult DumpRawTableUpdates(const nsACString& aRawUpdates);
#endif

  static void SplitTables(const nsACString& str, nsTArray<nsCString>& tables);

  // Given a root store directory, return a private store directory
  // based on the table name. To avoid migration issue, the private
  // store directory is only different from root directory for V4 tables.
  //
  // For V4 tables (suffixed by '-proto'), the private directory would
  // be [root directory path]/[provider]. The provider of V4 tables is
  // 'google4'.
  //
  // Note that if the table name is not owned by any provider, just use
  // the root directory.
  static nsresult GetPrivateStoreDirectory(nsIFile* aRootStoreDirectory,
                                           const nsACString& aTableName,
                                           const nsACString& aProvider,
                                           nsIFile** aPrivateStoreDirectory);

  // Swap in in-memory and on-disk database and remove all
  // update intermediaries.
  nsresult SwapInNewTablesAndCleanup();

  RefPtr<LookupCache> GetLookupCache(const nsACString& aTable,
                                     bool aForUpdate = false);

  void GetCacheInfo(const nsACString& aTable,
                    nsIUrlClassifierCacheInfo** aCache);

private:
  ~Classifier();

  void DropStores();
  void DeleteTables(nsIFile* aDirectory, const nsTArray<nsCString>& aTables);

  nsresult CreateStoreDirectory();
  nsresult SetupPathNames();
  nsresult RecoverBackups();
  nsresult CleanToDelete();
  nsresult CopyInUseDirForUpdate();
  nsresult RegenActiveTables();

  void MergeNewLookupCaches(); // Merge mNewLookupCaches into mLookupCaches.

  void CopyAndInvalidateFullHashCache();

  // Remove any intermediary for update, including in-memory
  // and on-disk data.
  void RemoveUpdateIntermediaries();

#ifdef MOZ_SAFEBROWSING_DUMP_FAILED_UPDATES
  already_AddRefed<nsIFile> GetFailedUpdateDirectroy();
  nsresult DumpFailedUpdate();
#endif

  nsresult ScanStoreDir(nsIFile* aDirectory, nsTArray<nsCString>& aTables);

  nsresult UpdateHashStore(TableUpdateArray& aUpdates,
                           const nsACString& aTable);

  nsresult UpdateTableV4(TableUpdateArray& aUpdates,
                         const nsACString& aTable);

  nsresult UpdateCache(RefPtr<const TableUpdate> aUpdates);

  RefPtr<LookupCache> GetLookupCacheForUpdate(const nsACString& aTable) {
    return GetLookupCache(aTable, true);
  }

  RefPtr<LookupCache> GetLookupCacheFrom(const nsACString& aTable,
                                         LookupCacheArray& aLookupCaches,
                                         nsIFile* aRootStoreDirectory);


  bool CheckValidUpdate(TableUpdateArray& aUpdates,
                        const nsACString& aTable);

  nsresult LoadMetadata(nsIFile* aDirectory, nsACString& aResult);

  static nsCString GetProvider(const nsACString& aTableName);

  /**
   * The "background" part of ApplyUpdates. Once the background update
   * is called, the foreground update has to be called along with the
   * background result no matter whether the background update is
   * successful or not.
   */
  nsresult ApplyUpdatesBackground(TableUpdateArray& aUpdates,
                                  nsACString& aFailedTableName);

  /**
   * The "foreground" part of ApplyUpdates. The in-use data (in-memory and
   * on-disk) will be touched so this MUST be mutually exclusive to other
   * member functions.
   *
   * If |aBackgroundRv| is successful, the return value is the result of
   * bringing stuff to the foreground. Otherwise, the foreground table may
   * be reset according to the background update failed reason and
   * |aBackgroundRv| will be returned to forward the background update result.
   */
  nsresult ApplyUpdatesForeground(nsresult aBackgroundRv,
                                  const nsACString& aFailedTableName);

  // Root dir of the Local profile.
  nsCOMPtr<nsIFile> mCacheDirectory;
  // Main directory where to store the databases.
  nsCOMPtr<nsIFile> mRootStoreDirectory;
  // Used for atomically updating the other dirs.
  nsCOMPtr<nsIFile> mBackupDirectory;
  nsCOMPtr<nsIFile> mUpdatingDirectory; // For update only.
  nsCOMPtr<nsIFile> mToDeleteDirectory;
  LookupCacheArray mLookupCaches; // For query only.
  nsTArray<nsCString> mActiveTablesCache;
  uint32_t mHashKey;

  // In-memory cache for the result of TableRequest. See
  // nsIUrlClassifierDBService.getTables for the format.
  nsCString mTableRequestResult;

  // Whether mTableRequestResult is outdated and needs to
  // be reloaded from disk.
  bool mIsTableRequestResultOutdated;

  // The copy of mLookupCaches for update only.
  LookupCacheArray mNewLookupCaches;

  bool mUpdateInterrupted;

  nsCOMPtr<nsIThread> mUpdateThread; // For async update.

  // Identical to mRootStoreDirectory but for update only because
  // nsIFile is not thread safe and mRootStoreDirectory needs to
  // be accessed in CopyInUseDirForUpdate().
  // It will be initialized right before update on the worker thread.
  nsCOMPtr<nsIFile> mRootStoreDirectoryForUpdate;

  bool mIsClosed; // true once Close() has been called
};

} // namespace safebrowsing
} // namespace mozilla

#endif
