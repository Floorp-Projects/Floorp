/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation <http://www.mozilla.org/>.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Witte <dwitte@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "mozilla/net/CookieServiceChild.h"
#include "mozilla/net/NeckoChild.h"
#include "nsIURI.h"

namespace mozilla {
namespace net {

static CookieServiceChild *gCookieService;

CookieServiceChild*
CookieServiceChild::GetSingleton()
{
  if (!gCookieService)
    gCookieService = new CookieServiceChild();

  NS_ADDREF(gCookieService);
  return gCookieService;
}

NS_IMPL_ISUPPORTS1(CookieServiceChild, nsICookieService)

CookieServiceChild::CookieServiceChild()
{
  NS_ASSERTION(IsNeckoChild(), "not a child process");

  // This corresponds to Release() in DeallocPCookieService.
  NS_ADDREF_THIS();

  // Create a child PCookieService actor.
  NeckoChild::InitNeckoChild();
  gNeckoChild->SendPCookieServiceConstructor(this);

  mPermissionService = do_GetService(NS_COOKIEPERMISSION_CONTRACTID);
  if (!mPermissionService)
    NS_WARNING("couldn't get nsICookiePermission in child");
}

CookieServiceChild::~CookieServiceChild()
{
  gCookieService = nsnull;
}

void
CookieServiceChild::SerializeURIs(nsIURI *aHostURI,
                                  nsIChannel *aChannel,
                                  nsCString &aHostSpec,
                                  nsCString &aHostCharset,
                                  nsCString &aOriginatingSpec,
                                  nsCString &aOriginatingCharset)
{
  // Serialize the host URI.
  // TODO: The cookieservice deals exclusively with ASCII hosts, but not paths.
  // We should fix that, and then we can just serialize the spec as ASCII here.
  aHostURI->GetSpec(aHostSpec);
  aHostURI->GetOriginCharset(aHostCharset);

  // Determine and serialize the originating URI. Failure is acceptable.
  if (!mPermissionService) {
    NS_WARNING("nsICookiePermission unavailable! Cookie may be rejected");
    return;
  }

  nsCOMPtr<nsIURI> originatingURI;
  mPermissionService->GetOriginatingURI(aChannel,
                                        getter_AddRefs(originatingURI));
  if (originatingURI) {
    originatingURI->GetSpec(aOriginatingSpec);
    originatingURI->GetOriginCharset(aOriginatingSpec);
  }
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

  nsCAutoString hostSpec, hostCharset, originatingSpec, originatingCharset;
  SerializeURIs(aHostURI, aChannel, hostSpec, hostCharset,
                originatingSpec, originatingCharset);

  // Synchronously call the parent.
  nsCAutoString result;
  SendGetCookieString(hostSpec, hostCharset,
                      originatingSpec, originatingCharset,
                      aFromHttp, &result);
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

  nsCAutoString hostSpec, hostCharset, originatingSpec, originatingCharset;
  SerializeURIs(aHostURI, aChannel, hostSpec, hostCharset,
                originatingSpec, originatingCharset);

  nsDependentCString cookieString(aCookieString);
  nsDependentCString serverTime;
  if (aServerTime)
    serverTime.Rebind(aServerTime);

  // Synchronously call the parent.
  SendSetCookieString(hostSpec, hostCharset,
                      originatingSpec, originatingCharset,
                      cookieString, serverTime, aFromHttp);
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
                                 nsnull, false);
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

