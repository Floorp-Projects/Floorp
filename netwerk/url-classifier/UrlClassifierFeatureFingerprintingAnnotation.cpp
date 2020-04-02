/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UrlClassifierFeatureFingerprintingAnnotation.h"

#include "mozilla/net/UrlClassifierCommon.h"
#include "nsIClassifiedChannel.h"
#include "nsContentUtils.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace net {

namespace {

#define FINGERPRINTING_ANNOTATION_FEATURE_NAME "fingerprinting-annotation"

#define URLCLASSIFIER_FINGERPRINTING_ANNOTATION_BLACKLIST \
  "urlclassifier.features.fingerprinting.annotate.blacklistTables"
#define URLCLASSIFIER_FINGERPRINTING_ANNOTATION_BLACKLIST_TEST_ENTRIES \
  "urlclassifier.features.fingerprinting.annotate.blacklistHosts"
#define URLCLASSIFIER_FINGERPRINTING_ANNOTATION_WHITELIST \
  "urlclassifier.features.fingerprinting.annotate.whitelistTables"
#define URLCLASSIFIER_FINGERPRINTING_ANNOTATION_WHITELIST_TEST_ENTRIES \
  "urlclassifier.features.fingerprinting.annotate.whitelistHosts"
#define URLCLASSIFIER_FINGERPRINTING_ANNOTATION_SKIP_URLS \
  "urlclassifier.features.fingerprinting.annotate.skipURLs"
#define TABLE_FINGERPRINTING_ANNOTATION_BLACKLIST_PREF \
  "fingerprinting-annotate-blacklist-pref"
#define TABLE_FINGERPRINTING_ANNOTATION_WHITELIST_PREF \
  "fingerprinting-annotate-whitelist-pref"

StaticRefPtr<UrlClassifierFeatureFingerprintingAnnotation>
    gFeatureFingerprintingAnnotation;

}  // namespace

UrlClassifierFeatureFingerprintingAnnotation::
    UrlClassifierFeatureFingerprintingAnnotation()
    : UrlClassifierFeatureBase(
          NS_LITERAL_CSTRING(FINGERPRINTING_ANNOTATION_FEATURE_NAME),
          NS_LITERAL_CSTRING(URLCLASSIFIER_FINGERPRINTING_ANNOTATION_BLACKLIST),
          NS_LITERAL_CSTRING(URLCLASSIFIER_FINGERPRINTING_ANNOTATION_WHITELIST),
          NS_LITERAL_CSTRING(
              URLCLASSIFIER_FINGERPRINTING_ANNOTATION_BLACKLIST_TEST_ENTRIES),
          NS_LITERAL_CSTRING(
              URLCLASSIFIER_FINGERPRINTING_ANNOTATION_WHITELIST_TEST_ENTRIES),
          NS_LITERAL_CSTRING(TABLE_FINGERPRINTING_ANNOTATION_BLACKLIST_PREF),
          NS_LITERAL_CSTRING(TABLE_FINGERPRINTING_ANNOTATION_WHITELIST_PREF),
          NS_LITERAL_CSTRING(
              URLCLASSIFIER_FINGERPRINTING_ANNOTATION_SKIP_URLS)) {}

/* static */ const char* UrlClassifierFeatureFingerprintingAnnotation::Name() {
  return FINGERPRINTING_ANNOTATION_FEATURE_NAME;
}

/* static */
void UrlClassifierFeatureFingerprintingAnnotation::MaybeInitialize() {
  UC_LOG(("UrlClassifierFeatureFingerprintingAnnotation: MaybeInitialize"));

  if (!gFeatureFingerprintingAnnotation) {
    gFeatureFingerprintingAnnotation =
        new UrlClassifierFeatureFingerprintingAnnotation();
    gFeatureFingerprintingAnnotation->InitializePreferences();
  }
}

/* static */
void UrlClassifierFeatureFingerprintingAnnotation::MaybeShutdown() {
  UC_LOG(("UrlClassifierFeatureFingerprintingAnnotation: MaybeShutdown"));

  if (gFeatureFingerprintingAnnotation) {
    gFeatureFingerprintingAnnotation->ShutdownPreferences();
    gFeatureFingerprintingAnnotation = nullptr;
  }
}

/* static */
already_AddRefed<UrlClassifierFeatureFingerprintingAnnotation>
UrlClassifierFeatureFingerprintingAnnotation::MaybeCreate(
    nsIChannel* aChannel) {
  MOZ_ASSERT(aChannel);

  UC_LOG(
      ("UrlClassifierFeatureFingerprintingAnnotation: MaybeCreate for channel "
       "%p",
       aChannel));

  if (!UrlClassifierCommon::ShouldEnableClassifier(aChannel)) {
    return nullptr;
  }

  if (UrlClassifierCommon::IsPassiveContent(aChannel)) {
    return nullptr;
  }

  MaybeInitialize();
  MOZ_ASSERT(gFeatureFingerprintingAnnotation);

  RefPtr<UrlClassifierFeatureFingerprintingAnnotation> self =
      gFeatureFingerprintingAnnotation;
  return self.forget();
}

/* static */
already_AddRefed<nsIUrlClassifierFeature>
UrlClassifierFeatureFingerprintingAnnotation::GetIfNameMatches(
    const nsACString& aName) {
  if (!aName.EqualsLiteral(FINGERPRINTING_ANNOTATION_FEATURE_NAME)) {
    return nullptr;
  }

  MaybeInitialize();
  MOZ_ASSERT(gFeatureFingerprintingAnnotation);

  RefPtr<UrlClassifierFeatureFingerprintingAnnotation> self =
      gFeatureFingerprintingAnnotation;
  return self.forget();
}

NS_IMETHODIMP
UrlClassifierFeatureFingerprintingAnnotation::ProcessChannel(
    nsIChannel* aChannel, const nsTArray<nsCString>& aList,
    const nsTArray<nsCString>& aHashes, bool* aShouldContinue) {
  NS_ENSURE_ARG_POINTER(aChannel);
  NS_ENSURE_ARG_POINTER(aShouldContinue);

  // This is not a blocking feature.
  *aShouldContinue = true;

  UC_LOG(
      ("UrlClassifierFeatureFingerprintingAnnotation::ProcessChannel, "
       "annotating channel[%p]",
       aChannel));

  static std::vector<UrlClassifierCommon::ClassificationData>
      sClassificationData = {
          {NS_LITERAL_CSTRING("content-fingerprinting-track-"),
           nsIClassifiedChannel::ClassificationFlags::
               CLASSIFIED_FINGERPRINTING_CONTENT},
      };

  uint32_t flags = UrlClassifierCommon::TablesToClassificationFlags(
      aList, sClassificationData,
      nsIClassifiedChannel::ClassificationFlags::CLASSIFIED_FINGERPRINTING);

  UrlClassifierCommon::SetTrackingInfo(aChannel, aList, aHashes);

  UrlClassifierCommon::AnnotateChannel(
      aChannel, flags,
      nsIWebProgressListener::STATE_LOADED_FINGERPRINTING_CONTENT);

  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierFeatureFingerprintingAnnotation::GetURIByListType(
    nsIChannel* aChannel, nsIUrlClassifierFeature::listType aListType,
    nsIUrlClassifierFeature::URIType* aURIType, nsIURI** aURI) {
  NS_ENSURE_ARG_POINTER(aChannel);
  NS_ENSURE_ARG_POINTER(aURIType);
  NS_ENSURE_ARG_POINTER(aURI);

  if (aListType == nsIUrlClassifierFeature::blacklist) {
    *aURIType = nsIUrlClassifierFeature::blacklistURI;
    return aChannel->GetURI(aURI);
  }

  MOZ_ASSERT(aListType == nsIUrlClassifierFeature::whitelist);

  *aURIType = nsIUrlClassifierFeature::pairwiseWhitelistURI;
  return UrlClassifierCommon::CreatePairwiseWhiteListURI(aChannel, aURI);
}

}  // namespace net
}  // namespace mozilla
