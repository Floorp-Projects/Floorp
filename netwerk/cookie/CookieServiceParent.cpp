/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/CookieService.h"
#include "mozilla/net/CookieServiceParent.h"
#include "mozilla/dom/PContentParent.h"
#include "mozilla/net/NeckoParent.h"

#include "mozilla/BasePrincipal.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/StoragePrincipalHelper.h"
#include "nsArrayUtils.h"
#include "nsIChannel.h"
#include "nsNetCID.h"
#include "nsPrintfCString.h"

using namespace mozilla::ipc;
using mozilla::BasePrincipal;
using mozilla::OriginAttributes;
using mozilla::dom::PContentParent;
using mozilla::net::NeckoParent;

namespace {

// Ignore failures from this function, as they only affect whether we do or
// don't show a dialog box in private browsing mode if the user sets a pref.
nsresult CreateDummyChannel(nsIURI* aHostURI, nsILoadInfo* aLoadInfo,
                            nsIChannel** aChannel) {
  nsCOMPtr<nsIChannel> dummyChannel;
  nsresult rv =
      NS_NewChannelInternal(getter_AddRefs(dummyChannel), aHostURI, aLoadInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  dummyChannel.forget(aChannel);
  return NS_OK;
}

}  // namespace

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
  OriginAttributes attrs = cookie->OriginAttributesRef();
  CookieStruct cookieStruct = cookie->ToIPC();
  if (cookie->IsHttpOnly()) {
    cookieStruct.value() = "";
  }
  Unused << SendRemoveCookie(cookieStruct, attrs);
}

void CookieServiceParent::AddCookie(nsICookie* aCookie) {
  auto cookie = static_cast<Cookie*>(aCookie);
  OriginAttributes attrs = cookie->OriginAttributesRef();
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
  mozilla::OriginAttributes attrs = loadInfo->GetOriginAttributes();
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
  SerialializeCookieList(foundCookieList, matchingCookiesList, uri);
  Unused << SendTrackCookiesLoad(matchingCookiesList, attrs);
}

void CookieServiceParent::SerialializeCookieList(
    const nsTArray<Cookie*>& aFoundCookieList,
    nsTArray<CookieStruct>& aCookiesList, nsIURI* aHostURI) {
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

mozilla::ipc::IPCResult CookieServiceParent::RecvPrepareCookieList(
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
  SerialializeCookieList(foundCookieList, matchingCookiesList, hostURI);
  Unused << SendTrackCookiesLoad(matchingCookiesList, aAttrs);
  return IPC_OK();
}

void CookieServiceParent::ActorDestroy(ActorDestroyReason aWhy) {
  // Nothing needed here. Called right before destructor since this is a
  // non-refcounted class.
}

mozilla::ipc::IPCResult CookieServiceParent::RecvSetCookieString(
    const URIParams& aHost, const Maybe<URIParams>& aChannelURI,
    const Maybe<LoadInfoArgs>& aLoadInfoArgs, const bool& aIsForeign,
    const bool& aIsThirdPartyTrackingResource,
    const bool& aIsThirdPartySocialTrackingResource,
    const bool& aFirstPartyStorageAccessGranted,
    const uint32_t& aRejectedReason, const OriginAttributes& aAttrs,
    const nsCString& aCookieString, const nsCString& aServerTime,
    const bool& aFromHttp) {
  if (!mCookieService) return IPC_OK();

  // Deserialize URI. Having a host URI is mandatory and should always be
  // provided by the child; thus we consider failure fatal.
  nsCOMPtr<nsIURI> hostURI = DeserializeURI(aHost);
  if (!hostURI) return IPC_FAIL_NO_REASON(this);

  nsCOMPtr<nsIURI> channelURI = DeserializeURI(aChannelURI);

  nsCOMPtr<nsILoadInfo> loadInfo;
  Unused << NS_WARN_IF(NS_FAILED(
      LoadInfoArgsToLoadInfo(aLoadInfoArgs, getter_AddRefs(loadInfo))));

  // This is a gross hack. We've already computed everything we need to know
  // for whether to set this cookie or not, but we need to communicate all of
  // this information through to nsICookiePermission, which indirectly
  // computes the information from the channel. We only care about the
  // aIsPrivate argument as CookieService::SetCookieStringInternal deals
  // with aIsForeign before we have to worry about CookiePermission trying
  // to use the channel to inspect it.
  nsCOMPtr<nsIChannel> dummyChannel;
  nsresult rv =
      CreateDummyChannel(channelURI, loadInfo, getter_AddRefs(dummyChannel));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // No reason to kill the content process.
    return IPC_OK();
  }

  // NB: dummyChannel could be null if something failed in CreateDummyChannel.
  nsDependentCString cookieString(aCookieString, 0);

  // We set this to true while processing this cookie update, to make sure
  // we don't send it back to the same content process.
  mProcessingCookie = true;
  mCookieService->SetCookieStringInternal(
      hostURI, aIsForeign, aIsThirdPartyTrackingResource,
      aIsThirdPartySocialTrackingResource, aFirstPartyStorageAccessGranted,
      aRejectedReason, cookieString, aServerTime, aFromHttp, aAttrs,
      dummyChannel);
  mProcessingCookie = false;
  return IPC_OK();
}

}  // namespace net
}  // namespace mozilla
