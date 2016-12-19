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
                       bool* aHas, bool* aComplete,
                       uint32_t* aMatchLength) override;

  nsresult Build(PrefixStringMap& aPrefixMap);

  nsresult GetPrefixes(PrefixStringMap& aPrefixMap);

  // ApplyUpdate will merge data stored in aTableUpdate with prefixes in aInputMap.
  nsresult ApplyUpdate(TableUpdateV4* aTableUpdate,
                       PrefixStringMap& aInputMap,
                       PrefixStringMap& aOutputMap);

  nsresult WriteMetadata(TableUpdateV4* aTableUpdate);
  nsresult LoadMetadata(nsACString& aState, nsACString& aChecksum);

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

  enum UPDATE_ERROR_TYPES {
    DUPLICATE_PREFIX = 0,
    INFINITE_LOOP = 1,
    WRONG_REMOVAL_INDICES = 2,
    CHECKSUM_MISMATCH = 3,
    MISSING_CHECKSUM = 4,
  };

  RefPtr<VariableLengthPrefixSet> mVLPrefixSet;
};

} // namespace safebrowsing
} // namespace mozilla

#endif
