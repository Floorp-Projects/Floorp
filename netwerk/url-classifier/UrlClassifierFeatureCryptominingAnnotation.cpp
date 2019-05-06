/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UrlClassifierFeatureCryptominingAnnotation.h"

#include "mozilla/AntiTrackingCommon.h"
#include "mozilla/net/UrlClassifierCommon.h"
#include "mozilla/StaticPrefs.h"
#include "nsContentUtils.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace net {

namespace {

#define CRYPTOMINING_ANNOTATION_FEATURE_NAME "cryptomining-annotation"

#define URLCLASSIFIER_CRYPTOMINING_ANNOTATION_BLACKLIST \
  "urlclassifier.features.cryptomining.annotate.blacklistTables"
#define URLCLASSIFIER_CRYPTOMINING_ANNOTATION_BLACKLIST_TEST_ENTRIES \
  "urlclassifier.features.cryptomining.annotate.blacklistHosts"
#define URLCLASSIFIER_CRYPTOMINING_ANNOTATION_WHITELIST \
  "urlclassifier.features.cryptomining.annotate.whitelistTables"
#define URLCLASSIFIER_CRYPTOMINING_ANNOTATION_WHITELIST_TEST_ENTRIES \
  "urlclassifier.features.cryptomining.annotate.whitelistHosts"
#define URLCLASSIFIER_CRYPTOMINING_ANNOTATION_SKIP_URLS \
  "urlclassifier.features.cryptomining.annotate.skipURLs"
#define TABLE_CRYPTOMINING_ANNOTATION_BLACKLIST_PREF \
  "cryptomining-annotate-blacklist-pref"
#define TABLE_CRYPTOMINING_ANNOTATION_WHITELIST_PREF \
  "cryptomining-annotate-whitelist-pref"

StaticRefPtr<UrlClassifierFeatureCryptominingAnnotation>
    gFeatureCryptominingAnnotation;

}  // namespace

UrlClassifierFeatureCryptominingAnnotation::
    UrlClassifierFeatureCryptominingAnnotation()
    : UrlClassifierFeatureBase(
          NS_LITERAL_CSTRING(CRYPTOMINING_ANNOTATION_FEATURE_NAME),
          NS_LITERAL_CSTRING(URLCLASSIFIER_CRYPTOMINING_ANNOTATION_BLACKLIST),
          NS_LITERAL_CSTRING(URLCLASSIFIER_CRYPTOMINING_ANNOTATION_WHITELIST),
          NS_LITERAL_CSTRING(
              URLCLASSIFIER_CRYPTOMINING_ANNOTATION_BLACKLIST_TEST_ENTRIES),
          NS_LITERAL_CSTRING(
              URLCLASSIFIER_CRYPTOMINING_ANNOTATION_WHITELIST_TEST_ENTRIES),
          NS_LITERAL_CSTRING(TABLE_CRYPTOMINING_ANNOTATION_BLACKLIST_PREF),
          NS_LITERAL_CSTRING(TABLE_CRYPTOMINING_ANNOTATION_WHITELIST_PREF),
          NS_LITERAL_CSTRING(URLCLASSIFIER_CRYPTOMINING_ANNOTATION_SKIP_URLS)) {
}

/* static */ const char* UrlClassifierFeatureCryptominingAnnotation::Name() {
  return CRYPTOMINING_ANNOTATION_FEATURE_NAME;
}

/* static */
void UrlClassifierFeatureCryptominingAnnotation::MaybeInitialize() {
  UC_LOG(("UrlClassifierFeatureCryptominingAnnotation: MaybeInitialize"));

  if (!gFeatureCryptominingAnnotation) {
    gFeatureCryptominingAnnotation =
        new UrlClassifierFeatureCryptominingAnnotation();
    gFeatureCryptominingAnnotation->InitializePreferences();
  }
}

/* static */
void UrlClassifierFeatureCryptominingAnnotation::MaybeShutdown() {
  UC_LOG(("UrlClassifierFeatureCryptominingAnnotation: MaybeShutdown"));

  if (gFeatureCryptominingAnnotation) {
    gFeatureCryptominingAnnotation->ShutdownPreferences();
    gFeatureCryptominingAnnotation = nullptr;
  }
}

/* static */
already_AddRefed<UrlClassifierFeatureCryptominingAnnotation>
UrlClassifierFeatureCryptominingAnnotation::MaybeCreate(nsIChannel* aChannel) {
  MOZ_ASSERT(aChannel);

  UC_LOG(
      ("UrlClassifierFeatureCryptominingAnnotation: MaybeCreate for channel %p",
       aChannel));

  if (!StaticPrefs::
          privacy_trackingprotection_cryptomining_annotate_enabled()) {
    return nullptr;
  }

  if (!UrlClassifierCommon::ShouldEnableClassifier(aChannel)) {
    return nullptr;
  }

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
      ("UrlClassifierFeatureCryptominingAnnotation::ProcessChannel, annotating "
       "channel[%p]",
       aChannel));

  static std::vector<UrlClassifierCommon::ClassificationData>
      sClassificationData = {
          {NS_LITERAL_CSTRING("content-cryptomining-track-"),
           nsIHttpChannel::ClassificationFlags::
               CLASSIFIED_CRYPTOMINING_CONTENT},
      };

  uint32_t flags = UrlClassifierCommon::TablesToClassificationFlags(
      aList, sClassificationData,
      nsIHttpChannel::ClassificationFlags::CLASSIFIED_CRYPTOMINING);

  UrlClassifierCommon::SetTrackingInfo(aChannel, aList, aHashes);

  UrlClassifierCommon::AnnotateChannel(
      aChannel, AntiTrackingCommon::eCryptomining, flags,
      nsIWebProgressListener::STATE_LOADED_CRYPTOMINING_CONTENT);
  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierFeatureCryptominingAnnotation::GetURIByListType(
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
