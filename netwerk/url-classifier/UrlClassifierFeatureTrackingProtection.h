/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_UrlClassifierFeatureTrackingProtection_h
#define mozilla_net_UrlClassifierFeatureTrackingProtection_h

#include "UrlClassifierFeatureBase.h"

class nsIChannel;

namespace mozilla {
namespace net {

class UrlClassifierFeatureTrackingProtection final
    : public UrlClassifierFeatureBase {
 public:
  static void Initialize();

  static void Shutdown();

  static already_AddRefed<UrlClassifierFeatureTrackingProtection> MaybeCreate(
      nsIChannel* aChannel);

  NS_IMETHOD ProcessChannel(nsIChannel* aChannel, const nsACString& aList,
                            bool* aShouldContinue) override;

 private:
  UrlClassifierFeatureTrackingProtection();
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_UrlClassifierFeatureTrackingProtection_h
