/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/CookieServiceChild.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/net/NeckoChild.h"
#include "nsIChannel.h"
#include "nsIURI.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsServiceManagerUtils.h"

using namespace mozilla::ipc;

namespace mozilla {
namespace net {

// Pref string constants
static const char kPrefCookieBehavior[] = "network.cookie.cookieBehavior";
static const char kPrefThirdPartySession[] =
  "network.cookie.thirdparty.sessionOnly";

static CookieServiceChild *gCookieService;

CookieServiceChild*
CookieServiceChild::GetSingleton()
{
  if (!gCookieService)
    gCookieService = new CookieServiceChild();

  NS_ADDREF(gCookieService);
  return gCookieService;
}

NS_IMPL_ISUPPORTS(CookieServiceChild,
                  nsICookieService,
                  nsIObserver,
                  nsISupportsWeakReference)

CookieServiceChild::CookieServiceChild()
  : mCookieBehavior(nsICookieService::BEHAVIOR_ACCEPT)
  , mThirdPartySession(false)
{
  NS_ASSERTION(IsNeckoChild(), "not a child process");

  mozilla::dom::ContentChild* cc =
    static_cast<mozilla::dom::ContentChild*>(gNeckoChild->Manager());
  if (cc->IsShuttingDown()) {
    return;
  }

  // This corresponds to Release() in DeallocPCookieService.
  NS_ADDREF_THIS();

  // Create a child PCookieService actor.
  NeckoChild::InitNeckoChild();
  gNeckoChild->SendPCookieServiceConstructor(this);

  // Init our prefs and observer.
  nsCOMPtr<nsIPrefBranch> prefBranch =
    do_GetService(NS_PREFSERVICE_CONTRACTID);
  NS_WARNING_ASSERTION(prefBranch, "no prefservice");
  if (prefBranch) {
    prefBranch->AddObserver(kPrefCookieBehavior, this, true);
    prefBranch->AddObserver(kPrefThirdPartySession, this, true);
    PrefChanged(prefBranch);
  }
}

CookieServiceChild::~CookieServiceChild()
{
  gCookieService = nullptr;
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

  if (!mThirdPartyUtil && RequireThirdPartyCheck()) {
    mThirdPartyUtil = do_GetService(THIRDPARTYUTIL_CONTRACTID);
    NS_ASSERTION(mThirdPartyUtil, "require ThirdPartyUtil service");
  }
}

bool
CookieServiceChild::RequireThirdPartyCheck()
{
  return mCookieBehavior == nsICookieService::BEHAVIOR_REJECT_FOREIGN ||
    mCookieBehavior == nsICookieService::BEHAVIOR_LIMIT_FOREIGN ||
    mThirdPartySession;
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

  // Determine whether the request is foreign. Failure is acceptable.
  bool isForeign = true;
  if (RequireThirdPartyCheck())
    mThirdPartyUtil->IsThirdPartyChannel(aChannel, aHostURI, &isForeign);

  URIParams uriParams;
  SerializeURI(aHostURI, uriParams);

  mozilla::OriginAttributes attrs;
  if (aChannel) {
    nsCOMPtr<nsILoadInfo> loadInfo = aChannel->GetLoadInfo();
    if (loadInfo) {
      attrs = loadInfo->GetOriginAttributes();
    }
  }

  // Synchronously call the parent.
  nsAutoCString result;
  SendGetCookieString(uriParams, !!isForeign, attrs, &result);
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

  nsDependentCString cookieString(aCookieString);
  nsDependentCString serverTime;
  if (aServerTime)
    serverTime.Rebind(aServerTime);

  URIParams uriParams;
  SerializeURI(aHostURI, uriParams);

  mozilla::OriginAttributes attrs;
  if (aChannel) {
    nsCOMPtr<nsILoadInfo> loadInfo = aChannel->GetLoadInfo();
    if (loadInfo) {
      attrs = loadInfo->GetOriginAttributes();
    }
  }

  // Synchronously call the parent.
  SendSetCookieString(uriParams, !!isForeign, cookieString, serverTime,
                      attrs, aFromHttp);
  return NS_OK;
}

NS_IMETHODIMP
CookieServiceChild::Observe(nsISupports     *aSubject,
                            const char      *aTopic,
                            const char16_t *aData)
{
  NS_ASSERTION(strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID) == 0,
               "not a pref change topic!");

  nsCOMPtr<nsIPrefBranch> prefBranch = do_QueryInterface(aSubject);
  if (prefBranch)
    PrefChanged(prefBranch);
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

