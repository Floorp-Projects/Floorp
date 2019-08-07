/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UrlClassifierFeatureCryptominingProtection.h"

#include "mozilla/AntiTrackingCommon.h"
#include "mozilla/net/UrlClassifierCommon.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "nsContentUtils.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace net {

namespace {

#define CRYPTOMINING_FEATURE_NAME "cryptomining-protection"

#define URLCLASSIFIER_CRYPTOMINING_BLACKLIST \
  "urlclassifier.features.cryptomining.blacklistTables"
#define URLCLASSIFIER_CRYPTOMINING_BLACKLIST_TEST_ENTRIES \
  "urlclassifier.features.cryptomining.blacklistHosts"
#define URLCLASSIFIER_CRYPTOMINING_WHITELIST \
  "urlclassifier.features.cryptomining.whitelistTables"
#define URLCLASSIFIER_CRYPTOMINING_WHITELIST_TEST_ENTRIES \
  "urlclassifier.features.cryptomining.whitelistHosts"
#define URLCLASSIFIER_CRYPTOMINING_SKIP_URLS \
  "urlclassifier.features.cryptomining.skipURLs"
#define TABLE_CRYPTOMINING_BLACKLIST_PREF "cryptomining-blacklist-pref"
#define TABLE_CRYPTOMINING_WHITELIST_PREF "cryptomining-whitelist-pref"

StaticRefPtr<UrlClassifierFeatureCryptominingProtection>
    gFeatureCryptominingProtection;

}  // namespace

UrlClassifierFeatureCryptominingProtection::
    UrlClassifierFeatureCryptominingProtection()
    : UrlClassifierFeatureBase(
          NS_LITERAL_CSTRING(CRYPTOMINING_FEATURE_NAME),
          NS_LITERAL_CSTRING(URLCLASSIFIER_CRYPTOMINING_BLACKLIST),
          NS_LITERAL_CSTRING(URLCLASSIFIER_CRYPTOMINING_WHITELIST),
          NS_LITERAL_CSTRING(URLCLASSIFIER_CRYPTOMINING_BLACKLIST_TEST_ENTRIES),
          NS_LITERAL_CSTRING(URLCLASSIFIER_CRYPTOMINING_WHITELIST_TEST_ENTRIES),
          NS_LITERAL_CSTRING(TABLE_CRYPTOMINING_BLACKLIST_PREF),
          NS_LITERAL_CSTRING(TABLE_CRYPTOMINING_WHITELIST_PREF),
          NS_LITERAL_CSTRING(URLCLASSIFIER_CRYPTOMINING_SKIP_URLS)) {}

/* static */ const char* UrlClassifierFeatureCryptominingProtection::Name() {
  return CRYPTOMINING_FEATURE_NAME;
}

/* static */
void UrlClassifierFeatureCryptominingProtection::MaybeInitialize() {
  UC_LOG(("UrlClassifierFeatureCryptominingProtection: MaybeInitialize"));

  if (!gFeatureCryptominingProtection) {
    gFeatureCryptominingProtection =
        new UrlClassifierFeatureCryptominingProtection();
    gFeatureCryptominingProtection->InitializePreferences();
  }
}

/* static */
void UrlClassifierFeatureCryptominingProtection::MaybeShutdown() {
  UC_LOG(("UrlClassifierFeatureCryptominingProtection: MaybeShutdown"));

  if (gFeatureCryptominingProtection) {
    gFeatureCryptominingProtection->ShutdownPreferences();
    gFeatureCryptominingProtection = nullptr;
  }
}

/* static */
already_AddRefed<UrlClassifierFeatureCryptominingProtection>
UrlClassifierFeatureCryptominingProtection::MaybeCreate(nsIChannel* aChannel) {
  MOZ_ASSERT(aChannel);

  UC_LOG(
      ("UrlClassifierFeatureCryptominingProtection: MaybeCreate for channel %p",
       aChannel));

  if (!StaticPrefs::privacy_trackingprotection_cryptomining_enabled()) {
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
          ("UrlClassifierFeatureCryptominingProtection: Skipping cryptomining "
           "checks "
           "for first party or top-level load channel[%p] "
           "with uri %s",
           aChannel, spec.get()));
    }

    return nullptr;
  }

  if (!UrlClassifierCommon::ShouldEnableClassifier(aChannel)) {
    return nullptr;
  }

  MaybeInitialize();
  MOZ_ASSERT(gFeatureCryptominingProtection);

  RefPtr<UrlClassifierFeatureCryptominingProtection> self =
      gFeatureCryptominingProtection;
  return self.forget();
}

/* static */
already_AddRefed<nsIUrlClassifierFeature>
UrlClassifierFeatureCryptominingProtection::GetIfNameMatches(
    const nsACString& aName) {
  if (!aName.EqualsLiteral(CRYPTOMINING_FEATURE_NAME)) {
    return nullptr;
  }

  MaybeInitialize();
  MOZ_ASSERT(gFeatureCryptominingProtection);

  RefPtr<UrlClassifierFeatureCryptominingProtection> self =
      gFeatureCryptominingProtection;
  return self.forget();
}

NS_IMETHODIMP
UrlClassifierFeatureCryptominingProtection::ProcessChannel(
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

  UrlClassifierCommon::SetBlockedContent(aChannel, NS_ERROR_CRYPTOMINING_URI,
                                         list, EmptyCString(), EmptyCString());

  UC_LOG(
      ("UrlClassifierFeatureCryptominingProtection::ProcessChannel, "
       "cancelling "
       "channel[%p]",
       aChannel));
  nsCOMPtr<nsIHttpChannelInternal> httpChannel = do_QueryInterface(aChannel);

  if (httpChannel) {
    Unused << httpChannel->CancelByURLClassifier(NS_ERROR_CRYPTOMINING_URI);
  } else {
    Unused << aChannel->Cancel(NS_ERROR_CRYPTOMINING_URI);
  }

  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierFeatureCryptominingProtection::GetURIByListType(
    nsIChannel* aChannel, nsIUrlClassifierFeature::listType aListType,
    nsIURI** aURI) {
  NS_ENSURE_ARG_POINTER(aChannel);
  NS_ENSURE_ARG_POINTER(aURI);

  if (aListType == nsIUrlClassifierFeature::blacklist) {
    return aChannel->GetURI(aURI);
  }

  MOZ_ASSERT(aListType == nsIUrlClassifierFeature::whitelist);
  return UrlClassifierCommon::CreatePairwiseWhiteListURI(aChannel, aURI);
}

}  // namespace net
}  // namespace mozilla
