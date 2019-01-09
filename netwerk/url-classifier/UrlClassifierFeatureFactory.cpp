/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/UrlClassifierFeatureFactory.h"

// List of Features
#include "UrlClassifierFeatureCryptomining.h"
#include "UrlClassifierFeatureFingerprinting.h"
#include "UrlClassifierFeatureFlash.h"
#include "UrlClassifierFeatureLoginReputation.h"
#include "UrlClassifierFeatureTrackingProtection.h"
#include "UrlClassifierFeatureTrackingAnnotation.h"
#include "UrlClassifierFeatureCustomTables.h"

#include "nsAppRunner.h"

namespace mozilla {
namespace net {

/* static */ void UrlClassifierFeatureFactory::Shutdown() {
  // We want to expose Features only in the parent process.
  if (!XRE_IsParentProcess()) {
    return;
  }

  UrlClassifierFeatureCryptomining::MaybeShutdown();
  UrlClassifierFeatureFingerprinting::MaybeShutdown();
  UrlClassifierFeatureFlash::MaybeShutdown();
  UrlClassifierFeatureLoginReputation::MaybeShutdown();
  UrlClassifierFeatureTrackingAnnotation::MaybeShutdown();
  UrlClassifierFeatureTrackingProtection::MaybeShutdown();
}

/* static */ void UrlClassifierFeatureFactory::GetFeaturesFromChannel(
    nsIChannel* aChannel,
    nsTArray<nsCOMPtr<nsIUrlClassifierFeature>>& aFeatures) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(aChannel);

  nsCOMPtr<nsIUrlClassifierFeature> feature;

  // Note that the order of the features is extremely important! When more than
  // 1 feature classifies the channel, we call ::ProcessChannel() following this
  // feature order, and this could produce different results with a different
  // feature ordering.

  // Cryptomining
  feature = UrlClassifierFeatureCryptomining::MaybeCreate(aChannel);
  if (feature) {
    aFeatures.AppendElement(feature);
  }

  // Fingerprinting
  feature = UrlClassifierFeatureFingerprinting::MaybeCreate(aChannel);
  if (feature) {
    aFeatures.AppendElement(feature);
  }

  // Tracking Protection
  feature = UrlClassifierFeatureTrackingProtection::MaybeCreate(aChannel);
  if (feature) {
    aFeatures.AppendElement(feature);
  }

  // Tracking Annotation
  feature = UrlClassifierFeatureTrackingAnnotation::MaybeCreate(aChannel);
  if (feature) {
    aFeatures.AppendElement(feature);
  }

  // Flash
  nsTArray<nsCOMPtr<nsIUrlClassifierFeature>> flashFeatures;
  UrlClassifierFeatureFlash::MaybeCreate(aChannel, flashFeatures);
  aFeatures.AppendElements(flashFeatures);
}

/* static */
nsIUrlClassifierFeature*
UrlClassifierFeatureFactory::GetFeatureLoginReputation() {
  return UrlClassifierFeatureLoginReputation::MaybeGetOrCreate();
}

/* static */ already_AddRefed<nsIUrlClassifierFeature>
UrlClassifierFeatureFactory::GetFeatureByName(const nsACString& aName) {
  if (!XRE_IsParentProcess()) {
    return nullptr;
  }

  nsCOMPtr<nsIUrlClassifierFeature> feature;

  // Cryptomining
  feature = UrlClassifierFeatureCryptomining::GetIfNameMatches(aName);
  if (feature) {
    return feature.forget();
  }

  // Fingerprinting
  feature = UrlClassifierFeatureFingerprinting::GetIfNameMatches(aName);
  if (feature) {
    return feature.forget();
  }

  // Tracking Protection
  feature = UrlClassifierFeatureTrackingProtection::GetIfNameMatches(aName);
  if (feature) {
    return feature.forget();
  }

  // Tracking Annotation
  feature = UrlClassifierFeatureTrackingAnnotation::GetIfNameMatches(aName);
  if (feature) {
    return feature.forget();
  }

  // Login reputation
  feature = UrlClassifierFeatureLoginReputation::GetIfNameMatches(aName);
  if (feature) {
    return feature.forget();
  }

  // We use Flash feature just for document loading.
  feature = UrlClassifierFeatureFlash::GetIfNameMatches(aName);
  if (feature) {
    return feature.forget();
  }

  return nullptr;
}

/* static */ void UrlClassifierFeatureFactory::GetFeatureNames(
    nsTArray<nsCString>& aArray) {
  if (!XRE_IsParentProcess()) {
    return;
  }

  // Cryptomining
  nsAutoCString name;
  name.Assign(UrlClassifierFeatureCryptomining::Name());
  if (!name.IsEmpty()) {
    aArray.AppendElement(name);
  }

  // Fingerprinting
  name.Assign(UrlClassifierFeatureFingerprinting::Name());
  if (!name.IsEmpty()) {
    aArray.AppendElement(name);
  }

  // Tracking Protection
  name.Assign(UrlClassifierFeatureTrackingProtection::Name());
  if (!name.IsEmpty()) {
    aArray.AppendElement(name);
  }

  // Tracking Annotation
  name.Assign(UrlClassifierFeatureTrackingAnnotation::Name());
  if (!name.IsEmpty()) {
    aArray.AppendElement(name);
  }

  // Login reputation
  name.Assign(UrlClassifierFeatureLoginReputation::Name());
  if (!name.IsEmpty()) {
    aArray.AppendElement(name);
  }

  // Flash features
  nsTArray<nsCString> features;
  UrlClassifierFeatureFlash::GetFeatureNames(features);
  aArray.AppendElements(features);
}

/* static */ already_AddRefed<nsIUrlClassifierFeature>
UrlClassifierFeatureFactory::CreateFeatureWithTables(
    const nsACString& aName, const nsTArray<nsCString>& aBlacklistTables,
    const nsTArray<nsCString>& aWhitelistTables) {
  nsCOMPtr<nsIUrlClassifierFeature> feature =
      new UrlClassifierFeatureCustomTables(aName, aBlacklistTables,
                                           aWhitelistTables);
  return feature.forget();
}

}  // namespace net
}  // namespace mozilla
