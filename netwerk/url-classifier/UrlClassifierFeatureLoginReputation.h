/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_UrlClassifierFeatureLoginReputation_h
#define mozilla_net_UrlClassifierFeatureLoginReputation_h

#include "UrlClassifierFeatureBase.h"

class nsIChannel;

namespace mozilla {
namespace net {

class UrlClassifierFeatureLoginReputation final
    : public UrlClassifierFeatureBase {
 public:
  static void MaybeShutdown();

  static nsIUrlClassifierFeature* MaybeGetOrCreate();

  NS_IMETHOD
  GetTables(nsIUrlClassifierFeature::listType aListType,
            nsTArray<nsCString>& aResult) override;

  NS_IMETHOD
  HasTable(const nsACString& aTable,
           nsIUrlClassifierFeature::listType aListType, bool* aResult) override;

  NS_IMETHOD
  HasHostInPreferences(const nsACString& aHost,
                       nsIUrlClassifierFeature::listType aListType,
                       nsACString& aPrefTableName, bool* aResult) override;

  NS_IMETHOD ProcessChannel(nsIChannel* aChannel, const nsACString& aList,
                            bool* aShouldContinue) override;

  NS_IMETHOD GetURIByListType(nsIChannel* aChannel,
                              nsIUrlClassifierFeature::listType aListType,
                              nsIURI** aURI) override;

 private:
  UrlClassifierFeatureLoginReputation();
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_UrlClassifierFeatureLoginReputation_h
