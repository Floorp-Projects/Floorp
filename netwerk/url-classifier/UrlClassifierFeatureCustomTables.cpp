/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UrlClassifierFeatureCustomTables.h"

namespace mozilla {

NS_INTERFACE_MAP_BEGIN(UrlClassifierFeatureCustomTables)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIUrlClassifierFeature)
  NS_INTERFACE_MAP_ENTRY(nsIUrlClassifierFeature)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(UrlClassifierFeatureCustomTables)
NS_IMPL_RELEASE(UrlClassifierFeatureCustomTables)

UrlClassifierFeatureCustomTables::UrlClassifierFeatureCustomTables(
    const nsACString& aName, const nsTArray<nsCString>& aBlacklistTables,
    const nsTArray<nsCString>& aWhitelistTables)
    : mName(aName),
      mBlacklistTables(aBlacklistTables.Clone()),
      mWhitelistTables(aWhitelistTables.Clone()) {}

UrlClassifierFeatureCustomTables::~UrlClassifierFeatureCustomTables() = default;

NS_IMETHODIMP
UrlClassifierFeatureCustomTables::GetName(nsACString& aName) {
  aName = mName;
  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierFeatureCustomTables::GetTables(
    nsIUrlClassifierFeature::listType aListType, nsTArray<nsCString>& aTables) {
  if (aListType == nsIUrlClassifierFeature::blacklist) {
    aTables = mBlacklistTables.Clone();
    return NS_OK;
  }

  if (aListType == nsIUrlClassifierFeature::whitelist) {
    aTables = mWhitelistTables.Clone();
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
UrlClassifierFeatureCustomTables::HasTable(
    const nsACString& aTable, nsIUrlClassifierFeature::listType aListType,
    bool* aResult) {
  NS_ENSURE_ARG_POINTER(aResult);

  if (aListType == nsIUrlClassifierFeature::blacklist) {
    *aResult = mBlacklistTables.Contains(aTable);
    return NS_OK;
  }

  if (aListType == nsIUrlClassifierFeature::whitelist) {
    *aResult = mWhitelistTables.Contains(aTable);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
UrlClassifierFeatureCustomTables::HasHostInPreferences(
    const nsACString& aHost, nsIUrlClassifierFeature::listType aListType,
    nsACString& aPrefTableName, bool* aResult) {
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = false;
  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierFeatureCustomTables::GetSkipHostList(nsACString& aList) {
  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierFeatureCustomTables::ProcessChannel(
    nsIChannel* aChannel, const nsTArray<nsCString>& aList,
    const nsTArray<nsCString>& aHashes, bool* aShouldContinue) {
  NS_ENSURE_ARG_POINTER(aChannel);
  NS_ENSURE_ARG_POINTER(aShouldContinue);

  // This is not a blocking feature.
  *aShouldContinue = true;

  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierFeatureCustomTables::GetURIByListType(
    nsIChannel* aChannel, nsIUrlClassifierFeature::listType aListType,
    nsIUrlClassifierFeature::URIType* aURIType, nsIURI** aURI) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

}  // namespace mozilla
