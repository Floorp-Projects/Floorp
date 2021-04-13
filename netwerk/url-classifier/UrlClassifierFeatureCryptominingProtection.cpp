/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UrlClassifierFeatureCryptominingProtection.h"

#include "mozilla/AntiTrackingUtils.h"
#include "mozilla/net/UrlClassifierCommon.h"
#include "ChannelClassifierService.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace net {

namespace {

#define CRYPTOMINING_FEATURE_NAME "cryptomining-protection"

#define URLCLASSIFIER_CRYPTOMINING_BLOCKLIST \
  "urlclassifier.features.cryptomining.blacklistTables"
#define URLCLASSIFIER_CRYPTOMINING_BLOCKLIST_TEST_ENTRIES \
  "urlclassifier.features.cryptomining.blacklistHosts"
#define URLCLASSIFIER_CRYPTOMINING_ENTITYLIST \
  "urlclassifier.features.cryptomining.whitelistTables"
#define URLCLASSIFIER_CRYPTOMINING_ENTITYLIST_TEST_ENTRIES \
  "urlclassifier.features.cryptomining.whitelistHosts"
#define URLCLASSIFIER_CRYPTOMINING_EXCEPTION_URLS \
  "urlclassifier.features.cryptomining.skipURLs"
#define TABLE_CRYPTOMINING_BLOCKLIST_PREF "cryptomining-blacklist-pref"
#define TABLE_CRYPTOMINING_ENTITYLIST_PREF "cryptomining-whitelist-pref"

StaticRefPtr<UrlClassifierFeatureCryptominingProtection>
    gFeatureCryptominingProtection;

}  // namespace

UrlClassifierFeatureCryptominingProtection::
    UrlClassifierFeatureCryptominingProtection()
    : UrlClassifierFeatureBase(
          nsLiteralCString(CRYPTOMINING_FEATURE_NAME),
          nsLiteralCString(URLCLASSIFIER_CRYPTOMINING_BLOCKLIST),
          nsLiteralCString(URLCLASSIFIER_CRYPTOMINING_ENTITYLIST),
          nsLiteralCString(URLCLASSIFIER_CRYPTOMINING_BLOCKLIST_TEST_ENTRIES),
          nsLiteralCString(URLCLASSIFIER_CRYPTOMINING_ENTITYLIST_TEST_ENTRIES),
          nsLiteralCString(TABLE_CRYPTOMINING_BLOCKLIST_PREF),
          nsLiteralCString(TABLE_CRYPTOMINING_ENTITYLIST_PREF),
          nsLiteralCString(URLCLASSIFIER_CRYPTOMINING_EXCEPTION_URLS)) {}

/* static */ const char* UrlClassifierFeatureCryptominingProtection::Name() {
  return CRYPTOMINING_FEATURE_NAME;
}

/* static */
void UrlClassifierFeatureCryptominingProtection::MaybeInitialize() {
  UC_LOG_LEAK(("UrlClassifierFeatureCryptominingProtection::MaybeInitialize"));

  if (!gFeatureCryptominingProtection) {
    gFeatureCryptominingProtection =
        new UrlClassifierFeatureCryptominingProtection();
    gFeatureCryptominingProtection->InitializePreferences();
  }
}

/* static */
void UrlClassifierFeatureCryptominingProtection::MaybeShutdown() {
  UC_LOG_LEAK(("UrlClassifierFeatureCryptominingProtection::MaybeShutdown"));

  if (gFeatureCryptominingProtection) {
    gFeatureCryptominingProtection->ShutdownPreferences();
    gFeatureCryptominingProtection = nullptr;
  }
}

/* static */
already_AddRefed<UrlClassifierFeatureCryptominingProtection>
UrlClassifierFeatureCryptominingProtection::MaybeCreate(nsIChannel* aChannel) {
  MOZ_ASSERT(aChannel);

  UC_LOG_LEAK(
      ("UrlClassifierFeatureCryptominingProtection::MaybeCreate - channel %p",
       aChannel));

  if (!StaticPrefs::privacy_trackingprotection_cryptomining_enabled()) {
    return nullptr;
  }

  bool isThirdParty = AntiTrackingUtils::IsThirdPartyChannel(aChannel);
  if (!isThirdParty) {
    UC_LOG(
        ("UrlClassifierFeatureCryptominingProtection::MaybeCreate - "
         "skipping first party or top-level load for channel %p",
         aChannel));
    return nullptr;
  }

  if (!UrlClassifierCommon::ShouldEnableProtectionForChannel(aChannel)) {
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

  UrlClassifierCommon::SetBlockedContent(aChannel, NS_ERROR_CRYPTOMINING_URI,
                                         list, ""_ns, ""_ns);

  UC_LOG(
      ("UrlClassifierFeatureCryptominingProtection::ProcessChannel - "
       "cancelling channel %p",
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
