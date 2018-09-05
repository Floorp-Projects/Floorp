/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AntiTrackingCommon.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/ipc/MessageChannel.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Logging.h"
#include "mozilla/StaticPrefs.h"
#include "mozIThirdPartyUtil.h"
#include "nsContentUtils.h"
#include "nsGlobalWindowInner.h"
#include "nsICookiePermission.h"
#include "nsICookieService.h"
#include "nsIHttpChannelInternal.h"
#include "nsIIOService.h"
#include "nsIParentChannel.h"
#include "nsIPermissionManager.h"
#include "nsIPrincipal.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsIWebProgressListener.h"
#include "nsNetUtil.h"
#include "nsPIDOMWindow.h"
#include "nsScriptSecurityManager.h"
#include "prtime.h"

#define ANTITRACKING_PERM_KEY "3rdPartyStorage"

using namespace mozilla;
using mozilla::dom::ContentChild;

static LazyLogModule gAntiTrackingLog("AntiTracking");
static const nsCString::size_type sMaxSpecLength = 128;

#define LOG(format) MOZ_LOG(gAntiTrackingLog, mozilla::LogLevel::Debug, format)

#define LOG_SPEC(format, uri)                                                 \
  PR_BEGIN_MACRO                                                              \
    if (MOZ_LOG_TEST(gAntiTrackingLog, mozilla::LogLevel::Debug)) {           \
      nsAutoCString _specStr(NS_LITERAL_CSTRING("(null)"));                   \
      _specStr.Truncate(std::min(_specStr.Length(), sMaxSpecLength));         \
      if (uri) {                                                              \
        _specStr = uri->GetSpecOrDefault();                                   \
      }                                                                       \
      const char* _spec = _specStr.get();                                     \
      LOG(format);                                                            \
    }                                                                         \
  PR_END_MACRO

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

bool
CheckContentBlockingAllowList(nsIURI* aTopWinURI)
{
  bool isAllowed = false;
  nsresult rv =
    AntiTrackingCommon::IsOnContentBlockingAllowList(aTopWinURI, isAllowed);
  if (NS_SUCCEEDED(rv) && isAllowed) {
    LOG_SPEC(("The top-level window (%s) is on the content blocking allow list, "
              "bail out early", _spec), aTopWinURI);
    return true;
  }
  if (NS_FAILED(rv)) {
    LOG_SPEC(("Checking the content blocking allow list for %s failed with %" PRIx32,
              _spec, static_cast<uint32_t>(rv)), aTopWinURI);
  }
  return false;
}

bool
CheckContentBlockingAllowList(nsPIDOMWindowInner* aWindow)
{
  nsPIDOMWindowOuter* top = aWindow->GetScriptableTop();
  if (top) {
    nsIURI* topWinURI = top->GetDocumentURI();
    return CheckContentBlockingAllowList(topWinURI);
  }

  LOG(("Could not check the content blocking allow list because the top "
       "window wasn't accessible"));
  return false;
}

bool
CheckContentBlockingAllowList(nsIHttpChannel* aChannel)
{
  nsCOMPtr<nsIHttpChannelInternal> chan = do_QueryInterface(aChannel);
  if (chan) {
    nsCOMPtr<nsIURI> topWinURI;
    nsresult rv = chan->GetTopWindowURI(getter_AddRefs(topWinURI));
    if (NS_SUCCEEDED(rv)) {
      return CheckContentBlockingAllowList(topWinURI);
    }
  }

  LOG(("Could not check the content blocking allow list because the top "
       "window wasn't accessible"));
  return false;
}

} // anonymous

/* static */ RefPtr<AntiTrackingCommon::StorageAccessGrantPromise>
AntiTrackingCommon::AddFirstPartyStorageAccessGrantedFor(const nsAString& aOrigin,
                                                         nsPIDOMWindowInner* aParentWindow)
{
  MOZ_ASSERT(aParentWindow);

  LOG(("Adding a first-party storage exception for %s...",
       NS_ConvertUTF16toUTF8(aOrigin).get()));

  if (StaticPrefs::network_cookie_cookieBehavior() !=
        nsICookieService::BEHAVIOR_REJECT_TRACKER) {
    LOG(("Disabled by network.cookie.cookieBehavior pref (%d), bailing out early",
         StaticPrefs::network_cookie_cookieBehavior()));
    return StorageAccessGrantPromise::CreateAndResolve(true, __func__);
  }

  if (!StaticPrefs::browser_contentblocking_enabled()) {
    LOG(("The content blocking pref has been disabled, bail out early"));
    return StorageAccessGrantPromise::CreateAndResolve(true, __func__);
  }

  if (CheckContentBlockingAllowList(aParentWindow)) {
    return StorageAccessGrantPromise::CreateAndResolve(true, __func__);
  }

  nsCOMPtr<nsIPrincipal> topLevelStoragePrincipal;
  nsAutoCString trackingOrigin;

  nsGlobalWindowInner* parentWindow = nsGlobalWindowInner::Cast(aParentWindow);
  nsGlobalWindowOuter* outerParentWindow =
    nsGlobalWindowOuter::Cast(parentWindow->GetOuterWindow());
  if (NS_WARN_IF(!outerParentWindow)) {
    LOG(("No outer window found for our parent window, bailing out early"));
    return StorageAccessGrantPromise::CreateAndReject(false, __func__);
  }

  LOG(("The current resource is %s-party",
       outerParentWindow->IsTopLevelWindow() ? "first" : "third"));

  // We are a first party resource.
  if (outerParentWindow->IsTopLevelWindow()) {
    CopyUTF16toUTF8(aOrigin, trackingOrigin);
    topLevelStoragePrincipal = parentWindow->GetPrincipal();
    if (NS_WARN_IF(!topLevelStoragePrincipal)) {
      LOG(("Top-level storage area principal not found, bailing out early"));
      return StorageAccessGrantPromise::CreateAndReject(false, __func__);
    }

  // We are a 3rd party source.
  } else if (!GetParentPrincipalAndTrackingOrigin(parentWindow,
                                                  getter_AddRefs(topLevelStoragePrincipal),
                                                  trackingOrigin)) {
    LOG(("Error while computing the parent principal and tracking origin, bailing out early"));
    return StorageAccessGrantPromise::CreateAndReject(false, __func__);
  }

  NS_ConvertUTF16toUTF8 grantedOrigin(aOrigin);

  if (XRE_IsParentProcess()) {
    LOG(("Saving the permission: trackingOrigin=%s, grantedOrigin=%s",
         trackingOrigin.get(), grantedOrigin.get()));

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

  LOG(("Asking the parent process to save the permission for us: trackingOrigin=%s, grantedOrigin=%s",
       trackingOrigin.get(), grantedOrigin.get()));

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

  nsCOMPtr<nsIURI> parentPrincipalURI;
  Unused << aParentPrincipal->GetURI(getter_AddRefs(parentPrincipalURI));
  LOG_SPEC(("Saving a first-party storage permission on %s for trackingOrigin=%s grantedOrigin=%s",
            _spec, aTrackingOrigin.get(), aGrantedOrigin.get()), parentPrincipalURI);

  if (NS_WARN_IF(!aParentPrincipal)) {
    // The child process is sending something wrong. Let's ignore it.
    LOG(("aParentPrincipal is null, bailing out early"));
    aResolver(false);
    return;
  }

  nsCOMPtr<nsIPermissionManager> pm = services::GetPermissionManager();
  if (NS_WARN_IF(!pm)) {
    LOG(("Permission manager is null, bailing out early"));
    aResolver(false);
    return;
  }

  // Remember that this pref is stored in seconds!
  uint32_t expirationType = nsIPermissionManager::EXPIRE_TIME;
  uint32_t expirationTime =
    StaticPrefs::privacy_restrict3rdpartystorage_expiration() * 1000;
  int64_t when = (PR_Now() / PR_USEC_PER_MSEC) + expirationTime;

  uint32_t privateBrowsingId = 0;
  nsresult rv = aParentPrincipal->GetPrivateBrowsingId(&privateBrowsingId);
  if (!NS_WARN_IF(NS_FAILED(rv)) && privateBrowsingId > 0) {
    // If we are coming from a private window, make sure to store a session-only
    // permission which won't get persisted to disk.
    expirationType = nsIPermissionManager::EXPIRE_SESSION;
    when = 0;
  }

  nsAutoCString type;
  CreatePermissionKey(aTrackingOrigin, aGrantedOrigin, type);

  LOG(("Computed permission key: %s, expiry: %d, proceeding to save in the permission manager",
       type.get(), expirationTime));

  rv = pm->AddFromPrincipal(aParentPrincipal, type.get(),
                            nsIPermissionManager::ALLOW_ACTION,
                            expirationType, when);
  Unused << NS_WARN_IF(NS_FAILED(rv));
  aResolver(NS_SUCCEEDED(rv));

  LOG(("Result: %s", NS_SUCCEEDED(rv) ? "success" : "failure"));
}

bool
AntiTrackingCommon::IsFirstPartyStorageAccessGrantedFor(nsPIDOMWindowInner* aWindow,
                                                        nsIURI* aURI,
                                                        uint32_t* aRejectedReason)
{
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aURI);

  // Let's avoid a null check on aRejectedReason everywhere else.
  uint32_t rejectedReason = 0;
  if (!aRejectedReason) {
    aRejectedReason = &rejectedReason;
  }

  LOG_SPEC(("Computing whether window %p has access to URI %s", aWindow, _spec), aURI);

  nsGlobalWindowInner* innerWindow = nsGlobalWindowInner::Cast(aWindow);
  nsIPrincipal* toplevelPrincipal = innerWindow->GetTopLevelPrincipal();
  if (!toplevelPrincipal) {
    // We are already the top-level principal. Let's use the window's principal.
    LOG(("Our inner window lacks a top-level principal, use the window's principal instead"));
    toplevelPrincipal = innerWindow->GetPrincipal();
  }

  if (!toplevelPrincipal) {
    // This should not be possible, right?
    LOG(("No top-level principal, this shouldn't be happening! Bail out early"));
    return false;
  }

  nsCookieAccess access = CheckCookiePermissionForPrincipal(toplevelPrincipal);
  if (access != nsICookiePermission::ACCESS_DEFAULT) {
    LOG(("CheckCookiePermissionForPrincipal() returned a non-default access code (%d), returning %s",
         int(access), access != nsICookiePermission::ACCESS_DENY ?
                        "success" : "failure"));
    if (access != nsICookiePermission::ACCESS_DENY) {
      return true;
    }

    *aRejectedReason = nsIWebProgressListener::STATE_COOKIES_BLOCKED_BY_PERMISSION;
    return false;
  }

  int32_t behavior = CookiesBehavior(toplevelPrincipal);
  if (behavior == nsICookieService::BEHAVIOR_ACCEPT) {
    LOG(("The cookie behavior pref mandates accepting all cookies!"));
    return true;
  }

  if (behavior == nsICookieService::BEHAVIOR_REJECT) {
    LOG(("The cookie behavior pref mandates rejecting all cookies!"));
    *aRejectedReason = nsIWebProgressListener::STATE_COOKIES_BLOCKED_ALL;
    return false;
  }

  // Let's check if this is a 3rd party context.
  if (!nsContentUtils::IsThirdPartyWindowOrChannel(aWindow, nullptr, aURI)) {
    LOG(("Our window isn't a third-party window"));
    return true;
  }

  if (behavior == nsICookieService::BEHAVIOR_REJECT_FOREIGN) {
    // Now, we have to also honour the Content Blocking pref.
    if (!StaticPrefs::browser_contentblocking_enabled()) {
      LOG(("The content blocking pref has been disabled, bail out early by "
           "by pretending our window isn't a third-party window"));
      return true;
    }

    if (CheckContentBlockingAllowList(aWindow)) {
      LOG(("Allowing access even though our behavior is reject foreign"));
      return true;
    }
  }

  if (behavior == nsICookieService::BEHAVIOR_REJECT_FOREIGN ||
      behavior == nsICookieService::BEHAVIOR_LIMIT_FOREIGN) {
    // XXX For non-cookie forms of storage, we handle BEHAVIOR_LIMIT_FOREIGN by
    // simply rejecting the request to use the storage. In the future, if we
    // change the meaning of BEHAVIOR_LIMIT_FOREIGN to be one which makes sense
    // for non-cookie storage types, this may change.
    LOG(("Nothing more to do due to the behavior code %d", int(behavior)));
    *aRejectedReason = nsIWebProgressListener::STATE_COOKIES_BLOCKED_FOREIGN;
    return false;
  }

  MOZ_ASSERT(behavior == nsICookieService::BEHAVIOR_REJECT_TRACKER);

  // Now, we have to also honour the Content Blocking pref.
  if (!StaticPrefs::browser_contentblocking_enabled()) {
    LOG(("The content blocking pref has been disabled, bail out early by "
         "by pretending our window isn't a tracking window"));
    return true;
  }

  if (CheckContentBlockingAllowList(aWindow)) {
    return true;
  }

  if (!nsContentUtils::IsTrackingResourceWindow(aWindow)) {
    LOG(("Our window isn't a tracking window"));
    return true;
  }

  nsCOMPtr<nsIPrincipal> parentPrincipal;
  nsAutoCString trackingOrigin;
  if (!GetParentPrincipalAndTrackingOrigin(nsGlobalWindowInner::Cast(aWindow),
                                           getter_AddRefs(parentPrincipal),
                                           trackingOrigin)) {
    LOG(("Failed to obtain the parent principal and the tracking origin"));
    return false;
  }

  nsAutoString origin;
  nsresult rv = nsContentUtils::GetUTFOrigin(aURI, origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG_SPEC(("Failed to compute the origin from %s", _spec), aURI);
    return false;
  }

  nsAutoCString type;
  CreatePermissionKey(trackingOrigin, NS_ConvertUTF16toUTF8(origin), type);

  nsCOMPtr<nsIPermissionManager> pm = services::GetPermissionManager();
  if (NS_WARN_IF(!pm)) {
    LOG(("Failed to obtain the permission manager"));
    return false;
  }

  uint32_t result = 0;
  rv = pm->TestPermissionFromPrincipal(parentPrincipal, type.get(), &result);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG(("Failed to test the permission"));
    return false;
  }

  nsCOMPtr<nsIURI> parentPrincipalURI;
  Unused << parentPrincipal->GetURI(getter_AddRefs(parentPrincipalURI));
  LOG_SPEC(("Testing permission type %s for %s resulted in %d (%s)",
            type.get(), _spec, int(result),
            result == nsIPermissionManager::ALLOW_ACTION ?
              "success" : "failure"), parentPrincipalURI);

  if (result != nsIPermissionManager::ALLOW_ACTION) {
    *aRejectedReason = nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER;
    return false;
  }

  return true;
}

bool
AntiTrackingCommon::IsFirstPartyStorageAccessGrantedFor(nsIHttpChannel* aChannel,
                                                        nsIURI* aURI,
                                                        uint32_t* aRejectedReason)
{
  MOZ_ASSERT(aURI);
  MOZ_ASSERT(aChannel);

  // Let's avoid a null check on aRejectedReason everywhere else.
  uint32_t rejectedReason = 0;
  if (!aRejectedReason) {
    aRejectedReason = &rejectedReason;
  }

  nsCOMPtr<nsIURI> channelURI;
  Unused << aChannel->GetURI(getter_AddRefs(channelURI));
  LOG_SPEC(("Computing whether channel %p has access to URI %s", aChannel, _spec),
           channelURI);

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->GetLoadInfo();
  if (!loadInfo) {
    LOG(("No loadInfo, bail out early"));
    return true;
  }

  // We need to find the correct principal to check the cookie permission. For
  // third-party contexts, we want to check if the top-level window has a custom
  // cookie permission.
  nsIPrincipal* toplevelPrincipal = loadInfo->TopLevelPrincipal();

  // If this is already the top-level window, we should use the loading
  // principal.
  if (!toplevelPrincipal) {
    LOG(("Our loadInfo lacks a top-level principal, use the loadInfo's loading principal instead"));
    toplevelPrincipal = loadInfo->LoadingPrincipal();
  }

  nsCOMPtr<nsIPrincipal> channelPrincipal;
  nsIScriptSecurityManager* ssm = nsScriptSecurityManager::GetScriptSecurityManager();
  nsresult rv = ssm->GetChannelResultPrincipal(aChannel,
                                               getter_AddRefs(channelPrincipal));

  // If we don't have a loading principal and this is a document channel, we are
  // a top-level window!
  if (!toplevelPrincipal) {
    LOG(("We don't have a loading principal, let's see if this is a document channel"
         " that belongs to a top-level window"));
    bool isDocument = false;
    nsresult rv2 = aChannel->GetIsMainDocumentChannel(&isDocument);
    if (NS_SUCCEEDED(rv) && NS_SUCCEEDED(rv2) && isDocument) {
      toplevelPrincipal = channelPrincipal;
      LOG(("Yes, we guessed right!"));
    } else {
      LOG(("No, we guessed wrong!"));
    }
  }

  // Let's use the triggering principal then.
  if (!toplevelPrincipal) {
    LOG(("Our loadInfo lacks a top-level principal, use the loadInfo's triggering principal instead"));
    toplevelPrincipal = loadInfo->TriggeringPrincipal();
  }

  if (NS_WARN_IF(!toplevelPrincipal)) {
    LOG(("No top-level principal! Bail out early"));
    return false;
  }

  nsCookieAccess access = CheckCookiePermissionForPrincipal(toplevelPrincipal);
  if (access != nsICookiePermission::ACCESS_DEFAULT) {
    LOG(("CheckCookiePermissionForPrincipal() returned a non-default access code (%d), returning %s",
         int(access), access != nsICookiePermission::ACCESS_DENY ?
                        "success" : "failure"));
    if (access != nsICookiePermission::ACCESS_DENY) {
      return true;
    }

    *aRejectedReason = nsIWebProgressListener::STATE_COOKIES_BLOCKED_BY_PERMISSION;
    return false;
  }

  if (NS_WARN_IF(NS_FAILED(rv) || !channelPrincipal)) {
    LOG(("No channel principal, bail out early"));
    return false;
  }

  int32_t behavior = CookiesBehavior(toplevelPrincipal);
  if (behavior == nsICookieService::BEHAVIOR_ACCEPT) {
    LOG(("The cookie behavior pref mandates accepting all cookies!"));
    return true;
  }

  if (behavior == nsICookieService::BEHAVIOR_REJECT) {
    LOG(("The cookie behavior pref mandates rejecting all cookies!"));
    *aRejectedReason = nsIWebProgressListener::STATE_COOKIES_BLOCKED_ALL;
    return false;
  }

  nsCOMPtr<mozIThirdPartyUtil> thirdPartyUtil = services::GetThirdPartyUtil();
  if (!thirdPartyUtil) {
    LOG(("No thirdPartyUtil, bail out early"));
    return true;
  }

  bool thirdParty = false;
  rv = thirdPartyUtil->IsThirdPartyChannel(aChannel,
                                           aURI,
                                           &thirdParty);
  // Grant if it's not a 3rd party.
  // Be careful to check the return value of IsThirdPartyChannel, since
  // IsThirdPartyChannel() will fail if the channel's loading principal is the
  // system principal...
  if (NS_SUCCEEDED(rv) && !thirdParty) {
    LOG(("Our channel isn't a third-party channel"));
    return true;
  }

  if (behavior == nsICookieService::BEHAVIOR_REJECT_FOREIGN) {
    // Now, we have to also honour the Content Blocking pref.
    if (!StaticPrefs::browser_contentblocking_enabled()) {
      LOG(("The content blocking pref has been disabled, bail out early by "
           "by pretending our window isn't a third-party window"));
      return true;
    }

    if (CheckContentBlockingAllowList(aChannel)) {
      LOG(("Allowing access even though our behavior is reject foreign"));
      return true;
    }
  }

  if (behavior == nsICookieService::BEHAVIOR_REJECT_FOREIGN ||
      behavior == nsICookieService::BEHAVIOR_LIMIT_FOREIGN) {
    // XXX For non-cookie forms of storage, we handle BEHAVIOR_LIMIT_FOREIGN by
    // simply rejecting the request to use the storage. In the future, if we
    // change the meaning of BEHAVIOR_LIMIT_FOREIGN to be one which makes sense
    // for non-cookie storage types, this may change.
    LOG(("Nothing more to do due to the behavior code %d", int(behavior)));
    *aRejectedReason = nsIWebProgressListener::STATE_COOKIES_BLOCKED_FOREIGN;
    return false;
  }

  MOZ_ASSERT(behavior == nsICookieService::BEHAVIOR_REJECT_TRACKER);

  // Now, we have to also honour the Content Blocking pref.
  if (!StaticPrefs::browser_contentblocking_enabled()) {
    LOG(("The content blocking pref has been disabled, bail out early by "
         "pretending our channel isn't a tracking channel"));
    return true;
  }

  if (CheckContentBlockingAllowList(aChannel)) {
    return true;
  }

  nsIPrincipal* parentPrincipal = loadInfo->TopLevelStorageAreaPrincipal();
  if (!parentPrincipal) {
    LOG(("No top-level storage area principal at hand"));

    // parentPrincipal can be null if the parent window is not the top-level
    // window.
    if (loadInfo->TopLevelPrincipal()) {
      LOG(("Parent window is the top-level window, bail out early"));
      return false;
    }

    parentPrincipal = toplevelPrincipal;
    if (NS_WARN_IF(!parentPrincipal)) {
      LOG(("No triggering principal, this shouldn't be happening! Bail out early"));
      // Why we are here?!?
      return true;
    }
  }

  // Not a tracker.
  if (!aChannel->GetIsTrackingResource()) {
    LOG(("Our channel isn't a tracking channel"));
    return true;
  }

  // Let's see if we have to grant the access for this particular channel.

  nsCOMPtr<nsIURI> trackingURI;
  rv = aChannel->GetURI(getter_AddRefs(trackingURI));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG(("Failed to get the channel URI"));
    return true;
  }

  nsAutoString trackingOrigin;
  rv = nsContentUtils::GetUTFOrigin(trackingURI, trackingOrigin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG_SPEC(("Failed to compute the origin from %s", _spec), trackingURI);
    return false;
  }

  nsAutoString origin;
  rv = nsContentUtils::GetUTFOrigin(aURI, origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG_SPEC(("Failed to compute the origin from %s", _spec), aURI);
    return false;
  }

  nsAutoCString type;
  CreatePermissionKey(NS_ConvertUTF16toUTF8(trackingOrigin),
                      NS_ConvertUTF16toUTF8(origin), type);

  nsCOMPtr<nsIPermissionManager> pm = services::GetPermissionManager();
  if (NS_WARN_IF(!pm)) {
    LOG(("Failed to obtain the permission manager"));
    return false;
  }

  uint32_t result = 0;
  rv = pm->TestPermissionFromPrincipal(parentPrincipal, type.get(), &result);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG(("Failed to test the permission"));
    return false;
  }

  nsCOMPtr<nsIURI> parentPrincipalURI;
  Unused << parentPrincipal->GetURI(getter_AddRefs(parentPrincipalURI));
  LOG_SPEC(("Testing permission type %s for %s resulted in %d (%s)",
            type.get(), _spec, int(result),
            result == nsIPermissionManager::ALLOW_ACTION ?
              "success" : "failure"), parentPrincipalURI);

  if (result != nsIPermissionManager::ALLOW_ACTION) {
    *aRejectedReason = nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER;
    return false;
  }

  return true;
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

  LOG_SPEC(("Computing a best guess as to whether window %p has access to URI %s",
            aFirstPartyWindow, _spec), aURI);

  if (StaticPrefs::network_cookie_cookieBehavior() !=
        nsICookieService::BEHAVIOR_REJECT_TRACKER) {
    LOG(("Disabled by the pref (%d), bail out early",
         StaticPrefs::network_cookie_cookieBehavior()));
    return true;
  }

  // Now, we have to also honour the Content Blocking pref.
  if (!StaticPrefs::browser_contentblocking_enabled()) {
    LOG(("The content blocking pref has been disabled, bail out early"));
    return true;
  }

  if (CheckContentBlockingAllowList(aFirstPartyWindow)) {
    return true;
  }

  if (!nsContentUtils::IsThirdPartyWindowOrChannel(aFirstPartyWindow,
                                                   nullptr, aURI)) {
    LOG(("Our window isn't a third-party window"));
    return true;
  }

  nsCOMPtr<nsIPrincipal> parentPrincipal =
    nsGlobalWindowInner::Cast(aFirstPartyWindow)->GetPrincipal();
  if (NS_WARN_IF(!parentPrincipal)) {
    LOG(("Failed to get the first party window's principal"));
    return false;
  }

  nsCookieAccess access = CheckCookiePermissionForPrincipal(parentPrincipal);
  if (access != nsICookiePermission::ACCESS_DEFAULT) {
    LOG(("CheckCookiePermissionForPrincipal() returned a non-default access code (%d), returning %s",
         int(access), access != nsICookiePermission::ACCESS_DENY ?
                        "success" : "failure"));
    return access != nsICookiePermission::ACCESS_DENY;
  }

  nsAutoString origin;
  nsresult rv = nsContentUtils::GetUTFOrigin(aURI, origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG_SPEC(("Failed to compute the origin from %s", _spec), aURI);
    return false;
  }

  NS_ConvertUTF16toUTF8 utf8Origin(origin);

  nsAutoCString type;
  CreatePermissionKey(utf8Origin, utf8Origin, type);

  nsCOMPtr<nsIPermissionManager> pm = services::GetPermissionManager();
  if (NS_WARN_IF(!pm)) {
    LOG(("Failed to obtain the permission manager"));
    return false;
  }

  uint32_t result = 0;
  rv = pm->TestPermissionFromPrincipal(parentPrincipal, type.get(), &result);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG(("Failed to test the permission"));
    return false;
  }

  nsCOMPtr<nsIURI> parentPrincipalURI;
  Unused << parentPrincipal->GetURI(getter_AddRefs(parentPrincipalURI));
  LOG_SPEC(("Testing permission type %s for %s resulted in %d (%s)",
            type.get(), _spec, int(result),
            result == nsIPermissionManager::ALLOW_ACTION ?
              "success" : "failure"), parentPrincipalURI);

  return result == nsIPermissionManager::ALLOW_ACTION;
}

nsresult
AntiTrackingCommon::IsOnContentBlockingAllowList(nsIURI* aTopWinURI,
                                                 bool& aIsAllowListed)
{
  aIsAllowListed = false;

  LOG_SPEC(("Deciding whether the user has overridden content blocking for %s",
            _spec), aTopWinURI);

  nsCOMPtr<nsIIOService> ios = services::GetIOService();
  NS_ENSURE_TRUE(ios, NS_ERROR_FAILURE);

  // Take the host/port portion so we can allowlist by site. Also ignore the
  // scheme, since users who put sites on the allowlist probably don't expect
  // allowlisting to depend on scheme.
  nsresult rv = NS_ERROR_FAILURE;
  nsCOMPtr<nsIURL> url = do_QueryInterface(aTopWinURI, &rv);
  if (NS_FAILED(rv)) {
    return rv; // normal for some loads, no need to print a warning
  }

  nsCString escaped(NS_LITERAL_CSTRING("https://"));
  nsAutoCString temp;
  rv = url->GetHostPort(temp);
  NS_ENSURE_SUCCESS(rv, rv);
  escaped.Append(temp);

  // Stuff the whole thing back into a URI for the permission manager.
  nsCOMPtr<nsIURI> topWinURI;
  rv = ios->NewURI(escaped, nullptr, nullptr, getter_AddRefs(topWinURI));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPermissionManager> permMgr =
    do_GetService(NS_PERMISSIONMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Check both the normal mode and private browsing mode user override permissions.
  const char* types[] = {
    "trackingprotection",
    "trackingprotection-pb"
  };

  for (size_t i = 0; i < ArrayLength(types); ++i) {
    uint32_t permissions = nsIPermissionManager::UNKNOWN_ACTION;
    rv = permMgr->TestPermission(topWinURI, types[i], &permissions);
    NS_ENSURE_SUCCESS(rv, rv);

    if (permissions == nsIPermissionManager::ALLOW_ACTION) {
      aIsAllowListed = true;
      LOG_SPEC(("Found user override type %s for %s", types[i], _spec),
               topWinURI);
      // Stop checking the next permisson type if we decided to override.
      break;
    }
  }

  if (!aIsAllowListed) {
    LOG(("No user override found"));
  }

  return NS_OK;
}

/* static */ void
AntiTrackingCommon::NotifyRejection(nsIChannel* aChannel,
                                    uint32_t aRejectedReason)
{
  MOZ_ASSERT(aRejectedReason == nsIWebProgressListener::STATE_COOKIES_BLOCKED_BY_PERMISSION ||
             aRejectedReason == nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER ||
             aRejectedReason == nsIWebProgressListener::STATE_COOKIES_BLOCKED_ALL ||
             aRejectedReason == nsIWebProgressListener::STATE_COOKIES_BLOCKED_FOREIGN ||
             aRejectedReason == nsIWebProgressListener::STATE_BLOCKED_SLOW_TRACKING_CONTENT);

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aChannel);
  if (!httpChannel) {
    return;
  }

  // Can be called in EITHER the parent or child process.
  nsCOMPtr<nsIParentChannel> parentChannel;
  NS_QueryNotificationCallbacks(aChannel, parentChannel);
  if (parentChannel) {
    // This channel is a parent-process proxy for a child process request.
    // Tell the child process channel to do this instead.
    parentChannel->NotifyTrackingCookieBlocked(aRejectedReason);
    return;
  }

  nsCOMPtr<mozIThirdPartyUtil> thirdPartyUtil = services::GetThirdPartyUtil();
  if (!thirdPartyUtil) {
    return;
  }

  nsCOMPtr<mozIDOMWindowProxy> win;
  nsresult rv = thirdPartyUtil->GetTopWindowForChannel(httpChannel,
                                                       getter_AddRefs(win));
  NS_ENSURE_SUCCESS_VOID(rv);

  nsCOMPtr<nsPIDOMWindowOuter> pwin = nsPIDOMWindowOuter::From(win);
  if (!pwin) {
    return;
  }

  pwin->NotifyContentBlockingState(aRejectedReason, aChannel);
}

/* static */ void
AntiTrackingCommon::NotifyRejection(nsPIDOMWindowInner* aWindow,
                                    uint32_t aRejectedReason)
{
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aRejectedReason == nsIWebProgressListener::STATE_COOKIES_BLOCKED_BY_PERMISSION ||
             aRejectedReason == nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER ||
             aRejectedReason == nsIWebProgressListener::STATE_COOKIES_BLOCKED_ALL ||
             aRejectedReason == nsIWebProgressListener::STATE_COOKIES_BLOCKED_FOREIGN ||
             aRejectedReason == nsIWebProgressListener::STATE_BLOCKED_SLOW_TRACKING_CONTENT);

  nsIDocument* document = aWindow->GetExtantDoc();
  if (!document) {
    return;
  }

  nsCOMPtr<nsIHttpChannel> httpChannel =
    do_QueryInterface(document->GetChannel());
  if (!httpChannel) {
    return;
  }

  nsCOMPtr<nsPIDOMWindowOuter> pwin;
  auto* outer = nsGlobalWindowOuter::Cast(aWindow->GetOuterWindow());
  if (outer) {
    pwin = outer->GetTopOuter();
  }

  if (pwin) {
    pwin->NotifyContentBlockingState(aRejectedReason, httpChannel);
  }
}
