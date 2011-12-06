//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Url Classifier code
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dave Camp <dcamp@mozilla.com>
 *   Gian-Carlo Pascutto <gpascutto@mozilla.com>
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
  nsresult ScanStoreDir(nsTArray<nsCString>& aTables);

  nsresult ApplyTableUpdates(nsTArray<TableUpdate*>* aUpdates,
                             const nsACString& aTable);

  LookupCache *GetLookupCache(const nsACString& aTable);
  nsresult InitKey();

  nsCOMPtr<nsICryptoHash> mCryptoHash;
  nsCOMPtr<nsIFile> mStoreDirectory;
  nsTArray<HashStore*> mHashStores;
  nsTArray<LookupCache*> mLookupCaches;
  PRUint32 mHashKey;
  // Stores the last time a given table was updated (seconds).
  nsDataHashtable<nsCStringHashKey, PRInt64> mTableFreshness;
  PRUint32 mFreshTime;
};

}
}

#endif
