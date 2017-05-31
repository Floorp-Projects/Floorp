/* This Source Code Form is subject to the terms of the Mozilla
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUrlClassifierInfo.h"

NS_IMPL_ISUPPORTS(nsUrlClassifierPositiveCacheEntry,
                  nsIUrlClassifierPositiveCacheEntry)

nsUrlClassifierPositiveCacheEntry::nsUrlClassifierPositiveCacheEntry()
  : expirySec(-1)
{
}

NS_IMETHODIMP
nsUrlClassifierPositiveCacheEntry::GetExpiry(int64_t* aExpiry)
{
  if (!aExpiry) {
    return NS_ERROR_NULL_POINTER;
  }

  *aExpiry = expirySec;
  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierPositiveCacheEntry::GetFullhash(nsACString& aFullHash)
{
  aFullHash = fullhash;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsUrlClassifierCacheEntry,
                  nsIUrlClassifierCacheEntry)

nsUrlClassifierCacheEntry::nsUrlClassifierCacheEntry()
  : expirySec(-1)
{
}

NS_IMETHODIMP
nsUrlClassifierCacheEntry::GetPrefix(nsACString& aPrefix)
{
  aPrefix = prefix;
  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierCacheEntry::GetExpiry(int64_t* aExpiry)
{
  if (!aExpiry) {
    return NS_ERROR_NULL_POINTER;
  }

  *aExpiry = expirySec;
  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierCacheEntry::GetMatches(nsIArray** aMatches)
{
  if (!aMatches) {
    return NS_ERROR_NULL_POINTER;
  }

  nsCOMPtr<nsIMutableArray> array(do_CreateInstance(NS_ARRAY_CONTRACTID));

  for (uint32_t i = 0;i < matches.Length(); i++) {
    array->AppendElement(matches[i], false);
  }

  NS_ADDREF(*aMatches = array);

  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsUrlClassifierCacheInfo,
                  nsIUrlClassifierCacheInfo)

nsUrlClassifierCacheInfo::nsUrlClassifierCacheInfo()
{
}

NS_IMETHODIMP
nsUrlClassifierCacheInfo::GetTable(nsACString& aTable)
{
  aTable = table;
  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierCacheInfo::GetEntries(nsIArray** aEntries)
{
  if (!aEntries) {
    return NS_ERROR_NULL_POINTER;
  }

  nsCOMPtr<nsIMutableArray> array(do_CreateInstance(NS_ARRAY_CONTRACTID));

  for (uint32_t i = 0;i < entries.Length(); i++) {
    array->AppendElement(entries[i], false);
  }

  NS_ADDREF(*aEntries = array);

  return NS_OK;
}
