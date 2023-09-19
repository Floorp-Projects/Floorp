/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AntiTrackingLog.h"
#include "StorageAccessAPIHelper.h"
#include "AntiTrackingUtils.h"
#include "TemporaryAccessGrantObserver.h"

#include "mozilla/Components.h"
#include "mozilla/ContentBlockingAllowList.h"
#include "mozilla/ContentBlockingUserInteraction.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/BrowsingContextGroup.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/FeaturePolicy.h"
#include "mozilla/dom/WindowContext.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/net/CookieJarSettings.h"
#include "mozilla/PermissionManager.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "mozilla/Telemetry.h"
#include "mozIThirdPartyUtil.h"
#include "nsContentUtils.h"
#include "nsIClassifiedChannel.h"
#include "nsICookiePermission.h"
#include "nsICookieService.h"
#include "nsIPermission.h"
#include "nsIPrincipal.h"
#include "nsIURI.h"
#include "nsIURIClassifier.h"
#include "nsIUrlClassifierFeature.h"
#include "nsIOService.h"
#include "nsIWebProgressListener.h"
#include "nsScriptSecurityManager.h"
#include "StorageAccess.h"
#include "nsStringFwd.h"

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

}  // namespace

/* static */ RefPtr<StorageAccessAPIHelper::StorageAccessPermissionGrantPromise>
StorageAccessAPIHelper::AllowAccessFor(
    nsIPrincipal* aPrincipal, dom::BrowsingContext* aParentContext,
    ContentBlockingNotifier::StorageAccessPermissionGrantedReason aReason,
    const StorageAccessAPIHelper::PerformPermissionGrant& aPerformFinalChecks) {
  MOZ_ASSERT(aParentContext);

  switch (aReason) {
    case ContentBlockingNotifier::eOpener:
      if (!StaticPrefs::privacy_antitracking_enableWebcompat() ||
          !StaticPrefs::
              privacy_restrict3rdpartystorage_heuristic_window_open()) {
        LOG(
            ("Bailing out early because the window open heuristic is disabled "
             "by pref"));
        return StorageAccessPermissionGrantPromise::CreateAndReject(false,
                                                                    __func__);
      }
      break;
    case ContentBlockingNotifier::eOpenerAfterUserInteraction:
      if (!StaticPrefs::privacy_antitracking_enableWebcompat() ||
          !StaticPrefs::
              privacy_restrict3rdpartystorage_heuristic_opened_window_after_interaction()) {
        LOG(
            ("Bailing out early because the window open after interaction "
             "heuristic is disabled by pref"));
        return StorageAccessPermissionGrantPromise::CreateAndReject(false,
                                                                    __func__);
      }
      break;
    default:
      break;
  }

  if (MOZ_LOG_TEST(gAntiTrackingLog, mozilla::LogLevel::Debug)) {
    nsAutoCString origin;
    aPrincipal->GetOriginNoSuffix(origin);
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
      behavior == nsICookieService::BEHAVIOR_REJECT_TRACKER ||
      behavior ==
          nsICookieService::BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN);

  // No need to continue when we are already in the allow list.
  if (parentWindowContext->GetIsOnContentBlockingAllowList()) {
    return StorageAccessPermissionGrantPromise::CreateAndResolve(true,
                                                                 __func__);
  }

  // Make sure storage access isn't disabled
  if (!aParentContext->IsTopContent() &&
      Document::StorageAccessSandboxed(aParentContext->GetSandboxFlags())) {
    LOG(("Our document is sandboxed"));
    return StorageAccessPermissionGrantPromise::CreateAndReject(false,
                                                                __func__);
  }

  bool isParentThirdParty = parentWindowContext->GetIsThirdPartyWindow();
  uint64_t topLevelWindowId;
  nsAutoCString trackingOrigin;
  nsCOMPtr<nsIPrincipal> trackingPrincipal;

  LOG(("The current resource is %s-party",
       isParentThirdParty ? "third" : "first"));

  // We are a first party resource.
  if (!isParentThirdParty) {
    nsAutoCString origin;
    nsresult rv = aPrincipal->GetOriginNoSuffix(origin);
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
    if (behavior ==
            nsICookieService::BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN &&
        !isParentThirdParty) {
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
  // We will follow below algorithm to decide if we can continue to run in
  // the current process, otherwise, we need to ask the parent to continue
  // the work.
  // 1. Check if aParentContext is an in-process browsing context. If it isn't,
  //    we cannot proceed in the content process because we need the
  //    principal of the parent window. Otherwise, we go to step 2.
  // 2. Check if the grant reason is ePrivilegeStorageAccessForOriginAPI. In
  //    this case, we don't need to check the user interaction of the tracking
  //    origin. So, we can proceed in the content process. Otherwise, go to
  //    step 3.
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
    // If we are already in the parent, then continue to run in the parent.
    runInSameProcess = true;
  } else {
    // We should run in the parent process when the tracking origin is
    // third-party with respect to it's parent window. This is because we can't
    // test if the user has interacted with the third-party origin in the child
    // process.
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
      runInSameProcess =
          aReason ==
              ContentBlockingNotifier::ePrivilegeStorageAccessForOriginAPI ||
          !isThirdParty;
    } else {
      runInSameProcess = false;
    }
  }

  if (runInSameProcess) {
    return StorageAccessAPIHelper::CompleteAllowAccessFor(
        aParentContext, topLevelWindowId, trackingPrincipal, trackingOrigin,
        behavior, aReason, aPerformFinalChecks);
  }

  MOZ_ASSERT(XRE_IsContentProcess());
  // Only support PerformPermissionGrant when we run ::CompleteAllowAccessFor in
  // the same process. This callback is only used by eStorageAccessAPI,
  // which is always runned in the same process.
  MOZ_ASSERT(!aPerformFinalChecks);

  ContentChild* cc = ContentChild::GetSingleton();
  MOZ_ASSERT(cc);

  RefPtr<BrowsingContext> bc = aParentContext;
  return cc
      ->SendCompleteAllowAccessFor(aParentContext, topLevelWindowId,
                                   trackingPrincipal, trackingOrigin, behavior,
                                   aReason)
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
                   StorageAccessAPIHelper::OnAllowAccessFor(bc, trackingOrigin,
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
//
// 4. ePrivilegeStorageAccessForOriginAPI
//    aParentContext is the browsing context of the top window which calls the
//    privilege API. So, it is always in-process. And we don't need to check the
//    user interaction permission for the tracking origin in this case. We can
//    run in the same process.
/* static */ RefPtr<StorageAccessAPIHelper::StorageAccessPermissionGrantPromise>
StorageAccessAPIHelper::CompleteAllowAccessFor(
    dom::BrowsingContext* aParentContext, uint64_t aTopLevelWindowId,
    nsIPrincipal* aTrackingPrincipal, const nsACString& aTrackingOrigin,
    uint32_t aCookieBehavior,
    ContentBlockingNotifier::StorageAccessPermissionGrantedReason aReason,
    const PerformPermissionGrant& aPerformFinalChecks) {
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
  //
  // For ePrivilegeStorageAccessForOriginAPI, we explicitly don't check the user
  // interaction for the tracking origin.

  bool isInPrefList = false;
  trackingPrincipal->IsURIInPrefList(
      "privacy.restrict3rdpartystorage."
      "userInteractionRequiredForHosts",
      &isInPrefList);
  if (aReason != ContentBlockingNotifier::ePrivilegeStorageAccessForOriginAPI &&
      isInPrefList &&
      !ContentBlockingUserInteraction::Exists(trackingPrincipal)) {
    LOG_PRIN(("Tracking principal (%s) hasn't been interacted with before, "
              "refusing to add a first-party storage permission to access it",
              _spec),
             trackingPrincipal);
    ContentBlockingNotifier::OnDecision(
        aParentContext, ContentBlockingNotifier::BlockingDecision::eBlock,
        nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER);
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
      StorageAccessAPIHelper::OnAllowAccessFor(aParentContext, trackingOrigin,
                                               aCookieBehavior, aReason);
    } else {
      MOZ_ASSERT(XRE_IsParentProcess());

      // We don't have the window, send an IPC to the content process that
      // owns the parent window. But there is a special case, for window.open,
      // we'll return to the content process we need to inform when this
      // function is done. So we don't need to create an extra IPC for the case.
      if (aReason != ContentBlockingNotifier::eOpener) {
        dom::ContentParent* cp =
            aParentContext->Canonical()->GetContentParent();
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
      bool frameOnly = StaticPrefs::dom_storage_access_frame_only() &&
                       aReason == ContentBlockingNotifier::eStorageAccessAPI;
      return SaveAccessForOriginOnParentProcess(
                 aTopLevelWindowId, aParentContext, trackingPrincipal,
                 aAllowMode, frameOnly)
          ->Then(
              GetCurrentSerialEventTarget(), __func__,
              [aReason, trackingPrincipal](
                  ParentAccessGrantPromise::ResolveOrRejectValue&& aValue) {
                if (!aValue.IsResolve()) {
                  return StorageAccessPermissionGrantPromise::CreateAndReject(
                      false, __func__);
                }
                // We only wish to observe user interaction in the case of a
                // "normal" requestStorageAccess grant. We do not observe user
                // interaction where the priveledged API is used. Acquiring
                // the storageAccessAPI permission for the first time will only
                // occur through the clicking accept on the doorhanger.
                if (aReason == ContentBlockingNotifier::eStorageAccessAPI) {
                  ContentBlockingUserInteraction::Observe(trackingPrincipal);
                }
                return StorageAccessPermissionGrantPromise::CreateAndResolve(
                    StorageAccessAPIHelper::eAllow, __func__);
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
    bool frameOnly = StaticPrefs::dom_storage_access_frame_only() &&
                     aReason == ContentBlockingNotifier::eStorageAccessAPI;
    return cc
        ->SendStorageAccessPermissionGrantedForOrigin(
            aTopLevelWindowId, aParentContext, trackingPrincipal,
            trackingOrigin, aAllowMode, reportReason, frameOnly)
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [aReason, trackingPrincipal](
                const ContentChild::
                    StorageAccessPermissionGrantedForOriginPromise::
                        ResolveOrRejectValue& aValue) {
              if (aValue.IsResolve()) {
                if (aValue.ResolveValue() &&
                    (aReason == ContentBlockingNotifier::eStorageAccessAPI)) {
                  ContentBlockingUserInteraction::Observe(trackingPrincipal);
                }
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

/* static */ void StorageAccessAPIHelper::OnAllowAccessFor(
    dom::BrowsingContext* aParentContext, const nsACString& aTrackingOrigin,
    uint32_t aCookieBehavior,
    ContentBlockingNotifier::StorageAccessPermissionGrantedReason aReason) {
  MOZ_ASSERT(aParentContext->IsInProcess());

  // Let's inform the parent window and the other windows having the
  // same tracking origin about the storage permission is granted
  // if it is not a frame-only permission grant which does not propogate.
  if (aReason != ContentBlockingNotifier::StorageAccessPermissionGrantedReason::
                     eStorageAccessAPI ||
      !StaticPrefs::dom_storage_access_frame_only()) {
    StorageAccessAPIHelper::UpdateAllowAccessOnCurrentProcess(aParentContext,
                                                              aTrackingOrigin);
  }

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

  if (!doc->GetChannel()) {
    return;
  }

  Telemetry::AccumulateCategorical(
      Telemetry::LABELS_STORAGE_ACCESS_GRANTED_COUNT::StorageGranted);

  switch (aReason) {
    case ContentBlockingNotifier::StorageAccessPermissionGrantedReason::
        eStorageAccessAPI:
      Telemetry::AccumulateCategorical(
          Telemetry::LABELS_STORAGE_ACCESS_GRANTED_COUNT::StorageAccessAPI);
      break;
    case ContentBlockingNotifier::StorageAccessPermissionGrantedReason::
        eOpenerAfterUserInteraction:
      Telemetry::AccumulateCategorical(
          Telemetry::LABELS_STORAGE_ACCESS_GRANTED_COUNT::OpenerAfterUI);
      break;
    case ContentBlockingNotifier::StorageAccessPermissionGrantedReason::eOpener:
      Telemetry::AccumulateCategorical(
          Telemetry::LABELS_STORAGE_ACCESS_GRANTED_COUNT::Opener);
      break;
    default:
      break;
  }

  // Theoratically this can be done in the parent process. But right now,
  // we need the channel while notifying content blocking events, and
  // we don't have a trivial way to obtain the channel in the parent
  // via BrowsingContext. So we just ask the child to do the work.
  ContentBlockingNotifier::OnEvent(
      doc->GetChannel(), false,
      nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER, aTrackingOrigin,
      Some(aReason));
}

/* static */
RefPtr<mozilla::StorageAccessAPIHelper::ParentAccessGrantPromise>
StorageAccessAPIHelper::SaveAccessForOriginOnParentProcess(
    uint64_t aTopLevelWindowId, BrowsingContext* aParentContext,
    nsIPrincipal* aTrackingPrincipal, int aAllowMode, bool aFrameOnly,
    uint64_t aExpirationTime) {
  MOZ_ASSERT(aTopLevelWindowId != 0);
  MOZ_ASSERT(aTrackingPrincipal);

  if (!aTrackingPrincipal || aTrackingPrincipal->IsSystemPrincipal() ||
      aTrackingPrincipal->GetIsNullPrincipal() ||
      aTrackingPrincipal->GetIsExpandedPrincipal()) {
    LOG(("aTrackingPrincipal is of invalid principal type"));
    return ParentAccessGrantPromise::CreateAndReject(false, __func__);
  }

  nsAutoCString trackingOrigin;
  nsresult rv = aTrackingPrincipal->GetOriginNoSuffix(trackingOrigin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return ParentAccessGrantPromise::CreateAndReject(false, __func__);
  }

  RefPtr<WindowGlobalParent> wgp =
      WindowGlobalParent::GetByInnerWindowId(aTopLevelWindowId);
  if (!wgp) {
    LOG(("Can't get window global parent"));
    return ParentAccessGrantPromise::CreateAndReject(false, __func__);
  }

  // If the permission is granted on a first-party window, also have to update
  // the permission to all the other windows with the same tracking origin (in
  // the same tab), if any, only it is not a frame-only permission grant which
  // does not propogate.
  if (!aFrameOnly) {
    StorageAccessAPIHelper::UpdateAllowAccessOnParentProcess(aParentContext,
                                                             trackingOrigin);
  }

  return StorageAccessAPIHelper::SaveAccessForOriginOnParentProcess(
      wgp->DocumentPrincipal(), aTrackingPrincipal, aAllowMode, aFrameOnly,
      aExpirationTime);
}

/* static */
RefPtr<mozilla::StorageAccessAPIHelper::ParentAccessGrantPromise>
StorageAccessAPIHelper::SaveAccessForOriginOnParentProcess(
    nsIPrincipal* aParentPrincipal, nsIPrincipal* aTrackingPrincipal,
    int aAllowMode, bool aFrameOnly, uint64_t aExpirationTime) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(aAllowMode == eAllow || aAllowMode == eAllowAutoGrant);

  if (!aParentPrincipal || !aTrackingPrincipal) {
    LOG(("Invalid input arguments passed"));
    return ParentAccessGrantPromise::CreateAndReject(false, __func__);
  };

  if (aTrackingPrincipal->IsSystemPrincipal() ||
      aTrackingPrincipal->GetIsNullPrincipal() ||
      aTrackingPrincipal->GetIsExpandedPrincipal()) {
    LOG(("aTrackingPrincipal is of invalid principal type"));
    return ParentAccessGrantPromise::CreateAndReject(false, __func__);
  }

  nsAutoCString trackingOrigin;
  nsresult rv = aTrackingPrincipal->GetOriginNoSuffix(trackingOrigin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return ParentAccessGrantPromise::CreateAndReject(false, __func__);
  }

  LOG_PRIN(("Saving a first-party storage permission on %s for "
            "trackingOrigin=%s",
            _spec, trackingOrigin.get()),
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
  rv = aParentPrincipal->GetPrivateBrowsingId(&privateBrowsingId);
  if ((!NS_WARN_IF(NS_FAILED(rv)) && privateBrowsingId > 0) ||
      (aAllowMode == eAllowAutoGrant)) {
    // If we are coming from a private window or are automatically granting a
    // permission, make sure to store a session-only permission which won't
    // get persisted to disk.
    expirationType = nsIPermissionManager::EXPIRE_SESSION;
    when = 0;
  }

  nsAutoCString type;
  if (aFrameOnly) {
    bool success = AntiTrackingUtils::CreateStorageFramePermissionKey(
        aTrackingPrincipal, type);
    if (NS_WARN_IF(!success)) {
      return ParentAccessGrantPromise::CreateAndReject(false, __func__);
    }
  } else {
    AntiTrackingUtils::CreateStoragePermissionKey(trackingOrigin, type);
  }

  LOG(
      ("Computed permission key: %s, expiry: %u, proceeding to save in the "
       "permission manager",
       type.get(), expirationTime));

  rv = permManager->AddFromPrincipal(aParentPrincipal, type,
                                     nsIPermissionManager::ALLOW_ACTION,
                                     expirationType, when);
  Unused << NS_WARN_IF(NS_FAILED(rv));

  if (StaticPrefs::privacy_antitracking_testing()) {
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    obs->NotifyObservers(nullptr, "antitracking-test-storage-access-perm-added",
                         nullptr);
  }

  if (NS_SUCCEEDED(rv) && (aAllowMode == eAllowAutoGrant)) {
    // Make sure temporary access grants do not survive more than 24 hours.
    TemporaryAccessGrantObserver::Create(permManager, aParentPrincipal, type);
  }

  LOG(("Result: %s", NS_SUCCEEDED(rv) ? "success" : "failure"));
  return ParentAccessGrantPromise::CreateAndResolve(rv, __func__);
}

// static
Maybe<bool>
StorageAccessAPIHelper::CheckCookiesPermittedDecidesStorageAccessAPI(
    nsICookieJarSettings* aCookieJarSettings,
    nsIPrincipal* aRequestingPrincipal) {
  MOZ_ASSERT(aCookieJarSettings);
  MOZ_ASSERT(aRequestingPrincipal);
  uint32_t cookiePermission = detail::CheckCookiePermissionForPrincipal(
      aCookieJarSettings, aRequestingPrincipal);
  if (cookiePermission == nsICookiePermission::ACCESS_ALLOW ||
      cookiePermission == nsICookiePermission::ACCESS_SESSION) {
    return Some(true);
  } else if (cookiePermission == nsICookiePermission::ACCESS_DENY) {
    return Some(false);
  }

  if (ContentBlockingAllowList::Check(aCookieJarSettings)) {
    return Some(true);
  }
  return Nothing();
}

// static
RefPtr<MozPromise<Maybe<bool>, nsresult, true>>
StorageAccessAPIHelper::AsyncCheckCookiesPermittedDecidesStorageAccessAPI(
    dom::BrowsingContext* aBrowsingContext,
    nsIPrincipal* aRequestingPrincipal) {
  MOZ_ASSERT(XRE_IsContentProcess());

  ContentChild* cc = ContentChild::GetSingleton();
  MOZ_ASSERT(cc);

  return cc
      ->SendTestCookiePermissionDecided(aBrowsingContext, aRequestingPrincipal)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [](const ContentChild::TestCookiePermissionDecidedPromise::
                 ResolveOrRejectValue& aPromise) {
            if (aPromise.IsResolve()) {
              return MozPromise<Maybe<bool>, nsresult, true>::CreateAndResolve(
                  aPromise.ResolveValue(), __func__);
            }
            return MozPromise<Maybe<bool>, nsresult, true>::CreateAndReject(
                NS_ERROR_UNEXPECTED, __func__);
          });
}

// static
Maybe<bool> StorageAccessAPIHelper::CheckBrowserSettingsDecidesStorageAccessAPI(
    nsICookieJarSettings* aCookieJarSettings, bool aThirdParty,
    bool aIsOnThirdPartySkipList, bool aIsThirdPartyTracker) {
  MOZ_ASSERT(aCookieJarSettings);
  uint32_t behavior = aCookieJarSettings->GetCookieBehavior();
  switch (behavior) {
    case nsICookieService::BEHAVIOR_ACCEPT:
      return Some(true);
    case nsICookieService::BEHAVIOR_REJECT_FOREIGN:
      if (!aThirdParty) {
        return Some(true);
      }
      return Some(false);
    case nsICookieService::BEHAVIOR_REJECT:
      return Some(false);
    case nsICookieService::BEHAVIOR_LIMIT_FOREIGN:
      if (!aThirdParty) {
        return Some(true);
      }
      return Some(false);
    case nsICookieService::BEHAVIOR_REJECT_TRACKER:
      if (!aIsThirdPartyTracker) {
        return Some(true);
      }
      if (aIsOnThirdPartySkipList) {
        return Some(true);
      }
      return Nothing();
    case nsICookieService::BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN:
      if (aIsOnThirdPartySkipList) {
        return Some(true);
      }
      return Nothing();
    default:
      MOZ_ASSERT_UNREACHABLE("Must not have undefined cookie behavior");
  }
  MOZ_ASSERT_UNREACHABLE("Must not have undefined cookie behavior");
  return Nothing();
}

// static
Maybe<bool> StorageAccessAPIHelper::CheckCallingContextDecidesStorageAccessAPI(
    Document* aDocument, bool aRequestingStorageAccess) {
  MOZ_ASSERT(aDocument);

  if (!aDocument->IsCurrentActiveDocument()) {
    return Some(false);
  }

  if (aRequestingStorageAccess) {
    // Perform a Permission Policy Request
    dom::FeaturePolicy* policy = aDocument->FeaturePolicy();
    MOZ_ASSERT(policy);

    if (!policy->AllowsFeature(u"storage-access"_ns,
                               dom::Optional<nsAString>())) {
      nsContentUtils::ReportToConsole(
          nsIScriptError::errorFlag, nsLiteralCString("requestStorageAccess"),
          aDocument, nsContentUtils::eDOM_PROPERTIES,
          "RequestStorageAccessPermissionsPolicy");
      return Some(false);
    }
  }

  RefPtr<BrowsingContext> bc = aDocument->GetBrowsingContext();
  if (!bc) {
    return Some(false);
  }

  // Check if NodePrincipal is not null
  if (!aDocument->NodePrincipal()) {
    return Some(false);
  }

  // If the document doesn't have a secure context, reject. The Static Pref is
  // used to pass existing tests that do not fulfil this check.
  if (StaticPrefs::dom_storage_access_dont_grant_insecure_contexts() &&
      !aDocument->NodePrincipal()->GetIsOriginPotentiallyTrustworthy()) {
    // Report the error to the console if we are requesting access
    if (aRequestingStorageAccess) {
      nsContentUtils::ReportToConsole(
          nsIScriptError::errorFlag, nsLiteralCString("requestStorageAccess"),
          aDocument, nsContentUtils::eDOM_PROPERTIES,
          "RequestStorageAccessNotSecureContext");
    }
    return Some(false);
  }

  // If the document has a null origin, reject.
  if (aDocument->NodePrincipal()->GetIsNullPrincipal()) {
    // Report an error to the console for this case if we are requesting access
    if (aRequestingStorageAccess) {
      nsContentUtils::ReportToConsole(
          nsIScriptError::errorFlag, nsLiteralCString("requestStorageAccess"),
          aDocument, nsContentUtils::eDOM_PROPERTIES,
          "RequestStorageAccessNullPrincipal");
    }
    return Some(false);
  }

  if (!AntiTrackingUtils::IsThirdPartyDocument(aDocument)) {
    return Some(true);
  }

  if (aDocument->IsTopLevelContentDocument()) {
    return Some(true);
  }

  if (aRequestingStorageAccess) {
    if (aDocument->StorageAccessSandboxed()) {
      nsContentUtils::ReportToConsole(
          nsIScriptError::errorFlag, nsLiteralCString("requestStorageAccess"),
          aDocument, nsContentUtils::eDOM_PROPERTIES,
          "RequestStorageAccessSandboxed");
      return Some(false);
    }
  }
  return Nothing();
}

// static
Maybe<bool>
StorageAccessAPIHelper::CheckSameSiteCallingContextDecidesStorageAccessAPI(
    dom::Document* aDocument, bool aRequireUserActivation) {
  MOZ_ASSERT(aDocument);
  if (aRequireUserActivation) {
    if (!aDocument->HasValidTransientUserGestureActivation()) {
      // Report an error to the console for this case
      nsContentUtils::ReportToConsole(
          nsIScriptError::errorFlag, nsLiteralCString("requestStorageAccess"),
          aDocument, nsContentUtils::eDOM_PROPERTIES,
          "RequestStorageAccessUserGesture");
      return Some(false);
    }
  }

  nsIChannel* chan = aDocument->GetChannel();
  if (!chan) {
    return Some(false);
  }
  nsCOMPtr<nsILoadInfo> loadInfo = chan->LoadInfo();
  if (loadInfo->GetIsThirdPartyContextToTopWindow()) {
    return Some(false);
  }

  // If the document has a null origin, reject.
  if (aDocument->NodePrincipal()->GetIsNullPrincipal()) {
    // Report an error to the console for this case
    nsContentUtils::ReportToConsole(nsIScriptError::errorFlag,
                                    nsLiteralCString("requestStorageAccess"),
                                    aDocument, nsContentUtils::eDOM_PROPERTIES,
                                    "RequestStorageAccessNullPrincipal");
    return Some(false);
  }
  return Maybe<bool>();
}

// static
Maybe<bool>
StorageAccessAPIHelper::CheckExistingPermissionDecidesStorageAccessAPI(
    dom::Document* aDocument, bool aRequestingStorageAccess) {
  MOZ_ASSERT(aDocument);
  if (aDocument->StorageAccessSandboxed()) {
    if (aRequestingStorageAccess) {
      nsContentUtils::ReportToConsole(
          nsIScriptError::errorFlag, nsLiteralCString("requestStorageAccess"),
          aDocument, nsContentUtils::eDOM_PROPERTIES,
          "RequestStorageAccessSandboxed");
    }
    return Some(false);
  }
  if (aDocument->UsingStorageAccess()) {
    return Some(true);
  }
  return Nothing();
}

// static
RefPtr<StorageAccessAPIHelper::StorageAccessPermissionGrantPromise>
StorageAccessAPIHelper::RequestStorageAccessAsyncHelper(
    dom::Document* aDocument, nsPIDOMWindowInner* aInnerWindow,
    dom::BrowsingContext* aBrowsingContext, nsIPrincipal* aPrincipal,
    bool aHasUserInteraction, bool aRequireUserInteraction, bool aFrameOnly,
    ContentBlockingNotifier::StorageAccessPermissionGrantedReason aNotifier,
    bool aRequireGrant) {
  MOZ_ASSERT(aDocument);

  if (!aRequireGrant) {
    // Try to allow access for the given principal.
    return StorageAccessAPIHelper::AllowAccessFor(aPrincipal, aBrowsingContext,
                                                  aNotifier);
  }

  RefPtr<nsIPrincipal> principal(aPrincipal);

  // This is a lambda function that has some variables bound to it. It will be
  // called later in CompleteAllowAccessFor inside of AllowAccessFor.
  auto performPermissionGrant = aDocument->CreatePermissionGrantPromise(
      aInnerWindow, principal, aHasUserInteraction, aRequireUserInteraction,
      Nothing(), aFrameOnly);

  // Try to allow access for the given principal.
  return StorageAccessAPIHelper::AllowAccessFor(
      principal, aBrowsingContext, aNotifier, performPermissionGrant);
}

// There are two methods to handle permission update:
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
void StorageAccessAPIHelper::UpdateAllowAccessOnCurrentProcess(
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

  // Propagate the storage permission to same-origin frames in the same tab.
  top->PreOrderWalk([&](BrowsingContext* aContext) {
    // Only check browsing contexts that are in-process.
    if (aContext->IsInProcess()) {
      nsAutoCString origin;
      Unused << AntiTrackingUtils::GetPrincipalAndTrackingOrigin(
          aContext, nullptr, origin);

      if (aTrackingOrigin == origin) {
        nsCOMPtr<nsPIDOMWindowInner> inner =
            AntiTrackingUtils::GetInnerWindow(aContext);
        if (inner) {
          inner->SaveStorageAccessPermissionGranted();
        }
      }
    }
  });
}

/* static */
void StorageAccessAPIHelper::UpdateAllowAccessOnParentProcess(
    BrowsingContext* aParentContext, const nsACString& aTrackingOrigin) {
  MOZ_ASSERT(XRE_IsParentProcess());

  nsAutoCString topKey;
  nsCOMPtr<nsIPrincipal> topPrincipal =
      AntiTrackingUtils::GetPrincipal(aParentContext->Top());
  PermissionManager::GetKeyForPrincipal(topPrincipal, false, true, topKey);

  // Propagate the storage permission to same-origin frames in the same
  // agent-cluster.
  for (const auto& topContext : aParentContext->Group()->Toplevels()) {
    if (topContext == aParentContext->Top()) {
      // In non-fission mode, storage permission is stored in the top-level,
      // don't need to propagate it to tracker frames.
      bool useRemoteSubframes;
      aParentContext->GetUseRemoteSubframes(&useRemoteSubframes);
      if (!useRemoteSubframes) {
        continue;
      }
      // If parent context is third-party, we already propagate permission
      // in the child process, skip propagating here.
      RefPtr<dom::WindowContext> ctx =
          aParentContext->GetCurrentWindowContext();
      if (ctx && ctx->GetIsThirdPartyWindow()) {
        continue;
      }
    } else {
      nsCOMPtr<nsIPrincipal> principal =
          AntiTrackingUtils::GetPrincipal(topContext);
      if (!principal) {
        continue;
      }

      nsAutoCString key;
      PermissionManager::GetKeyForPrincipal(principal, false, true, key);
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

      nsAutoCString origin;
      AntiTrackingUtils::GetPrincipalAndTrackingOrigin(aContext, nullptr,
                                                       origin);
      if (aTrackingOrigin == origin) {
        Unused << wgp->SendSaveStorageAccessPermissionGranted();
      }
    });
  }
}
