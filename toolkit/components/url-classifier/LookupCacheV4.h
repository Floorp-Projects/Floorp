//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LookupCacheV4_h__
#define LookupCacheV4_h__

#include "LookupCache.h"

namespace mozilla {
namespace safebrowsing {

// Forward declaration.
class TableUpdateV4;

class LookupCacheV4 final : public LookupCache
{
public:
  explicit LookupCacheV4(const nsACString& aTableName,
                         const nsACString& aProvider,
                         nsIFile* aStoreFile)
    : LookupCache(aTableName, aProvider, aStoreFile) {}
  ~LookupCacheV4() {}

  virtual nsresult Init() override;
  virtual nsresult Has(const Completion& aCompletion,
                       const TableFreshnessMap& aTableFreshness,
                       uint32_t aFreshnessGuarantee,
                       bool* aHas, uint32_t* aMatchLength,
                       bool* aConfirmed, bool* aFromCache) override;

  virtual void ClearCache() override;

#if DEBUG
  virtual void DumpCache() override;
#endif

  virtual bool IsEmpty() override;

  nsresult Build(PrefixStringMap& aPrefixMap);

  nsresult GetPrefixes(PrefixStringMap& aPrefixMap);
  nsresult GetFixedLengthPrefixes(FallibleTArray<uint32_t>& aPrefixes);

  // ApplyUpdate will merge data stored in aTableUpdate with prefixes in aInputMap.
  nsresult ApplyUpdate(TableUpdateV4* aTableUpdate,
                       PrefixStringMap& aInputMap,
                       PrefixStringMap& aOutputMap);

  nsresult AddFullHashResponseToCache(const FullHashResponseMap& aResponseMap);

  nsresult WriteMetadata(TableUpdateV4* aTableUpdate);
  nsresult LoadMetadata(nsACString& aState, nsACString& aChecksum);

  void InvalidateExpiredCacheEntry();

  static const int VER;

protected:
  virtual nsresult ClearPrefixes() override;
  virtual nsresult StoreToFile(nsIFile* aFile) override;
  virtual nsresult LoadFromFile(nsIFile* aFile) override;
  virtual size_t SizeOfPrefixSet() override;

private:
  virtual int Ver() const override { return VER; }

  nsresult InitCrypto(nsCOMPtr<nsICryptoHash>& aCrypto);
  nsresult VerifyChecksum(const nsACString& aChecksum);

  RefPtr<VariableLengthPrefixSet> mVLPrefixSet;

  FullHashResponseMap mCache;
};

} // namespace safebrowsing
} // namespace mozilla

#endif
