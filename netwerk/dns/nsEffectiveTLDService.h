//* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla TLD Service
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pamela Greene <pamg.bugs@gmail.com> (original author)
 *   Daniel Witte <dwitte@stanford.edu>
 *   Jeff Walden <jwalden+code@mit.edu>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

  enum { ALLOW_MEMMOVE = PR_TRUE };

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
