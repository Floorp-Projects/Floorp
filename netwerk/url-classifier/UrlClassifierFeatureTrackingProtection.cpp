/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UrlClassifierFeatureTrackingProtection.h"

#include "mozilla/AntiTrackingCommon.h"
#include "mozilla/net/UrlClassifierCommon.h"
#include "nsContentUtils.h"
#include "nsILoadContext.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace net {

namespace {

#define URLCLASSIFIER_TRACKING_BLACKLIST "urlclassifier.trackingTable"
#define URLCLASSIFIER_TRACKING_BLACKLIST_TEST_ENTRIES \
  "urlclassifier.trackingTable.testEntries"
#define URLCLASSIFIER_TRACKING_WHITELIST "urlclassifier.trackingWhitelistTable"
#define URLCLASSIFIER_TRACKING_WHITELIST_TEST_ENTRIES \
  "urlclassifier.trackingWhitelistTable.testEntries"
#define TABLE_TRACKING_BLACKLIST_PREF "tracking-blacklist-pref"
#define TABLE_TRACKING_WHITELIST_PREF "tracking-whitelist-pref"

StaticRefPtr<UrlClassifierFeatureTrackingProtection> gFeatureTrackingProtection;

}  // namespace

UrlClassifierFeatureTrackingProtection::UrlClassifierFeatureTrackingProtection()
    : UrlClassifierFeatureBase(
          NS_LITERAL_CSTRING("tracking-protection"),
          NS_LITERAL_CSTRING(URLCLASSIFIER_TRACKING_BLACKLIST),
          NS_LITERAL_CSTRING(URLCLASSIFIER_TRACKING_WHITELIST),
          NS_LITERAL_CSTRING(URLCLASSIFIER_TRACKING_BLACKLIST_TEST_ENTRIES),
          NS_LITERAL_CSTRING(URLCLASSIFIER_TRACKING_WHITELIST_TEST_ENTRIES),
          NS_LITERAL_CSTRING(TABLE_TRACKING_BLACKLIST_PREF),
          NS_LITERAL_CSTRING(TABLE_TRACKING_WHITELIST_PREF), EmptyCString()) {}

/* static */ void UrlClassifierFeatureTrackingProtection::Initialize() {
  UC_LOG(("UrlClassifierFeatureTrackingProtection: Initializing"));
  MOZ_ASSERT(!gFeatureTrackingProtection);

  gFeatureTrackingProtection = new UrlClassifierFeatureTrackingProtection();
  gFeatureTrackingProtection->InitializePreferences();
}

/* static */ void UrlClassifierFeatureTrackingProtection::Shutdown() {
  UC_LOG(("UrlClassifierFeatureTrackingProtection: Shutdown"));
  MOZ_ASSERT(gFeatureTrackingProtection);

  gFeatureTrackingProtection->ShutdownPreferences();
  gFeatureTrackingProtection = nullptr;
}

/* static */ already_AddRefed<UrlClassifierFeatureTrackingProtection>
UrlClassifierFeatureTrackingProtection::MaybeCreate(nsIChannel* aChannel) {
  MOZ_ASSERT(gFeatureTrackingProtection);
  MOZ_ASSERT(aChannel);

  UC_LOG(("UrlClassifierFeatureTrackingProtection: MaybeCreate for channel %p",
          aChannel));

  nsCOMPtr<nsILoadContext> loadContext;
  NS_QueryNotificationCallbacks(aChannel, loadContext);
  if (!loadContext || !loadContext->UseTrackingProtection()) {
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
          ("UrlClassifierFeatureTrackingProtection: Skipping tracking "
           "protection checks for first party or top-level load channel[%p] "
           "with uri %s",
           aChannel, spec.get()));
    }

    return nullptr;
  }

  if (!UrlClassifierCommon::ShouldEnableTrackingProtectionOrAnnotation(
          aChannel, AntiTrackingCommon::eTrackingProtection)) {
    return nullptr;
  }

  RefPtr<UrlClassifierFeatureTrackingProtection> self =
      gFeatureTrackingProtection;
  return self.forget();
}

}  // namespace net
}  // namespace mozilla
