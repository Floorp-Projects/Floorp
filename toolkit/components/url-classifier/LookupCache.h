//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LookupCache_h__
#define LookupCache_h__

#include "Entries.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsCOMPtr.h"
#include "nsIFile.h"
#include "nsIFileStreams.h"
#include "mozilla/RefPtr.h"
#include "nsUrlClassifierPrefixSet.h"
#include "VariableLengthPrefixSet.h"
#include "mozilla/Logging.h"

namespace mozilla {
namespace safebrowsing {

#define MAX_HOST_COMPONENTS 5
#define MAX_PATH_COMPONENTS 4

class LookupResult {
public:
  LookupResult() : mComplete(false), mNoise(false),
                   mFresh(false), mProtocolConfirmed(false),
                   mPartialHashLength(0) {}

  // The fragment that matched in the LookupCache
  union {
    Prefix fixedLengthPrefix;
    Completion complete;
  } hash;

  const Completion &CompleteHash() {
    MOZ_ASSERT(!mNoise);
    return hash.complete;
  }

  nsCString PartialHash() {
    MOZ_ASSERT(mPartialHashLength <= COMPLETE_SIZE);
    return nsCString(reinterpret_cast<char*>(hash.complete.buf), mPartialHashLength);
  }

  nsCString PartialHashHex() {
    nsAutoCString hex;
    for (size_t i = 0; i < mPartialHashLength; i++) {
      hex.AppendPrintf("%.2X", hash.complete.buf[i]);
    }
    return hex;
  }

  bool Confirmed() const { return (mComplete && mFresh) || mProtocolConfirmed; }
  bool Complete() const { return mComplete; }

  // True if we have a complete match for this hash in the table.
  bool mComplete;

  // True if this is a noise entry, i.e. an extra entry
  // that is inserted to mask the true URL we are requesting.
  // Noise entries will not have a complete 256-bit hash as
  // they are fetched from the local 32-bit database and we
  // don't know the corresponding full URL.
  bool mNoise;

  // True if we've updated this table recently-enough.
  bool mFresh;

  bool mProtocolConfirmed;

  nsCString mTableName;

  uint32_t mPartialHashLength;
};

typedef nsTArray<LookupResult> LookupResultArray;

struct CacheResult {
  AddComplete entry;
  nsCString table;

  bool operator==(const CacheResult& aOther) const {
    if (entry != aOther.entry) {
      return false;
    }
    return table == aOther.table;
  }
};
typedef nsTArray<CacheResult> CacheResultArray;

class LookupCache {
public:
  // Check for a canonicalized IP address.
  static bool IsCanonicalizedIP(const nsACString& aHost);

  // take a lookup string (www.hostname.com/path/to/resource.html) and
  // expand it into the set of fragments that should be searched for in an
  // entry
  static nsresult GetLookupFragments(const nsACString& aSpec,
                                     nsTArray<nsCString>* aFragments);
  // Similar to GetKey(), but if the domain contains three or more components,
  // two keys will be returned:
  //  hostname.com/foo/bar -> [hostname.com]
  //  mail.hostname.com/foo/bar -> [hostname.com, mail.hostname.com]
  //  www.mail.hostname.com/foo/bar -> [hostname.com, mail.hostname.com]
  static nsresult GetHostKeys(const nsACString& aSpec,
                              nsTArray<nsCString>* aHostKeys);

  LookupCache(const nsACString& aTableName,
              const nsACString& aProvider,
              nsIFile* aStoreFile);
  virtual ~LookupCache() {}

  const nsCString &TableName() const { return mTableName; }

  // The directory handle where we operate will
  // be moved away when a backup is made.
  nsresult UpdateRootDirHandle(nsIFile* aRootStoreDirectory);

  // This will Clear() the passed arrays when done.
  nsresult AddCompletionsToCache(AddCompleteArray& aAddCompletes);

  // Write data stored in lookup cache to disk.
  nsresult WriteFile();

  // Clear completions retrieved from gethash request.
  void ClearCache();

  bool IsPrimed() const { return mPrimed; };

#if DEBUG
  void DumpCache();
#endif

  virtual nsresult Open();
  virtual nsresult Init() = 0;
  virtual nsresult ClearPrefixes() = 0;
  virtual nsresult Has(const Completion& aCompletion,
                       bool* aHas, bool* aComplete,
                       uint32_t* aMatchLength) = 0;

  virtual void ClearAll();

  template<typename T>
  static T* Cast(LookupCache* aThat) {
    return ((aThat && T::VER == aThat->Ver()) ? reinterpret_cast<T*>(aThat) : nullptr);
  }

private:
  nsresult Reset();
  nsresult LoadPrefixSet();

  virtual nsresult StoreToFile(nsIFile* aFile) = 0;
  virtual nsresult LoadFromFile(nsIFile* aFile) = 0;
  virtual size_t SizeOfPrefixSet() = 0;

  virtual int Ver() const = 0;

protected:
  bool mPrimed;
  nsCString mTableName;
  nsCString mProvider;
  nsCOMPtr<nsIFile> mRootStoreDirectory;
  nsCOMPtr<nsIFile> mStoreDirectory;

  // Full length hashes obtained in gethash request
  CompletionArray mGetHashCache;

  // For gtest to inspect private members.
  friend class PerProviderDirectoryTestUtils;
};

class LookupCacheV2 final : public LookupCache
{
public:
  explicit LookupCacheV2(const nsACString& aTableName,
                         const nsACString& aProvider,
                         nsIFile* aStoreFile)
    : LookupCache(aTableName, aProvider, aStoreFile) {}
  ~LookupCacheV2() {}

  virtual nsresult Init() override;
  virtual nsresult Open() override;
  virtual void ClearAll() override;
  virtual nsresult Has(const Completion& aCompletion,
                       bool* aHas, bool* aComplete,
                       uint32_t* aMatchLength) override;

  nsresult Build(AddPrefixArray& aAddPrefixes,
                 AddCompleteArray& aAddCompletes);

  nsresult GetPrefixes(FallibleTArray<uint32_t>& aAddPrefixes);

#if DEBUG
  void DumpCompletions();
#endif

  static const int VER;

protected:
  nsresult ReadCompletions();

  virtual nsresult ClearPrefixes() override;
  virtual nsresult StoreToFile(nsIFile* aFile) override;
  virtual nsresult LoadFromFile(nsIFile* aFile) override;
  virtual size_t SizeOfPrefixSet() override;

private:
  virtual int Ver() const override { return VER; }

  // Construct a Prefix Set with known prefixes.
  // This will Clear() aAddPrefixes when done.
  nsresult ConstructPrefixSet(AddPrefixArray& aAddPrefixes);

  // Full length hashes obtained in update request
  CompletionArray mUpdateCompletions;

  // Set of prefixes known to be in the database
  RefPtr<nsUrlClassifierPrefixSet> mPrefixSet;
};

} // namespace safebrowsing
} // namespace mozilla

#endif
