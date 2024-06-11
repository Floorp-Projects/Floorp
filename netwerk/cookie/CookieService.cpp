/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CookieCommons.h"
#include "CookieLogging.h"
#include "mozilla/AppShutdown.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Components.h"
#include "mozilla/ContentBlockingNotifier.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/nsMixedContentBlocker.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/glean/GleanMetrics.h"
#include "mozilla/net/CookieJarSettings.h"
#include "mozilla/net/CookiePersistentStorage.h"
#include "mozilla/net/CookiePrivateStorage.h"
#include "mozilla/net/CookieService.h"
#include "mozilla/net/CookieServiceChild.h"
#include "mozilla/net/HttpBaseChannel.h"
#include "mozilla/net/NeckoCommon.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/StoragePrincipalHelper.h"
#include "mozilla/Telemetry.h"
#include "mozIThirdPartyUtil.h"
#include "nsICookiePermission.h"
#include "nsIConsoleReportCollector.h"
#include "nsIEffectiveTLDService.h"
#include "nsIIDNService.h"
#include "nsIScriptError.h"
#include "nsIScriptSecurityManager.h"
#include "nsIURL.h"
#include "nsIURI.h"
#include "nsIWebProgressListener.h"
#include "nsNetUtil.h"
#include "prprf.h"
#include "ThirdPartyUtil.h"

using namespace mozilla::dom;

namespace {

uint32_t MakeCookieBehavior(uint32_t aCookieBehavior) {
  bool isFirstPartyIsolated = OriginAttributes::IsFirstPartyEnabled();

  if (isFirstPartyIsolated &&
      aCookieBehavior ==
          nsICookieService::BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN) {
    return nsICookieService::BEHAVIOR_REJECT_TRACKER;
  }
  return aCookieBehavior;
}

/*
 Enables sanitizeOnShutdown cleaning prefs and disables the
 network.cookie.lifetimePolicy
*/
void MigrateCookieLifetimePrefs() {
  // Former network.cookie.lifetimePolicy values ACCEPT_SESSION/ACCEPT_NORMALLY
  // are not available anymore 2 = ACCEPT_SESSION
  if (mozilla::Preferences::GetInt("network.cookie.lifetimePolicy") != 2) {
    return;
  }
  if (!mozilla::Preferences::GetBool("privacy.sanitize.sanitizeOnShutdown")) {
    mozilla::Preferences::SetBool("privacy.sanitize.sanitizeOnShutdown", true);
    // To avoid clearing categories that the user did not intend to clear
    mozilla::Preferences::SetBool("privacy.clearOnShutdown.history", false);
    mozilla::Preferences::SetBool("privacy.clearOnShutdown.formdata", false);
    mozilla::Preferences::SetBool("privacy.clearOnShutdown.downloads", false);
    mozilla::Preferences::SetBool("privacy.clearOnShutdown.sessions", false);
    mozilla::Preferences::SetBool("privacy.clearOnShutdown.siteSettings",
                                  false);

    // We will migrate the new clear on shutdown prefs to align both sets of
    // prefs incase the user has not migrated yet. We don't have a new sessions
    // prefs, as it was merged into cookiesAndStorage as part of the effort for
    // the clear data revamp Bug 1853996
    mozilla::Preferences::SetBool(
        "privacy.clearOnShutdown_v2.historyFormDataAndDownloads", false);
    mozilla::Preferences::SetBool("privacy.clearOnShutdown_v2.siteSettings",
                                  false);
  }
  mozilla::Preferences::SetBool("privacy.clearOnShutdown.cookies", true);
  mozilla::Preferences::SetBool("privacy.clearOnShutdown.cache", true);
  mozilla::Preferences::SetBool("privacy.clearOnShutdown.offlineApps", true);

  // Migrate the new clear on shutdown prefs
  mozilla::Preferences::SetBool("privacy.clearOnShutdown_v2.cookiesAndStorage",
                                true);
  mozilla::Preferences::SetBool("privacy.clearOnShutdown_v2.cache", true);
  mozilla::Preferences::ClearUser("network.cookie.lifetimePolicy");
}

}  // anonymous namespace

// static
uint32_t nsICookieManager::GetCookieBehavior(bool aIsPrivate) {
  if (aIsPrivate) {
    // To sync the cookieBehavior pref between regular and private mode in ETP
    // custom mode, we will return the regular cookieBehavior pref for private
    // mode when
    //   1. The regular cookieBehavior pref has a non-default value.
    //   2. And the private cookieBehavior pref has a default value.
    // Also, this can cover the migration case where the user has a non-default
    // cookieBehavior before the private cookieBehavior was introduced. The
    // getter here will directly return the regular cookieBehavior, so that the
    // cookieBehavior for private mode is consistent.
    if (mozilla::Preferences::HasUserValue(
            "network.cookie.cookieBehavior.pbmode")) {
      return MakeCookieBehavior(
          mozilla::StaticPrefs::network_cookie_cookieBehavior_pbmode());
    }

    if (mozilla::Preferences::HasUserValue("network.cookie.cookieBehavior")) {
      return MakeCookieBehavior(
          mozilla::StaticPrefs::network_cookie_cookieBehavior());
    }

    return MakeCookieBehavior(
        mozilla::StaticPrefs::network_cookie_cookieBehavior_pbmode());
  }
  return MakeCookieBehavior(
      mozilla::StaticPrefs::network_cookie_cookieBehavior());
}

namespace mozilla {
namespace net {

/******************************************************************************
 * CookieService impl:
 * useful types & constants
 ******************************************************************************/

static StaticRefPtr<CookieService> gCookieService;

constexpr auto CONSOLE_CHIPS_CATEGORY = "cookiesCHIPS"_ns;
constexpr auto CONSOLE_SAMESITE_CATEGORY = "cookieSameSite"_ns;
constexpr auto CONSOLE_OVERSIZE_CATEGORY = "cookiesOversize"_ns;
constexpr auto CONSOLE_REJECTION_CATEGORY = "cookiesRejection"_ns;
constexpr auto SAMESITE_MDN_URL =
    "https://developer.mozilla.org/docs/Web/HTTP/Headers/Set-Cookie/"
    u"SameSite"_ns;

namespace {

void ComposeCookieString(nsTArray<RefPtr<Cookie>>& aCookieList,
                         nsACString& aCookieString) {
  for (Cookie* cookie : aCookieList) {
    // check if we have anything to write
    if (!cookie->Name().IsEmpty() || !cookie->Value().IsEmpty()) {
      // if we've already added a cookie to the return list, append a "; " so
      // that subsequent cookies are delimited in the final list.
      if (!aCookieString.IsEmpty()) {
        aCookieString.AppendLiteral("; ");
      }

      if (!cookie->Name().IsEmpty()) {
        // we have a name and value - write both
        aCookieString += cookie->Name() + "="_ns + cookie->Value();
      } else {
        // just write value
        aCookieString += cookie->Value();
      }
    }
  }
}

// Return false if the cookie should be ignored for the current channel.
bool ProcessSameSiteCookieForForeignRequest(nsIChannel* aChannel,
                                            Cookie* aCookie,
                                            bool aIsSafeTopLevelNav,
                                            bool aHadCrossSiteRedirects,
                                            bool aLaxByDefault) {
  // If it's a cross-site request and the cookie is same site only (strict)
  // don't send it.
  if (aCookie->SameSite() == nsICookie::SAMESITE_STRICT) {
    return false;
  }

  // Explicit SameSite=None cookies are always processed. When laxByDefault
  // is OFF then so are default cookies.
  if (aCookie->SameSite() == nsICookie::SAMESITE_NONE ||
      (!aLaxByDefault && aCookie->IsDefaultSameSite())) {
    return true;
  }

  // Lax-by-default cookies are processed even with an intermediate
  // cross-site redirect (they are treated like aIsSameSiteForeign = false).
  if (aLaxByDefault && aCookie->IsDefaultSameSite() && aHadCrossSiteRedirects &&
      StaticPrefs::
          network_cookie_sameSite_laxByDefault_allowBoomerangRedirect()) {
    return true;
  }

  int64_t currentTimeInUsec = PR_Now();

  // 2 minutes of tolerance for 'SameSite=Lax by default' for cookies set
  // without a SameSite value when used for unsafe http methods.
  if (aLaxByDefault && aCookie->IsDefaultSameSite() &&
      StaticPrefs::network_cookie_sameSite_laxPlusPOST_timeout() > 0 &&
      currentTimeInUsec - aCookie->CreationTime() <=
          (StaticPrefs::network_cookie_sameSite_laxPlusPOST_timeout() *
           PR_USEC_PER_SEC) &&
      !NS_IsSafeMethodNav(aChannel)) {
    return true;
  }

  MOZ_ASSERT((aLaxByDefault && aCookie->IsDefaultSameSite()) ||
             aCookie->SameSite() == nsICookie::SAMESITE_LAX);
  // We only have SameSite=Lax or lax-by-default cookies at this point.  These
  // are processed only if it's a top-level navigation
  return aIsSafeTopLevelNav;
}

}  // namespace

/******************************************************************************
 * CookieService impl:
 * singleton instance ctor/dtor methods
 ******************************************************************************/

already_AddRefed<nsICookieService> CookieService::GetXPCOMSingleton() {
  if (IsNeckoChild()) {
    return CookieServiceChild::GetSingleton();
  }

  return GetSingleton();
}

already_AddRefed<CookieService> CookieService::GetSingleton() {
  NS_ASSERTION(!IsNeckoChild(), "not a parent process");

  if (gCookieService) {
    return do_AddRef(gCookieService);
  }

  // Create a new singleton CookieService.
  // We AddRef only once since XPCOM has rules about the ordering of module
  // teardowns - by the time our module destructor is called, it's too late to
  // Release our members (e.g. nsIObserverService and nsIPrefBranch), since GC
  // cycles have already been completed and would result in serious leaks.
  // See bug 209571.
  // TODO: Verify what is the earliest point in time during shutdown where
  // we can deny the creation of the CookieService as a whole.
  gCookieService = new CookieService();
  if (gCookieService) {
    if (NS_SUCCEEDED(gCookieService->Init())) {
      ClearOnShutdown(&gCookieService);
    } else {
      gCookieService = nullptr;
    }
  }

  return do_AddRef(gCookieService);
}

/******************************************************************************
 * CookieService impl:
 * public methods
 ******************************************************************************/

NS_IMPL_ISUPPORTS(CookieService, nsICookieService, nsICookieManager,
                  nsIObserver, nsISupportsWeakReference, nsIMemoryReporter)

CookieService::CookieService() = default;

nsresult CookieService::Init() {
  nsresult rv;
  mTLDService = mozilla::components::EffectiveTLD::Service(&rv);
  NS_ENSURE_SUCCESS(rv, rv);

  mIDNService = mozilla::components::IDN::Service(&rv);
  NS_ENSURE_SUCCESS(rv, rv);

  mThirdPartyUtil = mozilla::components::ThirdPartyUtil::Service();
  NS_ENSURE_SUCCESS(rv, rv);

  // Init our default, and possibly private CookieStorages.
  InitCookieStorages();

  // Migrate network.cookie.lifetimePolicy pref to sanitizeOnShutdown prefs
  MigrateCookieLifetimePrefs();

  RegisterWeakMemoryReporter(this);

  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  NS_ENSURE_STATE(os);
  os->AddObserver(this, "profile-before-change", true);
  os->AddObserver(this, "profile-do-change", true);
  os->AddObserver(this, "last-pb-context-exited", true);

  return NS_OK;
}

void CookieService::InitCookieStorages() {
  NS_ASSERTION(!mPersistentStorage, "already have a default CookieStorage");
  NS_ASSERTION(!mPrivateStorage, "already have a private CookieStorage");

  // Create two new CookieStorages. If we are in or beyond our observed
  // shutdown phase, just be non-persistent.
  if (MOZ_UNLIKELY(StaticPrefs::network_cookie_noPersistentStorage() ||
                   AppShutdown::IsInOrBeyond(ShutdownPhase::AppShutdown))) {
    mPersistentStorage = CookiePrivateStorage::Create();
  } else {
    mPersistentStorage = CookiePersistentStorage::Create();
  }

  mPrivateStorage = CookiePrivateStorage::Create();
}

void CookieService::CloseCookieStorages() {
  // return if we already closed
  if (!mPersistentStorage) {
    return;
  }

  // Let's nullify both storages before calling Close().
  RefPtr<CookieStorage> privateStorage;
  privateStorage.swap(mPrivateStorage);

  RefPtr<CookieStorage> persistentStorage;
  persistentStorage.swap(mPersistentStorage);

  privateStorage->Close();
  persistentStorage->Close();
}

CookieService::~CookieService() {
  CloseCookieStorages();

  UnregisterWeakMemoryReporter(this);

  gCookieService = nullptr;
}

NS_IMETHODIMP
CookieService::Observe(nsISupports* /*aSubject*/, const char* aTopic,
                       const char16_t* /*aData*/) {
  // check the topic
  if (!strcmp(aTopic, "profile-before-change")) {
    // The profile is about to change,
    // or is going away because the application is shutting down.

    // Close the default DB connection and null out our CookieStorages before
    // changing.
    CloseCookieStorages();

  } else if (!strcmp(aTopic, "profile-do-change")) {
    NS_ASSERTION(!mPersistentStorage, "shouldn't have a default CookieStorage");
    NS_ASSERTION(!mPrivateStorage, "shouldn't have a private CookieStorage");

    // the profile has already changed; init the db from the new location.
    // if we are in the private browsing state, however, we do not want to read
    // data into it - we should instead put it into the default state, so it's
    // ready for us if and when we switch back to it.
    InitCookieStorages();

  } else if (!strcmp(aTopic, "last-pb-context-exited")) {
    // Flush all the cookies stored by private browsing contexts
    OriginAttributesPattern pattern;
    pattern.mPrivateBrowsingId.Construct(1);
    RemoveCookiesWithOriginAttributes(pattern, ""_ns);
    mPrivateStorage = CookiePrivateStorage::Create();
  }

  return NS_OK;
}

NS_IMETHODIMP
CookieService::GetCookieBehavior(bool aIsPrivate, uint32_t* aCookieBehavior) {
  NS_ENSURE_ARG_POINTER(aCookieBehavior);
  *aCookieBehavior = nsICookieManager::GetCookieBehavior(aIsPrivate);
  return NS_OK;
}

NS_IMETHODIMP
CookieService::GetCookieStringFromDocument(Document* aDocument,
                                           nsACString& aCookie) {
  NS_ENSURE_ARG(aDocument);

  nsresult rv;

  aCookie.Truncate();

  if (!IsInitialized()) {
    return NS_OK;
  }

  bool thirdParty = true;
  nsPIDOMWindowInner* innerWindow = aDocument->GetInnerWindow();
  // in gtests we don't have a window, let's consider those requests as 3rd
  // party.
  if (innerWindow) {
    ThirdPartyUtil* thirdPartyUtil = ThirdPartyUtil::GetInstance();

    if (thirdPartyUtil) {
      Unused << thirdPartyUtil->IsThirdPartyWindow(
          innerWindow->GetOuterWindow(), nullptr, &thirdParty);
    }
  }

  nsCOMPtr<nsIPrincipal> cookiePrincipal =
      aDocument->EffectiveCookiePrincipal();

  nsTArray<nsCOMPtr<nsIPrincipal>> principals;
  principals.AppendElement(cookiePrincipal);

  // CHIPS - If CHIPS is enabled the partitioned cookie jar is always available
  // (and therefore the partitioned principal), the unpartitioned cookie jar is
  // only available in first-party or third-party with storageAccess contexts.
  // In both cases, the document will have storage access.
  bool isCHIPS = StaticPrefs::network_cookie_CHIPS_enabled() &&
                 aDocument->CookieJarSettings()->GetPartitionForeign();
  bool documentHasStorageAccess = false;
  rv = aDocument->HasStorageAccessSync(documentHasStorageAccess);
  NS_ENSURE_SUCCESS(rv, rv);
  if (isCHIPS && documentHasStorageAccess) {
    // Assert that the cookie principal is unpartitioned.
    MOZ_ASSERT(cookiePrincipal->OriginAttributesRef().mPartitionKey.IsEmpty());
    // Only append the partitioned originAttributes if the partitionKey is set.
    // The partitionKey could be empty for partitionKey in partitioned
    // originAttributes if the document is for privilege context, such as the
    // extension's background page.
    if (!aDocument->PartitionedPrincipal()
             ->OriginAttributesRef()
             .mPartitionKey.IsEmpty()) {
      principals.AppendElement(aDocument->PartitionedPrincipal());
    }
  }

  nsTArray<RefPtr<Cookie>> cookieList;

  for (auto& principal : principals) {
    if (!CookieCommons::IsSchemeSupported(principal)) {
      return NS_OK;
    }

    CookieStorage* storage = PickStorage(principal->OriginAttributesRef());

    nsAutoCString baseDomain;
    rv = CookieCommons::GetBaseDomain(principal, baseDomain);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return NS_OK;
    }

    nsAutoCString hostFromURI;
    rv = nsContentUtils::GetHostOrIPv6WithBrackets(principal, hostFromURI);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return NS_OK;
    }

    nsAutoCString pathFromURI;
    rv = principal->GetFilePath(pathFromURI);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return NS_OK;
    }

    int64_t currentTimeInUsec = PR_Now();
    int64_t currentTime = currentTimeInUsec / PR_USEC_PER_SEC;

    nsTArray<RefPtr<Cookie>> cookies;
    storage->GetCookiesFromHost(baseDomain, principal->OriginAttributesRef(),
                                cookies);
    if (cookies.IsEmpty()) {
      continue;
    }

    // check if the nsIPrincipal is using an https secure protocol.
    // if it isn't, then we can't send a secure cookie over the connection.
    bool potentiallyTrustworthy =
        principal->GetIsOriginPotentiallyTrustworthy();

    bool stale = false;

    // iterate the cookies!
    for (Cookie* cookie : cookies) {
      // check the host, since the base domain lookup is conservative.
      if (!CookieCommons::DomainMatches(cookie, hostFromURI)) {
        continue;
      }

      // if the cookie is httpOnly and it's not going directly to the HTTP
      // connection, don't send it
      if (cookie->IsHttpOnly()) {
        continue;
      }

      if (thirdParty && !CookieCommons::ShouldIncludeCrossSiteCookieForDocument(
                            cookie, aDocument)) {
        continue;
      }

      // if the cookie is secure and the host scheme isn't, we can't send it
      if (cookie->IsSecure() && !potentiallyTrustworthy) {
        continue;
      }

      // if the nsIURI path doesn't match the cookie path, don't send it back
      if (!CookieCommons::PathMatches(cookie, pathFromURI)) {
        continue;
      }

      // check if the cookie has expired
      if (cookie->Expiry() <= currentTime) {
        continue;
      }

      // all checks passed - add to list and check if lastAccessed stamp needs
      // updating
      cookieList.AppendElement(cookie);
      if (cookie->IsStale()) {
        stale = true;
      }
    }

    if (cookieList.IsEmpty()) {
      continue;
    }

    // update lastAccessed timestamps. we only do this if the timestamp is stale
    // by a certain amount, to avoid thrashing the db during pageload.
    if (stale) {
      storage->StaleCookies(cookieList, currentTimeInUsec);
    }
  }

  if (cookieList.IsEmpty()) {
    return NS_OK;
  }

  // return cookies in order of path length; longest to shortest.
  // this is required per RFC2109.  if cookies match in length,
  // then sort by creation time (see bug 236772).
  cookieList.Sort(CompareCookiesForSending());
  ComposeCookieString(cookieList, aCookie);

  return NS_OK;
}

NS_IMETHODIMP
CookieService::GetCookieStringFromHttp(nsIURI* aHostURI, nsIChannel* aChannel,
                                       nsACString& aCookieString) {
  NS_ENSURE_ARG(aHostURI);
  NS_ENSURE_ARG(aChannel);

  aCookieString.Truncate();

  if (!CookieCommons::IsSchemeSupported(aHostURI)) {
    return NS_OK;
  }

  uint32_t rejectedReason = 0;
  ThirdPartyAnalysisResult result = mThirdPartyUtil->AnalyzeChannel(
      aChannel, false, aHostURI, nullptr, &rejectedReason);

  bool isSafeTopLevelNav = CookieCommons::IsSafeTopLevelNav(aChannel);
  bool hadCrossSiteRedirects = false;
  bool isSameSiteForeign = CookieCommons::IsSameSiteForeign(
      aChannel, aHostURI, &hadCrossSiteRedirects);

  OriginAttributes storageOriginAttributes;
  StoragePrincipalHelper::GetOriginAttributes(
      aChannel, storageOriginAttributes,
      StoragePrincipalHelper::eStorageAccessPrincipal);

  nsTArray<OriginAttributes> originAttributesList;
  originAttributesList.AppendElement(storageOriginAttributes);

  // CHIPS - If CHIPS is enabled the partitioned cookie jar is always available
  // (and therefore the partitioned OriginAttributes), the unpartitioned cookie
  // jar is only available in first-party or third-party with storageAccess
  // contexts.
  nsCOMPtr<nsICookieJarSettings> cookieJarSettings =
      CookieCommons::GetCookieJarSettings(aChannel);
  bool isCHIPS = StaticPrefs::network_cookie_CHIPS_enabled() &&
                 cookieJarSettings->GetPartitionForeign();
  bool isUnpartitioned =
      !result.contains(ThirdPartyAnalysis::IsForeign) ||
      result.contains(ThirdPartyAnalysis::IsStorageAccessPermissionGranted);
  if (isCHIPS && isUnpartitioned) {
    // Assert that the storage originAttributes is empty. In other words,
    // it's unpartitioned.
    MOZ_ASSERT(storageOriginAttributes.mPartitionKey.IsEmpty());
    // Add the partitioned principal to principals
    OriginAttributes partitionedOriginAttributes;
    StoragePrincipalHelper::GetOriginAttributes(
        aChannel, partitionedOriginAttributes,
        StoragePrincipalHelper::ePartitionedPrincipal);
    // Only append the partitioned originAttributes if the partitionKey is set.
    // The partitionKey could be empty for partitionKey in partitioned
    // originAttributes if the channel is for privilege request, such as
    // extension's requests.
    if (!partitionedOriginAttributes.mPartitionKey.IsEmpty()) {
      originAttributesList.AppendElement(partitionedOriginAttributes);
    }
  }

  AutoTArray<RefPtr<Cookie>, 8> foundCookieList;
  GetCookiesForURI(
      aHostURI, aChannel, result.contains(ThirdPartyAnalysis::IsForeign),
      result.contains(ThirdPartyAnalysis::IsThirdPartyTrackingResource),
      result.contains(ThirdPartyAnalysis::IsThirdPartySocialTrackingResource),
      result.contains(ThirdPartyAnalysis::IsStorageAccessPermissionGranted),
      rejectedReason, isSafeTopLevelNav, isSameSiteForeign,
      hadCrossSiteRedirects, true, false, originAttributesList,
      foundCookieList);

  ComposeCookieString(foundCookieList, aCookieString);

  if (!aCookieString.IsEmpty()) {
    COOKIE_LOGSUCCESS(GET_COOKIE, aHostURI, aCookieString, nullptr, false);
  }
  return NS_OK;
}

NS_IMETHODIMP
CookieService::SetCookieStringFromDocument(Document* aDocument,
                                           const nsACString& aCookieString) {
  NS_ENSURE_ARG(aDocument);

  if (!IsInitialized()) {
    return NS_OK;
  }

  nsCOMPtr<nsIURI> documentURI;
  nsAutoCString baseDomain;
  OriginAttributes attrs;

  int64_t currentTimeInUsec = PR_Now();

  // This function is executed in this context, I don't need to keep objects
  // alive.
  auto hasExistingCookiesLambda = [&](const nsACString& aBaseDomain,
                                      const OriginAttributes& aAttrs) {
    CookieStorage* storage = PickStorage(aAttrs);
    return !!storage->CountCookiesFromHost(aBaseDomain,
                                           aAttrs.mPrivateBrowsingId);
  };

  RefPtr<Cookie> cookie = CookieCommons::CreateCookieFromDocument(
      aDocument, aCookieString, currentTimeInUsec, mTLDService, mThirdPartyUtil,
      hasExistingCookiesLambda, getter_AddRefs(documentURI), baseDomain, attrs);
  if (!cookie) {
    return NS_OK;
  }

  bool thirdParty = true;
  nsPIDOMWindowInner* innerWindow = aDocument->GetInnerWindow();
  // in gtests we don't have a window, let's consider those requests as 3rd
  // party.
  if (innerWindow) {
    ThirdPartyUtil* thirdPartyUtil = ThirdPartyUtil::GetInstance();

    if (thirdPartyUtil) {
      Unused << thirdPartyUtil->IsThirdPartyWindow(
          innerWindow->GetOuterWindow(), nullptr, &thirdParty);
    }
  }

  if (thirdParty && !CookieCommons::ShouldIncludeCrossSiteCookieForDocument(
                        cookie, aDocument)) {
    return NS_OK;
  }

  nsCOMPtr<nsIConsoleReportCollector> crc =
      do_QueryInterface(aDocument->GetChannel());

  // add the cookie to the list. AddCookie() takes care of logging.
  PickStorage(attrs)->AddCookie(crc, baseDomain, attrs, cookie,
                                currentTimeInUsec, documentURI, aCookieString,
                                false, aDocument->GetBrowsingContext());
  return NS_OK;
}

NS_IMETHODIMP
CookieService::SetCookieStringFromHttp(nsIURI* aHostURI,
                                       const nsACString& aCookieHeader,
                                       nsIChannel* aChannel) {
  NS_ENSURE_ARG(aHostURI);
  NS_ENSURE_ARG(aChannel);

  if (!IsInitialized()) {
    return NS_OK;
  }

  if (!CookieCommons::IsSchemeSupported(aHostURI)) {
    return NS_OK;
  }

  uint32_t rejectedReason = 0;
  ThirdPartyAnalysisResult result = mThirdPartyUtil->AnalyzeChannel(
      aChannel, false, aHostURI, nullptr, &rejectedReason);

  OriginAttributes storagePrincipalOriginAttributes;
  StoragePrincipalHelper::GetOriginAttributes(
      aChannel, storagePrincipalOriginAttributes,
      StoragePrincipalHelper::eStorageAccessPrincipal);

  // get the base domain for the host URI.
  // e.g. for "www.bbc.co.uk", this would be "bbc.co.uk".
  // file:// URI's (i.e. with an empty host) are allowed, but any other
  // scheme must have a non-empty host. A trailing dot in the host
  // is acceptable.
  bool requireHostMatch;
  nsAutoCString baseDomain;
  nsresult rv = CookieCommons::GetBaseDomain(mTLDService, aHostURI, baseDomain,
                                             requireHostMatch);
  if (NS_FAILED(rv)) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, aCookieHeader,
                      "couldn't get base domain from URI");
    return NS_OK;
  }

  nsCOMPtr<nsICookieJarSettings> cookieJarSettings =
      CookieCommons::GetCookieJarSettings(aChannel);

  nsAutoCString hostFromURI;
  nsContentUtils::GetHostOrIPv6WithBrackets(aHostURI, hostFromURI);

  nsAutoCString baseDomainFromURI;
  rv = CookieCommons::GetBaseDomainFromHost(mTLDService, hostFromURI,
                                            baseDomainFromURI);
  NS_ENSURE_SUCCESS(rv, NS_OK);

  CookieStorage* storage = PickStorage(storagePrincipalOriginAttributes);

  // check default prefs
  uint32_t priorCookieCount = storage->CountCookiesFromHost(
      baseDomainFromURI, storagePrincipalOriginAttributes.mPrivateBrowsingId);

  nsCOMPtr<nsIConsoleReportCollector> crc = do_QueryInterface(aChannel);

  CookieStatus cookieStatus = CheckPrefs(
      crc, cookieJarSettings, aHostURI,
      result.contains(ThirdPartyAnalysis::IsForeign),
      result.contains(ThirdPartyAnalysis::IsThirdPartyTrackingResource),
      result.contains(ThirdPartyAnalysis::IsThirdPartySocialTrackingResource),
      result.contains(ThirdPartyAnalysis::IsStorageAccessPermissionGranted),
      aCookieHeader, priorCookieCount, storagePrincipalOriginAttributes,
      &rejectedReason);

  MOZ_ASSERT_IF(rejectedReason, cookieStatus == STATUS_REJECTED);

  // fire a notification if third party or if cookie was rejected
  // (but not if there was an error)
  switch (cookieStatus) {
    case STATUS_REJECTED:
      CookieCommons::NotifyRejected(aHostURI, aChannel, rejectedReason,
                                    OPERATION_WRITE);
      return NS_OK;  // Stop here
    case STATUS_REJECTED_WITH_ERROR:
      CookieCommons::NotifyRejected(aHostURI, aChannel, rejectedReason,
                                    OPERATION_WRITE);
      return NS_OK;
    case STATUS_ACCEPTED:  // Fallthrough
    case STATUS_ACCEPT_SESSION:
      NotifyAccepted(aChannel);
      break;
    default:
      break;
  }

  bool addonAllowsLoad = false;
  nsCOMPtr<nsIURI> channelURI;
  NS_GetFinalChannelURI(aChannel, getter_AddRefs(channelURI));
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  addonAllowsLoad = BasePrincipal::Cast(loadInfo->TriggeringPrincipal())
                        ->AddonAllowsLoad(channelURI);

  bool isForeignAndNotAddon = false;
  if (!addonAllowsLoad) {
    mThirdPartyUtil->IsThirdPartyChannel(aChannel, aHostURI,
                                         &isForeignAndNotAddon);

    // include sub-document navigations from cross-site to same-site
    // wrt top-level in our check for thirdparty-ness
    if (StaticPrefs::network_cookie_sameSite_crossSiteIframeSetCheck() &&
        !isForeignAndNotAddon &&
        loadInfo->GetExternalContentPolicyType() ==
            ExtContentPolicy::TYPE_SUBDOCUMENT) {
      bool triggeringPrincipalIsThirdParty = false;
      BasePrincipal::Cast(loadInfo->TriggeringPrincipal())
          ->IsThirdPartyURI(channelURI, &triggeringPrincipalIsThirdParty);
      isForeignAndNotAddon |= triggeringPrincipalIsThirdParty;
    }
  }

  bool mustBePartitioned =
      isForeignAndNotAddon &&
      cookieJarSettings->GetCookieBehavior() ==
          nsICookieService::BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN &&
      !result.contains(ThirdPartyAnalysis::IsStorageAccessPermissionGranted);

  nsCString cookieHeader(aCookieHeader);

  // CHIPS - The partitioned cookie jar is always available and it is always
  // possible to store cookies in it using the "Partitioned" attribute.
  // Prepare the partitioned principals OAs to enable possible partitioned
  // cookie storing from first-party or with StorageAccess.
  // Similar behavior to CookieServiceChild::SetCookieStringFromHttp().
  OriginAttributes partitionedPrincipalOriginAttributes;
  bool isPartitionedPrincipal =
      !storagePrincipalOriginAttributes.mPartitionKey.IsEmpty();
  bool isCHIPS = StaticPrefs::network_cookie_CHIPS_enabled() &&
                 cookieJarSettings->GetPartitionForeign();
  // Only need to get OAs if we don't already use the partitioned principal.
  if (isCHIPS && !isPartitionedPrincipal) {
    StoragePrincipalHelper::GetOriginAttributes(
        aChannel, partitionedPrincipalOriginAttributes,
        StoragePrincipalHelper::ePartitionedPrincipal);
  }

  // process each cookie in the header
  bool moreCookieToRead = true;
  while (moreCookieToRead) {
    CookieStruct cookieData;
    bool canSetCookie = false;

    moreCookieToRead = CanSetCookie(
        aHostURI, baseDomain, cookieData, requireHostMatch, cookieStatus,
        cookieHeader, true, isForeignAndNotAddon, mustBePartitioned,
        storagePrincipalOriginAttributes.mPrivateBrowsingId !=
            nsIScriptSecurityManager::DEFAULT_PRIVATE_BROWSING_ID,
        crc, canSetCookie);

    if (!canSetCookie) {
      continue;
    }

    // check permissions from site permission list.
    if (!CookieCommons::CheckCookiePermission(aChannel, cookieData)) {
      COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, aCookieHeader,
                        "cookie rejected by permission manager");
      CookieCommons::NotifyRejected(
          aHostURI, aChannel,
          nsIWebProgressListener::STATE_COOKIES_BLOCKED_BY_PERMISSION,
          OPERATION_WRITE);
      CookieLogging::LogMessageToConsole(
          crc, aHostURI, nsIScriptError::warningFlag,
          CONSOLE_REJECTION_CATEGORY, "CookieRejectedByPermissionManager"_ns,
          AutoTArray<nsString, 1>{
              NS_ConvertUTF8toUTF16(cookieData.name()),
          });
      continue;
    }

    // CHIPS - If the partitioned attribute is set, store cookie in partitioned
    // cookie jar independent of context. If the cookies are stored in the
    // partitioned cookie jar anyway no special treatment of CHIPS cookies
    // necessary.
    bool needPartitioned =
        isCHIPS && cookieData.isPartitioned() && !isPartitionedPrincipal;
    OriginAttributes& cookieOriginAttributes =
        needPartitioned ? partitionedPrincipalOriginAttributes
                        : storagePrincipalOriginAttributes;
    // Assert that partitionedPrincipalOriginAttributes are initialized if used.
    MOZ_ASSERT_IF(
        needPartitioned,
        !partitionedPrincipalOriginAttributes.mPartitionKey.IsEmpty());

    // create a new Cookie
    RefPtr<Cookie> cookie = Cookie::Create(cookieData, cookieOriginAttributes);
    MOZ_ASSERT(cookie);

    int64_t currentTimeInUsec = PR_Now();
    cookie->SetLastAccessed(currentTimeInUsec);
    cookie->SetCreationTime(
        Cookie::GenerateUniqueCreationTime(currentTimeInUsec));

    RefPtr<BrowsingContext> bc = loadInfo->GetBrowsingContext();

    // add the cookie to the list. AddCookie() takes care of logging.
    storage->AddCookie(crc, baseDomain, cookieOriginAttributes, cookie,
                       currentTimeInUsec, aHostURI, aCookieHeader, true, bc);
  }

  return NS_OK;
}

void CookieService::NotifyAccepted(nsIChannel* aChannel) {
  ContentBlockingNotifier::OnDecision(
      aChannel, ContentBlockingNotifier::BlockingDecision::eAllow, 0);
}

/******************************************************************************
 * CookieService:
 * public transaction helper impl
 ******************************************************************************/

NS_IMETHODIMP
CookieService::RunInTransaction(nsICookieTransactionCallback* aCallback) {
  NS_ENSURE_ARG(aCallback);

  if (!IsInitialized()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mPersistentStorage->EnsureInitialized();
  return mPersistentStorage->RunInTransaction(aCallback);
}

/******************************************************************************
 * nsICookieManager impl:
 * nsICookieManager
 ******************************************************************************/

NS_IMETHODIMP
CookieService::RemoveAll() {
  if (!IsInitialized()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mPersistentStorage->EnsureInitialized();
  mPersistentStorage->RemoveAll();
  return NS_OK;
}

NS_IMETHODIMP
CookieService::GetCookies(nsTArray<RefPtr<nsICookie>>& aCookies) {
  if (!IsInitialized()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mPersistentStorage->EnsureInitialized();

  // We expose only non-private cookies.
  mPersistentStorage->GetCookies(aCookies);

  return NS_OK;
}

NS_IMETHODIMP
CookieService::GetSessionCookies(nsTArray<RefPtr<nsICookie>>& aCookies) {
  if (!IsInitialized()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mPersistentStorage->EnsureInitialized();

  // We expose only non-private cookies.
  mPersistentStorage->GetSessionCookies(aCookies);

  return NS_OK;
}

NS_IMETHODIMP
CookieService::Add(const nsACString& aHost, const nsACString& aPath,
                   const nsACString& aName, const nsACString& aValue,
                   bool aIsSecure, bool aIsHttpOnly, bool aIsSession,
                   int64_t aExpiry, JS::Handle<JS::Value> aOriginAttributes,
                   int32_t aSameSite, nsICookie::schemeType aSchemeMap,
                   JSContext* aCx) {
  OriginAttributes attrs;

  if (!aOriginAttributes.isObject() || !attrs.Init(aCx, aOriginAttributes)) {
    return NS_ERROR_INVALID_ARG;
  }

  return AddNative(aHost, aPath, aName, aValue, aIsSecure, aIsHttpOnly,
                   aIsSession, aExpiry, &attrs, aSameSite, aSchemeMap);
}

NS_IMETHODIMP_(nsresult)
CookieService::AddNative(const nsACString& aHost, const nsACString& aPath,
                         const nsACString& aName, const nsACString& aValue,
                         bool aIsSecure, bool aIsHttpOnly, bool aIsSession,
                         int64_t aExpiry, OriginAttributes* aOriginAttributes,
                         int32_t aSameSite, nsICookie::schemeType aSchemeMap) {
  if (NS_WARN_IF(!aOriginAttributes)) {
    return NS_ERROR_FAILURE;
  }

  if (!IsInitialized()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // first, normalize the hostname, and fail if it contains illegal characters.
  nsAutoCString host(aHost);
  nsresult rv = NormalizeHost(host);
  NS_ENSURE_SUCCESS(rv, rv);

  // get the base domain for the host URI.
  // e.g. for "www.bbc.co.uk", this would be "bbc.co.uk".
  nsAutoCString baseDomain;
  rv = CookieCommons::GetBaseDomainFromHost(mTLDService, host, baseDomain);
  NS_ENSURE_SUCCESS(rv, rv);

  int64_t currentTimeInUsec = PR_Now();
  CookieKey key = CookieKey(baseDomain, *aOriginAttributes);

  CookieStruct cookieData(nsCString(aName), nsCString(aValue), nsCString(aHost),
                          nsCString(aPath), aExpiry, currentTimeInUsec,
                          Cookie::GenerateUniqueCreationTime(currentTimeInUsec),
                          aIsHttpOnly, aIsSession, aIsSecure, false, aSameSite,
                          aSameSite, aSchemeMap);

  RefPtr<Cookie> cookie = Cookie::Create(cookieData, key.mOriginAttributes);
  MOZ_ASSERT(cookie);

  CookieStorage* storage = PickStorage(*aOriginAttributes);
  storage->AddCookie(nullptr, baseDomain, *aOriginAttributes, cookie,
                     currentTimeInUsec, nullptr, VoidCString(), true, nullptr);
  return NS_OK;
}

nsresult CookieService::Remove(const nsACString& aHost,
                               const OriginAttributes& aAttrs,
                               const nsACString& aName,
                               const nsACString& aPath) {
  // first, normalize the hostname, and fail if it contains illegal characters.
  nsAutoCString host(aHost);
  nsresult rv = NormalizeHost(host);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString baseDomain;
  if (!host.IsEmpty()) {
    rv = CookieCommons::GetBaseDomainFromHost(mTLDService, host, baseDomain);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (!IsInitialized()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  CookieStorage* storage = PickStorage(aAttrs);
  storage->RemoveCookie(baseDomain, aAttrs, host, PromiseFlatCString(aName),
                        PromiseFlatCString(aPath));

  return NS_OK;
}

NS_IMETHODIMP
CookieService::Remove(const nsACString& aHost, const nsACString& aName,
                      const nsACString& aPath,
                      JS::Handle<JS::Value> aOriginAttributes, JSContext* aCx) {
  OriginAttributes attrs;

  if (!aOriginAttributes.isObject() || !attrs.Init(aCx, aOriginAttributes)) {
    return NS_ERROR_INVALID_ARG;
  }

  return RemoveNative(aHost, aName, aPath, &attrs);
}

NS_IMETHODIMP_(nsresult)
CookieService::RemoveNative(const nsACString& aHost, const nsACString& aName,
                            const nsACString& aPath,
                            OriginAttributes* aOriginAttributes) {
  if (NS_WARN_IF(!aOriginAttributes)) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = Remove(aHost, *aOriginAttributes, aName, aPath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void CookieService::GetCookiesForURI(
    nsIURI* aHostURI, nsIChannel* aChannel, bool aIsForeign,
    bool aIsThirdPartyTrackingResource,
    bool aIsThirdPartySocialTrackingResource,
    bool aStorageAccessPermissionGranted, uint32_t aRejectedReason,
    bool aIsSafeTopLevelNav, bool aIsSameSiteForeign,
    bool aHadCrossSiteRedirects, bool aHttpBound,
    bool aAllowSecureCookiesToInsecureOrigin,
    const nsTArray<OriginAttributes>& aOriginAttrsList,
    nsTArray<RefPtr<Cookie>>& aCookieList) {
  NS_ASSERTION(aHostURI, "null host!");

  if (!CookieCommons::IsSchemeSupported(aHostURI)) {
    return;
  }

  if (!IsInitialized()) {
    return;
  }

  nsCOMPtr<nsICookieJarSettings> cookieJarSettings =
      CookieCommons::GetCookieJarSettings(aChannel);

  nsCOMPtr<nsIConsoleReportCollector> crc = do_QueryInterface(aChannel);

  for (const auto& attrs : aOriginAttrsList) {
    CookieStorage* storage = PickStorage(attrs);

    // get the base domain, host, and path from the URI.
    // e.g. for "www.bbc.co.uk", the base domain would be "bbc.co.uk".
    // file:// URI's (i.e. with an empty host) are allowed, but any other
    // scheme must have a non-empty host. A trailing dot in the host
    // is acceptable.
    bool requireHostMatch;
    nsAutoCString baseDomain;
    nsAutoCString hostFromURI;
    nsAutoCString pathFromURI;
    nsresult rv = CookieCommons::GetBaseDomain(mTLDService, aHostURI,
                                               baseDomain, requireHostMatch);
    if (NS_SUCCEEDED(rv)) {
      rv = nsContentUtils::GetHostOrIPv6WithBrackets(aHostURI, hostFromURI);
    }
    if (NS_SUCCEEDED(rv)) {
      rv = aHostURI->GetFilePath(pathFromURI);
    }
    if (NS_FAILED(rv)) {
      COOKIE_LOGFAILURE(GET_COOKIE, aHostURI, VoidCString(),
                        "invalid host/path from URI");
      return;
    }

    nsAutoCString normalizedHostFromURI(hostFromURI);
    rv = NormalizeHost(normalizedHostFromURI);
    NS_ENSURE_SUCCESS_VOID(rv);

    nsAutoCString baseDomainFromURI;
    rv = CookieCommons::GetBaseDomainFromHost(
        mTLDService, normalizedHostFromURI, baseDomainFromURI);
    NS_ENSURE_SUCCESS_VOID(rv);

    // check default prefs
    uint32_t rejectedReason = aRejectedReason;
    uint32_t priorCookieCount = storage->CountCookiesFromHost(
        baseDomainFromURI, attrs.mPrivateBrowsingId);

    CookieStatus cookieStatus = CheckPrefs(
        crc, cookieJarSettings, aHostURI, aIsForeign,
        aIsThirdPartyTrackingResource, aIsThirdPartySocialTrackingResource,
        aStorageAccessPermissionGranted, VoidCString(), priorCookieCount, attrs,
        &rejectedReason);

    MOZ_ASSERT_IF(rejectedReason, cookieStatus == STATUS_REJECTED);

    // for GetCookie(), we only fire acceptance/rejection notifications
    // (but not if there was an error)
    switch (cookieStatus) {
      case STATUS_REJECTED:
        // If we don't have any cookies from this host, fail silently.
        if (priorCookieCount) {
          CookieCommons::NotifyRejected(aHostURI, aChannel, rejectedReason,
                                        OPERATION_READ);
        }
        return;
      default:
        break;
    }

    // Note: The following permissions logic is mirrored in
    // extensions::MatchPattern::MatchesCookie.
    // If it changes, please update that function, or file a bug for someone
    // else to do so.

    // check if aHostURI is using an https secure protocol.
    // if it isn't, then we can't send a secure cookie over the connection.
    // if SchemeIs fails, assume an insecure connection, to be on the safe side
    bool potentiallyTrustworthy =
        nsMixedContentBlocker::IsPotentiallyTrustworthyOrigin(aHostURI);

    int64_t currentTimeInUsec = PR_Now();
    int64_t currentTime = currentTimeInUsec / PR_USEC_PER_SEC;
    bool stale = false;

    nsTArray<RefPtr<Cookie>> cookies;
    storage->GetCookiesFromHost(baseDomain, attrs, cookies);
    if (cookies.IsEmpty()) {
      continue;
    }

    bool laxByDefault =
        StaticPrefs::network_cookie_sameSite_laxByDefault() &&
        !nsContentUtils::IsURIInPrefList(
            aHostURI, "network.cookie.sameSite.laxByDefault.disabledHosts");

    // iterate the cookies!
    for (Cookie* cookie : cookies) {
      // check the host, since the base domain lookup is conservative.
      if (!CookieCommons::DomainMatches(cookie, hostFromURI)) {
        continue;
      }

      // if the cookie is secure and the host scheme isn't, we avoid sending
      // cookie if possible. But for process synchronization purposes, we may
      // want the content process to know about the cookie (without it's value).
      // In which case we will wipe the value before sending
      if (cookie->IsSecure() && !potentiallyTrustworthy &&
          !aAllowSecureCookiesToInsecureOrigin) {
        continue;
      }

      // if the cookie is httpOnly and it's not going directly to the HTTP
      // connection, don't send it
      if (cookie->IsHttpOnly() && !aHttpBound) {
        continue;
      }

      // if the nsIURI path doesn't match the cookie path, don't send it back
      if (!CookieCommons::PathMatches(cookie, pathFromURI)) {
        continue;
      }

      // check if the cookie has expired
      if (cookie->Expiry() <= currentTime) {
        continue;
      }

      // Check if we need to block the cookie because of opt-in partitioning.
      // We will only allow partitioned cookies with "partitioned" attribution
      // if opt-in partitioning is enabled.
      if (aIsForeign && cookieJarSettings->GetPartitionForeign() &&
          (StaticPrefs::network_cookie_cookieBehavior_optInPartitioning() ||
           (attrs.mPrivateBrowsingId !=
                nsIScriptSecurityManager::DEFAULT_PRIVATE_BROWSING_ID &&
            StaticPrefs::
                network_cookie_cookieBehavior_optInPartitioning_pbmode())) &&
          !(cookie->IsPartitioned() && cookie->RawIsPartitioned()) &&
          !aStorageAccessPermissionGranted) {
        continue;
      }

      if (aHttpBound && aIsSameSiteForeign) {
        bool blockCookie = !ProcessSameSiteCookieForForeignRequest(
            aChannel, cookie, aIsSafeTopLevelNav, aHadCrossSiteRedirects,
            laxByDefault);

        if (blockCookie) {
          if (aHadCrossSiteRedirects) {
            CookieLogging::LogMessageToConsole(
                crc, aHostURI, nsIScriptError::warningFlag,
                CONSOLE_REJECTION_CATEGORY, "CookieBlockedCrossSiteRedirect"_ns,
                AutoTArray<nsString, 1>{
                    NS_ConvertUTF8toUTF16(cookie->Name()),
                });
          }
          continue;
        }
      }

      // all checks passed - add to list and check if lastAccessed stamp needs
      // updating
      aCookieList.AppendElement(cookie);
      if (cookie->IsStale()) {
        stale = true;
      }
    }

    if (aCookieList.IsEmpty()) {
      continue;
    }

    // update lastAccessed timestamps. we only do this if the timestamp is stale
    // by a certain amount, to avoid thrashing the db during pageload.
    if (stale) {
      storage->StaleCookies(aCookieList, currentTimeInUsec);
    }
  }

  if (aCookieList.IsEmpty()) {
    return;
  }

  // Send a notification about the acceptance of the cookies now that we found
  // some.
  NotifyAccepted(aChannel);

  // return cookies in order of path length; longest to shortest.
  // this is required per RFC2109.  if cookies match in length,
  // then sort by creation time (see bug 236772).
  aCookieList.Sort(CompareCookiesForSending());
}

static bool ContainsUnicodeChars(const nsCString& str) {
  const auto* start = str.BeginReading();
  const auto* end = str.EndReading();

  return std::find_if(start, end, [](unsigned char c) { return c >= 0x80; }) !=
         end;
}

static void RecordUnicodeTelemetry(const CookieStruct& cookieData) {
  auto label = Telemetry::LABELS_NETWORK_COOKIE_UNICODE_BYTE::none;
  if (ContainsUnicodeChars(cookieData.name())) {
    label = Telemetry::LABELS_NETWORK_COOKIE_UNICODE_BYTE::unicodeName;
  } else if (ContainsUnicodeChars(cookieData.value())) {
    label = Telemetry::LABELS_NETWORK_COOKIE_UNICODE_BYTE::unicodeValue;
  }
  Telemetry::AccumulateCategorical(label);
}

static void RecordPartitionedTelemetry(const CookieStruct& aCookieData,
                                       bool aIsForeign) {
  mozilla::glean::networking::set_cookie.Add(1);
  if (aCookieData.isPartitioned()) {
    mozilla::glean::networking::set_cookie_partitioned.AddToNumerator(1);
  }
  if (aIsForeign) {
    mozilla::glean::networking::set_cookie_foreign.AddToNumerator(1);
  }
  if (aIsForeign && aCookieData.isPartitioned()) {
    mozilla::glean::networking::set_cookie_foreign_partitioned.AddToNumerator(
        1);
  }
}

static bool HasSecurePrefix(const nsACString& aString) {
  return StringBeginsWith(aString, "__Secure-"_ns,
                          nsCaseInsensitiveCStringComparator);
}

static bool HasHostPrefix(const nsACString& aString) {
  return StringBeginsWith(aString, "__Host-"_ns,
                          nsCaseInsensitiveCStringComparator);
}

// processes a single cookie, and returns true if there are more cookies
// to be processed
bool CookieService::CanSetCookie(
    nsIURI* aHostURI, const nsACString& aBaseDomain, CookieStruct& aCookieData,
    bool aRequireHostMatch, CookieStatus aStatus, nsCString& aCookieHeader,
    bool aFromHttp, bool aIsForeignAndNotAddon, bool aPartitionedOnly,
    bool aIsInPrivateBrowsing, nsIConsoleReportCollector* aCRC,
    bool& aSetCookie) {
  MOZ_ASSERT(aHostURI);

  aSetCookie = false;

  // init expiryTime such that session cookies won't prematurely expire
  aCookieData.expiry() = INT64_MAX;

  aCookieData.schemeMap() = CookieCommons::URIToSchemeType(aHostURI);

  // aCookieHeader is an in/out param to point to the next cookie, if
  // there is one. Save the present value for logging purposes
  nsCString savedCookieHeader(aCookieHeader);

  // newCookie says whether there are multiple cookies in the header;
  // so we can handle them separately.
  nsAutoCString expires;
  nsAutoCString maxage;
  bool acceptedByParser = false;
  bool newCookie = ParseAttributes(aCRC, aHostURI, aCookieHeader, aCookieData,
                                   expires, maxage, acceptedByParser);
  if (!acceptedByParser) {
    return newCookie;
  }

  // Collect telemetry on how often secure cookies are set from non-secure
  // origins, and vice-versa.
  //
  // 0 = nonsecure and "http:"
  // 1 = nonsecure and "https:"
  // 2 = secure and "http:"
  // 3 = secure and "https:"
  bool potentiallyTrustworthy =
      nsMixedContentBlocker::IsPotentiallyTrustworthyOrigin(aHostURI);

  int64_t currentTimeInUsec = PR_Now();

  // calculate expiry time of cookie.
  aCookieData.isSession() =
      GetExpiry(aCookieData, expires, maxage,
                currentTimeInUsec / PR_USEC_PER_SEC, aFromHttp);
  if (aStatus == STATUS_ACCEPT_SESSION) {
    // force lifetime to session. note that the expiration time, if set above,
    // will still apply.
    aCookieData.isSession() = true;
  }

  // reject cookie if it's over the size limit, per RFC2109
  if (!CookieCommons::CheckNameAndValueSize(aCookieData)) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader,
                      "cookie too big (> 4kb)");

    AutoTArray<nsString, 2> params = {
        NS_ConvertUTF8toUTF16(aCookieData.name())};

    nsString size;
    size.AppendInt(kMaxBytesPerCookie);
    params.AppendElement(size);

    CookieLogging::LogMessageToConsole(
        aCRC, aHostURI, nsIScriptError::warningFlag, CONSOLE_OVERSIZE_CATEGORY,
        "CookieOversize"_ns, params);
    return newCookie;
  }

  RecordUnicodeTelemetry(aCookieData);

  // We count SetCookie operations in the parent process only for HTTP set
  // cookies to prevent double counting.
  if (XRE_IsParentProcess() || !aFromHttp) {
    RecordPartitionedTelemetry(aCookieData, aIsForeignAndNotAddon);
  }

  if (!CookieCommons::CheckName(aCookieData)) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader,
                      "invalid name character");
    CookieLogging::LogMessageToConsole(
        aCRC, aHostURI, nsIScriptError::warningFlag, CONSOLE_REJECTION_CATEGORY,
        "CookieRejectedInvalidCharName"_ns,
        AutoTArray<nsString, 1>{
            NS_ConvertUTF8toUTF16(aCookieData.name()),
        });
    return newCookie;
  }

  // domain & path checks
  if (!CheckDomain(aCookieData, aHostURI, aBaseDomain, aRequireHostMatch)) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader,
                      "failed the domain tests");
    CookieLogging::LogMessageToConsole(
        aCRC, aHostURI, nsIScriptError::warningFlag, CONSOLE_REJECTION_CATEGORY,
        "CookieRejectedInvalidDomain"_ns,
        AutoTArray<nsString, 1>{
            NS_ConvertUTF8toUTF16(aCookieData.name()),
        });
    return newCookie;
  }

  if (!CheckPath(aCookieData, aCRC, aHostURI)) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader,
                      "failed the path tests");
    return newCookie;
  }

  // If a cookie is nameless, then its value must not start with
  // `__Host-` or `__Secure-`
  if (aCookieData.name().IsEmpty() && (HasSecurePrefix(aCookieData.value()) ||
                                       HasHostPrefix(aCookieData.value()))) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader,
                      "failed hidden prefix tests");
    CookieLogging::LogMessageToConsole(
        aCRC, aHostURI, nsIScriptError::warningFlag, CONSOLE_REJECTION_CATEGORY,
        "CookieRejectedInvalidPrefix"_ns,
        AutoTArray<nsString, 1>{
            NS_ConvertUTF8toUTF16(aCookieData.name()),
        });
    return newCookie;
  }

  // magic prefix checks. MUST be run after CheckDomain() and CheckPath()
  if (!CheckPrefixes(aCookieData, potentiallyTrustworthy)) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader,
                      "failed the prefix tests");
    CookieLogging::LogMessageToConsole(
        aCRC, aHostURI, nsIScriptError::warningFlag, CONSOLE_REJECTION_CATEGORY,
        "CookieRejectedInvalidPrefix"_ns,
        AutoTArray<nsString, 1>{
            NS_ConvertUTF8toUTF16(aCookieData.name()),
        });
    return newCookie;
  }

  if (!CookieCommons::CheckValue(aCookieData)) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader,
                      "invalid value character");
    CookieLogging::LogMessageToConsole(
        aCRC, aHostURI, nsIScriptError::warningFlag, CONSOLE_REJECTION_CATEGORY,
        "CookieRejectedInvalidCharValue"_ns,
        AutoTArray<nsString, 1>{
            NS_ConvertUTF8toUTF16(aCookieData.name()),
        });
    return newCookie;
  }

  // if the new cookie is httponly, make sure we're not coming from script
  if (!aFromHttp && aCookieData.isHttpOnly()) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader,
                      "cookie is httponly; coming from script");
    CookieLogging::LogMessageToConsole(
        aCRC, aHostURI, nsIScriptError::warningFlag, CONSOLE_REJECTION_CATEGORY,
        "CookieRejectedHttpOnlyButFromScript"_ns,
        AutoTArray<nsString, 1>{
            NS_ConvertUTF8toUTF16(aCookieData.name()),
        });
    return newCookie;
  }

  // If the new cookie is non-https and wants to set secure flag,
  // browser have to ignore this new cookie.
  // (draft-ietf-httpbis-cookie-alone section 3.1)
  if (aCookieData.isSecure() && !potentiallyTrustworthy) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, aCookieHeader,
                      "non-https cookie can't set secure flag");
    CookieLogging::LogMessageToConsole(
        aCRC, aHostURI, nsIScriptError::warningFlag, CONSOLE_REJECTION_CATEGORY,
        "CookieRejectedSecureButNonHttps"_ns,
        AutoTArray<nsString, 1>{
            NS_ConvertUTF8toUTF16(aCookieData.name()),
        });
    return newCookie;
  }

  // If the new cookie is same-site but in a cross site context,
  // browser must ignore the cookie.
  bool laxByDefault =
      StaticPrefs::network_cookie_sameSite_laxByDefault() &&
      !nsContentUtils::IsURIInPrefList(
          aHostURI, "network.cookie.sameSite.laxByDefault.disabledHosts");
  auto effectiveSameSite =
      laxByDefault ? aCookieData.sameSite() : aCookieData.rawSameSite();
  if ((effectiveSameSite != nsICookie::SAMESITE_NONE) &&
      aIsForeignAndNotAddon) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader,
                      "failed the samesite tests");

    CookieLogging::LogMessageToConsole(
        aCRC, aHostURI, nsIScriptError::warningFlag, CONSOLE_SAMESITE_CATEGORY,
        "CookieRejectedForNonSameSiteness"_ns,
        AutoTArray<nsString, 1>{
            NS_ConvertUTF8toUTF16(aCookieData.name()),
        });
    return newCookie;
  }

  // If the cookie does not have the partitioned attribute,
  // but is foreign we should give the developer a message.
  // If CHIPS isn't required yet, we will warn the console
  // that we have upcoming changes. Otherwise we give a rejection message.
  if (aPartitionedOnly && !aCookieData.isPartitioned() &&
      aIsForeignAndNotAddon) {
    if (StaticPrefs::network_cookie_cookieBehavior_optInPartitioning() ||
        (aIsInPrivateBrowsing &&
         StaticPrefs::
             network_cookie_cookieBehavior_optInPartitioning_pbmode())) {
      COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader,
                        "foreign cookies must be partitioned");
      CookieLogging::LogMessageToConsole(
          aCRC, aHostURI, nsIScriptError::warningFlag, CONSOLE_CHIPS_CATEGORY,
          "CookieForeignNoPartitionedError"_ns,
          AutoTArray<nsString, 1>{
              NS_ConvertUTF8toUTF16(aCookieData.name()),
          });
      return newCookie;
    }
    CookieLogging::LogMessageToConsole(
        aCRC, aHostURI, nsIScriptError::warningFlag, CONSOLE_CHIPS_CATEGORY,
        "CookieForeignNoPartitionedWarning"_ns,
        AutoTArray<nsString, 1>{
            NS_ConvertUTF8toUTF16(aCookieData.name()),
        });
  }

  aSetCookie = true;
  return newCookie;
}

/******************************************************************************
 * CookieService impl:
 * private cookie header parsing functions
 ******************************************************************************/

// clang-format off
// The following comment block elucidates the function of ParseAttributes.
/******************************************************************************
 ** Augmented BNF, modified from RFC2109 Section 4.2.2 and RFC2616 Section 2.1
 ** please note: this BNF deviates from both specifications, and reflects this
 ** implementation. <bnf> indicates a reference to the defined grammar "bnf".

 ** Differences from RFC2109/2616 and explanations:
    1. implied *LWS
         The grammar described by this specification is word-based. Except
         where noted otherwise, linear white space (<LWS>) can be included
         between any two adjacent words (token or quoted-string), and
         between adjacent words and separators, without changing the
         interpretation of a field.
       <LWS> according to spec is SP|HT|CR|LF, but here, we allow only SP | HT.

    2. We use CR | LF as cookie separators, not ',' per spec, since ',' is in
       common use inside values.

    3. tokens and values have looser restrictions on allowed characters than
       spec. This is also due to certain characters being in common use inside
       values. We allow only '=' to separate token/value pairs, and ';' to
       terminate tokens or values. <LWS> is allowed within tokens and values
       (see bug 206022).

    4. where appropriate, full <OCTET>s are allowed, where the spec dictates to
       reject control chars or non-ASCII chars. This is erring on the loose
       side, since there's probably no good reason to enforce this strictness.

    5. Attribute "HttpOnly", not covered in the RFCs, is supported
       (see bug 178993).

 ** Begin BNF:
    token         = 1*<any allowed-chars except separators>
    value         = 1*<any allowed-chars except value-sep>
    separators    = ";" | "="
    value-sep     = ";"
    cookie-sep    = CR | LF
    allowed-chars = <any OCTET except cookie-sep>
    OCTET         = <any 8-bit sequence of data>
    LWS           = SP | HT
    CR            = <US-ASCII CR, carriage return (13)>
    LF            = <US-ASCII LF, linefeed (10)>
    SP            = <US-ASCII SP, space (32)>
    HT            = <US-ASCII HT, horizontal-tab (9)>

    set-cookie    = "Set-Cookie:" cookies
    cookies       = cookie *( cookie-sep cookie )
    cookie        = [NAME "="] VALUE *(";" cookie-av)    ; cookie NAME/VALUE must come first
    NAME          = token                                ; cookie name
    VALUE         = value                                ; cookie value
    cookie-av     = token ["=" value]

    valid values for cookie-av (checked post-parsing) are:
    cookie-av     = "Path"    "=" value
                  | "Domain"  "=" value
                  | "Expires" "=" value
                  | "Max-Age" "=" value
                  | "Comment" "=" value
                  | "Version" "=" value
                  | "Partitioned"
                  | "SameSite"
                  | "Secure"
                  | "HttpOnly"

******************************************************************************/
// clang-format on

// helper functions for GetTokenValue
static inline bool iswhitespace(char c) { return c == ' ' || c == '\t'; }
static inline bool isterminator(char c) { return c == '\n' || c == '\r'; }
static inline bool isvalueseparator(char c) {
  return isterminator(c) || c == ';';
}
static inline bool istokenseparator(char c) {
  return isvalueseparator(c) || c == '=';
}

// Parse a single token/value pair.
// Returns true if a cookie terminator is found, so caller can parse new cookie.
bool CookieService::GetTokenValue(nsACString::const_char_iterator& aIter,
                                  nsACString::const_char_iterator& aEndIter,
                                  nsDependentCSubstring& aTokenString,
                                  nsDependentCSubstring& aTokenValue,
                                  bool& aEqualsFound) {
  nsACString::const_char_iterator start;
  nsACString::const_char_iterator lastSpace;
  // initialize value string to clear garbage
  aTokenValue.Rebind(aIter, aIter);

  // find <token>, including any <LWS> between the end-of-token and the
  // token separator. we'll remove trailing <LWS> next
  while (aIter != aEndIter && iswhitespace(*aIter)) {
    ++aIter;
  }
  start = aIter;
  while (aIter != aEndIter && !istokenseparator(*aIter)) {
    ++aIter;
  }

  // remove trailing <LWS>; first check we're not at the beginning
  lastSpace = aIter;
  if (lastSpace != start) {
    while (--lastSpace != start && iswhitespace(*lastSpace)) {
    }
    ++lastSpace;
  }
  aTokenString.Rebind(start, lastSpace);

  aEqualsFound = (*aIter == '=');
  if (aEqualsFound) {
    // find <value>
    while (++aIter != aEndIter && iswhitespace(*aIter)) {
    }

    start = aIter;

    // process <token>
    // just look for ';' to terminate ('=' allowed)
    while (aIter != aEndIter && !isvalueseparator(*aIter)) {
      ++aIter;
    }

    // remove trailing <LWS>; first check we're not at the beginning
    if (aIter != start) {
      lastSpace = aIter;
      while (--lastSpace != start && iswhitespace(*lastSpace)) {
      }

      aTokenValue.Rebind(start, ++lastSpace);
    }
  }

  // aIter is on ';', or terminator, or EOS
  if (aIter != aEndIter) {
    // if on terminator, increment past & return true to process new cookie
    if (isterminator(*aIter)) {
      ++aIter;
      while (aIter != aEndIter && isvalueseparator(*aIter)) {
        ++aIter;
      }
      nsACString::const_char_iterator end = aIter - 1;
      if (!isterminator(*end)) {
        // The cookie isn't valid because we have multiple terminators or
        // a terminator followed by a value separator. Add those invalid
        // characters to the cookie string or value so it will be rejected.
        if (aEqualsFound) {
          aTokenString.Rebind(start, end);
        } else {
          aTokenValue.Rebind(start, end);
        }
        return false;
      }
      return true;
    }
    // fall-through: aIter is on ';', increment and return false
    ++aIter;
  }
  return false;
}

static inline void SetSameSiteAttributeDefault(CookieStruct& aCookieData) {
  // Set cookie with SameSite attribute that is treated as Default
  // and doesn't requires changing the DB schema.
  aCookieData.sameSite() = nsICookie::SAMESITE_LAX;
  aCookieData.rawSameSite() = nsICookie::SAMESITE_NONE;
}

static inline void SetSameSiteAttribute(CookieStruct& aCookieData,
                                        int32_t aValue) {
  aCookieData.sameSite() = aValue;
  aCookieData.rawSameSite() = aValue;
}

// Tests for control characters, defined by RFC 5234 to be %x00-1F / %x7F.
// An exception is made for HTAB as the cookie spec treats that as whitespace.
static bool ContainsControlChars(const nsACString& aString) {
  const auto* start = aString.BeginReading();
  const auto* end = aString.EndReading();

  return std::find_if(start, end, [](unsigned char c) {
           return (c <= 0x1F && c != 0x09) || c == 0x7F;
         }) != end;
}

// Parses attributes from cookie header. expires/max-age attributes aren't
// folded into the cookie struct here, because we don't know which one to use
// until we've parsed the header.
bool CookieService::ParseAttributes(nsIConsoleReportCollector* aCRC,
                                    nsIURI* aHostURI, nsCString& aCookieHeader,
                                    CookieStruct& aCookieData,
                                    nsACString& aExpires, nsACString& aMaxage,
                                    bool& aAcceptedByParser) {
  aAcceptedByParser = false;

  static const char kPath[] = "path";
  static const char kDomain[] = "domain";
  static const char kExpires[] = "expires";
  static const char kMaxage[] = "max-age";
  static const char kSecure[] = "secure";
  static const char kHttpOnly[] = "httponly";
  static const char kSameSite[] = "samesite";
  static const char kSameSiteLax[] = "lax";
  static const char kSameSiteNone[] = "none";
  static const char kSameSiteStrict[] = "strict";
  static const char kPartitioned[] = "partitioned";

  nsACString::const_char_iterator cookieStart;
  aCookieHeader.BeginReading(cookieStart);

  nsACString::const_char_iterator cookieEnd;
  aCookieHeader.EndReading(cookieEnd);

  aCookieData.isSecure() = false;
  aCookieData.isHttpOnly() = false;

  SetSameSiteAttributeDefault(aCookieData);

  nsDependentCSubstring tokenString(cookieStart, cookieStart);
  nsDependentCSubstring tokenValue(cookieStart, cookieStart);
  bool newCookie;
  bool equalsFound;

  // extract cookie <NAME> & <VALUE> (first attribute), and copy the strings.
  // if we find multiple cookies, return for processing
  // note: if there's no '=', we assume token is <VALUE>. this is required by
  //       some sites (see bug 169091).
  // XXX fix the parser to parse according to <VALUE> grammar for this case
  newCookie = GetTokenValue(cookieStart, cookieEnd, tokenString, tokenValue,
                            equalsFound);
  if (equalsFound) {
    aCookieData.name() = tokenString;
    aCookieData.value() = tokenValue;
  } else {
    aCookieData.value() = tokenString;
  }

  // extract remaining attributes
  while (cookieStart != cookieEnd && !newCookie) {
    newCookie = GetTokenValue(cookieStart, cookieEnd, tokenString, tokenValue,
                              equalsFound);

    if (ContainsControlChars(tokenString) || ContainsControlChars(tokenValue)) {
      CookieLogging::LogMessageToConsole(
          aCRC, aHostURI, nsIScriptError::errorFlag, CONSOLE_REJECTION_CATEGORY,
          "CookieRejectedInvalidCharAttributes"_ns,
          AutoTArray<nsString, 1>{NS_ConvertUTF8toUTF16(aCookieData.name())});
      return newCookie;
    }

    // decide which attribute we have, and copy the string
    if (tokenString.LowerCaseEqualsLiteral(kPath)) {
      aCookieData.path() = tokenValue;

    } else if (tokenString.LowerCaseEqualsLiteral(kDomain)) {
      aCookieData.host() = tokenValue;

    } else if (tokenString.LowerCaseEqualsLiteral(kExpires)) {
      aExpires = tokenValue;

    } else if (tokenString.LowerCaseEqualsLiteral(kMaxage)) {
      aMaxage = tokenValue;

      // ignore any tokenValue for isSecure; just set the boolean
    } else if (tokenString.LowerCaseEqualsLiteral(kSecure)) {
      aCookieData.isSecure() = true;

      // ignore any tokenValue for isPartitioned; just set the boolean
    } else if (tokenString.LowerCaseEqualsLiteral(kPartitioned)) {
      aCookieData.isPartitioned() = true;

      // ignore any tokenValue for isHttpOnly (see bug 178993);
      // just set the boolean
    } else if (tokenString.LowerCaseEqualsLiteral(kHttpOnly)) {
      aCookieData.isHttpOnly() = true;

    } else if (tokenString.LowerCaseEqualsLiteral(kSameSite)) {
      if (tokenValue.LowerCaseEqualsLiteral(kSameSiteLax)) {
        SetSameSiteAttribute(aCookieData, nsICookie::SAMESITE_LAX);
      } else if (tokenValue.LowerCaseEqualsLiteral(kSameSiteStrict)) {
        SetSameSiteAttribute(aCookieData, nsICookie::SAMESITE_STRICT);
      } else if (tokenValue.LowerCaseEqualsLiteral(kSameSiteNone)) {
        SetSameSiteAttribute(aCookieData, nsICookie::SAMESITE_NONE);
      } else {
        // Reset to Default if unknown token value (see Bug 1682450)
        SetSameSiteAttributeDefault(aCookieData);
        CookieLogging::LogMessageToConsole(
            aCRC, aHostURI, nsIScriptError::infoFlag, CONSOLE_SAMESITE_CATEGORY,
            "CookieSameSiteValueInvalid2"_ns,
            AutoTArray<nsString, 1>{NS_ConvertUTF8toUTF16(aCookieData.name())});
      }
    }
  }

  // re-assign aCookieHeader, in case we need to process another cookie
  aCookieHeader.Assign(Substring(cookieStart, cookieEnd));

  // If same-site is explicitly set to 'none' but this is not a secure context,
  // let's abort the parsing.
  if (!aCookieData.isSecure() &&
      aCookieData.sameSite() == nsICookie::SAMESITE_NONE) {
    if (StaticPrefs::network_cookie_sameSite_noneRequiresSecure()) {
      CookieLogging::LogMessageToConsole(
          aCRC, aHostURI, nsIScriptError::errorFlag, CONSOLE_SAMESITE_CATEGORY,
          "CookieRejectedNonRequiresSecure2"_ns,
          AutoTArray<nsString, 1>{NS_ConvertUTF8toUTF16(aCookieData.name())});
      return newCookie;
    }

    // Still warn about the missing Secure attribute when not enforcing.
    CookieLogging::LogMessageToConsole(
        aCRC, aHostURI, nsIScriptError::warningFlag, CONSOLE_SAMESITE_CATEGORY,
        "CookieRejectedNonRequiresSecureForBeta3"_ns,
        AutoTArray<nsString, 2>{NS_ConvertUTF8toUTF16(aCookieData.name()),
                                SAMESITE_MDN_URL});
  }

  // Ensure the partitioned cookie is set with the secure attribute if CHIPS
  // is enabled.
  if (StaticPrefs::network_cookie_CHIPS_enabled() &&
      aCookieData.isPartitioned() && !aCookieData.isSecure()) {
    CookieLogging::LogMessageToConsole(
        aCRC, aHostURI, nsIScriptError::errorFlag, CONSOLE_REJECTION_CATEGORY,
        "CookieRejectedPartitionedRequiresSecure"_ns,
        AutoTArray<nsString, 1>{NS_ConvertUTF8toUTF16(aCookieData.name())});

    return newCookie;
  }

  if (aCookieData.rawSameSite() == nsICookie::SAMESITE_NONE &&
      aCookieData.sameSite() == nsICookie::SAMESITE_LAX) {
    bool laxByDefault =
        StaticPrefs::network_cookie_sameSite_laxByDefault() &&
        !nsContentUtils::IsURIInPrefList(
            aHostURI, "network.cookie.sameSite.laxByDefault.disabledHosts");
    if (laxByDefault) {
      CookieLogging::LogMessageToConsole(
          aCRC, aHostURI, nsIScriptError::infoFlag, CONSOLE_SAMESITE_CATEGORY,
          "CookieLaxForced2"_ns,
          AutoTArray<nsString, 1>{NS_ConvertUTF8toUTF16(aCookieData.name())});
    } else {
      CookieLogging::LogMessageToConsole(
          aCRC, aHostURI, nsIScriptError::warningFlag,
          CONSOLE_SAMESITE_CATEGORY, "CookieLaxForcedForBeta2"_ns,
          AutoTArray<nsString, 2>{NS_ConvertUTF8toUTF16(aCookieData.name()),
                                  SAMESITE_MDN_URL});
    }
  }

  // Cookie accepted.
  aAcceptedByParser = true;

  MOZ_ASSERT(Cookie::ValidateSameSite(aCookieData));
  return newCookie;
}

/******************************************************************************
 * CookieService impl:
 * private domain & permission compliance enforcement functions
 ******************************************************************************/

// Normalizes the given hostname, component by component. ASCII/ACE
// components are lower-cased, and UTF-8 components are normalized per
// RFC 3454 and converted to ACE.
nsresult CookieService::NormalizeHost(nsCString& aHost) {
  if (!IsAscii(aHost)) {
    nsAutoCString host;
    nsresult rv = mIDNService->ConvertUTF8toACE(aHost, host);
    if (NS_FAILED(rv)) {
      return rv;
    }

    aHost = host;
  }

  ToLowerCase(aHost);
  return NS_OK;
}

// returns true if 'a' is equal to or a subdomain of 'b',
// assuming no leading dots are present.
static inline bool IsSubdomainOf(const nsACString& a, const nsACString& b) {
  if (a == b) {
    return true;
  }
  if (a.Length() > b.Length()) {
    return a[a.Length() - b.Length() - 1] == '.' && StringEndsWith(a, b);
  }
  return false;
}

CookieStatus CookieService::CheckPrefs(
    nsIConsoleReportCollector* aCRC, nsICookieJarSettings* aCookieJarSettings,
    nsIURI* aHostURI, bool aIsForeign, bool aIsThirdPartyTrackingResource,
    bool aIsThirdPartySocialTrackingResource,
    bool aStorageAccessPermissionGranted, const nsACString& aCookieHeader,
    const int aNumOfCookies, const OriginAttributes& aOriginAttrs,
    uint32_t* aRejectedReason) {
  nsresult rv;

  MOZ_ASSERT(aRejectedReason);

  *aRejectedReason = 0;

  // don't let unsupported scheme sites get/set cookies (could be a security
  // issue)
  if (!CookieCommons::IsSchemeSupported(aHostURI)) {
    COOKIE_LOGFAILURE(!aCookieHeader.IsVoid(), aHostURI, aCookieHeader,
                      "non http/https sites cannot read cookies");
    return STATUS_REJECTED_WITH_ERROR;
  }

  nsCOMPtr<nsIPrincipal> principal =
      BasePrincipal::CreateContentPrincipal(aHostURI, aOriginAttrs);

  if (!principal) {
    COOKIE_LOGFAILURE(!aCookieHeader.IsVoid(), aHostURI, aCookieHeader,
                      "non-content principals cannot get/set cookies");
    return STATUS_REJECTED_WITH_ERROR;
  }

  // check the permission list first; if we find an entry, it overrides
  // default prefs. see bug 184059.
  uint32_t cookiePermission = nsICookiePermission::ACCESS_DEFAULT;
  rv = aCookieJarSettings->CookiePermission(principal, &cookiePermission);
  if (NS_SUCCEEDED(rv)) {
    switch (cookiePermission) {
      case nsICookiePermission::ACCESS_DENY:
        COOKIE_LOGFAILURE(!aCookieHeader.IsVoid(), aHostURI, aCookieHeader,
                          "cookies are blocked for this site");
        CookieLogging::LogMessageToConsole(
            aCRC, aHostURI, nsIScriptError::warningFlag,
            CONSOLE_REJECTION_CATEGORY, "CookieRejectedByPermissionManager"_ns,
            AutoTArray<nsString, 1>{
                NS_ConvertUTF8toUTF16(aCookieHeader),
            });

        *aRejectedReason =
            nsIWebProgressListener::STATE_COOKIES_BLOCKED_BY_PERMISSION;
        return STATUS_REJECTED;

      case nsICookiePermission::ACCESS_ALLOW:
        return STATUS_ACCEPTED;
      default:
        break;
    }
  }

  // No cookies allowed if this request comes from a resource in a 3rd party
  // context, when anti-tracking protection is enabled and when we don't have
  // access to the first-party cookie jar.
  if (aIsForeign && aIsThirdPartyTrackingResource &&
      !aStorageAccessPermissionGranted &&
      aCookieJarSettings->GetRejectThirdPartyContexts()) {
    uint32_t rejectReason =
        nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER;
    if (StoragePartitioningEnabled(rejectReason, aCookieJarSettings)) {
      MOZ_ASSERT(!aOriginAttrs.mPartitionKey.IsEmpty(),
                 "We must have a StoragePrincipal here!");
      return STATUS_ACCEPTED;
    }

    COOKIE_LOGFAILURE(!aCookieHeader.IsVoid(), aHostURI, aCookieHeader,
                      "cookies are disabled in trackers");
    if (aIsThirdPartySocialTrackingResource) {
      *aRejectedReason =
          nsIWebProgressListener::STATE_COOKIES_BLOCKED_SOCIALTRACKER;
    } else {
      *aRejectedReason = nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER;
    }
    return STATUS_REJECTED;
  }

  // check default prefs.
  // Check aStorageAccessPermissionGranted when checking aCookieBehavior
  // so that we take things such as the content blocking allow list into
  // account.
  if (aCookieJarSettings->GetCookieBehavior() ==
          nsICookieService::BEHAVIOR_REJECT &&
      !aStorageAccessPermissionGranted) {
    COOKIE_LOGFAILURE(!aCookieHeader.IsVoid(), aHostURI, aCookieHeader,
                      "cookies are disabled");
    *aRejectedReason = nsIWebProgressListener::STATE_COOKIES_BLOCKED_ALL;
    return STATUS_REJECTED;
  }

  // check if cookie is foreign
  if (aIsForeign) {
    if (aCookieJarSettings->GetCookieBehavior() ==
            nsICookieService::BEHAVIOR_REJECT_FOREIGN &&
        !aStorageAccessPermissionGranted) {
      COOKIE_LOGFAILURE(!aCookieHeader.IsVoid(), aHostURI, aCookieHeader,
                        "context is third party");
      CookieLogging::LogMessageToConsole(
          aCRC, aHostURI, nsIScriptError::warningFlag,
          CONSOLE_REJECTION_CATEGORY, "CookieRejectedThirdParty"_ns,
          AutoTArray<nsString, 1>{
              NS_ConvertUTF8toUTF16(aCookieHeader),
          });
      *aRejectedReason = nsIWebProgressListener::STATE_COOKIES_BLOCKED_FOREIGN;
      return STATUS_REJECTED;
    }

    if (aCookieJarSettings->GetLimitForeignContexts() &&
        !aStorageAccessPermissionGranted && aNumOfCookies == 0) {
      COOKIE_LOGFAILURE(!aCookieHeader.IsVoid(), aHostURI, aCookieHeader,
                        "context is third party");
      CookieLogging::LogMessageToConsole(
          aCRC, aHostURI, nsIScriptError::warningFlag,
          CONSOLE_REJECTION_CATEGORY, "CookieRejectedThirdParty"_ns,
          AutoTArray<nsString, 1>{
              NS_ConvertUTF8toUTF16(aCookieHeader),
          });
      *aRejectedReason = nsIWebProgressListener::STATE_COOKIES_BLOCKED_FOREIGN;
      return STATUS_REJECTED;
    }

    if (StaticPrefs::network_cookie_thirdparty_sessionOnly()) {
      return STATUS_ACCEPT_SESSION;
    }

    if (StaticPrefs::network_cookie_thirdparty_nonsecureSessionOnly()) {
      if (!aHostURI->SchemeIs("https")) {
        return STATUS_ACCEPT_SESSION;
      }
    }
  }

  // if nothing has complained, accept cookie
  return STATUS_ACCEPTED;
}

// processes domain attribute, and returns true if host has permission to set
// for this domain.
bool CookieService::CheckDomain(CookieStruct& aCookieData, nsIURI* aHostURI,
                                const nsACString& aBaseDomain,
                                bool aRequireHostMatch) {
  // Note: The logic in this function is mirrored in
  // toolkit/components/extensions/ext-cookies.js:checkSetCookiePermissions().
  // If it changes, please update that function, or file a bug for someone
  // else to do so.

  // get host from aHostURI
  nsAutoCString hostFromURI;
  nsContentUtils::GetHostOrIPv6WithBrackets(aHostURI, hostFromURI);

  // if a domain is given, check the host has permission
  if (!aCookieData.host().IsEmpty()) {
    // Tolerate leading '.' characters, but not if it's otherwise an empty host.
    if (aCookieData.host().Length() > 1 && aCookieData.host().First() == '.') {
      aCookieData.host().Cut(0, 1);
    }

    // switch to lowercase now, to avoid case-insensitive compares everywhere
    ToLowerCase(aCookieData.host());

    // check whether the host is either an IP address, an alias such as
    // 'localhost', an eTLD such as 'co.uk', or the empty string. in these
    // cases, require an exact string match for the domain, and leave the cookie
    // as a non-domain one. bug 105917 originally noted the requirement to deal
    // with IP addresses.
    if (aRequireHostMatch) {
      return hostFromURI.Equals(aCookieData.host());
    }

    // ensure the proposed domain is derived from the base domain; and also
    // that the host domain is derived from the proposed domain (per RFC2109).
    if (IsSubdomainOf(aCookieData.host(), aBaseDomain) &&
        IsSubdomainOf(hostFromURI, aCookieData.host())) {
      // prepend a dot to indicate a domain cookie
      aCookieData.host().InsertLiteral(".", 0);
      return true;
    }

    /*
     * note: RFC2109 section 4.3.2 requires that we check the following:
     * that the portion of host not in domain does not contain a dot.
     * this prevents hosts of the form x.y.co.nz from setting cookies in the
     * entire .co.nz domain. however, it's only a only a partial solution and
     * it breaks sites (IE doesn't enforce it), so we don't perform this check.
     */
    return false;
  }

  // no domain specified, use hostFromURI
  aCookieData.host() = hostFromURI;
  return true;
}

namespace {
nsAutoCString GetPathFromURI(nsIURI* aHostURI) {
  // strip down everything after the last slash to get the path,
  // ignoring slashes in the query string part.
  // if we can QI to nsIURL, that'll take care of the query string portion.
  // otherwise, it's not an nsIURL and can't have a query string, so just find
  // the last slash.
  nsAutoCString path;
  nsCOMPtr<nsIURL> hostURL = do_QueryInterface(aHostURI);
  if (hostURL) {
    hostURL->GetDirectory(path);
  } else {
    aHostURI->GetPathQueryRef(path);
    int32_t slash = path.RFindChar('/');
    if (slash != kNotFound) {
      path.Truncate(slash + 1);
    }
  }

  // strip the right-most %x2F ("/") if the path doesn't contain only 1 '/'.
  int32_t lastSlash = path.RFindChar('/');
  int32_t firstSlash = path.FindChar('/');
  if (lastSlash != firstSlash && lastSlash != kNotFound &&
      lastSlash == static_cast<int32_t>(path.Length() - 1)) {
    path.Truncate(lastSlash);
  }

  return path;
}

}  // namespace

bool CookieService::CheckPath(CookieStruct& aCookieData,
                              nsIConsoleReportCollector* aCRC,
                              nsIURI* aHostURI) {
  // if a path is given, check the host has permission
  if (aCookieData.path().IsEmpty() || aCookieData.path().First() != '/') {
    aCookieData.path() = GetPathFromURI(aHostURI);
  }

  if (!CookieCommons::CheckPathSize(aCookieData)) {
    AutoTArray<nsString, 2> params = {
        NS_ConvertUTF8toUTF16(aCookieData.name())};

    nsString size;
    size.AppendInt(kMaxBytesPerPath);
    params.AppendElement(size);

    CookieLogging::LogMessageToConsole(
        aCRC, aHostURI, nsIScriptError::warningFlag, CONSOLE_OVERSIZE_CATEGORY,
        "CookiePathOversize"_ns, params);
    return false;
  }

  return !aCookieData.path().Contains('\t');
}

// CheckPrefixes
//
// Reject cookies whose name starts with the magic prefixes from
// https://datatracker.ietf.org/doc/html/draft-ietf-httpbis-rfc6265bis
// if they do not meet the criteria required by the prefix.
//
// Must not be called until after CheckDomain() and CheckPath() have
// regularized and validated the CookieStruct values!
bool CookieService::CheckPrefixes(CookieStruct& aCookieData,
                                  bool aSecureRequest) {
  bool hasSecurePrefix = HasSecurePrefix(aCookieData.name());
  bool hasHostPrefix = HasHostPrefix(aCookieData.name());

  if (!hasSecurePrefix && !hasHostPrefix) {
    // not one of the magic prefixes: carry on
    return true;
  }

  if (!aSecureRequest || !aCookieData.isSecure()) {
    // the magic prefixes may only be used from a secure request and
    // the secure attribute must be set on the cookie
    return false;
  }

  if (hasHostPrefix) {
    // The host prefix requires that the path is "/" and that the cookie
    // had no domain attribute. CheckDomain() and CheckPath() MUST be run
    // first to make sure invalid attributes are rejected and to regularlize
    // them. In particular all explicit domain attributes result in a host
    // that starts with a dot, and if the host doesn't start with a dot it
    // correctly matches the true host.
    if (aCookieData.host()[0] == '.' ||
        !aCookieData.path().EqualsLiteral("/")) {
      return false;
    }
  }

  return true;
}

bool CookieService::GetExpiry(CookieStruct& aCookieData,
                              const nsACString& aExpires,
                              const nsACString& aMaxage, int64_t aCurrentTime,
                              bool aFromHttp) {
  // maxageCap is in seconds.
  // Disabled for HTTP cookies.
  int64_t maxageCap =
      aFromHttp ? 0 : StaticPrefs::privacy_documentCookies_maxage();

  /* Determine when the cookie should expire. This is done by taking the
   * difference between the server time and the time the server wants the cookie
   * to expire, and adding that difference to the client time. This localizes
   * the client time regardless of whether or not the TZ environment variable
   * was set on the client.
   *
   * Note: We need to consider accounting for network lag here, per RFC.
   */
  // check for max-age attribute first; this overrides expires attribute
  if (!aMaxage.IsEmpty()) {
    // obtain numeric value of maxageAttribute
    int64_t maxage;
    int32_t numInts = PR_sscanf(aMaxage.BeginReading(), "%lld", &maxage);

    // default to session cookie if the conversion failed
    if (numInts != 1) {
      return true;
    }

    // if this addition overflows, expiryTime will be less than currentTime
    // and the cookie will be expired - that's okay.
    if (maxageCap) {
      aCookieData.expiry() = aCurrentTime + std::min(maxage, maxageCap);
    } else {
      aCookieData.expiry() = aCurrentTime + maxage;
    }

    // check for expires attribute
  } else if (!aExpires.IsEmpty()) {
    PRTime expires;

    // parse expiry time
    if (PR_ParseTimeString(aExpires.BeginReading(), true, &expires) !=
        PR_SUCCESS) {
      return true;
    }

    // If set-cookie used absolute time to set expiration, and it can't use
    // client time to set expiration.
    // Because if current time be set in the future, but the cookie expire
    // time be set less than current time and more than server time.
    // The cookie item have to be used to the expired cookie.
    if (maxageCap) {
      aCookieData.expiry() = std::min(expires / int64_t(PR_USEC_PER_SEC),
                                      aCurrentTime + maxageCap);
    } else {
      aCookieData.expiry() = expires / int64_t(PR_USEC_PER_SEC);
    }

    // default to session cookie if no attributes found.  Here we don't need to
    // enforce the maxage cap, because session cookies are short-lived by
    // definition.
  } else {
    return true;
  }

  return false;
}

/******************************************************************************
 * CookieService impl:
 * private cookielist management functions
 ******************************************************************************/

// find whether a given cookie has been previously set. this is provided by the
// nsICookieManager interface.
NS_IMETHODIMP
CookieService::CookieExists(const nsACString& aHost, const nsACString& aPath,
                            const nsACString& aName,
                            JS::Handle<JS::Value> aOriginAttributes,
                            JSContext* aCx, bool* aFoundCookie) {
  NS_ENSURE_ARG_POINTER(aCx);
  NS_ENSURE_ARG_POINTER(aFoundCookie);

  OriginAttributes attrs;
  if (!aOriginAttributes.isObject() || !attrs.Init(aCx, aOriginAttributes)) {
    return NS_ERROR_INVALID_ARG;
  }
  return CookieExistsNative(aHost, aPath, aName, &attrs, aFoundCookie);
}

NS_IMETHODIMP_(nsresult)
CookieService::CookieExistsNative(const nsACString& aHost,
                                  const nsACString& aPath,
                                  const nsACString& aName,
                                  OriginAttributes* aOriginAttributes,
                                  bool* aFoundCookie) {
  nsCOMPtr<nsICookie> cookie;
  nsresult rv = GetCookieNative(aHost, aPath, aName, aOriginAttributes,
                                getter_AddRefs(cookie));
  NS_ENSURE_SUCCESS(rv, rv);

  *aFoundCookie = cookie != nullptr;

  return NS_OK;
}

NS_IMETHODIMP_(nsresult)
CookieService::GetCookieNative(const nsACString& aHost, const nsACString& aPath,
                               const nsACString& aName,
                               OriginAttributes* aOriginAttributes,
                               nsICookie** aCookie) {
  NS_ENSURE_ARG_POINTER(aOriginAttributes);
  NS_ENSURE_ARG_POINTER(aCookie);

  if (!IsInitialized()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsAutoCString baseDomain;
  nsresult rv =
      CookieCommons::GetBaseDomainFromHost(mTLDService, aHost, baseDomain);
  NS_ENSURE_SUCCESS(rv, rv);

  CookieListIter iter{};
  CookieStorage* storage = PickStorage(*aOriginAttributes);
  bool foundCookie = storage->FindCookie(baseDomain, *aOriginAttributes, aHost,
                                         aName, aPath, iter);

  if (foundCookie) {
    RefPtr<Cookie> cookie = iter.Cookie();
    NS_ENSURE_TRUE(cookie, NS_ERROR_NULL_POINTER);

    cookie.forget(aCookie);
  }

  return NS_OK;
}

// count the number of cookies stored by a particular host. this is provided by
// the nsICookieManager interface.
NS_IMETHODIMP
CookieService::CountCookiesFromHost(const nsACString& aHost,
                                    uint32_t* aCountFromHost) {
  // first, normalize the hostname, and fail if it contains illegal characters.
  nsAutoCString host(aHost);
  nsresult rv = NormalizeHost(host);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString baseDomain;
  rv = CookieCommons::GetBaseDomainFromHost(mTLDService, host, baseDomain);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!IsInitialized()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mPersistentStorage->EnsureInitialized();

  *aCountFromHost = mPersistentStorage->CountCookiesFromHost(baseDomain, 0);

  return NS_OK;
}

// get an enumerator of cookies stored by a particular host. this is provided by
// the nsICookieManager interface.
NS_IMETHODIMP
CookieService::GetCookiesFromHost(const nsACString& aHost,
                                  JS::Handle<JS::Value> aOriginAttributes,
                                  JSContext* aCx,
                                  nsTArray<RefPtr<nsICookie>>& aResult) {
  // first, normalize the hostname, and fail if it contains illegal characters.
  nsAutoCString host(aHost);
  nsresult rv = NormalizeHost(host);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString baseDomain;
  rv = CookieCommons::GetBaseDomainFromHost(mTLDService, host, baseDomain);
  NS_ENSURE_SUCCESS(rv, rv);

  OriginAttributes attrs;
  if (!aOriginAttributes.isObject() || !attrs.Init(aCx, aOriginAttributes)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (!IsInitialized()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  CookieStorage* storage = PickStorage(attrs);

  nsTArray<RefPtr<Cookie>> cookies;
  storage->GetCookiesFromHost(baseDomain, attrs, cookies);

  if (cookies.IsEmpty()) {
    return NS_OK;
  }

  aResult.SetCapacity(cookies.Length());
  for (Cookie* cookie : cookies) {
    aResult.AppendElement(cookie);
  }

  return NS_OK;
}

NS_IMETHODIMP
CookieService::GetCookiesWithOriginAttributes(
    const nsAString& aPattern, const nsACString& aHost,
    nsTArray<RefPtr<nsICookie>>& aResult) {
  OriginAttributesPattern pattern;
  if (!pattern.Init(aPattern)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsAutoCString host(aHost);
  nsresult rv = NormalizeHost(host);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString baseDomain;
  rv = CookieCommons::GetBaseDomainFromHost(mTLDService, host, baseDomain);
  NS_ENSURE_SUCCESS(rv, rv);

  return GetCookiesWithOriginAttributes(pattern, baseDomain, aResult);
}

nsresult CookieService::GetCookiesWithOriginAttributes(
    const OriginAttributesPattern& aPattern, const nsCString& aBaseDomain,
    nsTArray<RefPtr<nsICookie>>& aResult) {
  CookieStorage* storage = PickStorage(aPattern);
  storage->GetCookiesWithOriginAttributes(aPattern, aBaseDomain, aResult);

  return NS_OK;
}

NS_IMETHODIMP
CookieService::RemoveCookiesWithOriginAttributes(const nsAString& aPattern,
                                                 const nsACString& aHost) {
  MOZ_ASSERT(XRE_IsParentProcess());

  OriginAttributesPattern pattern;
  if (!pattern.Init(aPattern)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsAutoCString host(aHost);
  nsresult rv = NormalizeHost(host);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString baseDomain;
  rv = CookieCommons::GetBaseDomainFromHost(mTLDService, host, baseDomain);
  NS_ENSURE_SUCCESS(rv, rv);

  return RemoveCookiesWithOriginAttributes(pattern, baseDomain);
}

nsresult CookieService::RemoveCookiesWithOriginAttributes(
    const OriginAttributesPattern& aPattern, const nsCString& aBaseDomain) {
  if (!IsInitialized()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  CookieStorage* storage = PickStorage(aPattern);
  storage->RemoveCookiesWithOriginAttributes(aPattern, aBaseDomain);

  return NS_OK;
}

NS_IMETHODIMP
CookieService::RemoveCookiesFromExactHost(const nsACString& aHost,
                                          const nsAString& aPattern) {
  MOZ_ASSERT(XRE_IsParentProcess());

  OriginAttributesPattern pattern;
  if (!pattern.Init(aPattern)) {
    return NS_ERROR_INVALID_ARG;
  }

  return RemoveCookiesFromExactHost(aHost, pattern);
}

nsresult CookieService::RemoveCookiesFromExactHost(
    const nsACString& aHost, const OriginAttributesPattern& aPattern) {
  nsAutoCString host(aHost);
  nsresult rv = NormalizeHost(host);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString baseDomain;
  rv = CookieCommons::GetBaseDomainFromHost(mTLDService, host, baseDomain);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!IsInitialized()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  CookieStorage* storage = PickStorage(aPattern);
  storage->RemoveCookiesFromExactHost(aHost, baseDomain, aPattern);

  return NS_OK;
}

namespace {

class RemoveAllSinceRunnable : public Runnable {
 public:
  using CookieArray = nsTArray<RefPtr<nsICookie>>;
  RemoveAllSinceRunnable(Promise* aPromise, CookieService* aSelf,
                         CookieArray&& aCookieArray, int64_t aSinceWhen)
      : Runnable("RemoveAllSinceRunnable"),
        mPromise(aPromise),
        mSelf(aSelf),
        mList(std::move(aCookieArray)),
        mIndex(0),
        mSinceWhen(aSinceWhen) {}

  NS_IMETHODIMP Run() override {
    RemoveSome();

    if (mIndex < mList.Length()) {
      return NS_DispatchToCurrentThread(this);
    }
    mPromise->MaybeResolveWithUndefined();

    return NS_OK;
  }

 private:
  void RemoveSome() {
    for (CookieArray::size_type iter = 0;
         iter < kYieldPeriod && mIndex < mList.Length(); ++mIndex, ++iter) {
      auto* cookie = static_cast<Cookie*>(mList[mIndex].get());
      if (cookie->CreationTime() > mSinceWhen &&
          NS_FAILED(mSelf->Remove(cookie->Host(), cookie->OriginAttributesRef(),
                                  cookie->Name(), cookie->Path()))) {
        continue;
      }
    }
  }

 private:
  RefPtr<Promise> mPromise;
  RefPtr<CookieService> mSelf;
  CookieArray mList;
  CookieArray::size_type mIndex;
  int64_t mSinceWhen;
  static const CookieArray::size_type kYieldPeriod = 10;
};

}  // namespace

NS_IMETHODIMP
CookieService::RemoveAllSince(int64_t aSinceWhen, JSContext* aCx,
                              Promise** aRetVal) {
  nsIGlobalObject* globalObject = xpc::CurrentNativeGlobal(aCx);
  if (NS_WARN_IF(!globalObject)) {
    return NS_ERROR_UNEXPECTED;
  }

  ErrorResult result;
  RefPtr<Promise> promise = Promise::Create(globalObject, result);
  if (NS_WARN_IF(result.Failed())) {
    return result.StealNSResult();
  }

  mPersistentStorage->EnsureInitialized();

  nsTArray<RefPtr<nsICookie>> cookieList;

  // We delete only non-private cookies.
  mPersistentStorage->GetAll(cookieList);

  RefPtr<RemoveAllSinceRunnable> runMe = new RemoveAllSinceRunnable(
      promise, this, std::move(cookieList), aSinceWhen);

  promise.forget(aRetVal);

  return runMe->Run();
}

namespace {

class CompareCookiesCreationTime {
 public:
  static bool Equals(const nsICookie* aCookie1, const nsICookie* aCookie2) {
    return static_cast<const Cookie*>(aCookie1)->CreationTime() ==
           static_cast<const Cookie*>(aCookie2)->CreationTime();
  }

  static bool LessThan(const nsICookie* aCookie1, const nsICookie* aCookie2) {
    return static_cast<const Cookie*>(aCookie1)->CreationTime() <
           static_cast<const Cookie*>(aCookie2)->CreationTime();
  }
};

}  // namespace

NS_IMETHODIMP
CookieService::GetCookiesSince(int64_t aSinceWhen,
                               nsTArray<RefPtr<nsICookie>>& aResult) {
  if (!IsInitialized()) {
    return NS_OK;
  }

  mPersistentStorage->EnsureInitialized();

  // We expose only non-private cookies.
  nsTArray<RefPtr<nsICookie>> cookieList;
  mPersistentStorage->GetAll(cookieList);

  for (RefPtr<nsICookie>& cookie : cookieList) {
    if (static_cast<Cookie*>(cookie.get())->CreationTime() >= aSinceWhen) {
      aResult.AppendElement(cookie);
    }
  }

  aResult.Sort(CompareCookiesCreationTime());
  return NS_OK;
}

size_t CookieService::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
  size_t n = aMallocSizeOf(this);

  if (mPersistentStorage) {
    n += mPersistentStorage->SizeOfIncludingThis(aMallocSizeOf);
  }
  if (mPrivateStorage) {
    n += mPrivateStorage->SizeOfIncludingThis(aMallocSizeOf);
  }

  return n;
}

MOZ_DEFINE_MALLOC_SIZE_OF(CookieServiceMallocSizeOf)

NS_IMETHODIMP
CookieService::CollectReports(nsIHandleReportCallback* aHandleReport,
                              nsISupports* aData, bool /*aAnonymize*/) {
  MOZ_COLLECT_REPORT("explicit/cookie-service", KIND_HEAP, UNITS_BYTES,
                     SizeOfIncludingThis(CookieServiceMallocSizeOf),
                     "Memory used by the cookie service.");

  return NS_OK;
}

bool CookieService::IsInitialized() const {
  if (!mPersistentStorage) {
    NS_WARNING("No CookieStorage! Profile already close?");
    return false;
  }

  MOZ_ASSERT(mPrivateStorage);
  return true;
}

CookieStorage* CookieService::PickStorage(const OriginAttributes& aAttrs) {
  MOZ_ASSERT(IsInitialized());

  if (aAttrs.mPrivateBrowsingId > 0) {
    return mPrivateStorage;
  }

  mPersistentStorage->EnsureInitialized();
  return mPersistentStorage;
}

CookieStorage* CookieService::PickStorage(
    const OriginAttributesPattern& aAttrs) {
  MOZ_ASSERT(IsInitialized());

  if (aAttrs.mPrivateBrowsingId.WasPassed() &&
      aAttrs.mPrivateBrowsingId.Value() > 0) {
    return mPrivateStorage;
  }

  mPersistentStorage->EnsureInitialized();
  return mPersistentStorage;
}

bool CookieService::SetCookiesFromIPC(const nsACString& aBaseDomain,
                                      const OriginAttributes& aAttrs,
                                      nsIURI* aHostURI, bool aFromHttp,
                                      const nsTArray<CookieStruct>& aCookies,
                                      BrowsingContext* aBrowsingContext) {
  if (!IsInitialized()) {
    // If we are probably shutting down, we can ignore this cookie.
    return true;
  }

  CookieStorage* storage = PickStorage(aAttrs);
  int64_t currentTimeInUsec = PR_Now();

  for (const CookieStruct& cookieData : aCookies) {
    if (!CookieCommons::CheckPathSize(cookieData)) {
      return false;
    }

    // reject cookie if it's over the size limit, per RFC2109
    if (!CookieCommons::CheckNameAndValueSize(cookieData)) {
      return false;
    }

    RecordUnicodeTelemetry(cookieData);

    if (!CookieCommons::CheckName(cookieData)) {
      return false;
    }

    if (!CookieCommons::CheckValue(cookieData)) {
      return false;
    }

    // create a new Cookie and copy attributes
    RefPtr<Cookie> cookie = Cookie::Create(cookieData, aAttrs);
    if (!cookie) {
      continue;
    }

    cookie->SetLastAccessed(currentTimeInUsec);
    cookie->SetCreationTime(
        Cookie::GenerateUniqueCreationTime(currentTimeInUsec));

    storage->AddCookie(nullptr, aBaseDomain, aAttrs, cookie, currentTimeInUsec,
                       aHostURI, ""_ns, aFromHttp, aBrowsingContext);
  }

  return true;
}

}  // namespace net
}  // namespace mozilla
