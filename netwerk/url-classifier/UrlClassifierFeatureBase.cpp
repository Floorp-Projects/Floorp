/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UrlClassifierFeatureBase.h"
#include "Classifier.h"
#include "mozilla/Preferences.h"

namespace mozilla {

using namespace safebrowsing;

namespace net {

namespace {

void OnPrefsChange(const char* aPrefName, void* aArray) {
  auto array = static_cast<nsTArray<nsCString>*>(aArray);
  MOZ_ASSERT(array);

  nsAutoCString value;
  Preferences::GetCString(aPrefName, value);
  Classifier::SplitTables(value, *array);
}

}  // namespace

NS_INTERFACE_MAP_BEGIN(UrlClassifierFeatureBase)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIUrlClassifierFeature)
  NS_INTERFACE_MAP_ENTRY(nsIUrlClassifierFeature)
  NS_INTERFACE_MAP_ENTRY(nsIUrlClassifierSkipListObserver)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(UrlClassifierFeatureBase)
NS_IMPL_RELEASE(UrlClassifierFeatureBase)

UrlClassifierFeatureBase::UrlClassifierFeatureBase(
    const nsACString& aName, const nsACString& aPrefBlacklistTables,
    const nsACString& aPrefWhitelistTables,
    const nsACString& aPrefBlacklistHosts,
    const nsACString& aPrefWhitelistHosts,
    const nsACString& aPrefBlacklistTableName,
    const nsACString& aPrefWhitelistTableName, const nsACString& aPrefSkipHosts)
    : mName(aName), mPrefSkipHosts(aPrefSkipHosts) {
  static_assert(nsIUrlClassifierFeature::blacklist == 0,
                "nsIUrlClassifierFeature::blacklist must be 0");
  static_assert(nsIUrlClassifierFeature::whitelist == 1,
                "nsIUrlClassifierFeature::whitelist must be 1");

  mPrefTables[nsIUrlClassifierFeature::blacklist] = aPrefBlacklistTables;
  mPrefTables[nsIUrlClassifierFeature::whitelist] = aPrefWhitelistTables;

  mPrefHosts[nsIUrlClassifierFeature::blacklist] = aPrefBlacklistHosts;
  mPrefHosts[nsIUrlClassifierFeature::whitelist] = aPrefWhitelistHosts;

  mPrefTableNames[nsIUrlClassifierFeature::blacklist] = aPrefBlacklistTableName;
  mPrefTableNames[nsIUrlClassifierFeature::whitelist] = aPrefWhitelistTableName;
}

UrlClassifierFeatureBase::~UrlClassifierFeatureBase() = default;

void UrlClassifierFeatureBase::InitializePreferences() {
  for (uint32_t i = 0; i < 2; ++i) {
    if (!mPrefTables[i].IsEmpty()) {
      Preferences::RegisterCallbackAndCall(OnPrefsChange, mPrefTables[i],
                                           &mTables[i]);
    }

    if (!mPrefHosts[i].IsEmpty()) {
      Preferences::RegisterCallbackAndCall(OnPrefsChange, mPrefHosts[i],
                                           &mHosts[i]);
    }
  }

  nsCOMPtr<nsIUrlClassifierSkipListService> skipListService =
      do_GetService("@mozilla.org/url-classifier/skip-list-service;1");
  if (NS_WARN_IF(!skipListService)) {
    return;
  }

  skipListService->RegisterAndRunSkipListObserver(mName, mPrefSkipHosts, this);
}

void UrlClassifierFeatureBase::ShutdownPreferences() {
  for (uint32_t i = 0; i < 2; ++i) {
    if (!mPrefTables[i].IsEmpty()) {
      Preferences::UnregisterCallback(OnPrefsChange, mPrefTables[i],
                                      &mTables[i]);
    }

    if (!mPrefHosts[i].IsEmpty()) {
      Preferences::UnregisterCallback(OnPrefsChange, mPrefHosts[i], &mHosts[i]);
    }
  }

  nsCOMPtr<nsIUrlClassifierSkipListService> skipListService =
      do_GetService("@mozilla.org/url-classifier/skip-list-service;1");
  if (skipListService) {
    skipListService->UnregisterSkipListObserver(mName, this);
  }
}

NS_IMETHODIMP
UrlClassifierFeatureBase::OnSkipListUpdate(const nsACString& aList) {
  mSkipHosts = aList;
  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierFeatureBase::GetName(nsACString& aName) {
  aName = mName;
  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierFeatureBase::GetTables(nsIUrlClassifierFeature::listType aListType,
                                    nsTArray<nsCString>& aTables) {
  if (aListType != nsIUrlClassifierFeature::blacklist &&
      aListType != nsIUrlClassifierFeature::whitelist) {
    return NS_ERROR_INVALID_ARG;
  }

  aTables = mTables[aListType].Clone();
  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierFeatureBase::HasTable(const nsACString& aTable,
                                   nsIUrlClassifierFeature::listType aListType,
                                   bool* aResult) {
  NS_ENSURE_ARG_POINTER(aResult);

  if (aListType != nsIUrlClassifierFeature::blacklist &&
      aListType != nsIUrlClassifierFeature::whitelist) {
    return NS_ERROR_INVALID_ARG;
  }

  *aResult = mTables[aListType].Contains(aTable);
  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierFeatureBase::HasHostInPreferences(
    const nsACString& aHost, nsIUrlClassifierFeature::listType aListType,
    nsACString& aPrefTableName, bool* aResult) {
  NS_ENSURE_ARG_POINTER(aResult);

  if (aListType != nsIUrlClassifierFeature::blacklist &&
      aListType != nsIUrlClassifierFeature::whitelist) {
    return NS_ERROR_INVALID_ARG;
  }

  *aResult = mHosts[aListType].Contains(aHost);
  if (*aResult) {
    aPrefTableName = mPrefTableNames[aListType];
  }
  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierFeatureBase::GetSkipHostList(nsACString& aList) {
  aList = mSkipHosts;
  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
