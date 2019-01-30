/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UrlClassifierFeatureFingerprinting.h"

#include "mozilla/AntiTrackingCommon.h"
#include "mozilla/net/UrlClassifierCommon.h"
#include "mozilla/StaticPrefs.h"
#include "nsContentUtils.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace net {

namespace {

#define FINGERPRINTING_FEATURE_NAME "fingerprinting"

#define URLCLASSIFIER_FINGERPRINTING_BLACKLIST \
  "urlclassifier.features.fingerprinting.blacklistTables"
#define URLCLASSIFIER_FINGERPRINTING_BLACKLIST_TEST_ENTRIES \
  "urlclassifier.features.fingerprinting.blacklistHosts"
#define URLCLASSIFIER_FINGERPRINTING_WHITELIST \
  "urlclassifier.features.fingerprinting.whitelistTables"
#define URLCLASSIFIER_FINGERPRINTING_WHITELIST_TEST_ENTRIES \
  "urlclassifier.features.fingerprinting.whitelistHosts"
#define TABLE_FINGERPRINTING_BLACKLIST_PREF "fingerprinting-blacklist-pref"
#define TABLE_FINGERPRINTING_WHITELIST_PREF "fingerprinting-whitelist-pref"

StaticRefPtr<UrlClassifierFeatureFingerprinting> gFeatureFingerprinting;

}  // namespace

UrlClassifierFeatureFingerprinting::UrlClassifierFeatureFingerprinting()
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
          EmptyCString()) {}

/* static */ const char* UrlClassifierFeatureFingerprinting::Name() {
  return FINGERPRINTING_FEATURE_NAME;
}

/* static */ void UrlClassifierFeatureFingerprinting::MaybeInitialize() {
  UC_LOG(("UrlClassifierFeatureFingerprinting: MaybeInitialize"));

  if (!gFeatureFingerprinting) {
    gFeatureFingerprinting = new UrlClassifierFeatureFingerprinting();
    gFeatureFingerprinting->InitializePreferences();
  }
}

/* static */ void UrlClassifierFeatureFingerprinting::MaybeShutdown() {
  UC_LOG(("UrlClassifierFeatureFingerprinting: MaybeShutdown"));

  if (gFeatureFingerprinting) {
    gFeatureFingerprinting->ShutdownPreferences();
    gFeatureFingerprinting = nullptr;
  }
}

/* static */ already_AddRefed<UrlClassifierFeatureFingerprinting>
UrlClassifierFeatureFingerprinting::MaybeCreate(nsIChannel* aChannel) {
  MOZ_ASSERT(aChannel);

  UC_LOG(("UrlClassifierFeatureFingerprinting: MaybeCreate for channel %p",
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
          ("UrlClassifierFeatureFingerprinting: Skipping fingerprinting checks "
           "for first party or top-level load channel[%p] "
           "with uri %s",
           aChannel, spec.get()));
    }

    return nullptr;
  }

  if (!UrlClassifierCommon::ShouldEnableClassifier(
          aChannel, AntiTrackingCommon::eFingerprinting)) {
    return nullptr;
  }

  MaybeInitialize();
  MOZ_ASSERT(gFeatureFingerprinting);

  RefPtr<UrlClassifierFeatureFingerprinting> self = gFeatureFingerprinting;
  return self.forget();
}

/* static */ already_AddRefed<nsIUrlClassifierFeature>
UrlClassifierFeatureFingerprinting::GetIfNameMatches(const nsACString& aName) {
  if (!aName.EqualsLiteral(FINGERPRINTING_FEATURE_NAME)) {
    return nullptr;
  }

  MaybeInitialize();
  MOZ_ASSERT(gFeatureFingerprinting);

  RefPtr<UrlClassifierFeatureFingerprinting> self = gFeatureFingerprinting;
  return self.forget();
}

NS_IMETHODIMP
UrlClassifierFeatureFingerprinting::ProcessChannel(nsIChannel* aChannel,
                                                   const nsACString& aList,
                                                   bool* aShouldContinue) {
  NS_ENSURE_ARG_POINTER(aChannel);
  NS_ENSURE_ARG_POINTER(aShouldContinue);

  // This is a blocking feature.
  *aShouldContinue = false;

  UrlClassifierCommon::SetBlockedContent(aChannel, NS_ERROR_TRACKING_URI, aList,
                                         EmptyCString(), EmptyCString());

  UC_LOG(
      ("UrlClassifierFeatureFingerprinting::ProcessChannel, cancelling "
       "channel[%p]",
       aChannel));
  nsCOMPtr<nsIHttpChannelInternal> httpChannel = do_QueryInterface(aChannel);

  // FIXME: the way we cancel the channel depends on what the UI wants to show.
  // This needs to change, at some point.
  if (httpChannel) {
    Unused << httpChannel->CancelForTrackingProtection();
  } else {
    Unused << aChannel->Cancel(NS_ERROR_TRACKING_URI);
  }

  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierFeatureFingerprinting::GetURIByListType(
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
