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
#include "mozilla/TypedEnumBits.h"
#include "nsIUrlClassifierInfo.h"

namespace mozilla {
namespace safebrowsing {

#define MAX_HOST_COMPONENTS 5
#define MAX_PATH_COMPONENTS 4

class LookupResult {
public:
  LookupResult() : mNoise(false), mProtocolConfirmed(false),
                   mPartialHashLength(0), mConfirmed(false),
                   mProtocolV2(true) {}

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(LookupResult);

  // The fragment that matched in the LookupCache
  union {
    Prefix fixedLengthPrefix;
    Completion complete;
  } hash;

  const Completion &CompleteHash() const {
    MOZ_ASSERT(!mNoise);
    return hash.complete;
  }

  nsCString PartialHash() const {
    MOZ_ASSERT(mPartialHashLength <= COMPLETE_SIZE);
    if (mNoise) {
      return nsCString(reinterpret_cast<const char*>(hash.fixedLengthPrefix.buf),
                       PREFIX_SIZE);
    } else {
      return nsCString(reinterpret_cast<const char*>(hash.complete.buf),
                       mPartialHashLength);
    }
  }

  nsCString PartialHashHex() const {
    nsAutoCString hex;
    for (size_t i = 0; i < mPartialHashLength; i++) {
      hex.AppendPrintf("%.2X", hash.complete.buf[i]);
    }
    return hex;
  }

  bool Confirmed() const { return mConfirmed || mProtocolConfirmed; }

  // True if we have a complete match for this hash in the table.
  bool Complete() const { return mPartialHashLength == COMPLETE_SIZE; }

  // True if this is a noise entry, i.e. an extra entry
  // that is inserted to mask the true URL we are requesting.
  // Noise entries will not have a complete 256-bit hash as
  // they are fetched from the local 32-bit database and we
  // don't know the corresponding full URL.
  bool mNoise;

  bool mProtocolConfirmed;

  nsCString mTableName;

  uint32_t mPartialHashLength;

  // True as long as this lookup is complete and hasn't expired.
  bool mConfirmed;

  bool mProtocolV2;
private:
  ~LookupResult() {}
};

typedef nsTArray<RefPtr<LookupResult>> LookupResultArray;

class CacheResult {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CacheResult);

  enum { V2, V4 };

  virtual int Ver() const = 0;
  virtual bool findCompletion(const Completion& aCompletion) const = 0;

  template<typename T>
  static const T* Cast(const CacheResult* aThat) {
    return ((aThat && T::VER == aThat->Ver()) ?
      reinterpret_cast<const T*>(aThat) : nullptr);
  }

  nsCString table;
  Prefix prefix;

protected:
  virtual ~CacheResult() {}
};

class CacheResultV2 final : public CacheResult
{
public:
  static const int VER;

  // True when 'prefix' in CacheResult indicates a prefix that
  // cannot be completed.
  bool miss = false;

  // 'completion' and 'addChunk' are used when 'miss' field is false.
  Completion completion;
  uint32_t addChunk;

  bool operator==(const CacheResultV2& aOther) const {
    if (table != aOther.table ||
        prefix != aOther.prefix ||
        miss != aOther.miss) {
      return false;
    }

    if (miss) {
      return true;
    }
    return completion == aOther.completion && addChunk == aOther.addChunk;
  }

  bool findCompletion(const Completion& aCompletion) const override {
    return completion == aCompletion;
  }

  virtual int Ver() const override { return VER; }
};

class CacheResultV4 final : public CacheResult
{
public:
  static const int VER;

  CachedFullHashResponse response;

  bool operator==(const CacheResultV4& aOther) const {
    return table == aOther.table &&
           prefix == aOther.prefix &&
           response == aOther.response;
  }

  bool findCompletion(const Completion& aCompletion) const override {
    nsDependentCSubstring completion(
      reinterpret_cast<const char*>(aCompletion.buf), COMPLETE_SIZE);
    return response.fullHashes.Contains(completion);
  }

  virtual int Ver() const override { return VER; }
};

typedef nsTArray<RefPtr<const CacheResult>> ConstCacheResultArray;

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

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(LookupCache);

  const nsCString &TableName() const { return mTableName; }

  // The directory handle where we operate will
  // be moved away when a backup is made.
  nsresult UpdateRootDirHandle(nsIFile* aRootStoreDirectory);

  // Write data stored in lookup cache to disk.
  nsresult WriteFile();

  bool IsPrimed() const { return mPrimed; };

  // Called when update to clear expired entries.
  void InvalidateExpiredCacheEntries();

  // Copy fullhash cache from another LookupCache.
  void CopyFullHashCache(const LookupCache* aSource);

  // Clear fullhash cache from fullhash/gethash response.
  void ClearCache();

  // Check if completions can be found in cache.
  // Currently this is only used by testcase.
  bool IsInCache(uint32_t key) const { return mFullHashCache.Get(key); };

#if DEBUG
  void DumpCache() const;
#endif

  void GetCacheInfo(nsIUrlClassifierCacheInfo** aCache) const;

  virtual nsresult Open();
  virtual nsresult Init() = 0;
  virtual nsresult ClearPrefixes() = 0;
  virtual nsresult Has(const Completion& aCompletion,
                       bool* aHas,
                       uint32_t* aMatchLength,
                       bool* aConfirmed) = 0;

  virtual bool IsEmpty() const = 0;

  virtual void ClearAll();

  template<typename T>
  static T* Cast(LookupCache* aThat) {
    return ((aThat && T::VER == aThat->Ver()) ? reinterpret_cast<T*>(aThat) : nullptr);
  }
  template<typename T>
  static const T* Cast(const LookupCache* aThat) {
    return ((aThat && T::VER == aThat->Ver()) ? reinterpret_cast<const T*>(aThat) : nullptr);
  }

private:
  nsresult LoadPrefixSet();

  virtual nsresult StoreToFile(nsIFile* aFile) = 0;
  virtual nsresult LoadFromFile(nsIFile* aFile) = 0;
  virtual size_t SizeOfPrefixSet() const = 0;

  virtual int Ver() const = 0;

protected:
  virtual ~LookupCache() {}

  // Check completions in positive cache and prefix in negative cache.
  // 'aHas' and 'aConfirmed' are output parameters.
  nsresult CheckCache(const Completion& aCompletion,
                      bool* aHas,
                      bool* aConfirmed);

  bool mPrimed; // true when the PrefixSet has been loaded (or constructed)
  const nsCString mTableName;
  const nsCString mProvider;
  nsCOMPtr<nsIFile> mRootStoreDirectory;
  nsCOMPtr<nsIFile> mStoreDirectory;

  // For gtest to inspect private members.
  friend class PerProviderDirectoryTestUtils;

  // Cache stores fullhash response(V4)/gethash response(V2)
  FullHashResponseMap mFullHashCache;
};

typedef nsTArray<RefPtr<LookupCache>> LookupCacheArray;

class LookupCacheV2 final : public LookupCache
{
public:
  explicit LookupCacheV2(const nsACString& aTableName,
                         const nsACString& aProvider,
                         nsIFile* aStoreFile)
    : LookupCache(aTableName, aProvider, aStoreFile) {}

  virtual nsresult Init() override;
  virtual nsresult Open() override;
  virtual void ClearAll() override;
  virtual nsresult Has(const Completion& aCompletion,
                       bool* aHas,
                       uint32_t* aMatchLength,
                       bool* aConfirmed) override;

  virtual bool IsEmpty() const override;

  nsresult Build(AddPrefixArray& aAddPrefixes,
                 AddCompleteArray& aAddCompletes);

  nsresult GetPrefixes(FallibleTArray<uint32_t>& aAddPrefixes);

  // This will Clear() the passed arrays when done.
  // 'aExpirySec' is used by testcase to config an expired time.
  void AddGethashResultToCache(const AddCompleteArray& aAddCompletes,
                               const MissPrefixArray& aMissPrefixes,
                               int64_t aExpirySec = 0);

#if DEBUG
  void DumpCompletions() const;
#endif

  static const int VER;

protected:
  nsresult ReadCompletions();

  virtual nsresult ClearPrefixes() override;
  virtual nsresult StoreToFile(nsIFile* aFile) override;
  virtual nsresult LoadFromFile(nsIFile* aFile) override;
  virtual size_t SizeOfPrefixSet() const override;

private:
  ~LookupCacheV2() {}

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
