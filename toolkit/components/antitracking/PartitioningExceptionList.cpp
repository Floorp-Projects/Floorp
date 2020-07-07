/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PartitioningExceptionList.h"

#include "AntiTrackingLog.h"
#include "nsContentUtils.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticPtr.h"

namespace mozilla {

namespace {

inline void CreateExceptionListKey(const nsACString& aFirstPartyOrigin,
                                   const nsACString& aThirdPartyOrigin,
                                   nsACString& aExceptionListKey) {
  MOZ_ASSERT(!aFirstPartyOrigin.IsEmpty());
  MOZ_ASSERT(!aThirdPartyOrigin.IsEmpty());

  aExceptionListKey.Assign(aFirstPartyOrigin);
  aExceptionListKey.Append(",");
  aExceptionListKey.Append(aThirdPartyOrigin);
}

inline void CreateExceptionListKey(nsIURI* aFirstPartyURI,
                                   nsIURI* aThirdPartyURI,
                                   nsACString& aExceptionListKey) {
  MOZ_ASSERT(aFirstPartyURI);
  MOZ_ASSERT(aThirdPartyURI);

  nsAutoCString firstPartyOrigin, thirdPartyOrigin;
  nsContentUtils::GetASCIIOrigin(aFirstPartyURI, firstPartyOrigin);
  nsContentUtils::GetASCIIOrigin(aThirdPartyURI, thirdPartyOrigin);

  CreateExceptionListKey(firstPartyOrigin, thirdPartyOrigin, aExceptionListKey);
}

StaticRefPtr<PartitioningExceptionList> gPartitioningExceptionList;

}  // namespace

NS_IMPL_ISUPPORTS(PartitioningExceptionList,
                  nsIPartitioningExceptionListObserver)

bool PartitioningExceptionList::Check(const nsACString& aFirstPartyOrigin,
                                      const nsACString& aThirdPartyOrigin) {
  if (aFirstPartyOrigin.IsEmpty() || aThirdPartyOrigin.IsEmpty()) {
    return false;
  }

  LOG(("Check partitioning exception list for url %s and %s",
       PromiseFlatCString(aFirstPartyOrigin).get(),
       PromiseFlatCString(aThirdPartyOrigin).get()));

  nsAutoCString key;
  CreateExceptionListKey(aFirstPartyOrigin, aThirdPartyOrigin, key);

  if (GetOrCreate()->mExceptionList.Contains(key)) {
    LOG(("URI is in exception list"));
    return true;
  }

  return false;
}

PartitioningExceptionList* PartitioningExceptionList::GetOrCreate() {
  if (!gPartitioningExceptionList) {
    gPartitioningExceptionList = new PartitioningExceptionList();
    gPartitioningExceptionList->Init();

    RunOnShutdown([&] {
      gPartitioningExceptionList->Shutdown();
      gPartitioningExceptionList = nullptr;
    });
  }

  return gPartitioningExceptionList;
}

nsresult PartitioningExceptionList::Init() {
  mService =
      do_GetService("@mozilla.org/partitioning/exception-list-service;1");
  if (NS_WARN_IF(!mService)) {
    return NS_ERROR_FAILURE;
  }

  mService->RegisterAndRunExceptionListObserver(this);
  return NS_OK;
}

void PartitioningExceptionList::Shutdown() {
  if (mService) {
    mService->UnregisterExceptionListObserver(this);
    mService = nullptr;
  }

  mExceptionList.Clear();
}

NS_IMETHODIMP
PartitioningExceptionList::OnExceptionListUpdate(const nsACString& aList) {
  mExceptionList.Clear();

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
    CreateExceptionListKey(firstPartyURI, thirdPartyURI, key);
    LOG(("onExceptionListUpdate: %s", key.get()));

    mExceptionList.AppendElement(key);
  }

  return NS_OK;
}

}  // namespace mozilla
