//* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIEffectiveTLDService.h"

#include "nsTHashtable.h"
#include "nsString.h"
#include "nsCOMPtr.h"

class nsIIDNService;

// struct for static data generated from effective_tld_names.dat
struct ETLDEntry {
  const char* domain;
  bool exception;
  bool wild;
};


// hash entry class
class nsDomainEntry : public PLDHashEntryHdr
{
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
    return mData->domain;
  }

  bool KeyEquals(KeyTypePointer aKey) const
  {
    return !strcmp(mData->domain, aKey);
  }

  static KeyTypePointer KeyToPointer(KeyType aKey)
  {
    return aKey;
  }

  static PLDHashNumber HashKey(KeyTypePointer aKey)
  {
    // PL_DHashStringKey doesn't use the table parameter, so we can safely
    // pass nsnull
    return PL_DHashStringKey(nsnull, aKey);
  }

  enum { ALLOW_MEMMOVE = true };

  void SetData(const ETLDEntry* entry) { mData = entry; }

  bool IsNormal() { return mData->wild || !mData->exception; }
  bool IsException() { return mData->exception; }
  bool IsWild() { return mData->wild; }

private:
  const ETLDEntry* mData;
};

class nsEffectiveTLDService : public nsIEffectiveTLDService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIEFFECTIVETLDSERVICE

  nsEffectiveTLDService() { }
  nsresult Init();

private:
  nsresult GetBaseDomainInternal(nsCString &aHostname, PRUint32 aAdditionalParts, nsACString &aBaseDomain);
  nsresult NormalizeHostname(nsCString &aHostname);
  ~nsEffectiveTLDService() { }

  nsTHashtable<nsDomainEntry> mHash;
  nsCOMPtr<nsIIDNService>     mIDNService;
};
