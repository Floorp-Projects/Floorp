/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCookieInjector.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Logging.h"
#include "mozilla/RefPtr.h"
#include "nsDebug.h"
#include "nsICookieBannerService.h"
#include "nsICookieManager.h"
#include "nsIObserverService.h"
#include "nsCRT.h"
#include "nsCOMPtr.h"
#include "mozilla/Components.h"
#include "Cookie.h"
#include "nsIHttpProtocolHandler.h"
#include "mozilla/StaticPrefs_cookiebanners.h"

namespace mozilla {

LazyLogModule gCookieInjectorLog("nsCookieInjector");

StaticRefPtr<nsCookieInjector> sCookieInjectorSingleton;

static constexpr auto kHttpObserverMessage =
    NS_HTTP_ON_MODIFY_REQUEST_BEFORE_COOKIES_TOPIC;
static const char kCookieInjectorEnabledPref[] =
    "cookiebanners.cookieInjector.enabled";

NS_IMPL_ISUPPORTS(nsCookieInjector, nsIObserver);

already_AddRefed<nsCookieInjector> nsCookieInjector::GetSingleton() {
  if (!sCookieInjectorSingleton) {
    sCookieInjectorSingleton = new nsCookieInjector();

    // Register pref listeners.
    DebugOnly<nsresult> rv = Preferences::RegisterCallbackAndCall(
        &nsCookieInjector::OnPrefChange, kCookieInjectorEnabledPref);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "Failed to register pref listener for kCookieInjectorEnabledPref.");
    rv = Preferences::RegisterCallbackAndCall(&nsCookieInjector::OnPrefChange,
                                              kCookieBannerServiceModePref);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "Failed to register pref listener for kCookieBannerServiceModePref.");

    // Clean up on shutdown.
    RunOnShutdown([] {
      MOZ_LOG(gCookieInjectorLog, LogLevel::Debug, ("RunOnShutdown"));

      // Unregister pref listeners.
      DebugOnly<nsresult> rv = Preferences::UnregisterCallback(
          &nsCookieInjector::OnPrefChange, kCookieInjectorEnabledPref);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "Failed to unregister pref listener for kCookieInjectorEnabledPref.");
      rv = Preferences::UnregisterCallback(&nsCookieInjector::OnPrefChange,
                                           kCookieBannerServiceModePref);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "Failed to unregister pref listener for "
                           "kCookieBannerServiceModePref.");

      rv = sCookieInjectorSingleton->Shutdown();
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "nsCookieInjector::Shutdown failed.");
      sCookieInjectorSingleton = nullptr;
    });
  }

  return do_AddRef(sCookieInjectorSingleton);
}

// static
void nsCookieInjector::OnPrefChange(const char* aPref, void* aData) {
  RefPtr<nsCookieInjector> injector = nsCookieInjector::GetSingleton();

  if (StaticPrefs::cookiebanners_service_mode() !=
          nsICookieBannerService::MODE_DISABLED &&
      StaticPrefs::cookiebanners_cookieInjector_enabled()) {
    MOZ_LOG(gCookieInjectorLog, LogLevel::Info,
            ("Initializing cookie injector after pref change. %s", aPref));

    DebugOnly<nsresult> rv = injector->Init();
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "nsCookieInjector::Init failed");
    return;
  }

  MOZ_LOG(gCookieInjectorLog, LogLevel::Info,
          ("Disabling cookie injector after pref change. %s", aPref));
  DebugOnly<nsresult> rv = injector->Shutdown();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "nsCookieInjector::Shutdown failed");
}

nsresult nsCookieInjector::Init() {
  MOZ_LOG(gCookieInjectorLog, LogLevel::Debug, ("%s", __FUNCTION__));

  // Check if already initialized.
  if (mIsInitialized) {
    return NS_OK;
  }
  mIsInitialized = true;

  // Add http observer.
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  NS_ENSURE_TRUE(observerService, NS_ERROR_FAILURE);

  return observerService->AddObserver(this, kHttpObserverMessage, false);
}

nsresult nsCookieInjector::Shutdown() {
  MOZ_LOG(gCookieInjectorLog, LogLevel::Debug, ("%s", __FUNCTION__));

  // Check if already shutdown.
  if (!mIsInitialized) {
    return NS_OK;
  }
  mIsInitialized = false;

  // Remove http observer.
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  NS_ENSURE_TRUE(observerService, NS_ERROR_FAILURE);

  return observerService->RemoveObserver(this, kHttpObserverMessage);
}

// nsIObserver
NS_IMETHODIMP
nsCookieInjector::Observe(nsISupports* aSubject, const char* aTopic,
                          const char16_t* aData) {
  MOZ_LOG(gCookieInjectorLog, LogLevel::Verbose, ("Observe topic %s", aTopic));
  if (nsCRT::strcmp(aTopic, kHttpObserverMessage) == 0) {
    nsCOMPtr<nsIHttpChannel> channel = do_QueryInterface(aSubject);
    NS_ENSURE_TRUE(channel, NS_ERROR_FAILURE);

    return MaybeInjectCookies(channel, aTopic);
  }

  return NS_OK;
}

nsresult nsCookieInjector::MaybeInjectCookies(nsIHttpChannel* aChannel,
                                              const char* aTopic) {
  NS_ENSURE_ARG_POINTER(aChannel);
  NS_ENSURE_ARG_POINTER(aTopic);

  // Skip non-document loads.
  if (!aChannel->IsDocument()) {
    MOZ_LOG(gCookieInjectorLog, LogLevel::Verbose,
            ("%s: Skip non-document load.", aTopic));
    return NS_OK;
  }

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  NS_ENSURE_TRUE(loadInfo, NS_ERROR_FAILURE);

  // Skip non-toplevel loads.
  if (!loadInfo->GetIsTopLevelLoad()) {
    MOZ_LOG(gCookieInjectorLog, LogLevel::Debug,
            ("%s: Skip non-top-level load.", aTopic));
    return NS_OK;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = aChannel->GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  // Get hostPort string, used for logging only.
  nsCString hostPort;
  rv = uri->GetHostPort(hostPort);
  NS_ENSURE_SUCCESS(rv, rv);

  // Cookie banner handling rules are fetched from the cookie banner service.
  nsCOMPtr<nsICookieBannerService> cookieBannerService =
      components::CookieBannerService::Service(&rv);
  NS_ENSURE_SUCCESS(rv, rv);

  MOZ_LOG(gCookieInjectorLog, LogLevel::Debug,
          ("Looking up rules for %s.", hostPort.get()));
  nsTArray<RefPtr<nsICookieRule>> rules;
  rv = cookieBannerService->GetCookiesForURI(uri, rules);
  NS_ENSURE_SUCCESS(rv, rv);

  // No cookie rules found.
  if (rules.IsEmpty()) {
    MOZ_LOG(gCookieInjectorLog, LogLevel::Debug,
            ("Abort: No cookie rules for %s.", hostPort.get()));
    return NS_OK;
  }

  MOZ_LOG(gCookieInjectorLog, LogLevel::Info,
          ("Got rules for %s.", hostPort.get()));

  // Get the OA from the channel. We may need to set the cookie in a specific
  // bucket, for example Private Browsing Mode.
  OriginAttributes attr = loadInfo->GetOriginAttributes();

  return InjectCookiesFromRules(hostPort, rules, attr);
}

nsresult nsCookieInjector::InjectCookiesFromRules(
    const nsCString& aHostPort, const nsTArray<RefPtr<nsICookieRule>>& aRules,
    OriginAttributes& aOriginAttributes) {
  NS_ENSURE_TRUE(aRules.Length(), NS_ERROR_FAILURE);

  MOZ_LOG(gCookieInjectorLog, LogLevel::Info,
          ("Injecting cookies for %s.", aHostPort.get()));

  // Write cookies from aRules to storage via the cookie manager.
  nsCOMPtr<nsICookieManager> cookieManager =
      do_GetService("@mozilla.org/cookiemanager;1");
  NS_ENSURE_TRUE(cookieManager, NS_ERROR_FAILURE);

  for (nsICookieRule* cookieRule : aRules) {
    nsCOMPtr<nsICookie> cookie;
    nsresult rv = cookieRule->GetCookie(getter_AddRefs(cookie));
    NS_ENSURE_SUCCESS(rv, rv);
    if (NS_WARN_IF(!cookie)) {
      continue;
    }

    // Convert to underlying implementer class to get fast non-xpcom property
    // access.
    net::Cookie* c = static_cast<net::Cookie*>(cookie.get());

    // Check if the cookie is already set to avoid overwriting any custom
    // settings.
    nsCOMPtr<nsICookie> existingCookie;
    rv = cookieManager->GetCookieNative(c->Host(), c->Path(), c->Name(),
                                        &aOriginAttributes,
                                        getter_AddRefs(existingCookie));
    NS_ENSURE_SUCCESS(rv, rv);

    // If a cookie with the same name already exists we need to perform further
    // checks. We can only overwrite if the rule defines the cookie's value as
    // the "unset" state.
    if (existingCookie) {
      nsCString unsetValue;
      rv = cookieRule->GetUnsetValue(unsetValue);
      NS_ENSURE_SUCCESS(rv, rv);

      // Cookie exists and the rule doesn't specify an unset value, skip.
      if (unsetValue.IsEmpty()) {
        MOZ_LOG(
            gCookieInjectorLog, LogLevel::Info,
            ("Skip setting already existing cookie. Cookie: %s, %s, %s, %s\n",
             c->Host().get(), c->Name().get(), c->Path().get(),
             c->Value().get()));
        continue;
      }

      nsAutoCString existingCookieValue;
      rv = existingCookie->GetValue(existingCookieValue);
      NS_ENSURE_SUCCESS(rv, rv);

      // If the unset value specified by the rule does not match the cookie
      // value. We must not overwrite, skip.
      if (!unsetValue.Equals(existingCookieValue)) {
        MOZ_LOG(gCookieInjectorLog, LogLevel::Info,
                ("Skip setting already existing cookie. Cookie: %s, %s, %s, "
                 "%s. Rule unset value: %s",
                 c->Host().get(), c->Name().get(), c->Path().get(),
                 c->Value().get(), unsetValue.get()));
        continue;
      }

      MOZ_LOG(gCookieInjectorLog, LogLevel::Info,
              ("Overwriting cookie because of known unset value state %s.",
               unsetValue.get()));
    }

    MOZ_LOG(gCookieInjectorLog, LogLevel::Info,
            ("Setting cookie: %s, %s, %s, %s\n", c->Host().get(),
             c->Name().get(), c->Path().get(), c->Value().get()));
    rv = cookieManager->AddNative(
        c->Host(), c->Path(), c->Name(), c->Value(), c->IsSecure(),
        c->IsHttpOnly(), c->IsSession(), c->Expiry(), &aOriginAttributes,
        c->SameSite(), static_cast<nsICookie::schemeType>(c->SchemeMap()));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

}  // namespace mozilla
