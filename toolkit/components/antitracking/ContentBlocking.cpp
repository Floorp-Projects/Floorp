/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AntiTrackingLog.h"
#include "ContentBlocking.h"
#include "AntiTrackingUtils.h"
#include "TemporaryAccessGrantObserver.h"

#include "mozilla/ContentBlockingAllowList.h"
#include "mozilla/ContentBlockingUserInteraction.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/BrowsingContextGroup.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/WindowContext.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/net/CookieJarSettings.h"
#include "mozilla/PermissionManager.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "mozIThirdPartyUtil.h"
#include "nsContentUtils.h"
#include "nsGlobalWindowInner.h"
#include "nsIClassifiedChannel.h"
#include "nsICookiePermission.h"
#include "nsICookieService.h"
#include "nsIPermission.h"
#include "nsIPrincipal.h"
#include "nsIURI.h"
#include "nsIOService.h"
#include "nsIWebProgressListener.h"
#include "nsScriptSecurityManager.h"
#include "RejectForeignAllowList.h"

namespace mozilla {

LazyLogModule gAntiTrackingLog("AntiTracking");

}

using namespace mozilla;
using mozilla::dom::BrowsingContext;
using mozilla::dom::ContentChild;
using mozilla::dom::Document;
using mozilla::dom::WindowGlobalParent;
using mozilla::net::CookieJarSettings;

namespace {

bool GetTopLevelWindowId(BrowsingContext* aParentContext, uint32_t aBehavior,
                         uint64_t& aTopLevelInnerWindowId) {
  MOZ_ASSERT(aParentContext);

  aTopLevelInnerWindowId =
      (aBehavior == nsICookieService::BEHAVIOR_REJECT_TRACKER)
          ? AntiTrackingUtils::GetTopLevelStorageAreaWindowId(aParentContext)
          : AntiTrackingUtils::GetTopLevelAntiTrackingWindowId(aParentContext);
  return aTopLevelInnerWindowId != 0;
}

// This internal method returns ACCESS_DENY if the access is denied,
// ACCESS_DEFAULT if unknown, some other access code if granted.
uint32_t CheckCookiePermissionForPrincipal(
    nsICookieJarSettings* aCookieJarSettings, nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(aCookieJarSettings);
  MOZ_ASSERT(aPrincipal);

  uint32_t cookiePermission = nsICookiePermission::ACCESS_DEFAULT;
  if (!aPrincipal->GetIsContentPrincipal()) {
    return cookiePermission;
  }

  nsresult rv =
      aCookieJarSettings->CookiePermission(aPrincipal, &cookiePermission);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nsICookiePermission::ACCESS_DEFAULT;
  }

  // If we have a custom cookie permission, let's use it.
  return cookiePermission;
}

int32_t CookiesBehavior(Document* a3rdPartyDocument) {
  MOZ_ASSERT(a3rdPartyDocument);

  // WebExtensions principals always get BEHAVIOR_ACCEPT as cookieBehavior
  // (See Bug 1406675 and Bug 1525917 for rationale).
  if (BasePrincipal::Cast(a3rdPartyDocument->NodePrincipal())->AddonPolicy()) {
    return nsICookieService::BEHAVIOR_ACCEPT;
  }

  return a3rdPartyDocument->CookieJarSettings()->GetCookieBehavior();
}

int32_t CookiesBehavior(nsILoadInfo* aLoadInfo, nsIURI* a3rdPartyURI) {
  MOZ_ASSERT(aLoadInfo);
  MOZ_ASSERT(a3rdPartyURI);

  // WebExtensions 3rd party URI always get BEHAVIOR_ACCEPT as cookieBehavior,
  // this is semantically equivalent to the principal having a AddonPolicy().
  if (a3rdPartyURI->SchemeIs("moz-extension")) {
    return nsICookieService::BEHAVIOR_ACCEPT;
  }

  nsCOMPtr<nsICookieJarSettings> cookieJarSettings;
  nsresult rv =
      aLoadInfo->GetCookieJarSettings(getter_AddRefs(cookieJarSettings));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nsICookieService::BEHAVIOR_REJECT;
  }

  return cookieJarSettings->GetCookieBehavior();
}

int32_t CookiesBehavior(nsIPrincipal* aPrincipal,
                        nsICookieJarSettings* aCookieJarSettings) {
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aCookieJarSettings);

  // WebExtensions principals always get BEHAVIOR_ACCEPT as cookieBehavior
  // (See Bug 1406675 for rationale).
  if (BasePrincipal::Cast(aPrincipal)->AddonPolicy()) {
    return nsICookieService::BEHAVIOR_ACCEPT;
  }

  return aCookieJarSettings->GetCookieBehavior();
}
}  // namespace

/* static */ RefPtr<ContentBlocking::StorageAccessPermissionGrantPromise>
ContentBlocking::AllowAccessFor(
    nsIPrincipal* aPrincipal, dom::BrowsingContext* aParentContext,
    ContentBlockingNotifier::StorageAccessPermissionGrantedReason aReason,
    const ContentBlocking::PerformFinalChecks& aPerformFinalChecks) {
  MOZ_ASSERT(aParentContext);

  switch (aReason) {
    case ContentBlockingNotifier::eOpener:
      if (!StaticPrefs::
              privacy_restrict3rdpartystorage_heuristic_window_open()) {
        LOG(
            ("Bailing out early because the "
             "privacy.restrict3rdpartystorage.heuristic.window_open preference "
             "has been disabled"));
        return StorageAccessPermissionGrantPromise::CreateAndReject(false,
                                                                    __func__);
      }
      break;
    case ContentBlockingNotifier::eOpenerAfterUserInteraction:
      if (!StaticPrefs::
              privacy_restrict3rdpartystorage_heuristic_opened_window_after_interaction()) {
        LOG(
            ("Bailing out early because the "
             "privacy.restrict3rdpartystorage.heuristic.opened_window_after_"
             "interaction preference has been disabled"));
        return StorageAccessPermissionGrantPromise::CreateAndReject(false,
                                                                    __func__);
      }
      break;
    default:
      break;
  }

  if (MOZ_LOG_TEST(gAntiTrackingLog, mozilla::LogLevel::Debug)) {
    nsAutoCString origin;
    aPrincipal->GetAsciiOrigin(origin);
    LOG(("Adding a first-party storage exception for %s, triggered by %s",
         PromiseFlatCString(origin).get(),
         AntiTrackingUtils::GrantedReasonToString(aReason).get()));
  }

  RefPtr<dom::WindowContext> parentWindowContext =
      aParentContext->GetCurrentWindowContext();
  if (!parentWindowContext) {
    LOG(
        ("No window context found for our parent browsing context, bailing out "
         "early"));
    return StorageAccessPermissionGrantPromise::CreateAndReject(false,
                                                                __func__);
  }

  if (parentWindowContext->GetCookieBehavior().isNothing()) {
    LOG(
        ("No cookie behaviour found for our parent window context, bailing "
         "out early"));
    return StorageAccessPermissionGrantPromise::CreateAndReject(false,
                                                                __func__);
  }

  // Only add storage permission when there is a reason to do so.
  uint32_t behavior = *parentWindowContext->GetCookieBehavior();
  if (!CookieJarSettings::IsRejectThirdPartyContexts(behavior)) {
    LOG(
        ("Disabled by network.cookie.cookieBehavior pref (%d), bailing out "
         "early",
         behavior));
    return StorageAccessPermissionGrantPromise::CreateAndResolve(true,
                                                                 __func__);
  }

  MOZ_ASSERT(
      CookieJarSettings::IsRejectThirdPartyWithExceptions(behavior) ||
      behavior == nsICookieService::BEHAVIOR_REJECT_TRACKER ||
      behavior ==
          nsICookieService::BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN);

  // No need to continue when we are already in the allow list.
  if (parentWindowContext->GetIsOnContentBlockingAllowList()) {
    return StorageAccessPermissionGrantPromise::CreateAndResolve(true,
                                                                 __func__);
  }

  bool isParentTopLevel = aParentContext->IsTopContent();

  // Make sure storage access isn't disabled
  if (!isParentTopLevel &&
      Document::StorageAccessSandboxed(aParentContext->GetSandboxFlags())) {
    LOG(("Our document is sandboxed"));
    return StorageAccessPermissionGrantPromise::CreateAndReject(false,
                                                                __func__);
  }

  uint64_t topLevelWindowId;
  nsAutoCString trackingOrigin;
  nsCOMPtr<nsIPrincipal> trackingPrincipal;

  LOG(("The current resource is %s-party",
       isParentTopLevel ? "first" : "third"));

  // We are a first party resource.
  if (isParentTopLevel) {
    nsAutoCString origin;
    nsresult rv = aPrincipal->GetAsciiOrigin(origin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      LOG(("Can't get the origin from the URI"));
      return StorageAccessPermissionGrantPromise::CreateAndReject(false,
                                                                  __func__);
    }

    trackingOrigin = origin;
    trackingPrincipal = aPrincipal;
    topLevelWindowId = aParentContext->GetCurrentInnerWindowId();
    if (NS_WARN_IF(!topLevelWindowId)) {
      LOG(("Top-level storage area window id not found, bailing out early"));
      return StorageAccessPermissionGrantPromise::CreateAndReject(false,
                                                                  __func__);
    }

  } else {
    // We should be a 3rd party source.
    if (behavior == nsICookieService::BEHAVIOR_REJECT_TRACKER &&
        !parentWindowContext->GetIsThirdPartyTrackingResourceWindow()) {
      LOG(("Our window isn't a third-party tracking window"));
      return StorageAccessPermissionGrantPromise::CreateAndReject(false,
                                                                  __func__);
    }
    if ((CookieJarSettings::IsRejectThirdPartyWithExceptions(behavior) ||
         behavior ==
             nsICookieService::BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN) &&
        !parentWindowContext->GetIsThirdPartyWindow()) {
      LOG(("Our window isn't a third-party window"));
      return StorageAccessPermissionGrantPromise::CreateAndReject(false,
                                                                  __func__);
    }

    if (!GetTopLevelWindowId(aParentContext,
                             // Don't request the ETP specific behaviour of
                             // allowing only singly-nested iframes here,
                             // because we are recording an allow permission.
                             nsICookieService::BEHAVIOR_ACCEPT,
                             topLevelWindowId)) {
      LOG(("Error while retrieving the parent window id, bailing out early"));
      return StorageAccessPermissionGrantPromise::CreateAndReject(false,
                                                                  __func__);
    }

    // If we can't get the principal and tracking origin at this point, the
    // tracking principal will be gotten while running ::CompleteAllowAccessFor
    // in the parent.
    if (aParentContext->IsInProcess()) {
      if (!AntiTrackingUtils::GetPrincipalAndTrackingOrigin(
              aParentContext, getter_AddRefs(trackingPrincipal),
              trackingOrigin)) {
        LOG(
            ("Error while computing the parent principal and tracking origin, "
             "bailing out early"));
        return StorageAccessPermissionGrantPromise::CreateAndReject(false,
                                                                    __func__);
      }
    }
  }

  // We MAY need information that is only accessible in the parent,
  // so we need to determine whether we can run it in the current process (in
  // most of cases it should be a child process).
  //
  // If the following two cases are both true, we can continue to run in
  // the current process, otherwise, we need to ask the parent to continue
  // the work.
  // 1. aParentContext is an in-process browsing contex because we need the
  //    principal of the parent window.
  // 2. tracking origin is not third-party with respect to the parent window
  //    (aParentContext). This is because we need to test whether the user
  //    has interacted with the tracking origin before, and this info is
  //    not supposed to be seen from cross-origin processes.

  // The only case that aParentContext is not in-process is when the heuristic
  // is triggered because of user interactions.
  MOZ_ASSERT_IF(
      !aParentContext->IsInProcess(),
      aReason == ContentBlockingNotifier::eOpenerAfterUserInteraction);

  bool runInSameProcess;
  if (XRE_IsParentProcess()) {
    // If we are already in the parent, then continue run in the parent.
    runInSameProcess = true;
  } else {
    // Only continue to run in child processes when aParentContext is
    // in-process and tracking origin is not third-party with respect to
    // the parent window.
    if (aParentContext->IsInProcess()) {
      bool isThirdParty;
      nsCOMPtr<nsIPrincipal> principal =
          AntiTrackingUtils::GetPrincipal(aParentContext);
      if (!principal) {
        LOG(("Can't get the principal from the browsing context"));
        return StorageAccessPermissionGrantPromise::CreateAndReject(false,
                                                                    __func__);
      }
      Unused << trackingPrincipal->IsThirdPartyPrincipal(principal,
                                                         &isThirdParty);
      runInSameProcess = !isThirdParty;
    } else {
      runInSameProcess = false;
    }
  }

  if (runInSameProcess) {
    return ContentBlocking::CompleteAllowAccessFor(
        aParentContext, topLevelWindowId, trackingPrincipal, trackingOrigin,
        behavior, aReason, aPerformFinalChecks);
  }

  MOZ_ASSERT(XRE_IsContentProcess());
  // Only support PerformFinalChecks when we run ::CompleteAllowAccessFor in
  // the same process. This callback is only used by eStorageAccessAPI,
  // which is always runned in the same process.
  MOZ_ASSERT(!aPerformFinalChecks);

  ContentChild* cc = ContentChild::GetSingleton();
  MOZ_ASSERT(cc);

  RefPtr<BrowsingContext> bc = aParentContext;
  return cc
      ->SendCompleteAllowAccessFor(aParentContext, topLevelWindowId,
                                   IPC::Principal(trackingPrincipal),
                                   trackingOrigin, behavior, aReason)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [bc, trackingOrigin, behavior,
              aReason](const ContentChild::CompleteAllowAccessForPromise::
                           ResolveOrRejectValue& aValue) {
               if (aValue.IsResolve() && aValue.ResolveValue().isSome()) {
                 // we don't call OnAllowAccessFor in the parent when this is
                 // triggered by the opener heuristic, so we have to do it here.
                 // See storePermission below for the reason.
                 if (aReason == ContentBlockingNotifier::eOpener &&
                     !bc->IsDiscarded()) {
                   MOZ_ASSERT(bc->IsInProcess());
                   ContentBlocking::OnAllowAccessFor(bc, trackingOrigin,
                                                     behavior, aReason);
                 }
                 return StorageAccessPermissionGrantPromise::CreateAndResolve(
                     aValue.ResolveValue().value(), __func__);
               }
               return StorageAccessPermissionGrantPromise::CreateAndReject(
                   false, __func__);
             });
}

// CompleteAllowAccessFor is used to process the remaining work in
// AllowAccessFor that may need to access information not accessible
// in the current process.
// This API supports running running in the child process and the
// parent process. When running in the child, aParentContext must be in-process.
//
// Here lists the possible cases based on our heuristics:
// 1. eStorageAccessAPI
//    aParentContext is the browsing context of the document that calls this
//    API, so it is always in-process. Since the tracking origin is the
//    document's origin, it's same-origin to the parent window.
//    CompleteAllowAccessFor runs in the same process as AllowAccessFor.
//
// 2. eOpener
//    aParentContext is the browsing context of the opener that calls this
//    API, so it is always in-process. However, when the opener is a first
//    party and it opens a third-party window, the tracking origin is
//    origin of the third-party window. In this case, we should
//    run this API in the parent, as for the other cases, we can run in the
//    same process.
//
// 3. eOpenerAfterUserInteraction
//    aParentContext is the browsing context of the opener window, but
//    AllowAccessFor is called by the opened window. So as long as
//    aParentContext is not in-process, we should run in the parent.
/* static */ RefPtr<ContentBlocking::StorageAccessPermissionGrantPromise>
ContentBlocking::CompleteAllowAccessFor(
    dom::BrowsingContext* aParentContext, uint64_t aTopLevelWindowId,
    nsIPrincipal* aTrackingPrincipal, const nsCString& aTrackingOrigin,
    uint32_t aCookieBehavior,
    ContentBlockingNotifier::StorageAccessPermissionGrantedReason aReason,
    const PerformFinalChecks& aPerformFinalChecks) {
  MOZ_ASSERT(aParentContext);
  MOZ_ASSERT_IF(XRE_IsContentProcess(), aParentContext->IsInProcess());

  nsCOMPtr<nsIPrincipal> trackingPrincipal;
  nsAutoCString trackingOrigin;
  if (!aTrackingPrincipal) {
    // User interaction is the only case that tracking principal is not
    // available.
    MOZ_ASSERT(XRE_IsParentProcess() &&
               aReason == ContentBlockingNotifier::eOpenerAfterUserInteraction);

    if (!AntiTrackingUtils::GetPrincipalAndTrackingOrigin(
            aParentContext, getter_AddRefs(trackingPrincipal),
            trackingOrigin)) {
      LOG(
          ("Error while computing the parent principal and tracking origin, "
           "bailing out early"));
      return StorageAccessPermissionGrantPromise::CreateAndReject(false,
                                                                  __func__);
    }
  } else {
    trackingPrincipal = aTrackingPrincipal;
    trackingOrigin = aTrackingOrigin;
  }

  LOG(("Tracking origin is %s", PromiseFlatCString(trackingOrigin).get()));

  // We hardcode this block reason since the first-party storage access
  // permission is granted for the purpose of blocking trackers.
  // Note that if aReason is eOpenerAfterUserInteraction and the
  // trackingPrincipal is not in a blocklist, we don't check the
  // user-interaction state, because it could be that the current process has
  // just sent the request to store the user-interaction permission into the
  // parent, without having received the permission itself yet.

  bool isInPrefList = false;
  trackingPrincipal->IsURIInPrefList(
      "privacy.restrict3rdpartystorage."
      "userInteractionRequiredForHosts",
      &isInPrefList);
  if (isInPrefList &&
      !ContentBlockingUserInteraction::Exists(trackingPrincipal)) {
    LOG_PRIN(("Tracking principal (%s) hasn't been interacted with before, "
              "refusing to add a first-party storage permission to access it",
              _spec),
             trackingPrincipal);
    ContentBlockingNotifier::OnDecision(
        aParentContext, ContentBlockingNotifier::BlockingDecision::eBlock,
        CookieJarSettings::IsRejectThirdPartyWithExceptions(aCookieBehavior)
            ? nsIWebProgressListener::STATE_COOKIES_BLOCKED_FOREIGN
            : nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER);
    return StorageAccessPermissionGrantPromise::CreateAndReject(false,
                                                                __func__);
  }

  // Ensure we can find the window before continuing, so we can safely
  // execute storePermission.
  if (aParentContext->IsInProcess() &&
      (!aParentContext->GetDOMWindow() ||
       !aParentContext->GetDOMWindow()->GetCurrentInnerWindow())) {
    LOG(
        ("No window found for our parent browsing context, bailing out "
         "early"));
    return StorageAccessPermissionGrantPromise::CreateAndReject(false,
                                                                __func__);
  }

  auto storePermission =
      [aParentContext, aTopLevelWindowId, trackingOrigin, trackingPrincipal,
       aCookieBehavior,
       aReason](int aAllowMode) -> RefPtr<StorageAccessPermissionGrantPromise> {
    // Inform the window we granted permission for. This has to be done in the
    // window's process.
    if (aParentContext->IsInProcess()) {
      ContentBlocking::OnAllowAccessFor(aParentContext, trackingOrigin,
                                        aCookieBehavior, aReason);
    } else {
      MOZ_ASSERT(XRE_IsParentProcess());

      // We don't have the window, send an IPC to the content process that
      // owns the parent window. But there is a special case, for window.open,
      // we'll return to the content process we need to inform when this
      // function is done. So we don't need to create an extra IPC for the case.
      if (aReason != ContentBlockingNotifier::eOpener) {
        ContentParent* cp = aParentContext->Canonical()->GetContentParent();
        Unused << cp->SendOnAllowAccessFor(aParentContext, trackingOrigin,
                                           aCookieBehavior, aReason);
      }
    }

    Maybe<ContentBlockingNotifier::StorageAccessPermissionGrantedReason>
        reportReason;
    // We can directly report here if we can know the origin of the top.
    if (XRE_IsParentProcess() || aParentContext->Top()->IsInProcess()) {
      ContentBlockingNotifier::ReportUnblockingToConsole(
          aParentContext, NS_ConvertUTF8toUTF16(trackingOrigin), aReason);

      // Set the report reason to nothing if we've already reported.
      reportReason = Nothing();
    } else {
      // Set the report reason, so that we can know the reason when reporting
      // in the parent.
      reportReason.emplace(aReason);
    }

    if (XRE_IsParentProcess()) {
      LOG(("Saving the permission: trackingOrigin=%s", trackingOrigin.get()));
      return SaveAccessForOriginOnParentProcess(
                 aTopLevelWindowId, aParentContext, trackingPrincipal,
                 trackingOrigin, aAllowMode)
          ->Then(
              GetCurrentSerialEventTarget(), __func__,
              [](ParentAccessGrantPromise::ResolveOrRejectValue&& aValue) {
                if (aValue.IsResolve()) {
                  return StorageAccessPermissionGrantPromise::CreateAndResolve(
                      ContentBlocking::eAllow, __func__);
                }
                return StorageAccessPermissionGrantPromise::CreateAndReject(
                    false, __func__);
              });
    }

    ContentChild* cc = ContentChild::GetSingleton();
    MOZ_ASSERT(cc);

    LOG(
        ("Asking the parent process to save the permission for us: "
         "trackingOrigin=%s",
         trackingOrigin.get()));

    // This is not really secure, because here we have the content process
    // sending the request of storing a permission.
    return cc
        ->SendStorageAccessPermissionGrantedForOrigin(
            aTopLevelWindowId, aParentContext,
            IPC::Principal(trackingPrincipal), trackingOrigin, aAllowMode,
            reportReason)
        ->Then(GetCurrentSerialEventTarget(), __func__,
               [](const ContentChild::
                      StorageAccessPermissionGrantedForOriginPromise::
                          ResolveOrRejectValue& aValue) {
                 if (aValue.IsResolve()) {
                   return StorageAccessPermissionGrantPromise::CreateAndResolve(
                       aValue.ResolveValue(), __func__);
                 }
                 return StorageAccessPermissionGrantPromise::CreateAndReject(
                     false, __func__);
               });
  };

  if (aPerformFinalChecks) {
    return aPerformFinalChecks()->Then(
        GetCurrentSerialEventTarget(), __func__,
        [storePermission](
            StorageAccessPermissionGrantPromise::ResolveOrRejectValue&&
                aValue) {
          if (aValue.IsResolve()) {
            return storePermission(aValue.ResolveValue());
          }
          return StorageAccessPermissionGrantPromise::CreateAndReject(false,
                                                                      __func__);
        });
  }
  return storePermission(false);
}

/* static */ void ContentBlocking::OnAllowAccessFor(
    dom::BrowsingContext* aParentContext, const nsCString& aTrackingOrigin,
    uint32_t aCookieBehavior,
    ContentBlockingNotifier::StorageAccessPermissionGrantedReason aReason) {
  MOZ_ASSERT(aParentContext->IsInProcess());

  // Let's inform the parent window and the other windows having the
  // same tracking origin about the stroage permission is granted.
  ContentBlocking::UpdateAllowAccessOnCurrentProcess(aParentContext,
                                                     aTrackingOrigin);

  // Let's inform the parent window.
  nsCOMPtr<nsPIDOMWindowInner> parentInner =
      AntiTrackingUtils::GetInnerWindow(aParentContext);
  if (NS_WARN_IF(!parentInner)) {
    return;
  }

  Document* doc = parentInner->GetExtantDoc();
  if (NS_WARN_IF(!doc)) {
    return;
  }

  // Theoratically this can be done in the parent process. But right now,
  // we need the channel while notifying content blocking events, and
  // we don't have a trivial way to obtain the channel in the parent
  // via BrowsingContext. So we just ask the child to do the work.
  ContentBlockingNotifier::OnEvent(
      doc->GetChannel(), false,
      CookieJarSettings::IsRejectThirdPartyWithExceptions(aCookieBehavior)
          ? nsIWebProgressListener::STATE_COOKIES_BLOCKED_FOREIGN
          : nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER,
      aTrackingOrigin, Some(aReason));
}

/* static */
RefPtr<mozilla::ContentBlocking::ParentAccessGrantPromise>
ContentBlocking::SaveAccessForOriginOnParentProcess(
    uint64_t aTopLevelWindowId, BrowsingContext* aParentContext,
    nsIPrincipal* aTrackingPrincipal, const nsCString& aTrackingOrigin,
    int aAllowMode, uint64_t aExpirationTime) {
  MOZ_ASSERT(aTopLevelWindowId != 0);

  RefPtr<WindowGlobalParent> wgp =
      WindowGlobalParent::GetByInnerWindowId(aTopLevelWindowId);
  if (!wgp) {
    LOG(("Can't get window global parent"));
    return ParentAccessGrantPromise::CreateAndReject(false, __func__);
  }

  // If the permission is granted on a first-party window, also have to update
  // the permission to all the other windows with the same tracking origin (in
  // the same tab), if any.
  ContentBlocking::UpdateAllowAccessOnParentProcess(aParentContext,
                                                    aTrackingOrigin);

  return ContentBlocking::SaveAccessForOriginOnParentProcess(
      wgp->DocumentPrincipal(), aTrackingPrincipal, aTrackingOrigin, aAllowMode,
      aExpirationTime);
}

/* static */
RefPtr<mozilla::ContentBlocking::ParentAccessGrantPromise>
ContentBlocking::SaveAccessForOriginOnParentProcess(
    nsIPrincipal* aParentPrincipal, nsIPrincipal* aTrackingPrincipal,
    const nsCString& aTrackingOrigin, int aAllowMode,
    uint64_t aExpirationTime) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(aAllowMode == eAllow || aAllowMode == eAllowAutoGrant);

  if (!aParentPrincipal || !aTrackingPrincipal) {
    LOG(("Invalid input arguments passed"));
    return ParentAccessGrantPromise::CreateAndReject(false, __func__);
  };

  LOG_PRIN(("Saving a first-party storage permission on %s for "
            "trackingOrigin=%s",
            _spec, aTrackingOrigin.get()),
           aParentPrincipal);

  if (NS_WARN_IF(!aParentPrincipal)) {
    // The child process is sending something wrong. Let's ignore it.
    LOG(("aParentPrincipal is null, bailing out early"));
    return ParentAccessGrantPromise::CreateAndReject(false, __func__);
  }

  PermissionManager* permManager = PermissionManager::GetInstance();
  if (NS_WARN_IF(!permManager)) {
    LOG(("Permission manager is null, bailing out early"));
    return ParentAccessGrantPromise::CreateAndReject(false, __func__);
  }

  // Remember that this pref is stored in seconds!
  uint32_t expirationType = nsIPermissionManager::EXPIRE_TIME;
  uint32_t expirationTime = aExpirationTime * 1000;
  int64_t when = (PR_Now() / PR_USEC_PER_MSEC) + expirationTime;

  uint32_t privateBrowsingId = 0;
  nsresult rv = aParentPrincipal->GetPrivateBrowsingId(&privateBrowsingId);
  if ((!NS_WARN_IF(NS_FAILED(rv)) && privateBrowsingId > 0) ||
      (aAllowMode == eAllowAutoGrant)) {
    // If we are coming from a private window or are automatically granting a
    // permission, make sure to store a session-only permission which won't
    // get persisted to disk.
    expirationType = nsIPermissionManager::EXPIRE_SESSION;
    when = 0;
  }

  nsAutoCString type;
  AntiTrackingUtils::CreateStoragePermissionKey(aTrackingOrigin, type);

  LOG(
      ("Computed permission key: %s, expiry: %u, proceeding to save in the "
       "permission manager",
       type.get(), expirationTime));

  rv = permManager->AddFromPrincipal(aParentPrincipal, type,
                                     nsIPermissionManager::ALLOW_ACTION,
                                     expirationType, when);
  Unused << NS_WARN_IF(NS_FAILED(rv));

  if (NS_SUCCEEDED(rv) && (aAllowMode == eAllowAutoGrant)) {
    // Make sure temporary access grants do not survive more than 24 hours.
    TemporaryAccessGrantObserver::Create(permManager, aParentPrincipal, type);
  }

  LOG(("Result: %s", NS_SUCCEEDED(rv) ? "success" : "failure"));
  return ParentAccessGrantPromise::CreateAndResolve(rv, __func__);
}

// There are two methods to handle permisson update:
// 1. UpdateAllowAccessOnCurrentProcess
// 2. UpdateAllowAccessOnParentProcess
//
// In general, UpdateAllowAccessOnCurrentProcess is used to propagate storage
// permission to same-origin frames in the same tab.
// UpdateAllowAccessOnParentProcess is used to propagate storage permission to
// same-origin frames in the same agent cluster.
//
// However, there is an exception in fission mode. When the heuristic is
// triggered by a first-party window, for instance, a first-party script calls
// window.open(tracker), we can't update 3rd-party frames's storage permission
// in the child process that triggers the permission update because the
// first-party and the 3rd-party are not in the same process. In this case, we
// should update the storage permission in UpdateAllowAccessOnParentProcess.

// This function is used to update permission to all in-process windows, so it
// can be called either from the parent or the child.
/* static */
void ContentBlocking::UpdateAllowAccessOnCurrentProcess(
    BrowsingContext* aParentContext, const nsACString& aTrackingOrigin) {
  MOZ_ASSERT(aParentContext && aParentContext->IsInProcess());

  bool useRemoteSubframes;
  aParentContext->GetUseRemoteSubframes(&useRemoteSubframes);

  if (useRemoteSubframes && aParentContext->IsTopContent()) {
    // If we are a first-party and we are in fission mode, bail out early
    // because we can't do anything here.
    return;
  }

  BrowsingContext* top = aParentContext->Top();
  uint32_t behavior = AntiTrackingUtils::GetCookieBehavior(top);

  // Propagate the storage permission to same-origin frames in the same tab.
  top->PreOrderWalk([&](BrowsingContext* aContext) {
    // Only check browsing contexts that are in-process.
    if (aContext->IsInProcess()) {
      nsAutoCString origin;
      Unused << AntiTrackingUtils::GetPrincipalAndTrackingOrigin(
          aContext, nullptr, origin);

      // Permission is only synced to first-level iframes.
      if ((aParentContext != aContext) &&
          (behavior == nsICookieService::BEHAVIOR_REJECT_TRACKER &&
           !AntiTrackingUtils::IsFirstLevelSubContext(aContext))) {
        return;
      }

      if (aTrackingOrigin == origin) {
        nsCOMPtr<nsPIDOMWindowInner> inner =
            AntiTrackingUtils::GetInnerWindow(aContext);
        if (inner) {
          inner->SaveStorageAccessPermissionGranted();
        }

        nsCOMPtr<nsPIDOMWindowOuter> outer =
            nsPIDOMWindowOuter::GetFromCurrentInner(inner);
        if (outer) {
          nsGlobalWindowOuter::Cast(outer)->SetStorageAccessPermissionGranted(
              true);
        }
      }
    }
  });
}

/* static */
void ContentBlocking::UpdateAllowAccessOnParentProcess(
    BrowsingContext* aParentContext, const nsACString& aTrackingOrigin) {
  MOZ_ASSERT(XRE_IsParentProcess());

  uint32_t behavior = AntiTrackingUtils::GetCookieBehavior(aParentContext);

  nsAutoCString topKey;
  nsCOMPtr<nsIPrincipal> topPrincipal =
      AntiTrackingUtils::GetPrincipal(aParentContext->Top());
  PermissionManager::GetKeyForPrincipal(topPrincipal, false, topKey);

  // Propagate the storage permission to same-origin frames in the same
  // agent-cluster.
  for (const auto& topContext : aParentContext->Group()->Toplevels()) {
    if (topContext == aParentContext->Top()) {
      // We don't have to update same-origin frames in the same tab unless
      // when we are in fission mode and the storage permission granted is
      // called by a first-party window and we are in fission mode.
      bool useRemoteSubframes;
      aParentContext->GetUseRemoteSubframes(&useRemoteSubframes);
      if (!useRemoteSubframes || !aParentContext->IsTop()) {
        continue;
      }
    } else {
      nsCOMPtr<nsIPrincipal> principal =
          AntiTrackingUtils::GetPrincipal(topContext);
      if (!principal) {
        continue;
      }

      nsAutoCString key;
      PermissionManager::GetKeyForPrincipal(principal, false, key);
      // Make sure we only apply to frames that have the same top-level.
      if (topKey != key) {
        continue;
      }
    }

    topContext->PreOrderWalk([&](BrowsingContext* aContext) {
      WindowGlobalParent* wgp = aContext->Canonical()->GetCurrentWindowGlobal();
      if (!wgp) {
        return;
      }

      if (behavior == nsICookieService::BEHAVIOR_REJECT_TRACKER &&
          !AntiTrackingUtils::IsFirstLevelSubContext(aContext)) {
        return;
      }

      nsAutoCString origin;
      AntiTrackingUtils::GetPrincipalAndTrackingOrigin(aContext, nullptr,
                                                       origin);
      if (aTrackingOrigin == origin) {
        Unused << wgp->SendSaveStorageAccessPermissionGranted();
      }
    });
  }
}

bool ContentBlocking::ShouldAllowAccessFor(nsPIDOMWindowInner* aWindow,
                                           nsIURI* aURI,
                                           uint32_t* aRejectedReason) {
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aURI);

  // Let's avoid a null check on aRejectedReason everywhere else.
  uint32_t rejectedReason = 0;
  if (!aRejectedReason) {
    aRejectedReason = &rejectedReason;
  }

  LOG_SPEC(("Computing whether window %p has access to URI %s", aWindow, _spec),
           aURI);

  nsGlobalWindowInner* innerWindow = nsGlobalWindowInner::Cast(aWindow);
  Document* document = innerWindow->GetExtantDoc();
  if (!document) {
    LOG(("Our window has no document"));
    return false;
  }

  uint32_t cookiePermission = CheckCookiePermissionForPrincipal(
      document->CookieJarSettings(), document->NodePrincipal());
  if (cookiePermission != nsICookiePermission::ACCESS_DEFAULT) {
    LOG(
        ("CheckCookiePermissionForPrincipal() returned a non-default access "
         "code (%d) for window's principal, returning %s",
         int(cookiePermission),
         cookiePermission != nsICookiePermission::ACCESS_DENY ? "success"
                                                              : "failure"));
    if (cookiePermission != nsICookiePermission::ACCESS_DENY) {
      return true;
    }

    *aRejectedReason =
        nsIWebProgressListener::STATE_COOKIES_BLOCKED_BY_PERMISSION;
    return false;
  }

  int32_t behavior = CookiesBehavior(document);
  if (behavior == nsICookieService::BEHAVIOR_ACCEPT) {
    LOG(("The cookie behavior pref mandates accepting all cookies!"));
    return true;
  }

  if (ContentBlockingAllowList::Check(aWindow)) {
    return true;
  }

  if (behavior == nsICookieService::BEHAVIOR_REJECT) {
    LOG(("The cookie behavior pref mandates rejecting all cookies!"));
    *aRejectedReason = nsIWebProgressListener::STATE_COOKIES_BLOCKED_ALL;
    return false;
  }

  // As a performance optimization, we only perform this check for
  // BEHAVIOR_REJECT_FOREIGN and BEHAVIOR_LIMIT_FOREIGN.  For
  // BEHAVIOR_REJECT_TRACKER and BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
  // third-partiness is implicily checked later below.
  if (behavior != nsICookieService::BEHAVIOR_REJECT_TRACKER &&
      behavior !=
          nsICookieService::BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN) {
    // Let's check if this is a 3rd party context.
    if (!AntiTrackingUtils::IsThirdPartyWindow(aWindow, aURI)) {
      LOG(("Our window isn't a third-party window"));
      return true;
    }
  }

  if ((behavior == nsICookieService::BEHAVIOR_REJECT_FOREIGN &&
       !CookieJarSettings::IsRejectThirdPartyWithExceptions(behavior)) ||
      behavior == nsICookieService::BEHAVIOR_LIMIT_FOREIGN) {
    // XXX For non-cookie forms of storage, we handle BEHAVIOR_LIMIT_FOREIGN by
    // simply rejecting the request to use the storage. In the future, if we
    // change the meaning of BEHAVIOR_LIMIT_FOREIGN to be one which makes sense
    // for non-cookie storage types, this may change.
    LOG(("Nothing more to do due to the behavior code %d", int(behavior)));
    *aRejectedReason = nsIWebProgressListener::STATE_COOKIES_BLOCKED_FOREIGN;
    return false;
  }

  MOZ_ASSERT(
      CookieJarSettings::IsRejectThirdPartyWithExceptions(behavior) ||
      behavior == nsICookieService::BEHAVIOR_REJECT_TRACKER ||
      behavior ==
          nsICookieService::BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN);

  uint32_t blockedReason =
      nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER;

  if (behavior == nsICookieService::BEHAVIOR_REJECT_TRACKER) {
    if (!nsContentUtils::IsThirdPartyTrackingResourceWindow(aWindow)) {
      LOG(("Our window isn't a third-party tracking window"));
      return true;
    }

    nsCOMPtr<nsIClassifiedChannel> classifiedChannel =
        do_QueryInterface(document->GetChannel());
    if (classifiedChannel) {
      uint32_t classificationFlags =
          classifiedChannel->GetThirdPartyClassificationFlags();
      if (classificationFlags & nsIClassifiedChannel::ClassificationFlags::
                                    CLASSIFIED_SOCIALTRACKING) {
        blockedReason =
            nsIWebProgressListener::STATE_COOKIES_BLOCKED_SOCIALTRACKER;
      }
    }
  } else if (behavior ==
             nsICookieService::BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN) {
    if (nsContentUtils::IsThirdPartyTrackingResourceWindow(aWindow)) {
      // fall through
    } else if (AntiTrackingUtils::IsThirdPartyWindow(aWindow, aURI)) {
      LOG(("We're in the third-party context, storage should be partitioned"));
      // fall through, but remember that we're partitioning.
      blockedReason = nsIWebProgressListener::STATE_COOKIES_PARTITIONED_FOREIGN;
    } else {
      LOG(("Our window isn't a third-party window, storage is allowed"));
      return true;
    }
  } else {
    MOZ_ASSERT(CookieJarSettings::IsRejectThirdPartyWithExceptions(behavior));
    if (RejectForeignAllowList::Check(document)) {
      LOG(("This window is exceptionlisted for reject foreign"));
      return true;
    }

    blockedReason = nsIWebProgressListener::STATE_COOKIES_PARTITIONED_FOREIGN;
  }

#ifdef DEBUG
  nsCOMPtr<mozIThirdPartyUtil> thirdPartyUtil = services::GetThirdPartyUtil();
  if (thirdPartyUtil) {
    bool thirdParty = false;
    nsresult rv = thirdPartyUtil->IsThirdPartyWindow(aWindow->GetOuterWindow(),
                                                     aURI, &thirdParty);
    // The result of this assertion depends on whether IsThirdPartyWindow
    // succeeds, because otherwise IsThirdPartyWindowOrChannel artificially
    // fails.
    MOZ_ASSERT_IF(NS_SUCCEEDED(rv), nsContentUtils::IsThirdPartyWindowOrChannel(
                                        aWindow, nullptr, aURI) == thirdParty);
  }
#endif

  Document* doc = aWindow->GetExtantDoc();
  // Make sure storage access isn't disabled
  if (doc && (doc->StorageAccessSandboxed())) {
    LOG(("Our document is sandboxed"));
    *aRejectedReason = blockedReason;
    return false;
  }

  // We will only allow the storage access for the first-level iframe in cookie
  // behavior BEHAVIOR_REJECT_TRACKER. We don't need to consider the top window
  // here since we only get here if the window is not a top.
  if (behavior == nsICookieService::BEHAVIOR_REJECT_TRACKER &&
      !AntiTrackingUtils::IsFirstLevelSubContext(
          aWindow->GetBrowsingContext())) {
    *aRejectedReason = blockedReason;
    return false;
  }

  // Document::HasStoragePermission first checks if storage access granted is
  // cached in the inner window, if no, it then checks the storage permission
  // flag in the channel's loadinfo
  bool allowed = document->HasStorageAccessPermissionGranted();

  if (!allowed) {
    *aRejectedReason = blockedReason;
  } else {
    if (MOZ_LOG_TEST(gAntiTrackingLog, mozilla::LogLevel::Debug) &&
        aWindow->HasStorageAccessPermissionGranted()) {
      LOG(("Permission stored in the window. All good."));
    }
  }

  return allowed;
}

bool ContentBlocking::ShouldAllowAccessFor(nsIChannel* aChannel, nsIURI* aURI,
                                           uint32_t* aRejectedReason) {
  MOZ_ASSERT(aURI);
  MOZ_ASSERT(aChannel);

  // Let's avoid a null check on aRejectedReason everywhere else.
  uint32_t rejectedReason = 0;
  if (!aRejectedReason) {
    aRejectedReason = &rejectedReason;
  }

  nsIScriptSecurityManager* ssm =
      nsScriptSecurityManager::GetScriptSecurityManager();
  MOZ_ASSERT(ssm);

  nsCOMPtr<nsIURI> channelURI;
  nsresult rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(channelURI));
  if (NS_FAILED(rv)) {
    LOG(("Failed to get the channel final URI, bail out early"));
    return true;
  }
  LOG_SPEC(
      ("Computing whether channel %p has access to URI %s", aChannel, _spec),
      channelURI);

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  nsCOMPtr<nsICookieJarSettings> cookieJarSettings;
  rv = loadInfo->GetCookieJarSettings(getter_AddRefs(cookieJarSettings));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG(
        ("Failed to get the cookie jar settings from the loadinfo, bail out "
         "early"));
    return true;
  }

  nsCOMPtr<nsIPrincipal> channelPrincipal;
  rv = ssm->GetChannelURIPrincipal(aChannel, getter_AddRefs(channelPrincipal));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG(("No channel principal, bail out early"));
    return false;
  }

  uint32_t cookiePermission =
      CheckCookiePermissionForPrincipal(cookieJarSettings, channelPrincipal);
  if (cookiePermission != nsICookiePermission::ACCESS_DEFAULT) {
    LOG(
        ("CheckCookiePermissionForPrincipal() returned a non-default access "
         "code (%d) for channel's principal, returning %s",
         int(cookiePermission),
         cookiePermission != nsICookiePermission::ACCESS_DENY ? "success"
                                                              : "failure"));
    if (cookiePermission != nsICookiePermission::ACCESS_DENY) {
      return true;
    }

    *aRejectedReason =
        nsIWebProgressListener::STATE_COOKIES_BLOCKED_BY_PERMISSION;
    return false;
  }

  if (!channelURI) {
    LOG(("No channel uri, bail out early"));
    return false;
  }

  int32_t behavior = CookiesBehavior(loadInfo, channelURI);
  if (behavior == nsICookieService::BEHAVIOR_ACCEPT) {
    LOG(("The cookie behavior pref mandates accepting all cookies!"));
    return true;
  }

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aChannel);

  if (httpChannel && ContentBlockingAllowList::Check(httpChannel)) {
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
  rv = thirdPartyUtil->IsThirdPartyChannel(aChannel, aURI, &thirdParty);
  // Grant if it's not a 3rd party.
  // Be careful to check the return value of IsThirdPartyChannel, since
  // IsThirdPartyChannel() will fail if the channel's loading principal is the
  // system principal...
  if (NS_SUCCEEDED(rv) && !thirdParty) {
    LOG(("Our channel isn't a third-party channel"));
    return true;
  }

  if ((behavior == nsICookieService::BEHAVIOR_REJECT_FOREIGN &&
       !CookieJarSettings::IsRejectThirdPartyWithExceptions(behavior)) ||
      behavior == nsICookieService::BEHAVIOR_LIMIT_FOREIGN) {
    // XXX For non-cookie forms of storage, we handle BEHAVIOR_LIMIT_FOREIGN by
    // simply rejecting the request to use the storage. In the future, if we
    // change the meaning of BEHAVIOR_LIMIT_FOREIGN to be one which makes sense
    // for non-cookie storage types, this may change.
    LOG(("Nothing more to do due to the behavior code %d", int(behavior)));
    *aRejectedReason = nsIWebProgressListener::STATE_COOKIES_BLOCKED_FOREIGN;
    return false;
  }

  MOZ_ASSERT(
      CookieJarSettings::IsRejectThirdPartyWithExceptions(behavior) ||
      behavior == nsICookieService::BEHAVIOR_REJECT_TRACKER ||
      behavior ==
          nsICookieService::BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN);

  uint32_t blockedReason =
      nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER;

  // Not a tracker.
  nsCOMPtr<nsIClassifiedChannel> classifiedChannel =
      do_QueryInterface(aChannel);
  if (behavior == nsICookieService::BEHAVIOR_REJECT_TRACKER) {
    if (classifiedChannel) {
      if (!classifiedChannel->IsThirdPartyTrackingResource()) {
        LOG(("Our channel isn't a third-party tracking channel"));
        return true;
      }

      uint32_t classificationFlags =
          classifiedChannel->GetThirdPartyClassificationFlags();
      if (classificationFlags & nsIClassifiedChannel::ClassificationFlags::
                                    CLASSIFIED_SOCIALTRACKING) {
        blockedReason =
            nsIWebProgressListener::STATE_COOKIES_BLOCKED_SOCIALTRACKER;
      }
    }
  } else if (behavior ==
             nsICookieService::BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN) {
    if (classifiedChannel &&
        classifiedChannel->IsThirdPartyTrackingResource()) {
      // fall through
    } else if (AntiTrackingUtils::IsThirdPartyChannel(aChannel)) {
      LOG(("We're in the third-party context, storage should be partitioned"));
      // fall through but remember that we're partitioning.
      blockedReason = nsIWebProgressListener::STATE_COOKIES_PARTITIONED_FOREIGN;
    } else {
      LOG(("Our channel isn't a third-party channel, storage is allowed"));
      return true;
    }
  } else {
    MOZ_ASSERT(CookieJarSettings::IsRejectThirdPartyWithExceptions(behavior));
    if (httpChannel && RejectForeignAllowList::Check(httpChannel)) {
      LOG(("This channel is exceptionlisted"));
      return true;
    }
    blockedReason = nsIWebProgressListener::STATE_COOKIES_BLOCKED_FOREIGN;
  }

  RefPtr<BrowsingContext> targetBC;
  rv = loadInfo->GetTargetBrowsingContext(getter_AddRefs(targetBC));
  if (!targetBC || NS_WARN_IF(NS_FAILED(rv))) {
    LOG(("Failed to get the channel's target browsing context"));
    return false;
  }

  // We will only allow the storage access for the channel of the first-level
  // iframe or top-level sub-resource in cookie behavior
  // BEHAVIOR_REJECT_TRACKER.
  if (behavior == nsICookieService::BEHAVIOR_REJECT_TRACKER &&
      !targetBC->IsTopContent() &&
      !AntiTrackingUtils::IsFirstLevelSubContext(targetBC)) {
    *aRejectedReason = blockedReason;
    return false;
  }

  if (Document::StorageAccessSandboxed(targetBC->GetSandboxFlags())) {
    LOG(("Our document is sandboxed"));
    *aRejectedReason = blockedReason;
    return false;
  }

  // Let's see if we have to grant the access for this particular channel.

  nsCOMPtr<nsIURI> trackingURI;
  rv = aChannel->GetURI(getter_AddRefs(trackingURI));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG(("Failed to get the channel URI"));
    return true;
  }

  nsAutoCString trackingOrigin;
  rv = nsContentUtils::GetASCIIOrigin(trackingURI, trackingOrigin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG_SPEC(("Failed to compute the origin from %s", _spec), trackingURI);
    return false;
  }

  // HasStorageAccessPermissionGranted only applies to channels that load
  // documents, for sub-resources loads, just returns the result from loadInfo.
  bool isDocument = false;
  aChannel->GetIsDocument(&isDocument);

  if (isDocument) {
    nsCOMPtr<nsPIDOMWindowInner> inner =
        AntiTrackingUtils::GetInnerWindow(targetBC);
    if (inner && inner->HasStorageAccessPermissionGranted()) {
      LOG(("Permission stored in the window. All good."));
      return true;
    }
  }

  bool allowed = loadInfo->GetHasStoragePermission();
  if (!allowed) {
    *aRejectedReason = blockedReason;
  }

  return allowed;
}

bool ContentBlocking::ShouldAllowAccessFor(
    nsIPrincipal* aPrincipal, nsICookieJarSettings* aCookieJarSettings) {
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aCookieJarSettings);

  uint32_t access = nsICookiePermission::ACCESS_DEFAULT;
  if (aPrincipal->GetIsContentPrincipal()) {
    PermissionManager* permManager = PermissionManager::GetInstance();
    if (permManager) {
      Unused << NS_WARN_IF(NS_FAILED(permManager->TestPermissionFromPrincipal(
          aPrincipal, "cookie"_ns, &access)));
    }
  }

  if (access != nsICookiePermission::ACCESS_DEFAULT) {
    return access != nsICookiePermission::ACCESS_DENY;
  }

  int32_t behavior = CookiesBehavior(aPrincipal, aCookieJarSettings);
  return behavior != nsICookieService::BEHAVIOR_REJECT;
}

/* static */
bool ContentBlocking::ApproximateAllowAccessForWithoutChannel(
    nsPIDOMWindowInner* aFirstPartyWindow, nsIURI* aURI) {
  MOZ_ASSERT(aFirstPartyWindow);
  MOZ_ASSERT(aURI);

  LOG_SPEC(
      ("Computing a best guess as to whether window %p has access to URI %s",
       aFirstPartyWindow, _spec),
      aURI);

  Document* parentDocument =
      nsGlobalWindowInner::Cast(aFirstPartyWindow)->GetExtantDoc();
  if (NS_WARN_IF(!parentDocument)) {
    LOG(("Failed to get the first party window's document"));
    return false;
  }

  if (!parentDocument->CookieJarSettings()->GetRejectThirdPartyContexts()) {
    LOG(("Disabled by the pref (%d), bail out early",
         parentDocument->CookieJarSettings()->GetCookieBehavior()));
    return true;
  }

  if (ContentBlockingAllowList::Check(aFirstPartyWindow)) {
    return true;
  }

  if (!AntiTrackingUtils::IsThirdPartyWindow(aFirstPartyWindow, aURI)) {
    LOG(("Our window isn't a third-party window"));
    return true;
  }

  uint32_t cookiePermission = CheckCookiePermissionForPrincipal(
      parentDocument->CookieJarSettings(), parentDocument->NodePrincipal());
  if (cookiePermission != nsICookiePermission::ACCESS_DEFAULT) {
    LOG(
        ("CheckCookiePermissionForPrincipal() returned a non-default access "
         "code (%d), returning %s",
         int(cookiePermission),
         cookiePermission != nsICookiePermission::ACCESS_DENY ? "success"
                                                              : "failure"));
    return cookiePermission != nsICookiePermission::ACCESS_DENY;
  }

  nsAutoCString origin;
  nsresult rv = nsContentUtils::GetASCIIOrigin(aURI, origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG_SPEC(("Failed to compute the origin from %s", _spec), aURI);
    return false;
  }

  nsIPrincipal* parentPrincipal = parentDocument->NodePrincipal();

  nsAutoCString type;
  AntiTrackingUtils::CreateStoragePermissionKey(origin, type);

  return AntiTrackingUtils::CheckStoragePermission(
      parentPrincipal, type,
      nsContentUtils::IsInPrivateBrowsing(parentDocument), nullptr, 0);
}
