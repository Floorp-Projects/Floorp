/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_UrlClassifierFeatureBase_h
#define mozilla_net_UrlClassifierFeatureBase_h

#include "nsIUrlClassifierFeature.h"
#include "nsIUrlClassifierSkipListService.h"
#include "nsTArray.h"
#include "nsString.h"
#include "mozilla/AntiTrackingCommon.h"

namespace mozilla {
namespace net {

class UrlClassifierFeatureBase : public nsIUrlClassifierFeature,
                                 public nsIUrlClassifierSkipListObserver {
 public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD
  GetName(nsACString& aName) override;

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

  NS_IMETHOD
  GetSkipHostList(nsACString& aList) override;

  NS_IMETHOD
  OnSkipListUpdate(const nsACString& aList) override;

 protected:
  UrlClassifierFeatureBase(const nsACString& aName,
                           const nsACString& aPrefBlacklistTables,
                           const nsACString& aPrefWhitelistTables,
                           const nsACString& aPrefBlacklistHosts,
                           const nsACString& aPrefWhitelistHosts,
                           const nsACString& aPrefBlacklistTableName,
                           const nsACString& aPrefWhitelistTableName,
                           const nsACString& aPrefSkipHosts);

  virtual ~UrlClassifierFeatureBase();

  void InitializePreferences();
  void ShutdownPreferences();

 private:
  nsCString mName;

  nsCString mPrefSkipHosts;

  // 2: blacklist and whitelist.
  nsCString mPrefTables[2];
  nsTArray<nsCString> mTables[2];

  nsCString mPrefHosts[2];
  nsCString mPrefTableNames[2];
  nsTArray<nsCString> mHosts[2];

  nsCString mSkipHosts;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_UrlClassifierFeatureBase_h
