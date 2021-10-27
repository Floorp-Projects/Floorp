/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UrlClassifierFeatureSocialTrackingAnnotation.h"

#include "mozilla/net/UrlClassifierCommon.h"
#include "nsIClassifiedChannel.h"
#include "nsContentUtils.h"
#include "nsNetUtil.h"
#include "mozilla/StaticPtr.h"
#include "nsIWebProgressListener.h"
#include "nsIChannel.h"

namespace mozilla {
namespace net {

namespace {

#define SOCIALTRACKING_ANNOTATION_FEATURE_NAME "socialtracking-annotation"

#define URLCLASSIFIER_SOCIALTRACKING_ANNOTATION_BLOCKLIST \
  "urlclassifier.features.socialtracking.annotate.blacklistTables"
#define URLCLASSIFIER_SOCIALTRACKING_ANNOTATION_BLOCKLIST_TEST_ENTRIES \
  "urlclassifier.features.socialtracking.annotate.blacklistHosts"
#define URLCLASSIFIER_SOCIALTRACKING_ANNOTATION_ENTITYLIST \
  "urlclassifier.features.socialtracking.annotate.whitelistTables"
#define URLCLASSIFIER_SOCIALTRACKING_ANNOTATION_ENTITYLIST_TEST_ENTRIES \
  "urlclassifier.features.socialtracking.annotate.whitelistHosts"
#define URLCLASSIFIER_SOCIALTRACKING_ANNOTATION_EXCEPTION_URLS \
  "urlclassifier.features.socialtracking.annotate.skipURLs"
#define TABLE_SOCIALTRACKING_ANNOTATION_BLOCKLIST_PREF \
  "socialtracking-annotate-blacklist-pref"
#define TABLE_SOCIALTRACKING_ANNOTATION_ENTITYLIST_PREF \
  "socialtracking-annotate-whitelist-pref"

StaticRefPtr<UrlClassifierFeatureSocialTrackingAnnotation>
    gFeatureSocialTrackingAnnotation;

}  // namespace

UrlClassifierFeatureSocialTrackingAnnotation::
    UrlClassifierFeatureSocialTrackingAnnotation()
    : UrlClassifierFeatureAntiTrackingBase(
          nsLiteralCString(SOCIALTRACKING_ANNOTATION_FEATURE_NAME),
          nsLiteralCString(URLCLASSIFIER_SOCIALTRACKING_ANNOTATION_BLOCKLIST),
          nsLiteralCString(URLCLASSIFIER_SOCIALTRACKING_ANNOTATION_ENTITYLIST),
          nsLiteralCString(
              URLCLASSIFIER_SOCIALTRACKING_ANNOTATION_BLOCKLIST_TEST_ENTRIES),
          nsLiteralCString(
              URLCLASSIFIER_SOCIALTRACKING_ANNOTATION_ENTITYLIST_TEST_ENTRIES),
          nsLiteralCString(TABLE_SOCIALTRACKING_ANNOTATION_BLOCKLIST_PREF),
          nsLiteralCString(TABLE_SOCIALTRACKING_ANNOTATION_ENTITYLIST_PREF),
          nsLiteralCString(
              URLCLASSIFIER_SOCIALTRACKING_ANNOTATION_EXCEPTION_URLS)) {}

/* static */ const char* UrlClassifierFeatureSocialTrackingAnnotation::Name() {
  return SOCIALTRACKING_ANNOTATION_FEATURE_NAME;
}

/* static */
void UrlClassifierFeatureSocialTrackingAnnotation::MaybeInitialize() {
  UC_LOG_LEAK(
      ("UrlClassifierFeatureSocialTrackingAnnotation::MaybeInitialize"));

  if (!gFeatureSocialTrackingAnnotation) {
    gFeatureSocialTrackingAnnotation =
        new UrlClassifierFeatureSocialTrackingAnnotation();
    gFeatureSocialTrackingAnnotation->InitializePreferences();
  }
}

/* static */
void UrlClassifierFeatureSocialTrackingAnnotation::MaybeShutdown() {
  UC_LOG_LEAK(("UrlClassifierFeatureSocialTrackingAnnotation::MaybeShutdown"));

  if (gFeatureSocialTrackingAnnotation) {
    gFeatureSocialTrackingAnnotation->ShutdownPreferences();
    gFeatureSocialTrackingAnnotation = nullptr;
  }
}

/* static */
already_AddRefed<UrlClassifierFeatureSocialTrackingAnnotation>
UrlClassifierFeatureSocialTrackingAnnotation::MaybeCreate(
    nsIChannel* aChannel) {
  MOZ_ASSERT(aChannel);

  UC_LOG_LEAK(
      ("UrlClassifierFeatureSocialTrackingAnnotation::MaybeCreate - channel %p",
       aChannel));

  MaybeInitialize();
  MOZ_ASSERT(gFeatureSocialTrackingAnnotation);

  RefPtr<UrlClassifierFeatureSocialTrackingAnnotation> self =
      gFeatureSocialTrackingAnnotation;
  return self.forget();
}

/* static */
already_AddRefed<nsIUrlClassifierFeature>
UrlClassifierFeatureSocialTrackingAnnotation::GetIfNameMatches(
    const nsACString& aName) {
  if (!aName.EqualsLiteral(SOCIALTRACKING_ANNOTATION_FEATURE_NAME)) {
    return nullptr;
  }

  MaybeInitialize();
  MOZ_ASSERT(gFeatureSocialTrackingAnnotation);

  RefPtr<UrlClassifierFeatureSocialTrackingAnnotation> self =
      gFeatureSocialTrackingAnnotation;
  return self.forget();
}

NS_IMETHODIMP
UrlClassifierFeatureSocialTrackingAnnotation::ProcessChannel(
    nsIChannel* aChannel, const nsTArray<nsCString>& aList,
    const nsTArray<nsCString>& aHashes, bool* aShouldContinue) {
  NS_ENSURE_ARG_POINTER(aChannel);
  NS_ENSURE_ARG_POINTER(aShouldContinue);

  // This is not a blocking feature.
  *aShouldContinue = true;

  UC_LOG(
      ("UrlClassifierFeatureSocialTrackingAnnotation::ProcessChannel"
       "annotating channel %p",
       aChannel));

  static std::vector<UrlClassifierCommon::ClassificationData>
      sClassificationData = {
          {"social-tracking-protection-facebook-"_ns,
           nsIClassifiedChannel::ClassificationFlags::
               CLASSIFIED_SOCIALTRACKING_FACEBOOK},
          {"social-tracking-protection-linkedin-"_ns,
           nsIClassifiedChannel::ClassificationFlags::
               CLASSIFIED_SOCIALTRACKING_LINKEDIN},
          {"social-tracking-protection-twitter-"_ns,
           nsIClassifiedChannel::ClassificationFlags::
               CLASSIFIED_SOCIALTRACKING_TWITTER},
      };

  uint32_t flags = UrlClassifierCommon::TablesToClassificationFlags(
      aList, sClassificationData,
      nsIClassifiedChannel::ClassificationFlags::CLASSIFIED_SOCIALTRACKING);

  UrlClassifierCommon::AnnotateChannel(
      aChannel, flags,
      nsIWebProgressListener::STATE_LOADED_SOCIALTRACKING_CONTENT);

  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierFeatureSocialTrackingAnnotation::GetURIByListType(
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
