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
#include "nsICookiePermission.h"
#include "nsICookieService.h"
#include "nsIPermissionManager.h"
#include "nsIPrincipal.h"
#include "nsIURI.h"
#include "nsPIDOMWindow.h"
#include "nsScriptSecurityManager.h"
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

// This internal method returns ACCESS_DENY if the access is denied,
// ACCESS_DEFAULT if unknown, some other access code if granted.
nsCookieAccess
CheckCookiePermissionForPrincipal(nsIPrincipal* aPrincipal)
{
  nsCookieAccess access = nsICookiePermission::ACCESS_DEFAULT;
  if (!aPrincipal->GetIsCodebasePrincipal()) {
    return access;
  }

  nsCOMPtr<nsICookiePermission> cps =
    do_GetService(NS_COOKIEPERMISSION_CONTRACTID);
  if (NS_WARN_IF(!cps)) {
    return access;
  }

  nsresult rv = cps->CanAccess(aPrincipal, &access);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return access;
  }

  // If we have a custom cookie permission, let's use it.
  return access;
}

int32_t
CookiesBehavior(nsIPrincipal* aPrincipal)
{
  // WebExtensions principals always get BEHAVIOR_ACCEPT as cookieBehavior
  // (See Bug 1406675 for rationale).
  if (BasePrincipal::Cast(aPrincipal)->AddonPolicy()) {
    return nsICookieService::BEHAVIOR_ACCEPT;
  }

  return StaticPrefs::network_cookie_cookieBehavior();
}

} // anonymous

/* static */ RefPtr<AntiTrackingCommon::StorageAccessGrantPromise>
AntiTrackingCommon::AddFirstPartyStorageAccessGrantedFor(const nsAString& aOrigin,
                                                         nsPIDOMWindowInner* aParentWindow)
{
  MOZ_ASSERT(aParentWindow);

  if (StaticPrefs::network_cookie_cookieBehavior() !=
        nsICookieService::BEHAVIOR_REJECT_TRACKER) {
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
AntiTrackingCommon::IsFirstPartyStorageAccessGrantedFor(nsPIDOMWindowInner* aWindow,
                                                        nsIURI* aURI)
{
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aURI);

  nsGlobalWindowInner* innerWindow = nsGlobalWindowInner::Cast(aWindow);
  nsIPrincipal* toplevelPrincipal = innerWindow->GetTopLevelPrincipal();
  if (!toplevelPrincipal) {
    // We are already the top-level principal. Let's use the window's principal.
    toplevelPrincipal = innerWindow->GetPrincipal();
  }

  if (!toplevelPrincipal) {
    // This should not be possible, right?
    return false;
  }

  nsCookieAccess access = CheckCookiePermissionForPrincipal(toplevelPrincipal);
  if (access != nsICookiePermission::ACCESS_DEFAULT) {
    return access != nsICookiePermission::ACCESS_DENY;
  }

  int32_t behavior = CookiesBehavior(toplevelPrincipal);
  if (behavior == nsICookieService::BEHAVIOR_ACCEPT) {
    return true;
  }

  if (behavior == nsICookieService::BEHAVIOR_REJECT) {
    return false;
  }

  // Let's check if this is a 3rd party context.
  if (!nsContentUtils::IsThirdPartyWindowOrChannel(aWindow, nullptr, aURI)) {
    return true;
  }

  if (behavior == nsICookieService::BEHAVIOR_REJECT_FOREIGN ||
      behavior == nsICookieService::BEHAVIOR_LIMIT_FOREIGN) {
    // XXX For non-cookie forms of storage, we handle BEHAVIOR_LIMIT_FOREIGN by
    // simply rejecting the request to use the storage. In the future, if we
    // change the meaning of BEHAVIOR_LIMIT_FOREIGN to be one which makes sense
    // for non-cookie storage types, this may change.
    return false;
  }

  MOZ_ASSERT(behavior == nsICookieService::BEHAVIOR_REJECT_TRACKER);
  if (!nsContentUtils::IsTrackingResourceWindow(aWindow)) {
    return true;
  }

  nsCOMPtr<nsIPrincipal> parentPrincipal;
  nsAutoCString trackingOrigin;
  if (!GetParentPrincipalAndTrackingOrigin(nsGlobalWindowInner::Cast(aWindow),
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

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->GetLoadInfo();
  if (!loadInfo) {
    return true;
  }

  // We need to find the correct principal to check the cookie permission. For
  // third-party contexts, we want to check if the top-level window has a custom
  // cookie permission.
  nsIPrincipal* toplevelPrincipal = loadInfo->TopLevelPrincipal();

  // If this is already the top-level window, we should use the loading
  // principal.
  if (!toplevelPrincipal) {
    toplevelPrincipal = loadInfo->LoadingPrincipal();
  }

  nsCOMPtr<nsIPrincipal> channelPrincipal;
  nsIScriptSecurityManager* ssm = nsScriptSecurityManager::GetScriptSecurityManager();
  nsresult rv = ssm->GetChannelResultPrincipal(aChannel,
                                               getter_AddRefs(channelPrincipal));

  // If we don't have a loading principal and this is a document channel, we are
  // a top-level window!
  if (!toplevelPrincipal) {
    bool isDocument = false;
    nsresult rv2 = aChannel->GetIsMainDocumentChannel(&isDocument);
    if (NS_SUCCEEDED(rv) && NS_SUCCEEDED(rv2) && isDocument) {
      toplevelPrincipal = channelPrincipal;
    }
  }

  // Let's use the triggering principal then.
  if (!toplevelPrincipal) {
    toplevelPrincipal = loadInfo->TriggeringPrincipal();
  }

  if (NS_WARN_IF(!toplevelPrincipal)) {
    return false;
  }

  nsCookieAccess access = CheckCookiePermissionForPrincipal(toplevelPrincipal);
  if (access != nsICookiePermission::ACCESS_DEFAULT) {
    return access != nsICookiePermission::ACCESS_DENY;
  }

  if (NS_WARN_IF(NS_FAILED(rv) || !channelPrincipal)) {
    return false;
  }

  int32_t behavior = CookiesBehavior(channelPrincipal);
  if (behavior == nsICookieService::BEHAVIOR_ACCEPT) {
    return true;
  }

  if (behavior == nsICookieService::BEHAVIOR_REJECT) {
    return false;
  }

  nsCOMPtr<mozIThirdPartyUtil> thirdPartyUtil = services::GetThirdPartyUtil();
  if (!thirdPartyUtil) {
    return true;
  }

  bool thirdParty = false;
  Unused << thirdPartyUtil->IsThirdPartyChannel(aChannel,
                                                nullptr,
                                                &thirdParty);
  // Grant if it's not a 3rd party.
  if (!thirdParty) {
    return true;
  }

  if (behavior == nsICookieService::BEHAVIOR_REJECT_FOREIGN ||
      behavior == nsICookieService::BEHAVIOR_LIMIT_FOREIGN) {
    // XXX For non-cookie forms of storage, we handle BEHAVIOR_LIMIT_FOREIGN by
    // simply rejecting the request to use the storage. In the future, if we
    // change the meaning of BEHAVIOR_LIMIT_FOREIGN to be one which makes sense
    // for non-cookie storage types, this may change.
    return false;
  }

  MOZ_ASSERT(behavior == nsICookieService::BEHAVIOR_REJECT_TRACKER);

  nsIPrincipal* parentPrincipal = loadInfo->TopLevelStorageAreaPrincipal();
  if (!parentPrincipal) {
    // parentPrincipal can be null if the parent window is not the top-level
    // window.
    if (loadInfo->TopLevelPrincipal()) {
      return false;
    }

    parentPrincipal = loadInfo->TriggeringPrincipal();
    if (NS_WARN_IF(!parentPrincipal)) {
      // Why we are here?!?
      return true;
    }
  }

  // Not a tracker.
  if (!aChannel->GetIsTrackingResource()) {
    return true;
  }

  // Let's see if we have to grant the access for this particular channel.

  nsCOMPtr<nsIURI> trackingURI;
  rv = aChannel->GetURI(getter_AddRefs(trackingURI));
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

bool
AntiTrackingCommon::IsFirstPartyStorageAccessGrantedFor(nsIPrincipal* aPrincipal)
{
  MOZ_ASSERT(aPrincipal);

  nsCookieAccess access = CheckCookiePermissionForPrincipal(aPrincipal);
  if (access != nsICookiePermission::ACCESS_DEFAULT) {
    return access != nsICookiePermission::ACCESS_DENY;
  }

  int32_t behavior = CookiesBehavior(aPrincipal);
  return behavior != nsICookieService::BEHAVIOR_REJECT;
}

/* static */ bool
AntiTrackingCommon::MaybeIsFirstPartyStorageAccessGrantedFor(nsPIDOMWindowInner* aFirstPartyWindow,
                                                             nsIURI* aURI)
{
  MOZ_ASSERT(aFirstPartyWindow);
  MOZ_ASSERT(!nsContentUtils::IsTrackingResourceWindow(aFirstPartyWindow));
  MOZ_ASSERT(aURI);

  if (StaticPrefs::network_cookie_cookieBehavior() !=
        nsICookieService::BEHAVIOR_REJECT_TRACKER) {
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

  nsCookieAccess access = CheckCookiePermissionForPrincipal(parentPrincipal);
  if (access != nsICookiePermission::ACCESS_DEFAULT) {
    return access != nsICookiePermission::ACCESS_DENY;
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
