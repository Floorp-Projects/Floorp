/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
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
  bool IsValidDestination(const string& url);
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

  CURLU* (*curl_url)();
  CURLUcode (*curl_url_get)(CURLU*, CURLUPart, char**, unsigned int);
  CURLUcode (*curl_url_set)(CURLU*, CURLUPart, const char*, unsigned int);
  void (*curl_free)(char*);
  void (*curl_url_cleanup)(CURLU*);

 private:
  void* mLib;
  void* mCurl;
  bool mCanParseUrl;
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
      curl_url(nullptr),
      curl_url_get(nullptr),
      curl_url_set(nullptr),
      curl_free(nullptr),
      curl_url_cleanup(nullptr),
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

  *(void**)(&curl_url) = dlsym(mLib, "curl_url");
  *(void**)(&curl_url_set) = dlsym(mLib, "curl_url_set");
  *(void**)(&curl_url_get) = dlsym(mLib, "curl_url_get");
  *(void**)(&curl_free) = dlsym(mLib, "curl_free");
  *(void**)(&curl_url_cleanup) = dlsym(mLib, "curl_url_cleanup");

  if (!easy_init || !easy_setopt || !easy_perform || !easy_getinfo ||
      !slist_append || !slist_free_all || !easy_strerror || !easy_cleanup ||
      !global_cleanup) {
    PINGSENDER_LOG("ERROR: libcurl is missing one of the required symbols\n");
    return false;
  }

  mCanParseUrl = true;
  if (!curl_url || !curl_url_get || !curl_url_set || !curl_free ||
      !curl_url_cleanup) {
    mCanParseUrl = false;
    PINGSENDER_LOG("WARNING: Do not have url parsing functions in libcurl\n");
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

// If we can't use curl's URL parsing (which is safer) we have to fallback
// to this handwritten one (which is only as safe as we are clever.)
bool FallbackIsValidDestination(const string& aUrl) {
  // Lowercase the url
  string url = aUrl;
  std::transform(url.begin(), url.end(), url.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  // Strip off the scheme in the beginning
  if (url.find("http://") == 0) {
    url = url.substr(7);
  } else if (url.find("https://") == 0) {
    url = url.substr(8);
  }

  // Remove any user information. If a @ appeared in the userinformation,
  // it would need to be encoded.
  unsigned long atStart = url.find_first_of("@");
  url = (atStart == std::string::npos) ? url : url.substr(atStart + 1);

  // Remove any path or fragment information, leaving us with a url that may
  // contain host, and port.
  unsigned long fragStart = url.find_first_of("#");
  url = (fragStart == std::string::npos) ? url : url.substr(0, fragStart);
  unsigned long pathStart = url.find_first_of("/");
  url = (pathStart == std::string::npos) ? url : url.substr(0, pathStart);

  // Remove the port, because we run tests targeting localhost:port
  unsigned long portStart = url.find_last_of(":");
  url = (portStart == std::string::npos) ? url : url.substr(0, portStart);

  return ::IsValidDestination(url);
}

bool CurlWrapper::IsValidDestination(const string& aUrl) {
  if (!mCanParseUrl) {
    return FallbackIsValidDestination(aUrl);
  }

  bool ret = false;
  CURLU* h = curl_url();
  if (!h) {
    return FallbackIsValidDestination(aUrl);
  }

  if (CURLUE_OK != curl_url_set(h, CURLUPART_URL, aUrl.c_str(), 0)) {
    goto cleanup;
  }

  char* host;
  if (CURLUE_OK != curl_url_get(h, CURLUPART_HOST, &host, 0)) {
    goto cleanup;
  }

  ret = ::IsValidDestination(host);
  curl_free(host);

cleanup:
  curl_url_cleanup(h);
  return ret;
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
  if (!curl.IsValidDestination(url)) {
    PINGSENDER_LOG("ERROR: Invalid destination host\n");
    return false;
  }

  return curl.Post(url, payload);
}

void ChangeCurrentWorkingDirectory(const string& pingPath) {
  // This is not needed under Linux/macOS
}

}  // namespace PingSender
