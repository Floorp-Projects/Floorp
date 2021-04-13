/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UrlClassifierFeatureSocialTrackingProtection.h"

#include "mozilla/AntiTrackingUtils.h"
#include "mozilla/net/UrlClassifierCommon.h"
#include "ChannelClassifierService.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace net {

namespace {

#define SOCIALTRACKING_FEATURE_NAME "socialtracking-protection"

#define URLCLASSIFIER_SOCIALTRACKING_BLOCKLIST \
  "urlclassifier.features.socialtracking.blacklistTables"
#define URLCLASSIFIER_SOCIALTRACKING_BLOCKLIST_TEST_ENTRIES \
  "urlclassifier.features.socialtracking.blacklistHosts"
#define URLCLASSIFIER_SOCIALTRACKING_ENTITYLIST \
  "urlclassifier.features.socialtracking.whitelistTables"
#define URLCLASSIFIER_SOCIALTRACKING_ENTITYLIST_TEST_ENTRIES \
  "urlclassifier.features.socialtracking.whitelistHosts"
#define URLCLASSIFIER_SOCIALTRACKING_EXCEPTION_URLS \
  "urlclassifier.features.socialtracking.skipURLs"
#define TABLE_SOCIALTRACKING_BLOCKLIST_PREF "socialtracking-blocklist-pref"
#define TABLE_SOCIALTRACKING_ENTITYLIST_PREF "socialtracking-entitylist-pref"

StaticRefPtr<UrlClassifierFeatureSocialTrackingProtection>
    gFeatureSocialTrackingProtection;

}  // namespace

UrlClassifierFeatureSocialTrackingProtection::
    UrlClassifierFeatureSocialTrackingProtection()
    : UrlClassifierFeatureBase(
          nsLiteralCString(SOCIALTRACKING_FEATURE_NAME),
          nsLiteralCString(URLCLASSIFIER_SOCIALTRACKING_BLOCKLIST),
          nsLiteralCString(URLCLASSIFIER_SOCIALTRACKING_ENTITYLIST),
          nsLiteralCString(URLCLASSIFIER_SOCIALTRACKING_BLOCKLIST_TEST_ENTRIES),
          nsLiteralCString(
              URLCLASSIFIER_SOCIALTRACKING_ENTITYLIST_TEST_ENTRIES),
          nsLiteralCString(TABLE_SOCIALTRACKING_BLOCKLIST_PREF),
          nsLiteralCString(TABLE_SOCIALTRACKING_ENTITYLIST_PREF),
          nsLiteralCString(URLCLASSIFIER_SOCIALTRACKING_EXCEPTION_URLS)) {}

/* static */ const char* UrlClassifierFeatureSocialTrackingProtection::Name() {
  return SOCIALTRACKING_FEATURE_NAME;
}

/* static */
void UrlClassifierFeatureSocialTrackingProtection::MaybeInitialize() {
  UC_LOG_LEAK(
      ("UrlClassifierFeatureSocialTrackingProtection::MaybeInitialize"));

  if (!gFeatureSocialTrackingProtection) {
    gFeatureSocialTrackingProtection =
        new UrlClassifierFeatureSocialTrackingProtection();
    gFeatureSocialTrackingProtection->InitializePreferences();
  }
}

/* static */
void UrlClassifierFeatureSocialTrackingProtection::MaybeShutdown() {
  UC_LOG_LEAK(("UrlClassifierFeatureSocialTrackingProtection::MaybeShutdown"));

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

  UC_LOG_LEAK(
      ("UrlClassifierFeatureSocialTrackingProtection::MaybeCreate - channel %p",
       aChannel));

  if (!StaticPrefs::privacy_trackingprotection_socialtracking_enabled()) {
    return nullptr;
  }

  bool isThirdParty = AntiTrackingUtils::IsThirdPartyChannel(aChannel);
  if (!isThirdParty) {
    UC_LOG(
        ("UrlClassifierFeatureSocialTrackingProtection::MaybeCreate - "
         "skipping first party or top-level load for channel %p",
         aChannel));
    return nullptr;
  }

  if (!UrlClassifierCommon::ShouldEnableProtectionForChannel(aChannel)) {
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

  ChannelBlockDecision decision =
      ChannelClassifierService::OnBeforeBlockChannel(aChannel, mName, list);
  if (decision != ChannelBlockDecision::Blocked) {
    uint32_t event =
        decision == ChannelBlockDecision::Replaced
            ? nsIWebProgressListener::STATE_REPLACED_TRACKING_CONTENT
            : nsIWebProgressListener::STATE_ALLOWED_TRACKING_CONTENT;
    ContentBlockingNotifier::OnEvent(aChannel, event, false);

    *aShouldContinue = true;
    return NS_OK;
  }

  UrlClassifierCommon::SetBlockedContent(aChannel, NS_ERROR_SOCIALTRACKING_URI,
                                         list, ""_ns, ""_ns);

  UC_LOG(
      ("UrlClassifierFeatureSocialTrackingProtection::ProcessChannel - "
       "cancelling channel %p",
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

  if (aListType == nsIUrlClassifierFeature::blocklist) {
    *aURIType = nsIUrlClassifierFeature::blocklistURI;
    return aChannel->GetURI(aURI);
  }

  MOZ_ASSERT(aListType == nsIUrlClassifierFeature::entitylist);

  *aURIType = nsIUrlClassifierFeature::pairwiseEntitylistURI;
  return UrlClassifierCommon::CreatePairwiseEntityListURI(aChannel, aURI);
}

}  // namespace net
}  // namespace mozilla
