/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/CookieService.h"
#include "mozilla/net/CookieServiceParent.h"
#include "mozilla/net/NeckoParent.h"

#include "mozilla/ipc/URIUtils.h"
#include "mozilla/StoragePrincipalHelper.h"
#include "mozIThirdPartyUtil.h"
#include "nsArrayUtils.h"
#include "nsIChannel.h"
#include "nsNetCID.h"

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
    auto cookie = static_cast<Cookie*>(xpcCookie.get());
    attrs = cookie->OriginAttributesRef();
    cookieStruct = cookie->ToIPC();
    if (cookie->IsHttpOnly()) {
      // Child only needs to exist if an HttpOnly cookie exists, not its value
      cookieStruct.value() = "";
    }
    cookieStructList.AppendElement(cookieStruct);
    attrsList.AppendElement(attrs);
  }
  Unused << SendRemoveBatchDeletedCookies(cookieStructList, attrsList);
}

void CookieServiceParent::RemoveAll() { Unused << SendRemoveAll(); }

void CookieServiceParent::RemoveCookie(nsICookie* aCookie) {
  auto cookie = static_cast<Cookie*>(aCookie);
  const OriginAttributes& attrs = cookie->OriginAttributesRef();
  CookieStruct cookieStruct = cookie->ToIPC();
  if (cookie->IsHttpOnly()) {
    cookieStruct.value() = "";
  }
  Unused << SendRemoveCookie(cookieStruct, attrs);
}

void CookieServiceParent::AddCookie(nsICookie* aCookie) {
  auto cookie = static_cast<Cookie*>(aCookie);
  const OriginAttributes& attrs = cookie->OriginAttributesRef();
  CookieStruct cookieStruct = cookie->ToIPC();
  if (cookie->IsHttpOnly()) {
    cookieStruct.value() = "";
  }
  Unused << SendAddCookie(cookieStruct, attrs);
}

void CookieServiceParent::TrackCookieLoad(nsIChannel* aChannel) {
  nsCOMPtr<nsIURI> uri;
  aChannel->GetURI(getter_AddRefs(uri));

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  OriginAttributes attrs = loadInfo->GetOriginAttributes();
  bool isSafeTopLevelNav = NS_IsSafeTopLevelNav(aChannel);
  bool aIsSameSiteForeign = NS_IsSameSiteForeign(aChannel, uri);

  StoragePrincipalHelper::PrepareOriginAttributes(aChannel, attrs);

  // Send matching cookies to Child.
  nsCOMPtr<mozIThirdPartyUtil> thirdPartyUtil;
  thirdPartyUtil = do_GetService(THIRDPARTYUTIL_CONTRACTID);

  uint32_t rejectedReason = 0;
  ThirdPartyAnalysisResult result = thirdPartyUtil->AnalyzeChannel(
      aChannel, false, nullptr, nullptr, &rejectedReason);

  nsTArray<Cookie*> foundCookieList;
  mCookieService->GetCookiesForURI(
      uri, aChannel, result.contains(ThirdPartyAnalysis::IsForeign),
      result.contains(ThirdPartyAnalysis::IsThirdPartyTrackingResource),
      result.contains(ThirdPartyAnalysis::IsThirdPartySocialTrackingResource),
      result.contains(ThirdPartyAnalysis::IsFirstPartyStorageAccessGranted),
      rejectedReason, isSafeTopLevelNav, aIsSameSiteForeign, false, attrs,
      foundCookieList);
  nsTArray<CookieStruct> matchingCookiesList;
  SerialializeCookieList(foundCookieList, matchingCookiesList);
  Unused << SendTrackCookiesLoad(matchingCookiesList, attrs);
}

// static
void CookieServiceParent::SerialializeCookieList(
    const nsTArray<Cookie*>& aFoundCookieList,
    nsTArray<CookieStruct>& aCookiesList) {
  for (uint32_t i = 0; i < aFoundCookieList.Length(); i++) {
    Cookie* cookie = aFoundCookieList.ElementAt(i);
    CookieStruct* cookieStruct = aCookiesList.AppendElement();
    *cookieStruct = cookie->ToIPC();
    if (cookie->IsHttpOnly()) {
      // Value only needs to exist if an HttpOnly cookie exists.
      cookieStruct->value() = "";
    }
  }
}

IPCResult CookieServiceParent::RecvPrepareCookieList(
    const URIParams& aHost, const bool& aIsForeign,
    const bool& aIsThirdPartyTrackingResource,
    const bool& aIsThirdPartySocialTrackingResource,
    const bool& aFirstPartyStorageAccessGranted,
    const uint32_t& aRejectedReason, const bool& aIsSafeTopLevelNav,
    const bool& aIsSameSiteForeign, const OriginAttributes& aAttrs) {
  nsCOMPtr<nsIURI> hostURI = DeserializeURI(aHost);

  // Send matching cookies to Child.
  nsTArray<Cookie*> foundCookieList;
  // Note: passing nullptr as aChannel to GetCookiesForURI() here is fine since
  // this argument is only used for proper reporting of cookie loads, but the
  // child process already does the necessary reporting in this case for us.
  mCookieService->GetCookiesForURI(
      hostURI, nullptr, aIsForeign, aIsThirdPartyTrackingResource,
      aIsThirdPartySocialTrackingResource, aFirstPartyStorageAccessGranted,
      aRejectedReason, aIsSafeTopLevelNav, aIsSameSiteForeign, false, aAttrs,
      foundCookieList);
  nsTArray<CookieStruct> matchingCookiesList;
  SerialializeCookieList(foundCookieList, matchingCookiesList);
  Unused << SendTrackCookiesLoad(matchingCookiesList, aAttrs);
  return IPC_OK();
}

void CookieServiceParent::ActorDestroy(ActorDestroyReason aWhy) {
  // Nothing needed here. Called right before destructor since this is a
  // non-refcounted class.
}

IPCResult CookieServiceParent::RecvSetCookies(
    const nsCString& aBaseDomain, const OriginAttributes& aOriginAttributes,
    const URIParams& aHost, bool aFromHttp,
    const nsTArray<CookieStruct>& aCookies) {
  if (!mCookieService) {
    return IPC_OK();
  }

  // Deserialize URI. Having a host URI is mandatory and should always be
  // provided by the child; thus we consider failure fatal.
  nsCOMPtr<nsIURI> hostURI = DeserializeURI(aHost);
  if (!hostURI) {
    return IPC_FAIL_NO_REASON(this);
  }

  // We set this to true while processing this cookie update, to make sure
  // we don't send it back to the same content process.
  mProcessingCookie = true;

  bool ok = mCookieService->SetCookiesFromIPC(aBaseDomain, aOriginAttributes,
                                              hostURI, aFromHttp, aCookies);

  mProcessingCookie = false;
  return ok ? IPC_OK() : IPC_FAIL(this, "Invalid cookie received.");
}

}  // namespace net
}  // namespace mozilla
