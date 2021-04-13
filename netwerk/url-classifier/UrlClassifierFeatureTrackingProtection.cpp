/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UrlClassifierFeatureTrackingProtection.h"

#include "mozilla/AntiTrackingUtils.h"
#include "mozilla/net/UrlClassifierCommon.h"
#include "ChannelClassifierService.h"
#include "nsIChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsILoadContext.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace net {

namespace {

#define TRACKING_PROTECTION_FEATURE_NAME "tracking-protection"

#define URLCLASSIFIER_TRACKING_BLOCKLIST "urlclassifier.trackingTable"
#define URLCLASSIFIER_TRACKING_BLOCKLIST_TEST_ENTRIES \
  "urlclassifier.trackingTable.testEntries"
#define URLCLASSIFIER_TRACKING_ENTITYLIST "urlclassifier.trackingWhitelistTable"
#define URLCLASSIFIER_TRACKING_ENTITYLIST_TEST_ENTRIES \
  "urlclassifier.trackingWhitelistTable.testEntries"
#define URLCLASSIFIER_TRACKING_PROTECTION_EXCEPTION_URLS \
  "urlclassifier.trackingSkipURLs"
#define TABLE_TRACKING_BLOCKLIST_PREF "tracking-blocklist-pref"
#define TABLE_TRACKING_ENTITYLIST_PREF "tracking-entitylist-pref"

StaticRefPtr<UrlClassifierFeatureTrackingProtection> gFeatureTrackingProtection;

}  // namespace

UrlClassifierFeatureTrackingProtection::UrlClassifierFeatureTrackingProtection()
    : UrlClassifierFeatureBase(
          nsLiteralCString(TRACKING_PROTECTION_FEATURE_NAME),
          nsLiteralCString(URLCLASSIFIER_TRACKING_BLOCKLIST),
          nsLiteralCString(URLCLASSIFIER_TRACKING_ENTITYLIST),
          nsLiteralCString(URLCLASSIFIER_TRACKING_BLOCKLIST_TEST_ENTRIES),
          nsLiteralCString(URLCLASSIFIER_TRACKING_ENTITYLIST_TEST_ENTRIES),
          nsLiteralCString(TABLE_TRACKING_BLOCKLIST_PREF),
          nsLiteralCString(TABLE_TRACKING_ENTITYLIST_PREF),
          nsLiteralCString(URLCLASSIFIER_TRACKING_PROTECTION_EXCEPTION_URLS)) {}

/* static */ const char* UrlClassifierFeatureTrackingProtection::Name() {
  return TRACKING_PROTECTION_FEATURE_NAME;
}

/* static */
void UrlClassifierFeatureTrackingProtection::MaybeInitialize() {
  MOZ_ASSERT(XRE_IsParentProcess());
  UC_LOG_LEAK(("UrlClassifierFeatureTrackingProtection::MaybeInitialize"));

  if (!gFeatureTrackingProtection) {
    gFeatureTrackingProtection = new UrlClassifierFeatureTrackingProtection();
    gFeatureTrackingProtection->InitializePreferences();
  }
}

/* static */
void UrlClassifierFeatureTrackingProtection::MaybeShutdown() {
  UC_LOG_LEAK(("UrlClassifierFeatureTrackingProtection::MaybeShutdown"));

  if (gFeatureTrackingProtection) {
    gFeatureTrackingProtection->ShutdownPreferences();
    gFeatureTrackingProtection = nullptr;
  }
}

/* static */
already_AddRefed<UrlClassifierFeatureTrackingProtection>
UrlClassifierFeatureTrackingProtection::MaybeCreate(nsIChannel* aChannel) {
  MOZ_ASSERT(aChannel);

  UC_LOG_LEAK(
      ("UrlClassifierFeatureTrackingProtection::MaybeCreate - channel %p",
       aChannel));

  nsCOMPtr<nsILoadContext> loadContext;
  NS_QueryNotificationCallbacks(aChannel, loadContext);
  if (!loadContext) {
    // Some channels don't have a loadcontext, check the global tracking
    // protection preference.
    if (!StaticPrefs::privacy_trackingprotection_enabled() &&
        !(NS_UsePrivateBrowsing(aChannel) &&
          StaticPrefs::privacy_trackingprotection_pbmode_enabled())) {
      return nullptr;
    }
  } else if (!loadContext->UseTrackingProtection()) {
    return nullptr;
  }

  bool isThirdParty = AntiTrackingUtils::IsThirdPartyChannel(aChannel);
  if (!isThirdParty) {
    UC_LOG(
        ("UrlClassifierFeatureTrackingProtection::MaybeCreate - "
         "skipping first party or top-level load for channel %p",
         aChannel));
    return nullptr;
  }

  if (!UrlClassifierCommon::ShouldEnableProtectionForChannel(aChannel)) {
    return nullptr;
  }

  MaybeInitialize();
  MOZ_ASSERT(gFeatureTrackingProtection);

  RefPtr<UrlClassifierFeatureTrackingProtection> self =
      gFeatureTrackingProtection;
  return self.forget();
}

/* static */
already_AddRefed<nsIUrlClassifierFeature>
UrlClassifierFeatureTrackingProtection::GetIfNameMatches(
    const nsACString& aName) {
  if (!aName.EqualsLiteral(TRACKING_PROTECTION_FEATURE_NAME)) {
    return nullptr;
  }

  MaybeInitialize();
  MOZ_ASSERT(gFeatureTrackingProtection);

  RefPtr<UrlClassifierFeatureTrackingProtection> self =
      gFeatureTrackingProtection;
  return self.forget();
}

NS_IMETHODIMP
UrlClassifierFeatureTrackingProtection::ProcessChannel(
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

  UrlClassifierCommon::SetBlockedContent(aChannel, NS_ERROR_TRACKING_URI, list,
                                         ""_ns, ""_ns);

  UC_LOG(
      ("UrlClassifierFeatureTrackingProtection::ProcessChannel - "
       "cancelling channel %p",
       aChannel));

  nsCOMPtr<nsIHttpChannelInternal> httpChannel = do_QueryInterface(aChannel);
  if (httpChannel) {
    Unused << httpChannel->CancelByURLClassifier(NS_ERROR_TRACKING_URI);
  } else {
    Unused << aChannel->Cancel(NS_ERROR_TRACKING_URI);
  }

  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierFeatureTrackingProtection::GetURIByListType(
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
