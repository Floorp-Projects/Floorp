/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExtensionProtocolHandler.h"

#include "mozilla/ExtensionPolicyService.h"
#include "nsServiceManagerUtils.h"
#include "nsIURL.h"
#include "nsIChannel.h"
#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIStreamConverterService.h"
#include "nsNetUtil.h"
#include "LoadInfo.h"
#include "SimpleChannel.h"

namespace mozilla {
namespace net {

using extensions::URLInfo;

NS_IMPL_QUERY_INTERFACE(ExtensionProtocolHandler, nsISubstitutingProtocolHandler,
                        nsIProtocolHandler, nsIProtocolHandlerWithDynamicFlags,
                        nsISupportsWeakReference)
NS_IMPL_ADDREF_INHERITED(ExtensionProtocolHandler, SubstitutingProtocolHandler)
NS_IMPL_RELEASE_INHERITED(ExtensionProtocolHandler, SubstitutingProtocolHandler)

static inline ExtensionPolicyService&
EPS()
{
  return ExtensionPolicyService::GetSingleton();
}

nsresult
ExtensionProtocolHandler::GetFlagsForURI(nsIURI* aURI, uint32_t* aFlags)
{
  // In general a moz-extension URI is only loadable by chrome, but a whitelisted
  // subset are web-accessible (and cross-origin fetchable). Check that whitelist.
  bool loadableByAnyone = false;

  URLInfo url(aURI);
  if (auto* policy = EPS().GetByURL(url)) {
    loadableByAnyone = policy->IsPathWebAccessible(url.FilePath());
  }

  *aFlags = URI_STD | URI_IS_LOCAL_RESOURCE | (loadableByAnyone ? (URI_LOADABLE_BY_ANYONE | URI_FETCHABLE_BY_ANYONE) : URI_DANGEROUS_TO_LOAD);
  return NS_OK;
}

bool
ExtensionProtocolHandler::ResolveSpecialCases(const nsACString& aHost,
                                              const nsACString& aPath,
                                              const nsACString& aPathname,
                                              nsACString& aResult)
{
  // Create special moz-extension:-pages such as moz-extension://foo/_blank.html
  // for all registered extensions. We can't just do this as a substitution
  // because substitutions can only match on host.
  if (!SubstitutingProtocolHandler::HasSubstitution(aHost)) {
    return false;
  }

  if (aPathname.EqualsLiteral("/_blank.html")) {
    aResult.AssignLiteral("about:blank");
    return true;
  }

  if (aPathname.EqualsLiteral("/_generated_background_page.html")) {
    Unused << EPS().GetGeneratedBackgroundPageUrl(aHost, aResult);
    return !aResult.IsEmpty();
  }

  return false;
}

static inline Result<Ok, nsresult>
WrapNSResult(nsresult aRv)
{
  if (NS_FAILED(aRv)) {
    return Err(aRv);
  }
  return Ok();
}

#define NS_TRY(expr) MOZ_TRY(WrapNSResult(expr))

nsresult
ExtensionProtocolHandler::SubstituteChannel(nsIURI* aURI,
                                            nsILoadInfo* aLoadInfo,
                                            nsIChannel** result)
{
  nsresult rv;
  nsCOMPtr<nsIURL> url = do_QueryInterface(aURI, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString ext;
  rv = url->GetFileExtension(ext);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!ext.LowerCaseEqualsLiteral("css")) {
    return NS_OK;
  }

  // Filter CSS files to replace locale message tokens with localized strings.

  bool haveLoadInfo = aLoadInfo;
  nsCOMPtr<nsIChannel> channel = NS_NewSimpleChannel(
    aURI, aLoadInfo, *result,
    [haveLoadInfo] (nsIStreamListener* listener, nsIChannel* channel, nsIChannel* origChannel) -> RequestOrReason {
      nsresult rv;
      nsCOMPtr<nsIStreamConverterService> convService =
        do_GetService(NS_STREAMCONVERTERSERVICE_CONTRACTID, &rv);
      NS_TRY(rv);

      nsCOMPtr<nsIURI> uri;
      NS_TRY(channel->GetURI(getter_AddRefs(uri)));

      const char* kFromType = "application/vnd.mozilla.webext.unlocalized";
      const char* kToType = "text/css";

      nsCOMPtr<nsIStreamListener> converter;
      NS_TRY(convService->AsyncConvertData(kFromType, kToType, listener,
                                        uri, getter_AddRefs(converter)));
      if (haveLoadInfo) {
        NS_TRY(origChannel->AsyncOpen2(converter));
      } else {
        NS_TRY(origChannel->AsyncOpen(converter, nullptr));
      }

      return RequestOrReason(origChannel);
    });
  NS_ENSURE_TRUE(channel, NS_ERROR_OUT_OF_MEMORY);

  if (aLoadInfo) {
    nsCOMPtr<nsILoadInfo> loadInfo =
        static_cast<LoadInfo*>(aLoadInfo)->CloneForNewRequest();
    (*result)->SetLoadInfo(loadInfo);
  }

  channel.swap(*result);

  return NS_OK;
}

#undef NS_TRY

} // namespace net
} // namespace mozilla
