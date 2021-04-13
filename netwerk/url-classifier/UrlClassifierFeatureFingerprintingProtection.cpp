/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UrlClassifierFeatureFingerprintingProtection.h"

#include "mozilla/AntiTrackingUtils.h"
#include "mozilla/net/UrlClassifierCommon.h"
#include "ChannelClassifierService.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace net {

namespace {

#define FINGERPRINTING_FEATURE_NAME "fingerprinting-protection"

#define URLCLASSIFIER_FINGERPRINTING_BLOCKLIST \
  "urlclassifier.features.fingerprinting.blacklistTables"
#define URLCLASSIFIER_FINGERPRINTING_BLOCKLIST_TEST_ENTRIES \
  "urlclassifier.features.fingerprinting.blacklistHosts"
#define URLCLASSIFIER_FINGERPRINTING_ENTITYLIST \
  "urlclassifier.features.fingerprinting.whitelistTables"
#define URLCLASSIFIER_FINGERPRINTING_ENTITYLIST_TEST_ENTRIES \
  "urlclassifier.features.fingerprinting.whitelistHosts"
#define URLCLASSIFIER_FINGERPRINTING_EXCEPTION_URLS \
  "urlclassifier.features.fingerprinting.skipURLs"
#define TABLE_FINGERPRINTING_BLOCKLIST_PREF "fingerprinting-blacklist-pref"
#define TABLE_FINGERPRINTING_ENTITYLIST_PREF "fingerprinting-whitelist-pref"

StaticRefPtr<UrlClassifierFeatureFingerprintingProtection>
    gFeatureFingerprintingProtection;

}  // namespace

UrlClassifierFeatureFingerprintingProtection::
    UrlClassifierFeatureFingerprintingProtection()
    : UrlClassifierFeatureBase(
          nsLiteralCString(FINGERPRINTING_FEATURE_NAME),
          nsLiteralCString(URLCLASSIFIER_FINGERPRINTING_BLOCKLIST),
          nsLiteralCString(URLCLASSIFIER_FINGERPRINTING_ENTITYLIST),
          nsLiteralCString(URLCLASSIFIER_FINGERPRINTING_BLOCKLIST_TEST_ENTRIES),
          nsLiteralCString(
              URLCLASSIFIER_FINGERPRINTING_ENTITYLIST_TEST_ENTRIES),
          nsLiteralCString(TABLE_FINGERPRINTING_BLOCKLIST_PREF),
          nsLiteralCString(TABLE_FINGERPRINTING_ENTITYLIST_PREF),
          nsLiteralCString(URLCLASSIFIER_FINGERPRINTING_EXCEPTION_URLS)) {}

/* static */ const char* UrlClassifierFeatureFingerprintingProtection::Name() {
  return FINGERPRINTING_FEATURE_NAME;
}

/* static */
void UrlClassifierFeatureFingerprintingProtection::MaybeInitialize() {
  UC_LOG_LEAK(
      ("UrlClassifierFeatureFingerprintingProtection::MaybeInitialize"));

  if (!gFeatureFingerprintingProtection) {
    gFeatureFingerprintingProtection =
        new UrlClassifierFeatureFingerprintingProtection();
    gFeatureFingerprintingProtection->InitializePreferences();
  }
}

/* static */
void UrlClassifierFeatureFingerprintingProtection::MaybeShutdown() {
  UC_LOG_LEAK(("UrlClassifierFeatureFingerprintingProtection::MaybeShutdown"));

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

  UC_LOG_LEAK(
      ("UrlClassifierFeatureFingerprintingProtection::MaybeCreate - channel %p",
       aChannel));

  if (!StaticPrefs::privacy_trackingprotection_fingerprinting_enabled()) {
    return nullptr;
  }

  bool isThirdParty = AntiTrackingUtils::IsThirdPartyChannel(aChannel);
  if (!isThirdParty) {
    UC_LOG(
        ("UrlClassifierFeatureFingerprintingProtection::MaybeCreate - "
         "skipping first party or top-level load for channel %p",
         aChannel));
    return nullptr;
  }

  if (UrlClassifierCommon::IsPassiveContent(aChannel)) {
    return nullptr;
  }

  if (!UrlClassifierCommon::ShouldEnableProtectionForChannel(aChannel)) {
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

  UrlClassifierCommon::SetBlockedContent(aChannel, NS_ERROR_FINGERPRINTING_URI,
                                         list, ""_ns, ""_ns);

  UC_LOG(
      ("UrlClassifierFeatureFingerprintingProtection::ProcessChannel - "
       "cancelling channel %p",
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
