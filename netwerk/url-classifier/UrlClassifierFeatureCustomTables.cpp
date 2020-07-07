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
    const nsACString& aName, const nsTArray<nsCString>& aBlocklistTables,
    const nsTArray<nsCString>& aEntitylistTables)
    : mName(aName),
      mBlocklistTables(aBlocklistTables.Clone()),
      mEntitylistTables(aEntitylistTables.Clone()) {}

UrlClassifierFeatureCustomTables::~UrlClassifierFeatureCustomTables() = default;

NS_IMETHODIMP
UrlClassifierFeatureCustomTables::GetName(nsACString& aName) {
  aName = mName;
  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierFeatureCustomTables::GetTables(
    nsIUrlClassifierFeature::listType aListType, nsTArray<nsCString>& aTables) {
  if (aListType == nsIUrlClassifierFeature::blocklist) {
    aTables = mBlocklistTables.Clone();
    return NS_OK;
  }

  if (aListType == nsIUrlClassifierFeature::entitylist) {
    aTables = mEntitylistTables.Clone();
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
UrlClassifierFeatureCustomTables::HasTable(
    const nsACString& aTable, nsIUrlClassifierFeature::listType aListType,
    bool* aResult) {
  NS_ENSURE_ARG_POINTER(aResult);

  if (aListType == nsIUrlClassifierFeature::blocklist) {
    *aResult = mBlocklistTables.Contains(aTable);
    return NS_OK;
  }

  if (aListType == nsIUrlClassifierFeature::entitylist) {
    *aResult = mEntitylistTables.Contains(aTable);
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
UrlClassifierFeatureCustomTables::GetExceptionHostList(nsACString& aList) {
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
