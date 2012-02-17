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

#ifndef HashStore_h__
#define HashStore_h__

#include "Entries.h"
#include "ChunkSet.h"

#include "nsString.h"
#include "nsTArray.h"
#include "nsIFile.h"
#include "nsIFileStreams.h"
#include "nsCOMPtr.h"

namespace mozilla {
namespace safebrowsing {

class TableUpdate {
public:
  TableUpdate(const nsACString& aTable)
      : mTable(aTable), mLocalUpdate(false) {}
  const nsCString& TableName() const { return mTable; }

  bool Empty() const {
    return mAddChunks.Length() == 0 &&
      mSubChunks.Length() == 0 &&
      mAddExpirations.Length() == 0 &&
      mSubExpirations.Length() == 0 &&
      mAddPrefixes.Length() == 0 &&
      mSubPrefixes.Length() == 0 &&
      mAddCompletes.Length() == 0 &&
      mSubCompletes.Length() == 0;
  }

  void NewAddChunk(PRUint32 aChunk) { mAddChunks.Set(aChunk); }
  void NewSubChunk(PRUint32 aChunk) { mSubChunks.Set(aChunk); }

  void NewAddExpiration(PRUint32 aChunk) { mAddExpirations.Set(aChunk); }
  void NewSubExpiration(PRUint32 aChunk) { mSubExpirations.Set(aChunk); }

  void NewAddPrefix(PRUint32 aAddChunk, const Prefix& aPrefix);
  void NewSubPrefix(PRUint32 aAddChunk, const Prefix& aPprefix, PRUint32 aSubChunk);
  void NewAddComplete(PRUint32 aChunk, const Completion& aCompletion);
  void NewSubComplete(PRUint32 aAddChunk, const Completion& aCompletion,
                      PRUint32 aSubChunk);
  void SetLocalUpdate(void) { mLocalUpdate = true; };
  bool IsLocalUpdate(void) { return mLocalUpdate; };

  ChunkSet& AddChunks() { return mAddChunks; }
  ChunkSet& SubChunks() { return mSubChunks; }

  ChunkSet& AddExpirations() { return mAddExpirations; }
  ChunkSet& SubExpirations() { return mSubExpirations; }

  AddPrefixArray& AddPrefixes() { return mAddPrefixes; }
  SubPrefixArray& SubPrefixes() { return mSubPrefixes; }
  AddCompleteArray& AddCompletes() { return mAddCompletes; }
  SubCompleteArray& SubCompletes() { return mSubCompletes; }

private:
  nsCString mTable;
  // Update not from the remote server (no freshness)
  bool mLocalUpdate;

  ChunkSet mAddChunks;
  ChunkSet mSubChunks;
  ChunkSet mAddExpirations;
  ChunkSet mSubExpirations;
  AddPrefixArray mAddPrefixes;
  SubPrefixArray mSubPrefixes;
  AddCompleteArray mAddCompletes;
  SubCompleteArray mSubCompletes;
};

class HashStore {
public:
  HashStore(const nsACString& aTableName, nsIFile* aStoreFile);
  ~HashStore();

  const nsCString& TableName() const { return mTableName; };

  nsresult Open();
  nsresult AugmentAdds(const nsTArray<PRUint32>& aPrefixes);

  ChunkSet& AddChunks() { return mAddChunks; }
  ChunkSet& SubChunks() { return mSubChunks; }
  AddPrefixArray& AddPrefixes() { return mAddPrefixes; }
  AddCompleteArray& AddCompletes() { return mAddCompletes; }
  SubPrefixArray& SubPrefixes() { return mSubPrefixes; }
  SubCompleteArray& SubCompletes() { return mSubCompletes; }

  // =======
  // Updates
  // =======
  // Begin the update process.  Reads the store into memory.
  nsresult BeginUpdate();

  // Imports the data from a TableUpdate.
  nsresult ApplyUpdate(TableUpdate &aUpdate);

  // Process expired chunks
  nsresult Expire();

  // Rebuild the store, Incorporating all the applied updates.
  nsresult Rebuild();

  // Write the current state of the store to disk.
  // If you call between ApplyUpdate() and Rebuild(), you'll
  // have a mess on your hands.
  nsresult WriteFile();

  // Drop memory used during the update process.
  nsresult FinishUpdate();

  // Force the entire store in memory
  nsresult ReadEntireStore();

private:
  void Clear();
  nsresult Reset();

  nsresult ReadHeader();
  nsresult SanityCheck(nsIFile* aStoreFile);
  nsresult CalculateChecksum(nsCAutoString& aChecksum, bool aChecksumPresent);
  nsresult CheckChecksum(nsIFile* aStoreFile);
  void UpdateHeader();

  nsresult EnsureChunkNumbers();
  nsresult ReadChunkNumbers();
  nsresult ReadHashes();
  nsresult ReadAddPrefixes();
  nsresult ReadSubPrefixes();

  nsresult WriteAddPrefixes(nsIOutputStream* aOut);
  nsresult WriteSubPrefixes(nsIOutputStream* aOut);

  nsresult ProcessSubs();

  struct Header {
    uint32 magic;
    uint32 version;
    uint32 numAddChunks;
    uint32 numSubChunks;
    uint32 numAddPrefixes;
    uint32 numSubPrefixes;
    uint32 numAddCompletes;
    uint32 numSubCompletes;
  };

  Header mHeader;

  nsCString mTableName;
  nsCOMPtr<nsIFile> mStoreDirectory;

  bool mInUpdate;

  nsCOMPtr<nsIInputStream> mInputStream;

  bool haveChunks;
  ChunkSet mAddChunks;
  ChunkSet mSubChunks;

  ChunkSet mAddExpirations;
  ChunkSet mSubExpirations;

  AddPrefixArray mAddPrefixes;
  AddCompleteArray mAddCompletes;
  SubPrefixArray mSubPrefixes;
  SubCompleteArray mSubCompletes;
};

}
}

#endif
