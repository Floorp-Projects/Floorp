/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AntiTrackingUtils.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/ContentBlockingAllowList.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/net/CookieJarSettings.h"
#include "mozilla/net/NeckoChannelParams.h"
#include "mozilla/Permission.h"
#include "mozilla/PermissionManager.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/Unused.h"
#include "nsGlobalWindowInner.h"
#include "nsIPrincipal.h"
#if defined(MOZ_THUNDERBIRD) || defined(MOZ_SUITE)
#  include "nsIProtocolHandler.h"
#endif
#include "nsIClassInfoImpl.h"
#include "nsIChannel.h"
#include "nsICookieManager.h"
#include "nsICookieService.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace net {

NS_IMPL_CLASSINFO(CookieJarSettings, nullptr, nsIClassInfo::THREADSAFE,
                  COOKIEJARSETTINGS_CID)

NS_IMPL_ISUPPORTS_CI(CookieJarSettings, nsICookieJarSettings, nsISerializable)

static StaticRefPtr<CookieJarSettings> sBlockinAll;

namespace {

class PermissionComparator {
 public:
  static bool Equals(nsIPermission* aA, nsIPermission* aB) {
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
  explicit ReleaseCookiePermissions(nsTArray<RefPtr<nsIPermission>>&& aArray)
      : Runnable("ReleaseCookiePermissions"), mArray(std::move(aArray)) {}

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
already_AddRefed<nsICookieJarSettings> CookieJarSettings::GetBlockingAll(
    bool aShouldResistFingerprinting) {
  MOZ_ASSERT(NS_IsMainThread());

  if (sBlockinAll) {
    return do_AddRef(sBlockinAll);
  }

  sBlockinAll = new CookieJarSettings(nsICookieService::BEHAVIOR_REJECT,
                                      OriginAttributes::IsFirstPartyEnabled(),
                                      aShouldResistFingerprinting, eFixed);
  ClearOnShutdown(&sBlockinAll);

  return do_AddRef(sBlockinAll);
}

// static
already_AddRefed<nsICookieJarSettings> CookieJarSettings::Create(
    CreateMode aMode, bool aShouldResistFingerprinting) {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<CookieJarSettings> cookieJarSettings;

  switch (aMode) {
    case eRegular:
    case ePrivate:
      cookieJarSettings = new CookieJarSettings(
          nsICookieManager::GetCookieBehavior(aMode == ePrivate),
          OriginAttributes::IsFirstPartyEnabled(), aShouldResistFingerprinting,
          eProgressive);
      break;

    default:
      MOZ_CRASH("Unexpected create mode.");
  }

  return cookieJarSettings.forget();
}

// static
already_AddRefed<nsICookieJarSettings> CookieJarSettings::Create(
    nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(NS_IsMainThread());

  bool shouldResistFingerprinting =
      nsContentUtils::ShouldResistFingerprinting_dangerous(
          aPrincipal, "We are constructing CookieJarSettings here.");

  if (aPrincipal && aPrincipal->OriginAttributesRef().mPrivateBrowsingId > 0) {
    return Create(ePrivate, shouldResistFingerprinting);
  }

  return Create(eRegular, shouldResistFingerprinting);
}

// static
already_AddRefed<nsICookieJarSettings> CookieJarSettings::Create(
    uint32_t aCookieBehavior, const nsAString& aPartitionKey,
    bool aIsFirstPartyIsolated, bool aIsOnContentBlockingAllowList,
    bool aShouldResistFingerprinting) {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<CookieJarSettings> cookieJarSettings =
      new CookieJarSettings(aCookieBehavior, aIsFirstPartyIsolated,
                            aShouldResistFingerprinting, eProgressive);
  cookieJarSettings->mPartitionKey = aPartitionKey;
  cookieJarSettings->mIsOnContentBlockingAllowList =
      aIsOnContentBlockingAllowList;

  return cookieJarSettings.forget();
}

// static
already_AddRefed<nsICookieJarSettings> CookieJarSettings::CreateForXPCOM() {
  MOZ_ASSERT(NS_IsMainThread());
  return Create(eRegular, /* shouldResistFingerprinting */ false);
}

CookieJarSettings::CookieJarSettings(uint32_t aCookieBehavior,
                                     bool aIsFirstPartyIsolated,
                                     bool aShouldResistFingerprinting,
                                     State aState)
    : mCookieBehavior(aCookieBehavior),
      mIsFirstPartyIsolated(aIsFirstPartyIsolated),
      mIsOnContentBlockingAllowList(false),
      mIsOnContentBlockingAllowListUpdated(false),
      mState(aState),
      mToBeMerged(false),
      mShouldResistFingerprinting(aShouldResistFingerprinting) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT_IF(
      mIsFirstPartyIsolated,
      mCookieBehavior !=
          nsICookieService::BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN);
}

CookieJarSettings::~CookieJarSettings() {
  if (!NS_IsMainThread() && !mCookiePermissions.IsEmpty()) {
    RefPtr<Runnable> r =
        new ReleaseCookiePermissions(std::move(mCookiePermissions));
    MOZ_ASSERT(mCookiePermissions.IsEmpty());

    SchedulerGroup::Dispatch(TaskCategory::Other, r.forget());
  }
}

NS_IMETHODIMP
CookieJarSettings::InitWithURI(nsIURI* aURI, bool aIsPrivate) {
  NS_ENSURE_ARG(aURI);

  mCookieBehavior = nsICookieManager::GetCookieBehavior(aIsPrivate);

  SetPartitionKey(aURI);
  return NS_OK;
}

NS_IMETHODIMP
CookieJarSettings::GetCookieBehavior(uint32_t* aCookieBehavior) {
  *aCookieBehavior = mCookieBehavior;
  return NS_OK;
}

NS_IMETHODIMP
CookieJarSettings::GetIsFirstPartyIsolated(bool* aIsFirstPartyIsolated) {
  *aIsFirstPartyIsolated = mIsFirstPartyIsolated;
  return NS_OK;
}

NS_IMETHODIMP
CookieJarSettings::GetShouldResistFingerprinting(
    bool* aShouldResistFingerprinting) {
  *aShouldResistFingerprinting = mShouldResistFingerprinting;
  return NS_OK;
}

NS_IMETHODIMP
CookieJarSettings::GetRejectThirdPartyContexts(
    bool* aRejectThirdPartyContexts) {
  *aRejectThirdPartyContexts =
      CookieJarSettings::IsRejectThirdPartyContexts(mCookieBehavior);
  return NS_OK;
}

NS_IMETHODIMP
CookieJarSettings::GetLimitForeignContexts(bool* aLimitForeignContexts) {
  *aLimitForeignContexts =
      mCookieBehavior == nsICookieService::BEHAVIOR_LIMIT_FOREIGN ||
      (StaticPrefs::privacy_dynamic_firstparty_limitForeign() &&
       mCookieBehavior ==
           nsICookieService::BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN);
  return NS_OK;
}

NS_IMETHODIMP
CookieJarSettings::GetBlockingAllThirdPartyContexts(
    bool* aBlockingAllThirdPartyContexts) {
  // XXX For non-cookie forms of storage, we handle BEHAVIOR_LIMIT_FOREIGN by
  // simply rejecting the request to use the storage. In the future, if we
  // change the meaning of BEHAVIOR_LIMIT_FOREIGN to be one which makes sense
  // for non-cookie storage types, this may change.
  *aBlockingAllThirdPartyContexts =
      mCookieBehavior == nsICookieService::BEHAVIOR_LIMIT_FOREIGN ||
      (!StaticPrefs::network_cookie_rejectForeignWithExceptions_enabled() &&
       mCookieBehavior == nsICookieService::BEHAVIOR_REJECT_FOREIGN);
  return NS_OK;
}

NS_IMETHODIMP
CookieJarSettings::GetBlockingAllContexts(bool* aBlockingAllContexts) {
  *aBlockingAllContexts = mCookieBehavior == nsICookieService::BEHAVIOR_REJECT;
  return NS_OK;
}

NS_IMETHODIMP
CookieJarSettings::GetPartitionForeign(bool* aPartitionForeign) {
  *aPartitionForeign =
      mCookieBehavior ==
      nsICookieService::BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN;
  return NS_OK;
}

NS_IMETHODIMP
CookieJarSettings::SetPartitionForeign(bool aPartitionForeign) {
  if (mIsFirstPartyIsolated) {
    return NS_OK;
  }

  if (aPartitionForeign) {
    mCookieBehavior =
        nsICookieService::BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN;
  }
  return NS_OK;
}

NS_IMETHODIMP
CookieJarSettings::GetIsOnContentBlockingAllowList(
    bool* aIsOnContentBlockingAllowList) {
  *aIsOnContentBlockingAllowList = mIsOnContentBlockingAllowList;
  return NS_OK;
}

NS_IMETHODIMP
CookieJarSettings::GetPartitionKey(nsAString& aPartitionKey) {
  aPartitionKey = mPartitionKey;
  return NS_OK;
}

NS_IMETHODIMP
CookieJarSettings::CookiePermission(nsIPrincipal* aPrincipal,
                                    uint32_t* aCookiePermission) {
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG_POINTER(aPrincipal);
  NS_ENSURE_ARG_POINTER(aCookiePermission);

  *aCookiePermission = nsIPermissionManager::UNKNOWN_ACTION;

  nsresult rv;

  // Let's see if we know this permission.
  if (!mCookiePermissions.IsEmpty()) {
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
  }

  // Let's ask the permission manager.
  PermissionManager* pm = PermissionManager::GetInstance();
  if (NS_WARN_IF(!pm)) {
    return NS_ERROR_FAILURE;
  }

#if defined(MOZ_THUNDERBIRD) || defined(MOZ_SUITE)
  // Check if this protocol doesn't allow cookies.
  bool hasFlags;
  nsCOMPtr<nsIURI> uri;
  BasePrincipal::Cast(aPrincipal)->GetURI(getter_AddRefs(uri));

  rv = NS_URIChainHasFlags(uri, nsIProtocolHandler::URI_FORBIDS_COOKIE_ACCESS,
                           &hasFlags);
  if (NS_FAILED(rv) || hasFlags) {
    *aCookiePermission = PermissionManager::DENY_ACTION;
    rv = NS_OK;  // Reset, so it's not caught as a bad status after the `else`.
  } else         // Note the tricky `else` which controls the call below.
#endif

    rv = pm->TestPermissionFromPrincipal(aPrincipal, "cookie"_ns,
                                         aCookiePermission);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Let's store the permission, also if the result is UNKNOWN in order to avoid
  // race conditions.

  nsCOMPtr<nsIPermission> permission =
      Permission::Create(aPrincipal, "cookie"_ns, *aCookiePermission, 0, 0, 0);
  if (permission) {
    mCookiePermissions.AppendElement(permission);
  }

  mToBeMerged = true;
  return NS_OK;
}

void CookieJarSettings::Serialize(CookieJarSettingsArgs& aData) {
  MOZ_ASSERT(NS_IsMainThread());

  aData.isFixed() = mState == eFixed;
  aData.cookieBehavior() = mCookieBehavior;
  aData.isFirstPartyIsolated() = mIsFirstPartyIsolated;
  aData.shouldResistFingerprinting() = mShouldResistFingerprinting;
  aData.isOnContentBlockingAllowList() = mIsOnContentBlockingAllowList;
  aData.partitionKey() = mPartitionKey;

  for (const RefPtr<nsIPermission>& permission : mCookiePermissions) {
    nsCOMPtr<nsIPrincipal> principal;
    nsresult rv = permission->GetPrincipal(getter_AddRefs(principal));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      continue;
    }

    ipc::PrincipalInfo principalInfo;
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

/* static */ void CookieJarSettings::Deserialize(
    const CookieJarSettingsArgs& aData,
    nsICookieJarSettings** aCookieJarSettings) {
  MOZ_ASSERT(NS_IsMainThread());

  CookiePermissionList list;
  for (const CookiePermissionData& data : aData.cookiePermissions()) {
    auto principalOrErr = PrincipalInfoToPrincipal(data.principalInfo());
    if (NS_WARN_IF(principalOrErr.isErr())) {
      continue;
    }

    nsCOMPtr<nsIPrincipal> principal = principalOrErr.unwrap();

    nsCOMPtr<nsIPermission> permission = Permission::Create(
        principal, "cookie"_ns, data.cookiePermission(), 0, 0, 0);
    if (NS_WARN_IF(!permission)) {
      continue;
    }

    list.AppendElement(permission);
  }

  RefPtr<CookieJarSettings> cookieJarSettings = new CookieJarSettings(
      aData.cookieBehavior(), aData.isFirstPartyIsolated(),
      aData.shouldResistFingerprinting(),
      aData.isFixed() ? eFixed : eProgressive);

  cookieJarSettings->mIsOnContentBlockingAllowList =
      aData.isOnContentBlockingAllowList();
  cookieJarSettings->mCookiePermissions = std::move(list);
  cookieJarSettings->mPartitionKey = aData.partitionKey();
  cookieJarSettings->mShouldResistFingerprinting =
      aData.shouldResistFingerprinting();

  cookieJarSettings.forget(aCookieJarSettings);
}

void CookieJarSettings::Merge(const CookieJarSettingsArgs& aData) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(
      mCookieBehavior == aData.cookieBehavior() ||
      (mCookieBehavior == nsICookieService::BEHAVIOR_REJECT_TRACKER &&
       aData.cookieBehavior() ==
           nsICookieService::BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN) ||
      (mCookieBehavior ==
           nsICookieService::BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN &&
       aData.cookieBehavior() == nsICookieService::BEHAVIOR_REJECT_TRACKER));

  if (mState == eFixed) {
    return;
  }

  // Merge cookie behavior pref values
  if (mCookieBehavior == nsICookieService::BEHAVIOR_REJECT_TRACKER &&
      aData.cookieBehavior() ==
          nsICookieService::BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN) {
    // If the other side has decided to partition third-party cookies, update
    // our side when first-party isolation is disabled.
    if (!mIsFirstPartyIsolated) {
      mCookieBehavior =
          nsICookieService::BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN;
    }
  }
  if (mCookieBehavior ==
          nsICookieService::BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN &&
      aData.cookieBehavior() == nsICookieService::BEHAVIOR_REJECT_TRACKER) {
    // If we've decided to partition third-party cookies, the other side may not
    // have caught up yet unless it has first-party isolation enabled.
    if (aData.isFirstPartyIsolated()) {
      mCookieBehavior = nsICookieService::BEHAVIOR_REJECT_TRACKER;
      mIsFirstPartyIsolated = true;
    }
  }
  // Ignore all other cases.
  MOZ_ASSERT_IF(
      mIsFirstPartyIsolated,
      mCookieBehavior !=
          nsICookieService::BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN);

  if (aData.shouldResistFingerprinting()) {
    mShouldResistFingerprinting = true;
  }

  PermissionComparator comparator;

  for (const CookiePermissionData& data : aData.cookiePermissions()) {
    auto principalOrErr = PrincipalInfoToPrincipal(data.principalInfo());
    if (NS_WARN_IF(principalOrErr.isErr())) {
      continue;
    }

    nsCOMPtr<nsIPrincipal> principal = principalOrErr.unwrap();
    nsCOMPtr<nsIPermission> permission = Permission::Create(
        principal, "cookie"_ns, data.cookiePermission(), 0, 0, 0);
    if (NS_WARN_IF(!permission)) {
      continue;
    }

    if (!mCookiePermissions.Contains(permission, comparator)) {
      mCookiePermissions.AppendElement(permission);
    }
  }
}

void CookieJarSettings::SetPartitionKey(nsIURI* aURI) {
  MOZ_ASSERT(aURI);

  OriginAttributes attrs;
  attrs.SetPartitionKey(aURI);
  mPartitionKey = std::move(attrs.mPartitionKey);
}

void CookieJarSettings::UpdateIsOnContentBlockingAllowList(
    nsIChannel* aChannel) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(aChannel);

  // Early return if the flag was updated before.
  if (mIsOnContentBlockingAllowListUpdated) {
    return;
  }
  mIsOnContentBlockingAllowListUpdated = true;

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();

  nsCOMPtr<nsIURI> uri;
  nsresult rv = aChannel->GetURI(getter_AddRefs(uri));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  // We need to recompute the ContentBlockingAllowListPrincipal here for the
  // top level channel because we might navigate from the the initial
  // about:blank page or the existing page which may have a different origin
  // than the URI we are going to load here. Thus, we need to recompute the
  // prinicpal in order to get the correct ContentBlockingAllowListPrincipal.
  nsCOMPtr<nsIPrincipal> contentBlockingAllowListPrincipal;
  OriginAttributes attrs;
  loadInfo->GetOriginAttributes(&attrs);
  ContentBlockingAllowList::RecomputePrincipal(
      uri, attrs, getter_AddRefs(contentBlockingAllowListPrincipal));

  if (!contentBlockingAllowListPrincipal ||
      !contentBlockingAllowListPrincipal->GetIsContentPrincipal()) {
    return;
  }

  Unused << ContentBlockingAllowList::Check(contentBlockingAllowListPrincipal,
                                            NS_UsePrivateBrowsing(aChannel),
                                            mIsOnContentBlockingAllowList);
}

// static
bool CookieJarSettings::IsRejectThirdPartyContexts(uint32_t aCookieBehavior) {
  return aCookieBehavior == nsICookieService::BEHAVIOR_REJECT_TRACKER ||
         aCookieBehavior ==
             nsICookieService::BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN ||
         IsRejectThirdPartyWithExceptions(aCookieBehavior);
}

// static
bool CookieJarSettings::IsRejectThirdPartyWithExceptions(
    uint32_t aCookieBehavior) {
  return aCookieBehavior == nsICookieService::BEHAVIOR_REJECT_FOREIGN &&
         StaticPrefs::network_cookie_rejectForeignWithExceptions_enabled();
}

NS_IMETHODIMP
CookieJarSettings::Read(nsIObjectInputStream* aStream) {
  nsresult rv = aStream->Read32(&mCookieBehavior);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aStream->ReadBoolean(&mIsFirstPartyIsolated);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aStream->ReadBoolean(&mShouldResistFingerprinting);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool isFixed;
  aStream->ReadBoolean(&isFixed);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  mState = isFixed ? eFixed : eProgressive;

  rv = aStream->ReadBoolean(&mIsOnContentBlockingAllowList);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aStream->ReadString(mPartitionKey);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Deserializing the cookie permission list.
  uint32_t cookiePermissionsLength;
  rv = aStream->Read32(&cookiePermissionsLength);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!cookiePermissionsLength) {
    // Bailing out early because there is no cookie permission.
    return NS_OK;
  }

  CookiePermissionList list;
  mCookiePermissions.SetCapacity(cookiePermissionsLength);
  for (uint32_t i = 0; i < cookiePermissionsLength; ++i) {
    nsAutoCString principalJSON;
    aStream->ReadCString(principalJSON);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsCOMPtr<nsIPrincipal> principal = BasePrincipal::FromJSON(principalJSON);

    if (NS_WARN_IF(!principal)) {
      continue;
    }

    uint32_t cookiePermission;
    aStream->Read32(&cookiePermission);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsCOMPtr<nsIPermission> permission =
        Permission::Create(principal, "cookie"_ns, cookiePermission, 0, 0, 0);
    if (NS_WARN_IF(!permission)) {
      continue;
    }

    list.AppendElement(permission);
  }

  mCookiePermissions = std::move(list);

  return NS_OK;
}

NS_IMETHODIMP
CookieJarSettings::Write(nsIObjectOutputStream* aStream) {
  nsresult rv = aStream->Write32(mCookieBehavior);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aStream->WriteBoolean(mIsFirstPartyIsolated);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aStream->WriteBoolean(mShouldResistFingerprinting);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aStream->WriteBoolean(mState == eFixed);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aStream->WriteBoolean(mIsOnContentBlockingAllowList);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aStream->WriteWStringZ(mPartitionKey.get());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Serializing the cookie permission list. It will first write the length of
  // the list, and then, write the cookie permission consecutively.
  uint32_t cookiePermissionsLength = mCookiePermissions.Length();
  rv = aStream->Write32(cookiePermissionsLength);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  for (const RefPtr<nsIPermission>& permission : mCookiePermissions) {
    nsCOMPtr<nsIPrincipal> principal;
    nsresult rv = permission->GetPrincipal(getter_AddRefs(principal));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      continue;
    }

    nsAutoCString principalJSON;
    BasePrincipal::Cast(principal)->ToJSON(principalJSON);

    rv = aStream->WriteStringZ(principalJSON.get());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    uint32_t cookiePermission = 0;
    rv = permission->GetCapability(&cookiePermission);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      continue;
    }

    rv = aStream->Write32(cookiePermission);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
