/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_UrlClassifierFeatureCustomTables_h
#define mozilla_UrlClassifierFeatureCustomTables_h

#include "nsIUrlClassifierFeature.h"
#include "nsTArray.h"
#include "nsString.h"

namespace mozilla {

class UrlClassifierFeatureCustomTables : public nsIUrlClassifierFeature {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIERFEATURE

  explicit UrlClassifierFeatureCustomTables(
      const nsACString& aName, const nsTArray<nsCString>& aBlocklistTables,
      const nsTArray<nsCString>& aEntitylistTables);

 private:
  virtual ~UrlClassifierFeatureCustomTables();

  nsCString mName;
  nsTArray<nsCString> mBlocklistTables;
  nsTArray<nsCString> mEntitylistTables;
};

}  // namespace mozilla

#endif  // mozilla_UrlClassifierFeatureCustomTables_h
