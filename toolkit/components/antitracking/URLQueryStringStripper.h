/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_URLQueryStringStripper_h
#define mozilla_URLQueryStringStripper_h

#include "nsIURLQueryStringStripper.h"
#include "nsIURLQueryStrippingListService.h"
#include "nsIObserver.h"
#include "mozilla/dom/StripOnShareRuleBinding.h"
#include "nsStringFwd.h"
#include "nsTHashSet.h"
#include "nsTHashMap.h"

class nsIURI;

namespace mozilla {

class URLQueryStringStripper final : public nsIObserver,
                                     public nsIURLQueryStringStripper,
                                     public nsIURLQueryStrippingListObserver {
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIURLQUERYSTRIPPINGLISTOBSERVER

  NS_DECL_NSIURLQUERYSTRINGSTRIPPER

 public:
  static already_AddRefed<URLQueryStringStripper> GetSingleton();

 private:
  URLQueryStringStripper();
  ~URLQueryStringStripper() = default;

  static void OnPrefChange(const char* aPref, void* aData);
  nsresult ManageObservers();

  [[nodiscard]] nsresult Init();
  [[nodiscard]] nsresult Shutdown();

  [[nodiscard]] nsresult StripQueryString(nsIURI* aURI, nsIURI** aOutput,
                                          uint32_t* aStripCount);

  bool CheckAllowList(nsIURI* aURI);

  void PopulateStripList(const nsAString& aList);
  void PopulateAllowList(const nsACString& aList);

  nsTHashSet<nsString> mList;
  nsTHashSet<nsCString> mAllowList;
  nsCOMPtr<nsIURLQueryStrippingListService> mListService;
  nsTHashMap<nsCString, dom::StripRule> mStripOnShareMap;
  bool mIsInitialized;
  // Indicates whether or not we currently have registered an observer
  // for the QPS/strip-on-share list updates
  bool mObservingQPS = false;
  bool mObservingStripOnShare = false;
};

}  // namespace mozilla

#endif  // mozilla_URLQueryStringStripper_h
