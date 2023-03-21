/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_PartitioningExceptionList_h
#define mozilla_PartitioningExceptionList_h

#include "nsCOMPtr.h"
#include "nsIPartitioningExceptionListService.h"
#include "nsTArray.h"
#include "nsString.h"

class nsIChannel;
class nsIPrincipal;

namespace mozilla {

class PartitioningExceptionList : public nsIPartitioningExceptionListObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPARTITIONINGEXCEPTIONLISTOBSERVER

  static bool Check(const nsACString& aFirstPartyOrigin,
                    const nsACString& aThirdPartyOrigin);

 private:
  static PartitioningExceptionList* GetOrCreate();

  PartitioningExceptionList() = default;
  virtual ~PartitioningExceptionList() = default;

  nsresult Init();
  void Shutdown();

  struct PartitionExceptionListPattern {
    nsCString mScheme;
    nsCString mSuffix;
    bool mIsWildCard = false;
  };

  struct PartitionExceptionListEntry {
    PartitionExceptionListPattern mFirstParty;
    PartitionExceptionListPattern mThirdParty;
  };

  static nsresult GetSchemeFromOrigin(const nsACString& aOrigin,
                                      nsACString& aScheme,
                                      nsACString& aOriginNoScheme);

  static bool OriginMatchesPattern(
      const nsACString& aOrigin, const PartitionExceptionListPattern& aPattern);

  static nsresult GetExceptionListPattern(
      const nsACString& aOriginPattern,
      PartitionExceptionListPattern& aPattern);

  nsCOMPtr<nsIPartitioningExceptionListService> mService;
  nsTArray<PartitionExceptionListEntry> mExceptionList;
};

}  // namespace mozilla

#endif  // mozilla_PartitioningExceptionList_h
