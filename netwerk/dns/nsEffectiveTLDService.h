//* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EffectiveTLDService_h
#define EffectiveTLDService_h

#include "nsIEffectiveTLDService.h"

#include "nsIMemoryReporter.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "mozilla/Attributes.h"
#include "mozilla/BinarySearch.h"
#include "mozilla/MemoryReporting.h"

class nsIIDNService;

// struct for static data generated from effective_tld_names.dat
struct ETLDEntry {
  friend class nsEffectiveTLDService;

public:
  bool IsNormal()    const { return wild || !exception; }
  bool IsException() const { return exception; }
  bool IsWild()      const { return wild; }

  const char* GetEffectiveTLDName() const
  {
    return strings.strtab + strtab_index;
  }

  static const ETLDEntry* GetEntry(const char* aDomain);

  static const size_t ETLD_ENTRY_N_INDEX_BITS = 30;

  // These fields must be public to allow static construction.
  uint32_t strtab_index : ETLD_ENTRY_N_INDEX_BITS;
  uint32_t exception : 1;
  uint32_t wild : 1;

private:
  struct Cmp {
    int operator()(const ETLDEntry aEntry) const
    {
      return strcmp(mName, aEntry.GetEffectiveTLDName());
    }
    explicit Cmp(const char* aName) : mName(aName) {}
    const char* mName;
  };

#define ETLD_STR_NUM_1(line) str##line
#define ETLD_STR_NUM(line) ETLD_STR_NUM_1(line)
  struct etld_string_list {
#define ETLD_ENTRY(name, ex, wild) char ETLD_STR_NUM(__LINE__)[sizeof(name)];
#include "etld_data.inc"
#undef ETLD_ENTRY
  };

  // This static string table is all the eTLD domain names packed together.
  static const union etld_strings {
    struct etld_string_list list;
    char strtab[1];
  } strings;

  // This is the static entries table. Each entry has an index into the string
  // table. The entries are in sorted order so that binary search can be used.
  static const ETLDEntry entries[];

  void FuncForStaticAsserts(void);
#undef ETLD_STR_NUM
#undef ETLD_STR_NUM1
};

class nsEffectiveTLDService final
  : public nsIEffectiveTLDService
  , public nsIMemoryReporter
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIEFFECTIVETLDSERVICE
  NS_DECL_NSIMEMORYREPORTER

  nsEffectiveTLDService();
  nsresult Init();

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf);

private:
  nsresult GetBaseDomainInternal(nsCString &aHostname, int32_t aAdditionalParts, nsACString &aBaseDomain);
  nsresult NormalizeHostname(nsCString &aHostname);
  ~nsEffectiveTLDService();

  nsCOMPtr<nsIIDNService>     mIDNService;
};

#endif // EffectiveTLDService_h
