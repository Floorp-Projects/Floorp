/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UrlClassifierFeatureSocialTrackingProtection.h"

#include "mozilla/net/UrlClassifierCommon.h"
#include "ChannelClassifierService.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "nsContentUtils.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace net {

namespace {

#define SOCIALTRACKING_FEATURE_NAME "socialtracking-protection"

#define URLCLASSIFIER_SOCIALTRACKING_BLACKLIST \
  "urlclassifier.features.socialtracking.blacklistTables"
#define URLCLASSIFIER_SOCIALTRACKING_BLACKLIST_TEST_ENTRIES \
  "urlclassifier.features.socialtracking.blacklistHosts"
#define URLCLASSIFIER_SOCIALTRACKING_WHITELIST \
  "urlclassifier.features.socialtracking.whitelistTables"
#define URLCLASSIFIER_SOCIALTRACKING_WHITELIST_TEST_ENTRIES \
  "urlclassifier.features.socialtracking.whitelistHosts"
#define URLCLASSIFIER_SOCIALTRACKING_SKIP_URLS \
  "urlclassifier.features.socialtracking.skipURLs"
#define TABLE_SOCIALTRACKING_BLACKLIST_PREF "socialtracking-blacklist-pref"
#define TABLE_SOCIALTRACKING_WHITELIST_PREF "socialtracking-whitelist-pref"

StaticRefPtr<UrlClassifierFeatureSocialTrackingProtection>
    gFeatureSocialTrackingProtection;

}  // namespace

UrlClassifierFeatureSocialTrackingProtection::
    UrlClassifierFeatureSocialTrackingProtection()
    : UrlClassifierFeatureBase(
          nsLiteralCString(SOCIALTRACKING_FEATURE_NAME),
          nsLiteralCString(URLCLASSIFIER_SOCIALTRACKING_BLACKLIST),
          nsLiteralCString(URLCLASSIFIER_SOCIALTRACKING_WHITELIST),
          nsLiteralCString(URLCLASSIFIER_SOCIALTRACKING_BLACKLIST_TEST_ENTRIES),
          nsLiteralCString(URLCLASSIFIER_SOCIALTRACKING_WHITELIST_TEST_ENTRIES),
          nsLiteralCString(TABLE_SOCIALTRACKING_BLACKLIST_PREF),
          nsLiteralCString(TABLE_SOCIALTRACKING_WHITELIST_PREF),
          nsLiteralCString(URLCLASSIFIER_SOCIALTRACKING_SKIP_URLS)) {}

/* static */ const char* UrlClassifierFeatureSocialTrackingProtection::Name() {
  return SOCIALTRACKING_FEATURE_NAME;
}

/* static */
void UrlClassifierFeatureSocialTrackingProtection::MaybeInitialize() {
  UC_LOG(("UrlClassifierFeatureSocialTrackingProtection: MaybeInitialize"));

  if (!gFeatureSocialTrackingProtection) {
    gFeatureSocialTrackingProtection =
        new UrlClassifierFeatureSocialTrackingProtection();
    gFeatureSocialTrackingProtection->InitializePreferences();
  }
}

/* static */
void UrlClassifierFeatureSocialTrackingProtection::MaybeShutdown() {
  UC_LOG(("UrlClassifierFeatureSocialTrackingProtection: MaybeShutdown"));

  if (gFeatureSocialTrackingProtection) {
    gFeatureSocialTrackingProtection->ShutdownPreferences();
    gFeatureSocialTrackingProtection = nullptr;
  }
}

/* static */
already_AddRefed<UrlClassifierFeatureSocialTrackingProtection>
UrlClassifierFeatureSocialTrackingProtection::MaybeCreate(
    nsIChannel* aChannel) {
  MOZ_ASSERT(aChannel);

  UC_LOG(
      ("UrlClassifierFeatureSocialTrackingProtection: MaybeCreate for channel "
       "%p",
       aChannel));

  if (!StaticPrefs::privacy_trackingprotection_socialtracking_enabled()) {
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
          ("UrlClassifierFeatureSocialTrackingProtection: Skipping "
           "socialtracking checks "
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
  MOZ_ASSERT(gFeatureSocialTrackingProtection);

  RefPtr<UrlClassifierFeatureSocialTrackingProtection> self =
      gFeatureSocialTrackingProtection;
  return self.forget();
}

/* static */
already_AddRefed<nsIUrlClassifierFeature>
UrlClassifierFeatureSocialTrackingProtection::GetIfNameMatches(
    const nsACString& aName) {
  if (!aName.EqualsLiteral(SOCIALTRACKING_FEATURE_NAME)) {
    return nullptr;
  }

  MaybeInitialize();
  MOZ_ASSERT(gFeatureSocialTrackingProtection);

  RefPtr<UrlClassifierFeatureSocialTrackingProtection> self =
      gFeatureSocialTrackingProtection;
  return self.forget();
}

NS_IMETHODIMP
UrlClassifierFeatureSocialTrackingProtection::ProcessChannel(
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

  if (ChannelClassifierService::OnBeforeBlockChannel(aChannel, mName, list) ==
      ChannelBlockDecision::Unblocked) {
    return NS_OK;
  }

  UrlClassifierCommon::SetBlockedContent(aChannel, NS_ERROR_SOCIALTRACKING_URI,
                                         list, EmptyCString(), EmptyCString());

  UC_LOG(
      ("UrlClassifierFeatureSocialTrackingProtection::ProcessChannel, "
       "cancelling "
       "channel[%p]",
       aChannel));
  nsCOMPtr<nsIHttpChannelInternal> httpChannel = do_QueryInterface(aChannel);

  if (httpChannel) {
    Unused << httpChannel->CancelByURLClassifier(NS_ERROR_SOCIALTRACKING_URI);
  } else {
    Unused << aChannel->Cancel(NS_ERROR_SOCIALTRACKING_URI);
  }

  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierFeatureSocialTrackingProtection::GetURIByListType(
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
