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
#include "mozilla/StaticPtr.h"
#include "nsIWebProgressListener.h"
#include "nsIChannel.h"

namespace mozilla {
namespace net {

namespace {

#define FINGERPRINTING_ANNOTATION_FEATURE_NAME "fingerprinting-annotation"

#define URLCLASSIFIER_FINGERPRINTING_ANNOTATION_BLOCKLIST \
  "urlclassifier.features.fingerprinting.annotate.blacklistTables"
#define URLCLASSIFIER_FINGERPRINTING_ANNOTATION_BLOCKLIST_TEST_ENTRIES \
  "urlclassifier.features.fingerprinting.annotate.blacklistHosts"
#define URLCLASSIFIER_FINGERPRINTING_ANNOTATION_ENTITYLIST \
  "urlclassifier.features.fingerprinting.annotate.whitelistTables"
#define URLCLASSIFIER_FINGERPRINTING_ANNOTATION_ENTITYLIST_TEST_ENTRIES \
  "urlclassifier.features.fingerprinting.annotate.whitelistHosts"
#define URLCLASSIFIER_FINGERPRINTING_ANNOTATION_EXCEPTION_URLS \
  "urlclassifier.features.fingerprinting.annotate.skipURLs"
#define TABLE_FINGERPRINTING_ANNOTATION_BLOCKLIST_PREF \
  "fingerprinting-annotate-blacklist-pref"
#define TABLE_FINGERPRINTING_ANNOTATION_ENTITYLIST_PREF \
  "fingerprinting-annotate-whitelist-pref"

StaticRefPtr<UrlClassifierFeatureFingerprintingAnnotation>
    gFeatureFingerprintingAnnotation;

}  // namespace

UrlClassifierFeatureFingerprintingAnnotation::
    UrlClassifierFeatureFingerprintingAnnotation()
    : UrlClassifierFeatureAntiTrackingBase(
          nsLiteralCString(FINGERPRINTING_ANNOTATION_FEATURE_NAME),
          nsLiteralCString(URLCLASSIFIER_FINGERPRINTING_ANNOTATION_BLOCKLIST),
          nsLiteralCString(URLCLASSIFIER_FINGERPRINTING_ANNOTATION_ENTITYLIST),
          nsLiteralCString(
              URLCLASSIFIER_FINGERPRINTING_ANNOTATION_BLOCKLIST_TEST_ENTRIES),
          nsLiteralCString(
              URLCLASSIFIER_FINGERPRINTING_ANNOTATION_ENTITYLIST_TEST_ENTRIES),
          nsLiteralCString(TABLE_FINGERPRINTING_ANNOTATION_BLOCKLIST_PREF),
          nsLiteralCString(TABLE_FINGERPRINTING_ANNOTATION_ENTITYLIST_PREF),
          nsLiteralCString(
              URLCLASSIFIER_FINGERPRINTING_ANNOTATION_EXCEPTION_URLS)) {}

/* static */ const char* UrlClassifierFeatureFingerprintingAnnotation::Name() {
  return FINGERPRINTING_ANNOTATION_FEATURE_NAME;
}

/* static */
void UrlClassifierFeatureFingerprintingAnnotation::MaybeInitialize() {
  UC_LOG_LEAK(
      ("UrlClassifierFeatureFingerprintingAnnotation::MaybeInitialize"));

  if (!gFeatureFingerprintingAnnotation) {
    gFeatureFingerprintingAnnotation =
        new UrlClassifierFeatureFingerprintingAnnotation();
    gFeatureFingerprintingAnnotation->InitializePreferences();
  }
}

/* static */
void UrlClassifierFeatureFingerprintingAnnotation::MaybeShutdown() {
  UC_LOG_LEAK(("UrlClassifierFeatureFingerprintingAnnotation::MaybeShutdown"));

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

  UC_LOG_LEAK(
      ("UrlClassifierFeatureFingerprintingAnnotation::MaybeCreate - channel %p",
       aChannel));

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
      ("UrlClassifierFeatureFingerprintingAnnotation::ProcessChannel - "
       "annotating channel %p",
       aChannel));

  static std::vector<UrlClassifierCommon::ClassificationData>
      sClassificationData = {
          {"content-fingerprinting-track-"_ns,
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
