/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/CookieServiceParent.h"
#include "mozilla/dom/PContentParent.h"
#include "mozilla/net/NeckoParent.h"

#include "mozilla/ipc/URIUtils.h"
#include "nsCookieService.h"
#include "nsNetUtil.h"
#include "nsPrintfCString.h"

using namespace mozilla::ipc;
using mozilla::dom::PContentParent;
using mozilla::net::NeckoParent;

namespace mozilla {
namespace net {

MOZ_WARN_UNUSED_RESULT
bool
CookieServiceParent::GetAppInfoFromParams(const IPC::SerializedLoadContext &aLoadContext,
                                          uint32_t& aAppId,
                                          bool& aIsInBrowserElement,
                                          bool& aIsPrivate)
{
  aAppId = NECKO_NO_APP_ID;
  aIsInBrowserElement = false;
  aIsPrivate = false;

  const char* error = NeckoParent::GetValidatedAppInfo(aLoadContext,
                                                       Manager()->Manager(),
                                                       &aAppId,
                                                       &aIsInBrowserElement);
  if (error) {
    NS_WARNING(nsPrintfCString("CookieServiceParent: GetAppInfoFromParams: "
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

  uint32_t appId;
  bool isInBrowserElement, isPrivate;
  bool valid = GetAppInfoFromParams(aLoadContext, appId,
                                    isInBrowserElement, isPrivate);
  if (!valid) {
    return false;
  }

  mCookieService->GetCookieStringInternal(hostURI, aIsForeign, aFromHttp, appId,
                                          isInBrowserElement, isPrivate, *aResult);
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

  uint32_t appId;
  bool isInBrowserElement, isPrivate;
  bool valid = GetAppInfoFromParams(aLoadContext, appId,
                                    isInBrowserElement, isPrivate);
  if (!valid) {
    return false;
  }

  nsDependentCString cookieString(aCookieString, 0);
  //TODO: bug 812475, pass a real channel object
  mCookieService->SetCookieStringInternal(hostURI, aIsForeign, cookieString,
                                          aServerTime, aFromHttp, appId,
                                          isInBrowserElement, isPrivate, nullptr);
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

}
}

