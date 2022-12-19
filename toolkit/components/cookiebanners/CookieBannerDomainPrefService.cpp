/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CookieBannerDomainPrefService.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Logging.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Services.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/StaticPtr.h"

#include "nsIContentPrefService2.h"
#include "nsICookieBannerService.h"
#include "nsIObserverService.h"
#include "nsServiceManagerUtils.h"
#include "nsVariant.h"

#define COOKIE_BANNER_CONTENT_PREF_NAME u"cookiebanner"_ns
#define COOKIE_BANNER_CONTENT_PREF_NAME_PRIVATE u"cookiebannerprivate"_ns

namespace mozilla {

NS_IMPL_ISUPPORTS(CookieBannerDomainPrefService, nsIAsyncShutdownBlocker,
                  nsIObserver)

NS_IMPL_ISUPPORTS(CookieBannerDomainPrefService::DomainPrefData, nsISupports)

LazyLogModule gCookieBannerPerSitePrefLog("CookieBannerDomainPref");

static StaticRefPtr<CookieBannerDomainPrefService>
    sCookieBannerDomainPrefService;

namespace {

// A helper function to get the profile-before-change shutdown barrier.

nsCOMPtr<nsIAsyncShutdownClient> GetShutdownBarrier() {
  nsCOMPtr<nsIAsyncShutdownService> svc = services::GetAsyncShutdownService();
  NS_ENSURE_TRUE(svc, nullptr);

  nsCOMPtr<nsIAsyncShutdownClient> barrier;
  nsresult rv = svc->GetProfileBeforeChange(getter_AddRefs(barrier));
  NS_ENSURE_SUCCESS(rv, nullptr);

  return barrier;
}

}  // anonymous namespace

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
  nsresult rv = obs->AddObserver(this, "last-pb-context-exited", false);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "Fail to add observer for 'last-pb-context-exited'.");

  auto initCallback = MakeRefPtr<InitialLoadContentPrefCallback>(this, false);

  // Populate the content pref for cookie banner domain preferences.
  rv = contentPrefService->GetByName(COOKIE_BANNER_CONTENT_PREF_NAME, nullptr,
                                     initCallback);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "Fail to get all content prefs during init.");

  auto initPrivateCallback =
      MakeRefPtr<InitialLoadContentPrefCallback>(this, true);

  // Populate the content pref for the private browsing.
  rv = contentPrefService->GetByName(COOKIE_BANNER_CONTENT_PREF_NAME_PRIVATE,
                                     nullptr, initPrivateCallback);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "Fail to get all content prefs during init.");

  rv = AddShutdownBlocker();
  NS_ENSURE_SUCCESS_VOID(rv);

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
  bool isContentPrefLoaded =
      aIsPrivate ? mIsPrivateContentPrefLoaded : mIsContentPrefLoaded;

  // We return nothing if the first reading of the content pref is not completed
  // yet. Note that, we won't be able to get the domain pref for early loads.
  // But, we think this is acceptable because the cookie banners on the early
  // load tabs would have interacted before when the user disabled the banner
  // handling. So, there should be consent cookies in place to prevent banner
  // showing. In this case, our cookie injection and banner clicking won't do
  // anything.
  if (!isContentPrefLoaded) {
    return Nothing();
  }

  Maybe<RefPtr<DomainPrefData>> data =
      aIsPrivate ? mPrefsPrivate.MaybeGet(aDomain) : mPrefs.MaybeGet(aDomain);

  if (!data) {
    return Nothing();
  }

  return Some(data.ref()->mMode);
}

nsresult CookieBannerDomainPrefService::SetPref(
    const nsACString& aDomain, nsICookieBannerService::Modes aMode,
    bool aIsPrivate, bool aPersistInPrivateBrowsing) {
  MOZ_ASSERT(NS_IsMainThread());
  // Don't do anything if we are shutting down.
  if (NS_WARN_IF(mIsShuttingDown)) {
    MOZ_LOG(gCookieBannerPerSitePrefLog, LogLevel::Warning,
            ("Attempt to set a domain pref while shutting down."));
    return NS_OK;
  }

  EnsureInitCompleted(aIsPrivate);

  // Create the domain pref data. The data is always persistent for normal
  // windows. For private windows, the data is only persistent if requested.
  auto domainPrefData = MakeRefPtr<DomainPrefData>(
      aMode, aIsPrivate ? aPersistInPrivateBrowsing : true);
  bool wasPersistentInPrivate = false;

  // Update the in-memory domain preference map.
  if (aIsPrivate) {
    Maybe<RefPtr<DomainPrefData>> data = mPrefsPrivate.MaybeGet(aDomain);

    wasPersistentInPrivate = data ? data.ref()->mIsPersistent : false;
    Unused << mPrefsPrivate.InsertOrUpdate(aDomain, domainPrefData);
  } else {
    Unused << mPrefs.InsertOrUpdate(aDomain, domainPrefData);
  }

  // For private windows, the domain prefs will only be stored in memory.
  // Unless, this function is instructed to persist setting for private
  // browsing. To make the disk state consistent with the memory state, we need
  // to clear the domain pref in the disk when we no longer need to persist the
  // domain pref for the domain in PBM.
  if (!aPersistInPrivateBrowsing && aIsPrivate) {
    // Clear the domain pref in disk if it was persistent.
    if (wasPersistentInPrivate) {
      return RemoveContentPrefForDomain(aDomain, true);
    }
    return NS_OK;
  }

  // Set the preference to the content pref service.
  nsCOMPtr<nsIContentPrefService2> contentPrefService =
      do_GetService(NS_CONTENT_PREF_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(contentPrefService, NS_ERROR_FAILURE);

  RefPtr<nsVariant> variant = new nsVariant();
  nsresult rv = variant->SetAsUint8(aMode);
  NS_ENSURE_SUCCESS(rv, rv);

  auto callback = MakeRefPtr<WriteContentPrefCallback>(this);
  mWritingCount++;

  // Store the domain preference to the content pref service.
  rv = contentPrefService->Set(NS_ConvertUTF8toUTF16(aDomain),
                               aIsPrivate
                                   ? COOKIE_BANNER_CONTENT_PREF_NAME_PRIVATE
                                   : COOKIE_BANNER_CONTENT_PREF_NAME,
                               variant, nullptr, callback);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "Fail to set cookie banner domain pref.");

  return rv;
}

nsresult CookieBannerDomainPrefService::RemovePref(const nsACString& aDomain,
                                                   bool aIsPrivate) {
  MOZ_ASSERT(NS_IsMainThread());
  // Don't do anything if we are shutting down.
  if (NS_WARN_IF(mIsShuttingDown)) {
    MOZ_LOG(gCookieBannerPerSitePrefLog, LogLevel::Warning,
            ("Attempt to remove a domain pref while shutting down."));
    return NS_OK;
  }

  EnsureInitCompleted(aIsPrivate);

  // Clear in-memory domain pref.
  if (aIsPrivate) {
    mPrefsPrivate.Remove(aDomain);
  } else {
    mPrefs.Remove(aDomain);
  }

  return RemoveContentPrefForDomain(aDomain, aIsPrivate);
}

nsresult CookieBannerDomainPrefService::RemoveAll(bool aIsPrivate) {
  MOZ_ASSERT(NS_IsMainThread());
  // Don't do anything if we are shutting down.
  if (NS_WARN_IF(mIsShuttingDown)) {
    MOZ_LOG(gCookieBannerPerSitePrefLog, LogLevel::Warning,
            ("Attempt to remove all domain prefs while shutting down."));
    return NS_OK;
  }

  EnsureInitCompleted(aIsPrivate);

  // Clear in-memory domain pref.
  if (aIsPrivate) {
    mPrefsPrivate.Clear();
  } else {
    mPrefs.Clear();
  }

  nsCOMPtr<nsIContentPrefService2> contentPrefService =
      do_GetService(NS_CONTENT_PREF_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(contentPrefService, NS_ERROR_FAILURE);

  auto callback = MakeRefPtr<WriteContentPrefCallback>(this);
  mWritingCount++;

  // Remove all the domain preferences.
  nsresult rv = contentPrefService->RemoveByName(
      aIsPrivate ? COOKIE_BANNER_CONTENT_PREF_NAME_PRIVATE
                 : COOKIE_BANNER_CONTENT_PREF_NAME,
      nullptr, callback);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "Fail to remove all cookie banner domain prefs.");

  return rv;
}

void CookieBannerDomainPrefService::EnsureInitCompleted(bool aIsPrivate) {
  bool& isContentPrefLoaded =
      aIsPrivate ? mIsPrivateContentPrefLoaded : mIsContentPrefLoaded;
  if (isContentPrefLoaded) {
    return;
  }

  // Wait until the service is fully initialized.
  SpinEventLoopUntil("CookieBannerDomainPrefService::EnsureUpdateComplete"_ns,
                     [&] { return isContentPrefLoaded; });
}

nsresult CookieBannerDomainPrefService::AddShutdownBlocker() {
  MOZ_ASSERT(!mIsShuttingDown);

  nsCOMPtr<nsIAsyncShutdownClient> barrier = GetShutdownBarrier();
  NS_ENSURE_TRUE(barrier, NS_ERROR_FAILURE);

  return GetShutdownBarrier()->AddBlocker(
      this, NS_LITERAL_STRING_FROM_CSTRING(__FILE__), __LINE__,
      u"CookieBannerDomainPrefService: shutdown"_ns);
}

nsresult CookieBannerDomainPrefService::RemoveShutdownBlocker() {
  MOZ_ASSERT(mIsShuttingDown);

  nsCOMPtr<nsIAsyncShutdownClient> barrier = GetShutdownBarrier();
  NS_ENSURE_TRUE(barrier, NS_ERROR_FAILURE);

  return GetShutdownBarrier()->RemoveBlocker(this);
}

nsresult CookieBannerDomainPrefService::RemoveContentPrefForDomain(
    const nsACString& aDomain, bool aIsPrivate) {
  nsCOMPtr<nsIContentPrefService2> contentPrefService =
      do_GetService(NS_CONTENT_PREF_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(contentPrefService, NS_ERROR_FAILURE);

  auto callback = MakeRefPtr<WriteContentPrefCallback>(this);
  mWritingCount++;

  // Remove the domain preference from the content pref service.
  nsresult rv = contentPrefService->RemoveByDomainAndName(
      NS_ConvertUTF8toUTF16(aDomain),
      aIsPrivate ? COOKIE_BANNER_CONTENT_PREF_NAME_PRIVATE
                 : COOKIE_BANNER_CONTENT_PREF_NAME,
      nullptr, callback);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "Fail to remove cookie banner domain pref.");
  return rv;
}

NS_IMPL_ISUPPORTS(CookieBannerDomainPrefService::BaseContentPrefCallback,
                  nsIContentPrefCallback2)

NS_IMETHODIMP
CookieBannerDomainPrefService::InitialLoadContentPrefCallback::HandleResult(
    nsIContentPref* aPref) {
  NS_ENSURE_ARG_POINTER(aPref);
  MOZ_ASSERT(mService);

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

  // Create the domain pref data and indicate it's persistent.
  auto domainPrefData =
      MakeRefPtr<DomainPrefData>(nsICookieBannerService::Modes(data), true);

  if (mIsPrivate) {
    Unused << mService->mPrefsPrivate.InsertOrUpdate(
        NS_ConvertUTF16toUTF8(domain), domainPrefData);
  } else {
    Unused << mService->mPrefs.InsertOrUpdate(NS_ConvertUTF16toUTF8(domain),
                                              domainPrefData);
  }

  return NS_OK;
}

NS_IMETHODIMP
CookieBannerDomainPrefService::InitialLoadContentPrefCallback::HandleCompletion(
    uint16_t aReason) {
  MOZ_ASSERT(mService);

  if (mIsPrivate) {
    mService->mIsPrivateContentPrefLoaded = true;
  } else {
    mService->mIsContentPrefLoaded = true;
  }

  return NS_OK;
}

NS_IMETHODIMP
CookieBannerDomainPrefService::InitialLoadContentPrefCallback::HandleError(
    nsresult error) {
  // We don't need to do anything here because HandleCompletion is always
  // called.

  if (NS_WARN_IF(NS_FAILED(error))) {
    MOZ_LOG(gCookieBannerPerSitePrefLog, LogLevel::Warning,
            ("Fail to get content pref during initiation."));
  }

  return NS_OK;
}

NS_IMETHODIMP
CookieBannerDomainPrefService::WriteContentPrefCallback::HandleResult(
    nsIContentPref* aPref) {
  return NS_OK;
}

NS_IMETHODIMP
CookieBannerDomainPrefService::WriteContentPrefCallback::HandleCompletion(
    uint16_t aReason) {
  MOZ_ASSERT(mService);
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mService->mWritingCount > 0);

  mService->mWritingCount--;

  // Remove the shutdown blocker after we complete writing to content pref.
  if (mService->mIsShuttingDown && mService->mWritingCount == 0) {
    mService->RemoveShutdownBlocker();
  }
  return NS_OK;
}

NS_IMETHODIMP
CookieBannerDomainPrefService::WriteContentPrefCallback::HandleError(
    nsresult error) {
  if (NS_WARN_IF(NS_FAILED(error))) {
    MOZ_LOG(gCookieBannerPerSitePrefLog, LogLevel::Warning,
            ("Fail to write content pref."));
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

  // Clear the private browsing domain prefs that are not persistent when we
  // observe the private browsing session has ended.
  mPrefsPrivate.RemoveIf([](const auto& iter) {
    const RefPtr<DomainPrefData>& data = iter.Data();
    return !data->mIsPersistent;
  });

  return NS_OK;
}

NS_IMETHODIMP
CookieBannerDomainPrefService::GetName(nsAString& aName) {
  aName.AssignLiteral(
      "CookieBannerDomainPrefService: write content pref before "
      "profile-before-change.");
  return NS_OK;
}

NS_IMETHODIMP
CookieBannerDomainPrefService::BlockShutdown(nsIAsyncShutdownClient*) {
  MOZ_ASSERT(NS_IsMainThread());

  mIsShuttingDown = true;

  // If we are not writing the content pref, we can remove the shutdown blocker
  // directly.
  if (mWritingCount == 0) {
    RemoveShutdownBlocker();
  }
  return NS_OK;
}

NS_IMETHODIMP
CookieBannerDomainPrefService::GetState(nsIPropertyBag**) { return NS_OK; }

}  // namespace mozilla
