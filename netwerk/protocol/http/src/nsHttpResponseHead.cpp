/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cin: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com> (original author)
 *   Andreas M. Schneider <clarence@clarence.de>
 *   Christian Biesinger <cbiesinger@web.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <stdlib.h>
#include "nsHttpResponseHead.h"
#include "nsPrintfCString.h"
#include "prprf.h"
#include "prtime.h"
#include "nsCRT.h"

//-----------------------------------------------------------------------------
// nsHttpResponseHead <public>
//-----------------------------------------------------------------------------

nsresult
nsHttpResponseHead::SetHeader(nsHttpAtom hdr,
                              const nsACString &val,
                              PRBool merge)
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
nsHttpResponseHead::SetContentLength(PRInt64 len)
{
    mContentLength = len;
    if (!LL_GE_ZERO(len)) // < 0
        mHeaders.ClearHeader(nsHttp::Content_Length);
    else
        mHeaders.SetHeader(nsHttp::Content_Length, nsPrintfCString(20, "%lld", len));
}

void
nsHttpResponseHead::Flatten(nsACString &buf, PRBool pruneTransients)
{
    if (mVersion == NS_HTTP_VERSION_0_9)
        return;

    buf.Append(NS_LITERAL_CSTRING("HTTP/"));
    if (mVersion == NS_HTTP_VERSION_1_1)
        buf.Append(NS_LITERAL_CSTRING("1.1 "));
    else
        buf.Append(NS_LITERAL_CSTRING("1.0 "));

    buf.Append(nsPrintfCString("%u", PRUintn(mStatus)) +
               NS_LITERAL_CSTRING(" ") +
               mStatusText +
               NS_LITERAL_CSTRING("\r\n"));

    if (!pruneTransients) {
        mHeaders.Flatten(buf, PR_FALSE);
        return;
    }

    // otherwise, we need to iterate over the headers and only flatten
    // those that are appropriate.
    PRUint32 i, count = mHeaders.Count();
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

    LOG(("nsHttpResponseHead::Parse [this=%x]\n", this));

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
nsHttpResponseHead::ParseStatusLine(char *line)
{
    //
    // Parse Status-Line:: HTTP-Version SP Status-Code SP Reason-Phrase CRLF
    //
 
    // HTTP-Version
    ParseVersion(line);
    
    if ((mVersion == NS_HTTP_VERSION_0_9) || !(line = PL_strchr(line, ' '))) {
        mStatus = 200;
        mStatusText = NS_LITERAL_CSTRING("OK");
    }
    else {
        // Status-Code
        mStatus = (PRUint16) atoi(++line);
        if (mStatus == 0) {
            LOG(("mal-formed response status; assuming status = 200\n"));
            mStatus = 200;
        }

        // Reason-Phrase is whatever is remaining of the line
        if (!(line = PL_strchr(line, ' '))) {
            LOG(("mal-formed response status line; assuming statusText = 'OK'\n"));
            mStatusText = NS_LITERAL_CSTRING("OK");
        }
        else
            mStatusText = ++line;
    }

    LOG(("Have status line [version=%u status=%u statusText=%s]\n",
        PRUintn(mVersion), PRUintn(mStatus), mStatusText.get()));
}

void
nsHttpResponseHead::ParseHeaderLine(char *line)
{
    nsHttpAtom hdr = {0};
    char *val;

    mHeaders.ParseHeaderLine(line, &hdr, &val);

    // handle some special case headers...
    if (hdr == nsHttp::Content_Length)
        PR_sscanf(val, "%lld", &mContentLength);
    else if (hdr == nsHttp::Content_Type)
        ParseContentType(val);
    else if (hdr == nsHttp::Cache_Control)
        ParseCacheControl(val);
    else if (hdr == nsHttp::Pragma)
        ParsePragma(val);
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
nsHttpResponseHead::ComputeCurrentAge(PRUint32 now,
                                      PRUint32 requestTime,
                                      PRUint32 *result)
{
    PRUint32 dateValue;
    PRUint32 ageValue;

    *result = 0;

    if (NS_FAILED(GetDateValue(&dateValue))) {
        LOG(("nsHttpResponseHead::ComputeCurrentAge [this=%x] "
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
        *result = PR_MAX(*result, ageValue);

    NS_ASSERTION(now >= requestTime, "bogus request time");

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
nsHttpResponseHead::ComputeFreshnessLifetime(PRUint32 *result)
{
    *result = 0;

    // Try HTTP/1.1 style max-age directive...
    if (NS_SUCCEEDED(GetMaxAgeValue(result)))
        return NS_OK;

    *result = 0;

    PRUint32 date = 0, date2 = 0;
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
        *result = (date - date2) / 10;
        return NS_OK;
    }

    // These responses can be cached indefinitely.
    if ((mStatus == 300) || (mStatus == 301)) {
        *result = PRUint32(-1);
        return NS_OK;
    }

    LOG(("nsHttpResponseHead::ComputeFreshnessLifetime [this = %x] "
         "Insufficient information to compute a non-zero freshness "
         "lifetime!\n", this));

    return NS_OK;
}

PRBool
nsHttpResponseHead::MustValidate()
{
    LOG(("nsHttpResponseHead::MustValidate ??\n"));

    // The no-cache response header indicates that we must validate this
    // cached response before reusing.
    if (NoCache()) {
        LOG(("Must validate since response contains 'no-cache' header\n"));
        return PR_TRUE;
    }

    // Likewise, if the response is no-store, then we must validate this
    // cached response before reusing.  NOTE: it may seem odd that a no-store
    // response may be cached, but indeed all responses are cached in order
    // to support File->SaveAs, View->PageSource, and other browser features.
    if (NoStore()) {
        LOG(("Must validate since response contains 'no-store' header\n"));
        return PR_TRUE;
    }

    // Compare the Expires header to the Date header.  If the server sent an
    // Expires header with a timestamp in the past, then we must validate this
    // cached response before reusing.
    if (ExpiresInPast()) {
        LOG(("Must validate since Expires < Date\n"));
        return PR_TRUE;
    }

    LOG(("no mandatory validation requirement\n"));
    return PR_FALSE;
}

PRBool
nsHttpResponseHead::MustValidateIfExpired()
{
    // according to RFC2616, section 14.9.4:
    //
    //  When the must-revalidate directive is present in a response received by a   
    //  cache, that cache MUST NOT use the entry after it becomes stale to respond to 
    //  a subsequent request without first revalidating it with the origin server.
    //
    const char *val = PeekHeader(nsHttp::Cache_Control);
    return val && PL_strcasestr(val, "must-revalidate");
}

PRBool
nsHttpResponseHead::IsResumable()
{
    // even though some HTTP/1.0 servers may support byte range requests, we're not
    // going to bother with them, since those servers wouldn't understand If-Range.
    return mVersion >= NS_HTTP_VERSION_1_1 &&
           PeekHeader(nsHttp::Content_Length) && 
          (PeekHeader(nsHttp::ETag) || PeekHeader(nsHttp::Last_Modified)) &&
           PL_strcasestr(PeekHeader(nsHttp::Accept_Ranges), "bytes");
}

PRBool
nsHttpResponseHead::ExpiresInPast()
{
    PRUint32 expiresVal, dateVal;
    return NS_SUCCEEDED(GetExpiresValue(&expiresVal)) &&
           NS_SUCCEEDED(GetDateValue(&dateVal)) &&
           expiresVal < dateVal;
}

nsresult
nsHttpResponseHead::UpdateHeaders(nsHttpHeaderArray &headers)
{
    LOG(("nsHttpResponseHead::UpdateHeaders [this=%x]\n", this));

    PRUint32 i, count = headers.Count();
    for (i=0; i<count; ++i) {
        nsHttpAtom header;
        const char *val = headers.PeekHeaderAt(i, header);

        if (!val) {
            NS_NOTREACHED("null header value");
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
            header == nsHttp::Last_Modified       ||
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
    mContentLength = LL_MaxUint();
    mCacheControlNoStore = PR_FALSE;
    mCacheControlNoCache = PR_FALSE;
    mPragmaNoCache = PR_FALSE;
    mStatusText.Truncate();
    mContentType.Truncate();
    mContentCharset.Truncate();
}

nsresult
nsHttpResponseHead::ParseDateHeader(nsHttpAtom header, PRUint32 *result)
{
    const char *val = PeekHeader(header);
    if (!val)
        return NS_ERROR_NOT_AVAILABLE;

    PRTime time;
    PRStatus st = PR_ParseTimeString(val, PR_TRUE, &time);
    if (st != PR_SUCCESS)
        return NS_ERROR_NOT_AVAILABLE;

    *result = PRTimeToSeconds(time); 
    return NS_OK;
}

nsresult
nsHttpResponseHead::GetAgeValue(PRUint32 *result)
{
    const char *val = PeekHeader(nsHttp::Age);
    if (!val)
        return NS_ERROR_NOT_AVAILABLE;

    *result = (PRUint32) atoi(val);
    return NS_OK;
}

// Return the value of the (HTTP 1.1) max-age directive, which itself is a
// component of the Cache-Control response header
nsresult
nsHttpResponseHead::GetMaxAgeValue(PRUint32 *result)
{
    const char *val = PeekHeader(nsHttp::Cache_Control);
    if (!val)
        return NS_ERROR_NOT_AVAILABLE;

    const char *p = PL_strcasestr(val, "max-age=");
    if (!p)
        return NS_ERROR_NOT_AVAILABLE;

    *result = (PRUint32) atoi(p + 8);
    return NS_OK;
}

nsresult
nsHttpResponseHead::GetExpiresValue(PRUint32 *result)
{
    const char *val = PeekHeader(nsHttp::Expires);
    if (!val)
        return NS_ERROR_NOT_AVAILABLE;

    PRTime time;
    PRStatus st = PR_ParseTimeString(val, PR_TRUE, &time);
    if (st != PR_SUCCESS) {
        // parsing failed... maybe this is an "Expires: 0"
        nsCAutoString buf(val);
        buf.StripWhitespace();
        if (buf.Length() == 1 && buf[0] == '0') {
            *result = 0;
            return NS_OK;
        }
        return NS_ERROR_NOT_AVAILABLE;
    }

    if (LL_CMP(time, <, LL_Zero()))
        *result = 0;
    else
        *result = PRTimeToSeconds(time); 
    return NS_OK;
}

PRInt64
nsHttpResponseHead::TotalEntitySize()
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

    PRInt64 size;
    PRInt32 items = PR_sscanf(slash, "%lld", &size);
    if (items <= 0)
        size = LL_MaxUint();
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
    if (p == nsnull) {
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
nsHttpResponseHead::ParseContentType(char *type)
{
    LOG(("nsHttpResponseHead::ParseContentType [type=%s]\n", type));

    //
    // Augmented BNF (from RFC 2616 section 3.7):
    //
    //   header-value = media-type *( LWS "," LWS media-type )
    //   media-type   = type "/" subtype *( LWS ";" LWS parameter )
    //   type         = token
    //   subtype      = token
    //   parameter    = attribute "=" value
    //   attribute    = token
    //   value        = token | quoted-string
    //   
    //
    // Examples:
    //
    //   text/html
    //   text/html, text/html
    //   text/html,text/html; charset=ISO-8859-1
    //   text/html;charset=ISO-8859-1, text/html
    //   application/octet-stream
    //

    // iterate over media-types
    char *nextType;
    do {
        nextType = (char *) strchr(type, ',');
        if (nextType) {
            *nextType = '\0';
            ++nextType;
        }
        // type points at this media-type; locate first parameter if any
        char *charset = "";
        char *param = (char *) strchr(type, ';');
        if (param) {
            *param = '\0';
            ++param;

            // iterate over parameters
            char *nextParam;
            do {
                nextParam = (char *) strchr(param, ';');
                if (nextParam) {
                    *nextParam = '\0';
                    ++nextParam;
                }
                // param points at this parameter

                param = net_FindCharNotInSet(param, HTTP_LWS);
                if (PL_strncasecmp(param, "charset=", 8) == 0)
                    charset = param + 8;

            } while ((param = nextParam) != nsnull);
        }

        // trim LWS leading and trailing whitespace from type and charset.
        // charset cannot have leading whitespace.  we include '(' in the
        // trailing trim set to catch media-type comments, which are not
        // at all standard, but may occur in rare cases.

        type = net_FindCharNotInSet(type, HTTP_LWS);

        char *typeEnd    = net_FindCharInSet(type,    HTTP_LWS "(");
        char *charsetEnd = net_FindCharInSet(charset, HTTP_LWS "(");

        // force content-type to be lowercase
        net_ToLowerCase(type, typeEnd - type);

        // if the server sent "*/*", it is meaningless, so do not store it.
        // also, if type is the same as mContentType, then just update the
        // charset.  however, if charset is empty and mContentType hasn't
        // changed, then don't wipe-out an existing mContentCharset.  we
        // also want to reject a mime-type if it does not include a slash.
        // some servers give junk after the charset parameter, which may
        // include a comma, so this check makes us a bit more tolerant.

        if (*type && strcmp(type, "*/*") != 0 && strchr(type, '/')) {
            PRBool eq = mContentType.Equals(Substring(type, typeEnd));
            if (!eq)
                mContentType.Assign(type, typeEnd - type);
            if (!eq || *charset)
                mContentCharset.Assign(charset, charsetEnd - charset);
        }

    } while ((type = nextType) != nsnull);
}

void
nsHttpResponseHead::ParseCacheControl(const char *val)
{
    if (!(val && *val)) {
        // clear flags
        mCacheControlNoCache = PR_FALSE;
        mCacheControlNoStore = PR_FALSE;
        return;
    }

    const char *s = val;

    // search header value for occurance(s) of "no-cache" but ignore
    // occurance(s) of "no-cache=blah"
    while ((s = PL_strcasestr(s, "no-cache")) != nsnull) {
        s += (sizeof("no-cache") - 1);
        if (*s != '=')
            mCacheControlNoCache = PR_TRUE;
    }

    // search header value for occurance of "no-store" 
    if (PL_strcasestr(val, "no-store"))
        mCacheControlNoStore = PR_TRUE;
}

void
nsHttpResponseHead::ParsePragma(const char *val)
{
    LOG(("nsHttpResponseHead::ParsePragma [val=%s]\n", val));

    if (!(val && *val)) {
        // clear no-cache flag
        mPragmaNoCache = PR_FALSE;
        return;
    }

    // Although 'Pragma: no-cache' is not a standard HTTP response header (it's
    // a request header), caching is inhibited when this header is present so
    // as to match existing Navigator behavior.
    if (PL_strcasestr(val, "no-cache"))
        mPragmaNoCache = PR_TRUE;
}
