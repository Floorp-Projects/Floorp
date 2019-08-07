/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UrlClassifierFeatureTrackingAnnotation.h"

#include "Classifier.h"
#include "mozilla/AntiTrackingCommon.h"
#include "mozilla/Logging.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/net/UrlClassifierCommon.h"
#include "nsContentUtils.h"

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
UrlClassifierFeatureTrackingAnnotation::ProcessChannel(
    nsIChannel* aChannel, const nsTArray<nsCString>& aList,
    const nsTArray<nsCString>& aHashes, bool* aShouldContinue) {
  NS_ENSURE_ARG_POINTER(aChannel);
  NS_ENSURE_ARG_POINTER(aShouldContinue);

  // This is not a blocking feature.
  *aShouldContinue = true;

  static std::vector<UrlClassifierCommon::ClassificationData>
      sClassificationData = {
          {NS_LITERAL_CSTRING("ads-track-"),
           nsIHttpChannel::ClassificationFlags::CLASSIFIED_TRACKING_AD},
          {NS_LITERAL_CSTRING("analytics-track-"),
           nsIHttpChannel::ClassificationFlags::CLASSIFIED_TRACKING_ANALYTICS},
          {NS_LITERAL_CSTRING("social-track-"),
           nsIHttpChannel::ClassificationFlags::CLASSIFIED_TRACKING_SOCIAL},
          {NS_LITERAL_CSTRING("content-track-"),
           nsIHttpChannel::ClassificationFlags::CLASSIFIED_TRACKING_CONTENT},
      };

  uint32_t flags = UrlClassifierCommon::TablesToClassificationFlags(
      aList, sClassificationData,
      nsIHttpChannel::ClassificationFlags::CLASSIFIED_TRACKING);

  UrlClassifierCommon::SetTrackingInfo(aChannel, aList, aHashes);

  UrlClassifierCommon::AnnotateChannel(
      aChannel, flags, nsIWebProgressListener::STATE_LOADED_TRACKING_CONTENT);

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
