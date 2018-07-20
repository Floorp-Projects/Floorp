/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AntiTrackingCommon.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/ipc/MessageChannel.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/StaticPrefs.h"
#include "mozIThirdPartyUtil.h"
#include "nsContentUtils.h"
#include "nsGlobalWindowInner.h"
#include "nsIPermissionManager.h"
#include "nsIPrincipal.h"
#include "nsIURI.h"
#include "nsPIDOMWindow.h"
#include "prtime.h"

#define ANTITRACKING_PERM_KEY "3rdPartyStorage"

using namespace mozilla;
using mozilla::dom::ContentChild;

namespace {

bool
GetParentPrincipalAndTrackingOrigin(nsGlobalWindowInner* a3rdPartyTrackingWindow,
                                    nsIPrincipal** aTopLevelStoragePrincipal,
                                    nsACString& aTrackingOrigin)
{
  MOZ_ASSERT(nsContentUtils::IsTrackingResourceWindow(a3rdPartyTrackingWindow));

  // Now we need the principal and the origin of the parent window.
  nsCOMPtr<nsIPrincipal> topLevelStoragePrincipal =
    a3rdPartyTrackingWindow->GetTopLevelStorageAreaPrincipal();
  if (NS_WARN_IF(!topLevelStoragePrincipal)) {
    return false;
  }

  // Let's take the principal and the origin of the tracker.
  nsIPrincipal* trackingPrincipal = a3rdPartyTrackingWindow->GetPrincipal();
  if (NS_WARN_IF(!trackingPrincipal)) {
    return false;
  }

  nsresult rv = trackingPrincipal->GetOriginNoSuffix(aTrackingOrigin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  topLevelStoragePrincipal.forget(aTopLevelStoragePrincipal);
  return true;
};

void
CreatePermissionKey(const nsCString& aTrackingOrigin,
                    const nsCString& aGrantedOrigin,
                    nsACString& aPermissionKey)
{
  if (aTrackingOrigin == aGrantedOrigin) {
    aPermissionKey = nsPrintfCString(ANTITRACKING_PERM_KEY "^%s",
                                     aTrackingOrigin.get());
    return;
  }

  aPermissionKey = nsPrintfCString(ANTITRACKING_PERM_KEY "^%s^%s",
                                   aTrackingOrigin.get(),
                                   aGrantedOrigin.get());
}

} // anonymous

/* static */ RefPtr<AntiTrackingCommon::StorageAccessGrantPromise>
AntiTrackingCommon::AddFirstPartyStorageAccessGrantedFor(const nsAString& aOrigin,
                                                         nsPIDOMWindowInner* aParentWindow)
{
  MOZ_ASSERT(aParentWindow);

  if (!StaticPrefs::privacy_restrict3rdpartystorage_enabled()) {
    return StorageAccessGrantPromise::CreateAndResolve(true, __func__);
  }

  nsCOMPtr<nsIPrincipal> topLevelStoragePrincipal;
  nsAutoCString trackingOrigin;

  nsGlobalWindowInner* parentWindow = nsGlobalWindowInner::Cast(aParentWindow);
  nsGlobalWindowOuter* outerParentWindow =
    nsGlobalWindowOuter::Cast(parentWindow->GetOuterWindow());
  if (NS_WARN_IF(!outerParentWindow)) {
    return StorageAccessGrantPromise::CreateAndReject(false, __func__);
  }

  // We are a first party resource.
  if (outerParentWindow->IsTopLevelWindow()) {
    CopyUTF16toUTF8(aOrigin, trackingOrigin);
    topLevelStoragePrincipal = parentWindow->GetPrincipal();
    if (NS_WARN_IF(!topLevelStoragePrincipal)) {
      return StorageAccessGrantPromise::CreateAndReject(false, __func__);
    }

  // We are a 3rd party source.
  } else if (!GetParentPrincipalAndTrackingOrigin(parentWindow,
                                                  getter_AddRefs(topLevelStoragePrincipal),
                                                  trackingOrigin)) {
    return StorageAccessGrantPromise::CreateAndReject(false, __func__);
  }

  NS_ConvertUTF16toUTF8 grantedOrigin(aOrigin);

  if (XRE_IsParentProcess()) {
    RefPtr<StorageAccessGrantPromise::Private> p = new StorageAccessGrantPromise::Private(__func__);
    SaveFirstPartyStorageAccessGrantedForOriginOnParentProcess(topLevelStoragePrincipal,
                                                               trackingOrigin,
                                                               grantedOrigin,
                                                               [p] (bool success) {
                                                                 p->Resolve(success, __func__);
                                                               });
    return p;
  }

  ContentChild* cc = ContentChild::GetSingleton();
  MOZ_ASSERT(cc);

  // This is not really secure, because here we have the content process sending
  // the request of storing a permission.
  RefPtr<StorageAccessGrantPromise::Private> p = new StorageAccessGrantPromise::Private(__func__);
  cc->SendFirstPartyStorageAccessGrantedForOrigin(IPC::Principal(topLevelStoragePrincipal),
                                                  trackingOrigin,
                                                  grantedOrigin)
    ->Then(GetCurrentThreadSerialEventTarget(), __func__,
           [p] (bool success) {
             p->Resolve(success, __func__);
           }, [p] (ipc::ResponseRejectReason aReason) {
             p->Reject(false, __func__);
           });
  return p;
}

/* static */ void
AntiTrackingCommon::SaveFirstPartyStorageAccessGrantedForOriginOnParentProcess(nsIPrincipal* aParentPrincipal,
                                                                               const nsCString& aTrackingOrigin,
                                                                               const nsCString& aGrantedOrigin,
                                                                               FirstPartyStorageAccessGrantedForOriginResolver&& aResolver)
{
  MOZ_ASSERT(XRE_IsParentProcess());

  if (NS_WARN_IF(!aParentPrincipal)) {
    // The child process is sending something wrong. Let's ignore it.
    aResolver(false);
    return;
  }

  nsCOMPtr<nsIPermissionManager> pm = services::GetPermissionManager();
  if (NS_WARN_IF(!pm)) {
    aResolver(false);
    return;
  }

  // Remember that this pref is stored in seconds!
  uint32_t expirationTime =
    StaticPrefs::privacy_restrict3rdpartystorage_expiration() * 1000;
  int64_t when = (PR_Now() / PR_USEC_PER_MSEC) + expirationTime;

  nsAutoCString type;
  CreatePermissionKey(aTrackingOrigin, aGrantedOrigin, type);

  nsresult rv = pm->AddFromPrincipal(aParentPrincipal, type.get(),
                                     nsIPermissionManager::ALLOW_ACTION,
                                     nsIPermissionManager::EXPIRE_TIME, when);
  Unused << NS_WARN_IF(NS_FAILED(rv));
  aResolver(NS_SUCCEEDED(rv));
}

bool
AntiTrackingCommon::IsFirstPartyStorageAccessGrantedFor(nsPIDOMWindowInner* a3rdPartyTrackingWindow,
                                                        nsIURI* aURI)
{
  MOZ_ASSERT(a3rdPartyTrackingWindow);
  MOZ_ASSERT(aURI);

  if (!StaticPrefs::privacy_restrict3rdpartystorage_enabled()) {
    return true;
  }

  if (!nsContentUtils::IsThirdPartyWindowOrChannel(a3rdPartyTrackingWindow,
                                                   nullptr, aURI) ||
      !nsContentUtils::IsTrackingResourceWindow(a3rdPartyTrackingWindow)) {
    return true;
  }

  nsCOMPtr<nsIPrincipal> parentPrincipal;
  nsAutoCString trackingOrigin;
  if (!GetParentPrincipalAndTrackingOrigin(nsGlobalWindowInner::Cast(a3rdPartyTrackingWindow),
                                           getter_AddRefs(parentPrincipal),
                                           trackingOrigin)) {
    return false;
  }

  nsAutoString origin;
  nsresult rv = nsContentUtils::GetUTFOrigin(aURI, origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  nsAutoCString type;
  CreatePermissionKey(trackingOrigin, NS_ConvertUTF16toUTF8(origin), type);

  nsCOMPtr<nsIPermissionManager> pm = services::GetPermissionManager();
  if (NS_WARN_IF(!pm)) {
    return false;
  }

  uint32_t result = 0;
  rv = pm->TestPermissionFromPrincipal(parentPrincipal, type.get(), &result);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  return result == nsIPermissionManager::ALLOW_ACTION;
}

bool
AntiTrackingCommon::IsFirstPartyStorageAccessGrantedFor(nsIHttpChannel* aChannel,
                                                        nsIURI* aURI)
{
  MOZ_ASSERT(aURI);
  MOZ_ASSERT(aChannel);
  MOZ_ASSERT(aChannel->GetIsTrackingResource());

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->GetLoadInfo();
  if (!loadInfo) {
    return true;
  }

  nsIPrincipal* parentPrincipal = loadInfo->TopLevelStorageAreaPrincipal();
  if (!parentPrincipal) {
    parentPrincipal = loadInfo->TriggeringPrincipal();
    if (NS_WARN_IF(!parentPrincipal)) {
      // Why we are here?!?
      return true;
    }
  }

  nsCOMPtr<nsIURI> trackingURI;
  nsresult rv = aChannel->GetURI(getter_AddRefs(trackingURI));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return true;
  }

  nsAutoString trackingOrigin;
  rv = nsContentUtils::GetUTFOrigin(trackingURI, trackingOrigin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  nsAutoString origin;
  rv = nsContentUtils::GetUTFOrigin(aURI, origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  nsAutoCString type;
  CreatePermissionKey(NS_ConvertUTF16toUTF8(trackingOrigin),
                      NS_ConvertUTF16toUTF8(origin), type);

  nsCOMPtr<nsIPermissionManager> pm = services::GetPermissionManager();
  if (NS_WARN_IF(!pm)) {
    return false;
  }

  uint32_t result = 0;
  rv = pm->TestPermissionFromPrincipal(parentPrincipal, type.get(), &result);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  return result == nsIPermissionManager::ALLOW_ACTION;
}

/* static */ bool
AntiTrackingCommon::MaybeIsFirstPartyStorageAccessGrantedFor(nsPIDOMWindowInner* aFirstPartyWindow,
                                                             nsIURI* aURI)
{
  MOZ_ASSERT(aFirstPartyWindow);
  MOZ_ASSERT(!nsContentUtils::IsTrackingResourceWindow(aFirstPartyWindow));
  MOZ_ASSERT(aURI);

  if (!StaticPrefs::privacy_restrict3rdpartystorage_enabled()) {
    return true;
  }

  if (!nsContentUtils::IsThirdPartyWindowOrChannel(aFirstPartyWindow,
                                                   nullptr, aURI)) {
    return true;
  }

  nsCOMPtr<nsIPrincipal> parentPrincipal =
    nsGlobalWindowInner::Cast(aFirstPartyWindow)->GetPrincipal();
  if (NS_WARN_IF(!parentPrincipal)) {
    return false;
  }

  nsAutoString origin;
  nsresult rv = nsContentUtils::GetUTFOrigin(aURI, origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  NS_ConvertUTF16toUTF8 utf8Origin(origin);

  nsAutoCString type;
  CreatePermissionKey(utf8Origin, utf8Origin, type);

  nsCOMPtr<nsIPermissionManager> pm = services::GetPermissionManager();
  if (NS_WARN_IF(!pm)) {
    return false;
  }

  uint32_t result = 0;
  rv = pm->TestPermissionFromPrincipal(parentPrincipal, type.get(), &result);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  return result == nsIPermissionManager::ALLOW_ACTION;
}
