/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHttpRequestHead_h__
#define nsHttpRequestHead_h__

#include "nsHttp.h"
#include "nsHttpHeaderArray.h"
#include "nsString.h"
#include "nsCRT.h"

//-----------------------------------------------------------------------------
// nsHttpRequestHead represents the request line and headers from an HTTP
// request.
//-----------------------------------------------------------------------------

class nsHttpRequestHead
{
public:
    nsHttpRequestHead() : mMethod(nsHttp::Get), mVersion(NS_HTTP_VERSION_1_1) {}
    ~nsHttpRequestHead() {}

    void SetMethod(nsHttpAtom method) { mMethod = method; }
    void SetVersion(nsHttpVersion version) { mVersion = version; }
    void SetRequestURI(const nsCSubstring &s) { mRequestURI = s; }

    nsHttpHeaderArray  &Headers()    { return mHeaders; }
    nsHttpAtom          Method()     { return mMethod; }
    nsHttpVersion       Version()    { return mVersion; }
    const nsCSubstring &RequestURI() { return mRequestURI; }

    const char *PeekHeader(nsHttpAtom h)                                     { return mHeaders.PeekHeader(h); }
    nsresult SetHeader(nsHttpAtom h, const nsACString &v, bool m=false) { return mHeaders.SetHeader(h, v, m); }
    nsresult GetHeader(nsHttpAtom h, nsACString &v)                          { return mHeaders.GetHeader(h, v); }
    void ClearHeader(nsHttpAtom h)                                           { mHeaders.ClearHeader(h); }
    void ClearHeaders()                                                      { mHeaders.Clear(); }

    const char *FindHeaderValue(nsHttpAtom h, const char *v) { return mHeaders.FindHeaderValue(h, v); }
    bool        HasHeaderValue(nsHttpAtom h, const char *v) { return mHeaders.HasHeaderValue(h, v); }

    void Flatten(nsACString &, bool pruneProxyHeaders = false);

private:
    nsHttpHeaderArray mHeaders;
    nsHttpAtom        mMethod;
    nsHttpVersion     mVersion;
    nsCString         mRequestURI;
};

#endif // nsHttpRequestHead_h__
