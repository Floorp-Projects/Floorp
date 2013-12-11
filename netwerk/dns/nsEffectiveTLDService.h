//* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIEffectiveTLDService.h"

#include "nsIMemoryReporter.h"
#include "nsTHashtable.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "mozilla/Attributes.h"
#include "mozilla/MemoryReporting.h"

class nsIIDNService;

#define ETLD_ENTRY_N_INDEX_BITS 30

// struct for static data generated from effective_tld_names.dat
struct ETLDEntry {
  uint32_t strtab_index : ETLD_ENTRY_N_INDEX_BITS;
  uint32_t exception : 1;
  uint32_t wild : 1;
};


// hash entry class
class nsDomainEntry : public PLDHashEntryHdr
{
  friend class nsEffectiveTLDService;
public:
  // Hash methods
  typedef const char* KeyType;
  typedef const char* KeyTypePointer;

  nsDomainEntry(KeyTypePointer aEntry)
  {
  }

  nsDomainEntry(const nsDomainEntry& toCopy)
  {
    // if we end up here, things will break. nsTHashtable shouldn't
    // allow this, since we set ALLOW_MEMMOVE to true.
    NS_NOTREACHED("nsDomainEntry copy constructor is forbidden!");
  }

  ~nsDomainEntry()
  {
  }

  KeyType GetKey() const
  {
    return GetEffectiveTLDName(mData->strtab_index);
  }

  bool KeyEquals(KeyTypePointer aKey) const
  {
    return !strcmp(GetKey(), aKey);
  }

  static KeyTypePointer KeyToPointer(KeyType aKey)
  {
    return aKey;
  }

  static PLDHashNumber HashKey(KeyTypePointer aKey)
  {
    // PL_DHashStringKey doesn't use the table parameter, so we can safely
    // pass nullptr
    return PL_DHashStringKey(nullptr, aKey);
  }

  enum { ALLOW_MEMMOVE = true };

  void SetData(const ETLDEntry* entry) { mData = entry; }

  bool IsNormal() { return mData->wild || !mData->exception; }
  bool IsException() { return mData->exception; }
  bool IsWild() { return mData->wild; }

  static const char *GetEffectiveTLDName(size_t idx)
  {
    return strings.strtab + idx;
  }

private:
  const ETLDEntry* mData;
#define ETLD_STR_NUM_1(line) str##line
#define ETLD_STR_NUM(line) ETLD_STR_NUM_1(line)
  struct etld_string_list {
#define ETLD_ENTRY(name, ex, wild) char ETLD_STR_NUM(__LINE__)[sizeof(name)];
#include "etld_data.inc"
#undef ETLD_ENTRY
  };
  static const union etld_strings {
    struct etld_string_list list;
    char strtab[1];
  } strings;
  static const ETLDEntry entries[];
  void FuncForStaticAsserts(void);
#undef ETLD_STR_NUM
#undef ETLD_STR_NUM1
};

class nsEffectiveTLDService MOZ_FINAL
  : public mozilla::MemoryUniReporter
  , public nsIEffectiveTLDService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIEFFECTIVETLDSERVICE

  nsEffectiveTLDService();
  nsresult Init();

  int64_t Amount() MOZ_OVERRIDE;
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf);

private:
  nsresult GetBaseDomainInternal(nsCString &aHostname, int32_t aAdditionalParts, nsACString &aBaseDomain);
  nsresult NormalizeHostname(nsCString &aHostname);
  ~nsEffectiveTLDService();

  nsTHashtable<nsDomainEntry> mHash;
  nsCOMPtr<nsIIDNService>     mIDNService;
};
