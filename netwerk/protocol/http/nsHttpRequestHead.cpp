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
    , mVersion(NS_HTTP_VERSION_1_1)
    , mParsedMethod(kMethod_Get)
    , mHTTPS(false)
    , mLock("nsHttpRequestHead.mLock")
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
    mLock.AssertCurrentThreadOwns();
    return mHeaders;
}

void
nsHttpRequestHead::SetHeaders(const nsHttpHeaderArray& aHeaders)
{
    MutexAutoLock lock(mLock);
    mHeaders = aHeaders;
}

void
nsHttpRequestHead::SetVersion(nsHttpVersion version)
{
    MutexAutoLock lock(mLock);
    mVersion = version;
}

void
nsHttpRequestHead::SetRequestURI(const nsCSubstring &s)
{
    MutexAutoLock lock(mLock);
    mRequestURI = s;
}

void
nsHttpRequestHead::SetPath(const nsCSubstring &s)
{
    MutexAutoLock lock(mLock);
    mPath = s;
}

uint32_t
nsHttpRequestHead::HeaderCount()
{
    MutexAutoLock lock(mLock);
    return mHeaders.Count();
}

nsresult
nsHttpRequestHead::VisitHeaders(nsIHttpHeaderVisitor *visitor,
                                nsHttpHeaderArray::VisitorFilter filter /* = nsHttpHeaderArray::eFilterAll*/)
{
    MutexAutoLock lock(mLock);
    return mHeaders.VisitHeaders(visitor, filter);
}

void
nsHttpRequestHead::Method(nsACString &aMethod)
{
    MutexAutoLock lock(mLock);
    aMethod = mMethod;
}

nsHttpVersion
nsHttpRequestHead::Version()
{
    MutexAutoLock lock(mLock);
    return mVersion;
}

void
nsHttpRequestHead::RequestURI(nsACString &aRequestURI)
{
    MutexAutoLock lock(mLock);
    aRequestURI = mRequestURI;
}

void
nsHttpRequestHead::Path(nsACString &aPath)
{
    MutexAutoLock lock(mLock);
    aPath = mPath.IsEmpty() ? mRequestURI : mPath;
}

void
nsHttpRequestHead::SetHTTPS(bool val)
{
    MutexAutoLock lock(mLock);
    mHTTPS = val;
}

void
nsHttpRequestHead::Origin(nsACString &aOrigin)
{
    MutexAutoLock lock(mLock);
    aOrigin = mOrigin;
}

nsresult
nsHttpRequestHead::SetHeader(nsHttpAtom h, const nsACString &v,
                             bool m /*= false*/)
{
    MutexAutoLock lock(mLock);
    return mHeaders.SetHeader(h, v, m,
                              nsHttpHeaderArray::eVarietyRequestOverride);
}

nsresult
nsHttpRequestHead::SetHeader(nsHttpAtom h, const nsACString &v, bool m,
                             nsHttpHeaderArray::HeaderVariety variety)
{
    MutexAutoLock lock(mLock);
    return mHeaders.SetHeader(h, v, m, variety);
}

nsresult
nsHttpRequestHead::SetEmptyHeader(nsHttpAtom h)
{
    MutexAutoLock lock(mLock);
    return mHeaders.SetEmptyHeader(h,
                                   nsHttpHeaderArray::eVarietyRequestOverride);
}

nsresult
nsHttpRequestHead::GetHeader(nsHttpAtom h, nsACString &v)
{
    v.Truncate();
    MutexAutoLock lock(mLock);
    return mHeaders.GetHeader(h, v);
}

void
nsHttpRequestHead::ClearHeader(nsHttpAtom h)
{
    MutexAutoLock lock(mLock);
    mHeaders.ClearHeader(h);
}

void
nsHttpRequestHead::ClearHeaders()
{
    MutexAutoLock lock(mLock);
    mHeaders.Clear();
}

bool
nsHttpRequestHead::HasHeader(nsHttpAtom h)
{
  MutexAutoLock lock(mLock);
  return mHeaders.HasHeader(h);
}

bool
nsHttpRequestHead::HasHeaderValue(nsHttpAtom h, const char *v)
{
    MutexAutoLock lock(mLock);
    return mHeaders.HasHeaderValue(h, v);
}

nsresult
nsHttpRequestHead::SetHeaderOnce(nsHttpAtom h, const char *v,
                                 bool merge /*= false */)
{
    MutexAutoLock lock(mLock);
    if (!merge || !mHeaders.HasHeaderValue(h, v)) {
        return mHeaders.SetHeader(h, nsDependentCString(v), merge,
                                  nsHttpHeaderArray::eVarietyRequestOverride);
    }
    return NS_OK;
}

nsHttpRequestHead::ParsedMethodType
nsHttpRequestHead::ParsedMethod()
{
    MutexAutoLock lock(mLock);
    return mParsedMethod;
}

bool
nsHttpRequestHead::EqualsMethod(ParsedMethodType aType)
{
    MutexAutoLock lock(mLock);
    return mParsedMethod == aType;
}

void
nsHttpRequestHead::ParseHeaderSet(char *buffer)
{
    MutexAutoLock lock(mLock);
    nsHttpAtom hdr;
    char *val;
    while (buffer) {
        char *eof = strchr(buffer, '\r');
        if (!eof) {
            break;
        }
        *eof = '\0';
        if (NS_SUCCEEDED(nsHttpHeaderArray::ParseHeaderLine(buffer,
                                                            &hdr,
                                                            &val))) {
            mHeaders.SetHeaderFromNet(hdr, nsDependentCString(val), false);
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
    MutexAutoLock lock(mLock);
    return mHTTPS;
}

void
nsHttpRequestHead::SetMethod(const nsACString &method)
{
    MutexAutoLock lock(mLock);
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
    MutexAutoLock lock(mLock);
    mOrigin.Assign(scheme);
    mOrigin.Append(NS_LITERAL_CSTRING("://"));
    mOrigin.Append(host);
    if (port >= 0) {
        mOrigin.Append(NS_LITERAL_CSTRING(":"));
        mOrigin.AppendInt(port);
    }
}

bool
nsHttpRequestHead::IsSafeMethod()
{
    MutexAutoLock lock(mLock);
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
    MutexAutoLock lock(mLock);
    // note: the first append is intentional.

    buf.Append(mMethod);
    buf.Append(' ');
    buf.Append(mRequestURI);
    buf.AppendLiteral(" HTTP/");

    switch (mVersion) {
    case NS_HTTP_VERSION_1_1:
        buf.AppendLiteral("1.1");
        break;
    case NS_HTTP_VERSION_0_9:
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
