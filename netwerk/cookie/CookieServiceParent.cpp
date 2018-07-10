/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/CookieServiceParent.h"
#include "mozilla/dom/PContentParent.h"
#include "mozilla/net/NeckoParent.h"

#include "mozilla/BasePrincipal.h"
#include "mozilla/ipc/URIUtils.h"
#include "nsArrayUtils.h"
#include "nsCookieService.h"
#include "nsIChannel.h"
#include "nsIEffectiveTLDService.h"
#include "nsIScriptSecurityManager.h"
#include "nsIPrivateBrowsingChannel.h"
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
void
CreateDummyChannel(nsIURI* aHostURI, nsIURI* aChannelURI,
                   OriginAttributes& aAttrs, nsIChannel** aChannel)
{
  nsCOMPtr<nsIPrincipal> principal =
    BasePrincipal::CreateCodebasePrincipal(aHostURI, aAttrs);
  if (!principal) {
    return;
  }

  // The following channel is never openend, so it does not matter what
  // securityFlags we pass; let's follow the principle of least privilege.
  nsCOMPtr<nsIChannel> dummyChannel;
  NS_NewChannel(getter_AddRefs(dummyChannel), aChannelURI, principal,
                nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_IS_BLOCKED,
                nsIContentPolicy::TYPE_INVALID);
  nsCOMPtr<nsIPrivateBrowsingChannel> pbChannel = do_QueryInterface(dummyChannel);
  if (!pbChannel) {
    return;
  }

  pbChannel->SetPrivate(aAttrs.mPrivateBrowsingId > 0);
  dummyChannel.forget(aChannel);
}

}

namespace mozilla {
namespace net {

CookieServiceParent::CookieServiceParent()
{
  // Instantiate the cookieservice via the service manager, so it sticks around
  // until shutdown.
  nsCOMPtr<nsICookieService> cs = do_GetService(NS_COOKIESERVICE_CONTRACTID);

  // Get the nsCookieService instance directly, so we can call internal methods.
  mCookieService = nsCookieService::GetSingleton();
  NS_ASSERTION(mCookieService, "couldn't get nsICookieService");
  mProcessingCookie = false;
}

void
GetInfoFromCookie(nsCookie         *aCookie,
                  CookieStruct     &aCookieStruct)
{
  aCookieStruct.name() = aCookie->Name();
  aCookieStruct.value() = aCookie->Value();
  aCookieStruct.host() = aCookie->Host();
  aCookieStruct.path() = aCookie->Path();
  aCookieStruct.expiry() = aCookie->Expiry();
  aCookieStruct.lastAccessed() = aCookie->LastAccessed();
  aCookieStruct.creationTime() = aCookie->CreationTime();
  aCookieStruct.isSession() = aCookie->IsSession();
  aCookieStruct.isSecure() = aCookie->IsSecure();
  aCookieStruct.isHttpOnly() = aCookie->IsHttpOnly();
  aCookieStruct.sameSite() = aCookie->SameSite();
}

void
CookieServiceParent::RemoveBatchDeletedCookies(nsIArray *aCookieList) {
  uint32_t len = 0;
  aCookieList->GetLength(&len);
  OriginAttributes attrs;
  CookieStruct cookieStruct;
  nsTArray<CookieStruct> cookieStructList;
  nsTArray<OriginAttributes> attrsList;
  for (uint32_t i = 0; i < len; i++) {
    nsCOMPtr<nsICookie> xpcCookie = do_QueryElementAt(aCookieList, i);
    auto cookie = static_cast<nsCookie*>(xpcCookie.get());
    attrs = cookie->OriginAttributesRef();
    GetInfoFromCookie(cookie, cookieStruct);
    if (!cookie->IsHttpOnly()) {
      cookieStructList.AppendElement(cookieStruct);
      attrsList.AppendElement(attrs);
    }
  }
  Unused << SendRemoveBatchDeletedCookies(cookieStructList, attrsList);
}

void
CookieServiceParent::RemoveAll()
{
  Unused << SendRemoveAll();
}

void
CookieServiceParent::RemoveCookie(nsICookie *aCookie)
{
  auto cookie = static_cast<nsCookie*>(aCookie);
  OriginAttributes attrs = cookie->OriginAttributesRef();
  CookieStruct cookieStruct;
  GetInfoFromCookie(cookie, cookieStruct);
  if (!cookie->IsHttpOnly()) {
    Unused << SendRemoveCookie(cookieStruct, attrs);
  }
}

void
CookieServiceParent::AddCookie(nsICookie *aCookie)
{
  auto cookie = static_cast<nsCookie*>(aCookie);
  OriginAttributes attrs = cookie->OriginAttributesRef();
  CookieStruct cookieStruct;
  GetInfoFromCookie(cookie, cookieStruct);
  Unused << SendAddCookie(cookieStruct, attrs);
}

void
CookieServiceParent::TrackCookieLoad(nsIChannel *aChannel)
{
  nsCOMPtr<nsIURI> uri;
  aChannel->GetURI(getter_AddRefs(uri));

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->GetLoadInfo();
  mozilla::OriginAttributes attrs;
  if (loadInfo) {
    attrs = loadInfo->GetOriginAttributes();
  }
  bool isSafeTopLevelNav = NS_IsSafeTopLevelNav(aChannel);
  bool aIsSameSiteForeign = NS_IsSameSiteForeign(aChannel, uri);

  // Send matching cookies to Child.
  nsCOMPtr<mozIThirdPartyUtil> thirdPartyUtil;
  thirdPartyUtil = do_GetService(THIRDPARTYUTIL_CONTRACTID);
  bool isForeign = true;
  thirdPartyUtil->IsThirdPartyChannel(aChannel, uri, &isForeign);

  bool isTrackingResource = false;
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aChannel);
  if (httpChannel) {
    isTrackingResource = httpChannel->GetIsTrackingResource();
  }

  bool storageAccessGranted = false;
  if (loadInfo && loadInfo->IsFirstPartyStorageAccessGrantedFor(uri)) {
    storageAccessGranted = true;
  }

  nsTArray<nsCookie*> foundCookieList;
  mCookieService->GetCookiesForURI(uri, isForeign, isTrackingResource,
                                   storageAccessGranted, isSafeTopLevelNav,
                                   aIsSameSiteForeign, false, attrs,
                                   foundCookieList);
  nsTArray<CookieStruct> matchingCookiesList;
  SerialializeCookieList(foundCookieList, matchingCookiesList, uri);
  Unused << SendTrackCookiesLoad(matchingCookiesList, attrs);
}

void
CookieServiceParent::SerialializeCookieList(const nsTArray<nsCookie*> &aFoundCookieList,
                                            nsTArray<CookieStruct>    &aCookiesList,
                                            nsIURI                    *aHostURI)
{
  for (uint32_t i = 0; i < aFoundCookieList.Length(); i++) {
    nsCookie *cookie = aFoundCookieList.ElementAt(i);
    CookieStruct* cookieStruct = aCookiesList.AppendElement();
    cookieStruct->name() = cookie->Name();
    cookieStruct->value() = cookie->Value();
    cookieStruct->host() = cookie->Host();
    cookieStruct->path() = cookie->Path();
    cookieStruct->expiry() = cookie->Expiry();
    cookieStruct->lastAccessed() = cookie->LastAccessed();
    cookieStruct->creationTime() = cookie->CreationTime();
    cookieStruct->isSession() = cookie->IsSession();
    cookieStruct->isSecure() = cookie->IsSecure();
    cookieStruct->sameSite() = cookie->SameSite();
  }
}

mozilla::ipc::IPCResult
CookieServiceParent::RecvPrepareCookieList(const URIParams        &aHost,
                                           const bool             &aIsForeign,
                                           const bool             &aIsTrackingResource,
                                           const bool             &aFirstPartyStorageAccessGranted,
                                           const bool             &aIsSafeTopLevelNav,
                                           const bool             &aIsSameSiteForeign,
                                           const OriginAttributes &aAttrs)
{
  nsCOMPtr<nsIURI> hostURI = DeserializeURI(aHost);

  // Send matching cookies to Child.
  nsTArray<nsCookie*> foundCookieList;
  mCookieService->GetCookiesForURI(hostURI, aIsForeign, aIsTrackingResource,
                                   aFirstPartyStorageAccessGranted, aIsSafeTopLevelNav,
                                   aIsSameSiteForeign, false, aAttrs,
                                   foundCookieList);
  nsTArray<CookieStruct> matchingCookiesList;
  SerialializeCookieList(foundCookieList, matchingCookiesList, hostURI);
  Unused << SendTrackCookiesLoad(matchingCookiesList, aAttrs);
  return IPC_OK();
}

void
CookieServiceParent::ActorDestroy(ActorDestroyReason aWhy)
{
  // Nothing needed here. Called right before destructor since this is a
  // non-refcounted class.
}

mozilla::ipc::IPCResult
CookieServiceParent::RecvGetCookieString(const URIParams& aHost,
                                         const bool& aIsForeign,
                                         const bool& aIsTrackingResource,
                                         const bool& aFirstPartyStorageAccessGranted,
                                         const bool& aIsSafeTopLevelNav,
                                         const bool& aIsSameSiteForeign,
                                         const OriginAttributes& aAttrs,
                                         nsCString* aResult)
{
  if (!mCookieService)
    return IPC_OK();

  // Deserialize URI. Having a host URI is mandatory and should always be
  // provided by the child; thus we consider failure fatal.
  nsCOMPtr<nsIURI> hostURI = DeserializeURI(aHost);
  if (!hostURI)
    return IPC_FAIL_NO_REASON(this);
  mCookieService->GetCookieStringInternal(hostURI, aIsForeign, aIsTrackingResource,
                                          aFirstPartyStorageAccessGranted, aIsSafeTopLevelNav,
                                          aIsSameSiteForeign, false, aAttrs, *aResult);
  return IPC_OK();
}

mozilla::ipc::IPCResult
CookieServiceParent::RecvSetCookieString(const URIParams& aHost,
                                         const URIParams& aChannelURI,
                                         const bool& aIsForeign,
                                         const bool& aIsTrackingResource,
                                         const bool& aFirstPartyStorageAccessGranted,
                                         const nsCString& aCookieString,
                                         const nsCString& aServerTime,
                                         const OriginAttributes& aAttrs,
                                         const bool& aFromHttp)
{
  if (!mCookieService)
    return IPC_OK();

  // Deserialize URI. Having a host URI is mandatory and should always be
  // provided by the child; thus we consider failure fatal.
  nsCOMPtr<nsIURI> hostURI = DeserializeURI(aHost);
  if (!hostURI)
    return IPC_FAIL_NO_REASON(this);

  nsCOMPtr<nsIURI> channelURI = DeserializeURI(aChannelURI);
  if (!channelURI)
    return IPC_FAIL_NO_REASON(this);

  // This is a gross hack. We've already computed everything we need to know
  // for whether to set this cookie or not, but we need to communicate all of
  // this information through to nsICookiePermission, which indirectly
  // computes the information from the channel. We only care about the
  // aIsPrivate argument as nsCookieService::SetCookieStringInternal deals
  // with aIsForeign before we have to worry about nsCookiePermission trying
  // to use the channel to inspect it.
  nsCOMPtr<nsIChannel> dummyChannel;
  CreateDummyChannel(hostURI, channelURI,
                     const_cast<OriginAttributes&>(aAttrs),
                     getter_AddRefs(dummyChannel));

  // NB: dummyChannel could be null if something failed in CreateDummyChannel.
  nsDependentCString cookieString(aCookieString, 0);

  // We set this to true while processing this cookie update, to make sure
  // we don't send it back to the same content process.
  mProcessingCookie = true;
  mCookieService->SetCookieStringInternal(hostURI, aIsForeign,
                                          aIsTrackingResource,
                                          aFirstPartyStorageAccessGranted,
                                          cookieString, aServerTime, aFromHttp,
                                          aAttrs, dummyChannel);
  mProcessingCookie = false;
  return IPC_OK();
}

} // namespace net
} // namespace mozilla

