/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "nsHttpRequestHead.h"
#include "nsIHttpHeaderVisitor.h"

//-----------------------------------------------------------------------------
// nsHttpRequestHead
//-----------------------------------------------------------------------------

namespace mozilla {
namespace net {

nsHttpRequestHead::nsHttpRequestHead() { MOZ_COUNT_CTOR(nsHttpRequestHead); }

nsHttpRequestHead::nsHttpRequestHead(const nsHttpRequestHead& aRequestHead)
    : mRecursiveMutex("nsHttpRequestHead.mRecursiveMutex"),
      mInVisitHeaders(false) {
  nsHttpRequestHead& other = const_cast<nsHttpRequestHead&>(aRequestHead);
  RecursiveMutexAutoLock monitor(other.mRecursiveMutex);

  mHeaders = other.mHeaders;
  mMethod = other.mMethod;
  mVersion = other.mVersion;
  mRequestURI = other.mRequestURI;
  mPath = other.mPath;
  mOrigin = other.mOrigin;
  mParsedMethod = other.mParsedMethod;
  mHTTPS = other.mHTTPS;
  mInVisitHeaders = false;
}

nsHttpRequestHead::~nsHttpRequestHead() { MOZ_COUNT_DTOR(nsHttpRequestHead); }

nsHttpRequestHead& nsHttpRequestHead::operator=(
    const nsHttpRequestHead& aRequestHead) {
  nsHttpRequestHead& other = const_cast<nsHttpRequestHead&>(aRequestHead);
  RecursiveMutexAutoLock monitorOther(other.mRecursiveMutex);
  RecursiveMutexAutoLock monitor(mRecursiveMutex);

  mHeaders = other.mHeaders;
  mMethod = other.mMethod;
  mVersion = other.mVersion;
  mRequestURI = other.mRequestURI;
  mPath = other.mPath;
  mOrigin = other.mOrigin;
  mParsedMethod = other.mParsedMethod;
  mHTTPS = other.mHTTPS;
  mInVisitHeaders = false;
  return *this;
}

// Don't use this function. It is only used by HttpChannelParent to avoid
// copying of request headers!!!
const nsHttpHeaderArray& nsHttpRequestHead::Headers() const {
  nsHttpRequestHead& curr = const_cast<nsHttpRequestHead&>(*this);
  curr.mRecursiveMutex.AssertCurrentThreadIn();
  return mHeaders;
}

void nsHttpRequestHead::SetHeaders(const nsHttpHeaderArray& aHeaders) {
  RecursiveMutexAutoLock mon(mRecursiveMutex);
  mHeaders = aHeaders;
}

void nsHttpRequestHead::SetVersion(HttpVersion version) {
  RecursiveMutexAutoLock mon(mRecursiveMutex);
  mVersion = version;
}

void nsHttpRequestHead::SetRequestURI(const nsACString& s) {
  RecursiveMutexAutoLock mon(mRecursiveMutex);
  mRequestURI = s;
}

void nsHttpRequestHead::SetPath(const nsACString& s) {
  RecursiveMutexAutoLock mon(mRecursiveMutex);
  mPath = s;
}

uint32_t nsHttpRequestHead::HeaderCount() {
  RecursiveMutexAutoLock mon(mRecursiveMutex);
  return mHeaders.Count();
}

nsresult nsHttpRequestHead::VisitHeaders(
    nsIHttpHeaderVisitor* visitor,
    nsHttpHeaderArray::VisitorFilter
        filter /* = nsHttpHeaderArray::eFilterAll*/) {
  RecursiveMutexAutoLock mon(mRecursiveMutex);
  mInVisitHeaders = true;
  nsresult rv = mHeaders.VisitHeaders(visitor, filter);
  mInVisitHeaders = false;
  return rv;
}

void nsHttpRequestHead::Method(nsACString& aMethod) {
  RecursiveMutexAutoLock mon(mRecursiveMutex);
  aMethod = mMethod;
}

HttpVersion nsHttpRequestHead::Version() {
  RecursiveMutexAutoLock mon(mRecursiveMutex);
  return mVersion;
}

void nsHttpRequestHead::RequestURI(nsACString& aRequestURI) {
  RecursiveMutexAutoLock mon(mRecursiveMutex);
  aRequestURI = mRequestURI;
}

void nsHttpRequestHead::Path(nsACString& aPath) {
  RecursiveMutexAutoLock mon(mRecursiveMutex);
  aPath = mPath.IsEmpty() ? mRequestURI : mPath;
}

void nsHttpRequestHead::SetHTTPS(bool val) {
  RecursiveMutexAutoLock mon(mRecursiveMutex);
  mHTTPS = val;
}

void nsHttpRequestHead::Origin(nsACString& aOrigin) {
  RecursiveMutexAutoLock mon(mRecursiveMutex);
  aOrigin = mOrigin;
}

nsresult nsHttpRequestHead::SetHeader(const nsACString& h, const nsACString& v,
                                      bool m /*= false*/) {
  RecursiveMutexAutoLock mon(mRecursiveMutex);

  if (mInVisitHeaders) {
    return NS_ERROR_FAILURE;
  }

  return mHeaders.SetHeader(h, v, m,
                            nsHttpHeaderArray::eVarietyRequestOverride);
}

nsresult nsHttpRequestHead::SetHeader(const nsHttpAtom& h, const nsACString& v,
                                      bool m /*= false*/) {
  RecursiveMutexAutoLock mon(mRecursiveMutex);

  if (mInVisitHeaders) {
    return NS_ERROR_FAILURE;
  }

  return mHeaders.SetHeader(h, v, m,
                            nsHttpHeaderArray::eVarietyRequestOverride);
}

nsresult nsHttpRequestHead::SetHeader(
    const nsHttpAtom& h, const nsACString& v, bool m,
    nsHttpHeaderArray::HeaderVariety variety) {
  RecursiveMutexAutoLock mon(mRecursiveMutex);

  if (mInVisitHeaders) {
    return NS_ERROR_FAILURE;
  }

  return mHeaders.SetHeader(h, v, m, variety);
}

nsresult nsHttpRequestHead::SetEmptyHeader(const nsACString& h) {
  RecursiveMutexAutoLock mon(mRecursiveMutex);

  if (mInVisitHeaders) {
    return NS_ERROR_FAILURE;
  }

  return mHeaders.SetEmptyHeader(h, nsHttpHeaderArray::eVarietyRequestOverride);
}

nsresult nsHttpRequestHead::GetHeader(const nsHttpAtom& h, nsACString& v) {
  v.Truncate();
  RecursiveMutexAutoLock mon(mRecursiveMutex);
  return mHeaders.GetHeader(h, v);
}

nsresult nsHttpRequestHead::ClearHeader(const nsHttpAtom& h) {
  RecursiveMutexAutoLock mon(mRecursiveMutex);

  if (mInVisitHeaders) {
    return NS_ERROR_FAILURE;
  }

  mHeaders.ClearHeader(h);
  return NS_OK;
}

void nsHttpRequestHead::ClearHeaders() {
  RecursiveMutexAutoLock mon(mRecursiveMutex);

  if (mInVisitHeaders) {
    return;
  }

  mHeaders.Clear();
}

bool nsHttpRequestHead::HasHeader(const nsHttpAtom& h) {
  RecursiveMutexAutoLock mon(mRecursiveMutex);
  return mHeaders.HasHeader(h);
}

bool nsHttpRequestHead::HasHeaderValue(const nsHttpAtom& h, const char* v) {
  RecursiveMutexAutoLock mon(mRecursiveMutex);
  return mHeaders.HasHeaderValue(h, v);
}

nsresult nsHttpRequestHead::SetHeaderOnce(const nsHttpAtom& h, const char* v,
                                          bool merge /*= false */) {
  RecursiveMutexAutoLock mon(mRecursiveMutex);

  if (mInVisitHeaders) {
    return NS_ERROR_FAILURE;
  }

  if (!merge || !mHeaders.HasHeaderValue(h, v)) {
    return mHeaders.SetHeader(h, nsDependentCString(v), merge,
                              nsHttpHeaderArray::eVarietyRequestOverride);
  }
  return NS_OK;
}

nsHttpRequestHead::ParsedMethodType nsHttpRequestHead::ParsedMethod() {
  RecursiveMutexAutoLock mon(mRecursiveMutex);
  return mParsedMethod;
}

bool nsHttpRequestHead::EqualsMethod(ParsedMethodType aType) {
  RecursiveMutexAutoLock mon(mRecursiveMutex);
  return mParsedMethod == aType;
}

void nsHttpRequestHead::ParseHeaderSet(const char* buffer) {
  RecursiveMutexAutoLock mon(mRecursiveMutex);
  nsHttpAtom hdr;
  nsAutoCString headerNameOriginal;
  nsAutoCString val;
  while (buffer) {
    const char* eof = strchr(buffer, '\r');
    if (!eof) {
      break;
    }
    if (NS_SUCCEEDED(nsHttpHeaderArray::ParseHeaderLine(
            nsDependentCSubstring(buffer, eof - buffer), &hdr,
            &headerNameOriginal, &val))) {
      DebugOnly<nsresult> rv =
          mHeaders.SetHeaderFromNet(hdr, headerNameOriginal, val, false);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
    buffer = eof + 1;
    if (*buffer == '\n') {
      buffer++;
    }
  }
}

bool nsHttpRequestHead::IsHTTPS() {
  RecursiveMutexAutoLock mon(mRecursiveMutex);
  return mHTTPS;
}

void nsHttpRequestHead::SetMethod(const nsACString& method) {
  RecursiveMutexAutoLock mon(mRecursiveMutex);

  mMethod = method;
  ParseMethod(mMethod, mParsedMethod);
}

// static
void nsHttpRequestHead::ParseMethod(const nsCString& aRawMethod,
                                    ParsedMethodType& aParsedMethod) {
  aParsedMethod = kMethod_Custom;
  if (!strcmp(aRawMethod.get(), "GET")) {
    aParsedMethod = kMethod_Get;
  } else if (!strcmp(aRawMethod.get(), "POST")) {
    aParsedMethod = kMethod_Post;
  } else if (!strcmp(aRawMethod.get(), "OPTIONS")) {
    aParsedMethod = kMethod_Options;
  } else if (!strcmp(aRawMethod.get(), "CONNECT")) {
    aParsedMethod = kMethod_Connect;
  } else if (!strcmp(aRawMethod.get(), "HEAD")) {
    aParsedMethod = kMethod_Head;
  } else if (!strcmp(aRawMethod.get(), "PUT")) {
    aParsedMethod = kMethod_Put;
  } else if (!strcmp(aRawMethod.get(), "TRACE")) {
    aParsedMethod = kMethod_Trace;
  }
}

void nsHttpRequestHead::SetOrigin(const nsACString& scheme,
                                  const nsACString& host, int32_t port) {
  RecursiveMutexAutoLock mon(mRecursiveMutex);
  mOrigin.Assign(scheme);
  mOrigin.AppendLiteral("://");
  mOrigin.Append(host);
  if (port >= 0) {
    mOrigin.AppendLiteral(":");
    mOrigin.AppendInt(port);
  }
}

bool nsHttpRequestHead::IsSafeMethod() {
  RecursiveMutexAutoLock mon(mRecursiveMutex);
  // This code will need to be extended for new safe methods, otherwise
  // they'll default to "not safe".
  if ((mParsedMethod == kMethod_Get) || (mParsedMethod == kMethod_Head) ||
      (mParsedMethod == kMethod_Options) || (mParsedMethod == kMethod_Trace)) {
    return true;
  }

  if (mParsedMethod != kMethod_Custom) {
    return false;
  }

  return (!strcmp(mMethod.get(), "PROPFIND") ||
          !strcmp(mMethod.get(), "REPORT") || !strcmp(mMethod.get(), "SEARCH"));
}

void nsHttpRequestHead::Flatten(nsACString& buf, bool pruneProxyHeaders) {
  RecursiveMutexAutoLock mon(mRecursiveMutex);
  // note: the first append is intentional.

  buf.Append(mMethod);
  buf.Append(' ');
  buf.Append(mRequestURI);
  buf.AppendLiteral(" HTTP/");

  switch (mVersion) {
    case HttpVersion::v1_1:
      buf.AppendLiteral("1.1");
      break;
    case HttpVersion::v0_9:
      buf.AppendLiteral("0.9");
      break;
    default:
      buf.AppendLiteral("1.0");
  }

  buf.AppendLiteral("\r\n");

  mHeaders.Flatten(buf, pruneProxyHeaders, false);
}

}  // namespace net
}  // namespace mozilla
