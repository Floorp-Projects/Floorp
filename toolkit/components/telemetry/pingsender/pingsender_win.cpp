/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string>

#include <windows.h>
#include <wininet.h>

namespace PingSender {

using std::string;

/**
 * A helper to automatically close internet handles when they go out of scope
 */
class ScopedHInternet {
 public:
  explicit ScopedHInternet(HINTERNET handle) : mHandle(handle) {}

  ~ScopedHInternet() {
    if (mHandle) {
      InternetCloseHandle(mHandle);
    }
  }

  bool empty() { return (mHandle == nullptr); }
  HINTERNET get() { return mHandle; }

 private:
  HINTERNET mHandle;
};

const size_t kUrlComponentsSchemeLength = 256;
const size_t kUrlComponentsHostLength = 256;
const size_t kUrlComponentsPathLength = 256;

/**
 * Post the specified payload to a telemetry server
 *
 * @param url The URL of the telemetry server
 * @param payload The ping payload
 */
bool Post(const string& url, const string& payload) {
  char scheme[kUrlComponentsSchemeLength];
  char host[kUrlComponentsHostLength];
  char path[kUrlComponentsPathLength];

  URL_COMPONENTS components = {};
  components.dwStructSize = sizeof(components);
  components.lpszScheme = scheme;
  components.dwSchemeLength = kUrlComponentsSchemeLength;
  components.lpszHostName = host;
  components.dwHostNameLength = kUrlComponentsHostLength;
  components.lpszUrlPath = path;
  components.dwUrlPathLength = kUrlComponentsPathLength;

  if (!InternetCrackUrl(url.c_str(), url.size(), 0, &components)) {
    PINGSENDER_LOG("ERROR: Could not separate the URL components\n");
    return false;
  }

  if (!IsValidDestination(host)) {
    PINGSENDER_LOG("ERROR: Invalid destination host '%s'\n", host);
    return false;
  }

  ScopedHInternet internet(InternetOpen(kUserAgent,
                                        INTERNET_OPEN_TYPE_PRECONFIG,
                                        /* lpszProxyName */ NULL,
                                        /* lpszProxyBypass */ NULL,
                                        /* dwFlags */ 0));

  if (internet.empty()) {
    PINGSENDER_LOG("ERROR: Could not open wininet internet handle\n");
    return false;
  }

  DWORD timeout = static_cast<DWORD>(kConnectionTimeoutMs);
  bool rv = InternetSetOption(internet.get(), INTERNET_OPTION_CONNECT_TIMEOUT,
                              &timeout, sizeof(timeout));
  if (!rv) {
    PINGSENDER_LOG("ERROR: Could not set the connection timeout\n");
    return false;
  }

  ScopedHInternet connection(
      InternetConnect(internet.get(), host, components.nPort,
                      /* lpszUsername */ NULL,
                      /* lpszPassword */ NULL, INTERNET_SERVICE_HTTP,
                      /* dwFlags */ 0,
                      /* dwContext */ NULL));

  if (connection.empty()) {
    PINGSENDER_LOG("ERROR: Could not connect\n");
    return false;
  }

  DWORD flags = ((strcmp(scheme, "https") == 0) ? INTERNET_FLAG_SECURE : 0) |
                INTERNET_FLAG_NO_COOKIES;
  ScopedHInternet request(HttpOpenRequest(connection.get(), "POST", path,
                                          /* lpszVersion */ NULL,
                                          /* lpszReferer */ NULL,
                                          /* lplpszAcceptTypes */ NULL, flags,
                                          /* dwContext */ NULL));

  if (request.empty()) {
    PINGSENDER_LOG("ERROR: Could not open HTTP POST request\n");
    return false;
  }

  // Build a string containing all the headers.
  std::string headers = GenerateDateHeader() + "\r\n";
  headers += kCustomVersionHeader;
  headers += "\r\n";
  headers += kContentEncodingHeader;

  rv = HttpSendRequest(request.get(), headers.c_str(), -1L,
                       (LPVOID)payload.c_str(), payload.size());
  if (!rv) {
    PINGSENDER_LOG("ERROR: Could not execute HTTP POST request\n");
    return false;
  }

  // HttpSendRequest doesn't fail if we hit an HTTP error, so manually check
  // for errors. Please note that this is not needed on the Linux/MacOS version
  // of the pingsender, as libcurl already automatically fails on HTTP errors.
  DWORD statusCode = 0;
  DWORD bufferLength = sizeof(DWORD);
  rv = HttpQueryInfo(
      request.get(),
      /* dwInfoLevel */ HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
      /* lpvBuffer */ &statusCode,
      /* lpdwBufferLength */ &bufferLength,
      /* lpdwIndex */ NULL);
  if (!rv) {
    PINGSENDER_LOG("ERROR: Could not get the HTTP status code\n");
    return false;
  }

  if (statusCode != 200) {
    PINGSENDER_LOG("ERROR: Error submitting the HTTP request: code %lu\n",
                   statusCode);
    return false;
  }

  return rv;
}

}  // namespace PingSender
