/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OCSPRequestor.h"

#include "nsIURLParser.h"
#include "nsNSSCallbacks.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"
#include "pkix/ScopedPtr.h"
#include "secerr.h"

namespace mozilla { namespace psm {

using mozilla::pkix::ScopedPtr;

void
ReleaseHttpServerSession(nsNSSHttpServerSession* httpServerSession)
{
  delete httpServerSession;
}
typedef ScopedPtr<nsNSSHttpServerSession, ReleaseHttpServerSession>
  ScopedHTTPServerSession;

void
ReleaseHttpRequestSession(nsNSSHttpRequestSession* httpRequestSession)
{
  httpRequestSession->Release();
}
typedef ScopedPtr<nsNSSHttpRequestSession, ReleaseHttpRequestSession>
  ScopedHTTPRequestSession;

SECItem* DoOCSPRequest(PLArenaPool* arena, const char* url,
                       const SECItem* encodedRequest, PRIntervalTime timeout)
{
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
  nsresult rv = urlParser->ParseURL(url, PL_strlen(url),
                                    &schemePos, &schemeLen,
                                    &authorityPos, &authorityLen,
                                    &pathPos, &pathLen);
  if (NS_FAILED(rv)) {
    PR_SetError(SEC_ERROR_CERT_BAD_ACCESS_LOCATION, 0);
    return nullptr;
  }
  uint32_t hostnamePos;
  int32_t hostnameLen;
  int32_t port;
  rv = urlParser->ParseAuthority(url + authorityPos, authorityLen,
                                 nullptr, nullptr, nullptr, nullptr,
                                 &hostnamePos, &hostnameLen, &port);
  if (NS_FAILED(rv)) {
    PR_SetError(SEC_ERROR_CERT_BAD_ACCESS_LOCATION, 0);
    return nullptr;
  }
  if (port == -1) {
    port = 80;
  }

  nsAutoCString hostname(url + authorityPos + hostnamePos, hostnameLen);
  SEC_HTTP_SERVER_SESSION serverSessionPtr = nullptr;
  if (nsNSSHttpInterface::createSessionFcn(hostname.BeginReading(), port,
                                           &serverSessionPtr) != SECSuccess) {
    PR_SetError(SEC_ERROR_NO_MEMORY, 0);
    return nullptr;
  }

  ScopedHTTPServerSession serverSession(
    reinterpret_cast<nsNSSHttpServerSession*>(serverSessionPtr));
  nsAutoCString path(url + pathPos, pathLen);
  SEC_HTTP_REQUEST_SESSION requestSessionPtr;
  if (nsNSSHttpInterface::createFcn(serverSession.get(), "http",
                                    path.BeginReading(), "POST",
                                    timeout, &requestSessionPtr)
        != SECSuccess) {
    PR_SetError(SEC_ERROR_NO_MEMORY, 0);
    return nullptr;
  }

  ScopedHTTPRequestSession requestSession(
    reinterpret_cast<nsNSSHttpRequestSession*>(requestSessionPtr));
  if (nsNSSHttpInterface::setPostDataFcn(requestSession.get(),
        reinterpret_cast<char*>(encodedRequest->data), encodedRequest->len,
        "application/ocsp-request") != SECSuccess) {
    PR_SetError(SEC_ERROR_NO_MEMORY, 0);
    return nullptr;
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
