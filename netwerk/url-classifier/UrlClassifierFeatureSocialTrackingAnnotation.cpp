/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UrlClassifierFeatureSocialTrackingAnnotation.h"

#include "mozilla/AntiTrackingCommon.h"
#include "mozilla/net/UrlClassifierCommon.h"
#include "nsContentUtils.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace net {

namespace {

#define SOCIALTRACKING_ANNOTATION_FEATURE_NAME "socialtracking-annotation"

#define URLCLASSIFIER_SOCIALTRACKING_ANNOTATION_BLACKLIST \
  "urlclassifier.features.socialtracking.annotate.blacklistTables"
#define URLCLASSIFIER_SOCIALTRACKING_ANNOTATION_BLACKLIST_TEST_ENTRIES \
  "urlclassifier.features.socialtracking.annotate.blacklistHosts"
#define URLCLASSIFIER_SOCIALTRACKING_ANNOTATION_WHITELIST \
  "urlclassifier.features.socialtracking.annotate.whitelistTables"
#define URLCLASSIFIER_SOCIALTRACKING_ANNOTATION_WHITELIST_TEST_ENTRIES \
  "urlclassifier.features.socialtracking.annotate.whitelistHosts"
#define URLCLASSIFIER_SOCIALTRACKING_ANNOTATION_SKIP_URLS \
  "urlclassifier.features.socialtracking.annotate.skipURLs"
#define TABLE_SOCIALTRACKING_ANNOTATION_BLACKLIST_PREF \
  "socialtracking-annotate-blacklist-pref"
#define TABLE_SOCIALTRACKING_ANNOTATION_WHITELIST_PREF \
  "socialtracking-annotate-whitelist-pref"

StaticRefPtr<UrlClassifierFeatureSocialTrackingAnnotation>
    gFeatureSocialTrackingAnnotation;

}  // namespace

UrlClassifierFeatureSocialTrackingAnnotation::
    UrlClassifierFeatureSocialTrackingAnnotation()
    : UrlClassifierFeatureBase(
          NS_LITERAL_CSTRING(SOCIALTRACKING_ANNOTATION_FEATURE_NAME),
          NS_LITERAL_CSTRING(URLCLASSIFIER_SOCIALTRACKING_ANNOTATION_BLACKLIST),
          NS_LITERAL_CSTRING(URLCLASSIFIER_SOCIALTRACKING_ANNOTATION_WHITELIST),
          NS_LITERAL_CSTRING(
              URLCLASSIFIER_SOCIALTRACKING_ANNOTATION_BLACKLIST_TEST_ENTRIES),
          NS_LITERAL_CSTRING(
              URLCLASSIFIER_SOCIALTRACKING_ANNOTATION_WHITELIST_TEST_ENTRIES),
          NS_LITERAL_CSTRING(TABLE_SOCIALTRACKING_ANNOTATION_BLACKLIST_PREF),
          NS_LITERAL_CSTRING(TABLE_SOCIALTRACKING_ANNOTATION_WHITELIST_PREF),
          NS_LITERAL_CSTRING(
              URLCLASSIFIER_SOCIALTRACKING_ANNOTATION_SKIP_URLS)) {}

/* static */ const char* UrlClassifierFeatureSocialTrackingAnnotation::Name() {
  return SOCIALTRACKING_ANNOTATION_FEATURE_NAME;
}

/* static */
void UrlClassifierFeatureSocialTrackingAnnotation::MaybeInitialize() {
  UC_LOG(("UrlClassifierFeatureSocialTrackingAnnotation: MaybeInitialize"));

  if (!gFeatureSocialTrackingAnnotation) {
    gFeatureSocialTrackingAnnotation =
        new UrlClassifierFeatureSocialTrackingAnnotation();
    gFeatureSocialTrackingAnnotation->InitializePreferences();
  }
}

/* static */
void UrlClassifierFeatureSocialTrackingAnnotation::MaybeShutdown() {
  UC_LOG(("UrlClassifierFeatureSocialTrackingAnnotation: MaybeShutdown"));

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

  UC_LOG(
      ("UrlClassifierFeatureSocialTrackingAnnotation: MaybeCreate for channel "
       "%p",
       aChannel));

  if (!UrlClassifierCommon::ShouldEnableClassifier(aChannel)) {
    return nullptr;
  }

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
      ("UrlClassifierFeatureSocialTrackingAnnotation::ProcessChannel, "
       "annotating channel[%p]",
       aChannel));

  static std::vector<UrlClassifierCommon::ClassificationData>
      sClassificationData = {
          {NS_LITERAL_CSTRING("social-tracking-protection-facebook-"),
           nsIHttpChannel::ClassificationFlags::
               CLASSIFIED_SOCIALTRACKING_FACEBOOK},
          {NS_LITERAL_CSTRING("social-tracking-protection-linkedin-"),
           nsIHttpChannel::ClassificationFlags::
               CLASSIFIED_SOCIALTRACKING_LINKEDIN},
          {NS_LITERAL_CSTRING("social-tracking-protection-twitter-"),
           nsIHttpChannel::ClassificationFlags::
               CLASSIFIED_SOCIALTRACKING_TWITTER},
      };

  uint32_t flags = UrlClassifierCommon::TablesToClassificationFlags(
      aList, sClassificationData,
      nsIHttpChannel::ClassificationFlags::CLASSIFIED_SOCIALTRACKING);

  UrlClassifierCommon::AnnotateChannel(
      aChannel, flags,
      nsIWebProgressListener::STATE_LOADED_SOCIALTRACKING_CONTENT);

  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierFeatureSocialTrackingAnnotation::GetURIByListType(
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
