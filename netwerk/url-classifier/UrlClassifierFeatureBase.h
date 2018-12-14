/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_UrlClassifierFeatureBase_h
#define mozilla_net_UrlClassifierFeatureBase_h

#include "nsIUrlClassifierFeature.h"
#include "nsTArray.h"
#include "nsString.h"

namespace mozilla {
namespace net {

class UrlClassifierFeatureBase : public nsIUrlClassifierFeature {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIERFEATURE

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
