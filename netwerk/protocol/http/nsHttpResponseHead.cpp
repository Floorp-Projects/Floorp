/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=4 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "mozilla/Unused.h"
#include "nsHttpResponseHead.h"
#include "nsIHttpHeaderVisitor.h"
#include "nsPrintfCString.h"
#include "prtime.h"
#include "nsCRT.h"
#include "nsURLHelper.h"
#include "CacheControlParser.h"
#include <algorithm>

namespace mozilla {
namespace net {

//-----------------------------------------------------------------------------
// nsHttpResponseHead <public>
//-----------------------------------------------------------------------------

// Note that the code below MUST be synchronized with the IPC
// serialization/deserialization in PHttpChannelParams.h.
nsHttpResponseHead::nsHttpResponseHead(const nsHttpResponseHead& aOther) {
  nsHttpResponseHead& other = const_cast<nsHttpResponseHead&>(aOther);
  RecursiveMutexAutoLock monitor(other.mRecursiveMutex);

  mHeaders = other.mHeaders;
  mVersion = other.mVersion;
  mStatus = other.mStatus;
  mStatusText = other.mStatusText;
  mContentLength = other.mContentLength;
  mContentType = other.mContentType;
  mContentCharset = other.mContentCharset;
  mHasCacheControl = other.mHasCacheControl;
  mCacheControlPublic = other.mCacheControlPublic;
  mCacheControlPrivate = other.mCacheControlPrivate;
  mCacheControlNoStore = other.mCacheControlNoStore;
  mCacheControlNoCache = other.mCacheControlNoCache;
  mCacheControlImmutable = other.mCacheControlImmutable;
  mCacheControlStaleWhileRevalidateSet =
      other.mCacheControlStaleWhileRevalidateSet;
  mCacheControlStaleWhileRevalidate = other.mCacheControlStaleWhileRevalidate;
  mCacheControlMaxAgeSet = other.mCacheControlMaxAgeSet;
  mCacheControlMaxAge = other.mCacheControlMaxAge;
  mPragmaNoCache = other.mPragmaNoCache;
}

nsHttpResponseHead& nsHttpResponseHead::operator=(
    const nsHttpResponseHead& aOther) {
  nsHttpResponseHead& other = const_cast<nsHttpResponseHead&>(aOther);
  RecursiveMutexAutoLock monitorOther(other.mRecursiveMutex);
  RecursiveMutexAutoLock monitor(mRecursiveMutex);

  mHeaders = other.mHeaders;
  mVersion = other.mVersion;
  mStatus = other.mStatus;
  mStatusText = other.mStatusText;
  mContentLength = other.mContentLength;
  mContentType = other.mContentType;
  mContentCharset = other.mContentCharset;
  mHasCacheControl = other.mHasCacheControl;
  mCacheControlPublic = other.mCacheControlPublic;
  mCacheControlPrivate = other.mCacheControlPrivate;
  mCacheControlNoStore = other.mCacheControlNoStore;
  mCacheControlNoCache = other.mCacheControlNoCache;
  mCacheControlImmutable = other.mCacheControlImmutable;
  mCacheControlStaleWhileRevalidateSet =
      other.mCacheControlStaleWhileRevalidateSet;
  mCacheControlStaleWhileRevalidate = other.mCacheControlStaleWhileRevalidate;
  mCacheControlMaxAgeSet = other.mCacheControlMaxAgeSet;
  mCacheControlMaxAge = other.mCacheControlMaxAge;
  mPragmaNoCache = other.mPragmaNoCache;

  return *this;
}

HttpVersion nsHttpResponseHead::Version() {
  RecursiveMutexAutoLock monitor(mRecursiveMutex);
  return mVersion;
}

uint16_t nsHttpResponseHead::Status() const {
  RecursiveMutexAutoLock monitor(mRecursiveMutex);
  return mStatus;
}

void nsHttpResponseHead::StatusText(nsACString& aStatusText) {
  RecursiveMutexAutoLock monitor(mRecursiveMutex);
  aStatusText = mStatusText;
}

int64_t nsHttpResponseHead::ContentLength() {
  RecursiveMutexAutoLock monitor(mRecursiveMutex);
  return mContentLength;
}

void nsHttpResponseHead::ContentType(nsACString& aContentType) const {
  RecursiveMutexAutoLock monitor(mRecursiveMutex);
  aContentType = mContentType;
}

void nsHttpResponseHead::ContentCharset(nsACString& aContentCharset) {
  RecursiveMutexAutoLock monitor(mRecursiveMutex);
  aContentCharset = mContentCharset;
}

bool nsHttpResponseHead::Public() {
  RecursiveMutexAutoLock monitor(mRecursiveMutex);
  return mCacheControlPublic;
}

bool nsHttpResponseHead::Private() {
  RecursiveMutexAutoLock monitor(mRecursiveMutex);
  return mCacheControlPrivate;
}

bool nsHttpResponseHead::NoStore() {
  RecursiveMutexAutoLock monitor(mRecursiveMutex);
  return mCacheControlNoStore;
}

bool nsHttpResponseHead::NoCache() {
  RecursiveMutexAutoLock monitor(mRecursiveMutex);
  return NoCache_locked();
}

bool nsHttpResponseHead::Immutable() {
  RecursiveMutexAutoLock monitor(mRecursiveMutex);
  return mCacheControlImmutable;
}

nsresult nsHttpResponseHead::SetHeader(const nsACString& hdr,
                                       const nsACString& val, bool merge) {
  RecursiveMutexAutoLock monitor(mRecursiveMutex);

  if (mInVisitHeaders) {
    return NS_ERROR_FAILURE;
  }

  nsHttpAtom atom = nsHttp::ResolveAtom(hdr);
  if (!atom) {
    NS_WARNING("failed to resolve atom");
    return NS_ERROR_NOT_AVAILABLE;
  }

  return SetHeader_locked(atom, hdr, val, merge);
}

nsresult nsHttpResponseHead::SetHeader(const nsHttpAtom& hdr,
                                       const nsACString& val, bool merge) {
  RecursiveMutexAutoLock monitor(mRecursiveMutex);

  if (mInVisitHeaders) {
    return NS_ERROR_FAILURE;
  }

  return SetHeader_locked(hdr, ""_ns, val, merge);
}

nsresult nsHttpResponseHead::SetHeader_locked(const nsHttpAtom& atom,
                                              const nsACString& hdr,
                                              const nsACString& val,
                                              bool merge) {
  nsresult rv = mHeaders.SetHeader(atom, hdr, val, merge,
                                   nsHttpHeaderArray::eVarietyResponse);
  if (NS_FAILED(rv)) return rv;

  // respond to changes in these headers.  we need to reparse the entire
  // header since the change may have merged in additional values.
  if (atom == nsHttp::Cache_Control) {
    ParseCacheControl(mHeaders.PeekHeader(atom));
  } else if (atom == nsHttp::Pragma) {
    ParsePragma(mHeaders.PeekHeader(atom));
  }

  return NS_OK;
}

nsresult nsHttpResponseHead::GetHeader(const nsHttpAtom& h,
                                       nsACString& v) const {
  v.Truncate();
  RecursiveMutexAutoLock monitor(mRecursiveMutex);
  return mHeaders.GetHeader(h, v);
}

void nsHttpResponseHead::ClearHeader(const nsHttpAtom& h) {
  RecursiveMutexAutoLock monitor(mRecursiveMutex);
  mHeaders.ClearHeader(h);
}

void nsHttpResponseHead::ClearHeaders() {
  RecursiveMutexAutoLock monitor(mRecursiveMutex);
  mHeaders.Clear();
}

bool nsHttpResponseHead::HasHeaderValue(const nsHttpAtom& h, const char* v) {
  RecursiveMutexAutoLock monitor(mRecursiveMutex);
  return mHeaders.HasHeaderValue(h, v);
}

bool nsHttpResponseHead::HasHeader(const nsHttpAtom& h) const {
  RecursiveMutexAutoLock monitor(mRecursiveMutex);
  return mHeaders.HasHeader(h);
}

void nsHttpResponseHead::SetContentType(const nsACString& s) {
  RecursiveMutexAutoLock monitor(mRecursiveMutex);
  mContentType = s;
}

void nsHttpResponseHead::SetContentCharset(const nsACString& s) {
  RecursiveMutexAutoLock monitor(mRecursiveMutex);
  mContentCharset = s;
}

void nsHttpResponseHead::SetContentLength(int64_t len) {
  RecursiveMutexAutoLock monitor(mRecursiveMutex);

  mContentLength = len;
  if (len < 0) {
    mHeaders.ClearHeader(nsHttp::Content_Length);
  } else {
    DebugOnly<nsresult> rv = mHeaders.SetHeader(
        nsHttp::Content_Length, nsPrintfCString("%" PRId64, len), false,
        nsHttpHeaderArray::eVarietyResponse);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }
}

void nsHttpResponseHead::Flatten(nsACString& buf, bool pruneTransients) {
  RecursiveMutexAutoLock monitor(mRecursiveMutex);
  if (mVersion == HttpVersion::v0_9) return;

  buf.AppendLiteral("HTTP/");
  if (mVersion == HttpVersion::v3_0) {
    buf.AppendLiteral("3 ");
  } else if (mVersion == HttpVersion::v2_0) {
    buf.AppendLiteral("2 ");
  } else if (mVersion == HttpVersion::v1_1) {
    buf.AppendLiteral("1.1 ");
  } else {
    buf.AppendLiteral("1.0 ");
  }

  buf.Append(nsPrintfCString("%u", unsigned(mStatus)) + " "_ns + mStatusText +
             "\r\n"_ns);

  mHeaders.Flatten(buf, false, pruneTransients);
}

void nsHttpResponseHead::FlattenNetworkOriginalHeaders(nsACString& buf) {
  RecursiveMutexAutoLock monitor(mRecursiveMutex);
  if (mVersion == HttpVersion::v0_9) {
    return;
  }

  mHeaders.FlattenOriginalHeader(buf);
}

nsresult nsHttpResponseHead::ParseCachedHead(const char* block) {
  RecursiveMutexAutoLock monitor(mRecursiveMutex);
  LOG(("nsHttpResponseHead::ParseCachedHead [this=%p]\n", this));

  // this command works on a buffer as prepared by Flatten, as such it is
  // not very forgiving ;-)

  const char* p = strstr(block, "\r\n");
  if (!p) return NS_ERROR_UNEXPECTED;

  ParseStatusLine_locked(nsDependentCSubstring(block, p - block));

  do {
    block = p + 2;

    if (*block == 0) break;

    p = strstr(block, "\r\n");
    if (!p) return NS_ERROR_UNEXPECTED;

    Unused << ParseHeaderLine_locked(nsDependentCSubstring(block, p - block),
                                     false);

  } while (true);

  return NS_OK;
}

nsresult nsHttpResponseHead::ParseCachedOriginalHeaders(char* block) {
  RecursiveMutexAutoLock monitor(mRecursiveMutex);
  LOG(("nsHttpResponseHead::ParseCachedOriginalHeader [this=%p]\n", this));

  // this command works on a buffer as prepared by FlattenOriginalHeader,
  // as such it is not very forgiving ;-)

  if (!block) {
    return NS_ERROR_UNEXPECTED;
  }

  char* p = block;
  nsHttpAtom hdr;
  nsAutoCString headerNameOriginal;
  nsAutoCString val;
  nsresult rv;

  do {
    block = p;

    if (*block == 0) break;

    p = strstr(block, "\r\n");
    if (!p) return NS_ERROR_UNEXPECTED;

    *p = 0;
    if (NS_FAILED(nsHttpHeaderArray::ParseHeaderLine(
            nsDependentCString(block, p - block), &hdr, &headerNameOriginal,
            &val))) {
      return NS_OK;
    }

    rv = mHeaders.SetResponseHeaderFromCache(
        hdr, headerNameOriginal, val,
        nsHttpHeaderArray::eVarietyResponseNetOriginal);

    if (NS_FAILED(rv)) {
      return rv;
    }

    p = p + 2;
  } while (true);

  return NS_OK;
}

void nsHttpResponseHead::AssignDefaultStatusText() {
  LOG(("response status line needs default reason phrase\n"));

  // if a http response doesn't contain a reason phrase, put one in based
  // on the status code. The reason phrase is totally meaningless so its
  // ok to have a default catch all here - but this makes debuggers and addons
  // a little saner to use if we don't map things to "404 OK" or other nonsense.
  // In particular, HTTP/2 does not use reason phrases at all so they need to
  // always be injected.

  switch (mStatus) {
      // start with the most common
    case 200:
      mStatusText.AssignLiteral("OK");
      break;
    case 404:
      mStatusText.AssignLiteral("Not Found");
      break;
    case 301:
      mStatusText.AssignLiteral("Moved Permanently");
      break;
    case 304:
      mStatusText.AssignLiteral("Not Modified");
      break;
    case 307:
      mStatusText.AssignLiteral("Temporary Redirect");
      break;
    case 500:
      mStatusText.AssignLiteral("Internal Server Error");
      break;

      // also well known
    case 100:
      mStatusText.AssignLiteral("Continue");
      break;
    case 101:
      mStatusText.AssignLiteral("Switching Protocols");
      break;
    case 201:
      mStatusText.AssignLiteral("Created");
      break;
    case 202:
      mStatusText.AssignLiteral("Accepted");
      break;
    case 203:
      mStatusText.AssignLiteral("Non Authoritative");
      break;
    case 204:
      mStatusText.AssignLiteral("No Content");
      break;
    case 205:
      mStatusText.AssignLiteral("Reset Content");
      break;
    case 206:
      mStatusText.AssignLiteral("Partial Content");
      break;
    case 207:
      mStatusText.AssignLiteral("Multi-Status");
      break;
    case 208:
      mStatusText.AssignLiteral("Already Reported");
      break;
    case 300:
      mStatusText.AssignLiteral("Multiple Choices");
      break;
    case 302:
      mStatusText.AssignLiteral("Found");
      break;
    case 303:
      mStatusText.AssignLiteral("See Other");
      break;
    case 305:
      mStatusText.AssignLiteral("Use Proxy");
      break;
    case 308:
      mStatusText.AssignLiteral("Permanent Redirect");
      break;
    case 400:
      mStatusText.AssignLiteral("Bad Request");
      break;
    case 401:
      mStatusText.AssignLiteral("Unauthorized");
      break;
    case 402:
      mStatusText.AssignLiteral("Payment Required");
      break;
    case 403:
      mStatusText.AssignLiteral("Forbidden");
      break;
    case 405:
      mStatusText.AssignLiteral("Method Not Allowed");
      break;
    case 406:
      mStatusText.AssignLiteral("Not Acceptable");
      break;
    case 407:
      mStatusText.AssignLiteral("Proxy Authentication Required");
      break;
    case 408:
      mStatusText.AssignLiteral("Request Timeout");
      break;
    case 409:
      mStatusText.AssignLiteral("Conflict");
      break;
    case 410:
      mStatusText.AssignLiteral("Gone");
      break;
    case 411:
      mStatusText.AssignLiteral("Length Required");
      break;
    case 412:
      mStatusText.AssignLiteral("Precondition Failed");
      break;
    case 413:
      mStatusText.AssignLiteral("Request Entity Too Large");
      break;
    case 414:
      mStatusText.AssignLiteral("Request URI Too Long");
      break;
    case 415:
      mStatusText.AssignLiteral("Unsupported Media Type");
      break;
    case 416:
      mStatusText.AssignLiteral("Requested Range Not Satisfiable");
      break;
    case 417:
      mStatusText.AssignLiteral("Expectation Failed");
      break;
    case 418:
      mStatusText.AssignLiteral("I'm a teapot");
      break;
    case 421:
      mStatusText.AssignLiteral("Misdirected Request");
      break;
    case 422:
      mStatusText.AssignLiteral("Unprocessable Entity");
      break;
    case 423:
      mStatusText.AssignLiteral("Locked");
      break;
    case 424:
      mStatusText.AssignLiteral("Failed Dependency");
      break;
    case 425:
      mStatusText.AssignLiteral("Too Early");
      break;
    case 426:
      mStatusText.AssignLiteral("Upgrade Required");
      break;
    case 428:
      mStatusText.AssignLiteral("Precondition Required");
      break;
    case 429:
      mStatusText.AssignLiteral("Too Many Requests");
      break;
    case 431:
      mStatusText.AssignLiteral("Request Header Fields Too Large");
      break;
    case 451:
      mStatusText.AssignLiteral("Unavailable For Legal Reasons");
      break;
    case 501:
      mStatusText.AssignLiteral("Not Implemented");
      break;
    case 502:
      mStatusText.AssignLiteral("Bad Gateway");
      break;
    case 503:
      mStatusText.AssignLiteral("Service Unavailable");
      break;
    case 504:
      mStatusText.AssignLiteral("Gateway Timeout");
      break;
    case 505:
      mStatusText.AssignLiteral("HTTP Version Unsupported");
      break;
    case 506:
      mStatusText.AssignLiteral("Variant Also Negotiates");
      break;
    case 507:
      mStatusText.AssignLiteral("Insufficient Storage ");
      break;
    case 508:
      mStatusText.AssignLiteral("Loop Detected");
      break;
    case 510:
      mStatusText.AssignLiteral("Not Extended");
      break;
    case 511:
      mStatusText.AssignLiteral("Network Authentication Required");
      break;
    default:
      mStatusText.AssignLiteral("No Reason Phrase");
      break;
  }
}

void nsHttpResponseHead::ParseStatusLine(const nsACString& line) {
  RecursiveMutexAutoLock monitor(mRecursiveMutex);
  ParseStatusLine_locked(line);
}

void nsHttpResponseHead::ParseStatusLine_locked(const nsACString& line) {
  //
  // Parse Status-Line:: HTTP-Version SP Status-Code SP Reason-Phrase CRLF
  //

  const char* start = line.BeginReading();
  const char* end = line.EndReading();
  const char* p = start;

  // HTTP-Version
  ParseVersion(start);

  int32_t index = line.FindChar(' ');

  if ((mVersion == HttpVersion::v0_9) || (index == -1)) {
    mStatus = 200;
    AssignDefaultStatusText();
  } else {
    // Status-Code
    p += index + 1;
    mStatus = (uint16_t)atoi(p);
    if (mStatus == 0) {
      LOG(("mal-formed response status; assuming status = 200\n"));
      mStatus = 200;
    }

    // Reason-Phrase is whatever is remaining of the line
    index = line.FindChar(' ', p - start);
    if (index == -1) {
      AssignDefaultStatusText();
    } else {
      p = start + index + 1;
      mStatusText = nsDependentCSubstring(p, end - p);
    }
  }

  LOG1(("Have status line [version=%u status=%u statusText=%s]\n",
        unsigned(mVersion), unsigned(mStatus), mStatusText.get()));
}

nsresult nsHttpResponseHead::ParseHeaderLine(const nsACString& line) {
  RecursiveMutexAutoLock monitor(mRecursiveMutex);
  return ParseHeaderLine_locked(line, true);
}

nsresult nsHttpResponseHead::ParseHeaderLine_locked(
    const nsACString& line, bool originalFromNetHeaders) {
  nsHttpAtom hdr;
  nsAutoCString headerNameOriginal;
  nsAutoCString val;

  if (NS_FAILED(nsHttpHeaderArray::ParseHeaderLine(
          line, &hdr, &headerNameOriginal, &val))) {
    return NS_OK;
  }
  nsresult rv;
  if (originalFromNetHeaders) {
    rv = mHeaders.SetHeaderFromNet(hdr, headerNameOriginal, val, true);
  } else {
    rv = mHeaders.SetResponseHeaderFromCache(
        hdr, headerNameOriginal, val, nsHttpHeaderArray::eVarietyResponse);
  }
  if (NS_FAILED(rv)) {
    return rv;
  }

  // leading and trailing LWS has been removed from |val|

  // handle some special case headers...
  if (hdr == nsHttp::Content_Length) {
    int64_t len;
    const char* ignored;
    // permit only a single value here.
    if (nsHttp::ParseInt64(val.get(), &ignored, &len)) {
      mContentLength = len;
    } else {
      // If this is a negative content length then just ignore it
      LOG(("invalid content-length! %s\n", val.get()));
    }
  } else if (hdr == nsHttp::Content_Type) {
    LOG(("ParseContentType [type=%s]\n", val.get()));
    bool dummy;
    net_ParseContentType(val, mContentType, mContentCharset, &dummy);
  } else if (hdr == nsHttp::Cache_Control) {
    ParseCacheControl(val.get());
  } else if (hdr == nsHttp::Pragma) {
    ParsePragma(val.get());
  }
  return NS_OK;
}

// From section 13.2.3 of RFC2616, we compute the current age of a cached
// response as follows:
//
//    currentAge = max(max(0, responseTime - dateValue), ageValue)
//               + now - requestTime
//
//    where responseTime == now
//
// This is typically a very small number.
//
nsresult nsHttpResponseHead::ComputeCurrentAge(uint32_t now,
                                               uint32_t requestTime,
                                               uint32_t* result) {
  RecursiveMutexAutoLock monitor(mRecursiveMutex);
  uint32_t dateValue;
  uint32_t ageValue;

  *result = 0;

  if (requestTime > now) {
    // for calculation purposes lets not allow the request to happen in the
    // future
    requestTime = now;
  }

  if (NS_FAILED(GetDateValue_locked(&dateValue))) {
    LOG(
        ("nsHttpResponseHead::ComputeCurrentAge [this=%p] "
         "Date response header not set!\n",
         this));
    // Assume we have a fast connection and that our clock
    // is in sync with the server.
    dateValue = now;
  }

  // Compute apparent age
  if (now > dateValue) *result = now - dateValue;

  // Compute corrected received age
  if (NS_SUCCEEDED(GetAgeValue_locked(&ageValue))) {
    *result = std::max(*result, ageValue);
  }

  // Compute current age
  *result += (now - requestTime);
  return NS_OK;
}

// From section 13.2.4 of RFC2616, we compute the freshness lifetime of a cached
// response as follows:
//
//     freshnessLifetime = max_age_value
// <or>
//     freshnessLifetime = expires_value - date_value
// <or>
//     freshnessLifetime = min(one-week,
//                             (date_value - last_modified_value) * 0.10)
// <or>
//     freshnessLifetime = 0
//
nsresult nsHttpResponseHead::ComputeFreshnessLifetime(uint32_t* result) {
  RecursiveMutexAutoLock monitor(mRecursiveMutex);
  *result = 0;

  // Try HTTP/1.1 style max-age directive...
  if (NS_SUCCEEDED(GetMaxAgeValue_locked(result))) return NS_OK;

  *result = 0;

  uint32_t date = 0, date2 = 0;
  if (NS_FAILED(GetDateValue_locked(&date))) {
    date = NowInSeconds();  // synthesize a date header if none exists
  }

  // Try HTTP/1.0 style expires header...
  if (NS_SUCCEEDED(GetExpiresValue_locked(&date2))) {
    if (date2 > date) *result = date2 - date;
    // the Expires header can specify a date in the past.
    return NS_OK;
  }

  // These responses can be cached indefinitely.
  if ((mStatus == 300) || (mStatus == 410) ||
      nsHttp::IsPermanentRedirect(mStatus)) {
    LOG(
        ("nsHttpResponseHead::ComputeFreshnessLifetime [this = %p] "
         "Assign an infinite heuristic lifetime\n",
         this));
    *result = uint32_t(-1);
    return NS_OK;
  }

  if (mStatus >= 400) {
    LOG(
        ("nsHttpResponseHead::ComputeFreshnessLifetime [this = %p] "
         "Do not calculate heuristic max-age for most responses >= 400\n",
         this));
    return NS_OK;
  }

  // From RFC 7234 Section 4.2.2, heuristics can only be used on responses
  // without explicit freshness whose status codes are defined as cacheable
  // by default, and those responses without explicit freshness that have been
  // marked as explicitly cacheable.
  // Note that |MustValidate| handled most of non-cacheable status codes.
  if ((mStatus == 302 || mStatus == 304 || mStatus == 307) &&
      !mCacheControlPublic && !mCacheControlPrivate) {
    LOG((
        "nsHttpResponseHead::ComputeFreshnessLifetime [this = %p] "
        "Do not calculate heuristic max-age for non-cacheable status code %u\n",
        this, unsigned(mStatus)));
    return NS_OK;
  }

  // Fallback on heuristic using last modified header...
  if (NS_SUCCEEDED(GetLastModifiedValue_locked(&date2))) {
    LOG(("using last-modified to determine freshness-lifetime\n"));
    LOG(("last-modified = %u, date = %u\n", date2, date));
    if (date2 <= date) {
      // this only makes sense if last-modified is actually in the past
      *result = (date - date2) / 10;
      const uint32_t kOneWeek = 60 * 60 * 24 * 7;
      *result = std::min(kOneWeek, *result);
      return NS_OK;
    }
  }

  LOG(
      ("nsHttpResponseHead::ComputeFreshnessLifetime [this = %p] "
       "Insufficient information to compute a non-zero freshness "
       "lifetime!\n",
       this));

  return NS_OK;
}

bool nsHttpResponseHead::MustValidate() {
  RecursiveMutexAutoLock monitor(mRecursiveMutex);
  LOG(("nsHttpResponseHead::MustValidate ??\n"));

  // Some response codes are cacheable, but the rest are not. This switch should
  // stay in sync with the list in nsHttpChannel::ContinueProcessResponse3
  switch (mStatus) {
      // Success codes
    case 200:
    case 203:
    case 204:
    case 206:
      // Cacheable redirects
    case 300:
    case 301:
    case 302:
    case 304:
    case 307:
    case 308:
      // Gone forever
    case 410:
      break;
      // Uncacheable redirects
    case 303:
    case 305:
      // Other known errors
    case 401:
    case 407:
    case 412:
    case 416:
    case 425:
    case 429:
    default:  // revalidate unknown error pages
      LOG(("Must validate since response is an uncacheable error page\n"));
      return true;
  }

  // The no-cache response header indicates that we must validate this
  // cached response before reusing.
  if (NoCache_locked()) {
    LOG(("Must validate since response contains 'no-cache' header\n"));
    return true;
  }

  // Likewise, if the response is no-store, then we must validate this
  // cached response before reusing.  NOTE: it may seem odd that a no-store
  // response may be cached, but indeed all responses are cached in order
  // to support File->SaveAs, View->PageSource, and other browser features.
  if (mCacheControlNoStore) {
    LOG(("Must validate since response contains 'no-store' header\n"));
    return true;
  }

  // Compare the Expires header to the Date header.  If the server sent an
  // Expires header with a timestamp in the past, then we must validate this
  // cached response before reusing.
  if (ExpiresInPast_locked()) {
    LOG(("Must validate since Expires < Date\n"));
    return true;
  }

  LOG(("no mandatory validation requirement\n"));
  return false;
}

bool nsHttpResponseHead::MustValidateIfExpired() {
  // according to RFC2616, section 14.9.4:
  //
  //  When the must-revalidate directive is present in a response received by a
  //  cache, that cache MUST NOT use the entry after it becomes stale to respond
  //  to a subsequent request without first revalidating it with the origin
  //  server.
  //
  return HasHeaderValue(nsHttp::Cache_Control, "must-revalidate");
}

bool nsHttpResponseHead::StaleWhileRevalidate(uint32_t now,
                                              uint32_t expiration) {
  RecursiveMutexAutoLock monitor(mRecursiveMutex);

  if (expiration <= 0 || !mCacheControlStaleWhileRevalidateSet) {
    return false;
  }

  // 'expiration' is the expiration time (an absolute unit), the swr window
  // extends the expiration time.
  CheckedInt<uint32_t> stallValidUntil = expiration;
  stallValidUntil += mCacheControlStaleWhileRevalidate;
  if (!stallValidUntil.isValid()) {
    // overflow means an indefinite stale window
    return true;
  }

  return now <= stallValidUntil.value();
}

bool nsHttpResponseHead::IsResumable() {
  RecursiveMutexAutoLock monitor(mRecursiveMutex);
  // even though some HTTP/1.0 servers may support byte range requests, we're
  // not going to bother with them, since those servers wouldn't understand
  // If-Range. Also, while in theory it may be possible to resume when the
  // status code is not 200, it is unlikely to be worth the trouble, especially
  // for non-2xx responses.
  return mStatus == 200 && mVersion >= HttpVersion::v1_1 &&
         mHeaders.PeekHeader(nsHttp::Content_Length) &&
         (mHeaders.PeekHeader(nsHttp::ETag) ||
          mHeaders.PeekHeader(nsHttp::Last_Modified)) &&
         mHeaders.HasHeaderValue(nsHttp::Accept_Ranges, "bytes");
}

bool nsHttpResponseHead::ExpiresInPast() {
  RecursiveMutexAutoLock monitor(mRecursiveMutex);
  return ExpiresInPast_locked();
}

bool nsHttpResponseHead::ExpiresInPast_locked() const {
  uint32_t maxAgeVal, expiresVal, dateVal;

  // Bug #203271. Ensure max-age directive takes precedence over Expires
  if (NS_SUCCEEDED(GetMaxAgeValue_locked(&maxAgeVal))) {
    return false;
  }

  return NS_SUCCEEDED(GetExpiresValue_locked(&expiresVal)) &&
         NS_SUCCEEDED(GetDateValue_locked(&dateVal)) && expiresVal < dateVal;
}

void nsHttpResponseHead::UpdateHeaders(nsHttpResponseHead* aOther) {
  LOG(("nsHttpResponseHead::UpdateHeaders [this=%p]\n", this));

  RecursiveMutexAutoLock monitor(mRecursiveMutex);
  RecursiveMutexAutoLock monitorOther(aOther->mRecursiveMutex);

  uint32_t i, count = aOther->mHeaders.Count();
  for (i = 0; i < count; ++i) {
    nsHttpAtom header;
    nsAutoCString headerNameOriginal;

    if (!aOther->mHeaders.PeekHeaderAt(i, header, headerNameOriginal)) {
      continue;
    }

    nsAutoCString val;
    if (NS_FAILED(aOther->GetHeader(header, val))) {
      continue;
    }

    // Ignore any hop-by-hop headers...
    if (header == nsHttp::Connection || header == nsHttp::Proxy_Connection ||
        header == nsHttp::Keep_Alive || header == nsHttp::Proxy_Authenticate ||
        header == nsHttp::Proxy_Authorization ||  // not a response header!
        header == nsHttp::TE || header == nsHttp::Trailer ||
        header == nsHttp::Transfer_Encoding || header == nsHttp::Upgrade ||
        // Ignore any non-modifiable headers...
        header == nsHttp::Content_Location || header == nsHttp::Content_MD5 ||
        header == nsHttp::ETag ||
        // Assume Cache-Control: "no-transform"
        header == nsHttp::Content_Encoding || header == nsHttp::Content_Range ||
        header == nsHttp::Content_Type ||
        // Ignore wacky headers too...
        // this one is for MS servers that send "Content-Length: 0"
        // on 304 responses
        header == nsHttp::Content_Length) {
      LOG(("ignoring response header [%s: %s]\n", header.get(), val.get()));
    } else {
      LOG(("new response header [%s: %s]\n", header.get(), val.get()));

      // overwrite the current header value with the new value...
      DebugOnly<nsresult> rv =
          SetHeader_locked(header, headerNameOriginal, val);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
  }
}

void nsHttpResponseHead::Reset() {
  LOG(("nsHttpResponseHead::Reset\n"));

  RecursiveMutexAutoLock monitor(mRecursiveMutex);

  mHeaders.Clear();

  mVersion = HttpVersion::v1_1;
  mStatus = 200;
  mContentLength = -1;
  mHasCacheControl = false;
  mCacheControlPublic = false;
  mCacheControlPrivate = false;
  mCacheControlNoStore = false;
  mCacheControlNoCache = false;
  mCacheControlImmutable = false;
  mCacheControlStaleWhileRevalidateSet = false;
  mCacheControlStaleWhileRevalidate = 0;
  mCacheControlMaxAgeSet = false;
  mCacheControlMaxAge = 0;
  mPragmaNoCache = false;
  mStatusText.Truncate();
  mContentType.Truncate();
  mContentCharset.Truncate();
}

nsresult nsHttpResponseHead::ParseDateHeader(const nsHttpAtom& header,
                                             uint32_t* result) const {
  const char* val = mHeaders.PeekHeader(header);
  if (!val) return NS_ERROR_NOT_AVAILABLE;

  PRTime time;
  PRStatus st = PR_ParseTimeString(val, true, &time);
  if (st != PR_SUCCESS) return NS_ERROR_NOT_AVAILABLE;

  *result = PRTimeToSeconds(time);
  return NS_OK;
}

nsresult nsHttpResponseHead::GetAgeValue(uint32_t* result) {
  RecursiveMutexAutoLock monitor(mRecursiveMutex);
  return GetAgeValue_locked(result);
}

nsresult nsHttpResponseHead::GetAgeValue_locked(uint32_t* result) const {
  const char* val = mHeaders.PeekHeader(nsHttp::Age);
  if (!val) return NS_ERROR_NOT_AVAILABLE;

  *result = (uint32_t)atoi(val);
  return NS_OK;
}

// Return the value of the (HTTP 1.1) max-age directive, which itself is a
// component of the Cache-Control response header
nsresult nsHttpResponseHead::GetMaxAgeValue(uint32_t* result) {
  RecursiveMutexAutoLock monitor(mRecursiveMutex);
  return GetMaxAgeValue_locked(result);
}

nsresult nsHttpResponseHead::GetMaxAgeValue_locked(uint32_t* result) const {
  if (!mCacheControlMaxAgeSet) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  *result = mCacheControlMaxAge;
  return NS_OK;
}

nsresult nsHttpResponseHead::GetDateValue(uint32_t* result) {
  RecursiveMutexAutoLock monitor(mRecursiveMutex);
  return GetDateValue_locked(result);
}

nsresult nsHttpResponseHead::GetExpiresValue(uint32_t* result) {
  RecursiveMutexAutoLock monitor(mRecursiveMutex);
  return GetExpiresValue_locked(result);
}

nsresult nsHttpResponseHead::GetExpiresValue_locked(uint32_t* result) const {
  const char* val = mHeaders.PeekHeader(nsHttp::Expires);
  if (!val) return NS_ERROR_NOT_AVAILABLE;

  PRTime time;
  PRStatus st = PR_ParseTimeString(val, true, &time);
  if (st != PR_SUCCESS) {
    // parsing failed... RFC 2616 section 14.21 says we should treat this
    // as an expiration time in the past.
    *result = 0;
    return NS_OK;
  }

  if (time < 0) {
    *result = 0;
  } else {
    *result = PRTimeToSeconds(time);
  }
  return NS_OK;
}

nsresult nsHttpResponseHead::GetLastModifiedValue(uint32_t* result) {
  RecursiveMutexAutoLock monitor(mRecursiveMutex);
  return ParseDateHeader(nsHttp::Last_Modified, result);
}

bool nsHttpResponseHead::operator==(const nsHttpResponseHead& aOther) const
    MOZ_NO_THREAD_SAFETY_ANALYSIS {
  nsHttpResponseHead& curr = const_cast<nsHttpResponseHead&>(*this);
  nsHttpResponseHead& other = const_cast<nsHttpResponseHead&>(aOther);
  RecursiveMutexAutoLock monitorOther(other.mRecursiveMutex);
  RecursiveMutexAutoLock monitor(curr.mRecursiveMutex);

  return mHeaders == aOther.mHeaders && mVersion == aOther.mVersion &&
         mStatus == aOther.mStatus && mStatusText == aOther.mStatusText &&
         mContentLength == aOther.mContentLength &&
         mContentType == aOther.mContentType &&
         mContentCharset == aOther.mContentCharset &&
         mHasCacheControl == aOther.mHasCacheControl &&
         mCacheControlPublic == aOther.mCacheControlPublic &&
         mCacheControlPrivate == aOther.mCacheControlPrivate &&
         mCacheControlNoCache == aOther.mCacheControlNoCache &&
         mCacheControlNoStore == aOther.mCacheControlNoStore &&
         mCacheControlImmutable == aOther.mCacheControlImmutable &&
         mCacheControlStaleWhileRevalidateSet ==
             aOther.mCacheControlStaleWhileRevalidateSet &&
         mCacheControlStaleWhileRevalidate ==
             aOther.mCacheControlStaleWhileRevalidate &&
         mCacheControlMaxAgeSet == aOther.mCacheControlMaxAgeSet &&
         mCacheControlMaxAge == aOther.mCacheControlMaxAge &&
         mPragmaNoCache == aOther.mPragmaNoCache;
}

int64_t nsHttpResponseHead::TotalEntitySize() {
  RecursiveMutexAutoLock monitor(mRecursiveMutex);
  const char* contentRange = mHeaders.PeekHeader(nsHttp::Content_Range);
  if (!contentRange) return mContentLength;

  // Total length is after a slash
  const char* slash = strrchr(contentRange, '/');
  if (!slash) return -1;  // No idea what the length is

  slash++;
  if (*slash == '*') {  // Server doesn't know the length
    return -1;
  }

  int64_t size;
  if (!nsHttp::ParseInt64(slash, &size)) size = UINT64_MAX;
  return size;
}

//-----------------------------------------------------------------------------
// nsHttpResponseHead <private>
//-----------------------------------------------------------------------------

void nsHttpResponseHead::ParseVersion(const char* str) {
  // Parse HTTP-Version:: "HTTP" "/" 1*DIGIT "." 1*DIGIT

  LOG(("nsHttpResponseHead::ParseVersion [version=%s]\n", str));

  Tokenizer t(str, nullptr, "");
  // make sure we have HTTP at the beginning
  if (!t.CheckWord("HTTP")) {
    if (nsCRT::strncasecmp(str, "ICY ", 4) == 0) {
      // ShoutCast ICY is HTTP/1.0-like. Assume it is HTTP/1.0.
      LOG(("Treating ICY as HTTP 1.0\n"));
      mVersion = HttpVersion::v1_0;
      return;
    }
    LOG(("looks like a HTTP/0.9 response\n"));
    mVersion = HttpVersion::v0_9;
    return;
  }

  if (!t.CheckChar('/')) {
    LOG(("server did not send a version number; assuming HTTP/1.0\n"));
    // NCSA/1.5.2 has a bug in which it fails to send a version number
    // if the request version is HTTP/1.1, so we fall back on HTTP/1.0
    mVersion = HttpVersion::v1_0;
    return;
  }

  uint32_t major;
  if (!t.ReadInteger(&major)) {
    LOG(("server did not send a correct version number; assuming HTTP/1.0"));
    mVersion = HttpVersion::v1_0;
    return;
  }

  if (major == 3) {
    mVersion = HttpVersion::v3_0;
    return;
  }

  if (major == 2) {
    mVersion = HttpVersion::v2_0;
    return;
  }

  if (major != 1) {
    LOG(("server did not send a correct version number; assuming HTTP/1.0"));
    mVersion = HttpVersion::v1_0;
    return;
  }

  if (!t.CheckChar('.')) {
    LOG(("mal-formed server version; assuming HTTP/1.0\n"));
    mVersion = HttpVersion::v1_0;
    return;
  }

  uint32_t minor;
  if (!t.ReadInteger(&minor)) {
    LOG(("server did not send a correct version number; assuming HTTP/1.0"));
    mVersion = HttpVersion::v1_0;
    return;
  }

  if (minor >= 1) {
    // at least HTTP/1.1
    mVersion = HttpVersion::v1_1;
  } else {
    // treat anything else as version 1.0
    mVersion = HttpVersion::v1_0;
  }
}

void nsHttpResponseHead::ParseCacheControl(const char* val) {
  if (!(val && *val)) {
    // clear flags
    mHasCacheControl = false;
    mCacheControlPublic = false;
    mCacheControlPrivate = false;
    mCacheControlNoCache = false;
    mCacheControlNoStore = false;
    mCacheControlImmutable = false;
    mCacheControlStaleWhileRevalidateSet = false;
    mCacheControlStaleWhileRevalidate = 0;
    mCacheControlMaxAgeSet = false;
    mCacheControlMaxAge = 0;
    return;
  }

  nsDependentCString cacheControlRequestHeader(val);
  CacheControlParser cacheControlRequest(cacheControlRequestHeader);

  mHasCacheControl = true;
  mCacheControlPublic = cacheControlRequest.Public();
  mCacheControlPrivate = cacheControlRequest.Private();
  mCacheControlNoCache = cacheControlRequest.NoCache();
  mCacheControlNoStore = cacheControlRequest.NoStore();
  mCacheControlImmutable = cacheControlRequest.Immutable();
  mCacheControlStaleWhileRevalidateSet =
      cacheControlRequest.StaleWhileRevalidate(
          &mCacheControlStaleWhileRevalidate);
  mCacheControlMaxAgeSet = cacheControlRequest.MaxAge(&mCacheControlMaxAge);
}

void nsHttpResponseHead::ParsePragma(const char* val) {
  LOG(("nsHttpResponseHead::ParsePragma [val=%s]\n", val));

  if (!(val && *val)) {
    // clear no-cache flag
    mPragmaNoCache = false;
    return;
  }

  // Although 'Pragma: no-cache' is not a standard HTTP response header (it's a
  // request header), caching is inhibited when this header is present so as to
  // match existing Navigator behavior.
  mPragmaNoCache = nsHttp::FindToken(val, "no-cache", HTTP_HEADER_VALUE_SEPS);
}

nsresult nsHttpResponseHead::VisitHeaders(
    nsIHttpHeaderVisitor* visitor, nsHttpHeaderArray::VisitorFilter filter) {
  RecursiveMutexAutoLock monitor(mRecursiveMutex);
  mInVisitHeaders = true;
  nsresult rv = mHeaders.VisitHeaders(visitor, filter);
  mInVisitHeaders = false;
  return rv;
}

nsresult nsHttpResponseHead::GetOriginalHeader(const nsHttpAtom& aHeader,
                                               nsIHttpHeaderVisitor* aVisitor) {
  RecursiveMutexAutoLock monitor(mRecursiveMutex);
  mInVisitHeaders = true;
  nsresult rv = mHeaders.GetOriginalHeader(aHeader, aVisitor);
  mInVisitHeaders = false;
  return rv;
}

bool nsHttpResponseHead::HasContentType() const {
  RecursiveMutexAutoLock monitor(mRecursiveMutex);
  return !mContentType.IsEmpty();
}

bool nsHttpResponseHead::HasContentCharset() {
  RecursiveMutexAutoLock monitor(mRecursiveMutex);
  return !mContentCharset.IsEmpty();
}

bool nsHttpResponseHead::GetContentTypeOptionsHeader(
    nsACString& aOutput) const {
  aOutput.Truncate();

  nsAutoCString contentTypeOptionsHeader;
  Unused << GetHeader(nsHttp::X_Content_Type_Options, contentTypeOptionsHeader);
  if (contentTypeOptionsHeader.IsEmpty()) {
    // if there is no XCTO header, then there is nothing to do.
    return false;
  }

  // XCTO header might contain multiple values which are comma separated, so:
  // a) let's skip all subsequent values
  //     e.g. "   NoSniFF   , foo " will be "   NoSniFF   "
  int32_t idx = contentTypeOptionsHeader.Find(",");
  if (idx > 0) {
    contentTypeOptionsHeader = Substring(contentTypeOptionsHeader, 0, idx);
  }
  // b) let's trim all surrounding whitespace
  //    e.g. "   NoSniFF   " -> "NoSniFF"
  nsHttp::TrimHTTPWhitespace(contentTypeOptionsHeader,
                             contentTypeOptionsHeader);

  aOutput.Assign(contentTypeOptionsHeader);
  return true;
}

}  // namespace net
}  // namespace mozilla
