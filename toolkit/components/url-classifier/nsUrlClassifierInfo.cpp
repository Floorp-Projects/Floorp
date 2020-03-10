/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsComponentManagerUtils.h"
#include "nsIMutableArray.h"
#include "nsUrlClassifierInfo.h"

NS_IMPL_ISUPPORTS(nsUrlClassifierPositiveCacheEntry,
                  nsIUrlClassifierPositiveCacheEntry)

nsUrlClassifierPositiveCacheEntry::nsUrlClassifierPositiveCacheEntry()
    : expirySec(-1) {}

NS_IMETHODIMP
nsUrlClassifierPositiveCacheEntry::GetExpiry(int64_t* aExpiry) {
  if (!aExpiry) {
    return NS_ERROR_NULL_POINTER;
  }

  *aExpiry = expirySec;
  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierPositiveCacheEntry::GetFullhash(nsACString& aFullHash) {
  aFullHash = fullhash;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsUrlClassifierCacheEntry, nsIUrlClassifierCacheEntry)

nsUrlClassifierCacheEntry::nsUrlClassifierCacheEntry() : expirySec(-1) {}

NS_IMETHODIMP
nsUrlClassifierCacheEntry::GetPrefix(nsACString& aPrefix) {
  aPrefix = prefix;
  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierCacheEntry::GetExpiry(int64_t* aExpiry) {
  if (!aExpiry) {
    return NS_ERROR_NULL_POINTER;
  }

  *aExpiry = expirySec;
  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierCacheEntry::GetMatches(nsIArray** aMatches) {
  if (!aMatches) {
    return NS_ERROR_NULL_POINTER;
  }

  nsCOMPtr<nsIMutableArray> array(do_CreateInstance(NS_ARRAY_CONTRACTID));

  for (uint32_t i = 0; i < matches.Length(); i++) {
    array->AppendElement(matches[i]);
  }

  NS_ADDREF(*aMatches = array);

  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsUrlClassifierCacheInfo, nsIUrlClassifierCacheInfo)

nsUrlClassifierCacheInfo::nsUrlClassifierCacheInfo() = default;

NS_IMETHODIMP
nsUrlClassifierCacheInfo::GetTable(nsACString& aTable) {
  aTable = table;
  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierCacheInfo::GetEntries(nsIArray** aEntries) {
  if (!aEntries) {
    return NS_ERROR_NULL_POINTER;
  }

  nsCOMPtr<nsIMutableArray> array(do_CreateInstance(NS_ARRAY_CONTRACTID));

  for (uint32_t i = 0; i < entries.Length(); i++) {
    array->AppendElement(entries[i]);
  }

  NS_ADDREF(*aEntries = array);

  return NS_OK;
}
