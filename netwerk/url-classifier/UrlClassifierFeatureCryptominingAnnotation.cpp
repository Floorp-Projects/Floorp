/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UrlClassifierFeatureCryptominingAnnotation.h"

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

#define CRYPTOMINING_ANNOTATION_FEATURE_NAME "cryptomining-annotation"

#define URLCLASSIFIER_CRYPTOMINING_ANNOTATION_BLOCKLIST \
  "urlclassifier.features.cryptomining.annotate.blacklistTables"
#define URLCLASSIFIER_CRYPTOMINING_ANNOTATION_BLOCKLIST_TEST_ENTRIES \
  "urlclassifier.features.cryptomining.annotate.blacklistHosts"
#define URLCLASSIFIER_CRYPTOMINING_ANNOTATION_ENTITYLIST \
  "urlclassifier.features.cryptomining.annotate.whitelistTables"
#define URLCLASSIFIER_CRYPTOMINING_ANNOTATION_ENTITYLIST_TEST_ENTRIES \
  "urlclassifier.features.cryptomining.annotate.whitelistHosts"
#define URLCLASSIFIER_CRYPTOMINING_ANNOTATION_EXCEPTION_URLS \
  "urlclassifier.features.cryptomining.annotate.skipURLs"
#define TABLE_CRYPTOMINING_ANNOTATION_BLOCKLIST_PREF \
  "cryptomining-annotate-blacklist-pref"
#define TABLE_CRYPTOMINING_ANNOTATION_ENTITYLIST_PREF \
  "cryptomining-annotate-whitelist-pref"

StaticRefPtr<UrlClassifierFeatureCryptominingAnnotation>
    gFeatureCryptominingAnnotation;

}  // namespace

UrlClassifierFeatureCryptominingAnnotation::
    UrlClassifierFeatureCryptominingAnnotation()
    : UrlClassifierFeatureAntiTrackingBase(
          nsLiteralCString(CRYPTOMINING_ANNOTATION_FEATURE_NAME),
          nsLiteralCString(URLCLASSIFIER_CRYPTOMINING_ANNOTATION_BLOCKLIST),
          nsLiteralCString(URLCLASSIFIER_CRYPTOMINING_ANNOTATION_ENTITYLIST),
          nsLiteralCString(
              URLCLASSIFIER_CRYPTOMINING_ANNOTATION_BLOCKLIST_TEST_ENTRIES),
          nsLiteralCString(
              URLCLASSIFIER_CRYPTOMINING_ANNOTATION_ENTITYLIST_TEST_ENTRIES),
          nsLiteralCString(TABLE_CRYPTOMINING_ANNOTATION_BLOCKLIST_PREF),
          nsLiteralCString(TABLE_CRYPTOMINING_ANNOTATION_ENTITYLIST_PREF),
          nsLiteralCString(
              URLCLASSIFIER_CRYPTOMINING_ANNOTATION_EXCEPTION_URLS)) {}
/* static */ const char* UrlClassifierFeatureCryptominingAnnotation::Name() {
  return CRYPTOMINING_ANNOTATION_FEATURE_NAME;
}

/* static */
void UrlClassifierFeatureCryptominingAnnotation::MaybeInitialize() {
  UC_LOG_LEAK(("UrlClassifierFeatureCryptominingAnnotation::MaybeInitialize"));

  if (!gFeatureCryptominingAnnotation) {
    gFeatureCryptominingAnnotation =
        new UrlClassifierFeatureCryptominingAnnotation();
    gFeatureCryptominingAnnotation->InitializePreferences();
  }
}

/* static */
void UrlClassifierFeatureCryptominingAnnotation::MaybeShutdown() {
  UC_LOG_LEAK(("UrlClassifierFeatureCryptominingAnnotation::MaybeShutdown"));

  if (gFeatureCryptominingAnnotation) {
    gFeatureCryptominingAnnotation->ShutdownPreferences();
    gFeatureCryptominingAnnotation = nullptr;
  }
}

/* static */
already_AddRefed<UrlClassifierFeatureCryptominingAnnotation>
UrlClassifierFeatureCryptominingAnnotation::MaybeCreate(nsIChannel* aChannel) {
  MOZ_ASSERT(aChannel);

  UC_LOG_LEAK(
      ("UrlClassifierFeatureCryptominingAnnotation::MaybeCreate - channel %p",
       aChannel));

  MaybeInitialize();
  MOZ_ASSERT(gFeatureCryptominingAnnotation);

  RefPtr<UrlClassifierFeatureCryptominingAnnotation> self =
      gFeatureCryptominingAnnotation;
  return self.forget();
}

/* static */
already_AddRefed<nsIUrlClassifierFeature>
UrlClassifierFeatureCryptominingAnnotation::GetIfNameMatches(
    const nsACString& aName) {
  if (!aName.EqualsLiteral(CRYPTOMINING_ANNOTATION_FEATURE_NAME)) {
    return nullptr;
  }

  MaybeInitialize();
  MOZ_ASSERT(gFeatureCryptominingAnnotation);

  RefPtr<UrlClassifierFeatureCryptominingAnnotation> self =
      gFeatureCryptominingAnnotation;
  return self.forget();
}

NS_IMETHODIMP
UrlClassifierFeatureCryptominingAnnotation::ProcessChannel(
    nsIChannel* aChannel, const nsTArray<nsCString>& aList,
    const nsTArray<nsCString>& aHashes, bool* aShouldContinue) {
  NS_ENSURE_ARG_POINTER(aChannel);
  NS_ENSURE_ARG_POINTER(aShouldContinue);

  // This is not a blocking feature.
  *aShouldContinue = true;

  UC_LOG(
      ("UrlClassifierFeatureCryptominingAnnotation::ProcessChannel - "
       "annotating channel %p",
       aChannel));

  static std::vector<UrlClassifierCommon::ClassificationData>
      sClassificationData = {
          {"content-cryptomining-track-"_ns,
           nsIClassifiedChannel::ClassificationFlags::
               CLASSIFIED_CRYPTOMINING_CONTENT},
      };

  uint32_t flags = UrlClassifierCommon::TablesToClassificationFlags(
      aList, sClassificationData,
      nsIClassifiedChannel::ClassificationFlags::CLASSIFIED_CRYPTOMINING);

  UrlClassifierCommon::SetTrackingInfo(aChannel, aList, aHashes);

  UrlClassifierCommon::AnnotateChannel(
      aChannel, flags,
      nsIWebProgressListener::STATE_LOADED_CRYPTOMINING_CONTENT);
  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierFeatureCryptominingAnnotation::GetURIByListType(
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
