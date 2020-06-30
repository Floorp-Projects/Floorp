/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PartitioningSkipList.h"

#include "AntiTrackingLog.h"
#include "nsContentUtils.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticPtr.h"

namespace mozilla {

namespace {

inline void CreateSkipListKey(const nsACString& aFirstPartyOrigin,
                              const nsACString& aThirdPartyOrigin,
                              nsACString& aSkipListKey) {
  MOZ_ASSERT(!aFirstPartyOrigin.IsEmpty());
  MOZ_ASSERT(!aThirdPartyOrigin.IsEmpty());

  aSkipListKey.Assign(aFirstPartyOrigin);
  aSkipListKey.Append(",");
  aSkipListKey.Append(aThirdPartyOrigin);
}

inline void CreateSkipListKey(nsIURI* aFirstPartyURI, nsIURI* aThirdPartyURI,
                              nsACString& aSkipListKey) {
  MOZ_ASSERT(aFirstPartyURI);
  MOZ_ASSERT(aThirdPartyURI);

  nsAutoCString firstPartyOrigin, thirdPartyOrigin;
  nsContentUtils::GetASCIIOrigin(aFirstPartyURI, firstPartyOrigin);
  nsContentUtils::GetASCIIOrigin(aThirdPartyURI, thirdPartyOrigin);

  CreateSkipListKey(firstPartyOrigin, thirdPartyOrigin, aSkipListKey);
}

StaticRefPtr<PartitioningSkipList> gPartitioningSkipList;

}  // namespace

NS_IMPL_ISUPPORTS(PartitioningSkipList, nsIPartitioningSkipListObserver)

bool PartitioningSkipList::Check(const nsACString& aFirstPartyOrigin,
                                 const nsACString& aThirdPartyOrigin) {
  if (aFirstPartyOrigin.IsEmpty() || aThirdPartyOrigin.IsEmpty()) {
    return false;
  }

  LOG(("Check partitioning skip list for url %s and %s",
       PromiseFlatCString(aFirstPartyOrigin).get(),
       PromiseFlatCString(aThirdPartyOrigin).get()));

  nsAutoCString key;
  CreateSkipListKey(aFirstPartyOrigin, aThirdPartyOrigin, key);

  if (GetOrCreate()->mSkipList.Contains(key)) {
    LOG(("URI is in skip list"));
    return true;
  }

  return false;
}

PartitioningSkipList* PartitioningSkipList::GetOrCreate() {
  if (!gPartitioningSkipList) {
    gPartitioningSkipList = new PartitioningSkipList();
    gPartitioningSkipList->Init();

    RunOnShutdown([&] {
      gPartitioningSkipList->Shutdown();
      gPartitioningSkipList = nullptr;
    });
  }

  return gPartitioningSkipList;
}

nsresult PartitioningSkipList::Init() {
  mService = do_GetService("@mozilla.org/partitioning/skip-list-service;1");
  if (NS_WARN_IF(!mService)) {
    return NS_ERROR_FAILURE;
  }

  mService->RegisterAndRunSkipListObserver(this);
  return NS_OK;
}

void PartitioningSkipList::Shutdown() {
  if (mService) {
    mService->UnregisterSkipListObserver(this);
    mService = nullptr;
  }

  mSkipList.Clear();
}

NS_IMETHODIMP
PartitioningSkipList::OnSkipListUpdate(const nsACString& aList) {
  mSkipList.Clear();

  nsresult rv;
  for (const nsACString& item : aList.Split(';')) {
    auto origins = item.Split(',');

    nsCOMPtr<nsIURI> firstPartyURI;
    rv = NS_NewURI(getter_AddRefs(firstPartyURI), origins.Get(0));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    nsCOMPtr<nsIURI> thirdPartyURI;
    rv = NS_NewURI(getter_AddRefs(thirdPartyURI), origins.Get(1));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsAutoCString key;
    CreateSkipListKey(firstPartyURI, thirdPartyURI, key);
    LOG(("onSkipListUpdate: %s", key.get()));

    mSkipList.AppendElement(key);
  }

  return NS_OK;
}

}  // namespace mozilla
