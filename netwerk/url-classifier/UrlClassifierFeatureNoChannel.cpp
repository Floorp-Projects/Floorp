/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UrlClassifierFeatureNoChannel.h"

namespace mozilla {
namespace net {

struct UrlClassifierFeatureNoChannel::NoChannelFeature {
  const char* mName;
  const char* mBlacklistPrefTables;
  bool (*mPref)();

  RefPtr<UrlClassifierFeatureNoChannel> mFeature;
};

namespace {

struct UrlClassifierFeatureNoChannel::NoChannelFeature sNoChannelFeaturesMap[] =
    {
        {"malware", "urlclassifier.malwareTable",
         StaticPrefs::browser_safebrowsing_malware_enabled},
        {"phishing", "urlclassifier.phishTable",
         StaticPrefs::browser_safebrowsing_phishing_enabled},
        {"blockedURIs", "urlclassifier.blockedTable",
         StaticPrefs::browser_safebrowsing_blockedURIs_enabled},
};

}  // namespace

UrlClassifierFeatureNoChannel::UrlClassifierFeatureNoChannel(
    const UrlClassifierFeatureNoChannel::NoChannelFeature& aFeature)
    : UrlClassifierFeatureBase(
          nsDependentCString(aFeature.mName),
          nsDependentCString(aFeature.mBlacklistPrefTables),
          EmptyCString(),    // aPrefWhitelistPrefTbles,
          EmptyCString(),    // aPrefBlacklistHosts
          EmptyCString(),    // aPrefWhitelistHosts
          EmptyCString(),    // aPrefBlacklistTableName
          EmptyCString(),    // aPrefWhitelistTableName
          EmptyCString()) {  // aPrefSkipHosts
}

/* static */ void UrlClassifierFeatureNoChannel::GetFeatureNames(
    nsTArray<nsCString>& aArray) {
  for (const NoChannelFeature& feature : sNoChannelFeaturesMap) {
    if (feature.mPref()) {
      aArray.AppendElement(nsDependentCString(feature.mName));
    }
  }
}

/* static */ void UrlClassifierFeatureNoChannel::MaybeInitialize() {
  for (NoChannelFeature& feature : sNoChannelFeaturesMap) {
    if (!feature.mFeature && feature.mPref()) {
      feature.mFeature = new UrlClassifierFeatureNoChannel(feature);
      feature.mFeature->InitializePreferences();
    }
  }
}

/* static */ void UrlClassifierFeatureNoChannel::MaybeShutdown() {
  for (NoChannelFeature& feature : sNoChannelFeaturesMap) {
    if (feature.mFeature) {
      feature.mFeature->ShutdownPreferences();
      feature.mFeature = nullptr;
    }
  }
}

/* static */ void UrlClassifierFeatureNoChannel::MaybeCreate(
    nsTArray<RefPtr<nsIUrlClassifierFeature>>& aFeatures) {
  MaybeInitialize();

  for (const NoChannelFeature& feature : sNoChannelFeaturesMap) {
    if (feature.mPref()) {
      MOZ_ASSERT(feature.mFeature);
      aFeatures.AppendElement(feature.mFeature);
    }
  }
}

/* static */ already_AddRefed<nsIUrlClassifierFeature>
UrlClassifierFeatureNoChannel::GetIfNameMatches(const nsACString& aName) {
  MaybeInitialize();

  for (const NoChannelFeature& feature : sNoChannelFeaturesMap) {
    if (feature.mPref() && aName.Equals(feature.mName)) {
      MOZ_ASSERT(feature.mFeature);
      nsCOMPtr<nsIUrlClassifierFeature> self = feature.mFeature.get();
      return self.forget();
    }
  }

  return nullptr;
}

NS_IMETHODIMP
UrlClassifierFeatureNoChannel::ProcessChannel(nsIChannel* aChannel,
                                              const nsACString& aList,
                                              bool* aShouldContinue) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
UrlClassifierFeatureNoChannel::GetURIByListType(
    nsIChannel* aChannel, nsIUrlClassifierFeature::listType aListType,
    nsIURI** aURI) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

}  // namespace net
}  // namespace mozilla
