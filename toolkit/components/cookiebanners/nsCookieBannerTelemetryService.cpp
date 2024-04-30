/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCookieBannerTelemetryService.h"

#include "cookieBanner.pb.h"

#include "mozilla/Base64.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/glean/GleanMetrics.h"
#include "mozilla/OriginAttributes.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Logging.h"
#include "Cookie.h"
#include "nsCRT.h"
#include "nsError.h"
#include "nsICookie.h"
#include "nsICookieManager.h"
#include "nsICookieNotification.h"
#include "nsTHashSet.h"
#include "nsIObserverService.h"
#include "nsIScriptSecurityManager.h"
#include "nsISearchService.h"
#include "nsServiceManagerUtils.h"
#include "nsStringFwd.h"
#include "nsTArray.h"

namespace mozilla {

NS_IMPL_ISUPPORTS(nsCookieBannerTelemetryService,
                  nsICookieBannerTelemetryService, nsIObserver)

static LazyLogModule gCookieBannerTelemetryLog(
    "nsCookieBannerTelemetryService");

static StaticRefPtr<nsCookieBannerTelemetryService>
    sCookieBannerTelemetryServiceSingleton;

// A hash set used to tell whether a base domain is a Google domain.
static StaticAutoPtr<nsTHashSet<nsCString>> sGoogleDomainsSet;

namespace {

// A helper function that decodes Google's SOCS cookie and returns the GDPR
// choice and the region. The choice and be either "Accept", "Reject", or
// "Custom".
nsresult DecodeSOCSGoogleCookie(const nsACString& aCookie, nsACString& aChoice,
                                nsACString& aRegion) {
  aChoice.Truncate();
  aRegion.Truncate();

  FallibleTArray<uint8_t> decoded;
  nsresult rv =
      Base64URLDecode(aCookie, Base64URLDecodePaddingPolicy::Ignore, decoded);
  NS_ENSURE_SUCCESS(rv, rv);

  std::string buf(reinterpret_cast<const char*>(decoded.Elements()),
                  decoded.Length());
  cookieBanner::GoogleSOCSCookie socs;

  if (!socs.ParseFromString(buf)) {
    return NS_ERROR_CANNOT_CONVERT_DATA;
  }

  aRegion.Assign(socs.data().region().c_str(), socs.data().region().length());

  // The first field represents the gdpr choice, 1 means "Reject" and 2 means
  // either "Accept" or "Custom". We need to check a following field to decide.
  if (socs.gdpr_choice() == 1) {
    aChoice.AssignLiteral("Reject");
    return NS_OK;
  }

  // The platform field represents where does this GDPR consent is selected. The
  // value can be either "gws_*" or "boq_identityfrontenduiserver_*". If the
  // field value starts with "gws_", it means the consent is from the Google
  // search page. Otherwise, it's from the consent.google.com, which is used for
  // the custom setting.
  std::string prefix = "gws_";

  if (socs.data().platform().compare(0, prefix.length(), prefix) == 0) {
    aChoice.AssignLiteral("Accept");
    return NS_OK;
  }

  aChoice.AssignLiteral("Custom");
  return NS_OK;
}

}  // anonymous namespace

// static
already_AddRefed<nsCookieBannerTelemetryService>
nsCookieBannerTelemetryService::GetSingleton() {
  MOZ_LOG(gCookieBannerTelemetryLog, LogLevel::Debug, ("GetSingleton."));

  if (!sCookieBannerTelemetryServiceSingleton) {
    sCookieBannerTelemetryServiceSingleton =
        new nsCookieBannerTelemetryService();

    RunOnShutdown([] {
      DebugOnly<nsresult> rv =
          sCookieBannerTelemetryServiceSingleton->Shutdown();
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "nsCookieBannerTelemetryService::Shutdown failed.");

      sCookieBannerTelemetryServiceSingleton = nullptr;
    });
  }

  return do_AddRef(sCookieBannerTelemetryServiceSingleton);
}

nsresult nsCookieBannerTelemetryService::Init() {
  MOZ_LOG(gCookieBannerTelemetryLog, LogLevel::Debug, ("Init."));
  if (mIsInitialized) {
    return NS_OK;
  }

  mIsInitialized = true;

  nsCOMPtr<nsIObserverService> obsSvc = mozilla::services::GetObserverService();
  NS_ENSURE_TRUE(obsSvc, NS_ERROR_FAILURE);

  nsresult rv = obsSvc->AddObserver(this, "browser-search-service", false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = obsSvc->AddObserver(this, "idle-daily", false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = obsSvc->AddObserver(this, "cookie-changed", false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = obsSvc->AddObserver(this, "private-cookie-changed", false);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult nsCookieBannerTelemetryService::Shutdown() {
  MOZ_LOG(gCookieBannerTelemetryLog, LogLevel::Debug, ("Shutdown."));

  if (!mIsInitialized) {
    return NS_OK;
  }

  mIsInitialized = false;

  sGoogleDomainsSet = nullptr;

  nsCOMPtr<nsIObserverService> obsSvc = mozilla::services::GetObserverService();
  NS_ENSURE_TRUE(obsSvc, NS_ERROR_FAILURE);

  nsresult rv = obsSvc->RemoveObserver(this, "browser-search-service");
  NS_ENSURE_SUCCESS(rv, rv);

  rv = obsSvc->RemoveObserver(this, "idle-daily");
  NS_ENSURE_SUCCESS(rv, rv);

  rv = obsSvc->RemoveObserver(this, "cookie-changed");
  NS_ENSURE_SUCCESS(rv, rv);

  rv = obsSvc->RemoveObserver(this, "private-cookie-changed");
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsCookieBannerTelemetryService::Observe(nsISupports* aSubject,
                                        const char* aTopic,
                                        const char16_t* aData) {
  if (nsCRT::strcmp(aTopic, "profile-after-change") == 0) {
    MOZ_LOG(gCookieBannerTelemetryLog, LogLevel::Debug,
            ("Observe profile-after-change"));

    return Init();
  }

  if (nsCRT::strcmp(aTopic, "idle-daily") == 0) {
    MOZ_LOG(gCookieBannerTelemetryLog, LogLevel::Debug, ("idle-daily"));

    return MaybeReportGoogleGDPRChoiceTelemetry();
  }

  if (nsCRT::strcmp(aTopic, "browser-search-service") == 0 &&
      nsDependentString(aData).EqualsLiteral("init-complete")) {
    MOZ_LOG(gCookieBannerTelemetryLog, LogLevel::Debug,
            ("Observe browser-search-service::init-complete."));
    mIsSearchServiceInitialized = true;

    return MaybeReportGoogleGDPRChoiceTelemetry();
  }

  if (nsCRT::strcmp(aTopic, "cookie-changed") == 0 ||
      nsCRT::strcmp(aTopic, "private-cookie-changed") == 0) {
    MOZ_LOG(gCookieBannerTelemetryLog, LogLevel::Debug, ("Observe %s", aTopic));

    nsCOMPtr<nsICookieNotification> notification = do_QueryInterface(aSubject);
    NS_ENSURE_TRUE(notification, NS_ERROR_FAILURE);

    if (notification->GetAction() != nsICookieNotification::COOKIE_ADDED &&
        notification->GetAction() != nsICookieNotification::COOKIE_CHANGED) {
      return NS_OK;
    }

    nsCOMPtr<nsICookie> cookie;
    nsresult rv = notification->GetCookie(getter_AddRefs(cookie));
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoCString name;
    rv = cookie->GetName(name);
    NS_ENSURE_SUCCESS(rv, rv);

    // Bail out early if this is not a SOCS cookie.
    if (!name.EqualsLiteral("SOCS")) {
      return NS_OK;
    }

    // A "SOCS" cookie is added. We record the GDPR choice as well as an event
    // telemetry for recording a choice has been made at the moment.
    return MaybeReportGoogleGDPRChoiceTelemetry(cookie, true);
  }

  return NS_OK;
}

nsresult nsCookieBannerTelemetryService::MaybeReportGoogleGDPRChoiceTelemetry(
    nsICookie* aCookie, bool aReportEvent) {
  MOZ_ASSERT(mIsInitialized);
  nsresult rv;

  // Don't report the telemetry if the search service is not yet initialized.
  if (!mIsSearchServiceInitialized) {
    MOZ_LOG(gCookieBannerTelemetryLog, LogLevel::Debug,
            ("Search Service is not yet initialized."));
    return NS_OK;
  }

  // We only collect Google GDPR choice if Google is the default search engine.
  nsCOMPtr<nsISearchService> searchService(
      do_GetService("@mozilla.org/browser/search-service;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISearchEngine> engine;
  rv = searchService->GetDefaultEngine(getter_AddRefs(engine));
  NS_ENSURE_SUCCESS(rv, rv);

  // Bail out early if no default search engine is available. This could happen
  // if no search engine is shipped with the Gecko.
  if (!engine) {
    MOZ_LOG(gCookieBannerTelemetryLog, LogLevel::Debug,
            ("No default search engine is available."));
    return NS_OK;
  }

  nsAutoString id;
  rv = engine->GetId(id);
  NS_ENSURE_SUCCESS(rv, rv);

  // Bail out early if the default search engine is not Google.
  if (!id.EqualsLiteral("google@search.mozilla.orgdefault")) {
    return NS_OK;
  }

  // Build up the Google domain set from the alternative Google search domains
  if (!sGoogleDomainsSet) {
    sGoogleDomainsSet = new nsTHashSet<nsCString>();

    nsTArray<nsCString> googleDomains;
    rv = searchService->GetAlternateDomains("www.google.com"_ns, googleDomains);
    NS_ENSURE_SUCCESS(rv, rv);

    for (const auto& domain : googleDomains) {
      // We need to trim down the preceding "www" to get the host because the
      // alternate domains provided by search service always have leading "www".
      // However, the "SOCS" cookie is always set for all subdomains, e.g.
      // ".google.com". Therefore, we need to trim down "www" to match the host
      // of the cookie.
      NS_ENSURE_TRUE(domain.Length() > 3, NS_ERROR_FAILURE);
      sGoogleDomainsSet->Insert(Substring(domain, 3, domain.Length() - 3));
    }
  }

  nsTArray<RefPtr<nsICookie>> cookies;
  if (aCookie) {
    const auto& attrs = aCookie->AsCookie().OriginAttributesRef();

    // We only report cookies for the default originAttributes or private
    // browsing mode.
    if (attrs.mPrivateBrowsingId !=
            nsIScriptSecurityManager::DEFAULT_PRIVATE_BROWSING_ID ||
        attrs == OriginAttributes()) {
      cookies.AppendElement(RefPtr<nsICookie>(aCookie));
    }
  } else {
    // If no cookie is given, we will iterate all cookies under Google Search
    // domains.
    nsCOMPtr<nsICookieManager> cookieManager =
        do_GetService("@mozilla.org/cookiemanager;1");
    if (!cookieManager) {
      return NS_ERROR_FAILURE;
    }

    for (const auto& domain : *sGoogleDomainsSet) {
      // Getting Google Search cookies only for normal windows. We will exclude
      // cookies of non-default originAttributes, like cookies from containers.
      //
      // Note that we need to trim the leading "." from the domain here.
      nsTArray<RefPtr<nsICookie>> googleCookies;
      rv = cookieManager->GetCookiesWithOriginAttributes(
          u"{ \"privateBrowsingId\": 0, \"userContextId\": 0 }"_ns,
          Substring(domain, 1, domain.Length() - 1), googleCookies);
      NS_ENSURE_SUCCESS(rv, rv);

      cookies.AppendElements(googleCookies);
    }
  }

  for (const auto& cookie : cookies) {
    nsAutoCString name;
    rv = cookie->GetName(name);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!name.EqualsLiteral("SOCS")) {
      continue;
    }

    nsAutoCString host;
    rv = cookie->GetHost(host);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!sGoogleDomainsSet->Contains(host)) {
      continue;
    }

    nsAutoCString value;
    rv = cookie->GetValue(value);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoCString choice;
    nsAutoCString region;
    rv = DecodeSOCSGoogleCookie(value, choice, region);
    NS_ENSURE_SUCCESS(rv, rv);

    bool isPrivateBrowsing =
        cookie->AsCookie().OriginAttributesRef().mPrivateBrowsingId !=
        nsIScriptSecurityManager::DEFAULT_PRIVATE_BROWSING_ID;

    MOZ_LOG(gCookieBannerTelemetryLog, LogLevel::Debug,
            ("Record the Google GDPR choice %s on the host %s in region %s for "
             "the %s window",
             choice.get(), host.get(), region.get(),
             isPrivateBrowsing ? "private" : "normal"));

    // We rely on the dynamical labelled string which can send most 16 different
    // labels per report. In average cases, 16 labels is sufficient because we
    // expect a user might have only 1 or 2 "SOCS" cookies on different Google
    // Search domains.
    //
    // Note that we only report event telemetry for private browsing windows
    // because the private session is ephemeral. People can change GDPR
    // choice across different private sessions, so it's hard to collect the
    // state correctly.
    if (!isPrivateBrowsing) {
      glean::cookie_banners::google_gdpr_choice_cookie.Get(host).Set(choice);
    }

    if (aReportEvent) {
      if (isPrivateBrowsing) {
        glean::cookie_banners::GoogleGdprChoiceCookieEventPbmExtra extra = {
            .choice = Some(choice),
        };
        glean::cookie_banners::google_gdpr_choice_cookie_event_pbm.Record(
            Some(extra));
      } else {
        glean::cookie_banners::GoogleGdprChoiceCookieEventExtra extra = {
            .choice = Some(choice),
            .region = Some(region),
            .searchDomain = Some(host),
        };
        glean::cookie_banners::google_gdpr_choice_cookie_event.Record(
            Some(extra));
      }
    }
  }

  return NS_OK;
}

}  // namespace mozilla
