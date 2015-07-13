/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "nsHttpRequestHead.h"

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
{
    MOZ_COUNT_CTOR(nsHttpRequestHead);
}

nsHttpRequestHead::~nsHttpRequestHead()
{
    MOZ_COUNT_DTOR(nsHttpRequestHead);
}

void
nsHttpRequestHead::SetMethod(const nsACString &method)
{
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
nsHttpRequestHead::SetOrigin(const nsACString &scheme, const nsACString &host, int32_t port)
{
    mOrigin.Assign(scheme);
    mOrigin.Append(NS_LITERAL_CSTRING("://"));
    mOrigin.Append(host);
    if (port >= 0) {
        mOrigin.Append(NS_LITERAL_CSTRING(":"));
        mOrigin.AppendInt(port);
    }
}

bool
nsHttpRequestHead::IsSafeMethod() const
{
  // This code will need to be extended for new safe methods, otherwise
  // they'll default to "not safe".
    if (IsGet() || IsHead() || IsOptions() || IsTrace()) {
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

    mHeaders.Flatten(buf, pruneProxyHeaders);
}

} // namespace net
} // namespace mozilla
