/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_UrlClassifierFeatureNoChannel_h
#define mozilla_UrlClassifierFeatureNoChannel_h

#include "UrlClassifierFeatureBase.h"

namespace mozilla {
namespace net {

class UrlClassifierFeatureNoChannel final : public UrlClassifierFeatureBase {
 public:
  struct NoChannelFeature;

  static void GetFeatureNames(nsTArray<nsCString>& aNames);

  static void MaybeShutdown();

  static already_AddRefed<nsIUrlClassifierFeature> GetIfNameMatches(
      const nsACString& aName);

  NS_IMETHOD
  ProcessChannel(nsIChannel* aChannel, const nsACString& aList,
                 bool* aShouldContinue) override;

  NS_IMETHOD GetURIByListType(nsIChannel* aChannel,
                              nsIUrlClassifierFeature::listType aListType,
                              nsIURI** aURI) override;

 private:
  explicit UrlClassifierFeatureNoChannel(const NoChannelFeature& aFeature);

  static void MaybeInitialize();
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_UrlClassifierFeatureNoChannel_h
