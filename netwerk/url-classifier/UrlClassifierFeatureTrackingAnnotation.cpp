/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UrlClassifierFeatureTrackingAnnotation.h"

#include "Classifier.h"
#include "mozilla/Logging.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/net/UrlClassifierCommon.h"
#include "nsIChannel.h"
#include "nsIClassifiedChannel.h"
#include "nsIWebProgressListener.h"
#include "nsContentUtils.h"

namespace mozilla {
namespace net {

namespace {

#define TRACKING_ANNOTATION_FEATURE_NAME "tracking-annotation"

#define URLCLASSIFIER_ANNOTATION_BLOCKLIST \
  "urlclassifier.trackingAnnotationTable"
#define URLCLASSIFIER_ANNOTATION_BLOCKLIST_TEST_ENTRIES \
  "urlclassifier.trackingAnnotationTable.testEntries"
#define URLCLASSIFIER_ANNOTATION_ENTITYLIST \
  "urlclassifier.trackingAnnotationWhitelistTable"
#define URLCLASSIFIER_ANNOTATION_ENTITYLIST_TEST_ENTRIES \
  "urlclassifier.trackingAnnotationWhitelistTable.testEntries"
#define URLCLASSIFIER_TRACKING_ANNOTATION_EXCEPTION_URLS \
  "urlclassifier.trackingAnnotationSkipURLs"
#define TABLE_ANNOTATION_BLOCKLIST_PREF "annotation-blacklist-pref"
#define TABLE_ANNOTATION_ENTITYLIST_PREF "annotation-whitelist-pref"

StaticRefPtr<UrlClassifierFeatureTrackingAnnotation> gFeatureTrackingAnnotation;

}  // namespace

UrlClassifierFeatureTrackingAnnotation::UrlClassifierFeatureTrackingAnnotation()
    : UrlClassifierFeatureAntiTrackingBase(
          nsLiteralCString(TRACKING_ANNOTATION_FEATURE_NAME),
          nsLiteralCString(URLCLASSIFIER_ANNOTATION_BLOCKLIST),
          nsLiteralCString(URLCLASSIFIER_ANNOTATION_ENTITYLIST),
          nsLiteralCString(URLCLASSIFIER_ANNOTATION_BLOCKLIST_TEST_ENTRIES),
          nsLiteralCString(URLCLASSIFIER_ANNOTATION_ENTITYLIST_TEST_ENTRIES),
          nsLiteralCString(TABLE_ANNOTATION_BLOCKLIST_PREF),
          nsLiteralCString(TABLE_ANNOTATION_ENTITYLIST_PREF),
          nsLiteralCString(URLCLASSIFIER_TRACKING_ANNOTATION_EXCEPTION_URLS)) {}

/* static */ const char* UrlClassifierFeatureTrackingAnnotation::Name() {
  return TRACKING_ANNOTATION_FEATURE_NAME;
}

/* static */
void UrlClassifierFeatureTrackingAnnotation::MaybeInitialize() {
  MOZ_ASSERT(XRE_IsParentProcess());
  UC_LOG_LEAK(("UrlClassifierFeatureTrackingAnnotation::MaybeInitialize"));

  if (!gFeatureTrackingAnnotation) {
    gFeatureTrackingAnnotation = new UrlClassifierFeatureTrackingAnnotation();
    gFeatureTrackingAnnotation->InitializePreferences();
  }
}

/* static */
void UrlClassifierFeatureTrackingAnnotation::MaybeShutdown() {
  UC_LOG_LEAK(("UrlClassifierFeatureTrackingAnnotation::MaybeShutdown"));

  if (gFeatureTrackingAnnotation) {
    gFeatureTrackingAnnotation->ShutdownPreferences();
    gFeatureTrackingAnnotation = nullptr;
  }
}

/* static */
already_AddRefed<UrlClassifierFeatureTrackingAnnotation>
UrlClassifierFeatureTrackingAnnotation::MaybeCreate(nsIChannel* aChannel) {
  MOZ_ASSERT(aChannel);

  UC_LOG_LEAK(
      ("UrlClassifierFeatureTrackingAnnotation::MaybeCreate - channel %p",
       aChannel));

  if (!StaticPrefs::privacy_trackingprotection_annotate_channels()) {
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
UrlClassifierFeatureTrackingAnnotation::ProcessChannel(
    nsIChannel* aChannel, const nsTArray<nsCString>& aList,
    const nsTArray<nsCString>& aHashes, bool* aShouldContinue) {
  NS_ENSURE_ARG_POINTER(aChannel);
  NS_ENSURE_ARG_POINTER(aShouldContinue);

  // This is not a blocking feature.
  *aShouldContinue = true;

  UC_LOG(
      ("UrlClassifierFeatureTrackingAnnotation::ProcessChannel - "
       "annotating channel %p",
       aChannel));

  static std::vector<UrlClassifierCommon::ClassificationData>
      sClassificationData = {
          {"ads-track-"_ns,
           nsIClassifiedChannel::ClassificationFlags::CLASSIFIED_TRACKING_AD},
          {"analytics-track-"_ns, nsIClassifiedChannel::ClassificationFlags::
                                      CLASSIFIED_TRACKING_ANALYTICS},
          {"social-track-"_ns, nsIClassifiedChannel::ClassificationFlags::
                                   CLASSIFIED_TRACKING_SOCIAL},
          {"content-track-"_ns, nsIClassifiedChannel::ClassificationFlags::
                                    CLASSIFIED_TRACKING_CONTENT},
      };

  uint32_t flags = UrlClassifierCommon::TablesToClassificationFlags(
      aList, sClassificationData,
      nsIClassifiedChannel::ClassificationFlags::CLASSIFIED_TRACKING);

  UrlClassifierCommon::SetTrackingInfo(aChannel, aList, aHashes);

  uint32_t notification =
      ((flags & nsIClassifiedChannel::ClassificationFlags::
                    CLASSIFIED_TRACKING_CONTENT) != 0)
          ? nsIWebProgressListener::STATE_LOADED_LEVEL_2_TRACKING_CONTENT
          : nsIWebProgressListener::STATE_LOADED_LEVEL_1_TRACKING_CONTENT;

  UrlClassifierCommon::AnnotateChannel(aChannel, flags, notification);

  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierFeatureTrackingAnnotation::GetURIByListType(
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
