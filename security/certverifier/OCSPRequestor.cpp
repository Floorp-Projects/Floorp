/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OCSPRequestor.h"

#include <limits>

#include "ScopedNSSTypes.h"
#include "mozilla/Base64.h"
#include "mozilla/Casting.h"
#include "nsIURLParser.h"
#include "nsNSSCallbacks.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"
#include "secerr.h"

extern mozilla::LazyLogModule gCertVerifierLog;

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

MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniqueHTTPServerSession,
                                      nsNSSHttpServerSession,
                                      ReleaseHttpServerSession)

MOZ_TYPE_SPECIFIC_UNIQUE_PTR_TEMPLATE(UniqueHTTPRequestSession,
                                      nsNSSHttpRequestSession,
                                      ReleaseHttpRequestSession)

} // namespace mozilla

namespace mozilla { namespace psm {

static nsresult
AppendEscapedBase64Item(const SECItem* encodedRequest, nsACString& path)
{
  nsresult rv;
  nsDependentCSubstring requestAsSubstring(
    BitwiseCast<char*, unsigned char*>(encodedRequest->data),
    encodedRequest->len);
  nsCString base64Request;
  rv = Base64Encode(requestAsSubstring, base64Request);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
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

Result
DoOCSPRequest(const UniquePLArenaPool& arena, const char* url,
              const char* firstPartyDomain, const SECItem* encodedRequest,
              PRIntervalTime timeout, bool useGET,
      /*out*/ SECItem*& encodedResponse)
{
  MOZ_ASSERT(arena.get());
  MOZ_ASSERT(url);
  MOZ_ASSERT(encodedRequest);
  MOZ_ASSERT(encodedRequest->data);
  if (!arena.get() || !url || !encodedRequest || !encodedRequest->data) {
    return Result::FATAL_ERROR_INVALID_ARGS;
  }
  uint32_t urlLen = PL_strlen(url);
  if (urlLen > static_cast<uint32_t>(std::numeric_limits<int32_t>::max())) {
    return Result::FATAL_ERROR_INVALID_ARGS;
  }

  nsCOMPtr<nsIURLParser> urlParser = do_GetService(NS_STDURLPARSER_CONTRACTID);
  if (!urlParser) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }

  uint32_t schemePos;
  int32_t schemeLen;
  uint32_t authorityPos;
  int32_t authorityLen;
  uint32_t pathPos;
  int32_t pathLen;
  nsresult nsrv = urlParser->ParseURL(url, static_cast<int32_t>(urlLen),
                                      &schemePos, &schemeLen,
                                      &authorityPos, &authorityLen,
                                      &pathPos, &pathLen);
  if (NS_FAILED(nsrv)) {
    return Result::ERROR_CERT_BAD_ACCESS_LOCATION;
  }
  if (schemeLen < 0 || authorityLen < 0) {
    return Result::ERROR_CERT_BAD_ACCESS_LOCATION;
  }
  nsAutoCString scheme(url + schemePos,
                       static_cast<nsAutoCString::size_type>(schemeLen));
  if (!scheme.LowerCaseEqualsLiteral("http")) {
    // We don't support HTTPS to avoid loops. See Bug 92923.
    // We also in general only support HTTP.
    return Result::ERROR_CERT_BAD_ACCESS_LOCATION;
  }

  uint32_t hostnamePos;
  int32_t hostnameLen;
  int32_t port;
  // We ignore user:password sections: if one is present, we send an OCSP
  // request to the URL as normal without sending the username or password.
  nsrv = urlParser->ParseAuthority(url + authorityPos, authorityLen,
                                   nullptr, nullptr, nullptr, nullptr,
                                   &hostnamePos, &hostnameLen, &port);
  if (NS_FAILED(nsrv)) {
    return Result::ERROR_CERT_BAD_ACCESS_LOCATION;
  }
  if (hostnameLen < 0) {
    return Result::ERROR_CERT_BAD_ACCESS_LOCATION;
  }
  if (port == -1) {
    port = 80;
  } else if (port < 0 || port > 0xffff) {
    return Result::ERROR_CERT_BAD_ACCESS_LOCATION;
  }
  nsAutoCString
    hostname(url + authorityPos + hostnamePos,
             static_cast<nsACString_internal::size_type>(hostnameLen));

  nsNSSHttpServerSession* serverSessionPtr = nullptr;
  Result rv = nsNSSHttpInterface::createSessionFcn(
    hostname.BeginReading(), static_cast<uint16_t>(port), &serverSessionPtr);
  if (rv != Success) {
    return rv;
  }
  UniqueHTTPServerSession serverSession(serverSessionPtr);

  nsAutoCString path;
  if (pathLen > 0) {
    path.Assign(url + pathPos, static_cast<nsAutoCString::size_type>(pathLen));
  } else {
    path.Assign("/");
  }
  MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
         ("Setting up OCSP request: pre all path =%s  pathlen=%d\n", path.get(),
          pathLen));
  nsAutoCString method("POST");
  if (useGET) {
    method.Assign("GET");
    if (!StringEndsWith(path, NS_LITERAL_CSTRING("/"))) {
      path.Append("/");
    }
    nsresult nsrv = AppendEscapedBase64Item(encodedRequest, path);
    if (NS_WARN_IF(NS_FAILED(nsrv))) {
      return Result::FATAL_ERROR_LIBRARY_FAILURE;
    }
  }

  nsNSSHttpRequestSession* requestSessionPtr;
  rv = nsNSSHttpInterface::createFcn(serverSession.get(), "http", path.get(),
                                     method.get(), firstPartyDomain, timeout,
                                     &requestSessionPtr);
  if (rv != Success) {
    return rv;
  }

  UniqueHTTPRequestSession requestSession(requestSessionPtr);

  if (!useGET) {
    rv = nsNSSHttpInterface::setPostDataFcn(
      requestSession.get(),
      BitwiseCast<char*, unsigned char*>(encodedRequest->data),
      encodedRequest->len, "application/ocsp-request");
    if (rv != Success) {
      return rv;
    }
  }

  uint16_t httpResponseCode;
  const char* httpResponseData;
  uint32_t httpResponseDataLen = 0; // 0 means any response size is acceptable
  rv = nsNSSHttpInterface::trySendAndReceiveFcn(requestSession.get(), nullptr,
                                                &httpResponseCode, nullptr,
                                                nullptr, &httpResponseData,
                                                &httpResponseDataLen);
  if (rv != Success) {
    return rv;
  }

  if (httpResponseCode != 200) {
    return Result::ERROR_OCSP_SERVER_ERROR;
  }

  encodedResponse = SECITEM_AllocItem(arena.get(), nullptr, httpResponseDataLen);
  if (!encodedResponse) {
    return Result::FATAL_ERROR_NO_MEMORY;
  }

  memcpy(encodedResponse->data, httpResponseData, httpResponseDataLen);
  return Success;
}

} } // namespace mozilla::psm
