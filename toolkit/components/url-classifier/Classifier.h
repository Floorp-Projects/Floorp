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
  void Reset();

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

private:
  void DropStores();
  void DeleteTables(nsIFile* aDirectory, const nsTArray<nsCString>& aTables);
  void AbortUpdateAndReset(const nsCString& aTable);

  nsresult CreateStoreDirectory();
  nsresult SetupPathNames();
  nsresult RecoverBackups();
  nsresult CleanToDelete();
  nsresult BackupTables();
  nsresult RemoveBackupTables();
  nsresult RegenActiveTables();

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

  LookupCache *GetLookupCache(const nsACString& aTable);

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
  nsCOMPtr<nsIFile> mToDeleteDirectory;
  nsCOMPtr<nsICryptoHash> mCryptoHash;
  nsTArray<LookupCache*> mLookupCaches;
  nsTArray<nsCString> mActiveTablesCache;
  uint32_t mHashKey;
  // Stores the last time a given table was updated (seconds).
  nsDataHashtable<nsCStringHashKey, int64_t> mTableFreshness;

  // In-memory cache for the result of TableRequest. See
  // nsIUrlClassifierDBService.getTables for the format.
  nsCString mTableRequestResult;

  // Whether mTableRequestResult is outdated and needs to
  // be reloaded from disk.
  bool mIsTableRequestResultOutdated;
};

} // namespace safebrowsing
} // namespace mozilla

#endif
