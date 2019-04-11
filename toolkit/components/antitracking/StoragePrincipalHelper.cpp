/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StoragePrincipalHelper.h"

#include "mozilla/ScopeExit.h"
#include "mozilla/StaticPrefs.h"
#include "nsContentUtils.h"
#include "nsIHttpChannel.h"

namespace mozilla {

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

  if (!StaticPrefs::privacy_storagePrincipal_enabledForTrackers()) {
    return NS_OK;
  }

  // Let's use the storage principal only if we need to partition the cookie
  // jar.
  nsContentUtils::StorageAccess access =
      nsContentUtils::StorageAllowedForChannel(aChannel);
  if (access != nsContentUtils::StorageAccess::ePartitionedOrDeny) {
    return NS_OK;
  }

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aChannel);
  if (!httpChannel) {
    return NS_OK;
  }

  MOZ_ASSERT(httpChannel->IsThirdPartyTrackingResource());

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  nsCOMPtr<nsIPrincipal> toplevelPrincipal = loadInfo->GetTopLevelPrincipal();
  if (!toplevelPrincipal) {
    return NS_OK;
  }

  nsCOMPtr<nsIURI> principalURI;
  nsresult rv = toplevelPrincipal->GetURI(getter_AddRefs(principalURI));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  scopeExit.release();

  nsCOMPtr<nsIPrincipal> storagePrincipal =
      BasePrincipal::Cast(aPrincipal)
          ->CloneForcingFirstPartyDomain(principalURI);

  storagePrincipal.forget(aStoragePrincipal);
  return NS_OK;
}

}  // namespace mozilla
