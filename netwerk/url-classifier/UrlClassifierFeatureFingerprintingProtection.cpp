/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UrlClassifierFeatureFingerprintingProtection.h"

#include "mozilla/net/UrlClassifierCommon.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "nsContentUtils.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace net {

namespace {

#define FINGERPRINTING_FEATURE_NAME "fingerprinting-protection"

#define URLCLASSIFIER_FINGERPRINTING_BLACKLIST \
  "urlclassifier.features.fingerprinting.blacklistTables"
#define URLCLASSIFIER_FINGERPRINTING_BLACKLIST_TEST_ENTRIES \
  "urlclassifier.features.fingerprinting.blacklistHosts"
#define URLCLASSIFIER_FINGERPRINTING_WHITELIST \
  "urlclassifier.features.fingerprinting.whitelistTables"
#define URLCLASSIFIER_FINGERPRINTING_WHITELIST_TEST_ENTRIES \
  "urlclassifier.features.fingerprinting.whitelistHosts"
#define URLCLASSIFIER_FINGERPRINTING_SKIP_URLS \
  "urlclassifier.features.fingerprinting.skipURLs"
#define TABLE_FINGERPRINTING_BLACKLIST_PREF "fingerprinting-blacklist-pref"
#define TABLE_FINGERPRINTING_WHITELIST_PREF "fingerprinting-whitelist-pref"

StaticRefPtr<UrlClassifierFeatureFingerprintingProtection>
    gFeatureFingerprintingProtection;

}  // namespace

UrlClassifierFeatureFingerprintingProtection::
    UrlClassifierFeatureFingerprintingProtection()
    : UrlClassifierFeatureBase(
          NS_LITERAL_CSTRING(FINGERPRINTING_FEATURE_NAME),
          NS_LITERAL_CSTRING(URLCLASSIFIER_FINGERPRINTING_BLACKLIST),
          NS_LITERAL_CSTRING(URLCLASSIFIER_FINGERPRINTING_WHITELIST),
          NS_LITERAL_CSTRING(
              URLCLASSIFIER_FINGERPRINTING_BLACKLIST_TEST_ENTRIES),
          NS_LITERAL_CSTRING(
              URLCLASSIFIER_FINGERPRINTING_WHITELIST_TEST_ENTRIES),
          NS_LITERAL_CSTRING(TABLE_FINGERPRINTING_BLACKLIST_PREF),
          NS_LITERAL_CSTRING(TABLE_FINGERPRINTING_WHITELIST_PREF),
          NS_LITERAL_CSTRING(URLCLASSIFIER_FINGERPRINTING_SKIP_URLS)) {}

/* static */ const char* UrlClassifierFeatureFingerprintingProtection::Name() {
  return FINGERPRINTING_FEATURE_NAME;
}

/* static */
void UrlClassifierFeatureFingerprintingProtection::MaybeInitialize() {
  UC_LOG(("UrlClassifierFeatureFingerprintingProtection: MaybeInitialize"));

  if (!gFeatureFingerprintingProtection) {
    gFeatureFingerprintingProtection =
        new UrlClassifierFeatureFingerprintingProtection();
    gFeatureFingerprintingProtection->InitializePreferences();
  }
}

/* static */
void UrlClassifierFeatureFingerprintingProtection::MaybeShutdown() {
  UC_LOG(("UrlClassifierFeatureFingerprintingProtection: MaybeShutdown"));

  if (gFeatureFingerprintingProtection) {
    gFeatureFingerprintingProtection->ShutdownPreferences();
    gFeatureFingerprintingProtection = nullptr;
  }
}

/* static */
already_AddRefed<UrlClassifierFeatureFingerprintingProtection>
UrlClassifierFeatureFingerprintingProtection::MaybeCreate(
    nsIChannel* aChannel) {
  MOZ_ASSERT(aChannel);

  UC_LOG(
      ("UrlClassifierFeatureFingerprintingProtection: MaybeCreate for channel "
       "%p",
       aChannel));

  if (!StaticPrefs::privacy_trackingprotection_fingerprinting_enabled()) {
    return nullptr;
  }

  nsCOMPtr<nsIURI> chanURI;
  nsresult rv = aChannel->GetURI(getter_AddRefs(chanURI));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  bool isThirdParty =
      nsContentUtils::IsThirdPartyWindowOrChannel(nullptr, aChannel, chanURI);
  if (!isThirdParty) {
    if (UC_LOG_ENABLED()) {
      nsCString spec = chanURI->GetSpecOrDefault();
      spec.Truncate(
          std::min(spec.Length(), UrlClassifierCommon::sMaxSpecLength));
      UC_LOG(
          ("UrlClassifierFeatureFingerprintingProtection: Skipping "
           "fingerprinting checks "
           "for first party or top-level load channel[%p] "
           "with uri %s",
           aChannel, spec.get()));
    }
    return nullptr;
  }

  if (UrlClassifierCommon::IsPassiveContent(aChannel)) {
    return nullptr;
  }

  if (!UrlClassifierCommon::ShouldEnableClassifier(aChannel)) {
    return nullptr;
  }

  MaybeInitialize();
  MOZ_ASSERT(gFeatureFingerprintingProtection);

  RefPtr<UrlClassifierFeatureFingerprintingProtection> self =
      gFeatureFingerprintingProtection;
  return self.forget();
}

/* static */
already_AddRefed<nsIUrlClassifierFeature>
UrlClassifierFeatureFingerprintingProtection::GetIfNameMatches(
    const nsACString& aName) {
  if (!aName.EqualsLiteral(FINGERPRINTING_FEATURE_NAME)) {
    return nullptr;
  }

  MaybeInitialize();
  MOZ_ASSERT(gFeatureFingerprintingProtection);

  RefPtr<UrlClassifierFeatureFingerprintingProtection> self =
      gFeatureFingerprintingProtection;
  return self.forget();
}

NS_IMETHODIMP
UrlClassifierFeatureFingerprintingProtection::ProcessChannel(
    nsIChannel* aChannel, const nsTArray<nsCString>& aList,
    const nsTArray<nsCString>& aHashes, bool* aShouldContinue) {
  NS_ENSURE_ARG_POINTER(aChannel);
  NS_ENSURE_ARG_POINTER(aShouldContinue);

  bool isAllowListed = UrlClassifierCommon::IsAllowListed(aChannel);

  // This is a blocking feature.
  *aShouldContinue = isAllowListed;

  if (isAllowListed) {
    return NS_OK;
  }

  nsAutoCString list;
  UrlClassifierCommon::TablesToString(aList, list);

  UrlClassifierCommon::SetBlockedContent(aChannel, NS_ERROR_FINGERPRINTING_URI,
                                         list, EmptyCString(), EmptyCString());

  UC_LOG(
      ("UrlClassifierFeatureFingerprintingProtection::ProcessChannel, "
       "cancelling "
       "channel[%p]",
       aChannel));
  nsCOMPtr<nsIHttpChannelInternal> httpChannel = do_QueryInterface(aChannel);

  if (httpChannel) {
    Unused << httpChannel->CancelByURLClassifier(NS_ERROR_FINGERPRINTING_URI);
  } else {
    Unused << aChannel->Cancel(NS_ERROR_FINGERPRINTING_URI);
  }

  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierFeatureFingerprintingProtection::GetURIByListType(
    nsIChannel* aChannel, nsIUrlClassifierFeature::listType aListType,
    nsIUrlClassifierFeature::URIType* aURIType, nsIURI** aURI) {
  NS_ENSURE_ARG_POINTER(aChannel);
  NS_ENSURE_ARG_POINTER(aURIType);
  NS_ENSURE_ARG_POINTER(aURI);

  if (aListType == nsIUrlClassifierFeature::blacklist) {
    *aURIType = nsIUrlClassifierFeature::blacklistURI;
    return aChannel->GetURI(aURI);
  }

  MOZ_ASSERT(aListType == nsIUrlClassifierFeature::whitelist);

  *aURIType = nsIUrlClassifierFeature::pairwiseWhitelistURI;
  return UrlClassifierCommon::CreatePairwiseWhiteListURI(aChannel, aURI);
}

}  // namespace net
}  // namespace mozilla
