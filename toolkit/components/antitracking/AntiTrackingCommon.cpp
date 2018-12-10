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
#include "mozilla/Pair.h"
#include "mozilla/StaticPrefs.h"
#include "mozIThirdPartyUtil.h"
#include "nsContentUtils.h"
#include "nsGlobalWindowInner.h"
#include "nsCookiePermission.h"
#include "nsICookieService.h"
#include "nsIDocShell.h"
#include "nsIEffectiveTLDService.h"
#include "nsIHttpChannelInternal.h"
#include "nsIIOService.h"
#include "nsIParentChannel.h"
#include "nsIPermission.h"
#include "nsIPermissionManager.h"
#include "nsIPrincipal.h"
#include "nsIScriptError.h"
#include "nsIURI.h"
#include "nsIURIFixup.h"
#include "nsIURL.h"
#include "nsIWebProgressListener.h"
#include "nsNetUtil.h"
#include "nsPIDOMWindow.h"
#include "nsPrintfCString.h"
#include "nsScriptSecurityManager.h"
#include "nsSandboxFlags.h"
#include "prtime.h"

#define ANTITRACKING_PERM_KEY "3rdPartyStorage"

using namespace mozilla;
using mozilla::dom::ContentChild;

static LazyLogModule gAntiTrackingLog("AntiTracking");
static const nsCString::size_type sMaxSpecLength = 128;

#define LOG(format) MOZ_LOG(gAntiTrackingLog, mozilla::LogLevel::Debug, format)

#define LOG_SPEC(format, uri)                                       \
  PR_BEGIN_MACRO                                                    \
  if (MOZ_LOG_TEST(gAntiTrackingLog, mozilla::LogLevel::Debug)) {   \
    nsAutoCString _specStr(NS_LITERAL_CSTRING("(null)"));           \
    _specStr.Truncate(std::min(_specStr.Length(), sMaxSpecLength)); \
    if (uri) {                                                      \
      _specStr = uri->GetSpecOrDefault();                           \
    }                                                               \
    const char* _spec = _specStr.get();                             \
    LOG(format);                                                    \
  }                                                                 \
  PR_END_MACRO

namespace {

bool GetParentPrincipalAndTrackingOrigin(
    nsGlobalWindowInner* a3rdPartyTrackingWindow,
    nsIPrincipal** aTopLevelStoragePrincipal, nsACString& aTrackingOrigin,
    nsIURI** aTrackingURI, nsIPrincipal** aTrackingPrincipal) {
  if (!nsContentUtils::IsThirdPartyTrackingResourceWindow(
          a3rdPartyTrackingWindow)) {
    return false;
  }

  nsIDocument* doc = a3rdPartyTrackingWindow->GetDocument();
  // Make sure storage access isn't disabled
  if (doc && ((doc->GetSandboxFlags() & SANDBOXED_STORAGE_ACCESS) != 0 ||
              nsContentUtils::IsInPrivateBrowsing(doc))) {
    return false;
  }

  // Now we need the principal and the origin of the parent window.
  nsCOMPtr<nsIPrincipal> topLevelStoragePrincipal =
      a3rdPartyTrackingWindow->GetTopLevelStorageAreaPrincipal();
  if (!topLevelStoragePrincipal) {
    LOG(("No top-level storage area principal at hand"));
    return false;
  }

  // Let's take the principal and the origin of the tracker.
  nsCOMPtr<nsIPrincipal> trackingPrincipal =
      a3rdPartyTrackingWindow->GetPrincipal();
  if (NS_WARN_IF(!trackingPrincipal)) {
    return false;
  }

  nsCOMPtr<nsIURI> trackingURI;
  nsresult rv = trackingPrincipal->GetURI(getter_AddRefs(trackingURI));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  rv = trackingPrincipal->GetOriginNoSuffix(aTrackingOrigin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  topLevelStoragePrincipal.forget(aTopLevelStoragePrincipal);
  if (aTrackingURI) {
    trackingURI.forget(aTrackingURI);
  }
  if (aTrackingPrincipal) {
    trackingPrincipal.forget(aTrackingPrincipal);
  }
  return true;
};

void CreatePermissionKey(const nsCString& aTrackingOrigin,
                         const nsCString& aGrantedOrigin,
                         nsACString& aPermissionKey) {
  if (aTrackingOrigin == aGrantedOrigin) {
    aPermissionKey =
        nsPrintfCString(ANTITRACKING_PERM_KEY "^%s", aTrackingOrigin.get());
    return;
  }

  aPermissionKey = nsPrintfCString(ANTITRACKING_PERM_KEY "^%s^%s",
                                   aTrackingOrigin.get(), aGrantedOrigin.get());
}

// This internal method returns ACCESS_DENY if the access is denied,
// ACCESS_DEFAULT if unknown, some other access code if granted.
nsCookieAccess CheckCookiePermissionForPrincipal(nsIPrincipal* aPrincipal) {
  nsCookieAccess access = nsICookiePermission::ACCESS_DEFAULT;
  if (!aPrincipal->GetIsCodebasePrincipal()) {
    return access;
  }

  nsCOMPtr<nsICookiePermission> cps = nsCookiePermission::GetOrCreate();

  nsresult rv = cps->CanAccess(aPrincipal, &access);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return access;
  }

  // If we have a custom cookie permission, let's use it.
  return access;
}

int32_t CookiesBehavior(nsIPrincipal* aTopLevelPrincipal,
                        nsIPrincipal* a3rdPartyPrincipal) {
  // WebExtensions principals always get BEHAVIOR_ACCEPT as cookieBehavior
  // (See Bug 1406675 for rationale).
  if (BasePrincipal::Cast(aTopLevelPrincipal)->AddonPolicy()) {
    return nsICookieService::BEHAVIOR_ACCEPT;
  }

  if (a3rdPartyPrincipal &&
      BasePrincipal::Cast(a3rdPartyPrincipal)->AddonPolicy()) {
    return nsICookieService::BEHAVIOR_ACCEPT;
  }

  return StaticPrefs::network_cookie_cookieBehavior();
}

bool CheckContentBlockingAllowList(nsIURI* aTopWinURI,
                                   bool aIsPrivateBrowsing) {
  bool isAllowed = false;
  nsresult rv = AntiTrackingCommon::IsOnContentBlockingAllowList(
      aTopWinURI, aIsPrivateBrowsing, AntiTrackingCommon::eStorageChecks,
      isAllowed);
  if (NS_SUCCEEDED(rv) && isAllowed) {
    LOG_SPEC(
        ("The top-level window (%s) is on the content blocking allow list, "
         "bail out early",
         _spec),
        aTopWinURI);
    return true;
  }
  if (NS_FAILED(rv)) {
    LOG_SPEC(
        ("Checking the content blocking allow list for %s failed with %" PRIx32,
         _spec, static_cast<uint32_t>(rv)),
        aTopWinURI);
  }
  return false;
}

bool CheckContentBlockingAllowList(nsPIDOMWindowInner* aWindow) {
  nsPIDOMWindowOuter* top = aWindow->GetScriptableTop();
  if (top) {
    nsIURI* topWinURI = top->GetDocumentURI();
    nsIDocument* doc = top->GetExtantDoc();
    bool isPrivateBrowsing =
        doc ? nsContentUtils::IsInPrivateBrowsing(doc) : false;
    return CheckContentBlockingAllowList(topWinURI, isPrivateBrowsing);
  }

  LOG(
      ("Could not check the content blocking allow list because the top "
       "window wasn't accessible"));
  return false;
}

bool CheckContentBlockingAllowList(nsIHttpChannel* aChannel) {
  nsCOMPtr<nsIHttpChannelInternal> chan = do_QueryInterface(aChannel);
  if (chan) {
    nsCOMPtr<nsIURI> topWinURI;
    nsresult rv = chan->GetTopWindowURI(getter_AddRefs(topWinURI));
    if (NS_SUCCEEDED(rv)) {
      return CheckContentBlockingAllowList(topWinURI,
                                           NS_UsePrivateBrowsing(aChannel));
    }
  }

  LOG(
      ("Could not check the content blocking allow list because the top "
       "window wasn't accessible"));
  return false;
}

void ReportBlockingToConsole(nsPIDOMWindowOuter* aWindow, nsIURI* aURI,
                             uint32_t aRejectedReason) {
  MOZ_ASSERT(aWindow && aURI);
  MOZ_ASSERT(
      aRejectedReason == 0 ||
      aRejectedReason ==
          nsIWebProgressListener::STATE_COOKIES_BLOCKED_BY_PERMISSION ||
      aRejectedReason ==
          nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER ||
      aRejectedReason == nsIWebProgressListener::STATE_COOKIES_BLOCKED_ALL ||
      aRejectedReason == nsIWebProgressListener::STATE_COOKIES_BLOCKED_FOREIGN);

  nsCOMPtr<nsIDocShell> docShell = aWindow->GetDocShell();
  if (NS_WARN_IF(!docShell)) {
    return;
  }

  nsCOMPtr<nsIDocument> doc = docShell->GetDocument();
  if (NS_WARN_IF(!doc)) {
    return;
  }

  const char* message = nullptr;
  switch (aRejectedReason) {
    case nsIWebProgressListener::STATE_COOKIES_BLOCKED_BY_PERMISSION:
      message = "CookieBlockedByPermission";
      break;

    case nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER:
      message = "CookieBlockedTracker";
      break;

    case nsIWebProgressListener::STATE_COOKIES_BLOCKED_ALL:
      message = "CookieBlockedAll";
      break;

    case nsIWebProgressListener::STATE_COOKIES_BLOCKED_FOREIGN:
      message = "CookieBlockedForeign";
      break;

    default:
      return;
  }

  MOZ_ASSERT(message);

  // Strip the URL of any possible username/password and make it ready to be
  // presented in the UI.
  nsCOMPtr<nsIURIFixup> urifixup = services::GetURIFixup();
  NS_ENSURE_TRUE_VOID(urifixup);
  nsCOMPtr<nsIURI> exposableURI;
  nsresult rv =
      urifixup->CreateExposableURI(aURI, getter_AddRefs(exposableURI));
  NS_ENSURE_SUCCESS_VOID(rv);

  NS_ConvertUTF8toUTF16 spec(exposableURI->GetSpecOrDefault());
  const char16_t* params[] = {spec.get()};

  nsContentUtils::ReportToConsole(
      nsIScriptError::warningFlag, NS_LITERAL_CSTRING("Content Blocking"), doc,
      nsContentUtils::eNECKO_PROPERTIES, message, params, ArrayLength(params));
}

void ReportUnblockingConsole(
    nsPIDOMWindowInner* aWindow, const nsAString& aTrackingOrigin,
    const nsAString& aGrantedOrigin,
    AntiTrackingCommon::StorageAccessGrantedReason aReason) {
  nsCOMPtr<nsIPrincipal> principal =
      nsGlobalWindowInner::Cast(aWindow)->GetPrincipal();
  if (NS_WARN_IF(!principal)) {
    return;
  }

  nsAutoString origin;
  nsresult rv = nsContentUtils::GetUTFOrigin(principal, origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  nsCOMPtr<nsIDocShell> docShell = aWindow->GetDocShell();
  if (NS_WARN_IF(!docShell)) {
    return;
  }

  nsCOMPtr<nsIDocument> doc = docShell->GetDocument();
  if (NS_WARN_IF(!doc)) {
    return;
  }

  const char16_t* params[] = {origin.BeginReading(),
                              aTrackingOrigin.BeginReading(),
                              aGrantedOrigin.BeginReading()};
  const char* messageWithDifferentOrigin = nullptr;
  const char* messageWithSameOrigin = nullptr;

  switch (aReason) {
    case AntiTrackingCommon::eStorageAccessAPI:
      messageWithDifferentOrigin =
          "CookieAllowedForOriginOnTrackerByStorageAccessAPI";
      messageWithSameOrigin = "CookieAllowedForTrackerByStorageAccessAPI";
      break;

    case AntiTrackingCommon::eOpenerAfterUserInteraction:
      MOZ_FALLTHROUGH;
    case AntiTrackingCommon::eOpener:
      messageWithDifferentOrigin = "CookieAllowedForOriginOnTrackerByHeuristic";
      messageWithSameOrigin = "CookieAllowedForTrackerByHeuristic";
      break;
  }

  if (aTrackingOrigin == aGrantedOrigin) {
    nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                    NS_LITERAL_CSTRING("Content Blocking"), doc,
                                    nsContentUtils::eNECKO_PROPERTIES,
                                    messageWithSameOrigin, params, 2);
  } else {
    nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                    NS_LITERAL_CSTRING("Content Blocking"), doc,
                                    nsContentUtils::eNECKO_PROPERTIES,
                                    messageWithDifferentOrigin, params, 3);
  }
}

already_AddRefed<nsPIDOMWindowOuter> GetTopWindow(nsPIDOMWindowInner* aWindow) {
  nsIDocument* document = aWindow->GetExtantDoc();
  if (!document) {
    return nullptr;
  }

  nsIChannel* channel = document->GetChannel();
  if (!channel) {
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowOuter> pwin;
  auto* outer = nsGlobalWindowOuter::Cast(aWindow->GetOuterWindow());
  if (outer) {
    pwin = outer->GetTopOuter();
  }

  if (!pwin) {
    return nullptr;
  }

  return pwin.forget();
}

bool CompareBaseDomains(nsIURI* aTrackingURI, nsIURI* aParentPrincipalBaseURI) {
  nsCOMPtr<nsIEffectiveTLDService> eTLDService =
      services::GetEffectiveTLDService();
  if (NS_WARN_IF(!eTLDService)) {
    LOG(("Failed to get the TLD service"));
    return false;
  }

  nsAutoCString trackingBaseDomain;
  nsAutoCString parentPrincipalBaseDomain;
  nsresult rv = eTLDService->GetBaseDomain(aTrackingURI, 0, trackingBaseDomain);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG(("Can't get the base domain from tracking URI"));
    return false;
  }
  rv = eTLDService->GetBaseDomain(aParentPrincipalBaseURI, 0,
                                  parentPrincipalBaseDomain);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG(("Can't get the base domain from parent principal"));
    return false;
  }

  return trackingBaseDomain.Equals(parentPrincipalBaseDomain,
                                   nsCaseInsensitiveCStringComparator());
}

class TemporaryAccessGrantObserver final : public nsIObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static void Create(nsIPermissionManager* aPM, nsIPrincipal* aPrincipal,
                     const nsACString& aType) {
    nsCOMPtr<nsITimer> timer;
    RefPtr<TemporaryAccessGrantObserver> observer =
        new TemporaryAccessGrantObserver(aPM, aPrincipal, aType);
    nsresult rv = NS_NewTimerWithObserver(getter_AddRefs(timer), observer,
                                          24 * 60 * 60 * 1000,  // 24 hours
                                          nsITimer::TYPE_ONE_SHOT);

    if (NS_SUCCEEDED(rv)) {
      observer->SetTimer(timer);
    } else {
      timer->Cancel();
    }
  }

  void SetTimer(nsITimer* aTimer) {
    mTimer = aTimer;
    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    if (observerService) {
      observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
    }
  }

 private:
  TemporaryAccessGrantObserver(nsIPermissionManager* aPM,
                               nsIPrincipal* aPrincipal,
                               const nsACString& aType)
      : mPM(aPM), mPrincipal(aPrincipal), mType(aType) {
    MOZ_ASSERT(XRE_IsParentProcess(),
               "Enforcing temporary access grant lifetimes can only be done in "
               "the parent process");
  }

  ~TemporaryAccessGrantObserver() = default;

 private:
  nsCOMPtr<nsITimer> mTimer;
  nsCOMPtr<nsIPermissionManager> mPM;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCString mType;
};

NS_IMPL_ISUPPORTS(TemporaryAccessGrantObserver, nsIObserver)

NS_IMETHODIMP
TemporaryAccessGrantObserver::Observe(nsISupports* aSubject, const char* aTopic,
                                      const char16_t* aData) {
  if (strcmp(aTopic, NS_TIMER_CALLBACK_TOPIC) == 0) {
    Unused << mPM->RemoveFromPrincipal(mPrincipal, mType.get());
  } else if (strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    if (observerService) {
      observerService->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
    }
    if (mTimer) {
      mTimer->Cancel();
      mTimer = nullptr;
    }
  }

  return NS_OK;
}

}  // namespace

/* static */ RefPtr<AntiTrackingCommon::StorageAccessGrantPromise>
AntiTrackingCommon::AddFirstPartyStorageAccessGrantedFor(
    nsIPrincipal* aPrincipal, nsPIDOMWindowInner* aParentWindow,
    StorageAccessGrantedReason aReason,
    const AntiTrackingCommon::PerformFinalChecks& aPerformFinalChecks) {
  MOZ_ASSERT(aParentWindow);

  nsCOMPtr<nsIURI> uri;
  aPrincipal->GetURI(getter_AddRefs(uri));
  if (NS_WARN_IF(!uri)) {
    LOG(("Can't get the URI from the principal"));
    return StorageAccessGrantPromise::CreateAndReject(false, __func__);
  }

  nsAutoString origin;
  nsresult rv = nsContentUtils::GetUTFOrigin(uri, origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG(("Can't get the origin from the URI"));
    return StorageAccessGrantPromise::CreateAndReject(false, __func__);
  }

  LOG(("Adding a first-party storage exception for %s...",
       NS_ConvertUTF16toUTF8(origin).get()));

  if (StaticPrefs::network_cookie_cookieBehavior() !=
      nsICookieService::BEHAVIOR_REJECT_TRACKER) {
    LOG(
        ("Disabled by network.cookie.cookieBehavior pref (%d), bailing out "
         "early",
         StaticPrefs::network_cookie_cookieBehavior()));
    return StorageAccessGrantPromise::CreateAndResolve(eAllowOnAnySite,
                                                       __func__);
  }

  if (CheckContentBlockingAllowList(aParentWindow)) {
    return StorageAccessGrantPromise::CreateAndResolve(eAllowOnAnySite,
                                                       __func__);
  }

  nsCOMPtr<nsIPrincipal> topLevelStoragePrincipal;
  nsCOMPtr<nsIURI> trackingURI;
  nsAutoCString trackingOrigin;
  nsCOMPtr<nsIPrincipal> trackingPrincipal;

  RefPtr<nsGlobalWindowInner> parentWindow =
      nsGlobalWindowInner::Cast(aParentWindow);
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
    CopyUTF16toUTF8(origin, trackingOrigin);
    trackingPrincipal = aPrincipal;
    rv = trackingPrincipal->GetURI(getter_AddRefs(trackingURI));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      LOG(("Couldn't get the tracking principal URI"));
      return StorageAccessGrantPromise::CreateAndReject(false, __func__);
    }
    topLevelStoragePrincipal = parentWindow->GetPrincipal();
    if (NS_WARN_IF(!topLevelStoragePrincipal)) {
      LOG(("Top-level storage area principal not found, bailing out early"));
      return StorageAccessGrantPromise::CreateAndReject(false, __func__);
    }

    // We are a 3rd party source.
  } else if (!GetParentPrincipalAndTrackingOrigin(
                 parentWindow, getter_AddRefs(topLevelStoragePrincipal),
                 trackingOrigin, getter_AddRefs(trackingURI),
                 getter_AddRefs(trackingPrincipal))) {
    LOG(
        ("Error while computing the parent principal and tracking origin, "
         "bailing out early"));
    return StorageAccessGrantPromise::CreateAndReject(false, __func__);
  }

  nsCOMPtr<nsPIDOMWindowOuter> topOuterWindow = outerParentWindow->GetTop();
  nsGlobalWindowOuter* topWindow = nsGlobalWindowOuter::Cast(topOuterWindow);
  if (NS_WARN_IF(!topWindow)) {
    LOG(("No top outer window."));
    return StorageAccessGrantPromise::CreateAndReject(false, __func__);
  }

  nsPIDOMWindowInner* topInnerWindow = topWindow->GetCurrentInnerWindow();
  if (NS_WARN_IF(!topInnerWindow)) {
    LOG(("No top inner window."));
    return StorageAccessGrantPromise::CreateAndReject(false, __func__);
  }

  // We hardcode this block reason since the first-party storage access
  // permission is granted for the purpose of blocking trackers.
  // Note that if aReason is eOpenerAfterUserInteraction and the
  // trackingPrincipal is not in a blacklist, we don't check the
  // user-interaction state, because it could be that the current process has
  // just sent the request to store the user-interaction permission into the
  // parent, without having received the permission itself yet.
  //
  // We define this as an enum, since without that MSVC fails to capturing this
  // name inside the lambda without the explicit capture and clang warns if
  // there is an explicit capture with -Wunused-lambda-capture.
  enum : uint32_t {
    blockReason = nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER
  };
  if ((aReason != eOpenerAfterUserInteraction ||
       nsContentUtils::IsURIInPrefList(trackingURI,
                                       "privacy.restrict3rdpartystorage."
                                       "userInteractionRequiredForHosts")) &&
      !HasUserInteraction(trackingPrincipal)) {
    LOG_SPEC(("Tracking principal (%s) hasn't been interacted with before, "
              "refusing to add a first-party storage permission to access it",
              _spec),
             trackingURI);
    NotifyBlockingDecision(aParentWindow, BlockingDecision::eBlock,
                           blockReason);
    return StorageAccessGrantPromise::CreateAndReject(false, __func__);
  }

  nsCOMPtr<nsPIDOMWindowOuter> pwin = GetTopWindow(parentWindow);
  if (!pwin) {
    LOG(("Couldn't get the top window"));
    return StorageAccessGrantPromise::CreateAndReject(false, __func__);
  }

  auto storePermission =
      [pwin, parentWindow, origin, trackingOrigin, trackingPrincipal,
       trackingURI, topInnerWindow, topLevelStoragePrincipal,
       aReason](int aAllowMode) -> RefPtr<StorageAccessGrantPromise> {
    NS_ConvertUTF16toUTF8 grantedOrigin(origin);

    nsAutoCString permissionKey;
    CreatePermissionKey(trackingOrigin, grantedOrigin, permissionKey);

    // Let's store the permission in the current parent window.
    topInnerWindow->SaveStorageAccessGranted(permissionKey);

    // Let's inform the parent window.
    parentWindow->StorageAccessGranted();

    nsIChannel* channel =
        pwin->GetCurrentInnerWindow()->GetExtantDoc()->GetChannel();

    pwin->NotifyContentBlockingState(blockReason, channel, false, trackingURI);

    ReportUnblockingConsole(parentWindow, NS_ConvertUTF8toUTF16(trackingOrigin),
                            origin, aReason);

    if (XRE_IsParentProcess()) {
      LOG(("Saving the permission: trackingOrigin=%s, grantedOrigin=%s",
           trackingOrigin.get(), grantedOrigin.get()));

      return SaveFirstPartyStorageAccessGrantedForOriginOnParentProcess(
                 topLevelStoragePrincipal, trackingPrincipal, trackingOrigin,
                 grantedOrigin, aAllowMode)
          ->Then(GetCurrentThreadSerialEventTarget(), __func__,
                 [](FirstPartyStorageAccessGrantPromise::ResolveOrRejectValue&&
                        aValue) {
                   if (aValue.IsResolve()) {
                     return StorageAccessGrantPromise::CreateAndResolve(
                         NS_SUCCEEDED(aValue.ResolveValue()) ? eAllowOnAnySite
                                                             : eAllow,
                         __func__);
                   }
                   return StorageAccessGrantPromise::CreateAndReject(false,
                                                                     __func__);
                 });
    }

    ContentChild* cc = ContentChild::GetSingleton();
    MOZ_ASSERT(cc);

    LOG(
        ("Asking the parent process to save the permission for us: "
         "trackingOrigin=%s, grantedOrigin=%s",
         trackingOrigin.get(), grantedOrigin.get()));

    // This is not really secure, because here we have the content process
    // sending the request of storing a permission.
    return cc
        ->SendFirstPartyStorageAccessGrantedForOrigin(
            IPC::Principal(topLevelStoragePrincipal),
            IPC::Principal(trackingPrincipal), trackingOrigin, grantedOrigin,
            aAllowMode)
        ->Then(GetCurrentThreadSerialEventTarget(), __func__,
               [](const ContentChild::
                      FirstPartyStorageAccessGrantedForOriginPromise::
                          ResolveOrRejectValue& aValue) {
                 if (aValue.IsResolve()) {
                   return StorageAccessGrantPromise::CreateAndResolve(
                       aValue.ResolveValue(), __func__);
                 }
                 return StorageAccessGrantPromise::CreateAndReject(false,
                                                                   __func__);
               });
  };

  if (aPerformFinalChecks) {
    return aPerformFinalChecks()->Then(
        GetCurrentThreadSerialEventTarget(), __func__,
        [storePermission](
            StorageAccessGrantPromise::ResolveOrRejectValue&& aValue) {
          if (aValue.IsResolve()) {
            return storePermission(aValue.ResolveValue());
          }
          return StorageAccessGrantPromise::CreateAndReject(false, __func__);
        });
  }
  return storePermission(false);
}

/* static */ RefPtr<
    mozilla::AntiTrackingCommon::FirstPartyStorageAccessGrantPromise>
AntiTrackingCommon::SaveFirstPartyStorageAccessGrantedForOriginOnParentProcess(
    nsIPrincipal* aParentPrincipal, nsIPrincipal* aTrackingPrincipal,
    const nsCString& aTrackingOrigin, const nsCString& aGrantedOrigin,
    int aAllowMode) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(aAllowMode == eAllow || aAllowMode == eAllowAutoGrant ||
             aAllowMode == eAllowOnAnySite);

  nsCOMPtr<nsIURI> parentPrincipalURI;
  Unused << aParentPrincipal->GetURI(getter_AddRefs(parentPrincipalURI));
  LOG_SPEC(("Saving a first-party storage permission on %s for "
            "trackingOrigin=%s grantedOrigin=%s",
            _spec, aTrackingOrigin.get(), aGrantedOrigin.get()),
           parentPrincipalURI);

  if (NS_WARN_IF(!aParentPrincipal)) {
    // The child process is sending something wrong. Let's ignore it.
    LOG(("aParentPrincipal is null, bailing out early"));
    return FirstPartyStorageAccessGrantPromise::CreateAndReject(false,
                                                                __func__);
  }

  nsCOMPtr<nsIPermissionManager> pm = services::GetPermissionManager();
  if (NS_WARN_IF(!pm)) {
    LOG(("Permission manager is null, bailing out early"));
    return FirstPartyStorageAccessGrantPromise::CreateAndReject(false,
                                                                __func__);
  }

  // Remember that this pref is stored in seconds!
  uint32_t expirationType = nsIPermissionManager::EXPIRE_TIME;
  uint32_t expirationTime =
      StaticPrefs::privacy_restrict3rdpartystorage_expiration() * 1000;
  int64_t when = (PR_Now() / PR_USEC_PER_MSEC) + expirationTime;

  nsresult rv;
  if (aAllowMode == eAllowOnAnySite) {
    uint32_t privateBrowsingId = 0;
    rv = aTrackingPrincipal->GetPrivateBrowsingId(&privateBrowsingId);
    if (!NS_WARN_IF(NS_FAILED(rv)) && privateBrowsingId > 0) {
      // If we are coming from a private window, make sure to store a
      // session-only permission which won't get persisted to disk.
      expirationType = nsIPermissionManager::EXPIRE_SESSION;
      when = 0;
    }

    LOG(
        ("Setting 'any site' permission expiry: %u, proceeding to save in the "
         "permission manager",
         expirationTime));

    rv = pm->AddFromPrincipal(aTrackingPrincipal, "cookie",
                              nsICookiePermission::ACCESS_ALLOW, expirationType,
                              when);
  } else {
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
    CreatePermissionKey(aTrackingOrigin, aGrantedOrigin, type);

    LOG(
        ("Computed permission key: %s, expiry: %u, proceeding to save in the "
         "permission manager",
         type.get(), expirationTime));

    rv = pm->AddFromPrincipal(aParentPrincipal, type.get(),
                              nsIPermissionManager::ALLOW_ACTION,
                              expirationType, when);

    if (NS_SUCCEEDED(rv) && (aAllowMode == eAllowAutoGrant)) {
      // Make sure temporary access grants do not survive more than 24 hours.
      TemporaryAccessGrantObserver::Create(pm, aParentPrincipal, type);
    }
  }
  Unused << NS_WARN_IF(NS_FAILED(rv));

  LOG(("Result: %s", NS_SUCCEEDED(rv) ? "success" : "failure"));
  return FirstPartyStorageAccessGrantPromise::CreateAndResolve(rv, __func__);
}

// static
bool AntiTrackingCommon::IsStorageAccessPermission(nsIPermission* aPermission,
                                                   nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(aPermission);
  MOZ_ASSERT(aPrincipal);

  nsAutoCString origin;
  nsresult rv = aPrincipal->GetOriginNoSuffix(origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  // The permission key may belong either to a tracking origin on the same
  // origin as the granted origin, or on another origin as the granted origin
  // (for example when a tracker in a third-party context uses window.open to
  // open another origin where that second origin would be the granted origin.)
  // But even in the second case, the type of the permission would still be
  // formed by concatenating the granted origin to the end of the type name
  // (see CreatePermissionKey).  Therefore, we pass in the same argument to
  // both tracking origin and granted origin here in order to compute the
  // shorter permission key and will then do a prefix match on the type of the
  // input permission to see if it is a storage access permission or not.
  nsAutoCString permissionKey;
  CreatePermissionKey(origin, origin, permissionKey);

  nsAutoCString type;
  rv = aPermission->GetType(type);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  return StringBeginsWith(type, permissionKey);
}

bool AntiTrackingCommon::IsFirstPartyStorageAccessGrantedFor(
    nsPIDOMWindowInner* aWindow, nsIURI* aURI, uint32_t* aRejectedReason) {
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
  nsIPrincipal* windowPrincipal = innerWindow->GetPrincipal();
  if (!windowPrincipal) {
    LOG(("Our window has no principal"));
    return false;
  }

  nsIPrincipal* toplevelPrincipal = innerWindow->GetTopLevelPrincipal();
  if (!toplevelPrincipal) {
    // We are already the top-level principal. Let's use the window's principal.
    LOG(
        ("Our inner window lacks a top-level principal, use the window's "
         "principal instead"));
    toplevelPrincipal = windowPrincipal;
  }

  MOZ_ASSERT(toplevelPrincipal);

  nsCookieAccess access = CheckCookiePermissionForPrincipal(toplevelPrincipal);
  if (access != nsICookiePermission::ACCESS_DEFAULT) {
    LOG(
        ("CheckCookiePermissionForPrincipal() returned a non-default access "
         "code (%d) for top-level window's principal, returning %s",
         int(access),
         access != nsICookiePermission::ACCESS_DENY ? "success" : "failure"));
    if (access != nsICookiePermission::ACCESS_DENY) {
      return true;
    }

    *aRejectedReason =
        nsIWebProgressListener::STATE_COOKIES_BLOCKED_BY_PERMISSION;
    return false;
  }

  access = CheckCookiePermissionForPrincipal(windowPrincipal);
  if (access != nsICookiePermission::ACCESS_DEFAULT) {
    LOG(
        ("CheckCookiePermissionForPrincipal() returned a non-default access "
         "code (%d) for window's principal, returning %s",
         int(access),
         access != nsICookiePermission::ACCESS_DENY ? "success" : "failure"));
    if (access != nsICookiePermission::ACCESS_DENY) {
      return true;
    }

    *aRejectedReason =
        nsIWebProgressListener::STATE_COOKIES_BLOCKED_BY_PERMISSION;
    return false;
  }

  int32_t behavior = CookiesBehavior(toplevelPrincipal, windowPrincipal);
  if (behavior == nsICookieService::BEHAVIOR_ACCEPT) {
    LOG(("The cookie behavior pref mandates accepting all cookies!"));
    return true;
  }

  if (CheckContentBlockingAllowList(aWindow)) {
    return true;
  }

  if (behavior == nsICookieService::BEHAVIOR_REJECT) {
    LOG(("The cookie behavior pref mandates rejecting all cookies!"));
    *aRejectedReason = nsIWebProgressListener::STATE_COOKIES_BLOCKED_ALL;
    return false;
  }

  // As a performance optimization, we only perform this check for
  // BEHAVIOR_REJECT_FOREIGN and BEHAVIOR_LIMIT_FOREIGN.  For
  // BEHAVIOR_REJECT_TRACKER, third-partiness is implicily checked later below.
  if (behavior != nsICookieService::BEHAVIOR_REJECT_TRACKER) {
    // Let's check if this is a 3rd party context.
    if (!nsContentUtils::IsThirdPartyWindowOrChannel(aWindow, nullptr, aURI)) {
      LOG(("Our window isn't a third-party window"));
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

  if (!nsContentUtils::IsThirdPartyTrackingResourceWindow(aWindow)) {
    LOG(("Our window isn't a third-party tracking window"));
    return true;
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
    MOZ_ASSERT(nsContentUtils::IsThirdPartyWindowOrChannel(
                   aWindow, nullptr, aURI) == NS_SUCCEEDED(rv));
  }
#endif

  nsCOMPtr<nsIPrincipal> parentPrincipal;
  nsCOMPtr<nsIURI> parentPrincipalURI;
  nsCOMPtr<nsIURI> trackingURI;
  nsAutoCString trackingOrigin;
  if (!GetParentPrincipalAndTrackingOrigin(
          nsGlobalWindowInner::Cast(aWindow), getter_AddRefs(parentPrincipal),
          trackingOrigin, getter_AddRefs(trackingURI), nullptr)) {
    LOG(("Failed to obtain the parent principal and the tracking origin"));
    return false;
  }
  Unused << parentPrincipal->GetURI(getter_AddRefs(parentPrincipalURI));

  if (CompareBaseDomains(trackingURI, parentPrincipalURI)) {
    LOG(
        ("Grant access across the same eTLD+1 because same domain trackers "
         "are considered part of the same organization"));

    return true;
  }

  nsAutoString origin;
  nsresult rv = nsContentUtils::GetUTFOrigin(aURI, origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG_SPEC(("Failed to compute the origin from %s", _spec), aURI);
    return false;
  }

  NS_ConvertUTF16toUTF8 grantedOrigin(origin);

  nsGlobalWindowOuter* outerWindow =
      nsGlobalWindowOuter::Cast(aWindow->GetOuterWindow());
  if (NS_WARN_IF(!outerWindow)) {
    LOG(("No outer window."));
    return false;
  }

  nsCOMPtr<nsPIDOMWindowOuter> topOuterWindow = outerWindow->GetTop();
  nsGlobalWindowOuter* topWindow = nsGlobalWindowOuter::Cast(topOuterWindow);
  if (NS_WARN_IF(!topWindow)) {
    LOG(("No top outer window."));
    return false;
  }

  nsPIDOMWindowInner* topInnerWindow = topWindow->GetCurrentInnerWindow();
  if (NS_WARN_IF(!topInnerWindow)) {
    LOG(("No top inner window."));
    return false;
  }

  nsAutoCString type;
  CreatePermissionKey(trackingOrigin, grantedOrigin, type);

  if (topInnerWindow->HasStorageAccessGranted(type)) {
    LOG(("Permission stored in the window. All good."));
    return true;
  }

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

  LOG_SPEC(
      ("Testing permission type %s for %s resulted in %d (%s)", type.get(),
       _spec, int(result),
       result == nsIPermissionManager::ALLOW_ACTION ? "success" : "failure"),
      parentPrincipalURI);

  if (result != nsIPermissionManager::ALLOW_ACTION) {
    *aRejectedReason = nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER;
    return false;
  }

  return true;
}

bool AntiTrackingCommon::IsFirstPartyStorageAccessGrantedFor(
    nsIHttpChannel* aChannel, nsIURI* aURI, uint32_t* aRejectedReason) {
  MOZ_ASSERT(aURI);
  MOZ_ASSERT(aChannel);

  // Let's avoid a null check on aRejectedReason everywhere else.
  uint32_t rejectedReason = 0;
  if (!aRejectedReason) {
    aRejectedReason = &rejectedReason;
  }

  nsCOMPtr<nsIURI> channelURI;
  Unused << aChannel->GetURI(getter_AddRefs(channelURI));
  LOG_SPEC(
      ("Computing whether channel %p has access to URI %s", aChannel, _spec),
      channelURI);

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->GetLoadInfo();
  if (!loadInfo) {
    LOG(("No loadInfo, bail out early"));
    return true;
  }

  // We need to find the correct principal to check the cookie permission. For
  // third-party contexts, we want to check if the top-level window has a custom
  // cookie permission.
  nsIPrincipal* toplevelPrincipal = loadInfo->GetTopLevelPrincipal();

  // If this is already the top-level window, we should use the loading
  // principal.
  if (!toplevelPrincipal) {
    LOG(
        ("Our loadInfo lacks a top-level principal, use the loadInfo's loading "
         "principal instead"));
    toplevelPrincipal = loadInfo->LoadingPrincipal();
  }

  nsCOMPtr<nsIPrincipal> channelPrincipal;
  nsIScriptSecurityManager* ssm =
      nsScriptSecurityManager::GetScriptSecurityManager();
  nsresult rv = ssm->GetChannelResultPrincipal(
      aChannel, getter_AddRefs(channelPrincipal));

  // If we don't have a loading principal and this is a document channel, we are
  // a top-level window!
  if (!toplevelPrincipal) {
    LOG(
        ("We don't have a loading principal, let's see if this is a document "
         "channel"
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
    LOG(
        ("Our loadInfo lacks a top-level principal, use the loadInfo's "
         "triggering principal instead"));
    toplevelPrincipal = loadInfo->TriggeringPrincipal();
  }

  if (NS_WARN_IF(!toplevelPrincipal)) {
    LOG(("No top-level principal! Bail out early"));
    return false;
  }

  nsCookieAccess access = CheckCookiePermissionForPrincipal(toplevelPrincipal);
  if (access != nsICookiePermission::ACCESS_DEFAULT) {
    LOG(
        ("CheckCookiePermissionForPrincipal() returned a non-default access "
         "code (%d) for top-level window's principal, returning %s",
         int(access),
         access != nsICookiePermission::ACCESS_DENY ? "success" : "failure"));
    if (access != nsICookiePermission::ACCESS_DENY) {
      return true;
    }

    *aRejectedReason =
        nsIWebProgressListener::STATE_COOKIES_BLOCKED_BY_PERMISSION;
    return false;
  }

  if (NS_WARN_IF(NS_FAILED(rv) || !channelPrincipal)) {
    LOG(("No channel principal, bail out early"));
    return false;
  }

  access = CheckCookiePermissionForPrincipal(channelPrincipal);
  if (access != nsICookiePermission::ACCESS_DEFAULT) {
    LOG(
        ("CheckCookiePermissionForPrincipal() returned a non-default access "
         "code (%d) for channel's principal, returning %s",
         int(access),
         access != nsICookiePermission::ACCESS_DENY ? "success" : "failure"));
    if (access != nsICookiePermission::ACCESS_DENY) {
      return true;
    }

    *aRejectedReason =
        nsIWebProgressListener::STATE_COOKIES_BLOCKED_BY_PERMISSION;
    return false;
  }

  int32_t behavior = CookiesBehavior(toplevelPrincipal, channelPrincipal);
  if (behavior == nsICookieService::BEHAVIOR_ACCEPT) {
    LOG(("The cookie behavior pref mandates accepting all cookies!"));
    return true;
  }

  if (CheckContentBlockingAllowList(aChannel)) {
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

  // Not a tracker.
  if (!aChannel->GetIsThirdPartyTrackingResource()) {
    LOG(("Our channel isn't a third-party tracking channel"));
    return true;
  }

  nsIPrincipal* parentPrincipal = loadInfo->GetTopLevelStorageAreaPrincipal();
  if (!parentPrincipal) {
    LOG(("No top-level storage area principal at hand"));

    // parentPrincipal can be null if the parent window is not the top-level
    // window.
    if (loadInfo->GetTopLevelPrincipal()) {
      LOG(("Parent window is the top-level window, bail out early"));
      return false;
    }

    parentPrincipal = toplevelPrincipal;
    if (NS_WARN_IF(!parentPrincipal)) {
      LOG(
          ("No triggering principal, this shouldn't be happening! Bail out "
           "early"));
      // Why we are here?!?
      return true;
    }
  }

  nsCOMPtr<nsIURI> parentPrincipalURI;
  Unused << parentPrincipal->GetURI(getter_AddRefs(parentPrincipalURI));

  // Let's see if we have to grant the access for this particular channel.

  nsCOMPtr<nsIURI> trackingURI;
  rv = aChannel->GetURI(getter_AddRefs(trackingURI));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG(("Failed to get the channel URI"));
    return true;
  }

  if (CompareBaseDomains(trackingURI, parentPrincipalURI)) {
    LOG(
        ("Grant access across the same eTLD+1 because same domain trackers "
         "are considered part of the same organization"));

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

  LOG_SPEC(
      ("Testing permission type %s for %s resulted in %d (%s)", type.get(),
       _spec, int(result),
       result == nsIPermissionManager::ALLOW_ACTION ? "success" : "failure"),
      parentPrincipalURI);

  if (result != nsIPermissionManager::ALLOW_ACTION) {
    *aRejectedReason = nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER;
    return false;
  }

  return true;
}

bool AntiTrackingCommon::IsFirstPartyStorageAccessGrantedFor(
    nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(aPrincipal);

  nsCookieAccess access = CheckCookiePermissionForPrincipal(aPrincipal);
  if (access != nsICookiePermission::ACCESS_DEFAULT) {
    return access != nsICookiePermission::ACCESS_DENY;
  }

  int32_t behavior = CookiesBehavior(aPrincipal, nullptr);
  return behavior != nsICookieService::BEHAVIOR_REJECT;
}

/* static */ bool AntiTrackingCommon::MaybeIsFirstPartyStorageAccessGrantedFor(
    nsPIDOMWindowInner* aFirstPartyWindow, nsIURI* aURI) {
  MOZ_ASSERT(aFirstPartyWindow);
  MOZ_ASSERT(aURI);

  LOG_SPEC(
      ("Computing a best guess as to whether window %p has access to URI %s",
       aFirstPartyWindow, _spec),
      aURI);

  if (StaticPrefs::network_cookie_cookieBehavior() !=
      nsICookieService::BEHAVIOR_REJECT_TRACKER) {
    LOG(("Disabled by the pref (%d), bail out early",
         StaticPrefs::network_cookie_cookieBehavior()));
    return true;
  }

  if (CheckContentBlockingAllowList(aFirstPartyWindow)) {
    return true;
  }

  if (!nsContentUtils::IsThirdPartyWindowOrChannel(aFirstPartyWindow, nullptr,
                                                   aURI)) {
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
    LOG(
        ("CheckCookiePermissionForPrincipal() returned a non-default access "
         "code (%d), returning %s",
         int(access),
         access != nsICookiePermission::ACCESS_DENY ? "success" : "failure"));
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
  LOG_SPEC(
      ("Testing permission type %s for %s resulted in %d (%s)", type.get(),
       _spec, int(result),
       result == nsIPermissionManager::ALLOW_ACTION ? "success" : "failure"),
      parentPrincipalURI);

  return result == nsIPermissionManager::ALLOW_ACTION;
}

nsresult AntiTrackingCommon::IsOnContentBlockingAllowList(
    nsIURI* aTopWinURI, bool aIsPrivateBrowsing,
    AntiTrackingCommon::ContentBlockingAllowListPurpose aPurpose,
    bool& aIsAllowListed) {
  aIsAllowListed = false;

  // For storage checks, check the storage pref, and for annotations checks,
  // check the corresponding pref as well.  This allows each set of checks to
  // be disabled individually if needed.
  if ((aPurpose == eStorageChecks &&
       !StaticPrefs::browser_contentblocking_allowlist_storage_enabled()) ||
      (aPurpose == eTrackingAnnotations &&
       !StaticPrefs::browser_contentblocking_allowlist_annotations_enabled())) {
    LOG(
        ("Attempting to check the content blocking allow list aborted because "
         "the third-party cookies UI has been disabled."));
    return NS_OK;
  }

  LOG_SPEC(("Deciding whether the user has overridden content blocking for %s",
            _spec),
           aTopWinURI);

  nsCOMPtr<nsIIOService> ios = services::GetIOService();
  NS_ENSURE_TRUE(ios, NS_ERROR_FAILURE);

  // Take the host/port portion so we can allowlist by site. Also ignore the
  // scheme, since users who put sites on the allowlist probably don't expect
  // allowlisting to depend on scheme.
  nsresult rv = NS_ERROR_FAILURE;
  nsCOMPtr<nsIURL> url = do_QueryInterface(aTopWinURI, &rv);
  if (NS_FAILED(rv)) {
    return rv;  // normal for some loads, no need to print a warning
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

  // Check both the normal mode and private browsing mode user override
  // permissions.
  Pair<const char*, bool> types[] = {{"trackingprotection", false},
                                     {"trackingprotection-pb", true}};

  for (size_t i = 0; i < ArrayLength(types); ++i) {
    if (aIsPrivateBrowsing != types[i].second()) {
      continue;
    }

    uint32_t permissions = nsIPermissionManager::UNKNOWN_ACTION;
    rv = permMgr->TestPermission(topWinURI, types[i].first(), &permissions);
    NS_ENSURE_SUCCESS(rv, rv);

    if (permissions == nsIPermissionManager::ALLOW_ACTION) {
      aIsAllowListed = true;
      LOG_SPEC(("Found user override type %s for %s", types[i].first(), _spec),
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

/* static */ void AntiTrackingCommon::NotifyBlockingDecision(
    nsIChannel* aChannel, BlockingDecision aDecision,
    uint32_t aRejectedReason) {
  MOZ_ASSERT(
      aRejectedReason == 0 ||
      aRejectedReason ==
          nsIWebProgressListener::STATE_COOKIES_BLOCKED_BY_PERMISSION ||
      aRejectedReason ==
          nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER ||
      aRejectedReason == nsIWebProgressListener::STATE_COOKIES_BLOCKED_ALL ||
      aRejectedReason == nsIWebProgressListener::STATE_COOKIES_BLOCKED_FOREIGN);
  MOZ_ASSERT(aDecision == BlockingDecision::eBlock ||
             aDecision == BlockingDecision::eAllow);

  if (!aChannel) {
    return;
  }

  // Can be called in EITHER the parent or child process.
  if (XRE_IsParentProcess()) {
    nsCOMPtr<nsIParentChannel> parentChannel;
    NS_QueryNotificationCallbacks(aChannel, parentChannel);
    if (parentChannel) {
      // This channel is a parent-process proxy for a child process request.
      // Tell the child process channel to do this instead.
      if (aDecision == BlockingDecision::eBlock) {
        parentChannel->NotifyTrackingCookieBlocked(aRejectedReason);
      } else {
        parentChannel->NotifyCookieAllowed();
      }
    }
    return;
  }

  MOZ_ASSERT(XRE_IsContentProcess());

  nsCOMPtr<mozIThirdPartyUtil> thirdPartyUtil = services::GetThirdPartyUtil();
  if (!thirdPartyUtil) {
    return;
  }

  nsCOMPtr<mozIDOMWindowProxy> win;
  nsresult rv =
      thirdPartyUtil->GetTopWindowForChannel(aChannel, getter_AddRefs(win));
  NS_ENSURE_SUCCESS_VOID(rv);

  nsCOMPtr<nsPIDOMWindowOuter> pwin = nsPIDOMWindowOuter::From(win);
  if (!pwin) {
    return;
  }

  nsCOMPtr<nsIURI> uri;
  aChannel->GetURI(getter_AddRefs(uri));

  if (aDecision == BlockingDecision::eBlock) {
    pwin->NotifyContentBlockingState(aRejectedReason, aChannel, true, uri);

    ReportBlockingToConsole(pwin, uri, aRejectedReason);
  }

  pwin->NotifyContentBlockingState(nsIWebProgressListener::STATE_COOKIES_LOADED,
                                   aChannel, false, uri);
}

/* static */ void AntiTrackingCommon::NotifyBlockingDecision(
    nsPIDOMWindowInner* aWindow, BlockingDecision aDecision,
    uint32_t aRejectedReason) {
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(
      aRejectedReason == 0 ||
      aRejectedReason ==
          nsIWebProgressListener::STATE_COOKIES_BLOCKED_BY_PERMISSION ||
      aRejectedReason ==
          nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER ||
      aRejectedReason == nsIWebProgressListener::STATE_COOKIES_BLOCKED_ALL ||
      aRejectedReason == nsIWebProgressListener::STATE_COOKIES_BLOCKED_FOREIGN);
  MOZ_ASSERT(aDecision == BlockingDecision::eBlock ||
             aDecision == BlockingDecision::eAllow);

  nsCOMPtr<nsPIDOMWindowOuter> pwin = GetTopWindow(aWindow);
  if (!pwin) {
    return;
  }

  nsPIDOMWindowInner* inner = pwin->GetCurrentInnerWindow();
  if (!inner) {
    return;
  }
  nsIDocument* pwinDoc = inner->GetExtantDoc();
  if (!pwinDoc) {
    return;
  }
  nsIChannel* channel = pwinDoc->GetChannel();
  if (!channel) {
    return;
  }

  nsIDocument* document = aWindow->GetExtantDoc();
  if (!document) {
    return;
  }
  nsIURI* uri = document->GetDocumentURI();

  if (aDecision == BlockingDecision::eBlock) {
    pwin->NotifyContentBlockingState(aRejectedReason, channel, true, uri);

    ReportBlockingToConsole(pwin, uri, aRejectedReason);
  }

  pwin->NotifyContentBlockingState(nsIWebProgressListener::STATE_COOKIES_LOADED,
                                   channel, false, uri);
}

/* static */ void AntiTrackingCommon::StoreUserInteractionFor(
    nsIPrincipal* aPrincipal) {
  if (XRE_IsParentProcess()) {
    nsCOMPtr<nsIURI> uri;
    Unused << aPrincipal->GetURI(getter_AddRefs(uri));
    LOG_SPEC(("Saving the userInteraction for %s", _spec), uri);

    nsCOMPtr<nsIPermissionManager> pm = services::GetPermissionManager();
    if (NS_WARN_IF(!pm)) {
      LOG(("Permission manager is null, bailing out early"));
      return;
    }

    // Remember that this pref is stored in seconds!
    uint32_t expirationType = nsIPermissionManager::EXPIRE_TIME;
    uint32_t expirationTime =
        StaticPrefs::privacy_userInteraction_expiration() * 1000;
    int64_t when = (PR_Now() / PR_USEC_PER_MSEC) + expirationTime;

    uint32_t privateBrowsingId = 0;
    nsresult rv = aPrincipal->GetPrivateBrowsingId(&privateBrowsingId);
    if (!NS_WARN_IF(NS_FAILED(rv)) && privateBrowsingId > 0) {
      // If we are coming from a private window, make sure to store a
      // session-only permission which won't get persisted to disk.
      expirationType = nsIPermissionManager::EXPIRE_SESSION;
      when = 0;
    }

    rv = pm->AddFromPrincipal(aPrincipal, USER_INTERACTION_PERM,
                              nsIPermissionManager::ALLOW_ACTION,
                              expirationType, when);
    Unused << NS_WARN_IF(NS_FAILED(rv));
    return;
  }

  ContentChild* cc = ContentChild::GetSingleton();
  MOZ_ASSERT(cc);

  nsCOMPtr<nsIURI> uri;
  Unused << aPrincipal->GetURI(getter_AddRefs(uri));
  LOG_SPEC(("Asking the parent process to save the user-interaction for us: %s",
            _spec),
           uri);
  cc->SendStoreUserInteractionAsPermission(IPC::Principal(aPrincipal));
}

/* static */ bool AntiTrackingCommon::HasUserInteraction(
    nsIPrincipal* aPrincipal) {
  nsCOMPtr<nsIPermissionManager> pm = services::GetPermissionManager();
  if (NS_WARN_IF(!pm)) {
    return false;
  }

  uint32_t result = 0;
  nsresult rv = pm->TestPermissionFromPrincipal(aPrincipal,
                                                USER_INTERACTION_PERM, &result);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  return result == nsIPermissionManager::ALLOW_ACTION;
}
