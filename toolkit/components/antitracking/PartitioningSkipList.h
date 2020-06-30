/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_PartitioningSkipList_h
#define mozilla_PartitioningSkipList_h

#include "nsIPartitioningSkipListService.h"
#include "nsTArrayForwardDeclare.h"

class nsIChannel;
class nsIPrincipal;

namespace mozilla {

class PartitioningSkipList : public nsIPartitioningSkipListObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPARTITIONINGSKIPLISTOBSERVER

  static bool Check(const nsACString& aFirstPartyOrigin,
                    const nsACString& aThirdPartyOrigin);

 private:
  static PartitioningSkipList* GetOrCreate();

  PartitioningSkipList() = default;
  virtual ~PartitioningSkipList() = default;

  nsresult Init();
  void Shutdown();

  nsCOMPtr<nsIPartitioningSkipListService> mService;
  nsTArray<nsCString> mSkipList;
};

}  // namespace mozilla

#endif  // mozilla_PartitioningSkipList_h
