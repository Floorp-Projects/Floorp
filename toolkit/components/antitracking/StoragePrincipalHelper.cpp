/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StoragePrincipalHelper.h"

#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "mozilla/ContentBlocking.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StorageAccess.h"
#include "nsContentUtils.h"

namespace mozilla {

namespace {

bool ChooseOriginAttributes(nsIChannel* aChannel, OriginAttributes& aAttrs) {
  MOZ_ASSERT(aChannel);

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  nsCOMPtr<nsICookieJarSettings> cjs;
  if (NS_FAILED(loadInfo->GetCookieJarSettings(getter_AddRefs(cjs)))) {
    return false;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = aChannel->GetURI(getter_AddRefs(uri));
  if (NS_FAILED(rv)) {
    return false;
  }

  uint32_t rejectedReason = 0;
  if (ContentBlocking::ShouldAllowAccessFor(aChannel, uri, &rejectedReason)) {
    return false;
  }

  // Let's use the storage principal only if we need to partition the cookie
  // jar.  We use the lower-level ContentBlocking API here to ensure this
  // check doesn't send notifications.
  if (!ShouldPartitionStorage(rejectedReason) ||
      !StoragePartitioningEnabled(rejectedReason, cjs)) {
    return false;
  }

  nsCOMPtr<nsIPrincipal> toplevelPrincipal = loadInfo->GetTopLevelPrincipal();
  if (!toplevelPrincipal) {
    return false;
  }
  // Cast to BasePrincipal to continue to get acess to GetUri()
  auto* basePrin = BasePrincipal::Cast(toplevelPrincipal);
  nsCOMPtr<nsIURI> principalURI;

  rv = basePrin->GetURI(getter_AddRefs(principalURI));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  aAttrs.SetFirstPartyDomain(false, principalURI, true /* aForced */);
  return true;
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

  OriginAttributes attrs = aPrincipal->OriginAttributesRef();
  if (!ChooseOriginAttributes(aChannel, attrs)) {
    return NS_OK;
  }

  scopeExit.release();

  nsCOMPtr<nsIPrincipal> storagePrincipal =
      BasePrincipal::Cast(aPrincipal)->CloneForcingOriginAttributes(attrs);

  storagePrincipal.forget(aStoragePrincipal);
  return NS_OK;
}

// static
nsresult StoragePrincipalHelper::PrepareOriginAttributes(
    nsIChannel* aChannel, OriginAttributes& aOriginAttributes) {
  MOZ_ASSERT(aChannel);

  ChooseOriginAttributes(aChannel, aOriginAttributes);
  return NS_OK;
}

// static
bool StoragePrincipalHelper::VerifyValidStoragePrincipalInfoForPrincipalInfo(
    const mozilla::ipc::PrincipalInfo& aStoragePrincipalInfo,
    const mozilla::ipc::PrincipalInfo& aPrincipalInfo) {
  if (aStoragePrincipalInfo.type() != aPrincipalInfo.type()) {
    return false;
  }

  if (aStoragePrincipalInfo.type() ==
      mozilla::ipc::PrincipalInfo::TContentPrincipalInfo) {
    const mozilla::ipc::ContentPrincipalInfo& spInfo =
        aStoragePrincipalInfo.get_ContentPrincipalInfo();
    const mozilla::ipc::ContentPrincipalInfo& pInfo =
        aPrincipalInfo.get_ContentPrincipalInfo();

    if (!spInfo.attrs().EqualsIgnoringFPD(pInfo.attrs()) ||
        spInfo.originNoSuffix() != pInfo.originNoSuffix() ||
        spInfo.spec() != pInfo.spec() || spInfo.domain() != pInfo.domain() ||
        spInfo.baseDomain() != pInfo.baseDomain()) {
      return false;
    }

    return true;
  }

  if (aStoragePrincipalInfo.type() ==
      mozilla::ipc::PrincipalInfo::TSystemPrincipalInfo) {
    // Nothing to check here.
    return true;
  }

  if (aStoragePrincipalInfo.type() ==
      mozilla::ipc::PrincipalInfo::TNullPrincipalInfo) {
    const mozilla::ipc::NullPrincipalInfo& spInfo =
        aStoragePrincipalInfo.get_NullPrincipalInfo();
    const mozilla::ipc::NullPrincipalInfo& pInfo =
        aPrincipalInfo.get_NullPrincipalInfo();

    return spInfo.spec() == pInfo.spec() &&
           spInfo.attrs().EqualsIgnoringFPD(pInfo.attrs());
  }

  if (aStoragePrincipalInfo.type() ==
      mozilla::ipc::PrincipalInfo::TExpandedPrincipalInfo) {
    const mozilla::ipc::ExpandedPrincipalInfo& spInfo =
        aStoragePrincipalInfo.get_ExpandedPrincipalInfo();
    const mozilla::ipc::ExpandedPrincipalInfo& pInfo =
        aPrincipalInfo.get_ExpandedPrincipalInfo();

    if (!spInfo.attrs().EqualsIgnoringFPD(pInfo.attrs())) {
      return false;
    }

    if (spInfo.allowlist().Length() != pInfo.allowlist().Length()) {
      return false;
    }

    for (uint32_t i = 0; i < spInfo.allowlist().Length(); ++i) {
      if (!VerifyValidStoragePrincipalInfoForPrincipalInfo(
              spInfo.allowlist()[i], pInfo.allowlist()[i])) {
        return false;
      }
    }

    return true;
  }

  MOZ_CRASH("Invalid principalInfo type");
  return false;
}

}  // namespace mozilla
