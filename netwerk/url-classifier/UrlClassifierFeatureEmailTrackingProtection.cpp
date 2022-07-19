/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UrlClassifierFeatureEmailTrackingProtection.h"

#include "ChannelClassifierService.h"
#include "mozilla/AntiTrackingUtils.h"
#include "mozilla/net/UrlClassifierCommon.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "mozilla/StaticPtr.h"
#include "nsIChannel.h"
#include "nsILoadContext.h"
#include "nsIHttpChannelInternal.h"
#include "nsIWebProgressListener.h"

namespace mozilla::net {

namespace {

#define EMAIL_TRACKING_PROTECTION_FEATURE_NAME "emailtracking-protection"

#define URLCLASSIFIER_EMAIL_TRACKING_BLOCKLIST \
  "urlclassifier.features.emailtracking.blocklistTables"
#define URLCLASSIFIER_EMAIL_TRACKING_BLOCKLIST_TEST_ENTRIES \
  "urlclassifier.features.emailtracking.blocklistHosts"
#define URLCLASSIFIER_EMAIL_TRACKING_ENTITYLIST \
  "urlclassifier.features.emailtracking.allowlistTables"
#define URLCLASSIFIER_EMAIL_TRACKING_ENTITYLIST_TEST_ENTRIES \
  "urlclassifier.features.emailtracking.allowlistHosts"
#define URLCLASSIFIER_EMAIL_TRACKING_PROTECTION_EXCEPTION_URLS \
  "urlclassifier.features.emailtracking.skipURLs"
#define TABLE_EMAIL_TRACKING_BLOCKLIST_PREF "emailtracking-blocklist-pref"
#define TABLE_EMAIL_TRACKING_ENTITYLIST_PREF "emailtracking-allowlist-pref"

StaticRefPtr<UrlClassifierFeatureEmailTrackingProtection>
    gFeatureEmailTrackingProtection;

}  // namespace

UrlClassifierFeatureEmailTrackingProtection::
    UrlClassifierFeatureEmailTrackingProtection()
    : UrlClassifierFeatureAntiTrackingBase(
          nsLiteralCString(EMAIL_TRACKING_PROTECTION_FEATURE_NAME),
          nsLiteralCString(URLCLASSIFIER_EMAIL_TRACKING_BLOCKLIST),
          nsLiteralCString(URLCLASSIFIER_EMAIL_TRACKING_ENTITYLIST),
          nsLiteralCString(URLCLASSIFIER_EMAIL_TRACKING_BLOCKLIST_TEST_ENTRIES),
          nsLiteralCString(
              URLCLASSIFIER_EMAIL_TRACKING_ENTITYLIST_TEST_ENTRIES),
          nsLiteralCString(TABLE_EMAIL_TRACKING_BLOCKLIST_PREF),
          nsLiteralCString(TABLE_EMAIL_TRACKING_ENTITYLIST_PREF),
          nsLiteralCString(
              URLCLASSIFIER_EMAIL_TRACKING_PROTECTION_EXCEPTION_URLS)) {}

/* static */
const char* UrlClassifierFeatureEmailTrackingProtection::Name() {
  return EMAIL_TRACKING_PROTECTION_FEATURE_NAME;
}

/* static */
void UrlClassifierFeatureEmailTrackingProtection::MaybeInitialize() {
  MOZ_ASSERT(XRE_IsParentProcess());
  UC_LOG_LEAK(("UrlClassifierFeatureEmailTrackingProtection::MaybeInitialize"));

  if (!gFeatureEmailTrackingProtection) {
    gFeatureEmailTrackingProtection =
        new UrlClassifierFeatureEmailTrackingProtection();
    gFeatureEmailTrackingProtection->InitializePreferences();
  }
}

/* static */
void UrlClassifierFeatureEmailTrackingProtection::MaybeShutdown() {
  UC_LOG_LEAK(("UrlClassifierFeatureEmailTrackingProtection::MaybeShutdown"));

  if (gFeatureEmailTrackingProtection) {
    gFeatureEmailTrackingProtection->ShutdownPreferences();
    gFeatureEmailTrackingProtection = nullptr;
  }
}

/* static */
already_AddRefed<UrlClassifierFeatureEmailTrackingProtection>
UrlClassifierFeatureEmailTrackingProtection::MaybeCreate(nsIChannel* aChannel) {
  MOZ_ASSERT(aChannel);

  UC_LOG_LEAK(
      ("UrlClassifierFeatureEmailTrackingProtection::MaybeCreate - channel %p",
       aChannel));

  // Check if the email tracking protection is enabled.
  if (!StaticPrefs::privacy_trackingprotection_emailtracking_enabled()) {
    return nullptr;
  }

  bool isThirdParty = AntiTrackingUtils::IsThirdPartyChannel(aChannel);
  if (!isThirdParty) {
    UC_LOG(
        ("UrlClassifierFeatureEmailTrackingProtection::MaybeCreate - "
         "skipping first party or top-level load for channel %p",
         aChannel));
    return nullptr;
  }

  if (!UrlClassifierCommon::ShouldEnableProtectionForChannel(aChannel)) {
    return nullptr;
  }

  MaybeInitialize();
  MOZ_ASSERT(gFeatureEmailTrackingProtection);

  RefPtr<UrlClassifierFeatureEmailTrackingProtection> self =
      gFeatureEmailTrackingProtection;
  return self.forget();
}

/* static */
already_AddRefed<nsIUrlClassifierFeature>
UrlClassifierFeatureEmailTrackingProtection::GetIfNameMatches(
    const nsACString& aName) {
  if (!aName.EqualsLiteral(EMAIL_TRACKING_PROTECTION_FEATURE_NAME)) {
    return nullptr;
  }

  MaybeInitialize();
  MOZ_ASSERT(gFeatureEmailTrackingProtection);

  RefPtr<UrlClassifierFeatureEmailTrackingProtection> self =
      gFeatureEmailTrackingProtection;
  return self.forget();
}

NS_IMETHODIMP
UrlClassifierFeatureEmailTrackingProtection::ProcessChannel(
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

  UrlClassifierCommon::SetBlockedContent(aChannel, NS_ERROR_EMAILTRACKING_URI,
                                         list, ""_ns, ""_ns);

  UC_LOG(
      ("UrlClassifierFeatureEmailTrackingProtection::ProcessChannel - "
       "cancelling channel %p",
       aChannel));

  nsCOMPtr<nsIHttpChannelInternal> httpChannel = do_QueryInterface(aChannel);
  if (httpChannel) {
    Unused << httpChannel->CancelByURLClassifier(NS_ERROR_EMAILTRACKING_URI);
  } else {
    Unused << aChannel->Cancel(NS_ERROR_EMAILTRACKING_URI);
  }

  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierFeatureEmailTrackingProtection::GetURIByListType(
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

}  // namespace mozilla::net
