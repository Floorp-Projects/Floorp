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
#include "nsICryptoHash.h"
#include "nsDataHashtable.h"

namespace mozilla {
namespace safebrowsing {

/**
 * Maintains the stores and LookupCaches for the url classifier.
 */
class Classifier {
public:
  typedef nsClassHashtable<nsCStringHashKey, nsCString> ProviderDictType;

public:
  Classifier();
  ~Classifier();

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
  nsresult ActiveTables(nsTArray<nsCString>& aTables);

  /**
   * Check a URL against the specified tables.
   */
  nsresult Check(const nsACString& aSpec,
                 const nsACString& tables,
                 uint32_t aFreshnessGuarantee,
                 LookupResultArray& aResults);

  /**
   * Apply the table updates in the array.  Takes ownership of
   * the updates in the array and clears it.  Wacky!
   */
  nsresult ApplyUpdates(nsTArray<TableUpdate*>* aUpdates);

  /**
   * The "background" part of ApplyUpdates. Once the background update
   * is called, the foreground update has to be called along with the
   * background result no matter whether the background update is
   * successful or not.
   */
  nsresult ApplyUpdatesBackground(nsTArray<TableUpdate*>* aUpdates,
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

  /**
   * Apply full hashes retrived from gethash to cache.
   */
  nsresult ApplyFullHashes(nsTArray<TableUpdate*>* aUpdates);

  void SetLastUpdateTime(const nsACString& aTableName, uint64_t updateTime);
  int64_t GetLastUpdateTime(const nsACString& aTableName);
  nsresult CacheCompletions(const CacheResultArray& aResults);
  uint32_t GetHashKey(void) { return mHashKey; }
  /*
   * Get a bunch of extra prefixes to query for completion
   * and mask the real entry being requested
   */
  nsresult ReadNoiseEntries(const Prefix& aPrefix,
                            const nsACString& aTableName,
                            uint32_t aCount,
                            PrefixArray* aNoiseEntries);

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

  LookupCache *GetLookupCache(const nsACString& aTable,
                              bool aForUpdate = false);

private:
  void DropStores();
  void DeleteTables(nsIFile* aDirectory, const nsTArray<nsCString>& aTables);

  nsresult CreateStoreDirectory();
  nsresult SetupPathNames();
  nsresult RecoverBackups();
  nsresult CleanToDelete();
  nsresult CopyInUseDirForUpdate();
  nsresult CopyInUseLookupCacheForUpdate();
  nsresult RegenActiveTables();



  // Remove any intermediary for update, including in-memory
  // and on-disk data.
  void RemoveUpdateIntermediaries();

#ifdef MOZ_SAFEBROWSING_DUMP_FAILED_UPDATES
  already_AddRefed<nsIFile> GetFailedUpdateDirectroy();
  nsresult DumpFailedUpdate();
#endif

  nsresult ScanStoreDir(nsTArray<nsCString>& aTables);

  nsresult UpdateHashStore(nsTArray<TableUpdate*>* aUpdates,
                           const nsACString& aTable);

  nsresult UpdateTableV4(nsTArray<TableUpdate*>* aUpdates,
                         const nsACString& aTable);

  nsresult UpdateCache(TableUpdate* aUpdates);

  LookupCache *GetLookupCacheForUpdate(const nsACString& aTable) {
    return GetLookupCache(aTable, true);
  }

  LookupCache *GetLookupCacheFrom(const nsACString& aTable,
                                  nsTArray<LookupCache*>& aLookupCaches,
                                  nsIFile* aRootStoreDirectory);


  bool CheckValidUpdate(nsTArray<TableUpdate*>* aUpdates,
                        const nsACString& aTable);

  nsresult LoadMetadata(nsIFile* aDirectory, nsACString& aResult);

  nsCString GetProvider(const nsACString& aTableName);

  // Root dir of the Local profile.
  nsCOMPtr<nsIFile> mCacheDirectory;
  // Main directory where to store the databases.
  nsCOMPtr<nsIFile> mRootStoreDirectory;
  // Used for atomically updating the other dirs.
  nsCOMPtr<nsIFile> mBackupDirectory;
  nsCOMPtr<nsIFile> mUpdatingDirectory; // For update only.
  nsCOMPtr<nsIFile> mToDeleteDirectory;
  nsCOMPtr<nsICryptoHash> mCryptoHash;
  nsTArray<LookupCache*> mLookupCaches; // For query only.
  nsTArray<nsCString> mActiveTablesCache;
  uint32_t mHashKey;
  // Stores the last time a given table was updated (seconds).
  TableFreshnessMap mTableFreshness;

  // In-memory cache for the result of TableRequest. See
  // nsIUrlClassifierDBService.getTables for the format.
  nsCString mTableRequestResult;

  // Whether mTableRequestResult is outdated and needs to
  // be reloaded from disk.
  bool mIsTableRequestResultOutdated;

  // The copy of mLookupCaches for update only.
  nsTArray<LookupCache*> mNewLookupCaches;
};

} // namespace safebrowsing
} // namespace mozilla

#endif
