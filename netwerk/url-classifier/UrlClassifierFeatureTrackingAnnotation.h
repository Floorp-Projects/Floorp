/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_UrlClassifierFeatureTrackingAnnotation_h
#define mozilla_net_UrlClassifierFeatureTrackingAnnotation_h

#include "UrlClassifierFeatureBase.h"

class nsIChannel;

namespace mozilla {
namespace net {

class UrlClassifierFeatureTrackingAnnotation final
    : public UrlClassifierFeatureAntiTrackingBase {
 public:
  static const char* Name();

  static void MaybeShutdown();

  static already_AddRefed<UrlClassifierFeatureTrackingAnnotation> MaybeCreate(
      nsIChannel* aChannel);

  static already_AddRefed<nsIUrlClassifierFeature> GetIfNameMatches(
      const nsACString& aName);

  NS_IMETHOD ProcessChannel(nsIChannel* aChannel,
                            const nsTArray<nsCString>& aList,
                            const nsTArray<nsCString>& aHashes,
                            bool* aShouldContinue) override;

  NS_IMETHOD GetURIByListType(nsIChannel* aChannel,
                              nsIUrlClassifierFeature::listType aListType,
                              nsIUrlClassifierFeature::URIType* aURIType,
                              nsIURI** aURI) override;

 private:
  UrlClassifierFeatureTrackingAnnotation();

  static void MaybeInitialize();
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_UrlClassifierFeatureTrackingAnnotation_h
