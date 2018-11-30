/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cerrno>
#include <cstring>
#include <dlfcn.h>
#include <string>
#include <unistd.h>
#include "mozilla/Unused.h"
#include "third_party/curl/curl.h"

namespace PingSender {

using std::string;

using mozilla::Unused;

/**
 * A simple wrapper around libcurl "easy" functions. Provides RAII opening
 * and initialization of the curl library
 */
class CurlWrapper {
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
  curl_slist* (*slist_append)(curl_slist*, const char*);
  void (*slist_free_all)(curl_slist*);
  const char* (*easy_strerror)(CURLcode);
  void (*easy_cleanup)(CURL*);
  void (*global_cleanup)(void);

 private:
  void* mLib;
  void* mCurl;
};

CurlWrapper::CurlWrapper()
    : easy_init(nullptr),
      easy_setopt(nullptr),
      easy_perform(nullptr),
      easy_getinfo(nullptr),
      slist_append(nullptr),
      slist_free_all(nullptr),
      easy_strerror(nullptr),
      easy_cleanup(nullptr),
      global_cleanup(nullptr),
      mLib(nullptr),
      mCurl(nullptr) {}

CurlWrapper::~CurlWrapper() {
  if (mLib) {
    if (mCurl && easy_cleanup) {
      easy_cleanup(mCurl);
    }

    if (global_cleanup) {
      global_cleanup();
    }

    dlclose(mLib);
  }
}

bool CurlWrapper::Init() {
  const char* libcurlPaths[] = {
#if defined(XP_MACOSX)
    // macOS
    "/usr/lib/libcurl.dylib",
    "/usr/lib/libcurl.4.dylib",
    "/usr/lib/libcurl.3.dylib",
#else  // Linux, *BSD, ...
    "libcurl.so",
    "libcurl.so.4",
    // Debian gives libcurl a different name when it is built against GnuTLS
    "libcurl-gnutls.so",
    "libcurl-gnutls.so.4",
    // Older versions in case we find nothing better
    "libcurl.so.3",
    "libcurl-gnutls.so.3",  // See above for Debian
#endif
  };

  // libcurl might show up under different names & paths, try them all until
  // we find it
  for (const char* libname : libcurlPaths) {
    mLib = dlopen(libname, RTLD_NOW);

    if (mLib) {
      break;
    }
  }

  if (!mLib) {
    PINGSENDER_LOG("ERROR: Could not find libcurl\n");
    return false;
  }

  *(void**)(&easy_init) = dlsym(mLib, "curl_easy_init");
  *(void**)(&easy_setopt) = dlsym(mLib, "curl_easy_setopt");
  *(void**)(&easy_perform) = dlsym(mLib, "curl_easy_perform");
  *(void**)(&easy_getinfo) = dlsym(mLib, "curl_easy_getinfo");
  *(void**)(&slist_append) = dlsym(mLib, "curl_slist_append");
  *(void**)(&slist_free_all) = dlsym(mLib, "curl_slist_free_all");
  *(void**)(&easy_strerror) = dlsym(mLib, "curl_easy_strerror");
  *(void**)(&easy_cleanup) = dlsym(mLib, "curl_easy_cleanup");
  *(void**)(&global_cleanup) = dlsym(mLib, "curl_global_cleanup");

  if (!easy_init || !easy_setopt || !easy_perform || !easy_getinfo ||
      !slist_append || !slist_free_all || !easy_strerror || !easy_cleanup ||
      !global_cleanup) {
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

static size_t DummyWriteCallback(char* ptr, size_t size, size_t nmemb,
                                 void* userdata) {
  Unused << ptr;
  Unused << size;
  Unused << nmemb;
  Unused << userdata;

  return size * nmemb;
}

bool CurlWrapper::Post(const string& url, const string& payload) {
  easy_setopt(mCurl, CURLOPT_URL, url.c_str());
  easy_setopt(mCurl, CURLOPT_USERAGENT, kUserAgent);
  easy_setopt(mCurl, CURLOPT_WRITEFUNCTION, DummyWriteCallback);

  // Build the date header.
  std::string dateHeader = GenerateDateHeader();

  // Set the custom headers.
  curl_slist* headerChunk = nullptr;
  headerChunk = slist_append(headerChunk, kCustomVersionHeader);
  headerChunk = slist_append(headerChunk, kContentEncodingHeader);
  headerChunk = slist_append(headerChunk, dateHeader.c_str());
  CURLcode err = easy_setopt(mCurl, CURLOPT_HTTPHEADER, headerChunk);
  if (err != CURLE_OK) {
    PINGSENDER_LOG("ERROR: Failed to set HTTP headers, %s\n",
                   easy_strerror(err));
    slist_free_all(headerChunk);
    return false;
  }

  // Set the size of the POST data
  easy_setopt(mCurl, CURLOPT_POSTFIELDSIZE, payload.length());

  // Set the contents of the POST data
  easy_setopt(mCurl, CURLOPT_POSTFIELDS, payload.c_str());

  // Fail if the server returns a 4xx code
  easy_setopt(mCurl, CURLOPT_FAILONERROR, 1);

  // Override the default connection timeout, which is 5 minutes.
  easy_setopt(mCurl, CURLOPT_CONNECTTIMEOUT_MS, kConnectionTimeoutMs);

  // Block until the operation is performend. Ignore the response, if the POST
  // fails we can't do anything about it.
  err = easy_perform(mCurl);
  // Whatever happens, we want to clean up the header memory.
  slist_free_all(headerChunk);

  if (err != CURLE_OK) {
    PINGSENDER_LOG("ERROR: Failed to send HTTP request, %s\n",
                   easy_strerror(err));
    return false;
  }

  return true;
}

bool Post(const string& url, const string& payload) {
  CurlWrapper curl;

  if (!curl.Init()) {
    return false;
  }

  return curl.Post(url, payload);
}

}  // namespace PingSender
