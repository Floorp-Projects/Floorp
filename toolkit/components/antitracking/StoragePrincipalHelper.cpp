/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StoragePrincipalHelper.h"

#include "mozilla/AntiTrackingCommon.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StorageAccess.h"
#include "mozilla/StaticPrefs.h"
#include "nsContentUtils.h"
#include "nsIHttpChannel.h"

namespace mozilla {

namespace {

already_AddRefed<nsIURI> MaybeGetFirstPartyURI(nsIChannel* aChannel) {
  MOZ_ASSERT(aChannel);

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  nsCOMPtr<nsICookieSettings> cs;
  if (NS_FAILED(loadInfo->GetCookieSettings(getter_AddRefs(cs)))) {
    return nullptr;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = aChannel->GetURI(getter_AddRefs(uri));
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aChannel);
  if (!httpChannel) {
    return nullptr;
  }

  uint32_t rejectedReason = 0;
  if (AntiTrackingCommon::IsFirstPartyStorageAccessGrantedFor(
          httpChannel, uri, &rejectedReason)) {
    return nullptr;
  }

  // Let's use the storage principal only if we need to partition the cookie
  // jar.  We use the lower-level AntiTrackingCommon API here to ensure this
  // check doesn't send notifications.
  if (!ShouldPartitionStorage(rejectedReason) ||
      !StoragePartitioningEnabled(rejectedReason, cs)) {
    return nullptr;
  }

  nsCOMPtr<nsIPrincipal> toplevelPrincipal = loadInfo->GetTopLevelPrincipal();
  if (!toplevelPrincipal) {
    return nullptr;
  }

  nsCOMPtr<nsIURI> principalURI;
  rv = toplevelPrincipal->GetURI(getter_AddRefs(principalURI));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  return principalURI.forget();
}

}  // namespace

// static
nsresult StoragePrincipalHelper::Create(nsIChannel* aChannel,
                                        nsIPrincipal* aPrincipal,
                                        nsIPrincipal** aStoragePrincipal) {
  MOZ_ASSERT(aChannel);
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aStoragePrincipal);

  auto scopeExit = MakeScopeExit([&] {
    nsCOMPtr<nsIPrincipal> storagePrincipal = aPrincipal;
    storagePrincipal.forget(aStoragePrincipal);
  });

  nsCOMPtr<nsIURI> principalURI = MaybeGetFirstPartyURI(aChannel);
  if (!principalURI) {
    return NS_OK;
  }

  scopeExit.release();

  nsCOMPtr<nsIPrincipal> storagePrincipal =
      BasePrincipal::Cast(aPrincipal)
          ->CloneForcingFirstPartyDomain(principalURI);

  storagePrincipal.forget(aStoragePrincipal);
  return NS_OK;
}

// static
nsresult StoragePrincipalHelper::PrepareOriginAttributes(
    nsIChannel* aChannel, OriginAttributes& aOriginAttributes) {
  MOZ_ASSERT(aChannel);

  nsCOMPtr<nsIURI> principalURI = MaybeGetFirstPartyURI(aChannel);
  if (!principalURI) {
    return NS_OK;
  }

  aOriginAttributes.SetFirstPartyDomain(false, principalURI,
                                        true /* aForced */);
  return NS_OK;
}

}  // namespace mozilla
