/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/CookieServiceParent.h"
#include "mozilla/dom/PContentParent.h"
#include "mozilla/net/NeckoParent.h"

#include "mozilla/BasePrincipal.h"
#include "mozilla/ipc/URIUtils.h"
#include "nsCookieService.h"
#include "nsIChannel.h"
#include "nsIScriptSecurityManager.h"
#include "nsIPrivateBrowsingChannel.h"
#include "nsNetCID.h"
#include "nsPrintfCString.h"
#include "SerializedLoadContext.h"

using namespace mozilla::ipc;
using mozilla::BasePrincipal;
using mozilla::OriginAttributes;
using mozilla::dom::PContentParent;
using mozilla::net::NeckoParent;

namespace {

// Ignore failures from this function, as they only affect whether we do or
// don't show a dialog box in private browsing mode if the user sets a pref.
void
CreateDummyChannel(nsIURI* aHostURI, OriginAttributes &aAttrs, bool aIsPrivate,
                   nsIChannel **aChannel)
{
  MOZ_ASSERT(aAttrs.mAppId != nsIScriptSecurityManager::UNKNOWN_APP_ID);

  nsCOMPtr<nsIPrincipal> principal =
    BasePrincipal::CreateCodebasePrincipal(aHostURI, aAttrs);
  if (!principal) {
    return;
  }

  nsCOMPtr<nsIURI> dummyURI;
  nsresult rv = NS_NewURI(getter_AddRefs(dummyURI), "about:blank");
  if (NS_FAILED(rv)) {
      return;
  }

  nsCOMPtr<nsIChannel> dummyChannel;
  NS_NewChannel(getter_AddRefs(dummyChannel), dummyURI, principal,
                nsILoadInfo::SEC_NORMAL, nsIContentPolicy::TYPE_INVALID);
  nsCOMPtr<nsIPrivateBrowsingChannel> pbChannel = do_QueryInterface(dummyChannel);
  if (!pbChannel) {
    return;
  }

  pbChannel->SetPrivate(aIsPrivate);
  dummyChannel.forget(aChannel);
  return;
}

}

namespace mozilla {
namespace net {

MOZ_WARN_UNUSED_RESULT
bool
CookieServiceParent::GetOriginAttributesFromParams(const IPC::SerializedLoadContext &aLoadContext,
                                                   OriginAttributes& aAttrs,
                                                   bool& aIsPrivate)
{
  aIsPrivate = false;

  const char* error = NeckoParent::GetValidatedAppInfo(aLoadContext,
                                                       Manager()->Manager(),
                                                       aAttrs);
  if (error) {
    NS_WARNING(nsPrintfCString("CookieServiceParent: GetOriginAttributesFromParams: "
                               "FATAL error: %s: KILLING CHILD PROCESS\n",
                               error).get());
    return false;
  }

  if (aLoadContext.IsPrivateBitValid()) {
    aIsPrivate = aLoadContext.mUsePrivateBrowsing;
  }
  return true;
}

CookieServiceParent::CookieServiceParent()
{
  // Instantiate the cookieservice via the service manager, so it sticks around
  // until shutdown.
  nsCOMPtr<nsICookieService> cs = do_GetService(NS_COOKIESERVICE_CONTRACTID);

  // Get the nsCookieService instance directly, so we can call internal methods.
  mCookieService =
    already_AddRefed<nsCookieService>(nsCookieService::GetSingleton());
  NS_ASSERTION(mCookieService, "couldn't get nsICookieService");
}

CookieServiceParent::~CookieServiceParent()
{
}

void
CookieServiceParent::ActorDestroy(ActorDestroyReason aWhy)
{
  // Implement me! Bug 1005181
}

bool
CookieServiceParent::RecvGetCookieString(const URIParams& aHost,
                                         const bool& aIsForeign,
                                         const bool& aFromHttp,
                                         const IPC::SerializedLoadContext&
                                               aLoadContext,
                                         nsCString* aResult)
{
  if (!mCookieService)
    return true;

  // Deserialize URI. Having a host URI is mandatory and should always be
  // provided by the child; thus we consider failure fatal.
  nsCOMPtr<nsIURI> hostURI = DeserializeURI(aHost);
  if (!hostURI)
    return false;

  OriginAttributes attrs;
  bool isPrivate;
  bool valid = GetOriginAttributesFromParams(aLoadContext, attrs, isPrivate);
  if (!valid) {
    return false;
  }

  mCookieService->GetCookieStringInternal(hostURI, aIsForeign, aFromHttp, attrs,
                                          isPrivate, *aResult);
  return true;
}

bool
CookieServiceParent::RecvSetCookieString(const URIParams& aHost,
                                         const bool& aIsForeign,
                                         const nsCString& aCookieString,
                                         const nsCString& aServerTime,
                                         const bool& aFromHttp,
                                         const IPC::SerializedLoadContext&
                                               aLoadContext)
{
  if (!mCookieService)
    return true;

  // Deserialize URI. Having a host URI is mandatory and should always be
  // provided by the child; thus we consider failure fatal.
  nsCOMPtr<nsIURI> hostURI = DeserializeURI(aHost);
  if (!hostURI)
    return false;

  OriginAttributes attrs;
  bool isPrivate;
  bool valid = GetOriginAttributesFromParams(aLoadContext, attrs, isPrivate);
  if (!valid) {
    return false;
  }

  // This is a gross hack. We've already computed everything we need to know
  // for whether to set this cookie or not, but we need to communicate all of
  // this information through to nsICookiePermission, which indirectly
  // computes the information from the channel. We only care about the
  // aIsPrivate argument as nsCookieService::SetCookieStringInternal deals
  // with aIsForeign before we have to worry about nsCookiePermission trying
  // to use the channel to inspect it.
  nsCOMPtr<nsIChannel> dummyChannel;
  CreateDummyChannel(hostURI, attrs, isPrivate, getter_AddRefs(dummyChannel));

  // NB: dummyChannel could be null if something failed in CreateDummyChannel.
  nsDependentCString cookieString(aCookieString, 0);
  mCookieService->SetCookieStringInternal(hostURI, aIsForeign, cookieString,
                                          aServerTime, aFromHttp, attrs,
                                          isPrivate, dummyChannel);
  return true;
}

mozilla::ipc::IProtocol*
CookieServiceParent::CloneProtocol(Channel* aChannel,
                                   mozilla::ipc::ProtocolCloneContext* aCtx)
{
  NeckoParent* manager = aCtx->GetNeckoParent();
  nsAutoPtr<PCookieServiceParent> actor(manager->AllocPCookieServiceParent());
  if (!actor || !manager->RecvPCookieServiceConstructor(actor)) {
    return nullptr;
  }
  return actor.forget();
}

} // namespace net
} // namespace mozilla

