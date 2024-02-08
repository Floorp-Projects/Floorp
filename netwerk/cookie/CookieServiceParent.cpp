/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CookieCommons.h"
#include "CookieLogging.h"
#include "mozilla/net/CookieService.h"
#include "mozilla/net/CookieServiceParent.h"
#include "mozilla/net/NeckoParent.h"

#include "mozilla/ipc/URIUtils.h"
#include "mozilla/StoragePrincipalHelper.h"
#include "mozIThirdPartyUtil.h"
#include "nsArrayUtils.h"
#include "nsIChannel.h"
#include "nsIEffectiveTLDService.h"
#include "nsNetCID.h"
#include "nsMixedContentBlocker.h"

using namespace mozilla::ipc;

namespace mozilla {
namespace net {

CookieServiceParent::CookieServiceParent() {
  // Instantiate the cookieservice via the service manager, so it sticks around
  // until shutdown.
  nsCOMPtr<nsICookieService> cs = do_GetService(NS_COOKIESERVICE_CONTRACTID);

  // Get the CookieService instance directly, so we can call internal methods.
  mCookieService = CookieService::GetSingleton();
  NS_ASSERTION(mCookieService, "couldn't get nsICookieService");

  mTLDService = do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID);
  MOZ_ALWAYS_TRUE(mTLDService);

  mProcessingCookie = false;
}

void CookieServiceParent::RemoveBatchDeletedCookies(nsIArray* aCookieList) {
  uint32_t len = 0;
  aCookieList->GetLength(&len);
  OriginAttributes attrs;
  CookieStruct cookieStruct;
  nsTArray<CookieStruct> cookieStructList;
  nsTArray<OriginAttributes> attrsList;
  for (uint32_t i = 0; i < len; i++) {
    nsCOMPtr<nsICookie> xpcCookie = do_QueryElementAt(aCookieList, i);
    const auto& cookie = xpcCookie->AsCookie();
    attrs = cookie.OriginAttributesRef();
    cookieStruct = cookie.ToIPC();

    // Child only needs to know HttpOnly cookies exists, not its value
    // Same for Secure cookies going to a process for an insecure site.
    if (cookie.IsHttpOnly() || !InsecureCookieOrSecureOrigin(cookie)) {
      cookieStruct.value() = "";
    }
    cookieStructList.AppendElement(cookieStruct);
    attrsList.AppendElement(attrs);
  }
  Unused << SendRemoveBatchDeletedCookies(cookieStructList, attrsList);
}

void CookieServiceParent::RemoveAll() { Unused << SendRemoveAll(); }

void CookieServiceParent::RemoveCookie(const Cookie& cookie) {
  const OriginAttributes& attrs = cookie.OriginAttributesRef();
  CookieStruct cookieStruct = cookie.ToIPC();

  // Child only needs to know HttpOnly cookies exists, not its value
  // Same for Secure cookies going to a process for an insecure site.
  if (cookie.IsHttpOnly() || !InsecureCookieOrSecureOrigin(cookie)) {
    cookieStruct.value() = "";
  }
  Unused << SendRemoveCookie(cookieStruct, attrs);
}

void CookieServiceParent::AddCookie(const Cookie& cookie) {
  const OriginAttributes& attrs = cookie.OriginAttributesRef();
  CookieStruct cookieStruct = cookie.ToIPC();

  // Child only needs to know HttpOnly cookies exists, not its value
  // Same for Secure cookies going to a process for an insecure site.
  if (cookie.IsHttpOnly() || !InsecureCookieOrSecureOrigin(cookie)) {
    cookieStruct.value() = "";
  }
  Unused << SendAddCookie(cookieStruct, attrs);
}

bool CookieServiceParent::ContentProcessHasCookie(const Cookie& cookie) {
  nsCString baseDomain;
  if (NS_WARN_IF(NS_FAILED(CookieCommons::GetBaseDomainFromHost(
          mTLDService, cookie.Host(), baseDomain)))) {
    return false;
  }

  CookieKey cookieKey(baseDomain, cookie.OriginAttributesRef());
  return mCookieKeysInContent.MaybeGet(cookieKey).isSome();
}

bool CookieServiceParent::InsecureCookieOrSecureOrigin(const Cookie& cookie) {
  nsCString baseDomain;
  // CookieStorage notifications triggering this won't fail to get base domain
  MOZ_ALWAYS_SUCCEEDS(CookieCommons::GetBaseDomainFromHost(
      mTLDService, cookie.Host(), baseDomain));

  // cookie is insecure or cookie is associated with a secure-origin process
  CookieKey cookieKey(baseDomain, cookie.OriginAttributesRef());
  if (Maybe<bool> allowSecure = mCookieKeysInContent.MaybeGet(cookieKey)) {
    return (!cookie.IsSecure() || *allowSecure);
  }
  return false;
}

void CookieServiceParent::TrackCookieLoad(nsIChannel* aChannel) {
  nsCOMPtr<nsIURI> uri;
  aChannel->GetURI(getter_AddRefs(uri));

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  OriginAttributes attrs = loadInfo->GetOriginAttributes();
  bool isSafeTopLevelNav = CookieCommons::IsSafeTopLevelNav(aChannel);
  bool hadCrossSiteRedirects = false;
  bool isSameSiteForeign =
      CookieCommons::IsSameSiteForeign(aChannel, uri, &hadCrossSiteRedirects);

  StoragePrincipalHelper::PrepareEffectiveStoragePrincipalOriginAttributes(
      aChannel, attrs);

  nsCOMPtr<mozIThirdPartyUtil> thirdPartyUtil;
  thirdPartyUtil = do_GetService(THIRDPARTYUTIL_CONTRACTID);

  uint32_t rejectedReason = 0;
  ThirdPartyAnalysisResult result = thirdPartyUtil->AnalyzeChannel(
      aChannel, false, nullptr, nullptr, &rejectedReason);

  UpdateCookieInContentList(uri, attrs);

  // Send matching cookies to Child.
  nsTArray<Cookie*> foundCookieList;
  mCookieService->GetCookiesForURI(
      uri, aChannel, result.contains(ThirdPartyAnalysis::IsForeign),
      result.contains(ThirdPartyAnalysis::IsThirdPartyTrackingResource),
      result.contains(ThirdPartyAnalysis::IsThirdPartySocialTrackingResource),
      result.contains(ThirdPartyAnalysis::IsStorageAccessPermissionGranted),
      rejectedReason, isSafeTopLevelNav, isSameSiteForeign,
      hadCrossSiteRedirects, false, true, attrs, foundCookieList);
  nsTArray<CookieStruct> matchingCookiesList;
  SerializeCookieList(foundCookieList, matchingCookiesList, uri);
  Unused << SendTrackCookiesLoad(matchingCookiesList, attrs);
}

// we append outgoing cookie info into a list here so the ContentParent can
// filter cookies passing to unnecessary ContentProcesses
void CookieServiceParent::UpdateCookieInContentList(
    nsIURI* uri, const OriginAttributes& originAttrs) {
  nsCString baseDomain;
  bool requireAHostMatch = false;

  // prevent malformed urls from being added to the cookie list
  if (NS_WARN_IF(NS_FAILED(CookieCommons::GetBaseDomain(
          mTLDService, uri, baseDomain, requireAHostMatch)))) {
    return;
  }

  CookieKey cookieKey(baseDomain, originAttrs);
  bool& allowSecure = mCookieKeysInContent.LookupOrInsert(cookieKey, false);
  allowSecure =
      allowSecure || nsMixedContentBlocker::IsPotentiallyTrustworthyOrigin(uri);
}

// static
void CookieServiceParent::SerializeCookieList(
    const nsTArray<Cookie*>& aFoundCookieList,
    nsTArray<CookieStruct>& aCookiesList, nsIURI* aHostURI) {
  for (uint32_t i = 0; i < aFoundCookieList.Length(); i++) {
    Cookie* cookie = aFoundCookieList.ElementAt(i);
    CookieStruct* cookieStruct = aCookiesList.AppendElement();
    *cookieStruct = cookie->ToIPC();

    // clear http-only cookie values
    if (cookie->IsHttpOnly()) {
      // Value only needs to exist if an HttpOnly cookie exists.
      cookieStruct->value() = "";
    }

    // clear secure cookie values in insecure context
    bool potentiallyTurstworthy =
        nsMixedContentBlocker::IsPotentiallyTrustworthyOrigin(aHostURI);
    if (cookie->IsSecure() && !potentiallyTurstworthy) {
      cookieStruct->value() = "";
    }
  }
}

IPCResult CookieServiceParent::RecvPrepareCookieList(
    nsIURI* aHost, const bool& aIsForeign,
    const bool& aIsThirdPartyTrackingResource,
    const bool& aIsThirdPartySocialTrackingResource,
    const bool& aStorageAccessPermissionGranted,
    const uint32_t& aRejectedReason, const bool& aIsSafeTopLevelNav,
    const bool& aIsSameSiteForeign, const bool& aHadCrossSiteRedirects,
    const OriginAttributes& aAttrs) {
  // Send matching cookies to Child.
  if (!aHost) {
    return IPC_FAIL(this, "aHost must not be null");
  }

  // we append outgoing cookie info into a list here so the ContentParent can
  // filter cookies that do not need to go to certain ContentProcesses
  UpdateCookieInContentList(aHost, aAttrs);

  nsTArray<Cookie*> foundCookieList;
  // Note: passing nullptr as aChannel to GetCookiesForURI() here is fine since
  // this argument is only used for proper reporting of cookie loads, but the
  // child process already does the necessary reporting in this case for us.
  mCookieService->GetCookiesForURI(
      aHost, nullptr, aIsForeign, aIsThirdPartyTrackingResource,
      aIsThirdPartySocialTrackingResource, aStorageAccessPermissionGranted,
      aRejectedReason, aIsSafeTopLevelNav, aIsSameSiteForeign,
      aHadCrossSiteRedirects, false, true, aAttrs, foundCookieList);
  nsTArray<CookieStruct> matchingCookiesList;
  SerializeCookieList(foundCookieList, matchingCookiesList, aHost);
  Unused << SendTrackCookiesLoad(matchingCookiesList, aAttrs);
  return IPC_OK();
}

void CookieServiceParent::ActorDestroy(ActorDestroyReason aWhy) {
  // Nothing needed here. Called right before destructor since this is a
  // non-refcounted class.
}

IPCResult CookieServiceParent::RecvSetCookies(
    const nsCString& aBaseDomain, const OriginAttributes& aOriginAttributes,
    nsIURI* aHost, bool aFromHttp, const nsTArray<CookieStruct>& aCookies) {
  return SetCookies(aBaseDomain, aOriginAttributes, aHost, aFromHttp, aCookies);
}

IPCResult CookieServiceParent::SetCookies(
    const nsCString& aBaseDomain, const OriginAttributes& aOriginAttributes,
    nsIURI* aHost, bool aFromHttp, const nsTArray<CookieStruct>& aCookies,
    dom::BrowsingContext* aBrowsingContext) {
  if (!mCookieService) {
    return IPC_OK();
  }

  // Deserialize URI. Having a host URI is mandatory and should always be
  // provided by the child; thus we consider failure fatal.
  if (!aHost) {
    return IPC_FAIL(this, "aHost must not be null");
  }

  // We set this to true while processing this cookie update, to make sure
  // we don't send it back to the same content process.
  mProcessingCookie = true;

  bool ok =
      mCookieService->SetCookiesFromIPC(aBaseDomain, aOriginAttributes, aHost,
                                        aFromHttp, aCookies, aBrowsingContext);
  mProcessingCookie = false;
  return ok ? IPC_OK() : IPC_FAIL(this, "Invalid cookie received.");
}

}  // namespace net
}  // namespace mozilla
