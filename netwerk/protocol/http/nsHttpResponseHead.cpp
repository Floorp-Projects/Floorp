/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "nsHttpResponseHead.h"
#include "nsPrintfCString.h"
#include "prprf.h"
#include "prtime.h"
#include "nsCRT.h"
#include <algorithm>

//-----------------------------------------------------------------------------
// nsHttpResponseHead <public>
//-----------------------------------------------------------------------------

nsresult
nsHttpResponseHead::SetHeader(nsHttpAtom hdr,
                              const nsACString &val,
                              bool merge)
{
    nsresult rv = mHeaders.SetHeader(hdr, val, merge);
    if (NS_FAILED(rv)) return rv;

    // respond to changes in these headers.  we need to reparse the entire
    // header since the change may have merged in additional values.
    if (hdr == nsHttp::Cache_Control)
        ParseCacheControl(mHeaders.PeekHeader(hdr));
    else if (hdr == nsHttp::Pragma)
        ParsePragma(mHeaders.PeekHeader(hdr));

    return NS_OK;
}

void
nsHttpResponseHead::SetContentLength(int64_t len)
{
    mContentLength = len;
    if (len < 0)
        mHeaders.ClearHeader(nsHttp::Content_Length);
    else
        mHeaders.SetHeader(nsHttp::Content_Length, nsPrintfCString("%lld", len));
}

void
nsHttpResponseHead::Flatten(nsACString &buf, bool pruneTransients)
{
    if (mVersion == NS_HTTP_VERSION_0_9)
        return;

    buf.AppendLiteral("HTTP/");
    if (mVersion == NS_HTTP_VERSION_1_1)
        buf.AppendLiteral("1.1 ");
    else
        buf.AppendLiteral("1.0 ");

    buf.Append(nsPrintfCString("%u", unsigned(mStatus)) +
               NS_LITERAL_CSTRING(" ") +
               mStatusText +
               NS_LITERAL_CSTRING("\r\n"));

    if (!pruneTransients) {
        mHeaders.Flatten(buf, false);
        return;
    }

    // otherwise, we need to iterate over the headers and only flatten
    // those that are appropriate.
    uint32_t i, count = mHeaders.Count();
    for (i=0; i<count; ++i) {
        nsHttpAtom header;
        const char *value = mHeaders.PeekHeaderAt(i, header);

        if (!value || header == nsHttp::Connection
                   || header == nsHttp::Proxy_Connection
                   || header == nsHttp::Keep_Alive
                   || header == nsHttp::WWW_Authenticate
                   || header == nsHttp::Proxy_Authenticate
                   || header == nsHttp::Trailer
                   || header == nsHttp::Transfer_Encoding
                   || header == nsHttp::Upgrade
                   // XXX this will cause problems when we start honoring
                   // Cache-Control: no-cache="set-cookie", what to do?
                   || header == nsHttp::Set_Cookie)
            continue;

        // otherwise, write out the "header: value\r\n" line
        buf.Append(nsDependentCString(header.get()) +
                   NS_LITERAL_CSTRING(": ") +
                   nsDependentCString(value) +
                   NS_LITERAL_CSTRING("\r\n"));
    }
}

nsresult
nsHttpResponseHead::Parse(char *block)
{

    LOG(("nsHttpResponseHead::Parse [this=%p]\n", this));

    // this command works on a buffer as prepared by Flatten, as such it is
    // not very forgiving ;-)

    char *p = PL_strstr(block, "\r\n");
    if (!p)
        return NS_ERROR_UNEXPECTED;

    *p = 0;
    ParseStatusLine(block);

    do {
        block = p + 2;

		if (*block == 0)
			break;

        p = PL_strstr(block, "\r\n");
        if (!p)
            return NS_ERROR_UNEXPECTED;

        *p = 0;
        ParseHeaderLine(block);

    } while (1);

    return NS_OK;
}

void
nsHttpResponseHead::ParseStatusLine(const char *line)
{
    //
    // Parse Status-Line:: HTTP-Version SP Status-Code SP Reason-Phrase CRLF
    //

    // HTTP-Version
    ParseVersion(line);

    if ((mVersion == NS_HTTP_VERSION_0_9) || !(line = PL_strchr(line, ' '))) {
        mStatus = 200;
        mStatusText.AssignLiteral("OK");
    }
    else {
        // Status-Code
        mStatus = (uint16_t) atoi(++line);
        if (mStatus == 0) {
            LOG(("mal-formed response status; assuming status = 200\n"));
            mStatus = 200;
        }

        // Reason-Phrase is whatever is remaining of the line
        if (!(line = PL_strchr(line, ' '))) {
            LOG(("mal-formed response status line; assuming statusText = 'OK'\n"));
            mStatusText.AssignLiteral("OK");
        }
        else
            mStatusText = nsDependentCString(++line);
    }

    LOG(("Have status line [version=%u status=%u statusText=%s]\n",
        unsigned(mVersion), unsigned(mStatus), mStatusText.get()));
}

nsresult
nsHttpResponseHead::ParseHeaderLine(const char *line)
{
    nsHttpAtom hdr = {0};
    char *val;
    nsresult rv;

    rv = mHeaders.ParseHeaderLine(line, &hdr, &val);
    if (NS_FAILED(rv))
        return rv;

    // leading and trailing LWS has been removed from |val|

    // handle some special case headers...
    if (hdr == nsHttp::Content_Length) {
        int64_t len;
        const char *ignored;
        // permit only a single value here.
        if (nsHttp::ParseInt64(val, &ignored, &len)) {
            mContentLength = len;
        }
        else {
            // If this is a negative content length then just ignore it
            LOG(("invalid content-length! %s\n", val));
        }
    }
    else if (hdr == nsHttp::Content_Type) {
        LOG(("ParseContentType [type=%s]\n", val));
        bool dummy;
        net_ParseContentType(nsDependentCString(val),
                             mContentType, mContentCharset, &dummy);
    }
    else if (hdr == nsHttp::Cache_Control)
        ParseCacheControl(val);
    else if (hdr == nsHttp::Pragma)
        ParsePragma(val);
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
nsresult
nsHttpResponseHead::ComputeCurrentAge(uint32_t now,
                                      uint32_t requestTime,
                                      uint32_t *result) const
{
    uint32_t dateValue;
    uint32_t ageValue;

    *result = 0;

    if (NS_FAILED(GetDateValue(&dateValue))) {
        LOG(("nsHttpResponseHead::ComputeCurrentAge [this=%p] "
             "Date response header not set!\n", this));
        // Assume we have a fast connection and that our clock
        // is in sync with the server.
        dateValue = now;
    }

    // Compute apparent age
    if (now > dateValue)
        *result = now - dateValue;

    // Compute corrected received age
    if (NS_SUCCEEDED(GetAgeValue(&ageValue)))
        *result = std::max(*result, ageValue);

    MOZ_ASSERT(now >= requestTime, "bogus request time");

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
//     freshnessLifetime = (date_value - last_modified_value) * 0.10
// <or>
//     freshnessLifetime = 0
//
nsresult
nsHttpResponseHead::ComputeFreshnessLifetime(uint32_t *result) const
{
    *result = 0;

    // Try HTTP/1.1 style max-age directive...
    if (NS_SUCCEEDED(GetMaxAgeValue(result)))
        return NS_OK;

    *result = 0;

    uint32_t date = 0, date2 = 0;
    if (NS_FAILED(GetDateValue(&date)))
        date = NowInSeconds(); // synthesize a date header if none exists

    // Try HTTP/1.0 style expires header...
    if (NS_SUCCEEDED(GetExpiresValue(&date2))) {
        if (date2 > date)
            *result = date2 - date;
        // the Expires header can specify a date in the past.
        return NS_OK;
    }

    // Fallback on heuristic using last modified header...
    if (NS_SUCCEEDED(GetLastModifiedValue(&date2))) {
        LOG(("using last-modified to determine freshness-lifetime\n"));
        LOG(("last-modified = %u, date = %u\n", date2, date));
        if (date2 <= date) {
            // this only makes sense if last-modified is actually in the past
            *result = (date - date2) / 10;
            return NS_OK;
        }
    }

    // These responses can be cached indefinitely.
    if ((mStatus == 300) || nsHttp::IsPermanentRedirect(mStatus)) {
        *result = uint32_t(-1);
        return NS_OK;
    }

    LOG(("nsHttpResponseHead::ComputeFreshnessLifetime [this = %x] "
         "Insufficient information to compute a non-zero freshness "
         "lifetime!\n", this));

    return NS_OK;
}

bool
nsHttpResponseHead::MustValidate() const
{
    LOG(("nsHttpResponseHead::MustValidate ??\n"));

    // Some response codes are cacheable, but the rest are not.  This switch
    // should stay in sync with the list in nsHttpChannel::ProcessResponse
    switch (mStatus) {
        // Success codes
    case 200:
    case 203:
    case 206:
        // Cacheable redirects
    case 300:
    case 301:
    case 302:
    case 304:
    case 307:
    case 308:
        break;
        // Uncacheable redirects
    case 303:
    case 305:
        // Other known errors
    case 401:
    case 407:
    case 412:
    case 416:
    default:  // revalidate unknown error pages
        LOG(("Must validate since response is an uncacheable error page\n"));
        return true;
    }

    // The no-cache response header indicates that we must validate this
    // cached response before reusing.
    if (NoCache()) {
        LOG(("Must validate since response contains 'no-cache' header\n"));
        return true;
    }

    // Likewise, if the response is no-store, then we must validate this
    // cached response before reusing.  NOTE: it may seem odd that a no-store
    // response may be cached, but indeed all responses are cached in order
    // to support File->SaveAs, View->PageSource, and other browser features.
    if (NoStore()) {
        LOG(("Must validate since response contains 'no-store' header\n"));
        return true;
    }

    // Compare the Expires header to the Date header.  If the server sent an
    // Expires header with a timestamp in the past, then we must validate this
    // cached response before reusing.
    if (ExpiresInPast()) {
        LOG(("Must validate since Expires < Date\n"));
        return true;
    }

    LOG(("no mandatory validation requirement\n"));
    return false;
}

bool
nsHttpResponseHead::MustValidateIfExpired() const
{
    // according to RFC2616, section 14.9.4:
    //
    //  When the must-revalidate directive is present in a response received by a
    //  cache, that cache MUST NOT use the entry after it becomes stale to respond to
    //  a subsequent request without first revalidating it with the origin server.
    //
    return HasHeaderValue(nsHttp::Cache_Control, "must-revalidate");
}

bool
nsHttpResponseHead::IsResumable() const
{
    // even though some HTTP/1.0 servers may support byte range requests, we're not
    // going to bother with them, since those servers wouldn't understand If-Range.
    // Also, while in theory it may be possible to resume when the status code
    // is not 200, it is unlikely to be worth the trouble, especially for
    // non-2xx responses.
    return mStatus == 200 &&
           mVersion >= NS_HTTP_VERSION_1_1 &&
           PeekHeader(nsHttp::Content_Length) &&
          (PeekHeader(nsHttp::ETag) || PeekHeader(nsHttp::Last_Modified)) &&
           HasHeaderValue(nsHttp::Accept_Ranges, "bytes");
}

bool
nsHttpResponseHead::ExpiresInPast() const
{
    uint32_t maxAgeVal, expiresVal, dateVal;

    // Bug #203271. Ensure max-age directive takes precedence over Expires
    if (NS_SUCCEEDED(GetMaxAgeValue(&maxAgeVal))) {
        return false;
    }

    return NS_SUCCEEDED(GetExpiresValue(&expiresVal)) &&
           NS_SUCCEEDED(GetDateValue(&dateVal)) &&
           expiresVal < dateVal;
}

nsresult
nsHttpResponseHead::UpdateHeaders(const nsHttpHeaderArray &headers)
{
    LOG(("nsHttpResponseHead::UpdateHeaders [this=%p]\n", this));

    uint32_t i, count = headers.Count();
    for (i=0; i<count; ++i) {
        nsHttpAtom header;
        const char *val = headers.PeekHeaderAt(i, header);

        if (!val) {
            continue;
        }

        // Ignore any hop-by-hop headers...
        if (header == nsHttp::Connection          ||
            header == nsHttp::Proxy_Connection    ||
            header == nsHttp::Keep_Alive          ||
            header == nsHttp::Proxy_Authenticate  ||
            header == nsHttp::Proxy_Authorization || // not a response header!
            header == nsHttp::TE                  ||
            header == nsHttp::Trailer             ||
            header == nsHttp::Transfer_Encoding   ||
            header == nsHttp::Upgrade             ||
        // Ignore any non-modifiable headers...
            header == nsHttp::Content_Location    ||
            header == nsHttp::Content_MD5         ||
            header == nsHttp::ETag                ||
        // Assume Cache-Control: "no-transform"
            header == nsHttp::Content_Encoding    ||
            header == nsHttp::Content_Range       ||
            header == nsHttp::Content_Type        ||
        // Ignore wacky headers too...
            // this one is for MS servers that send "Content-Length: 0"
            // on 304 responses
            header == nsHttp::Content_Length) {
            LOG(("ignoring response header [%s: %s]\n", header.get(), val));
        }
        else {
            LOG(("new response header [%s: %s]\n", header.get(), val));

            // overwrite the current header value with the new value...
            SetHeader(header, nsDependentCString(val));
        }
    }

    return NS_OK;
}

void
nsHttpResponseHead::Reset()
{
    LOG(("nsHttpResponseHead::Reset\n"));

    ClearHeaders();

    mVersion = NS_HTTP_VERSION_1_1;
    mStatus = 200;
    mContentLength = UINT64_MAX;
    mCacheControlNoStore = false;
    mCacheControlNoCache = false;
    mPragmaNoCache = false;
    mStatusText.Truncate();
    mContentType.Truncate();
    mContentCharset.Truncate();
}

nsresult
nsHttpResponseHead::ParseDateHeader(nsHttpAtom header, uint32_t *result) const
{
    const char *val = PeekHeader(header);
    if (!val)
        return NS_ERROR_NOT_AVAILABLE;

    PRTime time;
    PRStatus st = PR_ParseTimeString(val, true, &time);
    if (st != PR_SUCCESS)
        return NS_ERROR_NOT_AVAILABLE;

    *result = PRTimeToSeconds(time);
    return NS_OK;
}

nsresult
nsHttpResponseHead::GetAgeValue(uint32_t *result) const
{
    const char *val = PeekHeader(nsHttp::Age);
    if (!val)
        return NS_ERROR_NOT_AVAILABLE;

    *result = (uint32_t) atoi(val);
    return NS_OK;
}

// Return the value of the (HTTP 1.1) max-age directive, which itself is a
// component of the Cache-Control response header
nsresult
nsHttpResponseHead::GetMaxAgeValue(uint32_t *result) const
{
    const char *val = PeekHeader(nsHttp::Cache_Control);
    if (!val)
        return NS_ERROR_NOT_AVAILABLE;

    const char *p = PL_strcasestr(val, "max-age=");
    if (!p)
        return NS_ERROR_NOT_AVAILABLE;

    int maxAgeValue = atoi(p + 8);
    if (maxAgeValue < 0)
        maxAgeValue = 0;
    *result = uint32_t(maxAgeValue);
    return NS_OK;
}

nsresult
nsHttpResponseHead::GetExpiresValue(uint32_t *result) const
{
    const char *val = PeekHeader(nsHttp::Expires);
    if (!val)
        return NS_ERROR_NOT_AVAILABLE;

    PRTime time;
    PRStatus st = PR_ParseTimeString(val, true, &time);
    if (st != PR_SUCCESS) {
        // parsing failed... RFC 2616 section 14.21 says we should treat this
        // as an expiration time in the past.
        *result = 0;
        return NS_OK;
    }

    if (time < 0)
        *result = 0;
    else
        *result = PRTimeToSeconds(time);
    return NS_OK;
}

int64_t
nsHttpResponseHead::TotalEntitySize() const
{
    const char* contentRange = PeekHeader(nsHttp::Content_Range);
    if (!contentRange)
        return ContentLength();

    // Total length is after a slash
    const char* slash = strrchr(contentRange, '/');
    if (!slash)
        return -1; // No idea what the length is

    slash++;
    if (*slash == '*') // Server doesn't know the length
        return -1;

    int64_t size;
    if (!nsHttp::ParseInt64(slash, &size))
        size = UINT64_MAX;
    return size;
}

//-----------------------------------------------------------------------------
// nsHttpResponseHead <private>
//-----------------------------------------------------------------------------

void
nsHttpResponseHead::ParseVersion(const char *str)
{
    // Parse HTTP-Version:: "HTTP" "/" 1*DIGIT "." 1*DIGIT

    LOG(("nsHttpResponseHead::ParseVersion [version=%s]\n", str));

    // make sure we have HTTP at the beginning
    if (PL_strncasecmp(str, "HTTP", 4) != 0) {
        if (PL_strncasecmp(str, "ICY ", 4) == 0) {
            // ShoutCast ICY is HTTP/1.0-like. Assume it is HTTP/1.0.
            LOG(("Treating ICY as HTTP 1.0\n"));
            mVersion = NS_HTTP_VERSION_1_0;
            return;
        }
        LOG(("looks like a HTTP/0.9 response\n"));
        mVersion = NS_HTTP_VERSION_0_9;
        return;
    }
    str += 4;

    if (*str != '/') {
        LOG(("server did not send a version number; assuming HTTP/1.0\n"));
        // NCSA/1.5.2 has a bug in which it fails to send a version number
        // if the request version is HTTP/1.1, so we fall back on HTTP/1.0
        mVersion = NS_HTTP_VERSION_1_0;
        return;
    }

    char *p = PL_strchr(str, '.');
    if (p == nullptr) {
        LOG(("mal-formed server version; assuming HTTP/1.0\n"));
        mVersion = NS_HTTP_VERSION_1_0;
        return;
    }

    ++p; // let b point to the minor version

    int major = atoi(str + 1);
    int minor = atoi(p);

    if ((major > 1) || ((major == 1) && (minor >= 1)))
        // at least HTTP/1.1
        mVersion = NS_HTTP_VERSION_1_1;
    else
        // treat anything else as version 1.0
        mVersion = NS_HTTP_VERSION_1_0;
}

void
nsHttpResponseHead::ParseCacheControl(const char *val)
{
    if (!(val && *val)) {
        // clear flags
        mCacheControlNoCache = false;
        mCacheControlNoStore = false;
        return;
    }

    // search header value for occurrence(s) of "no-cache" but ignore
    // occurrence(s) of "no-cache=blah"
    if (nsHttp::FindToken(val, "no-cache", HTTP_HEADER_VALUE_SEPS))
        mCacheControlNoCache = true;

    // search header value for occurrence of "no-store"
    if (nsHttp::FindToken(val, "no-store", HTTP_HEADER_VALUE_SEPS))
        mCacheControlNoStore = true;
}

void
nsHttpResponseHead::ParsePragma(const char *val)
{
    LOG(("nsHttpResponseHead::ParsePragma [val=%s]\n", val));

    if (!(val && *val)) {
        // clear no-cache flag
        mPragmaNoCache = false;
        return;
    }

    // Although 'Pragma: no-cache' is not a standard HTTP response header (it's
    // a request header), caching is inhibited when this header is present so
    // as to match existing Navigator behavior.
    if (nsHttp::FindToken(val, "no-cache", HTTP_HEADER_VALUE_SEPS))
        mPragmaNoCache = true;
}
