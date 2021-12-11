/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_UrlClassifierFeatureBase_h
#define mozilla_net_UrlClassifierFeatureBase_h

#include "nsIUrlClassifierFeature.h"
#include "nsIUrlClassifierExceptionListService.h"
#include "nsTArray.h"
#include "nsString.h"

namespace mozilla {
namespace net {

class UrlClassifierFeatureBase : public nsIUrlClassifierFeature,
                                 public nsIUrlClassifierExceptionListObserver {
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
  GetExceptionHostList(nsACString& aList) override;

  NS_IMETHOD
  OnExceptionListUpdate(const nsACString& aList) override;

 protected:
  UrlClassifierFeatureBase(const nsACString& aName,
                           const nsACString& aPrefBlocklistTables,
                           const nsACString& aPrefEntitylistTables,
                           const nsACString& aPrefBlocklistHosts,
                           const nsACString& aPrefEntitylistHosts,
                           const nsACString& aPrefBlocklistTableName,
                           const nsACString& aPrefEntitylistTableName,
                           const nsACString& aPrefExceptionHosts);

  virtual ~UrlClassifierFeatureBase();

  void InitializePreferences();
  void ShutdownPreferences();

  nsCString mName;

 private:
  nsCString mPrefExceptionHosts;

  // 2: blocklist and entitylist.
  nsCString mPrefTables[2];
  nsTArray<nsCString> mTables[2];

  nsCString mPrefHosts[2];
  nsCString mPrefTableNames[2];
  nsTArray<nsCString> mHosts[2];

  nsCString mExceptionHosts;
};

class UrlClassifierFeatureAntiTrackingBase : public UrlClassifierFeatureBase {
  using UrlClassifierFeatureBase::UrlClassifierFeatureBase;

  NS_IMETHOD
  GetExceptionHostList(nsACString& aList) override;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_UrlClassifierFeatureBase_h
