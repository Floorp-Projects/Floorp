/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UrlClassifierFeatureTrackingProtection.h"

#include "mozilla/AntiTrackingCommon.h"
#include "mozilla/net/UrlClassifierCommon.h"
#include "nsContentUtils.h"
#include "nsIHttpChannelInternal.h"
#include "nsILoadContext.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace net {

namespace {

#define TRACKING_PROTECTION_FEATURE_NAME "tracking-protection"

#define URLCLASSIFIER_TRACKING_BLACKLIST "urlclassifier.trackingTable"
#define URLCLASSIFIER_TRACKING_BLACKLIST_TEST_ENTRIES \
  "urlclassifier.trackingTable.testEntries"
#define URLCLASSIFIER_TRACKING_WHITELIST "urlclassifier.trackingWhitelistTable"
#define URLCLASSIFIER_TRACKING_WHITELIST_TEST_ENTRIES \
  "urlclassifier.trackingWhitelistTable.testEntries"
#define URLCLASSIFIER_TRACKING_PROTECTION_SKIP_URLS \
  "urlclassifier.trackingSkipURLs"
#define TABLE_TRACKING_BLACKLIST_PREF "tracking-blacklist-pref"
#define TABLE_TRACKING_WHITELIST_PREF "tracking-whitelist-pref"

StaticRefPtr<UrlClassifierFeatureTrackingProtection> gFeatureTrackingProtection;

}  // namespace

UrlClassifierFeatureTrackingProtection::UrlClassifierFeatureTrackingProtection()
    : UrlClassifierFeatureBase(
          NS_LITERAL_CSTRING(TRACKING_PROTECTION_FEATURE_NAME),
          NS_LITERAL_CSTRING(URLCLASSIFIER_TRACKING_BLACKLIST),
          NS_LITERAL_CSTRING(URLCLASSIFIER_TRACKING_WHITELIST),
          NS_LITERAL_CSTRING(URLCLASSIFIER_TRACKING_BLACKLIST_TEST_ENTRIES),
          NS_LITERAL_CSTRING(URLCLASSIFIER_TRACKING_WHITELIST_TEST_ENTRIES),
          NS_LITERAL_CSTRING(TABLE_TRACKING_BLACKLIST_PREF),
          NS_LITERAL_CSTRING(TABLE_TRACKING_WHITELIST_PREF),
          NS_LITERAL_CSTRING(URLCLASSIFIER_TRACKING_PROTECTION_SKIP_URLS)) {}

/* static */ const char* UrlClassifierFeatureTrackingProtection::Name() {
  return TRACKING_PROTECTION_FEATURE_NAME;
}

/* static */
void UrlClassifierFeatureTrackingProtection::MaybeInitialize() {
  MOZ_ASSERT(XRE_IsParentProcess());
  UC_LOG(("UrlClassifierFeatureTrackingProtection: MaybeInitialize"));

  if (!gFeatureTrackingProtection) {
    gFeatureTrackingProtection = new UrlClassifierFeatureTrackingProtection();
    gFeatureTrackingProtection->InitializePreferences();
  }
}

/* static */
void UrlClassifierFeatureTrackingProtection::MaybeShutdown() {
  UC_LOG(("UrlClassifierFeatureTrackingProtection: Shutdown"));

  if (gFeatureTrackingProtection) {
    gFeatureTrackingProtection->ShutdownPreferences();
    gFeatureTrackingProtection = nullptr;
  }
}

/* static */
already_AddRefed<UrlClassifierFeatureTrackingProtection>
UrlClassifierFeatureTrackingProtection::MaybeCreate(nsIChannel* aChannel) {
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

  if (!UrlClassifierCommon::ShouldEnableClassifier(aChannel)) {
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

  UrlClassifierCommon::SetBlockedContent(aChannel, NS_ERROR_TRACKING_URI, list,
                                         EmptyCString(), EmptyCString());

  NS_SetRequestBlockingReason(
      aChannel, nsILoadInfo::BLOCKING_REASON_CLASSIFY_TRACKING_URI);

  UC_LOG(
      ("UrlClassifierFeatureTrackingProtection::ProcessChannel, cancelling "
       "channel[%p]",
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
