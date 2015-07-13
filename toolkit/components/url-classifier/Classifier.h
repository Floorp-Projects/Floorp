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
  Classifier();
  ~Classifier();

  nsresult Open(nsIFile& aCacheDirectory);
  void Close();
  void Reset();

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
   * Failed update. Spoil the entries so we don't block hosts
   * unnecessarily
   */
  nsresult MarkSpoiled(nsTArray<nsCString>& aTables);
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
  static void SplitTables(const nsACString& str, nsTArray<nsCString>& tables);

private:
  void DropStores();
  nsresult CreateStoreDirectory();
  nsresult SetupPathNames();
  nsresult RecoverBackups();
  nsresult CleanToDelete();
  nsresult BackupTables();
  nsresult RemoveBackupTables();
  nsresult RegenActiveTables();
  nsresult ScanStoreDir(nsTArray<nsCString>& aTables);

  nsresult ApplyTableUpdates(nsTArray<TableUpdate*>* aUpdates,
                             const nsACString& aTable);

  LookupCache *GetLookupCache(const nsACString& aTable);

  // Root dir of the Local profile.
  nsCOMPtr<nsIFile> mCacheDirectory;
  // Main directory where to store the databases.
  nsCOMPtr<nsIFile> mStoreDirectory;
  // Used for atomically updating the other dirs.
  nsCOMPtr<nsIFile> mBackupDirectory;
  nsCOMPtr<nsIFile> mToDeleteDirectory;
  nsCOMPtr<nsICryptoHash> mCryptoHash;
  nsTArray<HashStore*> mHashStores;
  nsTArray<LookupCache*> mLookupCaches;
  nsTArray<nsCString> mActiveTablesCache;
  uint32_t mHashKey;
  // Stores the last time a given table was updated (seconds).
  nsDataHashtable<nsCStringHashKey, int64_t> mTableFreshness;
};

} // namespace safebrowsing
} // namespace mozilla

#endif
