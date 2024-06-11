/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Cookie.h"
#include "CookieCommons.h"
#include "CookieLogging.h"
#include "CookieService.h"
#include "mozilla/net/CookieServiceChild.h"
#include "ErrorList.h"
#include "mozilla/net/HttpChannelChild.h"
#include "mozilla/net/NeckoChannelParams.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/Document.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/StoragePrincipalHelper.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsICookieJarSettings.h"
#include "nsIChannel.h"
#include "nsIClassifiedChannel.h"
#include "nsIHttpChannel.h"
#include "nsIEffectiveTLDService.h"
#include "nsIURI.h"
#include "nsIPrefBranch.h"
#include "nsIScriptSecurityManager.h"
#include "nsIWebProgressListener.h"
#include "nsQueryObject.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"
#include "ThirdPartyUtil.h"
#include "nsIConsoleReportCollector.h"
#include "mozilla/dom/WindowGlobalChild.h"

using namespace mozilla::ipc;

namespace mozilla {
namespace net {

static StaticRefPtr<CookieServiceChild> gCookieChildService;

already_AddRefed<CookieServiceChild> CookieServiceChild::GetSingleton() {
  if (!gCookieChildService) {
    gCookieChildService = new CookieServiceChild();
    gCookieChildService->Init();
    ClearOnShutdown(&gCookieChildService);
  }

  return do_AddRef(gCookieChildService);
}

NS_IMPL_ISUPPORTS(CookieServiceChild, nsICookieService,
                  nsISupportsWeakReference)

CookieServiceChild::CookieServiceChild() { NeckoChild::InitNeckoChild(); }

CookieServiceChild::~CookieServiceChild() { gCookieChildService = nullptr; }

void CookieServiceChild::Init() {
  auto* cc = static_cast<mozilla::dom::ContentChild*>(gNeckoChild->Manager());
  if (cc->IsShuttingDown()) {
    return;
  }

  // This corresponds to Release() in DeallocPCookieService.
  NS_ADDREF_THIS();

  // Create a child PCookieService actor. Don't do this in the constructor
  // since it could release 'this' on failure
  gNeckoChild->SendPCookieServiceConstructor(this);

  mThirdPartyUtil = ThirdPartyUtil::GetInstance();
  NS_ASSERTION(mThirdPartyUtil, "couldn't get ThirdPartyUtil service");

  mTLDService = do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID);
  NS_ASSERTION(mTLDService, "couldn't get TLDService");
}

RefPtr<GenericPromise> CookieServiceChild::TrackCookieLoad(
    nsIChannel* aChannel) {
  if (!CanSend()) {
    return GenericPromise::CreateAndReject(NS_ERROR_NOT_AVAILABLE, __func__);
  }

  uint32_t rejectedReason = 0;
  ThirdPartyAnalysisResult result = mThirdPartyUtil->AnalyzeChannel(
      aChannel, true, nullptr, RequireThirdPartyCheck, &rejectedReason);

  nsCOMPtr<nsIURI> uri;
  aChannel->GetURI(getter_AddRefs(uri));
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();

  OriginAttributes storageOriginAttributes = loadInfo->GetOriginAttributes();
  StoragePrincipalHelper::PrepareEffectiveStoragePrincipalOriginAttributes(
      aChannel, storageOriginAttributes);

  bool isSafeTopLevelNav = CookieCommons::IsSafeTopLevelNav(aChannel);
  bool hadCrossSiteRedirects = false;
  bool isSameSiteForeign =
      CookieCommons::IsSameSiteForeign(aChannel, uri, &hadCrossSiteRedirects);

  RefPtr<CookieServiceChild> self(this);

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
    // Add the partitioned principal to principals.
    OriginAttributes partitionedOriginAttributes;
    StoragePrincipalHelper::GetOriginAttributes(
        aChannel, partitionedOriginAttributes,
        StoragePrincipalHelper::ePartitionedPrincipal);
    originAttributesList.AppendElement(partitionedOriginAttributes);
    // Only append the partitioned originAttributes if the partitionKey is set.
    // The partitionKey could be empty for partitionKey in partitioned
    // originAttributes if the channel is for privilege request, such as
    // extension's requests.
    if (!partitionedOriginAttributes.mPartitionKey.IsEmpty()) {
      originAttributesList.AppendElement(partitionedOriginAttributes);
    }
  }

  return SendGetCookieList(
             uri, result.contains(ThirdPartyAnalysis::IsForeign),
             result.contains(ThirdPartyAnalysis::IsThirdPartyTrackingResource),
             result.contains(
                 ThirdPartyAnalysis::IsThirdPartySocialTrackingResource),
             result.contains(
                 ThirdPartyAnalysis::IsStorageAccessPermissionGranted),
             rejectedReason, isSafeTopLevelNav, isSameSiteForeign,
             hadCrossSiteRedirects, originAttributesList)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [self, uri](const nsTArray<CookieStructTable>& aCookiesListTable) {
            for (auto& entry : aCookiesListTable) {
              auto& cookies = entry.cookies();
              for (auto& cookieEntry : cookies) {
                RefPtr<Cookie> cookie =
                    Cookie::Create(cookieEntry, entry.attrs());
                cookie->SetIsHttpOnly(false);
                self->RecordDocumentCookie(cookie, entry.attrs());
              }
            }
            return GenericPromise::CreateAndResolve(true, __func__);
          },
          [](const mozilla::ipc::ResponseRejectReason) {
            return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
          });
}

IPCResult CookieServiceChild::RecvRemoveAll() {
  mCookiesMap.Clear();

  nsCOMPtr<nsIObserverService> obsService = services::GetObserverService();
  if (obsService) {
    obsService->NotifyObservers(nullptr, "content-removed-all-cookies",
                                nullptr);
  }
  return IPC_OK();
}

IPCResult CookieServiceChild::RecvRemoveCookie(const CookieStruct& aCookie,
                                               const OriginAttributes& aAttrs) {
  RemoveSingleCookie(aCookie, aAttrs);

  nsCOMPtr<nsIObserverService> obsService = services::GetObserverService();
  if (obsService) {
    obsService->NotifyObservers(nullptr, "content-removed-cookie", nullptr);
  }
  return IPC_OK();
}

void CookieServiceChild::RemoveSingleCookie(const CookieStruct& aCookie,
                                            const OriginAttributes& aAttrs) {
  nsCString baseDomain;
  CookieCommons::GetBaseDomainFromHost(mTLDService, aCookie.host(), baseDomain);
  CookieKey key(baseDomain, aAttrs);
  CookiesList* cookiesList = nullptr;
  mCookiesMap.Get(key, &cookiesList);

  if (!cookiesList) {
    return;
  }

  for (uint32_t i = 0; i < cookiesList->Length(); i++) {
    Cookie* cookie = cookiesList->ElementAt(i);
    // bug 1858366: In the case that we are updating a stale cookie
    // from the content process: the parent process will signal
    // a batch deletion for the old cookie.
    // When received by the content process we should not remove
    // the new cookie since we have already updated the content
    // process cookies. So we also check the expiry here.
    if (cookie->Name().Equals(aCookie.name()) &&
        cookie->Host().Equals(aCookie.host()) &&
        cookie->Path().Equals(aCookie.path()) &&
        cookie->Expiry() <= aCookie.expiry()) {
      cookiesList->RemoveElementAt(i);
      break;
    }
  }
}

IPCResult CookieServiceChild::RecvAddCookie(const CookieStruct& aCookie,
                                            const OriginAttributes& aAttrs) {
  RefPtr<Cookie> cookie = Cookie::Create(aCookie, aAttrs);
  RecordDocumentCookie(cookie, aAttrs);

  // signal test code to check their cookie list
  nsCOMPtr<nsIObserverService> obsService = services::GetObserverService();
  if (obsService) {
    obsService->NotifyObservers(nullptr, "content-added-cookie", nullptr);
  }

  return IPC_OK();
}

IPCResult CookieServiceChild::RecvRemoveBatchDeletedCookies(
    nsTArray<CookieStruct>&& aCookiesList,
    nsTArray<OriginAttributes>&& aAttrsList) {
  MOZ_ASSERT(aCookiesList.Length() == aAttrsList.Length());
  for (uint32_t i = 0; i < aCookiesList.Length(); i++) {
    CookieStruct cookieStruct = aCookiesList.ElementAt(i);
    RemoveSingleCookie(cookieStruct, aAttrsList.ElementAt(i));
  }

  nsCOMPtr<nsIObserverService> obsService = services::GetObserverService();
  if (obsService) {
    obsService->NotifyObservers(nullptr, "content-batch-deleted-cookies",
                                nullptr);
  }
  return IPC_OK();
}

IPCResult CookieServiceChild::RecvTrackCookiesLoad(
    nsTArray<CookieStructTable>&& aCookiesListTable) {
  for (auto& entry : aCookiesListTable) {
    for (auto& cookieEntry : entry.cookies()) {
      RefPtr<Cookie> cookie = Cookie::Create(cookieEntry, entry.attrs());
      cookie->SetIsHttpOnly(false);
      RecordDocumentCookie(cookie, entry.attrs());
    }
  }

  nsCOMPtr<nsIObserverService> obsService = services::GetObserverService();
  if (obsService) {
    obsService->NotifyObservers(nullptr, "content-track-cookies-loaded",
                                nullptr);
  }

  return IPC_OK();
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
    cookiesList = mCookiesMap.GetOrInsertNew(key);
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
          cookie->RawSameSite() == aCookie->RawSameSite() &&
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
CookieServiceChild::GetCookieStringFromDocument(dom::Document* aDocument,
                                                nsACString& aCookieString) {
  NS_ENSURE_ARG(aDocument);

  aCookieString.Truncate();

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
  bool isCHIPS = aDocument->CookieJarSettings()->GetPartitionForeign() &&
                 StaticPrefs::network_cookie_CHIPS_enabled();
  bool documentHasStorageAccess = false;
  nsresult rv = aDocument->HasStorageAccessSync(documentHasStorageAccess);
  NS_ENSURE_SUCCESS(rv, rv);
  if (isCHIPS && documentHasStorageAccess) {
    /// Assert that the cookie principal is unpartitioned.
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

  for (auto& principal : principals) {
    if (!CookieCommons::IsSchemeSupported(principal)) {
      return NS_OK;
    }

    nsAutoCString baseDomain;
    rv = CookieCommons::GetBaseDomain(principal, baseDomain);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return NS_OK;
    }

    CookieKey key(baseDomain, principal->OriginAttributesRef());
    CookiesList* cookiesList = nullptr;
    mCookiesMap.Get(key, &cookiesList);

    if (!cookiesList) {
      continue;
    }

    nsAutoCString hostFromURI;
    rv = nsContentUtils::GetHostOrIPv6WithBrackets(principal, hostFromURI);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return NS_OK;
    }

    nsAutoCString pathFromURI;
    principal->GetFilePath(pathFromURI);

    bool isPotentiallyTrustworthy =
        principal->GetIsOriginPotentiallyTrustworthy();
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

      if (thirdParty && !CookieCommons::ShouldIncludeCrossSiteCookieForDocument(
                            cookie, aDocument)) {
        continue;
      }

      // do not display the cookie if it is secure and the host scheme isn't
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
    dom::Document* aDocument, const nsACString& aCookieString) {
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

  CookieKey key(baseDomain, attrs);
  CookiesList* cookies = mCookiesMap.Get(key);

  if (cookies) {
    // We need to see if the cookie we're setting would overwrite an httponly
    // or a secure one. This would not affect anything we send over the net
    // (those come from the parent, which already checks this),
    // but script could see an inconsistent view of things.

    // CHIPS - If the cookie has the "Partitioned" attribute set it will be
    // stored in the partitioned cookie jar.
    bool needPartitioned = StaticPrefs::network_cookie_CHIPS_enabled() &&
                           cookie->RawIsPartitioned();
    nsCOMPtr<nsIPrincipal> principal =
        needPartitioned ? aDocument->PartitionedPrincipal()
                        : aDocument->EffectiveCookiePrincipal();
    bool isPotentiallyTrustworthy =
        principal->GetIsOriginPotentiallyTrustworthy();

    for (uint32_t i = 0; i < cookies->Length(); ++i) {
      RefPtr<Cookie> existingCookie = cookies->ElementAt(i);
      if (existingCookie->Name().Equals(cookie->Name()) &&
          existingCookie->Host().Equals(cookie->Host()) &&
          existingCookie->Path().Equals(cookie->Path())) {
        // Can't overwrite an httponly cookie from a script context.
        if (existingCookie->IsHttpOnly()) {
          return NS_OK;
        }

        // prevent insecure cookie from overwriting a secure one in insecure
        // context.
        if (existingCookie->IsSecure() && !isPotentiallyTrustworthy) {
          return NS_OK;
        }
      }
    }
  }

  RecordDocumentCookie(cookie, attrs);

  if (CanSend()) {
    nsTArray<CookieStruct> cookiesToSend;
    cookiesToSend.AppendElement(cookie->ToIPC());

    // Asynchronously call the parent.
    dom::WindowGlobalChild* windowGlobalChild =
        aDocument->GetWindowGlobalChild();

    // If there is no WindowGlobalChild fall back to PCookieService SetCookies.
    if (NS_WARN_IF(!windowGlobalChild)) {
      SendSetCookies(baseDomain, attrs, documentURI, false, cookiesToSend);
      return NS_OK;
    }
    windowGlobalChild->SendSetCookies(baseDomain, attrs, documentURI, false,
                                      cookiesToSend);
  }

  return NS_OK;
}

NS_IMETHODIMP
CookieServiceChild::SetCookieStringFromHttp(nsIURI* aHostURI,
                                            const nsACString& aCookieString,
                                            nsIChannel* aChannel) {
  NS_ENSURE_ARG(aHostURI);
  NS_ENSURE_ARG(aChannel);

  if (!CookieCommons::IsSchemeSupported(aHostURI)) {
    return NS_OK;
  }

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

  OriginAttributes storagePrincipalOriginAttributes =
      loadInfo->GetOriginAttributes();
  StoragePrincipalHelper::PrepareEffectiveStoragePrincipalOriginAttributes(
      aChannel, storagePrincipalOriginAttributes);

  bool requireHostMatch;
  nsCString baseDomain;
  CookieCommons::GetBaseDomain(mTLDService, aHostURI, baseDomain,
                               requireHostMatch);

  nsCOMPtr<nsICookieJarSettings> cookieJarSettings =
      CookieCommons::GetCookieJarSettings(aChannel);

  nsCOMPtr<nsIConsoleReportCollector> crc = do_QueryInterface(aChannel);

  CookieStatus cookieStatus = CookieService::CheckPrefs(
      crc, cookieJarSettings, aHostURI,
      result.contains(ThirdPartyAnalysis::IsForeign),
      result.contains(ThirdPartyAnalysis::IsThirdPartyTrackingResource),
      result.contains(ThirdPartyAnalysis::IsThirdPartySocialTrackingResource),
      result.contains(ThirdPartyAnalysis::IsStorageAccessPermissionGranted),
      aCookieString,
      CountCookiesFromHashTable(baseDomain, storagePrincipalOriginAttributes),
      storagePrincipalOriginAttributes, &rejectedReason);

  if (cookieStatus != STATUS_ACCEPTED &&
      cookieStatus != STATUS_ACCEPT_SESSION) {
    return NS_OK;
  }

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

    // include sub-document navigations from cross-site to same-site
    // wrt top-level in our check for thirdparty-ness
    if (StaticPrefs::network_cookie_sameSite_crossSiteIframeSetCheck() &&
        !isForeignAndNotAddon &&
        loadInfo->GetExternalContentPolicyType() ==
            ExtContentPolicy::TYPE_SUBDOCUMENT) {
      bool triggeringPrincipalIsThirdParty = false;
      BasePrincipal::Cast(loadInfo->TriggeringPrincipal())
          ->IsThirdPartyURI(finalChannelURI, &triggeringPrincipalIsThirdParty);
      isForeignAndNotAddon |= triggeringPrincipalIsThirdParty;
    }
  }

  bool mustBePartitioned =
      isForeignAndNotAddon &&
      cookieJarSettings->GetCookieBehavior() ==
          nsICookieService::BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN &&
      !result.contains(ThirdPartyAnalysis::IsStorageAccessPermissionGranted);

  // CHIPS - The partitioned cookie jar is always available and it is always
  // possible to store cookies in it using the "Partitioned" attribute.
  // Prepare the partitioned principals OAs to enable possible partitioned
  // cookie storing from first-party or with StorageAccess.
  // Similar behavior to CookieService::SetCookieStringFromHttp().
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

  nsTArray<CookieStruct> cookiesToSend, partitionedCookiesToSend;
  bool moreCookies;
  do {
    CookieStruct cookieData;
    bool canSetCookie = false;
    moreCookies = CookieService::CanSetCookie(
        aHostURI, baseDomain, cookieData, requireHostMatch, cookieStatus,
        cookieString, true, isForeignAndNotAddon, mustBePartitioned,
        storagePrincipalOriginAttributes.mPrivateBrowsingId !=
            nsIScriptSecurityManager::DEFAULT_PRIVATE_BROWSING_ID,
        crc, canSetCookie);
    if (!canSetCookie) {
      continue;
    }

    // check permissions from site permission list.
    if (!CookieCommons::CheckCookiePermission(aChannel, cookieData)) {
      COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, aCookieString,
                        "cookie rejected by permission manager");
      constexpr auto CONSOLE_REJECTION_CATEGORY = "cookiesRejection"_ns;
      CookieLogging::LogMessageToConsole(
          crc, aHostURI, nsIScriptError::warningFlag,
          CONSOLE_REJECTION_CATEGORY, "CookieRejectedByPermissionManager"_ns,
          AutoTArray<nsString, 1>{
              NS_ConvertUTF8toUTF16(cookieData.name()),
          });
      CookieCommons::NotifyRejected(
          aHostURI, aChannel,
          nsIWebProgressListener::STATE_COOKIES_BLOCKED_BY_PERMISSION,
          OPERATION_WRITE);
      continue;
    }

    // CHIPS - If the partitioned attribute is set, store cookie in partitioned
    // cookie jar independent of context. If the cookies are stored in the
    // partitioned cookie jar anyway no special treatment of CHIPS cookies
    // necessary.
    bool needPartitioned =
        isCHIPS && cookieData.isPartitioned() && !isPartitionedPrincipal;
    nsTArray<CookieStruct>& cookiesToSendRef =
        needPartitioned ? partitionedCookiesToSend : cookiesToSend;
    OriginAttributes& cookieOriginAttributes =
        needPartitioned ? partitionedPrincipalOriginAttributes
                        : storagePrincipalOriginAttributes;
    // Assert that partitionedPrincipalOriginAttributes are initialized if used.
    MOZ_ASSERT_IF(
        needPartitioned,
        !partitionedPrincipalOriginAttributes.mPartitionKey.IsEmpty());

    RefPtr<Cookie> cookie = Cookie::Create(cookieData, cookieOriginAttributes);
    MOZ_ASSERT(cookie);

    cookie->SetLastAccessed(currentTimeInUsec);
    cookie->SetCreationTime(
        Cookie::GenerateUniqueCreationTime(currentTimeInUsec));

    RecordDocumentCookie(cookie, cookieOriginAttributes);
    cookiesToSendRef.AppendElement(cookieData);
  } while (moreCookies);

  // Asynchronously call the parent.
  if (CanSend()) {
    RefPtr<HttpChannelChild> httpChannelChild = do_QueryObject(aChannel);
    MOZ_ASSERT(httpChannelChild);
    if (!cookiesToSend.IsEmpty()) {
      httpChannelChild->SendSetCookies(baseDomain,
                                       storagePrincipalOriginAttributes,
                                       aHostURI, true, cookiesToSend);
    }
    if (!partitionedCookiesToSend.IsEmpty()) {
      httpChannelChild->SendSetCookies(
          baseDomain, partitionedPrincipalOriginAttributes, aHostURI, true,
          partitionedCookiesToSend);
    }
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
