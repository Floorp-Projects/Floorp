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

// A table update is built from a single update chunk from the server. As the
// protocol parser processes each chunk, it constructs a table update with the
// new hashes.
class TableUpdate {
public:
  explicit TableUpdate(const nsACString& aTable)
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

  // Throughout, uint32_t aChunk refers only to the chunk number. Chunk data is
  // stored in the Prefix structures.
  MOZ_MUST_USE nsresult NewAddChunk(uint32_t aChunk) {
    return mAddChunks.Set(aChunk);
  };
  MOZ_MUST_USE nsresult NewSubChunk(uint32_t aChunk) {
    return mSubChunks.Set(aChunk);
  };
  MOZ_MUST_USE nsresult NewAddExpiration(uint32_t aChunk) {
    return mAddExpirations.Set(aChunk);
  };
  MOZ_MUST_USE nsresult NewSubExpiration(uint32_t aChunk) {
    return mSubExpirations.Set(aChunk);
  };
  MOZ_MUST_USE nsresult NewAddPrefix(uint32_t aAddChunk, const Prefix& aPrefix);
  MOZ_MUST_USE nsresult NewSubPrefix(uint32_t aAddChunk,
                                     const Prefix& aPrefix,
                                     uint32_t aSubChunk);
  MOZ_MUST_USE nsresult NewAddComplete(uint32_t aChunk,
                                       const Completion& aCompletion);
  MOZ_MUST_USE nsresult NewSubComplete(uint32_t aAddChunk,
                                       const Completion& aCompletion,
                                       uint32_t aSubChunk);
  void SetLocalUpdate(void) { mLocalUpdate = true; }
  bool IsLocalUpdate(void) { return mLocalUpdate; }

  ChunkSet& AddChunks() { return mAddChunks; }
  ChunkSet& SubChunks() { return mSubChunks; }

  // Expirations for chunks.
  ChunkSet& AddExpirations() { return mAddExpirations; }
  ChunkSet& SubExpirations() { return mSubExpirations; }

  // Hashes associated with this chunk.
  AddPrefixArray& AddPrefixes() { return mAddPrefixes; }
  SubPrefixArray& SubPrefixes() { return mSubPrefixes; }
  AddCompleteArray& AddCompletes() { return mAddCompletes; }
  SubCompleteArray& SubCompletes() { return mSubCompletes; }

private:
  nsCString mTable;
  // Update not from the remote server (no freshness)
  bool mLocalUpdate;

  // The list of chunk numbers that we have for each of the type of chunks.
  ChunkSet mAddChunks;
  ChunkSet mSubChunks;
  ChunkSet mAddExpirations;
  ChunkSet mSubExpirations;

  // 4-byte sha256 prefixes.
  AddPrefixArray mAddPrefixes;
  SubPrefixArray mSubPrefixes;

  // 32-byte hashes.
  AddCompleteArray mAddCompletes;
  SubCompleteArray mSubCompletes;
};

// There is one hash store per table.
class HashStore {
public:
  HashStore(const nsACString& aTableName, nsIFile* aStoreFile);
  ~HashStore();

  const nsCString& TableName() const { return mTableName; }

  nsresult Open();
  // Add Prefixes are stored partly in the PrefixSet (contains the
  // Prefix data organized for fast lookup/low RAM usage) and partly in the
  // HashStore (Add Chunk numbers - only used for updates, slow retrieval).
  // AugmentAdds function joins the separate datasets into one complete
  // prefixes+chunknumbers dataset.
  nsresult AugmentAdds(const nsTArray<uint32_t>& aPrefixes);

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

  // Wipe out all Completes.
  void ClearCompletes();

private:
  nsresult Reset();

  nsresult ReadHeader();
  nsresult SanityCheck();
  nsresult CalculateChecksum(nsAutoCString& aChecksum, uint32_t aFileSize,
                             bool aChecksumPresent);
  nsresult CheckChecksum(nsIFile* aStoreFile, uint32_t aFileSize);
  void UpdateHeader();

  nsresult ReadChunkNumbers();
  nsresult ReadHashes();

  nsresult ReadAddPrefixes();
  nsresult ReadSubPrefixes();

  nsresult WriteAddPrefixes(nsIOutputStream* aOut);
  nsresult WriteSubPrefixes(nsIOutputStream* aOut);

  nsresult ProcessSubs();

 // This is used for checking that the database is correct and for figuring out
 // the number of chunks, etc. to read from disk on restart.
  struct Header {
    uint32_t magic;
    uint32_t version;
    uint32_t numAddChunks;
    uint32_t numSubChunks;
    uint32_t numAddPrefixes;
    uint32_t numSubPrefixes;
    uint32_t numAddCompletes;
    uint32_t numSubCompletes;
  };

  Header mHeader;

  // The name of the table (must end in -shavar or -digest256, or evidently
  // -simple for unittesting.
  nsCString mTableName;
  nsCOMPtr<nsIFile> mStoreDirectory;

  bool mInUpdate;

  nsCOMPtr<nsIInputStream> mInputStream;

  // Chunk numbers, stored as uint32_t arrays.
  ChunkSet mAddChunks;
  ChunkSet mSubChunks;

  ChunkSet mAddExpirations;
  ChunkSet mSubExpirations;

  // Chunk data for shavar tables. See Entries.h for format.
  AddPrefixArray mAddPrefixes;
  SubPrefixArray mSubPrefixes;

  // See bug 806422 for background. We must be able to distinguish between
  // updates from the completion server and updates from the regular server.
  AddCompleteArray mAddCompletes;
  SubCompleteArray mSubCompletes;
};

} // namespace safebrowsing
} // namespace mozilla

#endif
