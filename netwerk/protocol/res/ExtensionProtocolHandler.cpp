/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExtensionProtocolHandler.h"

#include "nsIAddonPolicyService.h"
#include "nsServiceManagerUtils.h"
#include "nsIURL.h"
#include "nsIChannel.h"
#include "nsIStreamListener.h"
#include "nsIRequestObserver.h"
#include "nsIInputStreamChannel.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIStreamConverterService.h"
#include "nsIPipe.h"
#include "nsNetUtil.h"
#include "LoadInfo.h"

namespace mozilla {
namespace net {

NS_IMPL_QUERY_INTERFACE(ExtensionProtocolHandler, nsISubstitutingProtocolHandler,
                        nsIProtocolHandler, nsIProtocolHandlerWithDynamicFlags,
                        nsISupportsWeakReference)
NS_IMPL_ADDREF_INHERITED(ExtensionProtocolHandler, SubstitutingProtocolHandler)
NS_IMPL_RELEASE_INHERITED(ExtensionProtocolHandler, SubstitutingProtocolHandler)

nsresult
ExtensionProtocolHandler::GetFlagsForURI(nsIURI* aURI, uint32_t* aFlags)
{
  // In general a moz-extension URI is only loadable by chrome, but a whitelisted
  // subset are web-accessible (and cross-origin fetchable). Check that whitelist.
  nsCOMPtr<nsIAddonPolicyService> aps = do_GetService("@mozilla.org/addons/policy-service;1");
  bool loadableByAnyone = false;
  if (aps) {
    nsresult rv = aps->ExtensionURILoadableByAnyone(aURI, &loadableByAnyone);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  *aFlags = URI_STD | URI_IS_LOCAL_RESOURCE | (loadableByAnyone ? (URI_LOADABLE_BY_ANYONE | URI_FETCHABLE_BY_ANYONE) : URI_DANGEROUS_TO_LOAD);
  return NS_OK;
}

class PipeCloser : public nsIRequestObserver
{
public:
  NS_DECL_ISUPPORTS

  explicit PipeCloser(nsIOutputStream* aOutputStream) :
    mOutputStream(aOutputStream)
  {
  }

  NS_IMETHOD OnStartRequest(nsIRequest*, nsISupports*) override
  {
    return NS_OK;
  }

  NS_IMETHOD OnStopRequest(nsIRequest*, nsISupports*, nsresult aStatusCode) override
  {
    NS_ENSURE_TRUE(mOutputStream, NS_ERROR_UNEXPECTED);

    nsresult rv = mOutputStream->Close();
    mOutputStream = nullptr;
    return rv;
  }

protected:
  virtual ~PipeCloser() {}

private:
  nsCOMPtr<nsIOutputStream> mOutputStream;
};

NS_IMPL_ISUPPORTS(PipeCloser, nsIRequestObserver)

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

  nsCOMPtr<nsIStreamConverterService> convService =
    do_GetService(NS_STREAMCONVERTERSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  const char* kFromType = "application/vnd.mozilla.webext.unlocalized";
  const char* kToType = "text/css";

  nsCOMPtr<nsIInputStream> inputStream;
  if (aLoadInfo &&
      aLoadInfo->GetSecurityMode() == nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS) {
    // If the channel needs to enforce CORS, we need to open the channel async.

    nsCOMPtr<nsIOutputStream> outputStream;
    rv = NS_NewPipe(getter_AddRefs(inputStream), getter_AddRefs(outputStream),
                    0, UINT32_MAX, true, false);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIStreamListener> listener;
    nsCOMPtr<nsIRequestObserver> observer = new PipeCloser(outputStream);
    rv = NS_NewSimpleStreamListener(getter_AddRefs(listener), outputStream, observer);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIStreamListener> converter;
    rv = convService->AsyncConvertData(kFromType, kToType, listener,
                                       aURI, getter_AddRefs(converter));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsILoadInfo> loadInfo =
      static_cast<LoadInfo*>(aLoadInfo)->CloneForNewRequest();
    (*result)->SetLoadInfo(loadInfo);

    rv = (*result)->AsyncOpen2(converter);
  } else {
    // Stylesheet loads for extension content scripts require a sync channel.

    nsCOMPtr<nsIInputStream> sourceStream;
    if (aLoadInfo && aLoadInfo->GetEnforceSecurity()) {
      rv = (*result)->Open2(getter_AddRefs(sourceStream));
    } else {
      rv = (*result)->Open(getter_AddRefs(sourceStream));
    }
    NS_ENSURE_SUCCESS(rv, rv);

    rv = convService->Convert(sourceStream, kFromType, kToType,
                              aURI, getter_AddRefs(inputStream));
  }
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewInputStreamChannelInternal(getter_AddRefs(channel), aURI, inputStream,
                                        NS_LITERAL_CSTRING("text/css"),
                                        NS_LITERAL_CSTRING("utf-8"),
                                        aLoadInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  channel.swap(*result);
  return NS_OK;
}

} // namespace net
} // namespace mozilla
