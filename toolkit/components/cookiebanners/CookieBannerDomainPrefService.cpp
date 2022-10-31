/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CookieBannerDomainPrefService.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Logging.h"
#include "mozilla/Services.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/StaticPtr.h"

#include "nsIContentPrefService2.h"
#include "nsICookieBannerService.h"
#include "nsIObserverService.h"
#include "nsServiceManagerUtils.h"
#include "nsVariant.h"

#define COOKIE_BANNER_CONTENT_PREF_NAME u"cookiebanner"_ns

namespace mozilla {

NS_IMPL_ISUPPORTS(CookieBannerDomainPrefService, nsIContentPrefCallback2,
                  nsIObserver)

LazyLogModule gCookieBannerPerSitePrefLog("CookieBannerDomainPref");

static StaticRefPtr<CookieBannerDomainPrefService>
    sCookieBannerDomainPrefService;

/* static */
already_AddRefed<CookieBannerDomainPrefService>
CookieBannerDomainPrefService::GetOrCreate() {
  if (!sCookieBannerDomainPrefService) {
    sCookieBannerDomainPrefService = new CookieBannerDomainPrefService();

    RunOnShutdown([] {
      MOZ_LOG(gCookieBannerPerSitePrefLog, LogLevel::Debug, ("RunOnShutdown."));

      sCookieBannerDomainPrefService->Shutdown();

      sCookieBannerDomainPrefService = nullptr;
    });
  }

  return do_AddRef(sCookieBannerDomainPrefService);
}

void CookieBannerDomainPrefService::Init() {
  // Make sure we won't init again.
  if (mIsInitialized) {
    return;
  }

  nsCOMPtr<nsIContentPrefService2> contentPrefService =
      do_GetService(NS_CONTENT_PREF_SERVICE_CONTRACTID);

  if (!contentPrefService) {
    return;
  }

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();

  if (!obs) {
    return;
  }

  // Register the observer to watch private browsing session ends. We will clean
  // the private domain prefs when this happens.
  DebugOnly<nsresult> rv =
      obs->AddObserver(this, "last-pb-context-exited", false);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "Fail to add observer for 'last-pb-context-exited'.");

  // Populate the content pref for cookie banner domain preferences.
  rv = contentPrefService->GetByName(COOKIE_BANNER_CONTENT_PREF_NAME, nullptr,
                                     this);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "Fail to get all content prefs during init.");

  mIsInitialized = true;
}

void CookieBannerDomainPrefService::Shutdown() {
  // Bail out early if the service never gets initialized.
  if (!mIsInitialized) {
    return;
  }

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();

  if (!obs) {
    return;
  }

  DebugOnly<nsresult> rv = obs->RemoveObserver(this, "last-pb-context-exited");
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "Fail to remove observer for 'last-pb-context-exited'.");
}

Maybe<nsICookieBannerService::Modes> CookieBannerDomainPrefService::GetPref(
    const nsACString& aDomain, bool aIsPrivate) {
  // For private windows, the domain prefs will only be stored in memory.
  if (aIsPrivate) {
    return mPrefsPrivate.MaybeGet(aDomain);
  }

  // We return nothing if the first reading of the content pref is not completed
  // yet. Note that, we won't be able to get the domain pref for early loads.
  // But, we think this is acceptable because the cookie banners on the early
  // load tabs would have interacted before when the user disabled the banner
  // handling. So, there should be consent cookies in place to prevent banner
  // showing. In this case, our cookie injection and banner clicking won't do
  // anything.
  if (!mIsContentPrefLoaded) {
    return Nothing();
  }

  return mPrefs.MaybeGet(aDomain);
}

nsresult CookieBannerDomainPrefService::SetPref(
    const nsACString& aDomain, nsICookieBannerService::Modes aMode,
    bool aIsPrivate) {
  // For private windows, the domain prefs will only be stored in memory.
  if (aIsPrivate) {
    Unused << mPrefsPrivate.InsertOrUpdate(aDomain, aMode);
    return NS_OK;
  }

  EnsureInitCompleted();

  // Update the in-memory domain preference map.
  Unused << mPrefs.InsertOrUpdate(aDomain, aMode);

  // Set the preference to the content pref service.
  nsCOMPtr<nsIContentPrefService2> contentPrefService =
      do_GetService(NS_CONTENT_PREF_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(contentPrefService, NS_ERROR_FAILURE);

  RefPtr<nsVariant> variant = new nsVariant();
  nsresult rv = variant->SetAsUint8(aMode);
  NS_ENSURE_SUCCESS(rv, rv);

  // Store the domain preference to the content pref service.
  rv = contentPrefService->Set(NS_ConvertUTF8toUTF16(aDomain),
                               COOKIE_BANNER_CONTENT_PREF_NAME, variant,
                               nullptr, nullptr);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "Fail to set cookie banner domain pref.");

  return rv;
}

nsresult CookieBannerDomainPrefService::RemovePref(const nsACString& aDomain,
                                                   bool aIsPrivate) {
  // For private windows, we only need to remove in-memory settings.
  if (aIsPrivate) {
    mPrefsPrivate.Remove(aDomain);
    return NS_OK;
  }

  EnsureInitCompleted();

  mPrefs.Remove(aDomain);

  nsCOMPtr<nsIContentPrefService2> contentPrefService =
      do_GetService(NS_CONTENT_PREF_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(contentPrefService, NS_ERROR_FAILURE);

  // Remove the domain preference from the content pref service.
  nsresult rv = contentPrefService->RemoveByDomainAndName(
      NS_ConvertUTF8toUTF16(aDomain), COOKIE_BANNER_CONTENT_PREF_NAME, nullptr,
      nullptr);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "Fail to remove cookie banner domain pref.");
  return rv;
}

nsresult CookieBannerDomainPrefService::RemoveAll(bool aIsPrivate) {
  // For private windows, we only need to remove in-memory settings.
  if (aIsPrivate) {
    mPrefsPrivate.Clear();
    return NS_OK;
  }

  EnsureInitCompleted();

  mPrefs.Clear();

  nsCOMPtr<nsIContentPrefService2> contentPrefService =
      do_GetService(NS_CONTENT_PREF_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(contentPrefService, NS_ERROR_FAILURE);

  // Remove all the domain preferences.
  nsresult rv = contentPrefService->RemoveByName(
      COOKIE_BANNER_CONTENT_PREF_NAME, nullptr, nullptr);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "Fail to remove all cookie banner domain prefs.");

  return rv;
}

void CookieBannerDomainPrefService::EnsureInitCompleted() {
  if (mIsContentPrefLoaded) {
    return;
  }

  // Wait until the service is fully initialized.
  SpinEventLoopUntil("CookieBannerDomainPrefService::EnsureUpdateComplete"_ns,
                     [&] { return mIsContentPrefLoaded; });
}

NS_IMETHODIMP
CookieBannerDomainPrefService::HandleResult(nsIContentPref* aPref) {
  NS_ENSURE_ARG_POINTER(aPref);

  nsAutoString domain;
  nsresult rv = aPref->GetDomain(domain);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIVariant> value;
  rv = aPref->GetValue(getter_AddRefs(value));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!value) {
    return NS_OK;
  }

  uint8_t data;
  rv = value->GetAsUint8(&data);
  NS_ENSURE_SUCCESS(rv, rv);

  Unused << mPrefs.InsertOrUpdate(NS_ConvertUTF16toUTF8(domain),
                                  nsICookieBannerService::Modes(data));

  return NS_OK;
}

NS_IMETHODIMP
CookieBannerDomainPrefService::HandleCompletion(uint16_t aReason) {
  mIsContentPrefLoaded = true;
  return NS_OK;
}

NS_IMETHODIMP
CookieBannerDomainPrefService::HandleError(nsresult error) {
  // We don't need to do anything here because HandleCompletion is always
  // called.

  if (NS_WARN_IF(NS_FAILED(error))) {
    MOZ_LOG(gCookieBannerPerSitePrefLog, LogLevel::Warning,
            ("Fail to get content pref during initiation."));
    return NS_OK;
  }

  return NS_OK;
}

NS_IMETHODIMP
CookieBannerDomainPrefService::Observe(nsISupports* /*aSubject*/,
                                       const char* aTopic,
                                       const char16_t* /*aData*/) {
  if (strcmp(aTopic, "last-pb-context-exited") != 0) {
    MOZ_ASSERT_UNREACHABLE("unexpected topic");
    return NS_ERROR_UNEXPECTED;
  }

  // Clear the private browsing domain prefs if we observe the private browsing
  // session has ended.
  mPrefsPrivate.Clear();

  return NS_OK;
}

}  // namespace mozilla
