/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/CookieSettings.h"
#include "mozilla/Unused.h"
#include "nsGlobalWindowInner.h"
#if defined(MOZ_THUNDERBIRD) || defined(MOZ_SUITE)
#  include "nsIProtocolHandler.h"
#endif
#include "nsPermission.h"
#include "nsPermissionManager.h"

namespace mozilla {
namespace net {

namespace {

class PermissionComparator {
 public:
  bool Equals(nsIPermission* aA, nsIPermission* aB) const {
    nsCOMPtr<nsIPrincipal> principalA;
    nsresult rv = aA->GetPrincipal(getter_AddRefs(principalA));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return false;
    }

    nsCOMPtr<nsIPrincipal> principalB;
    rv = aB->GetPrincipal(getter_AddRefs(principalB));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return false;
    }

    bool equals = false;
    rv = principalA->Equals(principalB, &equals);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return false;
    }

    return equals;
  }
};

class ReleaseCookiePermissions final : public Runnable {
 public:
  explicit ReleaseCookiePermissions(nsTArray<RefPtr<nsIPermission>>& aArray)
      : Runnable("ReleaseCookiePermissions") {
    mArray.SwapElements(aArray);
  }

  NS_IMETHOD Run() override {
    MOZ_ASSERT(NS_IsMainThread());
    mArray.Clear();
    return NS_OK;
  }

 private:
  nsTArray<RefPtr<nsIPermission>> mArray;
};

}  // namespace

// static
already_AddRefed<nsICookieSettings> CookieSettings::CreateBlockingAll() {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<CookieSettings> cookieSettings =
      new CookieSettings(nsICookieService::BEHAVIOR_REJECT, eFixed);
  return cookieSettings.forget();
}

// static
already_AddRefed<nsICookieSettings> CookieSettings::Create() {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<CookieSettings> cookieSettings = new CookieSettings(
      StaticPrefs::network_cookie_cookieBehavior(), eProgressive);
  return cookieSettings.forget();
}

CookieSettings::CookieSettings(uint32_t aCookieBehavior, State aState)
    : mCookieBehavior(aCookieBehavior), mState(aState), mToBeMerged(false) {
  MOZ_ASSERT(NS_IsMainThread());
}

CookieSettings::~CookieSettings() {
  if (!NS_IsMainThread() && !mCookiePermissions.IsEmpty()) {
    nsCOMPtr<nsIEventTarget> systemGroupEventTarget =
        mozilla::SystemGroup::EventTargetFor(mozilla::TaskCategory::Other);
    MOZ_ASSERT(systemGroupEventTarget);

    RefPtr<Runnable> r = new ReleaseCookiePermissions(mCookiePermissions);
    MOZ_ASSERT(mCookiePermissions.IsEmpty());

    systemGroupEventTarget->Dispatch(r.forget());
  }
}

NS_IMETHODIMP
CookieSettings::GetCookieBehavior(uint32_t* aCookieBehavior) {
  *aCookieBehavior = mCookieBehavior;
  return NS_OK;
}

NS_IMETHODIMP
CookieSettings::CookiePermission(nsIPrincipal* aPrincipal,
                                 uint32_t* aCookiePermission) {
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG_POINTER(aPrincipal);
  NS_ENSURE_ARG_POINTER(aCookiePermission);

  *aCookiePermission = nsIPermissionManager::UNKNOWN_ACTION;

  nsresult rv;

  // Let's see if we know this permission.
  for (const RefPtr<nsIPermission>& permission : mCookiePermissions) {
    bool match = false;
    rv = permission->Matches(aPrincipal, false, &match);
    if (NS_WARN_IF(NS_FAILED(rv)) || !match) {
      continue;
    }

    rv = permission->GetCapability(aCookiePermission);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    return NS_OK;
  }

  // Let's ask the permission manager.
  nsPermissionManager* pm = nsPermissionManager::GetInstance();
  if (NS_WARN_IF(!pm)) {
    return NS_ERROR_FAILURE;
  }

#if defined(MOZ_THUNDERBIRD) || defined(MOZ_SUITE)
  // Check if this protocol doesn't allow cookies.
  bool hasFlags;
  nsCOMPtr<nsIURI> uri;
  aPrincipal->GetURI(getter_AddRefs(uri));
  rv = NS_URIChainHasFlags(uri, nsIProtocolHandler::URI_FORBIDS_COOKIE_ACCESS,
                           &hasFlags);
  if (NS_FAILED(rv) || hasFlags) {
    *aCookiePermission = nsPermissionManager::DENY_ACTION;
    rv = NS_OK;  // Reset, so it's not caught as a bad status after the `else`.
  } else         // Note the tricky `else` which controls the call below.
#endif

    rv = pm->TestPermissionFromPrincipal(
        aPrincipal, NS_LITERAL_CSTRING("cookie"), aCookiePermission);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Let's store the permission, also if the result is UNKNOWN in order to avoid
  // race conditions.

  nsCOMPtr<nsIPermission> permission = nsPermission::Create(
      aPrincipal, NS_LITERAL_CSTRING("cookie"), *aCookiePermission, 0, 0, 0);
  if (permission) {
    mCookiePermissions.AppendElement(permission);
  }

  mToBeMerged = true;
  return NS_OK;
}

void CookieSettings::Serialize(CookieSettingsArgs& aData) {
  MOZ_ASSERT(NS_IsMainThread());

  aData.isFixed() = mState == eFixed;
  aData.cookieBehavior() = mCookieBehavior;

  for (const RefPtr<nsIPermission>& permission : mCookiePermissions) {
    nsCOMPtr<nsIPrincipal> principal;
    nsresult rv = permission->GetPrincipal(getter_AddRefs(principal));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      continue;
    }

    PrincipalInfo principalInfo;
    rv = PrincipalToPrincipalInfo(principal, &principalInfo,
                                  true /* aSkipBaseDomain */);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      continue;
    }

    uint32_t cookiePermission = 0;
    rv = permission->GetCapability(&cookiePermission);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      continue;
    }

    aData.cookiePermissions().AppendElement(
        CookiePermissionData(principalInfo, cookiePermission));
  }

  mToBeMerged = false;
}

/* static */ void CookieSettings::Deserialize(
    const CookieSettingsArgs& aData, nsICookieSettings** aCookieSettings) {
  MOZ_ASSERT(NS_IsMainThread());

  CookiePermissionList list;
  for (const CookiePermissionData& data : aData.cookiePermissions()) {
    nsCOMPtr<nsIPrincipal> principal =
        PrincipalInfoToPrincipal(data.principalInfo());
    if (NS_WARN_IF(!principal)) {
      continue;
    }

    nsCOMPtr<nsIPermission> permission =
        nsPermission::Create(principal, NS_LITERAL_CSTRING("cookie"),
                             data.cookiePermission(), 0, 0, 0);
    if (NS_WARN_IF(!permission)) {
      continue;
    }

    list.AppendElement(permission);
  }

  RefPtr<CookieSettings> cookieSettings = new CookieSettings(
      aData.cookieBehavior(), aData.isFixed() ? eFixed : eProgressive);

  cookieSettings->mCookiePermissions.SwapElements(list);

  cookieSettings.forget(aCookieSettings);
}

void CookieSettings::Merge(const CookieSettingsArgs& aData) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mCookieBehavior == aData.cookieBehavior());

  if (mState == eFixed) {
    return;
  }

  PermissionComparator comparator;

  for (const CookiePermissionData& data : aData.cookiePermissions()) {
    nsCOMPtr<nsIPrincipal> principal =
        PrincipalInfoToPrincipal(data.principalInfo());
    if (NS_WARN_IF(!principal)) {
      continue;
    }

    nsCOMPtr<nsIPermission> permission =
        nsPermission::Create(principal, NS_LITERAL_CSTRING("cookie"),
                             data.cookiePermission(), 0, 0, 0);
    if (NS_WARN_IF(!permission)) {
      continue;
    }

    if (!mCookiePermissions.Contains(permission, comparator)) {
      mCookiePermissions.AppendElement(permission);
    }
  }
}

NS_IMPL_ISUPPORTS(CookieSettings, nsICookieSettings)

}  // namespace net
}  // namespace mozilla
