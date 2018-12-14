/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UrlClassifierFeatureTrackingAnnotation.h"

#include "mozilla/AntiTrackingCommon.h"
#include "mozilla/Logging.h"
#include "mozilla/StaticPrefs.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/net/UrlClassifierCommon.h"

namespace mozilla {
namespace net {

namespace {

#define URLCLASSIFIER_ANNOTATION_BLACKLIST \
  "urlclassifier.trackingAnnotationTable"
#define URLCLASSIFIER_ANNOTATION_BLACKLIST_TEST_ENTRIES \
  "urlclassifier.trackingAnnotationTable.testEntries"
#define URLCLASSIFIER_ANNOTATION_WHITELIST \
  "urlclassifier.trackingAnnotationWhitelistTable"
#define URLCLASSIFIER_ANNOTATION_WHITELIST_TEST_ENTRIES \
  "urlclassifier.trackingAnnotationWhitelistTable.testEntries"
#define TABLE_ANNOTATION_BLACKLIST_PREF "annotation-blacklist-pref"
#define TABLE_ANNOTATION_WHITELIST_PREF "annotation-whitelist-pref"

StaticRefPtr<UrlClassifierFeatureTrackingAnnotation> gFeatureTrackingAnnotation;

}  // namespace

UrlClassifierFeatureTrackingAnnotation::UrlClassifierFeatureTrackingAnnotation()
    : UrlClassifierFeatureBase(
          NS_LITERAL_CSTRING("tracking-annotation"),
          NS_LITERAL_CSTRING(URLCLASSIFIER_ANNOTATION_BLACKLIST),
          NS_LITERAL_CSTRING(URLCLASSIFIER_ANNOTATION_WHITELIST),
          NS_LITERAL_CSTRING(URLCLASSIFIER_ANNOTATION_BLACKLIST_TEST_ENTRIES),
          NS_LITERAL_CSTRING(URLCLASSIFIER_ANNOTATION_WHITELIST_TEST_ENTRIES),
          NS_LITERAL_CSTRING(TABLE_ANNOTATION_BLACKLIST_PREF),
          NS_LITERAL_CSTRING(TABLE_ANNOTATION_WHITELIST_PREF), EmptyCString()) {
}

/* static */ void UrlClassifierFeatureTrackingAnnotation::Initialize() {
  UC_LOG(("UrlClassifierFeatureTrackingAnnotation: Initializing"));
  MOZ_ASSERT(!gFeatureTrackingAnnotation);

  gFeatureTrackingAnnotation = new UrlClassifierFeatureTrackingAnnotation();
  gFeatureTrackingAnnotation->InitializePreferences();
}

/* static */ void UrlClassifierFeatureTrackingAnnotation::Shutdown() {
  UC_LOG(("UrlClassifierFeatureTrackingAnnotation: Shutdown"));
  MOZ_ASSERT(gFeatureTrackingAnnotation);

  gFeatureTrackingAnnotation->ShutdownPreferences();
  gFeatureTrackingAnnotation = nullptr;
}

/* static */ already_AddRefed<UrlClassifierFeatureTrackingAnnotation>
UrlClassifierFeatureTrackingAnnotation::MaybeCreate(nsIChannel* aChannel) {
  MOZ_ASSERT(gFeatureTrackingAnnotation);
  MOZ_ASSERT(aChannel);

  UC_LOG(("UrlClassifierFeatureTrackingAnnotation: MaybeCreate for channel %p",
          aChannel));

  if (!StaticPrefs::privacy_trackingprotection_annotate_channels()) {
    return nullptr;
  }

  if (!UrlClassifierCommon::ShouldEnableTrackingProtectionOrAnnotation(
          aChannel, AntiTrackingCommon::eTrackingAnnotations)) {
    return nullptr;
  }

  RefPtr<UrlClassifierFeatureTrackingAnnotation> self =
      gFeatureTrackingAnnotation;
  return self.forget();
}

}  // namespace net
}  // namespace mozilla
