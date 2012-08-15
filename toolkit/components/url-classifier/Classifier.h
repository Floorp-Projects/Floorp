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
  nsresult Close();
  nsresult Reset();

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
   * Check a URL against the database.
   */
  nsresult Check(const nsACString& aSpec, LookupResultArray& aResults);

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
  PRUint32 GetHashKey(void) { return mHashKey; };
  void SetFreshTime(PRUint32 aTime) { mFreshTime = aTime; };
  void SetPerClientRandomize(bool aRandomize) { mPerClientRandomize = aRandomize; };
  /*
   * Get a bunch of extra prefixes to query for completion
   * and mask the real entry being requested
   */
  nsresult ReadNoiseEntries(const Prefix& aPrefix,
                            const nsACString& aTableName,
                            PRInt32 aCount,
                            PrefixArray* aNoiseEntries);
private:
  void DropStores();
  nsresult RegenActiveTables();
  nsresult ScanStoreDir(nsTArray<nsCString>& aTables);

  nsresult ApplyTableUpdates(nsTArray<TableUpdate*>* aUpdates,
                             const nsACString& aTable);

  LookupCache *GetLookupCache(const nsACString& aTable);
  nsresult InitKey();

  nsCOMPtr<nsICryptoHash> mCryptoHash;
  nsCOMPtr<nsIFile> mStoreDirectory;
  nsTArray<HashStore*> mHashStores;
  nsTArray<LookupCache*> mLookupCaches;
  nsTArray<nsCString> mActiveTablesCache;
  PRUint32 mHashKey;
  // Stores the last time a given table was updated (seconds).
  nsDataHashtable<nsCStringHashKey, PRInt64> mTableFreshness;
  PRUint32 mFreshTime;
  bool mPerClientRandomize;
};

}
}

#endif
