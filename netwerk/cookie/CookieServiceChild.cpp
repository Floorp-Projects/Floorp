/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/CookieServiceChild.h"
#include "mozilla/net/NeckoChannelParams.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/SystemGroup.h"
#include "mozilla/StoragePrincipalHelper.h"
#include "Cookie.h"
#include "nsCookieService.h"
#include "nsContentUtils.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsIChannel.h"
#include "nsIClassifiedChannel.h"
#include "nsIHttpChannel.h"
#include "nsCookiePermission.h"
#include "nsIEffectiveTLDService.h"
#include "nsIURI.h"
#include "nsIPrefBranch.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"
#include "ThirdPartyUtil.h"

using namespace mozilla::ipc;
using mozilla::OriginAttributes;

namespace mozilla {
namespace net {

// Pref string constants
static const char kCookieMoveIntervalSecs[] =
    "network.cookie.move.interval_sec";

static StaticRefPtr<CookieServiceChild> gCookieService;
static uint32_t gMoveCookiesIntervalSeconds = 10;

already_AddRefed<CookieServiceChild> CookieServiceChild::GetSingleton() {
  if (!gCookieService) {
    gCookieService = new CookieServiceChild();
    ClearOnShutdown(&gCookieService);
  }

  return do_AddRef(gCookieService);
}

NS_IMPL_ISUPPORTS(CookieServiceChild, nsICookieService, nsIObserver,
                  nsITimerCallback, nsISupportsWeakReference)

CookieServiceChild::CookieServiceChild() {
  NS_ASSERTION(IsNeckoChild(), "not a child process");

  mozilla::dom::ContentChild* cc =
      static_cast<mozilla::dom::ContentChild*>(gNeckoChild->Manager());
  if (cc->IsShuttingDown()) {
    return;
  }

  // This corresponds to Release() in DeallocPCookieService.
  NS_ADDREF_THIS();

  NeckoChild::InitNeckoChild();

  // Create a child PCookieService actor.
  gNeckoChild->SendPCookieServiceConstructor(this);

  mThirdPartyUtil = ThirdPartyUtil::GetInstance();
  NS_ASSERTION(mThirdPartyUtil, "couldn't get ThirdPartyUtil service");

  mTLDService = do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID);
  NS_ASSERTION(mTLDService, "couldn't get TLDService");

  // Init our prefs and observer.
  nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID);
  NS_WARNING_ASSERTION(prefBranch, "no prefservice");
  if (prefBranch) {
    prefBranch->AddObserver(kCookieMoveIntervalSecs, this, true);
    PrefChanged(prefBranch);
  }

  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (observerService) {
    observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
  }
}

void CookieServiceChild::MoveCookies() {
  TimeStamp start = TimeStamp::Now();
  for (auto iter = mCookiesMap.Iter(); !iter.Done(); iter.Next()) {
    CookiesList* cookiesList = iter.UserData();
    CookiesList newCookiesList;
    for (uint32_t i = 0; i < cookiesList->Length(); ++i) {
      Cookie* cookie = cookiesList->ElementAt(i);
      RefPtr<Cookie> newCookie = Cookie::Create(
          cookie->Name(), cookie->Value(), cookie->Host(), cookie->Path(),
          cookie->Expiry(), cookie->LastAccessed(), cookie->CreationTime(),
          cookie->IsSession(), cookie->IsSecure(), cookie->IsHttpOnly(),
          cookie->OriginAttributesRef(), cookie->SameSite(),
          cookie->RawSameSite());
      newCookiesList.AppendElement(newCookie);
    }
    cookiesList->SwapElements(newCookiesList);
  }

  Telemetry::AccumulateTimeDelta(Telemetry::COOKIE_TIME_MOVING_MS, start);
}

NS_IMETHODIMP
CookieServiceChild::Notify(nsITimer* aTimer) {
  if (aTimer == mCookieTimer) {
    MoveCookies();
  } else {
    MOZ_CRASH("Unknown timer");
  }
  return NS_OK;
}

CookieServiceChild::~CookieServiceChild() { gCookieService = nullptr; }

void CookieServiceChild::TrackCookieLoad(nsIChannel* aChannel) {
  if (!CanSend()) {
    return;
  }

  uint32_t rejectedReason = 0;
  ThirdPartyAnalysisResult result = mThirdPartyUtil->AnalyzeChannel(
      aChannel, true, nullptr, RequireThirdPartyCheck, &rejectedReason);

  nsCOMPtr<nsIURI> uri;
  aChannel->GetURI(getter_AddRefs(uri));
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();

  mozilla::OriginAttributes attrs = loadInfo->GetOriginAttributes();
  StoragePrincipalHelper::PrepareOriginAttributes(aChannel, attrs);
  URIParams uriParams;
  SerializeURI(uri, uriParams);
  bool isSafeTopLevelNav = NS_IsSafeTopLevelNav(aChannel);
  bool isSameSiteForeign = NS_IsSameSiteForeign(aChannel, uri);
  SendPrepareCookieList(
      uriParams, result.contains(ThirdPartyAnalysis::IsForeign),
      result.contains(ThirdPartyAnalysis::IsThirdPartyTrackingResource),
      result.contains(ThirdPartyAnalysis::IsThirdPartySocialTrackingResource),
      result.contains(ThirdPartyAnalysis::IsFirstPartyStorageAccessGranted),
      rejectedReason, isSafeTopLevelNav, isSameSiteForeign, attrs);
}

mozilla::ipc::IPCResult CookieServiceChild::RecvRemoveAll() {
  mCookiesMap.Clear();
  return IPC_OK();
}

mozilla::ipc::IPCResult CookieServiceChild::RecvRemoveCookie(
    const CookieStruct& aCookie, const OriginAttributes& aAttrs) {
  nsCString baseDomain;
  nsCookieService::GetBaseDomainFromHost(mTLDService, aCookie.host(),
                                         baseDomain);
  nsCookieKey key(baseDomain, aAttrs);
  CookiesList* cookiesList = nullptr;
  mCookiesMap.Get(key, &cookiesList);

  if (!cookiesList) {
    return IPC_OK();
  }

  for (uint32_t i = 0; i < cookiesList->Length(); i++) {
    Cookie* cookie = cookiesList->ElementAt(i);
    if (cookie->Name().Equals(aCookie.name()) &&
        cookie->Host().Equals(aCookie.host()) &&
        cookie->Path().Equals(aCookie.path())) {
      cookiesList->RemoveElementAt(i);
      break;
    }
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult CookieServiceChild::RecvAddCookie(
    const CookieStruct& aCookie, const OriginAttributes& aAttrs) {
  RefPtr<Cookie> cookie = Cookie::Create(
      aCookie.name(), aCookie.value(), aCookie.host(), aCookie.path(),
      aCookie.expiry(), aCookie.lastAccessed(), aCookie.creationTime(),
      aCookie.isSession(), aCookie.isSecure(), aCookie.isHttpOnly(), aAttrs,
      aCookie.sameSite(), aCookie.rawSameSite());
  RecordDocumentCookie(cookie, aAttrs);
  return IPC_OK();
}

mozilla::ipc::IPCResult CookieServiceChild::RecvRemoveBatchDeletedCookies(
    nsTArray<CookieStruct>&& aCookiesList,
    nsTArray<OriginAttributes>&& aAttrsList) {
  MOZ_ASSERT(aCookiesList.Length() == aAttrsList.Length());
  for (uint32_t i = 0; i < aCookiesList.Length(); i++) {
    CookieStruct cookieStruct = aCookiesList.ElementAt(i);
    RecvRemoveCookie(cookieStruct, aAttrsList.ElementAt(i));
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult CookieServiceChild::RecvTrackCookiesLoad(
    nsTArray<CookieStruct>&& aCookiesList, const OriginAttributes& aAttrs) {
  for (uint32_t i = 0; i < aCookiesList.Length(); i++) {
    RefPtr<Cookie> cookie = Cookie::Create(
        aCookiesList[i].name(), aCookiesList[i].value(), aCookiesList[i].host(),
        aCookiesList[i].path(), aCookiesList[i].expiry(),
        aCookiesList[i].lastAccessed(), aCookiesList[i].creationTime(),
        aCookiesList[i].isSession(), aCookiesList[i].isSecure(), false, aAttrs,
        aCookiesList[i].sameSite(), aCookiesList[i].rawSameSite());
    RecordDocumentCookie(cookie, aAttrs);
  }

  return IPC_OK();
}

void CookieServiceChild::PrefChanged(nsIPrefBranch* aPrefBranch) {
  int32_t val;
  if (NS_SUCCEEDED(aPrefBranch->GetIntPref(kCookieMoveIntervalSecs, &val))) {
    gMoveCookiesIntervalSeconds = clamped<uint32_t>(val, 0, 3600);
    if (gMoveCookiesIntervalSeconds && !mCookieTimer) {
      NS_NewTimerWithCallback(getter_AddRefs(mCookieTimer), this,
                              gMoveCookiesIntervalSeconds * 1000,
                              nsITimer::TYPE_REPEATING_SLACK_LOW_PRIORITY);
    }
    if (!gMoveCookiesIntervalSeconds && mCookieTimer) {
      mCookieTimer->Cancel();
      mCookieTimer = nullptr;
    }
    if (mCookieTimer) {
      mCookieTimer->SetDelay(gMoveCookiesIntervalSeconds * 1000);
    }
  }
}

void CookieServiceChild::GetCookieStringFromCookieHashTable(
    nsIURI* aHostURI, bool aIsForeign, bool aIsThirdPartyTrackingResource,
    bool aIsThirdPartySocialTrackingResource,
    bool aFirstPartyStorageAccessGranted, uint32_t aRejectedReason,
    bool aIsSafeTopLevelNav, bool aIsSameSiteForeign, nsIChannel* aChannel,
    nsACString& aCookieString) {
  nsCOMPtr<nsIEffectiveTLDService> TLDService =
      do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID);
  NS_ASSERTION(TLDService, "Can't get TLDService");
  bool requireHostMatch;
  nsAutoCString baseDomain;

  nsCOMPtr<nsILoadInfo> loadInfo;
  mozilla::OriginAttributes attrs;
  if (aChannel) {
    loadInfo = aChannel->LoadInfo();
    attrs = loadInfo->GetOriginAttributes();
    StoragePrincipalHelper::PrepareOriginAttributes(aChannel, attrs);
  }

  nsCookieService::GetBaseDomain(TLDService, aHostURI, baseDomain,
                                 requireHostMatch);
  nsCookieKey key(baseDomain, attrs);
  CookiesList* cookiesList = nullptr;
  mCookiesMap.Get(key, &cookiesList);

  if (!cookiesList) {
    return;
  }

  nsAutoCString hostFromURI, pathFromURI;
  aHostURI->GetAsciiHost(hostFromURI);
  aHostURI->GetFilePath(pathFromURI);
  bool isSecure = aHostURI->SchemeIs("https");
  int64_t currentTimeInUsec = PR_Now();
  int64_t currentTime = currentTimeInUsec / PR_USEC_PER_SEC;

  nsCOMPtr<nsICookieJarSettings> cookieJarSettings =
      nsCookieService::GetCookieJarSettings(aChannel);

  CookieStatus cookieStatus = nsCookieService::CheckPrefs(
      cookieJarSettings, aHostURI, aIsForeign, aIsThirdPartyTrackingResource,
      aIsThirdPartySocialTrackingResource, aFirstPartyStorageAccessGranted,
      VoidCString(), CountCookiesFromHashTable(baseDomain, attrs), attrs,
      &aRejectedReason);

  if (cookieStatus != STATUS_ACCEPTED &&
      cookieStatus != STATUS_ACCEPT_SESSION) {
    return;
  }

  cookiesList->Sort(CompareCookiesForSending());
  for (uint32_t i = 0; i < cookiesList->Length(); i++) {
    Cookie* cookie = cookiesList->ElementAt(i);
    // check the host, since the base domain lookup is conservative.
    if (!nsCookieService::DomainMatches(cookie, hostFromURI)) continue;

    // We don't show HttpOnly cookies in content processes.
    if (cookie->IsHttpOnly()) {
      continue;
    }

    // if the cookie is secure and the host scheme isn't, we can't send it
    if (cookie->IsSecure() && !isSecure) continue;

    int32_t sameSiteAttr = 0;
    cookie->GetSameSite(&sameSiteAttr);
    if (aIsSameSiteForeign) {
      // it if's a cross origin request and the cookie is same site only
      // (strict) don't send it
      if (sameSiteAttr == nsICookie::SAMESITE_STRICT) {
        continue;
      }
      // if it's a cross origin request, the cookie is same site lax, but it's
      // not a top-level navigation, don't send it
      if (sameSiteAttr == nsICookie::SAMESITE_LAX && !aIsSafeTopLevelNav) {
        continue;
      }
    }

    // if the nsIURI path doesn't match the cookie path, don't send it back
    if (!nsCookieService::PathMatches(cookie, pathFromURI)) continue;

    // check if the cookie has expired
    if (cookie->Expiry() <= currentTime) {
      continue;
    }

    if (!cookie->Name().IsEmpty() || !cookie->Value().IsEmpty()) {
      if (!aCookieString.IsEmpty()) {
        aCookieString.AppendLiteral("; ");
      }
      if (!cookie->Name().IsEmpty()) {
        aCookieString.Append(cookie->Name().get());
        aCookieString.AppendLiteral("=");
        aCookieString.Append(cookie->Value().get());
      } else {
        aCookieString.Append(cookie->Value().get());
      }
    }
  }
}

uint32_t CookieServiceChild::CountCookiesFromHashTable(
    const nsCString& aBaseDomain, const OriginAttributes& aOriginAttrs) {
  CookiesList* cookiesList = nullptr;

  nsCString baseDomain;
  nsCookieKey key(aBaseDomain, aOriginAttrs);
  mCookiesMap.Get(key, &cookiesList);

  return cookiesList ? cookiesList->Length() : 0;
}

void CookieServiceChild::SetCookieInternal(
    const CookieStruct& aCookieData, const mozilla::OriginAttributes& aAttrs,
    nsIChannel* aChannel, bool aFromHttp,
    nsICookiePermission* aPermissionService) {
  int64_t currentTimeInUsec = PR_Now();
  RefPtr<Cookie> cookie = Cookie::Create(
      aCookieData.name(), aCookieData.value(), aCookieData.host(),
      aCookieData.path(), aCookieData.expiry(), currentTimeInUsec,
      Cookie::GenerateUniqueCreationTime(currentTimeInUsec),
      aCookieData.isSession(), aCookieData.isSecure(), aCookieData.isHttpOnly(),
      aAttrs, aCookieData.sameSite(), aCookieData.rawSameSite());

  RecordDocumentCookie(cookie, aAttrs);
}

/* static */ bool CookieServiceChild::RequireThirdPartyCheck(
    nsILoadInfo* aLoadInfo) {
  if (!aLoadInfo) {
    return false;
  }

  nsCOMPtr<nsICookieJarSettings> cookieJarSettings;
  nsresult rv =
      aLoadInfo->GetCookieJarSettings(getter_AddRefs(cookieJarSettings));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  uint32_t cookieBehavior = cookieJarSettings->GetCookieBehavior();
  return cookieBehavior == nsICookieService::BEHAVIOR_REJECT_FOREIGN ||
         cookieBehavior == nsICookieService::BEHAVIOR_LIMIT_FOREIGN ||
         cookieBehavior == nsICookieService::BEHAVIOR_REJECT_TRACKER ||
         cookieBehavior ==
             nsICookieService::BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN ||
         StaticPrefs::network_cookie_thirdparty_sessionOnly() ||
         StaticPrefs::network_cookie_thirdparty_nonsecureSessionOnly();
}

void CookieServiceChild::RecordDocumentCookie(Cookie* aCookie,
                                              const OriginAttributes& aAttrs) {
  nsAutoCString baseDomain;
  nsCookieService::GetBaseDomainFromHost(mTLDService, aCookie->Host(),
                                         baseDomain);

  nsCookieKey key(baseDomain, aAttrs);
  CookiesList* cookiesList = nullptr;
  mCookiesMap.Get(key, &cookiesList);

  if (!cookiesList) {
    cookiesList = mCookiesMap.LookupOrAdd(key);
  }
  for (uint32_t i = 0; i < cookiesList->Length(); i++) {
    Cookie* cookie = cookiesList->ElementAt(i);
    if (cookie->Name().Equals(aCookie->Name()) &&
        cookie->Host().Equals(aCookie->Host()) &&
        cookie->Path().Equals(aCookie->Path())) {
      if (cookie->Value().Equals(aCookie->Value()) &&
          cookie->Expiry() == aCookie->Expiry() &&
          cookie->IsSecure() == aCookie->IsSecure() &&
          cookie->SameSite() == aCookie->SameSite() &&
          cookie->IsSession() == aCookie->IsSession() &&
          cookie->IsHttpOnly() == aCookie->IsHttpOnly()) {
        cookie->SetLastAccessed(aCookie->LastAccessed());
        return;
      }
      cookiesList->RemoveElementAt(i);
      break;
    }
  }

  int64_t currentTime = PR_Now() / PR_USEC_PER_SEC;
  if (aCookie->Expiry() <= currentTime) {
    return;
  }

  cookiesList->AppendElement(aCookie);
}

nsresult CookieServiceChild::GetCookieStringInternal(
    nsIURI* aHostURI, nsIChannel* aChannel, nsACString& aCookieString) {
  NS_ENSURE_ARG(aHostURI);

  aCookieString.Truncate();

  // Fast past: don't bother sending IPC messages about nullprincipal'd
  // documents.
  nsAutoCString scheme;
  aHostURI->GetScheme(scheme);
  if (scheme.EqualsLiteral("moz-nullprincipal")) return NS_OK;

  uint32_t rejectedReason = 0;
  ThirdPartyAnalysisResult result = mThirdPartyUtil->AnalyzeChannel(
      aChannel, false, aHostURI, RequireThirdPartyCheck, &rejectedReason);

  bool isSafeTopLevelNav = NS_IsSafeTopLevelNav(aChannel);
  bool isSameSiteForeign = NS_IsSameSiteForeign(aChannel, aHostURI);

  GetCookieStringFromCookieHashTable(
      aHostURI, result.contains(ThirdPartyAnalysis::IsForeign),
      result.contains(ThirdPartyAnalysis::IsThirdPartyTrackingResource),
      result.contains(ThirdPartyAnalysis::IsThirdPartySocialTrackingResource),
      result.contains(ThirdPartyAnalysis::IsFirstPartyStorageAccessGranted),
      rejectedReason, isSafeTopLevelNav, isSameSiteForeign, aChannel,
      aCookieString);

  return NS_OK;
}

nsresult CookieServiceChild::SetCookieStringInternal(
    nsIURI* aHostURI, nsIChannel* aChannel, const nsACString& aCookieString,
    const nsACString& aServerTime, bool aFromHttp) {
  NS_ENSURE_ARG(aHostURI);

  // Fast past: don't bother sending IPC messages about nullprincipal'd
  // documents.
  nsAutoCString scheme;
  aHostURI->GetScheme(scheme);
  if (scheme.EqualsLiteral("moz-nullprincipal")) return NS_OK;

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel ? aChannel->LoadInfo() : nullptr;

  uint32_t rejectedReason = 0;
  ThirdPartyAnalysisResult result = mThirdPartyUtil->AnalyzeChannel(
      aChannel, false, aHostURI, RequireThirdPartyCheck, &rejectedReason);

  nsCString cookieString(aCookieString);

  URIParams hostURIParams;
  SerializeURI(aHostURI, hostURIParams);

  Maybe<URIParams> channelURIParams;
  mozilla::OriginAttributes attrs;
  if (aChannel) {
    nsCOMPtr<nsIURI> channelURI;
    aChannel->GetURI(getter_AddRefs(channelURI));
    SerializeURI(channelURI, channelURIParams);

    MOZ_ASSERT(loadInfo);
    attrs = loadInfo->GetOriginAttributes();
    StoragePrincipalHelper::PrepareOriginAttributes(aChannel, attrs);
  } else {
    SerializeURI(nullptr, channelURIParams);
  }

  Maybe<LoadInfoArgs> optionalLoadInfoArgs;
  LoadInfoToLoadInfoArgs(loadInfo, &optionalLoadInfoArgs);

  // Asynchronously call the parent.
  if (CanSend()) {
    SendSetCookieString(
        hostURIParams, channelURIParams, optionalLoadInfoArgs,
        result.contains(ThirdPartyAnalysis::IsForeign),
        result.contains(ThirdPartyAnalysis::IsThirdPartyTrackingResource),
        result.contains(ThirdPartyAnalysis::IsThirdPartySocialTrackingResource),
        result.contains(ThirdPartyAnalysis::IsFirstPartyStorageAccessGranted),
        rejectedReason, attrs, cookieString, nsCString(aServerTime), aFromHttp);
  }

  bool requireHostMatch;
  nsCString baseDomain;
  nsCookieService::GetBaseDomain(mTLDService, aHostURI, baseDomain,
                                 requireHostMatch);

  nsCOMPtr<nsICookieJarSettings> cookieJarSettings =
      nsCookieService::GetCookieJarSettings(aChannel);

  CookieStatus cookieStatus = nsCookieService::CheckPrefs(
      cookieJarSettings, aHostURI,
      result.contains(ThirdPartyAnalysis::IsForeign),
      result.contains(ThirdPartyAnalysis::IsThirdPartyTrackingResource),
      result.contains(ThirdPartyAnalysis::IsThirdPartySocialTrackingResource),
      result.contains(ThirdPartyAnalysis::IsFirstPartyStorageAccessGranted),
      aCookieString, CountCookiesFromHashTable(baseDomain, attrs), attrs,
      &rejectedReason);

  if (cookieStatus != STATUS_ACCEPTED &&
      cookieStatus != STATUS_ACCEPT_SESSION) {
    return NS_OK;
  }

  nsCookieKey key(baseDomain, attrs);
  CookiesList* cookies = mCookiesMap.Get(key);

  nsCString serverTimeString(aServerTime);
  int64_t serverTime = nsCookieService::ParseServerTime(serverTimeString);
  bool moreCookies;
  do {
    CookieStruct cookieData;
    bool canSetCookie = false;
    moreCookies = nsCookieService::CanSetCookie(
        aHostURI, key, cookieData, requireHostMatch, cookieStatus, cookieString,
        serverTime, aFromHttp, aChannel, canSetCookie, mThirdPartyUtil);

    // We need to see if the cookie we're setting would overwrite an httponly
    // one. This would not affect anything we send over the net (those come from
    // the parent, which already checks this), but script could see an
    // inconsistent view of things.
    if (cookies && canSetCookie && !aFromHttp) {
      for (uint32_t i = 0; i < cookies->Length(); ++i) {
        RefPtr<Cookie> cookie = cookies->ElementAt(i);
        if (cookie->Name().Equals(cookieData.name()) &&
            cookie->Host().Equals(cookieData.host()) &&
            cookie->Path().Equals(cookieData.path()) && cookie->IsHttpOnly()) {
          // Can't overwrite an httponly cookie from a script context.
          canSetCookie = false;
        }
      }
    }

    if (canSetCookie) {
      nsCOMPtr<nsICookiePermission> permissionService =
          nsCookiePermission::GetOrCreate();
      SetCookieInternal(cookieData, attrs, aChannel, aFromHttp,
                        permissionService);
    }

    // document.cookie can only set one cookie at a time.
    if (!aFromHttp) {
      break;
    }

  } while (moreCookies);
  return NS_OK;
}

NS_IMETHODIMP
CookieServiceChild::Observe(nsISupports* aSubject, const char* aTopic,
                            const char16_t* aData) {
  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    if (mCookieTimer) {
      mCookieTimer->Cancel();
      mCookieTimer = nullptr;
    }
    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    if (observerService) {
      observerService->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
    }
  } else if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    nsCOMPtr<nsIPrefBranch> prefBranch = do_QueryInterface(aSubject);
    if (prefBranch) {
      PrefChanged(prefBranch);
    }
  } else {
    MOZ_ASSERT(false, "unexpected topic!");
  }

  return NS_OK;
}

NS_IMETHODIMP
CookieServiceChild::GetCookieString(nsIURI* aHostURI, nsIChannel* aChannel,
                                    nsACString& aCookieString) {
  return GetCookieStringInternal(aHostURI, aChannel, aCookieString);
}

NS_IMETHODIMP
CookieServiceChild::GetCookieStringFromHttp(nsIURI* aHostURI, nsIURI* aFirstURI,
                                            nsIChannel* aChannel,
                                            nsACString& aCookieString) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
CookieServiceChild::SetCookieString(nsIURI* aHostURI,
                                    const nsACString& aCookieString,
                                    nsIChannel* aChannel) {
  return SetCookieStringInternal(aHostURI, aChannel, aCookieString,
                                 VoidCString(), false);
}

NS_IMETHODIMP
CookieServiceChild::SetCookieStringFromHttp(nsIURI* aHostURI, nsIURI* aFirstURI,
                                            const nsACString& aCookieString,
                                            const nsACString& aServerTime,
                                            nsIChannel* aChannel) {
  return SetCookieStringInternal(aHostURI, aChannel, aCookieString, aServerTime,
                                 true);
}

NS_IMETHODIMP
CookieServiceChild::RunInTransaction(nsICookieTransactionCallback* aCallback) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

}  // namespace net
}  // namespace mozilla
