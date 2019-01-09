/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UrlClassifierFeatureCryptomining.h"

#include "mozilla/AntiTrackingCommon.h"
#include "mozilla/net/UrlClassifierCommon.h"
#include "mozilla/StaticPrefs.h"
#include "nsContentUtils.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace net {

namespace {

#define CRYPTOMINING_FEATURE_NAME "cryptomining"

#define URLCLASSIFIER_CRYPTOMINING_BLACKLIST \
  "urlclassifier.features.cryptomining.blacklistTables"
#define URLCLASSIFIER_CRYPTOMINING_BLACKLIST_TEST_ENTRIES \
  "urlclassifier.features.cryptomining.blacklistHosts"
#define URLCLASSIFIER_CRYPTOMINING_WHITELIST \
  "urlclassifier.features.cryptomining.whitelistTables"
#define URLCLASSIFIER_CRYPTOMINING_WHITELIST_TEST_ENTRIES \
  "urlclassifier.features.cryptomining.whitelistHosts"
#define TABLE_CRYPTOMINING_BLACKLIST_PREF "cryptomining-blacklist-pref"
#define TABLE_CRYPTOMINING_WHITELIST_PREF "cryptomining-whitelist-pref"

StaticRefPtr<UrlClassifierFeatureCryptomining> gFeatureCryptomining;

}  // namespace

UrlClassifierFeatureCryptomining::UrlClassifierFeatureCryptomining()
    : UrlClassifierFeatureBase(
          NS_LITERAL_CSTRING(CRYPTOMINING_FEATURE_NAME),
          NS_LITERAL_CSTRING(URLCLASSIFIER_CRYPTOMINING_BLACKLIST),
          NS_LITERAL_CSTRING(URLCLASSIFIER_CRYPTOMINING_WHITELIST),
          NS_LITERAL_CSTRING(URLCLASSIFIER_CRYPTOMINING_BLACKLIST_TEST_ENTRIES),
          NS_LITERAL_CSTRING(URLCLASSIFIER_CRYPTOMINING_WHITELIST_TEST_ENTRIES),
          NS_LITERAL_CSTRING(TABLE_CRYPTOMINING_BLACKLIST_PREF),
          NS_LITERAL_CSTRING(TABLE_CRYPTOMINING_WHITELIST_PREF),
          EmptyCString()) {}

/* static */ const char* UrlClassifierFeatureCryptomining::Name() {
  return CRYPTOMINING_FEATURE_NAME;
}

/* static */ void UrlClassifierFeatureCryptomining::MaybeInitialize() {
  UC_LOG(("UrlClassifierFeatureCryptomining: MaybeInitialize"));

  if (!gFeatureCryptomining) {
    gFeatureCryptomining = new UrlClassifierFeatureCryptomining();
    gFeatureCryptomining->InitializePreferences();
  }
}

/* static */ void UrlClassifierFeatureCryptomining::MaybeShutdown() {
  UC_LOG(("UrlClassifierFeatureCryptomining: MaybeShutdown"));

  if (gFeatureCryptomining) {
    gFeatureCryptomining->ShutdownPreferences();
    gFeatureCryptomining = nullptr;
  }
}

/* static */ already_AddRefed<UrlClassifierFeatureCryptomining>
UrlClassifierFeatureCryptomining::MaybeCreate(nsIChannel* aChannel) {
  MOZ_ASSERT(aChannel);

  UC_LOG(("UrlClassifierFeatureCryptomining: MaybeCreate for channel %p",
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
          ("UrlClassifierFeatureCryptomining: Skipping cryptomining checks "
           "for first party or top-level load channel[%p] "
           "with uri %s",
           aChannel, spec.get()));
    }

    return nullptr;
  }

  if (!UrlClassifierCommon::ShouldEnableClassifier(
          aChannel, AntiTrackingCommon::eCryptomining)) {
    return nullptr;
  }

  MaybeInitialize();
  MOZ_ASSERT(gFeatureCryptomining);

  RefPtr<UrlClassifierFeatureCryptomining> self = gFeatureCryptomining;
  return self.forget();
}

/* static */ already_AddRefed<nsIUrlClassifierFeature>
UrlClassifierFeatureCryptomining::GetIfNameMatches(const nsACString& aName) {
  if (!aName.EqualsLiteral(CRYPTOMINING_FEATURE_NAME)) {
    return nullptr;
  }

  MaybeInitialize();
  MOZ_ASSERT(gFeatureCryptomining);

  RefPtr<UrlClassifierFeatureCryptomining> self = gFeatureCryptomining;
  return self.forget();
}

NS_IMETHODIMP
UrlClassifierFeatureCryptomining::ProcessChannel(nsIChannel* aChannel,
                                                 const nsACString& aList,
                                                 bool* aShouldContinue) {
  NS_ENSURE_ARG_POINTER(aChannel);
  NS_ENSURE_ARG_POINTER(aShouldContinue);

  // This is a blocking feature.
  *aShouldContinue = false;

  UrlClassifierCommon::SetBlockedContent(aChannel, NS_ERROR_TRACKING_URI, aList,
                                         EmptyCString(), EmptyCString());

  UC_LOG(
      ("UrlClassifierFeatureCryptomining::ProcessChannel, cancelling "
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
UrlClassifierFeatureCryptomining::GetURIByListType(
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
