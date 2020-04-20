/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CookieCommons.h"
#include "CookieLogging.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/nsMixedContentBlocker.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/net/CookieJarSettings.h"
#include "mozilla/net/CookiePermission.h"
#include "mozilla/net/CookiePersistentStorage.h"
#include "mozilla/net/CookiePrivateStorage.h"
#include "mozilla/net/CookieService.h"
#include "mozilla/net/CookieServiceChild.h"
#include "mozilla/net/HttpBaseChannel.h"
#include "mozilla/net/NeckoCommon.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/Telemetry.h"
#include "mozIThirdPartyUtil.h"
#include "nsIEffectiveTLDService.h"
#include "nsIIDNService.h"
#include "nsIScriptError.h"
#include "nsIURL.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "prprf.h"

using namespace mozilla::dom;

namespace mozilla {
namespace net {

/******************************************************************************
 * CookieService impl:
 * useful types & constants
 ******************************************************************************/

static StaticRefPtr<CookieService> gCookieService;

constexpr auto CONSOLE_SAMESITE_CATEGORY = NS_LITERAL_CSTRING("cookieSameSite");
constexpr auto CONSOLE_OVERSIZE_CATEGORY =
    NS_LITERAL_CSTRING("cookiesOversize");

constexpr auto SAMESITE_MDN_URL = NS_LITERAL_STRING(
    "https://developer.mozilla.org/docs/Web/HTTP/Headers/Set-Cookie/SameSite");

namespace {

void ComposeCookieString(nsTArray<Cookie*>& aCookieList,
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
        aCookieString +=
            cookie->Name() + NS_LITERAL_CSTRING("=") + cookie->Value();
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
                                            bool aLaxByDefault) {
  int32_t sameSiteAttr = 0;
  aCookie->GetSameSite(&sameSiteAttr);

  // it if's a cross origin request and the cookie is same site only (strict)
  // don't send it
  if (sameSiteAttr == nsICookie::SAMESITE_STRICT) {
    return false;
  }

  int64_t currentTimeInUsec = PR_Now();

  // 2 minutes of tolerance for 'sameSite=lax by default' for cookies set
  // without a sameSite value when used for unsafe http methods.
  if (StaticPrefs::network_cookie_sameSite_laxPlusPOST_timeout() > 0 &&
      aLaxByDefault && sameSiteAttr == nsICookie::SAMESITE_LAX &&
      aCookie->RawSameSite() == nsICookie::SAMESITE_NONE &&
      currentTimeInUsec - aCookie->CreationTime() <=
          (StaticPrefs::network_cookie_sameSite_laxPlusPOST_timeout() *
           PR_USEC_PER_SEC) &&
      !NS_IsSafeMethodNav(aChannel)) {
    return true;
  }

  // if it's a cross origin request, the cookie is same site lax, but it's not a
  // top-level navigation, don't send it
  return sameSiteAttr != nsICookie::SAMESITE_LAX || aIsSafeTopLevelNav;
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
  mTLDService = do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  mIDNService = do_GetService(NS_IDNSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  mThirdPartyUtil = do_GetService(THIRDPARTYUTIL_CONTRACTID);
  NS_ENSURE_SUCCESS(rv, rv);

  // Init our default, and possibly private CookieStorages.
  InitCookieStorages();

  RegisterWeakMemoryReporter(this);

  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  NS_ENSURE_STATE(os);
  os->AddObserver(this, "profile-before-change", true);
  os->AddObserver(this, "profile-do-change", true);
  os->AddObserver(this, "last-pb-context-exited", true);

  mPermissionService = CookiePermission::GetOrCreate();

  return NS_OK;
}

void CookieService::InitCookieStorages() {
  NS_ASSERTION(!mPersistentStorage, "already have a default CookieStorage");
  NS_ASSERTION(!mPrivateStorage, "already have a private CookieStorage");

  // Create two new CookieStorages.
  mPersistentStorage = CookiePersistentStorage::Create();
  mPrivateStorage = CookiePrivateStorage::Create();

  mPersistentStorage->Activate();
}

void CookieService::CloseCookieStorages() {
  // return if we already closed
  if (!mPersistentStorage) {
    return;
  }

  // Let's nullify both storages before calling Close().
  RefPtr<CookiePrivateStorage> privateStorage;
  privateStorage.swap(mPrivateStorage);

  RefPtr<CookiePersistentStorage> persistentStorage;
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
    RemoveCookiesWithOriginAttributes(pattern, EmptyCString());
    mPrivateStorage = CookiePrivateStorage::Create();
  }

  return NS_OK;
}

NS_IMETHODIMP
CookieService::GetCookieStringForPrincipal(nsIPrincipal* aPrincipal,
                                           nsACString& aCookie) {
  NS_ENSURE_ARG(aPrincipal);

  nsresult rv;

  aCookie.Truncate();

  if (!IsInitialized()) {
    return NS_OK;
  }

  CookieStorage* storage = PickStorage(aPrincipal->OriginAttributesRef());

  nsAutoCString baseDomain;
  // for historical reasons we use ascii host for file:// URLs.
  if (aPrincipal->SchemeIs("file")) {
    rv = aPrincipal->GetAsciiHost(baseDomain);
  } else {
    rv = aPrincipal->GetBaseDomain(baseDomain);
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_OK;
  }

  nsAutoCString hostFromURI;
  rv = aPrincipal->GetAsciiHost(hostFromURI);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_OK;
  }

  nsAutoCString pathFromURI;
  rv = aPrincipal->GetFilePath(pathFromURI);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_OK;
  }

  int64_t currentTimeInUsec = PR_Now();
  int64_t currentTime = currentTimeInUsec / PR_USEC_PER_SEC;

  const nsTArray<RefPtr<Cookie>>* cookies = storage->GetCookiesFromHost(
      baseDomain, aPrincipal->OriginAttributesRef());
  if (!cookies) {
    return NS_OK;
  }

  // check if aPrincipal is using an https secure protocol.
  // if it isn't, then we can't send a secure cookie over the connection.
  bool potentiallyTurstworthy = aPrincipal->GetIsOriginPotentiallyTrustworthy();

  bool stale = false;
  nsTArray<Cookie*> cookieList;

  // iterate the cookies!
  for (Cookie* cookie : *cookies) {
    // check the host, since the base domain lookup is conservative.
    if (!CookieCommons::DomainMatches(cookie, hostFromURI)) {
      continue;
    }

    // if the cookie is httpOnly and it's not going directly to the HTTP
    // connection, don't send it
    if (cookie->IsHttpOnly()) {
      continue;
    }

    // if the cookie is secure and the host scheme isn't, we can't send it
    if (cookie->IsSecure() && !potentiallyTurstworthy) {
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
    return NS_OK;
  }

  // update lastAccessed timestamps. we only do this if the timestamp is stale
  // by a certain amount, to avoid thrashing the db during pageload.
  if (stale) {
    storage->StaleCookies(cookieList, currentTimeInUsec);
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

  aCookieString.Truncate();

  uint32_t rejectedReason = 0;
  ThirdPartyAnalysisResult result = mThirdPartyUtil->AnalyzeChannel(
      aChannel, false, aHostURI, nullptr, &rejectedReason);

  OriginAttributes attrs;
  if (aChannel) {
    NS_GetOriginAttributes(aChannel, attrs,
                           true /* considering storage principal */);
  }

  bool isSafeTopLevelNav = NS_IsSafeTopLevelNav(aChannel);
  bool isSameSiteForeign = NS_IsSameSiteForeign(aChannel, aHostURI);

  AutoTArray<Cookie*, 8> foundCookieList;
  GetCookiesForURI(
      aHostURI, aChannel, result.contains(ThirdPartyAnalysis::IsForeign),
      result.contains(ThirdPartyAnalysis::IsThirdPartyTrackingResource),
      result.contains(ThirdPartyAnalysis::IsThirdPartySocialTrackingResource),
      result.contains(ThirdPartyAnalysis::IsFirstPartyStorageAccessGranted),
      rejectedReason, isSafeTopLevelNav, isSameSiteForeign, true, attrs,
      foundCookieList);

  ComposeCookieString(foundCookieList, aCookieString);

  if (!aCookieString.IsEmpty()) {
    COOKIE_LOGSUCCESS(GET_COOKIE, aHostURI, aCookieString, nullptr, false);
  }
  return NS_OK;
}

// static
already_AddRefed<nsICookieJarSettings> CookieService::GetCookieJarSettings(
    nsIChannel* aChannel) {
  nsCOMPtr<nsICookieJarSettings> cookieJarSettings;
  if (aChannel) {
    nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
    nsresult rv =
        loadInfo->GetCookieJarSettings(getter_AddRefs(cookieJarSettings));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      cookieJarSettings = CookieJarSettings::GetBlockingAll();
    }
  } else {
    cookieJarSettings = CookieJarSettings::Create();
  }

  MOZ_ASSERT(cookieJarSettings);
  return cookieJarSettings.forget();
}

NS_IMETHODIMP
CookieService::SetCookieString(nsIURI* aHostURI,
                               const nsACString& aCookieHeader,
                               nsIChannel* aChannel) {
  return SetCookieStringCommon(aHostURI, aCookieHeader, aChannel, false);
}

NS_IMETHODIMP
CookieService::SetCookieStringFromHttp(nsIURI* aHostURI,
                                       const nsACString& aCookieHeader,
                                       nsIChannel* aChannel) {
  return SetCookieStringCommon(aHostURI, aCookieHeader, aChannel, true);
}

nsresult CookieService::SetCookieStringCommon(nsIURI* aHostURI,
                                              const nsACString& aCookieHeader,
                                              nsIChannel* aChannel,
                                              bool aFromHttp) {
  NS_ENSURE_ARG(aHostURI);

  uint32_t rejectedReason = 0;
  ThirdPartyAnalysisResult result = mThirdPartyUtil->AnalyzeChannel(
      aChannel, false, aHostURI, nullptr, &rejectedReason);

  OriginAttributes attrs;
  if (aChannel) {
    NS_GetOriginAttributes(aChannel, attrs,
                           true /* considering storage principal */);
  }

  nsCString cookieString(aCookieHeader);
  SetCookieStringInternal(
      aHostURI, result.contains(ThirdPartyAnalysis::IsForeign),
      result.contains(ThirdPartyAnalysis::IsThirdPartyTrackingResource),
      result.contains(ThirdPartyAnalysis::IsThirdPartySocialTrackingResource),
      result.contains(ThirdPartyAnalysis::IsFirstPartyStorageAccessGranted),
      rejectedReason, cookieString, aFromHttp, attrs, aChannel);
  return NS_OK;
}

void CookieService::SetCookieStringInternal(
    nsIURI* aHostURI, bool aIsForeign, bool aIsThirdPartyTrackingResource,
    bool aIsThirdPartySocialTrackingResource,
    bool aFirstPartyStorageAccessGranted, uint32_t aRejectedReason,
    nsCString& aCookieHeader, bool aFromHttp,
    const OriginAttributes& aOriginAttrs, nsIChannel* aChannel) {
  NS_ASSERTION(aHostURI, "null host!");

  if (!IsInitialized()) {
    return;
  }

  CookieStorage* storage = PickStorage(aOriginAttrs);

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
    return;
  }

  nsCOMPtr<nsICookieJarSettings> cookieJarSettings =
      GetCookieJarSettings(aChannel);

  nsAutoCString hostFromURI;
  aHostURI->GetHost(hostFromURI);
  rv = NormalizeHost(hostFromURI);
  NS_ENSURE_SUCCESS_VOID(rv);

  nsAutoCString baseDomainFromURI;
  rv = CookieCommons::GetBaseDomainFromHost(mTLDService, hostFromURI,
                                            baseDomainFromURI);
  NS_ENSURE_SUCCESS_VOID(rv);

  // check default prefs
  uint32_t priorCookieCount = storage->CountCookiesFromHost(
      baseDomainFromURI, aOriginAttrs.mPrivateBrowsingId);
  uint32_t rejectedReason = aRejectedReason;

  CookieStatus cookieStatus = CheckPrefs(
      cookieJarSettings, aHostURI, aIsForeign, aIsThirdPartyTrackingResource,
      aIsThirdPartySocialTrackingResource, aFirstPartyStorageAccessGranted,
      aCookieHeader, priorCookieCount, aOriginAttrs, &rejectedReason);

  MOZ_ASSERT_IF(rejectedReason, cookieStatus == STATUS_REJECTED);

  // fire a notification if third party or if cookie was rejected
  // (but not if there was an error)
  switch (cookieStatus) {
    case STATUS_REJECTED:
      CookieCommons::NotifyRejected(aHostURI, aChannel, rejectedReason,
                                    OPERATION_WRITE);
      return;  // Stop here
    case STATUS_REJECTED_WITH_ERROR:
      CookieCommons::NotifyRejected(aHostURI, aChannel, rejectedReason,
                                    OPERATION_WRITE);
      return;
    case STATUS_ACCEPTED:  // Fallthrough
    case STATUS_ACCEPT_SESSION:
      NotifyAccepted(aChannel);
      break;
    default:
      break;
  }

  // process each cookie in the header
  while (SetCookieInternal(storage, aHostURI, baseDomain, aOriginAttrs,
                           requireHostMatch, cookieStatus, aCookieHeader,
                           aFromHttp, aChannel)) {
    // document.cookie can only set one cookie at a time
    if (!aFromHttp) {
      break;
    }
  }
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

  mPersistentStorage->EnsureReadComplete();
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

  mPersistentStorage->EnsureReadComplete();
  mPersistentStorage->RemoveAll();
  return NS_OK;
}

NS_IMETHODIMP
CookieService::GetCookies(nsTArray<RefPtr<nsICookie>>& aCookies) {
  if (!IsInitialized()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mPersistentStorage->EnsureReadComplete();

  // We expose only non-private cookies.
  mPersistentStorage->GetCookies(aCookies);

  return NS_OK;
}

NS_IMETHODIMP
CookieService::GetSessionCookies(nsTArray<RefPtr<nsICookie>>& aCookies) {
  if (!IsInitialized()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mPersistentStorage->EnsureReadComplete();

  // We expose only non-private cookies.
  mPersistentStorage->GetCookies(aCookies);

  return NS_OK;
}

NS_IMETHODIMP
CookieService::Add(const nsACString& aHost, const nsACString& aPath,
                   const nsACString& aName, const nsACString& aValue,
                   bool aIsSecure, bool aIsHttpOnly, bool aIsSession,
                   int64_t aExpiry, JS::HandleValue aOriginAttributes,
                   int32_t aSameSite, JSContext* aCx) {
  OriginAttributes attrs;

  if (!aOriginAttributes.isObject() || !attrs.Init(aCx, aOriginAttributes)) {
    return NS_ERROR_INVALID_ARG;
  }

  return AddNative(aHost, aPath, aName, aValue, aIsSecure, aIsHttpOnly,
                   aIsSession, aExpiry, &attrs, aSameSite);
}

NS_IMETHODIMP_(nsresult)
CookieService::AddNative(const nsACString& aHost, const nsACString& aPath,
                         const nsACString& aName, const nsACString& aValue,
                         bool aIsSecure, bool aIsHttpOnly, bool aIsSession,
                         int64_t aExpiry, OriginAttributes* aOriginAttributes,
                         int32_t aSameSite) {
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

  RefPtr<Cookie> cookie = Cookie::Create(
      aName, aValue, host, aPath, aExpiry, currentTimeInUsec,
      Cookie::GenerateUniqueCreationTime(currentTimeInUsec), aIsSession,
      aIsSecure, aIsHttpOnly, key.mOriginAttributes, aSameSite, aSameSite);
  if (!cookie) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  CookieStorage* storage = PickStorage(*aOriginAttributes);
  storage->AddCookie(baseDomain, *aOriginAttributes, cookie, currentTimeInUsec,
                     nullptr, VoidCString(), true);
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
                      JS::HandleValue aOriginAttributes, JSContext* aCx) {
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

NS_IMETHODIMP
CookieService::ImportCookies(nsIFile* aCookieFile) {
  if (!IsInitialized()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mPersistentStorage->EnsureReadComplete();

  return mPersistentStorage->ImportCookies(aCookieFile);
}

void CookieService::GetCookiesForURI(
    nsIURI* aHostURI, nsIChannel* aChannel, bool aIsForeign,
    bool aIsThirdPartyTrackingResource,
    bool aIsThirdPartySocialTrackingResource,
    bool aFirstPartyStorageAccessGranted, uint32_t aRejectedReason,
    bool aIsSafeTopLevelNav, bool aIsSameSiteForeign, bool aHttpBound,
    const OriginAttributes& aOriginAttrs, nsTArray<Cookie*>& aCookieList) {
  NS_ASSERTION(aHostURI, "null host!");

  if (!IsInitialized()) {
    return;
  }

  CookieStorage* storage = PickStorage(aOriginAttrs);

  // get the base domain, host, and path from the URI.
  // e.g. for "www.bbc.co.uk", the base domain would be "bbc.co.uk".
  // file:// URI's (i.e. with an empty host) are allowed, but any other
  // scheme must have a non-empty host. A trailing dot in the host
  // is acceptable.
  bool requireHostMatch;
  nsAutoCString baseDomain;
  nsAutoCString hostFromURI;
  nsAutoCString pathFromURI;
  nsresult rv = CookieCommons::GetBaseDomain(mTLDService, aHostURI, baseDomain,
                                             requireHostMatch);
  if (NS_SUCCEEDED(rv)) {
    rv = aHostURI->GetAsciiHost(hostFromURI);
  }
  if (NS_SUCCEEDED(rv)) {
    rv = aHostURI->GetFilePath(pathFromURI);
  }
  if (NS_FAILED(rv)) {
    COOKIE_LOGFAILURE(GET_COOKIE, aHostURI, VoidCString(),
                      "invalid host/path from URI");
    return;
  }

  nsCOMPtr<nsICookieJarSettings> cookieJarSettings =
      GetCookieJarSettings(aChannel);

  nsAutoCString normalizedHostFromURI(hostFromURI);
  rv = NormalizeHost(normalizedHostFromURI);
  NS_ENSURE_SUCCESS_VOID(rv);

  nsAutoCString baseDomainFromURI;
  rv = CookieCommons::GetBaseDomainFromHost(mTLDService, normalizedHostFromURI,
                                            baseDomainFromURI);
  NS_ENSURE_SUCCESS_VOID(rv);

  // check default prefs
  uint32_t rejectedReason = aRejectedReason;
  uint32_t priorCookieCount = storage->CountCookiesFromHost(
      baseDomainFromURI, aOriginAttrs.mPrivateBrowsingId);

  CookieStatus cookieStatus = CheckPrefs(
      cookieJarSettings, aHostURI, aIsForeign, aIsThirdPartyTrackingResource,
      aIsThirdPartySocialTrackingResource, aFirstPartyStorageAccessGranted,
      VoidCString(), priorCookieCount, aOriginAttrs, &rejectedReason);

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
  bool potentiallyTurstworthy =
      nsMixedContentBlocker::IsPotentiallyTrustworthyOrigin(aHostURI);

  int64_t currentTimeInUsec = PR_Now();
  int64_t currentTime = currentTimeInUsec / PR_USEC_PER_SEC;
  bool stale = false;

  const nsTArray<RefPtr<Cookie>>* cookies =
      storage->GetCookiesFromHost(baseDomain, aOriginAttrs);
  if (!cookies) {
    return;
  }

  bool laxByDefault =
      StaticPrefs::network_cookie_sameSite_laxByDefault() &&
      !nsContentUtils::IsURIInPrefList(
          aHostURI, "network.cookie.sameSite.laxByDefault.disabledHosts");

  // iterate the cookies!
  for (Cookie* cookie : *cookies) {
    // check the host, since the base domain lookup is conservative.
    if (!CookieCommons::DomainMatches(cookie, hostFromURI)) {
      continue;
    }

    // if the cookie is secure and the host scheme isn't, we can't send it
    if (cookie->IsSecure() && !potentiallyTurstworthy) {
      continue;
    }

    if (aHttpBound && aIsSameSiteForeign &&
        !ProcessSameSiteCookieForForeignRequest(
            aChannel, cookie, aIsSafeTopLevelNav, laxByDefault)) {
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

    // all checks passed - add to list and check if lastAccessed stamp needs
    // updating
    aCookieList.AppendElement(cookie);
    if (cookie->IsStale()) {
      stale = true;
    }
  }

  if (aCookieList.IsEmpty()) {
    return;
  }

  // Send a notification about the acceptance of the cookies now that we found
  // some.
  NotifyAccepted(aChannel);

  // update lastAccessed timestamps. we only do this if the timestamp is stale
  // by a certain amount, to avoid thrashing the db during pageload.
  if (stale) {
    storage->StaleCookies(aCookieList, currentTimeInUsec);
  }

  // return cookies in order of path length; longest to shortest.
  // this is required per RFC2109.  if cookies match in length,
  // then sort by creation time (see bug 236772).
  aCookieList.Sort(CompareCookiesForSending());
}

// processes a single cookie, and returns true if there are more cookies
// to be processed
bool CookieService::CanSetCookie(nsIURI* aHostURI,
                                 const nsACString& aBaseDomain,
                                 CookieStruct& aCookieData,
                                 bool aRequireHostMatch, CookieStatus aStatus,
                                 nsCString& aCookieHeader, bool aFromHttp,
                                 nsIChannel* aChannel, bool& aSetCookie,
                                 mozIThirdPartyUtil* aThirdPartyUtil) {
  NS_ASSERTION(aHostURI, "null host!");

  aSetCookie = false;

  // init expiryTime such that session cookies won't prematurely expire
  aCookieData.expiry() = INT64_MAX;

  // aCookieHeader is an in/out param to point to the next cookie, if
  // there is one. Save the present value for logging purposes
  nsCString savedCookieHeader(aCookieHeader);

  // newCookie says whether there are multiple cookies in the header;
  // so we can handle them separately.
  nsAutoCString expires;
  nsAutoCString maxage;
  bool acceptedByParser = false;
  bool newCookie =
      ParseAttributes(aChannel, aHostURI, aCookieHeader, aCookieData, expires,
                      maxage, acceptedByParser);
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
  bool potentiallyTurstworthy =
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

    LogMessageToConsole(aChannel, aHostURI, nsIScriptError::warningFlag,
                        CONSOLE_OVERSIZE_CATEGORY,
                        NS_LITERAL_CSTRING("CookieOversize"), params);
    return newCookie;
  }

  if (!CookieCommons::CheckName(aCookieData)) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader,
                      "invalid name character");
    return newCookie;
  }

  // domain & path checks
  if (!CheckDomain(aCookieData, aHostURI, aBaseDomain, aRequireHostMatch)) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader,
                      "failed the domain tests");
    return newCookie;
  }

  if (!CheckPath(aCookieData, aChannel, aHostURI)) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader,
                      "failed the path tests");
    return newCookie;
  }

  // magic prefix checks. MUST be run after CheckDomain() and CheckPath()
  if (!CheckPrefixes(aCookieData, potentiallyTurstworthy)) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader,
                      "failed the prefix tests");
    return newCookie;
  }

  if (aFromHttp && !CookieCommons::CheckHttpValue(aCookieData)) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader,
                      "invalid value character");
    return newCookie;
  }

  // if the new cookie is httponly, make sure we're not coming from script
  if (!aFromHttp && aCookieData.isHttpOnly()) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader,
                      "cookie is httponly; coming from script");
    return newCookie;
  }

  // If the new cookie is non-https and wants to set secure flag,
  // browser have to ignore this new cookie.
  // (draft-ietf-httpbis-cookie-alone section 3.1)
  if (aCookieData.isSecure() && !potentiallyTurstworthy) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, aCookieHeader,
                      "non-https cookie can't set secure flag");
    return newCookie;
  }

  // If the new cookie is same-site but in a cross site context,
  // browser must ignore the cookie.
  if ((aCookieData.sameSite() != nsICookie::SAMESITE_NONE) && aThirdPartyUtil) {
    // Do not treat loads triggered by web extensions as foreign
    bool addonAllowsLoad = false;
    if (aChannel) {
      nsCOMPtr<nsIURI> channelURI;
      NS_GetFinalChannelURI(aChannel, getter_AddRefs(channelURI));
      nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
      addonAllowsLoad = BasePrincipal::Cast(loadInfo->TriggeringPrincipal())
                            ->AddonAllowsLoad(channelURI);
    }

    if (!addonAllowsLoad) {
      bool isThirdParty = false;
      nsresult rv = aThirdPartyUtil->IsThirdPartyChannel(aChannel, aHostURI,
                                                         &isThirdParty);
      if (NS_FAILED(rv) || isThirdParty) {
        COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader,
                          "failed the samesite tests");
        return newCookie;
      }
    }
  }

  aSetCookie = true;
  return newCookie;
}

// processes a single cookie, and returns true if there are more cookies
// to be processed
bool CookieService::SetCookieInternal(CookieStorage* aStorage, nsIURI* aHostURI,
                                      const nsACString& aBaseDomain,
                                      const OriginAttributes& aOriginAttributes,
                                      bool aRequireHostMatch,
                                      CookieStatus aStatus,
                                      nsCString& aCookieHeader, bool aFromHttp,
                                      nsIChannel* aChannel) {
  NS_ASSERTION(aHostURI, "null host!");
  bool canSetCookie = false;
  nsCString savedCookieHeader(aCookieHeader);
  CookieStruct cookieData;
  bool newCookie = CanSetCookie(
      aHostURI, aBaseDomain, cookieData, aRequireHostMatch, aStatus,
      aCookieHeader, aFromHttp, aChannel, canSetCookie, mThirdPartyUtil);

  if (!canSetCookie) {
    return newCookie;
  }

  int64_t currentTimeInUsec = PR_Now();
  // create a new Cookie and copy attributes
  RefPtr<Cookie> cookie = Cookie::Create(
      cookieData.name(), cookieData.value(), cookieData.host(),
      cookieData.path(), cookieData.expiry(), currentTimeInUsec,
      Cookie::GenerateUniqueCreationTime(currentTimeInUsec),
      cookieData.isSession(), cookieData.isSecure(), cookieData.isHttpOnly(),
      aOriginAttributes, cookieData.sameSite(), cookieData.rawSameSite());
  if (!cookie) {
    return newCookie;
  }

  // check permissions from site permission list, or ask the user,
  // to determine if we can set the cookie
  if (mPermissionService) {
    bool permission;
    mPermissionService->CanSetCookie(
        aHostURI, aChannel,
        static_cast<nsICookie*>(static_cast<Cookie*>(cookie)),
        &cookieData.isSession(), &cookieData.expiry(), &permission);
    if (!permission) {
      COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader,
                        "cookie rejected by permission manager");
      CookieCommons::NotifyRejected(
          aHostURI, aChannel,
          nsIWebProgressListener::STATE_COOKIES_BLOCKED_BY_PERMISSION,
          OPERATION_WRITE);
      return newCookie;
    }

    // update isSession and expiry attributes, in case they changed
    cookie->SetIsSession(cookieData.isSession());
    cookie->SetExpiry(cookieData.expiry());
  }

  // add the cookie to the list. AddCookie() takes care of logging.
  // we get the current time again here, since it may have changed during
  // prompting
  aStorage->AddCookie(aBaseDomain, aOriginAttributes, cookie, currentTimeInUsec,
                      aHostURI, savedCookieHeader, aFromHttp);
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
    allowed-chars = <any OCTET except NUL or cookie-sep>
    OCTET         = <any 8-bit sequence of data>
    LWS           = SP | HT
    NUL           = <US-ASCII NUL, null control character (0)>
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
                  | "Secure"
                  | "HttpOnly"

******************************************************************************/
// clang-format on

// helper functions for GetTokenValue
static inline bool isnull(char c) { return c == 0; }
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
  while (aIter != aEndIter && !isnull(*aIter) && !istokenseparator(*aIter)) {
    ++aIter;
  }

  // remove trailing <LWS>; first check we're not at the beginning
  lastSpace = aIter;
  if (lastSpace != start) {
    while (--lastSpace != start && iswhitespace(*lastSpace)) {
      continue;
    }
    ++lastSpace;
  }
  aTokenString.Rebind(start, lastSpace);

  aEqualsFound = (*aIter == '=');
  if (aEqualsFound) {
    // find <value>
    while (++aIter != aEndIter && iswhitespace(*aIter)) {
      continue;
    }

    start = aIter;

    // process <token>
    // just look for ';' to terminate ('=' allowed)
    while (aIter != aEndIter && !isnull(*aIter) && !isvalueseparator(*aIter)) {
      ++aIter;
    }

    // remove trailing <LWS>; first check we're not at the beginning
    if (aIter != start) {
      lastSpace = aIter;
      while (--lastSpace != start && iswhitespace(*lastSpace)) {
        continue;
      }

      aTokenValue.Rebind(start, ++lastSpace);
    }
  }

  // aIter is on ';', or terminator, or EOS
  if (aIter != aEndIter) {
    // if on terminator, increment past & return true to process new cookie
    if (isterminator(*aIter)) {
      ++aIter;
      return true;
    }
    // fall-through: aIter is on ';', increment and return false
    ++aIter;
  }
  return false;
}

// Parses attributes from cookie header. expires/max-age attributes aren't
// folded into the cookie struct here, because we don't know which one to use
// until we've parsed the header.
bool CookieService::ParseAttributes(nsIChannel* aChannel, nsIURI* aHostURI,
                                    nsCString& aCookieHeader,
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

  nsACString::const_char_iterator cookieStart;
  aCookieHeader.BeginReading(cookieStart);

  nsACString::const_char_iterator cookieEnd;
  aCookieHeader.EndReading(cookieEnd);

  aCookieData.isSecure() = false;
  aCookieData.isHttpOnly() = false;
  aCookieData.sameSite() = nsICookie::SAMESITE_NONE;
  aCookieData.rawSameSite() = nsICookie::SAMESITE_NONE;

  bool laxByDefault =
      StaticPrefs::network_cookie_sameSite_laxByDefault() &&
      !nsContentUtils::IsURIInPrefList(
          aHostURI, "network.cookie.sameSite.laxByDefault.disabledHosts");

  if (laxByDefault) {
    aCookieData.sameSite() = nsICookie::SAMESITE_LAX;
  }

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

  bool sameSiteSet = false;

  // extract remaining attributes
  while (cookieStart != cookieEnd && !newCookie) {
    newCookie = GetTokenValue(cookieStart, cookieEnd, tokenString, tokenValue,
                              equalsFound);

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

      // ignore any tokenValue for isHttpOnly (see bug 178993);
      // just set the boolean
    } else if (tokenString.LowerCaseEqualsLiteral(kHttpOnly)) {
      aCookieData.isHttpOnly() = true;

    } else if (tokenString.LowerCaseEqualsLiteral(kSameSite)) {
      if (tokenValue.LowerCaseEqualsLiteral(kSameSiteLax)) {
        aCookieData.sameSite() = nsICookie::SAMESITE_LAX;
        aCookieData.rawSameSite() = nsICookie::SAMESITE_LAX;
        sameSiteSet = true;
      } else if (tokenValue.LowerCaseEqualsLiteral(kSameSiteStrict)) {
        aCookieData.sameSite() = nsICookie::SAMESITE_STRICT;
        aCookieData.rawSameSite() = nsICookie::SAMESITE_STRICT;
        sameSiteSet = true;
      } else if (tokenValue.LowerCaseEqualsLiteral(kSameSiteNone)) {
        aCookieData.sameSite() = nsICookie::SAMESITE_NONE;
        aCookieData.rawSameSite() = nsICookie::SAMESITE_NONE;
        sameSiteSet = true;
      } else {
        LogMessageToConsole(
            aChannel, aHostURI, nsIScriptError::infoFlag,
            CONSOLE_SAMESITE_CATEGORY,
            NS_LITERAL_CSTRING("CookieSameSiteValueInvalid"),
            AutoTArray<nsString, 1>{NS_ConvertUTF8toUTF16(aCookieData.name())});
      }
    }
  }

  Telemetry::Accumulate(Telemetry::COOKIE_SAMESITE_SET_VS_UNSET,
                        sameSiteSet ? 1 : 0);

  // re-assign aCookieHeader, in case we need to process another cookie
  aCookieHeader.Assign(Substring(cookieStart, cookieEnd));

  // If same-site is set to 'none' but this is not a secure context, let's abort
  // the parsing.
  if (!aCookieData.isSecure() &&
      aCookieData.sameSite() == nsICookie::SAMESITE_NONE) {
    if (laxByDefault &&
        StaticPrefs::network_cookie_sameSite_noneRequiresSecure()) {
      LogMessageToConsole(
          aChannel, aHostURI, nsIScriptError::infoFlag,
          CONSOLE_SAMESITE_CATEGORY,
          NS_LITERAL_CSTRING("CookieRejectedNonRequiresSecure"),
          AutoTArray<nsString, 1>{NS_ConvertUTF8toUTF16(aCookieData.name())});
      return newCookie;
    }

    // if sameSite=lax by default is disabled, we want to warn the user.
    LogMessageToConsole(
        aChannel, aHostURI, nsIScriptError::warningFlag,
        CONSOLE_SAMESITE_CATEGORY,
        NS_LITERAL_CSTRING("CookieRejectedNonRequiresSecureForBeta"),
        AutoTArray<nsString, 2>{NS_ConvertUTF8toUTF16(aCookieData.name()),
                                SAMESITE_MDN_URL});
  }

  if (aCookieData.rawSameSite() == nsICookie::SAMESITE_NONE &&
      aCookieData.sameSite() == nsICookie::SAMESITE_LAX) {
    if (laxByDefault) {
      LogMessageToConsole(
          aChannel, aHostURI, nsIScriptError::infoFlag,
          CONSOLE_SAMESITE_CATEGORY, NS_LITERAL_CSTRING("CookieLaxForced"),
          AutoTArray<nsString, 1>{NS_ConvertUTF8toUTF16(aCookieData.name())});
    } else {
      LogMessageToConsole(
          aChannel, aHostURI, nsIScriptError::warningFlag,
          CONSOLE_SAMESITE_CATEGORY,
          NS_LITERAL_CSTRING("CookieLaxForcedForBeta"),
          AutoTArray<nsString, 2>{NS_ConvertUTF8toUTF16(aCookieData.name()),
                                  SAMESITE_MDN_URL});
    }
  }

  // Cookie accepted.
  aAcceptedByParser = true;

  MOZ_ASSERT(Cookie::ValidateRawSame(aCookieData));
  return newCookie;
}

// static
void CookieService::LogMessageToConsole(nsIChannel* aChannel, nsIURI* aURI,
                                        uint32_t aErrorFlags,
                                        const nsACString& aCategory,
                                        const nsACString& aMsg,
                                        const nsTArray<nsString>& aParams) {
  MOZ_ASSERT(aURI);

  nsCOMPtr<HttpBaseChannel> httpChannel = do_QueryInterface(aChannel);
  if (!httpChannel) {
    return;
  }

  nsAutoCString uri;
  nsresult rv = aURI->GetSpec(uri);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  httpChannel->AddConsoleReport(aErrorFlags, aCategory,
                                nsContentUtils::eNECKO_PROPERTIES, uri, 0, 0,
                                aMsg, aParams);
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

CookieStatus CookieService::CheckPrefs(nsICookieJarSettings* aCookieJarSettings,
                                       nsIURI* aHostURI, bool aIsForeign,
                                       bool aIsThirdPartyTrackingResource,
                                       bool aIsThirdPartySocialTrackingResource,
                                       bool aFirstPartyStorageAccessGranted,
                                       const nsACString& aCookieHeader,
                                       const int aNumOfCookies,
                                       const OriginAttributes& aOriginAttrs,
                                       uint32_t* aRejectedReason) {
  nsresult rv;

  MOZ_ASSERT(aRejectedReason);

  *aRejectedReason = 0;

  // don't let ftp sites get/set cookies (could be a security issue)
  if (aHostURI->SchemeIs("ftp")) {
    COOKIE_LOGFAILURE(aCookieHeader.IsVoid() ? GET_COOKIE : SET_COOKIE,
                      aHostURI, aCookieHeader, "ftp sites cannot read cookies");
    return STATUS_REJECTED_WITH_ERROR;
  }

  nsCOMPtr<nsIPrincipal> principal =
      BasePrincipal::CreateContentPrincipal(aHostURI, aOriginAttrs);

  if (!principal) {
    COOKIE_LOGFAILURE(aCookieHeader.IsVoid() ? GET_COOKIE : SET_COOKIE,
                      aHostURI, aCookieHeader,
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
        COOKIE_LOGFAILURE(aCookieHeader.IsVoid() ? GET_COOKIE : SET_COOKIE,
                          aHostURI, aCookieHeader,
                          "cookies are blocked for this site");
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
      !aFirstPartyStorageAccessGranted &&
      aCookieJarSettings->GetRejectThirdPartyContexts()) {
    bool rejectThirdPartyWithExceptions =
        CookieJarSettings::IsRejectThirdPartyWithExceptions(
            aCookieJarSettings->GetCookieBehavior());

    uint32_t rejectReason =
        rejectThirdPartyWithExceptions
            ? nsIWebProgressListener::STATE_COOKIES_BLOCKED_FOREIGN
            : nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER;
    if (StoragePartitioningEnabled(rejectReason, aCookieJarSettings)) {
      MOZ_ASSERT(!aOriginAttrs.mFirstPartyDomain.IsEmpty(),
                 "We must have a StoragePrincipal here!");
      return STATUS_ACCEPTED;
    }

    COOKIE_LOGFAILURE(aCookieHeader.IsVoid() ? GET_COOKIE : SET_COOKIE,
                      aHostURI, aCookieHeader,
                      "cookies are disabled in trackers");
    if (aIsThirdPartySocialTrackingResource) {
      *aRejectedReason =
          nsIWebProgressListener::STATE_COOKIES_BLOCKED_SOCIALTRACKER;
    } else if (rejectThirdPartyWithExceptions) {
      *aRejectedReason = nsIWebProgressListener::STATE_COOKIES_BLOCKED_FOREIGN;
    } else {
      *aRejectedReason = nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER;
    }
    return STATUS_REJECTED;
  }

  // check default prefs.
  // Check aFirstPartyStorageAccessGranted when checking aCookieBehavior
  // so that we take things such as the content blocking allow list into
  // account.
  if (aCookieJarSettings->GetCookieBehavior() ==
          nsICookieService::BEHAVIOR_REJECT &&
      !aFirstPartyStorageAccessGranted) {
    COOKIE_LOGFAILURE(aCookieHeader.IsVoid() ? GET_COOKIE : SET_COOKIE,
                      aHostURI, aCookieHeader, "cookies are disabled");
    *aRejectedReason = nsIWebProgressListener::STATE_COOKIES_BLOCKED_ALL;
    return STATUS_REJECTED;
  }

  // check if cookie is foreign
  if (aIsForeign) {
    if (aCookieJarSettings->GetCookieBehavior() ==
            nsICookieService::BEHAVIOR_REJECT_FOREIGN &&
        !aFirstPartyStorageAccessGranted) {
      COOKIE_LOGFAILURE(aCookieHeader.IsVoid() ? GET_COOKIE : SET_COOKIE,
                        aHostURI, aCookieHeader, "context is third party");
      *aRejectedReason = nsIWebProgressListener::STATE_COOKIES_BLOCKED_FOREIGN;
      return STATUS_REJECTED;
    }

    if (aCookieJarSettings->GetLimitForeignContexts() &&
        !aFirstPartyStorageAccessGranted && aNumOfCookies == 0) {
      COOKIE_LOGFAILURE(aCookieHeader.IsVoid() ? GET_COOKIE : SET_COOKIE,
                        aHostURI, aCookieHeader, "context is third party");
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
  aHostURI->GetAsciiHost(hostFromURI);

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

nsAutoCString CookieService::GetPathFromURI(nsIURI* aHostURI) {
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

bool CookieService::CheckPath(CookieStruct& aCookieData, nsIChannel* aChannel,
                              nsIURI* aHostURI) {
  // if a path is given, check the host has permission
  if (aCookieData.path().IsEmpty() || aCookieData.path().First() != '/') {
    aCookieData.path() = GetPathFromURI(aHostURI);

#if 0
  } else {
    /**
     * The following test is part of the RFC2109 spec.  Loosely speaking, it says that a site
     * cannot set a cookie for a path that it is not on.  See bug 155083.  However this patch
     * broke several sites -- nordea (bug 155768) and citibank (bug 156725).  So this test has
     * been disabled, unless we can evangelize these sites.
     */
    // get path from aHostURI
    nsAutoCString pathFromURI;
    if (NS_FAILED(aHostURI->GetPathQueryRef(pathFromURI)) ||
        !StringBeginsWith(pathFromURI, aCookieData.path())) {
      return false;
    }
#endif
  }

  if (!CookieCommons::CheckPathSize(aCookieData)) {
    AutoTArray<nsString, 2> params = {
        NS_ConvertUTF8toUTF16(aCookieData.name())};

    nsString size;
    size.AppendInt(kMaxBytesPerPath);
    params.AppendElement(size);

    LogMessageToConsole(aChannel, aHostURI, nsIScriptError::warningFlag,
                        CONSOLE_OVERSIZE_CATEGORY,
                        NS_LITERAL_CSTRING("CookiePathOversize"), params);
    return false;
  }

  if (aCookieData.path().Contains('\t')) {
    return false;
  }

  return true;
}

// CheckPrefixes
//
// Reject cookies whose name starts with the magic prefixes from
// https://tools.ietf.org/html/draft-ietf-httpbis-cookie-prefixes-00
// if they do not meet the criteria required by the prefix.
//
// Must not be called until after CheckDomain() and CheckPath() have
// regularized and validated the CookieStruct values!
bool CookieService::CheckPrefixes(CookieStruct& aCookieData,
                                  bool aSecureRequest) {
  static const char kSecure[] = "__Secure-";
  static const char kHost[] = "__Host-";
  static const int kSecureLen = sizeof(kSecure) - 1;
  static const int kHostLen = sizeof(kHost) - 1;

  bool isSecure = strncmp(aCookieData.name().get(), kSecure, kSecureLen) == 0;
  bool isHost = strncmp(aCookieData.name().get(), kHost, kHostLen) == 0;

  if (!isSecure && !isHost) {
    // not one of the magic prefixes: carry on
    return true;
  }

  if (!aSecureRequest || !aCookieData.isSecure()) {
    // the magic prefixes may only be used from a secure request and
    // the secure attribute must be set on the cookie
    return false;
  }

  if (isHost) {
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
                            JS::HandleValue aOriginAttributes, JSContext* aCx,
                            bool* aFoundCookie) {
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
  NS_ENSURE_ARG_POINTER(aOriginAttributes);
  NS_ENSURE_ARG_POINTER(aFoundCookie);

  if (!IsInitialized()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsAutoCString baseDomain;
  nsresult rv =
      CookieCommons::GetBaseDomainFromHost(mTLDService, aHost, baseDomain);
  NS_ENSURE_SUCCESS(rv, rv);

  CookieListIter iter;
  CookieStorage* storage = PickStorage(*aOriginAttributes);
  *aFoundCookie = storage->FindCookie(baseDomain, *aOriginAttributes, aHost,
                                      aName, aPath, iter);
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

  mPersistentStorage->EnsureReadComplete();

  *aCountFromHost = mPersistentStorage->CountCookiesFromHost(baseDomain, 0);

  return NS_OK;
}

// get an enumerator of cookies stored by a particular host. this is provided by
// the nsICookieManager interface.
NS_IMETHODIMP
CookieService::GetCookiesFromHost(const nsACString& aHost,
                                  JS::HandleValue aOriginAttributes,
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

  const nsTArray<RefPtr<Cookie>>* cookies =
      storage->GetCookiesFromHost(baseDomain, attrs);

  if (cookies) {
    aResult.SetCapacity(cookies->Length());
    for (Cookie* cookie : *cookies) {
      aResult.AppendElement(cookie);
    }
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

  mPersistentStorage->EnsureReadComplete();

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
  mPersistentStorage->EnsureReadComplete();

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

  mPersistentStorage->EnsureReadComplete();
  return mPersistentStorage;
}

CookieStorage* CookieService::PickStorage(
    const OriginAttributesPattern& aAttrs) {
  MOZ_ASSERT(IsInitialized());

  if (aAttrs.mPrivateBrowsingId.WasPassed() &&
      aAttrs.mPrivateBrowsingId.Value() > 0) {
    return mPrivateStorage;
  }

  mPersistentStorage->EnsureReadComplete();
  return mPersistentStorage;
}

bool CookieService::SetCookiesFromIPC(const nsACString& aBaseDomain,
                                      const OriginAttributes& aAttrs,
                                      nsIURI* aHostURI, bool aFromHttp,
                                      const nsTArray<CookieStruct>& aCookies) {
  MOZ_ASSERT(IsInitialized());

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

    if (!CookieCommons::CheckName(cookieData)) {
      return false;
    }

    if (aFromHttp && !CookieCommons::CheckHttpValue(cookieData)) {
      return false;
    }

    // create a new Cookie and copy attributes
    RefPtr<Cookie> cookie = Cookie::Create(
        cookieData.name(), cookieData.value(), cookieData.host(),
        cookieData.path(), cookieData.expiry(), currentTimeInUsec,
        Cookie::GenerateUniqueCreationTime(currentTimeInUsec),
        cookieData.isSession(), cookieData.isSecure(), cookieData.isHttpOnly(),
        aAttrs, cookieData.sameSite(), cookieData.rawSameSite());
    if (!cookie) {
      continue;
    }

    storage->AddCookie(aBaseDomain, aAttrs, cookie, currentTimeInUsec, aHostURI,
                       EmptyCString(), aFromHttp);
  }

  return true;
}

}  // namespace net
}  // namespace mozilla
