/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsViewSourceHandler.h"
#include "nsViewSourceChannel.h"
#include "nsNetUtil.h"
#include "nsSimpleNestedURI.h"

#define VIEW_SOURCE "view-source"

namespace mozilla {
namespace net {

////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS(nsViewSourceHandler, nsIProtocolHandler)

////////////////////////////////////////////////////////////////////////////////
// nsIProtocolHandler methods:

NS_IMETHODIMP
nsViewSourceHandler::GetScheme(nsACString &result)
{
    result.AssignLiteral(VIEW_SOURCE);
    return NS_OK;
}

NS_IMETHODIMP
nsViewSourceHandler::GetDefaultPort(int32_t *result)
{
    *result = -1;
    return NS_OK;
}

NS_IMETHODIMP
nsViewSourceHandler::GetProtocolFlags(uint32_t *result)
{
    *result = URI_NORELATIVE | URI_NOAUTH | URI_DANGEROUS_TO_LOAD |
        URI_NON_PERSISTABLE;
    return NS_OK;
}

NS_IMETHODIMP
nsViewSourceHandler::NewURI(const nsACString &aSpec,
                            const char *aCharset,
                            nsIURI *aBaseURI,
                            nsIURI **aResult)
{
    *aResult = nullptr;

    // Extract inner URL and normalize to ASCII.  This is done to properly
    // support IDN in cases like "view-source:http://www.szalagavató.hu/"

    int32_t colon = aSpec.FindChar(':');
    if (colon == kNotFound)
        return NS_ERROR_MALFORMED_URI;

    nsCOMPtr<nsIURI> innerURI;
    nsresult rv = NS_NewURI(getter_AddRefs(innerURI),
                            Substring(aSpec, colon + 1), aCharset, aBaseURI);
    if (NS_FAILED(rv))
        return rv;

    nsAutoCString asciiSpec;
    rv = innerURI->GetAsciiSpec(asciiSpec);
    if (NS_FAILED(rv))
        return rv;

    // put back our scheme and construct a simple-uri wrapper

    asciiSpec.Insert(VIEW_SOURCE ":", 0);

    // We can't swap() from an RefPtr<nsSimpleNestedURI> to an nsIURI**,
    // sadly.
    nsSimpleNestedURI* ourURI = new nsSimpleNestedURI(innerURI);
    nsCOMPtr<nsIURI> uri = ourURI;
    if (!uri)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = ourURI->SetSpec(asciiSpec);
    if (NS_FAILED(rv))
        return rv;

    // Make the URI immutable so it's impossible to get it out of sync
    // with its inner URI.
    ourURI->SetMutable(false);

    uri.swap(*aResult);
    return rv;
}

NS_IMETHODIMP
nsViewSourceHandler::NewChannel2(nsIURI* uri,
                                 nsILoadInfo* aLoadInfo,
                                 nsIChannel** result)
{
    NS_ENSURE_ARG_POINTER(uri);
    nsViewSourceChannel *channel = new nsViewSourceChannel();
    if (!channel)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(channel);

    nsresult rv = channel->Init(uri);
    if (NS_FAILED(rv)) {
        NS_RELEASE(channel);
        return rv;
    }

    // set the loadInfo on the new channel
    rv = channel->SetLoadInfo(aLoadInfo);
    if (NS_FAILED(rv)) {
        NS_RELEASE(channel);
        return rv;
    }

    *result = static_cast<nsIViewSourceChannel*>(channel);
    return NS_OK;
}

NS_IMETHODIMP
nsViewSourceHandler::NewChannel(nsIURI* uri, nsIChannel* *result)
{
    return NewChannel2(uri, nullptr, result);
}

nsresult
nsViewSourceHandler::NewSrcdocChannel(nsIURI *aURI,
                                      nsIURI *aBaseURI,
                                      const nsAString &aSrcdoc,
                                      nsILoadInfo* aLoadInfo,
                                      nsIChannel** outChannel)
{
    NS_ENSURE_ARG_POINTER(aURI);
    RefPtr<nsViewSourceChannel> channel = new nsViewSourceChannel();

    nsresult rv = channel->InitSrcdoc(aURI, aBaseURI, aSrcdoc, aLoadInfo);
    if (NS_FAILED(rv)) {
        return rv;
    }

    *outChannel = static_cast<nsIViewSourceChannel*>(channel.forget().take());
    return NS_OK;
}

NS_IMETHODIMP
nsViewSourceHandler::AllowPort(int32_t port, const char *scheme, bool *_retval)
{
    // don't override anything.
    *_retval = false;
    return NS_OK;
}

nsViewSourceHandler::nsViewSourceHandler()
{
    gInstance = this;
}

nsViewSourceHandler::~nsViewSourceHandler()
{
    gInstance = nullptr;
}

nsViewSourceHandler* nsViewSourceHandler::gInstance = nullptr;

nsViewSourceHandler*
nsViewSourceHandler::GetInstance()
{
    return gInstance;
}

} // namespace net
} // namespace mozilla
