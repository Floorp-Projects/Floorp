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

class LookupCacheV4 final : public LookupCache {
 public:
  explicit LookupCacheV4(const nsACString& aTableName,
                         const nsACString& aProvider,
                         nsCOMPtr<nsIFile>& aStoreFile)
      : LookupCache(aTableName, aProvider, aStoreFile) {}

  virtual nsresult Has(const Completion& aCompletion, bool* aHas,
                       uint32_t* aMatchLength, bool* aConfirmed) override;

  nsresult Build(PrefixStringMap& aPrefixMap);

  nsresult GetPrefixes(PrefixStringMap& aPrefixMap);
  nsresult GetFixedLengthPrefixes(FallibleTArray<uint32_t>& aPrefixes);

  // ApplyUpdate will merge data stored in aTableUpdate with prefixes in
  // aInputMap.
  nsresult ApplyUpdate(RefPtr<TableUpdateV4> aTableUpdate,
                       PrefixStringMap& aInputMap, PrefixStringMap& aOutputMap);

  nsresult AddFullHashResponseToCache(const FullHashResponseMap& aResponseMap);

  nsresult WriteMetadata(RefPtr<const TableUpdateV4> aTableUpdate);
  nsresult LoadMetadata(nsACString& aState, nsACString& aChecksum);

  virtual nsresult LoadMozEntries() override;

  static const int VER;
  static const uint32_t MAX_METADATA_VALUE_LENGTH;
  static const uint32_t VLPSET_MAGIC;
  static const uint32_t VLPSET_VERSION;

 protected:
  virtual nsCString GetPrefixSetSuffix() const override;
  nsCString GetMetadataSuffix() const;

 private:
  ~LookupCacheV4() = default;

  virtual int Ver() const override { return VER; }

  virtual nsresult LoadLegacyFile() override;
  virtual nsresult ClearLegacyFile() override;

  virtual void GetHeader(Header& aHeader) override;
  virtual nsresult SanityCheck(const Header& aHeader) override;
};

}  // namespace safebrowsing
}  // namespace mozilla

#endif
