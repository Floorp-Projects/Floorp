/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BounceTrackingStorageObserver.h"

#include "BounceTrackingState.h"
#include "mozilla/Services.h"
#include "mozilla/dom/WindowContext.h"
#include "mozilla/dom/WindowGlobalChild.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "nsCOMPtr.h"
#include "nsICookieNotification.h"
#include "nsIObserverService.h"
#include "mozilla/dom/BrowsingContext.h"
#include "nsICookie.h"
#include "nsIPrincipal.h"

namespace mozilla {

NS_IMPL_ISUPPORTS(BounceTrackingStorageObserver, nsIObserver);

nsresult BounceTrackingStorageObserver::Init() {
  MOZ_ASSERT(XRE_IsParentProcess());

  MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug, ("%s", __FUNCTION__));

  // Add observers to listen for cookie changes.
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  NS_ENSURE_TRUE(observerService, NS_ERROR_FAILURE);

  nsresult rv = observerService->AddObserver(this, "cookie-changed", false);
  NS_ENSURE_SUCCESS(rv, rv);
  return observerService->AddObserver(this, "private-cookie-changed", false);
}

// nsIObserver
NS_IMETHODIMP
BounceTrackingStorageObserver::Observe(nsISupports* aSubject,
                                       const char* aTopic,
                                       const char16_t* aData) {
  MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Verbose,
          ("Observe topic %s", aTopic));

  NS_ENSURE_TRUE(aSubject, NS_ERROR_FAILURE);

  nsresult rv = NS_OK;
  nsCOMPtr<nsICookieNotification> notification =
      do_QueryInterface(aSubject, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsICookieNotification::Action action = notification->GetAction();
  // Filter for cookies added, changed or deleted. We don't care about other
  // actions such as clearing the entire cookie store.
  if (action != nsICookieNotification::COOKIE_ADDED &&
      action != nsICookieNotification::COOKIE_CHANGED &&
      action != nsICookieNotification::COOKIE_DELETED) {
    return NS_OK;
  }

  // Ensure the notification is associated with a BrowsingContext. It's only set
  // for cases where a website updated a cookie.
  RefPtr<dom::BrowsingContext> browsingContext;
  rv = notification->GetBrowsingContext(getter_AddRefs(browsingContext));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!browsingContext) {
    MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Verbose,
            ("Could not get BC for CookieNotification."));
    return NS_OK;
  }

  // Filter http(s) cookies
  nsCOMPtr<nsICookie> cookie;
  rv = notification->GetCookie(getter_AddRefs(cookie));
  NS_ENSURE_SUCCESS(rv, rv);
  MOZ_ASSERT(cookie);

  nsICookie::schemeType schemeMap;
  rv = cookie->GetSchemeMap(&schemeMap);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!(schemeMap & (nsICookie::schemeType::SCHEME_HTTP |
                     nsICookie::schemeType::SCHEME_HTTPS))) {
    MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Verbose,
            ("Skipping non-HTTP(S) cookie."));
    return NS_OK;
  }

  // Check if the cookie is partitioned. Partitioned cookies can not be used for
  // bounce tracking.
  if (!cookie->OriginAttributesNative().mPartitionKey.IsEmpty()) {
    MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Verbose,
            ("Skipping partitioned cookie."));
    return NS_OK;
  }

  dom::BrowsingContext* topBC = browsingContext->Top();
  dom::BrowsingContextWebProgress* webProgress =
      topBC->Canonical()->GetWebProgress();
  if (!webProgress) {
    return NS_OK;
  }

  RefPtr<BounceTrackingState> bounceTrackingState =
      webProgress->GetBounceTrackingState();
  if (!bounceTrackingState) {
    MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Verbose,
            ("BC does not have BounceTrackingState."));
    return NS_OK;
  }

  nsAutoCString baseDomain;
  rv = notification->GetBaseDomain(baseDomain);
  NS_ENSURE_SUCCESS(rv, rv);

  return bounceTrackingState->OnCookieWrite(baseDomain);
}

// static
nsresult BounceTrackingStorageObserver::OnInitialStorageAccess(
    dom::WindowContext* aWindowContext) {
  NS_ENSURE_ARG_POINTER(aWindowContext);

  if (!XRE_IsParentProcess()) {
    // Check if the principal needs to be tracked for bounce tracking. Checking
    // this in the content process may save us IPC to the parent.
    nsIPrincipal* storagePrincipal =
        aWindowContext->GetInnerWindow()->GetEffectiveStoragePrincipal();
    if (!BounceTrackingState::ShouldTrackPrincipal(storagePrincipal)) {
      MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Verbose,
              ("%s: Skipping principal (content process).", __FUNCTION__));
      return NS_OK;
    }

    dom::WindowGlobalChild* windowGlobalChild =
        aWindowContext->GetWindowGlobalChild();
    NS_ENSURE_TRUE(windowGlobalChild, NS_ERROR_FAILURE);
    NS_ENSURE_TRUE(windowGlobalChild->SendOnInitialStorageAccess(),
                   NS_ERROR_FAILURE);

    return NS_OK;
  }

  MOZ_ASSERT(XRE_IsParentProcess());
  nsCOMPtr<nsIPrincipal> storagePrincipal =
      aWindowContext->Canonical()->DocumentStoragePrincipal();
  NS_ENSURE_TRUE(storagePrincipal, NS_ERROR_FAILURE);

  if (!BounceTrackingState::ShouldTrackPrincipal(storagePrincipal)) {
    MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Verbose,
            ("%s: Skipping principal.", __FUNCTION__));
    return NS_OK;
  }

  if (!storagePrincipal->OriginAttributesRef().mPartitionKey.IsEmpty()) {
    MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Verbose,
            ("Skipping partitioned storage access."));
    return NS_OK;
  }

  dom::BrowsingContext* browsingContext = aWindowContext->GetBrowsingContext();
  NS_ENSURE_TRUE(browsingContext, NS_ERROR_FAILURE);

  dom::BrowsingContextWebProgress* webProgress =
      browsingContext->Top()->Canonical()->GetWebProgress();
  NS_ENSURE_TRUE(webProgress, NS_ERROR_FAILURE);

  RefPtr<BounceTrackingState> bounceTrackingState =
      webProgress->GetBounceTrackingState();

  // We may not always get a BounceTrackingState, e.g. if the feature is
  // disabled or we don't keep track of bounce tracking for the given
  // BrowsingContext.
  if (!bounceTrackingState) {
    return NS_OK;
  }

  return bounceTrackingState->OnStorageAccess(storagePrincipal);
}

}  // namespace mozilla
