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
#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsClassHashtable.h"
#include <string>

namespace mozilla {
namespace safebrowsing {

// The abstract class of TableUpdateV2 and TableUpdateV4. This
// is convenient for passing the TableUpdate* around associated
// with v2 and v4 instance.
class TableUpdate {
 public:
  TableUpdate(const nsACString& aTable) : mTable(aTable) {}

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TableUpdate);

  // To be overriden.
  virtual bool Empty() const = 0;

  // Common interfaces.
  const nsCString& TableName() const { return mTable; }

  template <typename T>
  static T* Cast(TableUpdate* aThat) {
    return (T::TAG == aThat->Tag() ? reinterpret_cast<T*>(aThat) : nullptr);
  }
  template <typename T>
  static const T* Cast(const TableUpdate* aThat) {
    return (T::TAG == aThat->Tag() ? reinterpret_cast<const T*>(aThat)
                                   : nullptr);
  }

 protected:
  virtual ~TableUpdate() = default;

 private:
  virtual int Tag() const = 0;

  const nsCString mTable;
};

typedef nsTArray<RefPtr<TableUpdate>> TableUpdateArray;
typedef nsTArray<RefPtr<const TableUpdate>> ConstTableUpdateArray;

// A table update is built from a single update chunk from the server. As the
// protocol parser processes each chunk, it constructs a table update with the
// new hashes.
class TableUpdateV2 : public TableUpdate {
 public:
  explicit TableUpdateV2(const nsACString& aTable) : TableUpdate(aTable) {}

  bool Empty() const override {
    return mAddChunks.Length() == 0 && mSubChunks.Length() == 0 &&
           mAddExpirations.Length() == 0 && mSubExpirations.Length() == 0 &&
           mAddPrefixes.Length() == 0 && mSubPrefixes.Length() == 0 &&
           mAddCompletes.Length() == 0 && mSubCompletes.Length() == 0 &&
           mMissPrefixes.Length() == 0;
  }

  // Throughout, uint32_t aChunk refers only to the chunk number. Chunk data is
  // stored in the Prefix structures.
  [[nodiscard]] nsresult NewAddChunk(uint32_t aChunk) {
    return mAddChunks.Set(aChunk);
  };
  [[nodiscard]] nsresult NewSubChunk(uint32_t aChunk) {
    return mSubChunks.Set(aChunk);
  };
  [[nodiscard]] nsresult NewAddExpiration(uint32_t aChunk) {
    return mAddExpirations.Set(aChunk);
  };
  [[nodiscard]] nsresult NewSubExpiration(uint32_t aChunk) {
    return mSubExpirations.Set(aChunk);
  };
  [[nodiscard]] nsresult NewAddPrefix(uint32_t aAddChunk,
                                      const Prefix& aPrefix);
  [[nodiscard]] nsresult NewSubPrefix(uint32_t aAddChunk, const Prefix& aPrefix,
                                      uint32_t aSubChunk);
  [[nodiscard]] nsresult NewAddComplete(uint32_t aChunk,
                                        const Completion& aCompletion);
  [[nodiscard]] nsresult NewSubComplete(uint32_t aAddChunk,
                                        const Completion& aCompletion,
                                        uint32_t aSubChunk);
  [[nodiscard]] nsresult NewMissPrefix(const Prefix& aPrefix);

  const ChunkSet& AddChunks() const { return mAddChunks; }
  const ChunkSet& SubChunks() const { return mSubChunks; }

  // Expirations for chunks.
  const ChunkSet& AddExpirations() const { return mAddExpirations; }
  const ChunkSet& SubExpirations() const { return mSubExpirations; }

  // Hashes associated with this chunk.
  AddPrefixArray& AddPrefixes() { return mAddPrefixes; }
  SubPrefixArray& SubPrefixes() { return mSubPrefixes; }
  const AddCompleteArray& AddCompletes() const { return mAddCompletes; }
  AddCompleteArray& AddCompletes() { return mAddCompletes; }
  SubCompleteArray& SubCompletes() { return mSubCompletes; }

  // Entries that cannot be completed.
  const MissPrefixArray& MissPrefixes() const { return mMissPrefixes; }

  // For downcasting.
  static const int TAG = 2;

 private:
  // The list of chunk numbers that we have for each of the type of chunks.
  ChunkSet mAddChunks;
  ChunkSet mSubChunks;
  ChunkSet mAddExpirations;
  ChunkSet mSubExpirations;

  // 4-byte sha256 prefixes.
  AddPrefixArray mAddPrefixes;
  SubPrefixArray mSubPrefixes;

  // This is only used by gethash so don't add this to Header.
  MissPrefixArray mMissPrefixes;

  // 32-byte hashes.
  AddCompleteArray mAddCompletes;
  SubCompleteArray mSubCompletes;

  virtual int Tag() const override { return TAG; }
};

// Structure for DBService/HashStore/Classifiers to update.
// It would contain the prefixes (both fixed and variable length)
// for addition and indices to removal. See Bug 1283009.
class TableUpdateV4 : public TableUpdate {
 public:
  typedef nsTArray<int32_t> RemovalIndiceArray;

 public:
  explicit TableUpdateV4(const nsACString& aTable)
      : TableUpdate(aTable), mFullUpdate(false) {}

  bool Empty() const override {
    return mPrefixesMap.IsEmpty() && mRemovalIndiceArray.IsEmpty() &&
           mFullHashResponseMap.IsEmpty();
  }

  bool IsFullUpdate() const { return mFullUpdate; }
  const PrefixStringMap& Prefixes() const { return mPrefixesMap; }
  const RemovalIndiceArray& RemovalIndices() const {
    return mRemovalIndiceArray;
  }
  const nsACString& ClientState() const { return mClientState; }
  const nsACString& SHA256() const { return mSHA256; }
  const FullHashResponseMap& FullHashResponse() const {
    return mFullHashResponseMap;
  }

  // For downcasting.
  static const int TAG = 4;

  void SetFullUpdate(bool aIsFullUpdate) { mFullUpdate = aIsFullUpdate; }
  void NewPrefixes(int32_t aSize, const nsACString& aPrefixes);
  void SetNewClientState(const nsACString& aState) { mClientState = aState; }
  void SetSHA256(const std::string& aSHA256);

  nsresult NewRemovalIndices(const uint32_t* aIndices, size_t aNumOfIndices);
  nsresult NewFullHashResponse(const Prefix& aPrefix,
                               const CachedFullHashResponse& aResponse);

  // Clear Prefixes & Removal indice.
  void Clear();

 private:
  virtual int Tag() const override { return TAG; }

  bool mFullUpdate;
  PrefixStringMap mPrefixesMap;
  RemovalIndiceArray mRemovalIndiceArray;
  nsCString mClientState;
  nsCString mSHA256;

  // This is used to store response from fullHashes.find.
  FullHashResponseMap mFullHashResponseMap;
};

// There is one hash store per table.
class HashStore {
 public:
  HashStore(const nsACString& aTableName, const nsACString& aProvider,
            nsIFile* aRootStoreFile);
  ~HashStore();

  const nsCString& TableName() const { return mTableName; }

  // Version is set to 0 by default, it is only used when we want to open
  // a specific version of HashStore. Note that the intention of aVersion
  // is only to pass SanityCheck, reading data from older version should
  // be handled additionally.
  nsresult Open(uint32_t aVersion = 0);

  // Add Prefixes/Completes are stored partly in the PrefixSet (contains the
  // Prefix data organized for fast lookup/low RAM usage) and partly in the
  // HashStore (Add Chunk numbers - only used for updates, slow retrieval).
  // AugmentAdds function joins the separate datasets into one complete
  // prefixes+chunknumbers dataset.
  nsresult AugmentAdds(const nsTArray<uint32_t>& aPrefixes,
                       const nsTArray<nsCString>& aCompletes);

  ChunkSet& AddChunks();
  ChunkSet& SubChunks();
  AddPrefixArray& AddPrefixes() { return mAddPrefixes; }
  SubPrefixArray& SubPrefixes() { return mSubPrefixes; }
  AddCompleteArray& AddCompletes() { return mAddCompletes; }
  SubCompleteArray& SubCompletes() { return mSubCompletes; }

  // =======
  // Updates
  // =======
  // Begin the update process.  Reads the store into memory.
  nsresult BeginUpdate();

  // Imports the data from a TableUpdate.
  nsresult ApplyUpdate(RefPtr<TableUpdateV2> aUpdate);

  // Process expired chunks
  nsresult Expire();

  // Rebuild the store, Incorporating all the applied updates.
  nsresult Rebuild();

  // Write the current state of the store to disk.
  // If you call between ApplyUpdate() and Rebuild(), you'll
  // have a mess on your hands.
  nsresult WriteFile();

  nsresult ReadCompletionsLegacyV3(AddCompleteArray& aCompletes);

  nsresult Reset();

 private:
  nsresult ReadHeader();
  nsresult SanityCheck(uint32_t aVersion = 0) const;
  nsresult CalculateChecksum(nsAutoCString& aChecksum, uint32_t aFileSize,
                             bool aChecksumPresent);
  nsresult CheckChecksum(uint32_t aFileSize);
  void UpdateHeader();

  nsresult ReadCompletions();
  nsresult ReadChunkNumbers();
  nsresult ReadHashes();

  nsresult ReadAddPrefixes();
  nsresult ReadSubPrefixes();
  nsresult ReadAddCompletes();

  nsresult WriteAddPrefixChunks(nsIOutputStream* aOut);
  nsresult WriteSubPrefixes(nsIOutputStream* aOut);
  nsresult WriteAddCompleteChunks(nsIOutputStream* aOut);

  nsresult ProcessSubs();

  nsresult PrepareForUpdate();

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
  const nsCString mTableName;
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

  uint32_t mFileSize;

  // For gtest to inspect private members.
  friend class PerProviderDirectoryTestUtils;
};

}  // namespace safebrowsing
}  // namespace mozilla

#endif
