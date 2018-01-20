//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Implementation of moz-page-thumb protocol. This accesses and displays
 * screenshots for URLs that are stored in the profile directory.
 */

#include "nsIPageThumbsStorageService.h"
#include "PageThumbsProtocol.h"
#include "nsIURI.h"
#include "nsIFileURL.h"
#include "nsIFile.h"
#include "nsIChannel.h"
#include "nsComponentManagerUtils.h"
#include "nsNetUtil.h"
#include "mozilla/dom/URLSearchParams.h"
#include "nsStandardURL.h"

using mozilla::dom::URLParams;
using mozilla::net::nsStandardURL;

NS_IMPL_ISUPPORTS(PageThumbsProtocol, nsIProtocolHandler);

// PageThumbsProtocol::GetScheme

NS_IMETHODIMP
PageThumbsProtocol::GetScheme(nsACString& aScheme)
{
  aScheme.AssignLiteral("moz-page-thumb");
  return NS_OK;
}

// PageThumbsProtocol::GetDefaultPort

NS_IMETHODIMP
PageThumbsProtocol::GetDefaultPort(int32_t *aDefaultPort)
{
  *aDefaultPort = -1;
  return NS_OK;
}

// PageThumbsProtocol::GetProtocolFlags

NS_IMETHODIMP
PageThumbsProtocol::GetProtocolFlags(uint32_t *aProtocolFlags)
{
  *aProtocolFlags = (URI_DANGEROUS_TO_LOAD | URI_IS_LOCAL_RESOURCE |
                     URI_NORELATIVE | URI_NOAUTH);
  return NS_OK;
}

// PageThumbsProtocol::NewURI

NS_IMETHODIMP
PageThumbsProtocol::NewURI(const nsACString& aSpec,
                           const char *aOriginCharset,
                           nsIURI *aBaseURI, nsIURI **_retval)
{
  return NS_MutateURI(NS_SIMPLEURIMUTATOR_CONTRACTID)
           .SetSpec(aSpec)
           .Finalize(_retval);
}

// PageThumbsProtocol::NewChannel

NS_IMETHODIMP
PageThumbsProtocol::NewChannel2(nsIURI* aURI,
                                nsILoadInfo *aLoadInfo,
                                nsIChannel** _retval)
{
  // Get the file path for the URL
  nsCOMPtr <nsIFile> filePath;
  nsresult rv = GetFilePathForURL(aURI, getter_AddRefs(filePath));
  if (NS_WARN_IF(NS_FAILED(rv))) return rv;

  // Get a file URI from the local file path
  nsCOMPtr <nsIURI> fileURI;
  rv = NS_NewFileURI(getter_AddRefs(fileURI), filePath);
  if (NS_WARN_IF(NS_FAILED(rv))) return rv;

  // Create a new channel with the file URI created
  nsCOMPtr <nsIChannel> channel;
  nsCOMPtr <nsIIOService> ios = do_GetIOService();
  rv = ios->NewChannelFromURIWithLoadInfo(fileURI, aLoadInfo, getter_AddRefs(channel));
  if (NS_WARN_IF(NS_FAILED(rv))) return rv;

  channel->SetOriginalURI(aURI);
  channel.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
PageThumbsProtocol::NewChannel(nsIURI* aURI, nsIChannel** _retval)
{
  return NewChannel2(aURI, nullptr, _retval);
}

// PageThumbsProtocol::AllowPort

NS_IMETHODIMP
PageThumbsProtocol::AllowPort(int32_t aPort, const char *aScheme, bool *_retval)
{
  *_retval = false;
  return NS_OK;
}

// PageThumbsProtocol::ParseProtocolURL
//
//    Extracts the URL from the query parameter. The URI is passed in in the form:
//    'moz-page-thumb://thumbnail/?url=http%3A%2F%2Fwww.mozilla.org%2F'.

nsresult
PageThumbsProtocol::ParseProtocolURL(nsIURI* aURI, nsString& aParsedURL)
{
  nsAutoCString spec;
  aURI->GetSpec(spec);

  // Check that we have the correct host
  nsAutoCString host;
  host = Substring(spec, spec.FindChar(':') + 3, 9);
  if (!host.EqualsLiteral("thumbnail")) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Get the path out of the URI
  nsAutoCString path;
  nsresult rv = aURI->GetPathQueryRef(path);

  if (NS_WARN_IF(NS_FAILED(rv))) return rv;

  // Since this is a protocol URI and it doesn't parse nicely, we split on where
  // the start of the query is and parse it from there
  int32_t queryBegins = path.FindChar('?');
  if (queryBegins <= 0) {
    return NS_ERROR_MALFORMED_URI;
  }

  URLParams::Extract(Substring(path, queryBegins + 1),
                     NS_LITERAL_STRING("url"),
                     aParsedURL);

  // If there's no URL as part of the query params, there will be no thumbnail
  if (aParsedURL.IsVoid()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return NS_OK;
}

// PageThumbsProtocol::GetFilePathForURL
//
//    Returns the thumbnail's file path for a given URL

nsresult
PageThumbsProtocol::GetFilePathForURL(nsIURI* aURI, nsIFile **_retval)
{
  nsresult rv;

  // Use PageThumbsStorageService to get the local file path of the screenshot
  // for the given URL
  nsAutoString filePathForURL;
  nsCOMPtr <nsIPageThumbsStorageService> pageThumbsStorage =
            do_GetService("@mozilla.org/thumbnails/pagethumbs-service;1", &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) return rv;

  // Parse the protocol URL to extract the thumbnail's URL
  nsAutoString parsedURL;
  rv = ParseProtocolURL(aURI, parsedURL);
  if (NS_WARN_IF(NS_FAILED(rv))) return rv;

  rv = pageThumbsStorage->GetFilePathForURL(parsedURL, filePathForURL);
  if (NS_WARN_IF(NS_FAILED(rv))) return rv;

  // Find the local file containing the screenshot
  nsCOMPtr <nsIFile> filePath = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);
  rv = filePath->InitWithPath(filePathForURL);
  if (NS_WARN_IF(NS_FAILED(rv))) return rv;

  filePath.forget(_retval);
  return NS_OK;
}

