/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Cookie.h"
#include "CookieCommons.h"
#include "CookieLogging.h"
#include "CookieService.h"
#include "mozilla/net/CookieServiceChild.h"
#include "mozilla/net/NeckoChannelParams.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/StoragePrincipalHelper.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsIChannel.h"
#include "nsIClassifiedChannel.h"
#include "nsIHttpChannel.h"
#include "nsIEffectiveTLDService.h"
#include "nsIURI.h"
#include "nsIPrefBranch.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"
#include "ThirdPartyUtil.h"

using namespace mozilla::ipc;

namespace mozilla {
namespace net {

// Pref string constants
static const char kCookieMoveIntervalSecs[] =
    "network.cookie.move.interval_sec";

static StaticRefPtr<CookieServiceChild> gCookieChildService;
static uint32_t gMoveCookiesIntervalSeconds = 10;

already_AddRefed<CookieServiceChild> CookieServiceChild::GetSingleton() {
  if (!gCookieChildService) {
    gCookieChildService = new CookieServiceChild();
    ClearOnShutdown(&gCookieChildService);
  }

  return do_AddRef(gCookieChildService);
}

NS_IMPL_ISUPPORTS(CookieServiceChild, nsICookieService, nsIObserver,
                  nsITimerCallback, nsISupportsWeakReference)

CookieServiceChild::CookieServiceChild() {
  NS_ASSERTION(IsNeckoChild(), "not a child process");

  auto* cc = static_cast<mozilla::dom::ContentChild*>(gNeckoChild->Manager());
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

  nsCOMPtr<nsIObserverService> observerService = services::GetObserverService();
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
      RefPtr<Cookie> newCookie = cookie->Clone();
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

CookieServiceChild::~CookieServiceChild() { gCookieChildService = nullptr; }

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

  OriginAttributes attrs = loadInfo->GetOriginAttributes();
  StoragePrincipalHelper::PrepareEffectiveStoragePrincipalOriginAttributes(
      aChannel, attrs);

  bool isSafeTopLevelNav = NS_IsSafeTopLevelNav(aChannel);
  bool isSameSiteForeign = NS_IsSameSiteForeign(aChannel, uri);
  SendPrepareCookieList(
      uri, result.contains(ThirdPartyAnalysis::IsForeign),
      result.contains(ThirdPartyAnalysis::IsThirdPartyTrackingResource),
      result.contains(ThirdPartyAnalysis::IsThirdPartySocialTrackingResource),
      result.contains(ThirdPartyAnalysis::IsFirstPartyStorageAccessGranted),
      rejectedReason, isSafeTopLevelNav, isSameSiteForeign, attrs);
}

IPCResult CookieServiceChild::RecvRemoveAll() {
  mCookiesMap.Clear();
  return IPC_OK();
}

IPCResult CookieServiceChild::RecvRemoveCookie(const CookieStruct& aCookie,
                                               const OriginAttributes& aAttrs) {
  nsCString baseDomain;
  CookieCommons::GetBaseDomainFromHost(mTLDService, aCookie.host(), baseDomain);
  CookieKey key(baseDomain, aAttrs);
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

IPCResult CookieServiceChild::RecvAddCookie(const CookieStruct& aCookie,
                                            const OriginAttributes& aAttrs) {
  RefPtr<Cookie> cookie = Cookie::Create(aCookie, aAttrs);
  RecordDocumentCookie(cookie, aAttrs);
  return IPC_OK();
}

IPCResult CookieServiceChild::RecvRemoveBatchDeletedCookies(
    nsTArray<CookieStruct>&& aCookiesList,
    nsTArray<OriginAttributes>&& aAttrsList) {
  MOZ_ASSERT(aCookiesList.Length() == aAttrsList.Length());
  for (uint32_t i = 0; i < aCookiesList.Length(); i++) {
    CookieStruct cookieStruct = aCookiesList.ElementAt(i);
    RecvRemoveCookie(cookieStruct, aAttrsList.ElementAt(i));
  }
  return IPC_OK();
}

IPCResult CookieServiceChild::RecvTrackCookiesLoad(
    nsTArray<CookieStruct>&& aCookiesList, const OriginAttributes& aAttrs) {
  for (uint32_t i = 0; i < aCookiesList.Length(); i++) {
    RefPtr<Cookie> cookie = Cookie::Create(aCookiesList[i], aAttrs);
    cookie->SetIsHttpOnly(false);
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

uint32_t CookieServiceChild::CountCookiesFromHashTable(
    const nsACString& aBaseDomain, const OriginAttributes& aOriginAttrs) {
  CookiesList* cookiesList = nullptr;

  nsCString baseDomain;
  CookieKey key(aBaseDomain, aOriginAttrs);
  mCookiesMap.Get(key, &cookiesList);

  return cookiesList ? cookiesList->Length() : 0;
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
  CookieCommons::GetBaseDomainFromHost(mTLDService, aCookie->Host(),
                                       baseDomain);

  CookieKey key(baseDomain, aAttrs);
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

NS_IMETHODIMP
CookieServiceChild::Observe(nsISupports* aSubject, const char* aTopic,
                            const char16_t* /*aData*/) {
  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    if (mCookieTimer) {
      mCookieTimer->Cancel();
      mCookieTimer = nullptr;
    }
    nsCOMPtr<nsIObserverService> observerService =
        services::GetObserverService();
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
CookieServiceChild::GetCookieStringForPrincipal(nsIPrincipal* aPrincipal,
                                                nsACString& aCookieString) {
  NS_ENSURE_ARG(aPrincipal);

  aCookieString.Truncate();

  nsAutoCString baseDomain;
  nsresult rv = CookieCommons::GetBaseDomain(aPrincipal, baseDomain);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_OK;
  }

  CookieKey key(baseDomain, aPrincipal->OriginAttributesRef());
  CookiesList* cookiesList = nullptr;
  mCookiesMap.Get(key, &cookiesList);

  if (!cookiesList) {
    return NS_OK;
  }

  nsAutoCString hostFromURI;
  aPrincipal->GetAsciiHost(hostFromURI);

  nsAutoCString pathFromURI;
  aPrincipal->GetFilePath(pathFromURI);

  bool isPotentiallyTrustworthy =
      aPrincipal->GetIsOriginPotentiallyTrustworthy();
  int64_t currentTimeInUsec = PR_Now();
  int64_t currentTime = currentTimeInUsec / PR_USEC_PER_SEC;

  cookiesList->Sort(CompareCookiesForSending());
  for (uint32_t i = 0; i < cookiesList->Length(); i++) {
    Cookie* cookie = cookiesList->ElementAt(i);
    // check the host, since the base domain lookup is conservative.
    if (!CookieCommons::DomainMatches(cookie, hostFromURI)) {
      continue;
    }

    // We don't show HttpOnly cookies in content processes.
    if (cookie->IsHttpOnly()) {
      continue;
    }

    // if the cookie is secure and the host scheme isn't, we can't send it
    if (cookie->IsSecure() && !isPotentiallyTrustworthy) {
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

  return NS_OK;
}

NS_IMETHODIMP
CookieServiceChild::GetCookieStringFromHttp(nsIURI* /*aHostURI*/,
                                            nsIChannel* /*aChannel*/,
                                            nsACString& /*aCookieString*/) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
CookieServiceChild::SetCookieStringFromDocument(
    Document* aDocument, const nsACString& aCookieString) {
  NS_ENSURE_ARG(aDocument);

  nsCOMPtr<nsIURI> documentURI;
  nsAutoCString baseDomain;
  OriginAttributes attrs;

  // This function is executed in this context, I don't need to keep objects
  // alive.
  auto hasExistingCookiesLambda = [&](const nsACString& aBaseDomain,
                                      const OriginAttributes& aAttrs) {
    return !!CountCookiesFromHashTable(aBaseDomain, aAttrs);
  };

  RefPtr<Cookie> cookie = CookieCommons::CreateCookieFromDocument(
      aDocument, aCookieString, PR_Now(), mTLDService, mThirdPartyUtil,
      hasExistingCookiesLambda, getter_AddRefs(documentURI), baseDomain, attrs);
  if (!cookie) {
    return NS_OK;
  }

  CookieKey key(baseDomain, attrs);
  CookiesList* cookies = mCookiesMap.Get(key);

  if (cookies) {
    // We need to see if the cookie we're setting would overwrite an httponly
    // one. This would not affect anything we send over the net (those come
    // from the parent, which already checks this), but script could see an
    // inconsistent view of things.
    for (uint32_t i = 0; i < cookies->Length(); ++i) {
      RefPtr<Cookie> existingCookie = cookies->ElementAt(i);
      if (existingCookie->Name().Equals(cookie->Name()) &&
          existingCookie->Host().Equals(cookie->Host()) &&
          existingCookie->Path().Equals(cookie->Path()) &&
          existingCookie->IsHttpOnly()) {
        // Can't overwrite an httponly cookie from a script context.
        return NS_OK;
      }
    }
  }

  RecordDocumentCookie(cookie, attrs);

  if (CanSend()) {
    nsTArray<CookieStruct> cookiesToSend;
    cookiesToSend.AppendElement(cookie->ToIPC());

    // Asynchronously call the parent.
    SendSetCookies(baseDomain, attrs, documentURI, false, cookiesToSend);
  }

  return NS_OK;
}

NS_IMETHODIMP
CookieServiceChild::SetCookieStringFromHttp(nsIURI* aHostURI,
                                            const nsACString& aCookieString,
                                            nsIChannel* aChannel) {
  NS_ENSURE_ARG(aHostURI);
  NS_ENSURE_ARG(aChannel);

  // Fast past: don't bother sending IPC messages about nullprincipal'd
  // documents.
  nsAutoCString scheme;
  aHostURI->GetScheme(scheme);
  if (scheme.EqualsLiteral("moz-nullprincipal")) {
    return NS_OK;
  }

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();

  uint32_t rejectedReason = 0;
  ThirdPartyAnalysisResult result = mThirdPartyUtil->AnalyzeChannel(
      aChannel, false, aHostURI, RequireThirdPartyCheck, &rejectedReason);

  nsCString cookieString(aCookieString);

  OriginAttributes attrs = loadInfo->GetOriginAttributes();
  StoragePrincipalHelper::PrepareEffectiveStoragePrincipalOriginAttributes(
      aChannel, attrs);

  bool requireHostMatch;
  nsCString baseDomain;
  CookieCommons::GetBaseDomain(mTLDService, aHostURI, baseDomain,
                               requireHostMatch);

  nsCOMPtr<nsICookieJarSettings> cookieJarSettings =
      CookieCommons::GetCookieJarSettings(aChannel);

  CookieStatus cookieStatus = CookieService::CheckPrefs(
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

  CookieKey key(baseDomain, attrs);

  nsTArray<CookieStruct> cookiesToSend;

  int64_t currentTimeInUsec = PR_Now();

  bool addonAllowsLoad = false;
  nsCOMPtr<nsIURI> finalChannelURI;
  NS_GetFinalChannelURI(aChannel, getter_AddRefs(finalChannelURI));
  addonAllowsLoad = BasePrincipal::Cast(loadInfo->TriggeringPrincipal())
                        ->AddonAllowsLoad(finalChannelURI);

  bool isForeignAndNotAddon = false;
  if (!addonAllowsLoad) {
    mThirdPartyUtil->IsThirdPartyChannel(aChannel, aHostURI,
                                         &isForeignAndNotAddon);
  }

  nsCOMPtr<nsIConsoleReportCollector> crc = do_QueryInterface(aChannel);

  bool moreCookies;
  do {
    CookieStruct cookieData;
    bool canSetCookie = false;
    moreCookies = CookieService::CanSetCookie(
        aHostURI, baseDomain, cookieData, requireHostMatch, cookieStatus,
        cookieString, true, isForeignAndNotAddon, crc, canSetCookie);
    if (!canSetCookie) {
      continue;
    }

    // check permissions from site permission list.
    if (!CookieCommons::CheckCookiePermission(aChannel, cookieData)) {
      COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, aCookieString,
                        "cookie rejected by permission manager");
      CookieCommons::NotifyRejected(
          aHostURI, aChannel,
          nsIWebProgressListener::STATE_COOKIES_BLOCKED_BY_PERMISSION,
          OPERATION_WRITE);
      continue;
    }

    RefPtr<Cookie> cookie = Cookie::Create(cookieData, attrs);
    MOZ_ASSERT(cookie);

    cookie->SetLastAccessed(currentTimeInUsec);
    cookie->SetCreationTime(
        Cookie::GenerateUniqueCreationTime(currentTimeInUsec));

    RecordDocumentCookie(cookie, attrs);
    cookiesToSend.AppendElement(cookieData);
  } while (moreCookies);

  // Asynchronously call the parent.
  if (CanSend() && !cookiesToSend.IsEmpty()) {
    SendSetCookies(baseDomain, attrs, aHostURI, true, cookiesToSend);
  }

  return NS_OK;
}

NS_IMETHODIMP
CookieServiceChild::RunInTransaction(
    nsICookieTransactionCallback* /*aCallback*/) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

}  // namespace net
}  // namespace mozilla
