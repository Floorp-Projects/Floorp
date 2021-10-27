/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UrlClassifierFeaturePhishingProtection.h"
#include "mozilla/StaticPrefs_browser.h"
#include "nsCOMPtr.h"

namespace mozilla {
namespace net {

struct UrlClassifierFeaturePhishingProtection::PhishingProtectionFeature {
  const char* mName;
  const char* mBlocklistPrefTables;
  bool (*mPref)();

  RefPtr<UrlClassifierFeaturePhishingProtection> mFeature;
};

namespace {

struct UrlClassifierFeaturePhishingProtection::PhishingProtectionFeature
    sPhishingProtectionFeaturesMap[] = {
        {"malware", "urlclassifier.malwareTable",
         StaticPrefs::browser_safebrowsing_malware_enabled},
        {"phishing", "urlclassifier.phishTable",
         StaticPrefs::browser_safebrowsing_phishing_enabled},
        {"blockedURIs", "urlclassifier.blockedTable",
         StaticPrefs::browser_safebrowsing_blockedURIs_enabled},
};

}  // namespace

UrlClassifierFeaturePhishingProtection::UrlClassifierFeaturePhishingProtection(
    const UrlClassifierFeaturePhishingProtection::PhishingProtectionFeature&
        aFeature)
    : UrlClassifierFeatureBase(
          nsDependentCString(aFeature.mName),
          nsDependentCString(aFeature.mBlocklistPrefTables),
          ""_ns,    // aPrefEntitylistPrefTbles,
          ""_ns,    // aPrefBlocklistHosts
          ""_ns,    // aPrefEntitylistHosts
          ""_ns,    // aPrefBlocklistTableName
          ""_ns,    // aPrefEntitylistTableName
          ""_ns) {  // aPrefExceptionHosts
}

/* static */
void UrlClassifierFeaturePhishingProtection::GetFeatureNames(
    nsTArray<nsCString>& aArray) {
  for (const PhishingProtectionFeature& feature :
       sPhishingProtectionFeaturesMap) {
    if (feature.mPref()) {
      aArray.AppendElement(nsDependentCString(feature.mName));
    }
  }
}

/* static */
void UrlClassifierFeaturePhishingProtection::MaybeInitialize() {
  for (PhishingProtectionFeature& feature : sPhishingProtectionFeaturesMap) {
    if (!feature.mFeature && feature.mPref()) {
      feature.mFeature = new UrlClassifierFeaturePhishingProtection(feature);
      feature.mFeature->InitializePreferences();
    }
  }
}

/* static */
void UrlClassifierFeaturePhishingProtection::MaybeShutdown() {
  for (PhishingProtectionFeature& feature : sPhishingProtectionFeaturesMap) {
    if (feature.mFeature) {
      feature.mFeature->ShutdownPreferences();
      feature.mFeature = nullptr;
    }
  }
}

/* static */
void UrlClassifierFeaturePhishingProtection::MaybeCreate(
    nsTArray<RefPtr<nsIUrlClassifierFeature>>& aFeatures) {
  MaybeInitialize();

  for (const PhishingProtectionFeature& feature :
       sPhishingProtectionFeaturesMap) {
    if (feature.mPref()) {
      MOZ_ASSERT(feature.mFeature);
      aFeatures.AppendElement(feature.mFeature);
    }
  }
}

/* static */
already_AddRefed<nsIUrlClassifierFeature>
UrlClassifierFeaturePhishingProtection::GetIfNameMatches(
    const nsACString& aName) {
  MaybeInitialize();

  for (const PhishingProtectionFeature& feature :
       sPhishingProtectionFeaturesMap) {
    if (feature.mPref() && aName.Equals(feature.mName)) {
      MOZ_ASSERT(feature.mFeature);
      nsCOMPtr<nsIUrlClassifierFeature> self = feature.mFeature.get();
      return self.forget();
    }
  }

  return nullptr;
}

NS_IMETHODIMP
UrlClassifierFeaturePhishingProtection::ProcessChannel(
    nsIChannel* aChannel, const nsTArray<nsCString>& aList,
    const nsTArray<nsCString>& aHashes, bool* aShouldContinue) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
UrlClassifierFeaturePhishingProtection::GetURIByListType(
    nsIChannel* aChannel, nsIUrlClassifierFeature::listType aListType,
    nsIUrlClassifierFeature::URIType* aURIType, nsIURI** aURI) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

}  // namespace net
}  // namespace mozilla
