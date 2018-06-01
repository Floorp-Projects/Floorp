/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

nsHttpRequestHead::nsHttpRequestHead()
    : mMethod(NS_LITERAL_CSTRING("GET"))
    , mVersion(HttpVersion::v1_1)
    , mParsedMethod(kMethod_Get)
    , mHTTPS(false)
    , mRecursiveMutex("nsHttpRequestHead.mRecursiveMutex")
    , mInVisitHeaders(false)
{
    MOZ_COUNT_CTOR(nsHttpRequestHead);
}

nsHttpRequestHead::~nsHttpRequestHead()
{
    MOZ_COUNT_DTOR(nsHttpRequestHead);
}

// Don't use this function. It is only used by HttpChannelParent to avoid
// copying of request headers!!!
const nsHttpHeaderArray &
nsHttpRequestHead::Headers() const
{
    nsHttpRequestHead &curr = const_cast<nsHttpRequestHead&>(*this);
    curr.mRecursiveMutex.AssertCurrentThreadIn();
    return mHeaders;
}

void
nsHttpRequestHead::SetHeaders(const nsHttpHeaderArray& aHeaders)
{
    RecursiveMutexAutoLock mon(mRecursiveMutex);
    mHeaders = aHeaders;
}

void
nsHttpRequestHead::SetVersion(HttpVersion version)
{
    RecursiveMutexAutoLock mon(mRecursiveMutex);
    mVersion = version;
}

void
nsHttpRequestHead::SetRequestURI(const nsACString& s)
{
    RecursiveMutexAutoLock mon(mRecursiveMutex);
    mRequestURI = s;
}

void
nsHttpRequestHead::SetPath(const nsACString& s)
{
    RecursiveMutexAutoLock mon(mRecursiveMutex);
    mPath = s;
}

uint32_t
nsHttpRequestHead::HeaderCount()
{
    RecursiveMutexAutoLock mon(mRecursiveMutex);
    return mHeaders.Count();
}

nsresult
nsHttpRequestHead::VisitHeaders(nsIHttpHeaderVisitor *visitor,
                                nsHttpHeaderArray::VisitorFilter filter /* = nsHttpHeaderArray::eFilterAll*/)
{
    RecursiveMutexAutoLock mon(mRecursiveMutex);
    mInVisitHeaders = true;
    nsresult rv = mHeaders.VisitHeaders(visitor, filter);
    mInVisitHeaders = false;
    return rv;
}

void
nsHttpRequestHead::Method(nsACString &aMethod)
{
    RecursiveMutexAutoLock mon(mRecursiveMutex);
    aMethod = mMethod;
}

HttpVersion
nsHttpRequestHead::Version()
{
    RecursiveMutexAutoLock mon(mRecursiveMutex);
    return mVersion;
}

void
nsHttpRequestHead::RequestURI(nsACString &aRequestURI)
{
    RecursiveMutexAutoLock mon(mRecursiveMutex);
    aRequestURI = mRequestURI;
}

void
nsHttpRequestHead::Path(nsACString &aPath)
{
    RecursiveMutexAutoLock mon(mRecursiveMutex);
    aPath = mPath.IsEmpty() ? mRequestURI : mPath;
}

void
nsHttpRequestHead::SetHTTPS(bool val)
{
    RecursiveMutexAutoLock mon(mRecursiveMutex);
    mHTTPS = val;
}

void
nsHttpRequestHead::Origin(nsACString &aOrigin)
{
    RecursiveMutexAutoLock mon(mRecursiveMutex);
    aOrigin = mOrigin;
}

nsresult
nsHttpRequestHead::SetHeader(const nsACString &h, const nsACString &v,
                             bool m /*= false*/)
{
    RecursiveMutexAutoLock mon(mRecursiveMutex);

    if (mInVisitHeaders) {
        return NS_ERROR_FAILURE;
    }

    return mHeaders.SetHeader(h, v, m,
                              nsHttpHeaderArray::eVarietyRequestOverride);
}

nsresult
nsHttpRequestHead::SetHeader(nsHttpAtom h, const nsACString &v,
                             bool m /*= false*/)
{
    RecursiveMutexAutoLock mon(mRecursiveMutex);

    if (mInVisitHeaders) {
        return NS_ERROR_FAILURE;
    }

    return mHeaders.SetHeader(h, v, m,
                              nsHttpHeaderArray::eVarietyRequestOverride);
}

nsresult
nsHttpRequestHead::SetHeader(nsHttpAtom h, const nsACString &v, bool m,
                             nsHttpHeaderArray::HeaderVariety variety)
{
    RecursiveMutexAutoLock mon(mRecursiveMutex);

    if (mInVisitHeaders) {
        return NS_ERROR_FAILURE;
    }

    return mHeaders.SetHeader(h, v, m, variety);
}

nsresult
nsHttpRequestHead::SetEmptyHeader(const nsACString &h)
{
    RecursiveMutexAutoLock mon(mRecursiveMutex);

    if (mInVisitHeaders) {
        return NS_ERROR_FAILURE;
    }

    return mHeaders.SetEmptyHeader(h,
                                   nsHttpHeaderArray::eVarietyRequestOverride);
}

nsresult
nsHttpRequestHead::GetHeader(nsHttpAtom h, nsACString &v)
{
    v.Truncate();
    RecursiveMutexAutoLock mon(mRecursiveMutex);
    return mHeaders.GetHeader(h, v);
}

nsresult
nsHttpRequestHead::ClearHeader(nsHttpAtom h)
{
    RecursiveMutexAutoLock mon(mRecursiveMutex);

    if (mInVisitHeaders) {
        return NS_ERROR_FAILURE;
    }

    mHeaders.ClearHeader(h);
    return NS_OK;
}

void
nsHttpRequestHead::ClearHeaders()
{
    RecursiveMutexAutoLock mon(mRecursiveMutex);

    if (mInVisitHeaders) {
        return;
    }

    mHeaders.Clear();
}

bool
nsHttpRequestHead::HasHeader(nsHttpAtom h)
{
    RecursiveMutexAutoLock mon(mRecursiveMutex);
    return mHeaders.HasHeader(h);
}

bool
nsHttpRequestHead::HasHeaderValue(nsHttpAtom h, const char *v)
{
    RecursiveMutexAutoLock mon(mRecursiveMutex);
    return mHeaders.HasHeaderValue(h, v);
}

nsresult
nsHttpRequestHead::SetHeaderOnce(nsHttpAtom h, const char *v,
                                 bool merge /*= false */)
{
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

nsHttpRequestHead::ParsedMethodType
nsHttpRequestHead::ParsedMethod()
{
    RecursiveMutexAutoLock mon(mRecursiveMutex);
    return mParsedMethod;
}

bool
nsHttpRequestHead::EqualsMethod(ParsedMethodType aType)
{
    RecursiveMutexAutoLock mon(mRecursiveMutex);
    return mParsedMethod == aType;
}

void
nsHttpRequestHead::ParseHeaderSet(const char *buffer)
{
    RecursiveMutexAutoLock mon(mRecursiveMutex);
    nsHttpAtom hdr;
    nsAutoCString headerNameOriginal;
    nsAutoCString val;
    while (buffer) {
        const char *eof = strchr(buffer, '\r');
        if (!eof) {
            break;
        }
        if (NS_SUCCEEDED(nsHttpHeaderArray::ParseHeaderLine(
            nsDependentCSubstring(buffer, eof - buffer),
            &hdr,
            &headerNameOriginal,
            &val))) {

            DebugOnly<nsresult> rv = mHeaders.SetHeaderFromNet(hdr,
                                                               headerNameOriginal,
                                                               val,
                                                               false);
            MOZ_ASSERT(NS_SUCCEEDED(rv));
        }
        buffer = eof + 1;
        if (*buffer == '\n') {
            buffer++;
        }
    }
}

bool
nsHttpRequestHead::IsHTTPS()
{
    RecursiveMutexAutoLock mon(mRecursiveMutex);
    return mHTTPS;
}

void
nsHttpRequestHead::SetMethod(const nsACString &method)
{
    RecursiveMutexAutoLock mon(mRecursiveMutex);
    mParsedMethod = kMethod_Custom;
    mMethod = method;
    if (!strcmp(mMethod.get(), "GET")) {
        mParsedMethod = kMethod_Get;
    } else if (!strcmp(mMethod.get(), "POST")) {
        mParsedMethod = kMethod_Post;
    } else if (!strcmp(mMethod.get(), "OPTIONS")) {
        mParsedMethod = kMethod_Options;
    } else if (!strcmp(mMethod.get(), "CONNECT")) {
        mParsedMethod = kMethod_Connect;
    } else if (!strcmp(mMethod.get(), "HEAD")) {
        mParsedMethod = kMethod_Head;
    } else if (!strcmp(mMethod.get(), "PUT")) {
        mParsedMethod = kMethod_Put;
    } else if (!strcmp(mMethod.get(), "TRACE")) {
        mParsedMethod = kMethod_Trace;
    }
}

void
nsHttpRequestHead::SetOrigin(const nsACString &scheme, const nsACString &host,
                             int32_t port)
{
    RecursiveMutexAutoLock mon(mRecursiveMutex);
    mOrigin.Assign(scheme);
    mOrigin.AppendLiteral("://");
    mOrigin.Append(host);
    if (port >= 0) {
        mOrigin.AppendLiteral(":");
        mOrigin.AppendInt(port);
    }
}

bool
nsHttpRequestHead::IsSafeMethod()
{
    RecursiveMutexAutoLock mon(mRecursiveMutex);
    // This code will need to be extended for new safe methods, otherwise
    // they'll default to "not safe".
    if ((mParsedMethod == kMethod_Get) || (mParsedMethod == kMethod_Head) ||
        (mParsedMethod == kMethod_Options) || (mParsedMethod == kMethod_Trace)
       ) {
        return true;
    }

    if (mParsedMethod != kMethod_Custom) {
        return false;
    }

    return (!strcmp(mMethod.get(), "PROPFIND") ||
            !strcmp(mMethod.get(), "REPORT") ||
            !strcmp(mMethod.get(), "SEARCH"));
}

void
nsHttpRequestHead::Flatten(nsACString &buf, bool pruneProxyHeaders)
{
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

} // namespace net
} // namespace mozilla
