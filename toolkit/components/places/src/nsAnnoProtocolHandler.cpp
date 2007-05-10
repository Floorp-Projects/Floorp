//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Annotation Service
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brett Wilson <brettw@gmail.com> (original author)
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

/**
 * Implementation of moz-anno: URLs for accessing annotation values. This just
 * reads binary data from the annotation service.
 *
 * There is a special case for favicons. Annotation URLs with the name "favicon"
 * will be sent to the favicon service. If the favicon service doesn't have the
 * data, a stream containing the default favicon will be returned.
 */

#include "nsAnnoProtocolHandler.h"
#include "nsFaviconService.h"
#include "nsIChannel.h"
#include "nsIInputStreamChannel.h"
#include "nsILoadGroup.h"
#include "nsIStandardURL.h"
#include "nsIStringStream.h"
#include "nsISupportsUtils.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "nsStringStream.h"

NS_IMPL_ISUPPORTS1(nsAnnoProtocolHandler, nsIProtocolHandler)

// nsAnnoProtocolHandler::GetScheme

NS_IMETHODIMP
nsAnnoProtocolHandler::GetScheme(nsACString& aScheme)
{
  aScheme.AssignLiteral("moz-anno");
  return NS_OK;
}


// nsAnnoProtocolHandler::GetDefaultPort
//
//    There is no default port for annotation URLs

NS_IMETHODIMP
nsAnnoProtocolHandler::GetDefaultPort(PRInt32 *aDefaultPort)
{
  *aDefaultPort = -1;
  return NS_OK;
}


// nsAnnoProtocolHandler::GetProtocolFlags

NS_IMETHODIMP
nsAnnoProtocolHandler::GetProtocolFlags(PRUint32 *aProtocolFlags)
{
  *aProtocolFlags = (URI_NORELATIVE | URI_NOAUTH | URI_DANGEROUS_TO_LOAD);
  return NS_OK;
}


// nsAnnoProtocolHandler::NewURI

NS_IMETHODIMP
nsAnnoProtocolHandler::NewURI(const nsACString& aSpec,
                              const char *aOriginCharset,
                              nsIURI *aBaseURI, nsIURI **_retval)
{
  nsCOMPtr <nsIURI> uri = do_CreateInstance(NS_SIMPLEURI_CONTRACTID);
  if (!uri)
    return NS_ERROR_OUT_OF_MEMORY;
  nsresult rv = uri->SetSpec(aSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  *_retval = nsnull;
  uri.swap(*_retval);
  return NS_OK;
}


// nsAnnoProtocolHandler::NewChannel
//

NS_IMETHODIMP
nsAnnoProtocolHandler::NewChannel(nsIURI *aURI, nsIChannel **_retval)
{
  NS_ENSURE_ARG_POINTER(aURI);
  nsresult rv;

  nsCAutoString path;
  rv = aURI->GetPath(path);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIAnnotationService> annotationService = do_GetService(
                              "@mozilla.org/browser/annotation-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // annotation info
  nsCOMPtr<nsIURI> annoURI;
  nsCAutoString annoName;
  rv = ParseAnnoURI(aURI, getter_AddRefs(annoURI), annoName);
  NS_ENSURE_SUCCESS(rv, rv);

  // get the data from the annotation service and hand it off to the stream
  PRUint8* data;
  PRUint32 dataLen;
  nsCAutoString mimeType;

  if (annoName.EqualsLiteral(FAVICON_ANNOTATION_NAME)) {
    // special handling for favicons: ask favicon service
    nsFaviconService* faviconService = nsFaviconService::GetFaviconService();
    if (! faviconService) {
      NS_WARNING("Favicon service is unavailable.");
      return GetDefaultIcon(_retval);
    }
    rv = faviconService->GetFaviconData(annoURI, mimeType, &dataLen, &data);
    if (NS_FAILED(rv))
      return GetDefaultIcon(_retval);

    // don't allow icons without MIME types
    if (mimeType.IsEmpty()) {
      NS_Free(data);
      return GetDefaultIcon(_retval);
    }
  } else {
    // normal handling for annotations
    rv = annotationService->GetPageAnnotationBinary(annoURI, annoName, &data,
                                                    &dataLen, mimeType);
    NS_ENSURE_SUCCESS(rv, rv);

    // disallow annotations with no MIME types
    if (mimeType.IsEmpty()) {
      NS_Free(data);
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  nsCOMPtr<nsIStringInputStream> stream = do_CreateInstance(
                                          NS_STRINGINPUTSTREAM_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    NS_Free(data);
    return rv;
  }
  rv = stream->AdoptData((char*)data, dataLen);
  if (NS_FAILED(rv)) {
    NS_Free(data);
    return rv;
  }

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewInputStreamChannel(getter_AddRefs(channel), aURI, stream, mimeType);
  NS_ENSURE_SUCCESS(rv, rv);

  *_retval = channel;
  NS_ADDREF(*_retval);
  return NS_OK;
}


// nsAnnoProtocolHandler::AllowPort
//
//    Don't override any bans on bad ports.

NS_IMETHODIMP
nsAnnoProtocolHandler::AllowPort(PRInt32 port, const char *scheme,
                                 PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}


// nsAnnoProtocolHandler::ParseAnnoURI
//
//    Splits an annotation URL into its URI and name parts

nsresult
nsAnnoProtocolHandler::ParseAnnoURI(nsIURI* aURI,
                                    nsIURI** aResultURI, nsCString& aName)
{
  nsresult rv;
  nsCAutoString path;
  rv = aURI->GetPath(path);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 firstColon = path.FindChar(':');
  if (firstColon <= 0)
    return NS_ERROR_MALFORMED_URI;

  rv = NS_NewURI(aResultURI, Substring(path, firstColon + 1));
  NS_ENSURE_SUCCESS(rv, rv);

  aName = Substring(path, 0, firstColon);
  return NS_OK;
}


// nsAnnoProtocolHandler::GetDefaultIcon
//
//    This creates a channel for the default web page favicon.

nsresult
nsAnnoProtocolHandler::GetDefaultIcon(nsIChannel** aChannel)
{
  nsresult rv;
  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), NS_LITERAL_CSTRING(FAVICON_DEFAULT_URL));
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_NewChannel(aChannel, uri);
}
