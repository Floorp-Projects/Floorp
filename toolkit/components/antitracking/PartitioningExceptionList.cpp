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

inline nsresult CreateExceptionListKey(const nsACString& aFirstPartyOrigin,
                                       const nsACString& aThirdPartyOrigin,
                                       nsACString& aExceptionListKey) {
  MOZ_ASSERT(!aFirstPartyOrigin.IsEmpty());
  MOZ_ASSERT(!aThirdPartyOrigin.IsEmpty());

  // Don't allow exceptions for everything
  if (aFirstPartyOrigin.EqualsLiteral("*") &&
      aThirdPartyOrigin.EqualsLiteral("*")) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  aExceptionListKey.Assign(aFirstPartyOrigin);
  aExceptionListKey.Append(",");
  aExceptionListKey.Append(aThirdPartyOrigin);

  return NS_OK;
}

inline nsresult CreateUnifiedOriginString(const nsACString& aInput,
                                          nsACString& aOutput) {
  nsCOMPtr<nsIURI> uri;
  if (aInput.EqualsLiteral("*")) {
    aOutput.AssignLiteral("*");
    return NS_OK;
  }
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aInput);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return nsContentUtils::GetASCIIOrigin(uri, aOutput);
}

StaticRefPtr<PartitioningExceptionList> gPartitioningExceptionList;

}  // namespace

NS_IMPL_ISUPPORTS(PartitioningExceptionList,
                  nsIPartitioningExceptionListObserver)

bool PartitioningExceptionList::Check(const nsACString& aFirstPartyOrigin,
                                      const nsACString& aThirdPartyOrigin) {
  if (!StaticPrefs::privacy_antitracking_enableWebcompat()) {
    LOG(("Partition exception list disabled via pref"));
    return false;
  }

  if (aFirstPartyOrigin.IsEmpty() || aThirdPartyOrigin.IsEmpty()) {
    return false;
  }

  LOG(("Check partitioning exception list for url %s and %s",
       PromiseFlatCString(aFirstPartyOrigin).get(),
       PromiseFlatCString(aThirdPartyOrigin).get()));

  nsAutoCString key, wildcardFirstPartyKey, wildcardThirdPartyKey;
  CreateExceptionListKey(aFirstPartyOrigin, aThirdPartyOrigin, key);
  CreateExceptionListKey("*"_ns, aThirdPartyOrigin, wildcardFirstPartyKey);
  CreateExceptionListKey(aFirstPartyOrigin, "*"_ns, wildcardThirdPartyKey);

  if (GetOrCreate()->mExceptionList.Contains(key) ||
      GetOrCreate()->mExceptionList.Contains(wildcardFirstPartyKey) ||
      GetOrCreate()->mExceptionList.Contains(wildcardThirdPartyKey)) {
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
    auto originsIt = origins.begin();

    if (originsIt == origins.end()) {
      LOG(("Ignoring empty exception entry"));
      continue;
    }

    nsAutoCString firstPartyOrigin;
    rv = CreateUnifiedOriginString(*originsIt, firstPartyOrigin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    ++originsIt;

    if (originsIt == origins.end()) {
      LOG(("Ignoring incomplete exception entry"));
      continue;
    }

    nsAutoCString thirdPartyOrigin;
    rv = CreateUnifiedOriginString(*originsIt, thirdPartyOrigin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsAutoCString key;
    rv = CreateExceptionListKey(firstPartyOrigin, thirdPartyOrigin, key);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    LOG(("onExceptionListUpdate: %s", key.get()));

    mExceptionList.AppendElement(key);
  }

  return NS_OK;
}

}  // namespace mozilla
