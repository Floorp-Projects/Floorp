/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
  void NewSubPrefix(PRUint32 aAddChunk, const Prefix& aPrefix, PRUint32 aSubChunk);
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
