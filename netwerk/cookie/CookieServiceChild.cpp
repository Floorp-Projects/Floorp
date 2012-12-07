/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/CookieServiceChild.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/net/NeckoChild.h"
#include "nsIURI.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsITabChild.h"
#include "nsNetUtil.h"

using namespace mozilla::ipc;

namespace mozilla {
namespace net {

// Behavior pref constants
static const int32_t BEHAVIOR_ACCEPT = 0;
static const int32_t BEHAVIOR_REJECTFOREIGN = 1;
static const int32_t BEHAVIOR_REJECT = 2;

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

NS_IMPL_ISUPPORTS3(CookieServiceChild,
                   nsICookieService,
                   nsIObserver,
                   nsISupportsWeakReference)

CookieServiceChild::CookieServiceChild()
  : mCookieBehavior(BEHAVIOR_ACCEPT)
  , mThirdPartySession(false)
{
  NS_ASSERTION(IsNeckoChild(), "not a child process");

  // This corresponds to Release() in DeallocPCookieService.
  NS_ADDREF_THIS();

  // Create a child PCookieService actor.
  NeckoChild::InitNeckoChild();
  gNeckoChild->SendPCookieServiceConstructor(this);

  // Init our prefs and observer.
  nsCOMPtr<nsIPrefBranch> prefBranch =
    do_GetService(NS_PREFSERVICE_CONTRACTID);
  NS_WARN_IF_FALSE(prefBranch, "no prefservice");
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
      val >= BEHAVIOR_ACCEPT && val <= BEHAVIOR_REJECT ? val : BEHAVIOR_ACCEPT;

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
  return mCookieBehavior == BEHAVIOR_REJECTFOREIGN || mThirdPartySession;
}

nsresult
CookieServiceChild::GetCookieStringInternal(nsIURI *aHostURI,
                                            nsIChannel *aChannel,
                                            char **aCookieString,
                                            bool aFromHttp)
{
  NS_ENSURE_ARG(aHostURI);
  NS_ENSURE_ARG_POINTER(aCookieString);

  *aCookieString = NULL;

  // Determine whether the request is foreign. Failure is acceptable.
  bool isForeign = true;
  if (RequireThirdPartyCheck())
    mThirdPartyUtil->IsThirdPartyChannel(aChannel, aHostURI, &isForeign);

  URIParams uriParams;
  SerializeURI(aHostURI, uriParams);

  nsCOMPtr<nsITabChild> iTabChild;
  mozilla::dom::TabChild* tabChild = nullptr;
  if (aChannel) {
    NS_QueryNotificationCallbacks(aChannel, iTabChild);
    if (iTabChild) {
      tabChild = static_cast<mozilla::dom::TabChild*>(iTabChild.get());
    }
  }

  // Synchronously call the parent.
  nsAutoCString result;
  SendGetCookieString(uriParams, !!isForeign, aFromHttp,
                      IPC::SerializedLoadContext(aChannel), tabChild, &result);
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

  nsCOMPtr<nsITabChild> iTabChild;
  mozilla::dom::TabChild* tabChild = nullptr;
  if (aChannel) {
    NS_QueryNotificationCallbacks(aChannel, iTabChild);
    if (iTabChild) {
      tabChild = static_cast<mozilla::dom::TabChild*>(iTabChild.get());
    }
  }

  // Synchronously call the parent.
  SendSetCookieString(uriParams, !!isForeign, cookieString, serverTime,
                      aFromHttp, IPC::SerializedLoadContext(aChannel), tabChild);
  return NS_OK;
}

NS_IMETHODIMP
CookieServiceChild::Observe(nsISupports     *aSubject,
                            const char      *aTopic,
                            const PRUnichar *aData)
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
  return GetCookieStringInternal(aHostURI, aChannel, aCookieString, false);
}

NS_IMETHODIMP
CookieServiceChild::GetCookieStringFromHttp(nsIURI *aHostURI,
                                            nsIURI *aFirstURI,
                                            nsIChannel *aChannel,
                                            char **aCookieString)
{
  return GetCookieStringInternal(aHostURI, aChannel, aCookieString, true);
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

}
}

