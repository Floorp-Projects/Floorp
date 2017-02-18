/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cerrno>
#include <cstring>
#include <string>

#include <dlfcn.h>
#include <unistd.h>

#include "third_party/curl/curl.h"

namespace PingSender {

using std::string;

/**
 * A simple wrapper around libcurl "easy" functions. Provides RAII opening
 * and initialization of the curl library
 */
class CurlWrapper
{
public:
  CurlWrapper();
  ~CurlWrapper();
  bool Init();
  bool Post(const string& url, const string& payload);

  // libcurl functions
  CURL* (*easy_init)(void);
  CURLcode (*easy_setopt)(CURL*, CURLoption, ...);
  CURLcode (*easy_perform)(CURL*);
  CURLcode (*easy_getinfo)(CURL*, CURLINFO, ...);
  const char* (*easy_strerror)(CURLcode);
  void (*easy_cleanup)(CURL*);

private:
  void* mLib;
  void* mCurl;
};

CurlWrapper::CurlWrapper()
  : easy_init(nullptr)
  , easy_setopt(nullptr)
  , easy_perform(nullptr)
  , easy_getinfo(nullptr)
  , easy_strerror(nullptr)
  , easy_cleanup(nullptr)
  , mLib(nullptr)
  , mCurl(nullptr)
{}

CurlWrapper::~CurlWrapper()
{
  if(mLib) {
    if(mCurl && easy_cleanup) {
      easy_cleanup(mCurl);
    }

    dlclose(mLib);
  }
}

bool
CurlWrapper::Init()
{
  // libcurl might show up under different names, try them all until we find it
  const char* libcurlNames[] = {
    "libcurl.so",
    "libcurl.so.4",
    // Debian gives libcurl a different name when it is built against GnuTLS
    "libcurl-gnutls.so.4",
    // Older libcurl if we can't find anything better
    "libcurl.so.3",
#ifndef HAVE_64BIT_BUILD
    // 32-bit versions on 64-bit hosts
    "/usr/lib32/libcurl.so",
    "/usr/lib32/libcurl.so.4",
    "/usr/lib32/libcurl-gnutls.so.4",
    "/usr/lib32/libcurl.so.3",
#endif
    // macOS
    "libcurl.dylib",
    "libcurl.4.dylib",
    "libcurl.3.dylib"
  };

  for (const char* libname : libcurlNames) {
    mLib = dlopen(libname, RTLD_NOW);

    if (mLib) {
      break;
    }
  }

  if (!mLib) {
    PINGSENDER_LOG("ERROR: Could not find libcurl\n");
    return false;
  }

  *(void**) (&easy_init) = dlsym(mLib, "curl_easy_init");
  *(void**) (&easy_setopt) = dlsym(mLib, "curl_easy_setopt");
  *(void**) (&easy_perform) = dlsym(mLib, "curl_easy_perform");
  *(void**) (&easy_getinfo) = dlsym(mLib, "curl_easy_getinfo");
  *(void**) (&easy_strerror) = dlsym(mLib, "curl_easy_strerror");
  *(void**) (&easy_cleanup) = dlsym(mLib, "curl_easy_cleanup");

  if (!easy_init ||
      !easy_setopt ||
      !easy_perform ||
      !easy_getinfo ||
      !easy_strerror ||
      !easy_cleanup) {
    PINGSENDER_LOG("ERROR: libcurl is missing one of the required symbols\n");
    return false;
  }

  mCurl = easy_init();

  if (!mCurl) {
    PINGSENDER_LOG("ERROR: Could not initialize libcurl\n");
    return false;
  }

  return true;
}

bool
CurlWrapper::Post(const string& url, const string& payload)
{
  easy_setopt(mCurl, CURLOPT_URL, url.c_str());
  easy_setopt(mCurl, CURLOPT_USERAGENT, kUserAgent);

  // Set the size of the POST data
  easy_setopt(mCurl, CURLOPT_POSTFIELDSIZE, payload.length());

  // Set the contents of the POST data
  easy_setopt(mCurl, CURLOPT_POSTFIELDS, payload.c_str());

  // Fail if the server returns a 4xx code
  easy_setopt(mCurl, CURLOPT_FAILONERROR, 1);

  // Block until the operation is performend. Ignore the response, if the POST
  // fails we can't do anything about it.
  CURLcode err = easy_perform(mCurl);

  if (err != CURLE_OK) {
    PINGSENDER_LOG("ERROR: Failed to send HTTP request, %s\n",
                   easy_strerror(err));
    return false;
  }

  return true;
}

bool
Post(const string& url, const string& payload)
{
  CurlWrapper curl;

  if (!curl.Init()) {
    return false;
  }

  return curl.Post(url, payload);
}

} // namespace PingSender
