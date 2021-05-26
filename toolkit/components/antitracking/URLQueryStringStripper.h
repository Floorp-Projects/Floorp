/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_URLQueryStringStripper_h
#define mozilla_URLQueryStringStripper_h

#include "nsIObserver.h"

#include "nsStringFwd.h"
#include "nsTHashSet.h"

class nsIURI;

namespace mozilla {

// URLQueryStringStripper is responsible for stripping certain part of the query
// string of the given URI to address the bounce(redirect) tracking issues. It
// will strip every query parameter which matches the strip list defined in
// the pref 'privacy.query_stripping.strip_list'. Note that It's different from
// URLDecorationStripper which strips the entire query string from the referrer
// if there is a tracking query parameter present in the URI.
//
// TODO: Given that URLQueryStringStripper and URLDecorationStripper are doing
//       similar things. We could somehow combine these two modules into one.
//       We will improve this in the future.

class URLQueryStringStripper final : public nsIObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  // Strip the query parameters that are in the strip list. Return true if there
  // is any query parameter has been stripped. Otherwise, false.
  static bool Strip(nsIURI* aURI, nsCOMPtr<nsIURI>& aOutput);

 private:
  URLQueryStringStripper() = default;

  ~URLQueryStringStripper() = default;

  static URLQueryStringStripper* GetOrCreate();

  void Init();
  void Shutdown();

  bool StripQueryString(nsIURI* aURI, nsCOMPtr<nsIURI>& aOutput);

  bool CheckAllowList(nsIURI* aURI);

  void PopulateStripList();
  void PopulateAllowList();

  nsTHashSet<nsString> mList;
  nsTHashSet<nsCString> mAllowList;
};

}  // namespace mozilla

#endif  // mozilla_URLQueryStringStripper_h
