/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UrlClassifierFeatureTrackingAnnotation.h"

#include "mozilla/AntiTrackingCommon.h"
#include "mozilla/Logging.h"
#include "mozilla/net/HttpBaseChannel.h"
#include "mozilla/StaticPrefs.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/net/UrlClassifierCommon.h"
#include "nsContentUtils.h"
#include "nsQueryObject.h"
#include "TrackingDummyChannel.h"

namespace mozilla {
namespace net {

namespace {

#define TRACKING_ANNOTATION_FEATURE_NAME "tracking-annotation"

#define URLCLASSIFIER_ANNOTATION_BLACKLIST \
  "urlclassifier.trackingAnnotationTable"
#define URLCLASSIFIER_ANNOTATION_BLACKLIST_TEST_ENTRIES \
  "urlclassifier.trackingAnnotationTable.testEntries"
#define URLCLASSIFIER_ANNOTATION_WHITELIST \
  "urlclassifier.trackingAnnotationWhitelistTable"
#define URLCLASSIFIER_ANNOTATION_WHITELIST_TEST_ENTRIES \
  "urlclassifier.trackingAnnotationWhitelistTable.testEntries"
#define URLCLASSIFIER_TRACKING_ANNOTATION_SKIP_URLS \
  "urlclassifier.trackingAnnotationSkipURLs"
#define TABLE_ANNOTATION_BLACKLIST_PREF "annotation-blacklist-pref"
#define TABLE_ANNOTATION_WHITELIST_PREF "annotation-whitelist-pref"

StaticRefPtr<UrlClassifierFeatureTrackingAnnotation> gFeatureTrackingAnnotation;

static void SetIsTrackingResourceHelper(nsIChannel* aChannel,
                                        bool aIsThirdParty) {
  MOZ_ASSERT(aChannel);

  nsCOMPtr<nsIParentChannel> parentChannel;
  NS_QueryNotificationCallbacks(aChannel, parentChannel);
  if (parentChannel) {
    // This channel is a parent-process proxy for a child process
    // request. We should notify the child process as well.
    parentChannel->NotifyTrackingResource(aIsThirdParty);
  }

  RefPtr<HttpBaseChannel> httpChannel = do_QueryObject(aChannel);
  if (httpChannel) {
    httpChannel->SetIsTrackingResource(aIsThirdParty);
  }

  RefPtr<TrackingDummyChannel> dummyChannel = do_QueryObject(aChannel);
  if (dummyChannel) {
    dummyChannel->SetIsTrackingResource();
  }
}

static void LowerPriorityHelper(nsIChannel* aChannel) {
  MOZ_ASSERT(aChannel);

  bool isBlockingResource = false;

  nsCOMPtr<nsIClassOfService> cos(do_QueryInterface(aChannel));
  if (cos) {
    if (nsContentUtils::IsTailingEnabled()) {
      uint32_t cosFlags = 0;
      cos->GetClassFlags(&cosFlags);
      isBlockingResource =
          cosFlags & (nsIClassOfService::UrgentStart |
                      nsIClassOfService::Leader | nsIClassOfService::Unblocked);

      // Requests not allowed to be tailed are usually those with higher
      // prioritization.  That overweights being a tracker: don't throttle
      // them when not in background.
      if (!(cosFlags & nsIClassOfService::TailForbidden)) {
        cos->AddClassFlags(nsIClassOfService::Throttleable);
      }
    } else {
      // Yes, we even don't want to evaluate the isBlockingResource when tailing
      // is off see bug 1395525.

      cos->AddClassFlags(nsIClassOfService::Throttleable);
    }
  }

  if (!isBlockingResource) {
    nsCOMPtr<nsISupportsPriority> p = do_QueryInterface(aChannel);
    if (p) {
      if (UC_LOG_ENABLED()) {
        nsCOMPtr<nsIURI> uri;
        aChannel->GetURI(getter_AddRefs(uri));
        nsAutoCString spec;
        uri->GetAsciiSpec(spec);
        spec.Truncate(
            std::min(spec.Length(), UrlClassifierCommon::sMaxSpecLength));
        UC_LOG(("Setting PRIORITY_LOWEST for channel[%p] (%s)", aChannel,
                spec.get()));
      }
      p->SetPriority(nsISupportsPriority::PRIORITY_LOWEST);
    }
  }
}

}  // namespace

UrlClassifierFeatureTrackingAnnotation::UrlClassifierFeatureTrackingAnnotation()
    : UrlClassifierFeatureBase(
          NS_LITERAL_CSTRING(TRACKING_ANNOTATION_FEATURE_NAME),
          NS_LITERAL_CSTRING(URLCLASSIFIER_ANNOTATION_BLACKLIST),
          NS_LITERAL_CSTRING(URLCLASSIFIER_ANNOTATION_WHITELIST),
          NS_LITERAL_CSTRING(URLCLASSIFIER_ANNOTATION_BLACKLIST_TEST_ENTRIES),
          NS_LITERAL_CSTRING(URLCLASSIFIER_ANNOTATION_WHITELIST_TEST_ENTRIES),
          NS_LITERAL_CSTRING(TABLE_ANNOTATION_BLACKLIST_PREF),
          NS_LITERAL_CSTRING(TABLE_ANNOTATION_WHITELIST_PREF),
          NS_LITERAL_CSTRING(URLCLASSIFIER_TRACKING_ANNOTATION_SKIP_URLS)) {}

/* static */ const char* UrlClassifierFeatureTrackingAnnotation::Name() {
  return TRACKING_ANNOTATION_FEATURE_NAME;
}

/* static */
void UrlClassifierFeatureTrackingAnnotation::MaybeInitialize() {
  MOZ_ASSERT(XRE_IsParentProcess());
  UC_LOG(("UrlClassifierFeatureTrackingAnnotation: MaybeInitialize"));

  if (!gFeatureTrackingAnnotation) {
    gFeatureTrackingAnnotation = new UrlClassifierFeatureTrackingAnnotation();
    gFeatureTrackingAnnotation->InitializePreferences();
  }
}

/* static */
void UrlClassifierFeatureTrackingAnnotation::MaybeShutdown() {
  UC_LOG(("UrlClassifierFeatureTrackingAnnotation: MaybeShutdown"));

  if (gFeatureTrackingAnnotation) {
    gFeatureTrackingAnnotation->ShutdownPreferences();
    gFeatureTrackingAnnotation = nullptr;
  }
}

/* static */
already_AddRefed<UrlClassifierFeatureTrackingAnnotation>
UrlClassifierFeatureTrackingAnnotation::MaybeCreate(nsIChannel* aChannel) {
  MOZ_ASSERT(aChannel);

  UC_LOG(("UrlClassifierFeatureTrackingAnnotation: MaybeCreate for channel %p",
          aChannel));

  if (!StaticPrefs::privacy_trackingprotection_annotate_channels()) {
    return nullptr;
  }

  if (!UrlClassifierCommon::ShouldEnableClassifier(aChannel)) {
    return nullptr;
  }

  MaybeInitialize();
  MOZ_ASSERT(gFeatureTrackingAnnotation);

  RefPtr<UrlClassifierFeatureTrackingAnnotation> self =
      gFeatureTrackingAnnotation;
  return self.forget();
}

/* static */
already_AddRefed<nsIUrlClassifierFeature>
UrlClassifierFeatureTrackingAnnotation::GetIfNameMatches(
    const nsACString& aName) {
  if (!aName.EqualsLiteral(TRACKING_ANNOTATION_FEATURE_NAME)) {
    return nullptr;
  }

  MaybeInitialize();
  MOZ_ASSERT(gFeatureTrackingAnnotation);

  RefPtr<UrlClassifierFeatureTrackingAnnotation> self =
      gFeatureTrackingAnnotation;
  return self.forget();
}

NS_IMETHODIMP
UrlClassifierFeatureTrackingAnnotation::ProcessChannel(nsIChannel* aChannel,
                                                       const nsACString& aList,
                                                       bool* aShouldContinue) {
  NS_ENSURE_ARG_POINTER(aChannel);
  NS_ENSURE_ARG_POINTER(aShouldContinue);

  // This is not a blocking feature.
  *aShouldContinue = true;

  nsCOMPtr<nsIURI> chanURI;
  nsresult rv = aChannel->GetURI(getter_AddRefs(chanURI));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    UC_LOG(
        ("UrlClassifierFeatureTrackingAnnotation::ProcessChannel "
         "nsIChannel::GetURI(%p) failed",
         (void*)aChannel));
    return NS_OK;
  }

  bool isThirdPartyWithTopLevelWinURI =
      nsContentUtils::IsThirdPartyWindowOrChannel(nullptr, aChannel, chanURI);

  bool isAllowListed =
      IsAllowListed(aChannel, AntiTrackingCommon::eTrackingAnnotations);

  UC_LOG(
      ("UrlClassifierFeatureTrackingAnnotation::ProcessChannel, annotating "
       "channel[%p]",
       aChannel));

  SetIsTrackingResourceHelper(aChannel, isThirdPartyWithTopLevelWinURI);

  if (isThirdPartyWithTopLevelWinURI || isAllowListed) {
    // Even with TP disabled, we still want to show the user that there
    // are unblocked trackers on the site, so notify the UI that we loaded
    // tracking content. UI code can treat this notification differently
    // depending on whether TP is enabled or disabled.
    UrlClassifierCommon::NotifyChannelClassifierProtectionDisabled(
        aChannel, nsIWebProgressListener::STATE_LOADED_TRACKING_CONTENT);
  }

  if (isThirdPartyWithTopLevelWinURI &&
      StaticPrefs::privacy_trackingprotection_lower_network_priority()) {
    LowerPriorityHelper(aChannel);
  }

  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierFeatureTrackingAnnotation::GetURIByListType(
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
