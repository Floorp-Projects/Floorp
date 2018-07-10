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
#include "mozilla/SystemGroup.h"
#include "nsCookie.h"
#include "nsCookieService.h"
#include "nsContentUtils.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsIChannel.h"
#include "nsICookiePermission.h"
#include "nsIEffectiveTLDService.h"
#include "nsIURI.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"

using namespace mozilla::ipc;
using mozilla::OriginAttributes;

namespace mozilla {
namespace net {

// Pref string constants
static const char kPrefCookieBehavior[] = "network.cookie.cookieBehavior";
static const char kPrefThirdPartySession[] =
  "network.cookie.thirdparty.sessionOnly";
static const char kPrefThirdPartyNonsecureSession[] =
  "network.cookie.thirdparty.nonsecureSessionOnly";
static const char kPrefCookieIPCSync[] = "network.cookie.ipc.sync";
static const char kCookieLeaveSecurityAlone[] = "network.cookie.leave-secure-alone";
static const char kCookieMoveIntervalSecs[] = "network.cookie.move.interval_sec";

static StaticRefPtr<CookieServiceChild> gCookieService;
static uint32_t gMoveCookiesIntervalSeconds = 10;

already_AddRefed<CookieServiceChild>
CookieServiceChild::GetSingleton()
{
  if (!gCookieService) {
    gCookieService = new CookieServiceChild();
    ClearOnShutdown(&gCookieService);
  }

  return do_AddRef(gCookieService);
}

NS_IMPL_ISUPPORTS(CookieServiceChild,
                  nsICookieService,
                  nsIObserver,
                  nsITimerCallback,
                  nsISupportsWeakReference)

CookieServiceChild::CookieServiceChild()
  : mCookieBehavior(nsICookieService::BEHAVIOR_ACCEPT)
  , mThirdPartySession(false)
  , mThirdPartyNonsecureSession(false)
  , mLeaveSecureAlone(true)
  , mIPCSync(false)
  , mIPCOpen(false)
{
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

  mIPCOpen = true;

  mTLDService = do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID);
  NS_ASSERTION(mTLDService, "couldn't get TLDService");

  // Init our prefs and observer.
  nsCOMPtr<nsIPrefBranch> prefBranch =
    do_GetService(NS_PREFSERVICE_CONTRACTID);
  NS_WARNING_ASSERTION(prefBranch, "no prefservice");
  if (prefBranch) {
    prefBranch->AddObserver(kPrefCookieBehavior, this, true);
    prefBranch->AddObserver(kPrefThirdPartySession, this, true);
    prefBranch->AddObserver(kPrefThirdPartyNonsecureSession, this, true);
    prefBranch->AddObserver(kPrefCookieIPCSync, this, true);
    prefBranch->AddObserver(kCookieLeaveSecurityAlone, this, true);
    prefBranch->AddObserver(kCookieMoveIntervalSecs, this, true);
    PrefChanged(prefBranch);
  }

  nsCOMPtr<nsIObserverService> observerService
    = mozilla::services::GetObserverService();
  if (observerService) {
    observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
  }
}

void
CookieServiceChild::MoveCookies()
{
  TimeStamp start = TimeStamp::Now();
  for (auto iter = mCookiesMap.Iter(); !iter.Done(); iter.Next()) {
    CookiesList *cookiesList = iter.UserData();
    CookiesList newCookiesList;
    for (uint32_t i = 0; i < cookiesList->Length(); ++i) {
      nsCookie *cookie = cookiesList->ElementAt(i);
      RefPtr<nsCookie> newCookie = nsCookie::Create(cookie->Name(),
                                                    cookie->Value(),
                                                    cookie->Host(),
                                                    cookie->Path(),
                                                    cookie->Expiry(),
                                                    cookie->LastAccessed(),
                                                    cookie->CreationTime(),
                                                    cookie->IsSession(),
                                                    cookie->IsSecure(),
                                                    cookie->IsHttpOnly(),
                                                    cookie->OriginAttributesRef(),
                                                    cookie->SameSite());
      newCookiesList.AppendElement(newCookie);
    }
    cookiesList->SwapElements(newCookiesList);
  }

  Telemetry::AccumulateTimeDelta(Telemetry::COOKIE_TIME_MOVING_MS, start);
}

NS_IMETHODIMP
CookieServiceChild::Notify(nsITimer *aTimer)
{
  if (aTimer == mCookieTimer) {
    MoveCookies();
  } else {
    MOZ_CRASH("Unknown timer");
  }
  return NS_OK;
}

CookieServiceChild::~CookieServiceChild()
{
  gCookieService = nullptr;
}

void
CookieServiceChild::ActorDestroy(ActorDestroyReason why)
{
  mIPCOpen = false;
}

void
CookieServiceChild::TrackCookieLoad(nsIChannel *aChannel)
{
  if (!mIPCOpen) {
    return;
  }

  bool isForeign = false;
  bool isTrackingResource = false;
  bool firstPartyStorageAccessGranted = false;
  nsCOMPtr<nsIURI> uri;
  aChannel->GetURI(getter_AddRefs(uri));
  if (RequireThirdPartyCheck()) {
    mThirdPartyUtil->IsThirdPartyChannel(aChannel, uri, &isForeign);
  }
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aChannel);
  if (httpChannel) {
    isTrackingResource = httpChannel->GetIsTrackingResource();
  }
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->GetLoadInfo();
  mozilla::OriginAttributes attrs;
  if (loadInfo) {
    attrs = loadInfo->GetOriginAttributes();
    if (loadInfo->IsFirstPartyStorageAccessGrantedFor(uri)) {
      firstPartyStorageAccessGranted = true;
    }
  }
  URIParams uriParams;
  SerializeURI(uri, uriParams);
  bool isSafeTopLevelNav = NS_IsSafeTopLevelNav(aChannel);
  bool isSameSiteForeign = NS_IsSameSiteForeign(aChannel, uri);
  SendPrepareCookieList(uriParams, isForeign, isTrackingResource,
                        firstPartyStorageAccessGranted, isSafeTopLevelNav,
                        isSameSiteForeign, attrs);
}

mozilla::ipc::IPCResult
CookieServiceChild::RecvRemoveAll(){
  mCookiesMap.Clear();
  return IPC_OK();
}

mozilla::ipc::IPCResult
CookieServiceChild::RecvRemoveCookie(const CookieStruct     &aCookie,
                                     const OriginAttributes &aAttrs)
{
  nsCString baseDomain;
  nsCookieService::
    GetBaseDomainFromHost(mTLDService, aCookie.host(), baseDomain);
  nsCookieKey key(baseDomain, aAttrs);
  CookiesList *cookiesList = nullptr;
  mCookiesMap.Get(key, &cookiesList);

  if (!cookiesList) {
    return IPC_OK();
  }

  for (uint32_t i = 0; i < cookiesList->Length(); i++) {
    nsCookie *cookie = cookiesList->ElementAt(i);
    if (cookie->Name().Equals(aCookie.name()) &&
        cookie->Host().Equals(aCookie.host()) &&
        cookie->Path().Equals(aCookie.path())) {
      cookiesList->RemoveElementAt(i);
      break;
    }
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
CookieServiceChild::RecvAddCookie(const CookieStruct     &aCookie,
                                  const OriginAttributes &aAttrs)
{
  RefPtr<nsCookie> cookie = nsCookie::Create(aCookie.name(),
                                             aCookie.value(),
                                             aCookie.host(),
                                             aCookie.path(),
                                             aCookie.expiry(),
                                             aCookie.lastAccessed(),
                                             aCookie.creationTime(),
                                             aCookie.isSession(),
                                             aCookie.isSecure(),
                                             aCookie.isHttpOnly(),
                                             aAttrs,
                                             aCookie.sameSite());
  RecordDocumentCookie(cookie, aAttrs);
  return IPC_OK();
}

mozilla::ipc::IPCResult
CookieServiceChild::RecvRemoveBatchDeletedCookies(nsTArray<CookieStruct>&& aCookiesList,
                                                  nsTArray<OriginAttributes>&& aAttrsList)
{
  MOZ_ASSERT(aCookiesList.Length() == aAttrsList.Length());
  for (uint32_t i = 0; i < aCookiesList.Length(); i++) {
    CookieStruct cookieStruct = aCookiesList.ElementAt(i);
    RecvRemoveCookie(cookieStruct, aAttrsList.ElementAt(i));
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
CookieServiceChild::RecvTrackCookiesLoad(nsTArray<CookieStruct>&& aCookiesList,
                                         const OriginAttributes &aAttrs)
{
  for (uint32_t i = 0; i < aCookiesList.Length(); i++) {
    RefPtr<nsCookie> cookie = nsCookie::Create(aCookiesList[i].name(),
                                               aCookiesList[i].value(),
                                               aCookiesList[i].host(),
                                               aCookiesList[i].path(),
                                               aCookiesList[i].expiry(),
                                               aCookiesList[i].lastAccessed(),
                                               aCookiesList[i].creationTime(),
                                               aCookiesList[i].isSession(),
                                               aCookiesList[i].isSecure(),
                                               false,
                                               aAttrs,
                                               aCookiesList[i].sameSite());
    RecordDocumentCookie(cookie, aAttrs);
  }

  return IPC_OK();
}

void
CookieServiceChild::PrefChanged(nsIPrefBranch *aPrefBranch)
{
  int32_t val;
  if (NS_SUCCEEDED(aPrefBranch->GetIntPref(kPrefCookieBehavior, &val)))
    mCookieBehavior =
      val >= nsICookieService::BEHAVIOR_ACCEPT &&
      val <= nsICookieService::BEHAVIOR_LIMIT_FOREIGN
        ? val : nsICookieService::BEHAVIOR_ACCEPT;

  bool boolval;
  if (NS_SUCCEEDED(aPrefBranch->GetBoolPref(kPrefThirdPartySession, &boolval)))
    mThirdPartySession = !!boolval;

  if (NS_SUCCEEDED(aPrefBranch->GetBoolPref(kPrefThirdPartyNonsecureSession,
                                            &boolval)))
    mThirdPartyNonsecureSession = boolval;

  if (NS_SUCCEEDED(aPrefBranch->GetBoolPref(kPrefCookieIPCSync, &boolval)))
    mIPCSync = !!boolval;

  if (NS_SUCCEEDED(aPrefBranch->GetBoolPref(kCookieLeaveSecurityAlone, &boolval)))
    mLeaveSecureAlone = !!boolval;

  if (!mThirdPartyUtil && RequireThirdPartyCheck()) {
    mThirdPartyUtil = do_GetService(THIRDPARTYUTIL_CONTRACTID);
    NS_ASSERTION(mThirdPartyUtil, "require ThirdPartyUtil service");
  }

  if (NS_SUCCEEDED(aPrefBranch->GetIntPref(kCookieMoveIntervalSecs, &val))) {
    gMoveCookiesIntervalSeconds = clamped<uint32_t>(val, 0, 3600);
    if (gMoveCookiesIntervalSeconds && !mCookieTimer) {
      NS_NewTimerWithCallback(getter_AddRefs(mCookieTimer),
                              this, gMoveCookiesIntervalSeconds * 1000,
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

void
CookieServiceChild::GetCookieStringFromCookieHashTable(nsIURI                 *aHostURI,
                                                       bool                   aIsForeign,
                                                       bool                   aIsTrackingResource,
                                                       bool                   aFirstPartyStorageAccessGranted,
                                                       bool                   aIsSafeTopLevelNav,
                                                       bool                   aIsSameSiteForeign,
                                                       const OriginAttributes &aOriginAttrs,
                                                       nsCString              &aCookieString)
{
  nsCOMPtr<nsIEffectiveTLDService> TLDService =
    do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID);
  NS_ASSERTION(TLDService, "Can't get TLDService");
  bool requireHostMatch;
  nsAutoCString baseDomain;
  nsCookieService::
    GetBaseDomain(TLDService, aHostURI, baseDomain, requireHostMatch);
  nsCookieKey key(baseDomain, aOriginAttrs);
  CookiesList *cookiesList = nullptr;
  mCookiesMap.Get(key, &cookiesList);

  if (!cookiesList) {
    return;
  }

  nsAutoCString hostFromURI, pathFromURI;
  bool isSecure;
  aHostURI->GetAsciiHost(hostFromURI);
  aHostURI->GetPathQueryRef(pathFromURI);
  aHostURI->SchemeIs("https", &isSecure);
  int64_t currentTimeInUsec = PR_Now();
  int64_t currentTime = currentTimeInUsec / PR_USEC_PER_SEC;

  nsCOMPtr<nsICookiePermission> permissionService = do_GetService(NS_COOKIEPERMISSION_CONTRACTID);
  CookieStatus cookieStatus =
    nsCookieService::CheckPrefs(permissionService, mCookieBehavior,
                                mThirdPartySession,
                                mThirdPartyNonsecureSession, aHostURI,
                                aIsForeign, aIsTrackingResource,
                                aFirstPartyStorageAccessGranted, nullptr,
                                CountCookiesFromHashTable(baseDomain, aOriginAttrs),
                                aOriginAttrs);

  if (cookieStatus != STATUS_ACCEPTED && cookieStatus != STATUS_ACCEPT_SESSION) {
    return;
  }

  cookiesList->Sort(CompareCookiesForSending());
  for (uint32_t i = 0; i < cookiesList->Length(); i++) {
    nsCookie *cookie = cookiesList->ElementAt(i);
    // check the host, since the base domain lookup is conservative.
    if (!nsCookieService::DomainMatches(cookie, hostFromURI))
      continue;

    // if the cookie is secure and the host scheme isn't, we can't send it
    if (cookie->IsSecure() && !isSecure)
      continue;

    int32_t sameSiteAttr = 0;
    cookie->GetSameSite(&sameSiteAttr);
    if (aIsSameSiteForeign && nsCookieService::IsSameSiteEnabled()) {
      // it if's a cross origin request and the cookie is same site only (strict)
      // don't send it
      if (sameSiteAttr == nsICookie2::SAMESITE_STRICT) {
        continue;
      }
      // if it's a cross origin request, the cookie is same site lax, but it's not
      // a top-level navigation, don't send it
      if (sameSiteAttr == nsICookie2::SAMESITE_LAX && !aIsSafeTopLevelNav) {
        continue;
      }
    }

    // if the nsIURI path doesn't match the cookie path, don't send it back
    if (!nsCookieService::PathMatches(cookie, pathFromURI))
      continue;

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

void
CookieServiceChild::GetCookieStringSyncIPC(nsIURI                 *aHostURI,
                                           bool                   aIsForeign,
                                           bool                   aIsTrackingResource,
                                           bool                   aFirstPartyStorageAccessGranted,
                                           bool                   aIsSafeTopLevelNav,
                                           bool                   aIsSameSiteForeign,
                                           const OriginAttributes &aAttrs,
                                           nsAutoCString          &aCookieString)
{
  URIParams uriParams;
  SerializeURI(aHostURI, uriParams);

  SendGetCookieString(uriParams, aIsForeign, aIsTrackingResource,
                      aFirstPartyStorageAccessGranted, aIsSafeTopLevelNav,
                      aIsSameSiteForeign, aAttrs, &aCookieString);
}

uint32_t
CookieServiceChild::CountCookiesFromHashTable(const nsCString &aBaseDomain,
                                              const OriginAttributes &aOriginAttrs)
{
  CookiesList *cookiesList = nullptr;

  nsCString baseDomain;
  nsCookieKey key(aBaseDomain, aOriginAttrs);
  mCookiesMap.Get(key, &cookiesList);

  return cookiesList ? cookiesList->Length() : 0;
}

void
CookieServiceChild::SetCookieInternal(nsCookieAttributes              &aCookieAttributes,
                                      const mozilla::OriginAttributes &aAttrs,
                                      nsIChannel                      *aChannel,
                                      bool                             aFromHttp,
                                      nsICookiePermission             *aPermissionService)
{
  int64_t currentTimeInUsec = PR_Now();
  RefPtr<nsCookie> cookie =
    nsCookie::Create(aCookieAttributes.name,
                     aCookieAttributes.value,
                     aCookieAttributes.host,
                     aCookieAttributes.path,
                     aCookieAttributes.expiryTime,
                     currentTimeInUsec,
                     nsCookie::GenerateUniqueCreationTime(currentTimeInUsec),
                     aCookieAttributes.isSession,
                     aCookieAttributes.isSecure,
                     aCookieAttributes.isHttpOnly,
                     aAttrs,
                     aCookieAttributes.sameSite);

  RecordDocumentCookie(cookie, aAttrs);
}

bool
CookieServiceChild::RequireThirdPartyCheck()
{
  return mCookieBehavior == nsICookieService::BEHAVIOR_REJECT_FOREIGN ||
    mCookieBehavior == nsICookieService::BEHAVIOR_LIMIT_FOREIGN ||
    mThirdPartySession ||
    mThirdPartyNonsecureSession;
}

void
CookieServiceChild::RecordDocumentCookie(nsCookie               *aCookie,
                                         const OriginAttributes &aAttrs)
{
  nsAutoCString baseDomain;
  nsCookieService::
    GetBaseDomainFromHost(mTLDService, aCookie->Host(), baseDomain);

  nsCookieKey key(baseDomain, aAttrs);
  CookiesList *cookiesList = nullptr;
  mCookiesMap.Get(key, &cookiesList);

  if (!cookiesList) {
    cookiesList = mCookiesMap.LookupOrAdd(key);
  }
  for (uint32_t i = 0; i < cookiesList->Length(); i++) {
    nsCookie *cookie = cookiesList->ElementAt(i);
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

  if (!aCookie->IsHttpOnly()) {
    cookiesList->AppendElement(aCookie);
  }
}

nsresult
CookieServiceChild::GetCookieStringInternal(nsIURI *aHostURI,
                                            nsIChannel *aChannel,
                                            char **aCookieString)
{
  NS_ENSURE_ARG(aHostURI);
  NS_ENSURE_ARG_POINTER(aCookieString);

  *aCookieString = nullptr;

  // Fast past: don't bother sending IPC messages about nullprincipal'd
  // documents.
  nsAutoCString scheme;
  aHostURI->GetScheme(scheme);
  if (scheme.EqualsLiteral("moz-nullprincipal"))
    return NS_OK;

  nsCOMPtr<nsILoadInfo> loadInfo;
  mozilla::OriginAttributes attrs;
  if (aChannel) {
    loadInfo = aChannel->GetLoadInfo();
    if (loadInfo) {
      attrs = loadInfo->GetOriginAttributes();
    }
  }

  // Asynchronously call the parent.
  bool isForeign = true;
  if (RequireThirdPartyCheck())
    mThirdPartyUtil->IsThirdPartyChannel(aChannel, aHostURI, &isForeign);

  bool isTrackingResource = false;
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aChannel);
  if (httpChannel) {
    isTrackingResource = httpChannel->GetIsTrackingResource();
  }

  bool firstPartyStorageAccessGranted = false;
  if (loadInfo->IsFirstPartyStorageAccessGrantedFor(aHostURI)) {
    firstPartyStorageAccessGranted = true;
  }

  bool isSafeTopLevelNav = NS_IsSafeTopLevelNav(aChannel);
  bool isSameSiteForeign = NS_IsSameSiteForeign(aChannel, aHostURI);

  nsAutoCString result;
  if (!mIPCSync) {
    GetCookieStringFromCookieHashTable(aHostURI, isForeign, isTrackingResource,
                                       firstPartyStorageAccessGranted, isSafeTopLevelNav,
                                       isSameSiteForeign, attrs, result);
  } else {
    if (!mIPCOpen) {
      return NS_ERROR_NOT_AVAILABLE;
    }
    GetCookieStringSyncIPC(aHostURI, isForeign, isTrackingResource,
                           firstPartyStorageAccessGranted, isSafeTopLevelNav,
                           isSameSiteForeign, attrs, result);
  }

  if (!result.IsEmpty())
    *aCookieString = ToNewCString(result);

  return NS_OK;
}

nsresult
CookieServiceChild::SetCookieStringInternal(nsIURI *aHostURI,
                                            nsIChannel *aChannel,
                                            const char *aCookieString,
                                            const char *aServerTime,
                                            bool aFromHttp)
{
  NS_ENSURE_ARG(aHostURI);
  NS_ENSURE_ARG_POINTER(aCookieString);

  // Fast past: don't bother sending IPC messages about nullprincipal'd
  // documents.
  nsAutoCString scheme;
  aHostURI->GetScheme(scheme);
  if (scheme.EqualsLiteral("moz-nullprincipal"))
    return NS_OK;

  // Determine whether the request is foreign. Failure is acceptable.
  bool isForeign = true;
  if (RequireThirdPartyCheck())
    mThirdPartyUtil->IsThirdPartyChannel(aChannel, aHostURI, &isForeign);

  bool isTrackingResource = false;
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aChannel);
  if (httpChannel) {
    isTrackingResource = httpChannel->GetIsTrackingResource();
  }

  nsDependentCString cookieString(aCookieString);
  nsDependentCString stringServerTime;
  if (aServerTime)
    stringServerTime.Rebind(aServerTime);

  URIParams hostURIParams;
  SerializeURI(aHostURI, hostURIParams);

  nsCOMPtr<nsIURI> channelURI;
  aChannel->GetURI(getter_AddRefs(channelURI));
  URIParams channelURIParams;
  SerializeURI(channelURI, channelURIParams);

  bool firstPartyStorageAccessGranted = false;
  mozilla::OriginAttributes attrs;
  if (aChannel) {
    nsCOMPtr<nsILoadInfo> loadInfo = aChannel->GetLoadInfo();
    if (loadInfo) {
      attrs = loadInfo->GetOriginAttributes();
      if (loadInfo->IsFirstPartyStorageAccessGrantedFor(aHostURI)) {
        firstPartyStorageAccessGranted = true;
      }
    }
  }

  // Asynchronously call the parent.
  if (mIPCOpen) {
    SendSetCookieString(hostURIParams, channelURIParams,
                        isForeign, isTrackingResource,
                        firstPartyStorageAccessGranted, cookieString,
                        stringServerTime, attrs, aFromHttp);
  }

  if (mIPCSync) {
    return NS_OK;
  }

  bool requireHostMatch;
  nsCString baseDomain;
  nsCookieService::
    GetBaseDomain(mTLDService, aHostURI, baseDomain, requireHostMatch);

  nsCOMPtr<nsICookiePermission> permissionService = do_GetService(NS_COOKIEPERMISSION_CONTRACTID);

  CookieStatus cookieStatus =
    nsCookieService::CheckPrefs(permissionService, mCookieBehavior,
                                mThirdPartySession,
                                mThirdPartyNonsecureSession, aHostURI,
                                isForeign, isTrackingResource,
                                firstPartyStorageAccessGranted, aCookieString,
                                CountCookiesFromHashTable(baseDomain, attrs),
                                attrs);

  if (cookieStatus != STATUS_ACCEPTED && cookieStatus != STATUS_ACCEPT_SESSION) {
    return NS_OK;
  }

  nsCString serverTimeString(aServerTime);
  int64_t serverTime = nsCookieService::ParseServerTime(serverTimeString);
  bool moreCookies;
  do {
    nsCookieAttributes cookieAttributes;
    bool canSetCookie = false;
    nsCookieKey key(baseDomain, attrs);
    moreCookies = nsCookieService::CanSetCookie(aHostURI, key, cookieAttributes,
                                                requireHostMatch, cookieStatus,
                                                cookieString, serverTime, aFromHttp,
                                                aChannel, mLeaveSecureAlone,
                                                canSetCookie, mThirdPartyUtil);

    if (canSetCookie) {
      SetCookieInternal(cookieAttributes, attrs, aChannel,
                        aFromHttp, permissionService);
    }

    // document.cookie can only set one cookie at a time.
    if (!aFromHttp) {
      break;
    }

  } while (moreCookies);
  return NS_OK;
}

NS_IMETHODIMP
CookieServiceChild::Observe(nsISupports     *aSubject,
                            const char      *aTopic,
                            const char16_t *aData)
{
  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    if (mCookieTimer) {
      mCookieTimer->Cancel();
      mCookieTimer = nullptr;
    }
    nsCOMPtr<nsIObserverService> observerService
      = mozilla::services::GetObserverService();
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
CookieServiceChild::GetCookieString(nsIURI *aHostURI,
                                    nsIChannel *aChannel,
                                    char **aCookieString)
{
  return GetCookieStringInternal(aHostURI, aChannel, aCookieString);
}

NS_IMETHODIMP
CookieServiceChild::GetCookieStringFromHttp(nsIURI *aHostURI,
                                            nsIURI *aFirstURI,
                                            nsIChannel *aChannel,
                                            char **aCookieString)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
CookieServiceChild::SetCookieString(nsIURI *aHostURI,
                                    nsIPrompt *aPrompt,
                                    const char *aCookieString,
                                    nsIChannel *aChannel)
{
  return SetCookieStringInternal(aHostURI, aChannel, aCookieString,
                                 nullptr, false);
}

NS_IMETHODIMP
CookieServiceChild::SetCookieStringFromHttp(nsIURI     *aHostURI,
                                            nsIURI     *aFirstURI,
                                            nsIPrompt  *aPrompt,
                                            const char *aCookieString,
                                            const char *aServerTime,
                                            nsIChannel *aChannel)
{
  return SetCookieStringInternal(aHostURI, aChannel, aCookieString,
                                 aServerTime, true);
}

NS_IMETHODIMP
CookieServiceChild::RunInTransaction(nsICookieTransactionCallback* aCallback)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

} // namespace net
} // namespace mozilla
