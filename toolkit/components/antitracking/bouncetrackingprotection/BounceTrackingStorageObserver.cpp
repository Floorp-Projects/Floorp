/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BounceTrackingStorageObserver.h"

#include "BounceTrackingState.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "nsCOMPtr.h"
#include "nsICookieNotification.h"
#include "nsIObserverService.h"
#include "mozilla/dom/BrowsingContext.h"
#include "nsICookie.h"

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

  // Check if the cookie is partitioned. Partitioned cookies can not be used for
  // bounce tracking.
  nsCOMPtr<nsICookie> cookie;
  rv = notification->GetCookie(getter_AddRefs(cookie));
  NS_ENSURE_SUCCESS(rv, rv);
  MOZ_ASSERT(cookie);

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

}  // namespace mozilla
