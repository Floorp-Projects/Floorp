/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OCSPRequestor.h"

#include <limits>

#include "mozilla/Base64.h"
#include "mozilla/Scoped.h"
#include "nsIURLParser.h"
#include "nsNSSCallbacks.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"
#include "secerr.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo* gCertVerifierLog;
#endif

namespace mozilla {

void
ReleaseHttpServerSession(nsNSSHttpServerSession* httpServerSession)
{
  delete httpServerSession;
}

void
ReleaseHttpRequestSession(nsNSSHttpRequestSession* httpRequestSession)
{
  httpRequestSession->Release();
}

MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedHTTPServerSession,
                                          nsNSSHttpServerSession,
                                          ReleaseHttpServerSession)

MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedHTTPRequestSession,
                                          nsNSSHttpRequestSession,
                                          ReleaseHttpRequestSession)

} // namespace mozilla

namespace mozilla { namespace psm {

static nsresult
AppendEscapedBase64Item(const SECItem* encodedRequest, nsACString& path)
{
  nsresult rv;
  nsDependentCSubstring requestAsSubstring(
    reinterpret_cast<const char*>(encodedRequest->data), encodedRequest->len);
  nsCString base64Request;
  rv = Base64Encode(requestAsSubstring, base64Request);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  PR_LOG(gCertVerifierLog, PR_LOG_DEBUG,
         ("Setting up OCSP GET path, pre path =%s\n",
          PromiseFlatCString(path).get()));

  // The path transformation is not a direct url encoding. Three characters
  // need change '+' -> "%2B", '/' -> "%2F", and '=' -> '%3D'.
  // http://tools.ietf.org/html/rfc5019#section-5
  base64Request.ReplaceSubstring("+", "%2B");
  base64Request.ReplaceSubstring("/", "%2F");
  base64Request.ReplaceSubstring("=", "%3D");
  path.Append(base64Request);
  return NS_OK;
}

SECItem*
DoOCSPRequest(PLArenaPool* arena, const char* url,
              const SECItem* encodedRequest, PRIntervalTime timeout,
              bool useGET)
{
  if (!arena || !url || !encodedRequest || !encodedRequest->data) {
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
    return nullptr;
  }
  uint32_t urlLen = PL_strlen(url);
  if (urlLen > static_cast<uint32_t>(std::numeric_limits<int32_t>::max())) {
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
    return nullptr;
  }

  nsCOMPtr<nsIURLParser> urlParser = do_GetService(NS_STDURLPARSER_CONTRACTID);
  if (!urlParser) {
    PR_SetError(SEC_ERROR_LIBRARY_FAILURE, 0);
    return nullptr;
  }

  uint32_t schemePos;
  int32_t schemeLen;
  uint32_t authorityPos;
  int32_t authorityLen;
  uint32_t pathPos;
  int32_t pathLen;
  nsresult rv = urlParser->ParseURL(url, static_cast<int32_t>(urlLen),
                                    &schemePos, &schemeLen,
                                    &authorityPos, &authorityLen,
                                    &pathPos, &pathLen);
  if (NS_FAILED(rv)) {
    PR_SetError(SEC_ERROR_CERT_BAD_ACCESS_LOCATION, 0);
    return nullptr;
  }
  if (schemeLen < 0 || authorityLen < 0) {
    PR_SetError(SEC_ERROR_CERT_BAD_ACCESS_LOCATION, 0);
    return nullptr;
  }
  nsAutoCString scheme(url + schemePos,
                       static_cast<nsAutoCString::size_type>(schemeLen));
  if (!scheme.LowerCaseEqualsLiteral("http")) {
    // We dont support https:// to avoid loops see Bug 92923
    PR_SetError(SEC_ERROR_CERT_BAD_ACCESS_LOCATION, 0);
    return nullptr;
  }

  uint32_t hostnamePos;
  int32_t hostnameLen;
  int32_t port;
  // We do not support urls with user@pass sections in the URL,
  // In cas we find them we will ignore and try to connect with
  rv = urlParser->ParseAuthority(url + authorityPos, authorityLen,
                                 nullptr, nullptr, nullptr, nullptr,
                                 &hostnamePos, &hostnameLen, &port);
  if (NS_FAILED(rv)) {
    PR_SetError(SEC_ERROR_CERT_BAD_ACCESS_LOCATION, 0);
    return nullptr;
  }
  if (hostnameLen < 0) {
    PR_SetError(SEC_ERROR_CERT_BAD_ACCESS_LOCATION, 0);
    return nullptr;
  }
  if (port == -1) {
    port = 80;
  } else if (port < 0 || port > 0xffff) {
    PR_SetError(SEC_ERROR_CERT_BAD_ACCESS_LOCATION, 0);
    return nullptr;
  }
  nsAutoCString
    hostname(url + authorityPos + hostnamePos,
             static_cast<nsACString_internal::size_type>(hostnameLen));

  SEC_HTTP_SERVER_SESSION serverSessionPtr = nullptr;
  if (nsNSSHttpInterface::createSessionFcn(hostname.BeginReading(),
                                           static_cast<uint16_t>(port),
                                           &serverSessionPtr) != SECSuccess) {
    PR_SetError(SEC_ERROR_NO_MEMORY, 0);
    return nullptr;
  }
  ScopedHTTPServerSession serverSession(
    reinterpret_cast<nsNSSHttpServerSession*>(serverSessionPtr));

  nsAutoCString path;
  if (pathLen > 0) {
    path.Assign(url + pathPos, static_cast<nsAutoCString::size_type>(pathLen));
  } else {
    path.Assign("/");
  }
  PR_LOG(gCertVerifierLog, PR_LOG_DEBUG,
         ("Setting up OCSP request: pre all path =%s  pathlen=%d\n", path.get(),
          pathLen));
  nsAutoCString method("POST");
  if (useGET) {
    method.Assign("GET");
    if (!StringEndsWith(path, NS_LITERAL_CSTRING("/"))) {
      path.Append("/");
    }
    nsresult rv = AppendEscapedBase64Item(encodedRequest, path);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }
  }

  SEC_HTTP_REQUEST_SESSION requestSessionPtr;
  if (nsNSSHttpInterface::createFcn(serverSession.get(), "http",
                                    path.get(), method.get(),
                                    timeout, &requestSessionPtr)
        != SECSuccess) {
    PR_SetError(SEC_ERROR_NO_MEMORY, 0);
    return nullptr;
  }

  ScopedHTTPRequestSession requestSession(
    reinterpret_cast<nsNSSHttpRequestSession*>(requestSessionPtr));

  if (!useGET) {
    if (nsNSSHttpInterface::setPostDataFcn(requestSession.get(),
          reinterpret_cast<char*>(encodedRequest->data), encodedRequest->len,
          "application/ocsp-request") != SECSuccess) {
      PR_SetError(SEC_ERROR_NO_MEMORY, 0);
      return nullptr;
    }
  }

  uint16_t httpResponseCode;
  const char* httpResponseData;
  uint32_t httpResponseDataLen = 0; // 0 means any response size is acceptable
  if (nsNSSHttpInterface::trySendAndReceiveFcn(requestSession.get(), nullptr,
                                               &httpResponseCode, nullptr,
                                               nullptr, &httpResponseData,
                                               &httpResponseDataLen)
        != SECSuccess) {
    PR_SetError(SEC_ERROR_OCSP_SERVER_ERROR, 0);
    return nullptr;
  }

  if (httpResponseCode != 200) {
    PR_SetError(SEC_ERROR_OCSP_SERVER_ERROR, 0);
    return nullptr;
  }

  SECItem* encodedResponse = SECITEM_AllocItem(arena, nullptr,
                                               httpResponseDataLen);
  if (!encodedResponse) {
    PR_SetError(SEC_ERROR_NO_MEMORY, 0);
    return nullptr;
  }

  memcpy(encodedResponse->data, httpResponseData, httpResponseDataLen);
  return encodedResponse;
}

} } // namespace mozilla::psm
